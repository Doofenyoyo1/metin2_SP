# -*- coding: utf-8 -*-
"""Renderuje playerbot_price_tables.h z cennika Iwakury (data/iwakura_ceny.txt).

Jeden plik, jego wersja 1.2 (16 wrzesnia; 1.1 z 15, 1.0 z 14): skalowanie wedlug dropu yang,
mnozniki kamieni duszy i bonusow, ulepszacze, ksiegi, opaski zapomnienia,
kamienie duszy, marmury, szkatulki, inne, ulepszanie, pasywne, kon, lowienie,
zielarstwo, gildia, rudy i ceny sprzetu - bronie,
zbroje, buty, bransolety, naszyjniki, kolczyki i tarcze. Wczesniej byly to dwa
pliki i trzy tabele pisane recznie w types.h i bonus.h.

Wejscie to jego plik plus trzy tabele swiata mt2009, zrzucone z bazy (nazwy sa
w cp1250, stad HEX):

    docker exec -i <db> sh -c 'exec mariadb -uroot -p"$MARIADB_ROOT_PASSWORD" -N' > item_proto_hex.tsv
      SELECT vnum, HEX(locale_name), type, subtype, COALESCE(limitvalue0,0) FROM world.item_proto ORDER BY vnum;
    ... > mob_proto_hex.tsv
      SELECT vnum, HEX(locale_name), level FROM world.mob_proto ORDER BY vnum;
    ... > item_attr.tsv
      SELECT apply, weapon, body, wrist, foots, neck, head, shield, ear FROM world.item_attr ORDER BY apply;

Uzycie (z katalogu linux-port/overlays/playerbot):
    python tools/generate_iwakura_prices.py item_proto_hex.tsv mob_proto_hex.tsv item_attr.tsv \\
        data/iwakura_ceny.txt src/game/src/playerbot_price_tables.h

Skrypt **przerywa z bledem**, gdy ktorejs nazwy nie da sie zwiazac z vnumem,
gdy bonus nie ma przypisanego APPLY albo gdy bonus z mnoznikiem nie losuje sie
na slocie, pod ktorym go wpisal (world.item_attr). Cennik, ktory po cichu gubi
polowe wierszy, jest gorszy niz jego brak: bot wystawia wtedy stara cene i nikt
nie wie dlaczego.
"""
import io
import re
import sys

# Nazwy, ktore Iwakura pisze w pelni, a gra skraca. Kazda sprawdzona recznie:
# w item_proto jest dokladnie jeden kandydat na kazda z nich.
GEAR_ALIASES = {
    # Cennik 1.2 zgubil kropke skrotu; w item_proto jest "Sztylet Piesci Diab.".
    'sztylet pięści diab': 'sztylet pięści diab.',
    'mnisia zbr. płytowa': 'mnisia zbr. płyt.',
    'żelazna zbr. płytowa': 'żelazna zbr. płyt.',
    'żałobna zbr. płytowa': 'żałobna zbr. płyt.',
    'burzowa zbroj. płytowa': 'burzowa zbroj. płyt.',
    'nieszczęsna zbr. płytowa': 'nieszczęsna zbr. płyt.',
    'upiorna zbroja płytowa': 'upiorna zbroja płyt.',
    'mistyczna zbroja płytowa': 'mistyczna zbroja płyt.',
    'mglista zbroja płytowa': 'mglista zbroja płyt.',
    'duchowa zbroja płytowa': 'duchowa zbroja płyt.',
    # Sekcja [DZWONY], wiec chodzi o dzwon - w grze pelna nazwa ma to slowo.
    'złoty robaczy': 'złoty robaczy dzwon',
}

# To samo dla ulepszaczy i rud.
GOODS_ALIASES = {
    'worek z pajęczymi jajami': 'worek z pajęcz. jajami',
    'ruda białego złota': 'ruda biał. złota',
    'ruda niebiańskich łez': 'ruda niebiań. łez',
    # item_proto zna jedna "Waleczna Dusza" (30356, material) i nic dluzszego.
    'waleczna dusza zaprzys': 'waleczna dusza',
    # Cennik 1.1: item_proto skraca "Czerwona" do "Czerw." (70204); jedyny kandydat.
    'czerwona farba do włosów': 'czerw. farba do włosów',
    # Cennik 1.2 pisze bez ogonka; 70049.
    'pierścien lucy': 'pierścień lucy',
}

# Nazwa, pod ktora gra ma kilka przedmiotow, a jego cena dotyczy jednego z nich.
# Poza tymi dwiema kazdy vnum o danej nazwie dostaje te cene - nazwa to nie
# przedmiot, a bot wycenia przedmioty (Nieznany Talizman+ i Zabie Udka sa
# w grze dwa razy kazdy).
VNUM_OVERRIDES = {
    # 25041 ma w tym locale te sama nazwe, ale to Magiczny Kamien (HYUNIRON,
    # niszczy przedmiot przy porazce); zwoj z jego cennika to 25040.
    'zwój błogosławieństwa': [25040],
    # Sekcja [RUDY]: przetopiony krysztal z kopalni. 30203 i 90003 to inne
    # przedmioty o tej samej nazwie.
    'kryształ': [50631],
}

# Wiersze, ktore generator pomija z rozmyslem, z powodem: kazdy inny brak
# nazwy przerywa render. Cennik 1.2 (16 wrzesnia) ma dwa wiersze na jeden
# przedmiot - ten, ktory zostaje, jest nizej na liscie, czyli nowszy.
SKIPPED_ROWS = {
    # 30356 to jedyna "Waleczna Dusza" w grze; 1.2 dodal jej wlasny wiersz
    # (40000) obok starego "Zaprzys" (32500) z 1.1.
    'waleczna dusza zaprzys': 'jeden przedmiot, wiersz "Waleczna dusza" go wycenia',
    # Literowka obok wlasciwego wiersza "Wysuszone Oczy" (75000).
    'wyuszone oczy': 'literowka wiersza "Wysuszone Oczy"',
}

# Potwory z listy wyjatkow do marmurow. Tak samo: gra skraca, on pisze w pelni.
MOB_ALIASES = {
    'ezoteryczny fanatyk': 'ezot. fanatyk',
    'podły śmier. truj. pająk': 'podły śm. truj. pająk',
    # "v2" to Loch Pajakow V2, na ktorym ten potwor stoi; w mob_proto jest
    # jeden "Maly Trujacy Pajak" i to jest ten.
    'mały trujący pająk v2': 'mały trujący pająk',
}

# Umiejetnosci po nazwie, jak pisze je Iwakura -> vnum ze skill_proto.
# Zrodlo: PLAYER_SKILLS w files/admin_panel.py (tam sa nazwy klienta).
SKILL_IDS = {
    'trzystronne cięcie': 1, 'wir miecza': 2, 'berserk': 3, 'aura miecza': 4, 'szarża': 5,
    'duchowe uderzenie': 16, 'tąpnięcie': 17, 'uderzenie miecza': 18, 'silne ciało': 19,
    'walnięcie': 20,
    'zasadzka': 31, 'szybki atak': 32, 'wirujący sztylet': 33, 'krycie się': 34,
    'trująca chmura': 35,
    'powtarzalny strzał': 46, 'deszcz strzał': 47, 'ognista strzała': 48,
    'bezszelestny chód': 49, 'trująca strzała': 50,
    'uderzenie palcem': 61, 'smoczy wir': 62, 'czarowane ostrze': 63, 'strach': 64,
    'czarowana zbroja': 65, 'rozproszenie magii': 66,
    'mroczne uderzenie': 76, 'ogniste uderzenie': 77, 'ognisty duch': 78,
    'mroczna ochrona': 79, 'duchowy cios': 80,
    # Klient nazywa 81 "Mroczna Sfera"; Iwakura pisze "Mroczny Kamien" - to ta
    # sama, ostatnia umiejetnosc czarnej magii.
    'mroczny kamień': 81, 'mroczna sfera': 81,
    'latający talizman': 91, 'strzelający smok': 92, 'smoczy skowyt': 93,
    'błogosławieństwo': 94, 'odbicie': 95, 'pomoc smoka': 96,
    'błyskawiczny rzut': 106, 'przywołanie błyskawicy': 107,
    # 108 to "Burzowy Szpon" w kliencie, u niego "Szpon Blyskawicy".
    'szpon błyskawicy': 108, 'burzowy szpon': 108,
    'leczenie': 109, 'zwinność': 110,
    # 111 to "Zwiekszenie Ataku"; zostaje jako jedyna nieobsadzona w jego
    # grupie uzdrawiania, gdzie pisze "Burza".
    'burza': 111, 'zwiększenie ataku': 111,
}

# Kamienie duszy: nazwa u Iwakury -> "kind", czyli dwie ostatnie cyfry vnumu.
# Bronowe 30-37, zbrojowe 38-43 (patrz IsPlayerBotWeaponSoulStoneKind).
SOUL_STONE_KINDS = {
    'penetracji': 30, 'śmierci': 31, 'powtórki': 32, 'wojownika': 33, 'ninja': 34,
    'sury': 35, 'szamana': 36, 'potwora': 37,
    'uchylenia': 38, 'uniku': 39, 'magii': 40, 'witalności': 41, 'obrony': 42,
    'przyspieszenia': 43,
}

# Bonusy: nazwa u Iwakury -> stala APPLY_* nakladki. Na mt2009 punkt jest
# bonusem (playerbot_engine_compat.h), a world.item_attr mowi, na ktorym slocie
# ktory sie losuje - kazdy wiersz jest z tym sprawdzany, i to sprawdzenie
# znalazlo trzy bledy starej tabeli recznej: "Szansa na kradziez PE" to
# MANA_BURN_PCT, nie STEAL_SP; "Punkty doswiadczenia +%" to MALL_EXPBONUS, nie
# EXP_DOUBLE_BONUS; a "Szansa na dobicie ciosu" na zbroi to odbicie ciosu
# (REFLECT_MELEE) - cios krytyczny na zbroi sie nie losuje.
BONUS_APPLIES = {
    'silny przeciwko ludziom': 'APPLY_ATTBONUS_HUMAN',
    'silny przeciwko mistykom': 'APPLY_ATTBONUS_MILGYO',
    'silny przeciwko diablom': 'APPLY_ATTBONUS_DEVIL',
    'silny przeciwko nieumarlym': 'APPLY_ATTBONUS_UNDEAD',
    'silny przeciwko nieumarłym': 'APPLY_ATTBONUS_UNDEAD',
    'silny przeciwko zwierzętom': 'APPLY_ATTBONUS_ANIMAL',
    'silny przeciwko orkom': 'APPLY_ATTBONUS_ORC',
    'odpornosc na magie': 'APPLY_RESIST_MAGIC',
    'odporność na magie': 'APPLY_RESIST_MAGIC',
    'regeneracja st (staminy)': 'APPLY_ST_REGEN',
    'maks. stamina': 'APPLY_MAX_STAMINA',
    'regeneracja mistur pż': 'APPLY_HP_REGEN',
    'regeneracja mistur pe': 'APPLY_SP_REGEN',
    # Cennik 1.2 poprawil pisownie; 1.1 zostaje rozumiany.
    'regeneracja mikstur pż': 'APPLY_HP_REGEN',
    'regeneracja mikstur pe': 'APPLY_SP_REGEN',
    'czas trwania umiejętności': 'APPLY_SKILL_DURATION',
    'x% obrażen dodanych do pe': 'APPLY_STEAL_SP',
    'x% obrażen dodanych do pż': 'APPLY_STEAL_HP',
    'szybkość ataku': 'APPLY_ATT_SPEED',
    'szbykość ataku': 'APPLY_ATT_SPEED',
    'szansa na unik. strzały': 'APPLY_DODGE',
    'szansa na otrucie': 'APPLY_POISON_PCT',
    'maks. pż': 'APPLY_MAX_HP',
    'maks. pe': 'APPLY_MAX_SP',
    'wartość ataku': 'APPLY_ATT_GRADE_BONUS',
    'szybkość zaklęcia': 'APPLY_CAST_SPEED',
    'szansa na dobicie ciosu': 'APPLY_REFLECT_MELEE',
    'odporność na sztylety': 'APPLY_RESIST_DAGGER',
    'odporność na strzały': 'APPLY_RESIST_BOW',
    'odporność na wahlarze': 'APPLY_RESIST_FAN',
    'odporność na wachlarze': 'APPLY_RESIST_FAN',
    'odporność na dzwony': 'APPLY_RESIST_BELL',
    'odporność na miecze': 'APPLY_RESIST_SWORD',
    'odrponość na broń dwuręczną': 'APPLY_RESIST_TWOHAND',
    'odporność na broń dwuręczną': 'APPLY_RESIST_TWOHAND',
    'niewrażliwy na omdlenie': 'APPLY_IMMUNE_STUN',
    'niewrażliwy na spowolnienie': 'APPLY_IMMUNE_SLOW',
    'szansa na blok ciosu': 'APPLY_BLOCK',
    'szansa na odbicie ciosu': 'APPLY_REFLECT_MELEE',
    'szansa na podwójną ilość yang': 'APPLY_GOLD_DOUBLE_BONUS',
    'szansa na podwojna ilosc yang': 'APPLY_GOLD_DOUBLE_BONUS',
    'siła': 'APPLY_STR',
    'inteligencja': 'APPLY_INT',
    'zręczność': 'APPLY_DEX',
    'witalność': 'APPLY_CON',
    'szansa na cios krytyczny': 'APPLY_CRITICAL_PCT',
    'punkty doświadczenia +%': 'APPLY_MALL_EXPBONUS',
    'szansa na omdlenie': 'APPLY_STUN_PCT',
    'szansa na spowoleninie': 'APPLY_SLOW_PCT',
    'szansa na spowolnienie': 'APPLY_SLOW_PCT',
    'szansa na kradzież pe': 'APPLY_MANA_BURN_PCT',
    'szansa na przeszywające uderzenie': 'APPLY_PENETRATE_PCT',
    'szansa na odbicie pocisku': 'APPLY_REFLECT_ARROW',
    'szybkość ruchu': 'APPLY_MOV_SPEED',
    'odporność na trucizny': 'APPLY_POISON_REDUCE',
}
# Punkty, ktore ma tylko silnik mt2009: na r40250 takich bonusow nie ma, wiec
# ich wiersze sa w bloku #if.
MT2009_ONLY = {'APPLY_ST_REGEN', 'APPLY_SKILL_DURATION', 'APPLY_REFLECT_ARROW'}
# Nazwy punktu tam, gdzie nie sa po prostu POINT_ + reszta nazwy (compat.h).
POINT_OF = {'APPLY_CAST_SPEED': 'POINT_CASTING_SPEED', 'APPLY_CON': 'POINT_HT',
            'APPLY_INT': 'POINT_IQ', 'APPLY_STR': 'POINT_ST', 'APPLY_DEX': 'POINT_DX'}
# Jego naglowek slotu -> (maska w C, kolumna world.item_attr).
SLOTS = {
    'hełm': ('PRICE_SLOT_HEAD', 'head'), 'zbroja': ('PRICE_SLOT_BODY', 'body'),
    'tarcza': ('PRICE_SLOT_SHIELD', 'shield'), 'buty': ('PRICE_SLOT_FOOTS', 'foots'),
    'bransoleta': ('PRICE_SLOT_WRIST', 'wrist'), 'naszyjnik': ('PRICE_SLOT_NECK', 'neck'),
    'kolczyki': ('PRICE_SLOT_EAR', 'ear'), 'broń': ('PRICE_SLOT_WEAPON', 'weapon'),
}
SLOT_ORDER = ['PRICE_SLOT_HEAD', 'PRICE_SLOT_BODY', 'PRICE_SLOT_SHIELD', 'PRICE_SLOT_FOOTS',
              'PRICE_SLOT_WRIST', 'PRICE_SLOT_NECK', 'PRICE_SLOT_EAR', 'PRICE_SLOT_WEAPON']
ATTR_COLUMNS = ['weapon', 'body', 'wrist', 'foots', 'neck', 'head', 'shield', 'ear']

TOP_SECTIONS = ['INFORMACJE OGOLNE', 'MNOŻNIK KAMIENI DUSZY W ZBROJACH I BRONIACH',
                'MNOŻNIK BONUSÓW DODATKOWYCH', 'ULEPSZACZE', 'KSIĘGI UMIEJĘTNOŚCI',
                'Opaski zapomnienia', 'Kamienie duszy', 'Marmury Polimorfi', 'Szkatułki',
                'Inne', 'Ulepszanie', 'Pasywne', 'Koń', 'Łowienie',
                'Zielarstwo', 'Gildia', 'RUDY', '[BRONIE]']

BAND_RE = re.compile(r'^(?:od\s*)?\+(\d)\s*(?:do\s*\+(\d))?\s*[-–]\s*(.+?)\s*$', re.I)
PRICE_RE = re.compile(r'^(.*?)\s*[-–]\s*([\d ]+)$')
RATE_RE = re.compile(r'dla dropu yang\s+(\d+)\s*%\s*[-–]\s*przelicznik\s*x\s*([\d.,]+)', re.I)


def norm(text):
    return re.sub(r'\s+', ' ', text.replace(' ', ' ')).strip().lower()


def goods_key(text):
    # "Amulet Orka +" w cenniku, "Amulet Orka+" w grze.
    return re.sub(r'\s+\+', '+', norm(text))


# Overlay sources are ASCII by convention, and these names only ever appear in
# a trailing comment saying which row is which. Transliterate rather than drop
# them: "Zloty Robaczy Dzwon" still names the thing, "Z?oty" does not.
_ASCII = {
    0x104: 'A', 0x105: 'a', 0x106: 'C', 0x107: 'c', 0x118: 'E', 0x119: 'e',
    0x141: 'L', 0x142: 'l', 0x143: 'N', 0x144: 'n', 0xd3: 'O', 0xf3: 'o',
    0x15a: 'S', 0x15b: 's', 0x179: 'Z', 0x17a: 'z', 0x17b: 'Z', 0x17c: 'z',
}


def ascii_comment(text):
    return ''.join(_ASCII.get(ord(ch), ch if ord(ch) < 128 else '?') for ch in text)


def load_hex(path):
    out = []
    for line in io.open(path, encoding='latin-1'):
        parts = line.rstrip('\n').split('\t')
        if len(parts) < 2:
            continue
        try:
            name = bytes.fromhex(parts[1]).decode('cp1250')
        except ValueError:
            continue
        out.append((int(parts[0]), name, parts[2:]))
    return out


def load_item_attr(path):
    table = {}
    for line in io.open(path, encoding='latin-1'):
        parts = line.rstrip('\n').split('\t')
        if len(parts) != 1 + len(ATTR_COLUMNS):
            continue
        table[parts[0]] = dict(zip(ATTR_COLUMNS, (int(v) for v in parts[1:])))
    return table


class Resolver(object):
    """Nazwa -> vnum, z aliasami i lista tego, czego nie znalazl."""

    def __init__(self, items, mobs):
        self.items = {}
        self.goods = {}
        for vnum, name, rest in items:
            self.items.setdefault(norm(name), vnum)
            item_type = int(rest[0]) if rest and rest[0].isdigit() else 0
            self.goods.setdefault(goods_key(name), []).append((vnum, item_type))
        # Cennik 1.3 wycenia je jednym wierszem - "WSZYSTKIE RECEPTURY np.
        # Zielony Wywar, Platynowy Wywar, Szary wywar itd." - bo w grze jest
        # ich czterdziesci i wszystkie kosztuja tyle samo. Jego przyklady to
        # nazwy po slowie "Receptura", wiec wiazemy po tym slowie, a nie po
        # nazwie wywaru.
        self.recipes = sorted(set(
            vnum for vnum, name, rest in items
            if norm(name).startswith('receptura') or norm(name).endswith(' receptura')))
        self.mobs = {}
        for vnum, name, rest in mobs:
            self.mobs.setdefault(norm(name), vnum)
        self.missing = []

    def gear(self, family):
        key = GEAR_ALIASES.get(norm(family), norm(family))
        vnum = self.items.get(key + '+0')
        if vnum is None:
            self.missing.append('rodzina sprzetu: %s' % family)
        return vnum

    def goods_vnums(self, name):
        key = goods_key(name)
        if key in SKIPPED_ROWS:
            return []
        if key.startswith('wszystkie receptury'):
            if not self.recipes:
                self.missing.append('przedmiot: %s (zadnej receptury w item_proto)' % name)
            return self.recipes
        key = GOODS_ALIASES.get(key, key)
        if key in VNUM_OVERRIDES:
            return VNUM_OVERRIDES[key]
        # Never a weapon or armour (types 1 and 2): those are priced as gear.
        vnums = sorted(v for v, t in self.goods.get(key, []) if t not in (1, 2))
        if not vnums:
            self.missing.append('przedmiot: %s' % name)
        return vnums

    def mob(self, name):
        key = MOB_ALIASES.get(norm(name), norm(name))
        vnum = self.mobs.get(key)
        if vnum is None:
            self.missing.append('potwor: %s' % name)
        return vnum

    def skill(self, name):
        key = norm(re.sub(r'^oz\s+', '', name, flags=re.I))
        vnum = SKILL_IDS.get(key)
        if vnum is None:
            self.missing.append('umiejetnosc: %s' % name)
        return vnum


class Sheet(object):
    def __init__(self, path):
        self.lines = io.open(path, encoding='utf-8-sig').read().replace('\r\n', '\n').split('\n')
        self.headers = {}
        for i, line in enumerate(self.lines):
            stripped = line.strip()
            for title in TOP_SECTIONS:
                if stripped == '[' + title + ']':
                    if title in self.headers:
                        raise SystemExit('sekcja [%s] wystepuje dwa razy' % title)
                    self.headers[title] = i
        missing = [t for t in TOP_SECTIONS if t not in self.headers]
        if missing:
            raise SystemExit('brak sekcji: %s' % ', '.join(missing))

    def block(self, title):
        start = self.headers[title] + 1
        later = sorted(i for i in self.headers.values() if i >= start)
        end = later[0] if later else len(self.lines)
        return [line.strip() for line in self.lines[start:end]]


def parse_rate_points(sheet):
    points = []
    for line in sheet.block('INFORMACJE OGOLNE'):
        m = RATE_RE.search(line)
        if m:
            points.append((int(m.group(1)), int(round(float(m.group(2).replace(',', '.')) * 100))))
    if not points or points[0][0] != 100 or any(b[0] <= a[0] for a, b in zip(points, points[1:])):
        raise SystemExit('skalowanie wg dropu yang: punkty nie rosna od 100%%: %r' % points)
    return points


def parse_socket_multipliers(sheet):
    """Mnozniki kamieni duszy w zbroi/broni: (kind, grade, procent) i liczba KD."""
    out, count_mult = [], {}
    for line in sheet.block('MNOŻNIK KAMIENI DUSZY W ZBROJACH I BRONIACH'):
        m = re.match(r'^(.*?)\s*[-–]\s*([\d.,]+)\s*$', line)
        if not m:
            continue
        name, factor = m.group(1).strip(), float(m.group(2).replace(',', '.'))
        low = norm(name)
        if low.startswith('pęknięte kd'):
            count_mult[0] = factor
        elif low.startswith('jedno włożone'):
            count_mult[1] = factor
        elif low.startswith('dwa włożone'):
            count_mult[2] = factor
        elif low.startswith('trzy włożone'):
            count_mult[3] = factor
        elif low.startswith('kamień duszy'):
            m2 = re.match(r'^kamień duszy\s+(.*?)\s*\+(\d)$', low)
            if not (m2 and m2.group(1) in SOUL_STONE_KINDS):
                raise SystemExit('mnoznik KD: nieznany kamien "%s"' % name)
            out.append((SOUL_STONE_KINDS[m2.group(1)], int(m2.group(2)), factor))
    if sorted(count_mult) != [0, 1, 2, 3]:
        raise SystemExit('mnoznik KD: brak mnoznikow liczby kamieni: %r' % count_mult)
    return out, count_mult


def pct(value):
    return int(round(float(value) * 100))


def parse_bonus_multipliers(sheet, item_attr, errors):
    rows = []          # (slot, apply, max, other, lo, hi)
    tiers = {'avg': [], 'skill': []}
    slot = None
    tier_mode = None
    pending = None
    row_re = re.compile(r'^(.*?)\s+([\d.]+)\s*\|\s*([\d.]+)\s*$')
    split_re = re.compile(r'^dla item[oó]w (powy[zż]ej|poni[zż]ej) 33 lvl\s+([\d.]+)\s*\|\s*([\d.]+)\s*$', re.I)
    tier_re = re.compile(r'^zakres od\s+(\d+)\s+do\s+(\d+)\s+([\d.]+)\s*$', re.I)
    top_re = re.compile(r'^maks\.? warto[sś][cć]\s+(\d+)\s+([\d.]+)\s*$', re.I)
    for line in sheet.block('MNOŻNIK BONUSÓW DODATKOWYCH'):
        if not line:
            continue
        if line.startswith('>'):
            key = norm(line[1:])
            if key not in SLOTS:
                raise SystemExit('bonusy: nieznany slot "%s"' % line)
            slot, tier_mode, pending = SLOTS[key], None, None
            continue
        low = norm(line)
        if line.startswith('['):
            if 'średnie obrażenia' in low:
                tier_mode = 'avg'
            elif 'obrażenia umi' in low:
                tier_mode = 'skill'
            continue
        if slot is None:
            continue
        m = tier_re.match(line) or top_re.match(line)
        if m and tier_mode:
            tiers[tier_mode].append((int(m.group(1)), pct(m.group(m.lastindex))))
            continue
        m = split_re.match(line)
        if m:
            if not pending:
                raise SystemExit('bonusy: pasmo 33 lvl bez nazwy bonusu: "%s"' % line)
            above = m.group(1).lower().startswith('powy')
            rows.append((slot, pending, pct(m.group(2)), pct(m.group(3)),
                         33 if above else 0, 255 if above else 32))
            continue
        m = row_re.match(line)
        if m:
            rows.append((slot, norm(m.group(1)), pct(m.group(2)), pct(m.group(3)), 0, 255))
            pending = None
            continue
        pending = low

    merged = {}
    for (slot_enum, column), name, max_pct, other_pct, lo, hi in rows:
        if max_pct == 100 and other_pct == 100:
            continue  # x1.0 w obu przypadkach - nie zmienia ceny
        apply_name = BONUS_APPLIES.get(name)
        if apply_name is None:
            errors.append('bonus bez APPLY: "%s" (%s)' % (name, slot_enum))
            continue
        point = POINT_OF.get(apply_name, 'POINT_' + apply_name[len('APPLY_'):])
        if item_attr.get(point, {}).get(column, 0) <= 0:
            errors.append('bonus "%s" (%s) nie losuje sie na slocie %s wg world.item_attr'
                          % (name, point, column))
            continue
        key = (apply_name, max_pct, other_pct, lo, hi)
        entry = merged.setdefault(key, [set(), set()])
        entry[0].add(slot_enum)
        entry[1].add(name)
    for mode in ('avg', 'skill'):
        values = tiers[mode]
        if not values or any(b[0] <= a[0] for a, b in zip(values, values[1:])):
            raise SystemExit('bonusy: progi obrazen "%s" nie rosna: %r' % (mode, values))
    return merged, tiers


def parse_priced_lines(block):
    for line in block:
        if not line or line.startswith('/'):
            continue
        m = PRICE_RE.match(line)
        if m:
            yield m.group(1).strip(), int(m.group(2).replace(' ', ''))


def parse_goods(sheet, resolver, titles):
    out = {}
    for title in titles:
        for name, price in parse_priced_lines(sheet.block(title)):
            for vnum in resolver.goods_vnums(name):
                if vnum in out and out[vnum][0] != price:
                    raise SystemExit('vnum %d ma dwie ceny: %s i %s' % (vnum, out[vnum][1], name))
                out[vnum] = (price, name)
    return sorted((vnum, price, name) for vnum, (price, name) in out.items())


def parse_books(sheet, resolver):
    out = {}
    for name, price in parse_priced_lines(sheet.block('KSIĘGI UMIEJĘTNOŚCI')):
        skill = resolver.skill(name)
        if skill is None:
            continue
        if skill in out:
            raise SystemExit('ksiega %s wystepuje dwa razy' % name)
        out[skill] = (price, name)
    return sorted((skill, price, name) for skill, (price, name) in out.items())


def parse_scrolls(sheet, resolver):
    scrolls = []
    for line in sheet.block('Opaski zapomnienia'):
        if not line.lower().startswith('oz '):
            continue
        m = PRICE_RE.match(line)
        if m:
            skill = resolver.skill(m.group(1))
            if skill:
                scrolls.append((skill, int(m.group(2).replace(' ', '')), m.group(1).strip()))
        elif 'handlar' in line.lower():
            name = re.split(r'[-–]', line)[0].strip()
            skill = resolver.skill(name)
            if skill:
                scrolls.append((skill, 0, name))
    return sorted(scrolls)


def parse_soul_stones(sheet, resolver):
    stones, grade_default = [], {}
    for name, price in parse_priced_lines(sheet.block('Kamienie duszy')):
        low = norm(name)
        price_all = re.match(r'^kamienie duszy \+(\d) \(wszystkie\)$', low)
        if price_all:
            grade_default[int(price_all.group(1))] = price
            continue
        one = re.match(r'^kamień duszy\s+(.*?)\s*\+(\d)$', low)
        if one and one.group(1) in SOUL_STONE_KINDS:
            stones.append((SOUL_STONE_KINDS[one.group(1)], int(one.group(2)), price))
        else:
            resolver.missing.append('kamien duszy: %s' % name)
    return sorted(stones), grade_default


def parse_marbles(sheet, resolver):
    marbles = []
    body = sheet.block('Marmury Polimorfi')
    band = None
    for line in body:
        m = re.search(r'od\s*(\d+)\s*[-–]\s*(\d+)', line)
        if m and 'losowo' in line:
            band = (int(m.group(1)), int(m.group(2)))
            continue
        if not line or line.startswith('/') or line.startswith('('):
            continue
        m = re.match(r'^(.*?)\s*[-–]\s*(\d+)$', line)
        if not m:
            continue
        mob = resolver.mob(m.group(1).strip())
        if mob:
            marbles.append((mob, int(m.group(2)), m.group(1).strip()))
    if band is None:
        raise SystemExit('marmury: brak przedzialu "od X - Y (losowo)"')
    return sorted(marbles), band


def parse_gear(sheet, resolver, errors):
    """[(vnum, maska handlarki, [ceny 0..9], rodzina)] od [[BRONIE]] do konca."""
    start = sheet.headers['[BRONIE]'] + 1
    entries, current = [], None
    for raw in sheet.lines[start:]:
        line = raw.strip()
        if not line or line.startswith('['):
            continue
        band = BAND_RE.match(line)
        if band:
            if current is None:
                raise SystemExit('sprzet: pasmo bez rodziny: "%s"' % line)
            lo = int(band.group(1))
            hi = int(band.group(2)) if band.group(2) else lo
            value = band.group(3)
            for plus in range(lo, hi + 1):
                if re.fullmatch(r'[\d ]+', value):
                    current[1][plus] = int(value.replace(' ', ''))
                elif 'handlar' in value.lower():
                    current[1][plus] = None
                else:
                    raise SystemExit('sprzet %s: niezrozumiala cena "%s"' % (current[0], value))
            continue
        current = (line, {})
        entries.append(current)
    rows = []
    for family, bands in entries:
        if sorted(bands) != list(range(10)):
            errors.append('rodzina %s: brak pasm %s' % (family, sorted(set(range(10)) - set(bands))))
            continue
        vnum = resolver.gear(family)
        if vnum is None:
            continue
        mask = sum(1 << plus for plus in range(10) if bands[plus] is None)
        prices = [bands[plus] or 0 for plus in range(10)]
        rows.append((vnum, mask, prices, family))
    return rows


HEADER = u'''// Rendered by linux-port/overlays/playerbot/tools/generate_iwakura_prices.py from Iwakura's price
// list (data/iwakura_ceny.txt, his v1.0). DO NOT EDIT; edit the list and re-run.
//
// Every number here is his, every vnum was resolved against this world's own
// item_proto and mob_proto, and every bonus row was checked against
// world.item_attr for the slot he put it under - the generator refuses to
// write this file if one name cannot be matched or one bonus cannot roll.
//
// Every price scales with the world's yang drop rate along the curve at the top
// of his sheet (PLAYERBOT_PRICE_RATE_POINTS, see ScalePlayerBotIwakuraPrice).
#ifndef __INC_METIN2_PLAYERBOT_PRICE_TABLES_H__
#define __INC_METIN2_PLAYERBOT_PRICE_TABLES_H__

namespace
{
\t// The yang drop rate in percent and the multiplier it pays, in hundredths:
\t// read straight through between his points and proportionally outside them.
\tstruct TPlayerBotPriceRatePoint { int iRate; int iPct; };
\tconst TPlayerBotPriceRatePoint PLAYERBOT_PRICE_RATE_POINTS[] = {
%(rates)s\t};

\t// Weapons, armour, boots, bracelets, necklaces, earrings and shields by
\t// family and refine. The index is the refine level, 0 to 9; a bit set in the
\t// mask is a refine his sheet marks "do handlarki" - that piece is the
\t// merchant's and its price is zero here. A family the table does not carry
\t// keeps the old flat prices.
\tstruct TPlayerBotGearPrice { DWORD dwBaseVnum; WORD wMerchantMask; DWORD adwPrice[10]; };
\tconst TPlayerBotGearPrice PLAYERBOT_GEAR_PRICES[] = {
%(gear)s\t};

\t// Upgrade materials ("ULEPSZACZE"), and then everything else he prices by
\t// name - the Moonlight chest, the Blessing Scroll, the horse medal, herbs,
\t// guild materials, ores and smelted ores - in the second table.
\tstruct TPlayerBotMaterialPrice { DWORD dwVnum; DWORD dwPrice; };
\tconst TPlayerBotMaterialPrice PLAYERBOT_MATERIAL_PRICES[] = {
%(materials)s\t};
\tconst TPlayerBotMaterialPrice PLAYERBOT_EXTRA_MATERIAL_PRICES[] = {
%(extras)s\t};

\t// Skill books by the skill in socket 0, before the per-listing jitter.
\tstruct TPlayerBotBookPrice { DWORD dwSkill; DWORD dwPrice; };
\tconst TPlayerBotBookPrice PLAYERBOT_BOOK_PRICES[] = {
%(books)s\t};

\t// Soul stones by kind and grade. The kinds are the last two digits of the
\t// vnum (30-37 weapon, 38-43 armour) and the grade its hundreds digit.
\tstruct TPlayerBotSoulStonePrice { int iKind; int iGrade; DWORD dwPrice; };
\tconst TPlayerBotSoulStonePrice PLAYERBOT_SOUL_STONE_PRICES[] = {
%(stones)s\t};
\t// What a stone of that grade is worth when its kind is not named above.
\t// Grade four has no general price in his table: every +4 is listed by name.
\tconst DWORD PLAYERBOT_SOUL_STONE_GRADE_PRICES[5] = { %(grades)s };

\t// A stone seated in a weapon or armour raises what the piece is worth.
\t// First by how many are in it, then by which ones (percent, 100 = x1.0).
\tconst int PLAYERBOT_SOCKET_COUNT_PERCENT[4] = { %(counts)s };
\tstruct TPlayerBotSocketStoneMultiplier { int iKind; int iGrade; int iPercent; };
\tconst TPlayerBotSocketStoneMultiplier PLAYERBOT_SOCKET_STONE_PERCENT[] = {
%(mults)s\t};

\t// Polymorph marbles, by the monster in socket 0. Anything not named here
\t// is drawn from the band below, stable per marble.
\tstruct TPlayerBotMarblePrice { DWORD dwMob; DWORD dwPrice; };
\tconst TPlayerBotMarblePrice PLAYERBOT_MARBLE_PRICES[] = {
%(marbles)s\t};
\tconst DWORD PLAYERBOT_MARBLE_PRICE_MIN = %(marble_min)d;
\tconst DWORD PLAYERBOT_MARBLE_PRICE_MAX = %(marble_max)d;

\t// Forgetting Scrolls by the skill in the socket. A price of zero is his
\t// "do sprzedazy u handlarki": that one is the merchant's, not a counter's.
\tstruct TPlayerBotForgetScrollPrice { DWORD dwSkill; DWORD dwPrice; };
\tconst TPlayerBotForgetScrollPrice PLAYERBOT_FORGET_SCROLL_PRICES[] = {
%(scrolls)s\t};

\t// His bonus multipliers: per slot, per line, one multiplier for the maximum
\t// roll and one for any other value, the races split at level 33; a weapon's
\t// two damage lines by tiers of their value. They compound into the asking
\t// price (GetPlayerBotBonusPricePercent). "Maximum" is the engine's own:
\t// g_map_itemAttr's top value for the apply on the item's attribute set.
\t// Lines his sheet does not name, or names at x1.0, multiply by nothing. The
\t// percent points are hundredths: 250 is x2.5.
\tenum EPlayerBotPriceSlot
\t{
\t\tPRICE_SLOT_HEAD = 1, PRICE_SLOT_BODY = 2, PRICE_SLOT_SHIELD = 4, PRICE_SLOT_FOOTS = 8,
\t\tPRICE_SLOT_WRIST = 16, PRICE_SLOT_NECK = 32, PRICE_SLOT_EAR = 64, PRICE_SLOT_WEAPON = 128
\t};
\tstruct TPlayerBotBonusPriceRow
\t{
\t\tBYTE bSlots;      // EPlayerBotPriceSlot mask
\t\tBYTE bApply;      // APPLY_*
\t\tWORD wMaxPct;     // the maximum roll, hundredths
\t\tWORD wOtherPct;   // any other value, hundredths
\t\tBYTE bMinLevel;   // the item's level limit band, inclusive
\t\tBYTE bMaxLevel;
\t};
\tconst TPlayerBotBonusPriceRow PLAYERBOT_BONUS_PRICE_ROWS[] = {
%(bonus)s\t};
\t// A weapon's average and skill damage, by tier of the value.
\tstruct TPlayerBotDamageTier { BYTE bFrom; WORD wPct; };
\tconst TPlayerBotDamageTier PLAYERBOT_AVERAGE_DAMAGE_TIERS[] = {
\t\t%(avg)s
\t};
\tconst TPlayerBotDamageTier PLAYERBOT_SKILL_DAMAGE_TIERS[] = {
\t\t%(skill)s
\t};
}

#endif
'''


def bonus_rows_text(merged):
    common, mt2009 = [], []
    for (apply_name, max_pct, other_pct, lo, hi), (slots, names) in sorted(
            merged.items(), key=lambda kv: (kv[0][0], kv[0][3], min(SLOT_ORDER.index(s) for s in kv[1][0]))):
        mask = ' | '.join(s for s in SLOT_ORDER if s in slots)
        line = '\t\t{ %s, %s, %d, %d, %d, %d },\t// %s\n' % (
            mask, apply_name, max_pct, other_pct, lo, hi, ascii_comment(sorted(names)[0]))
        (mt2009 if apply_name in MT2009_ONLY else common).append(line)
    text = ''.join(common)
    if mt2009:
        text += '#if defined(PLAYERBOT_ENGINE_MT2009)\n' + ''.join(mt2009) + '#endif\n'
    return text


def main(item_path, mob_path, attr_path, sheet_path, out_path):
    resolver = Resolver(load_hex(item_path), load_hex(mob_path))
    item_attr = load_item_attr(attr_path)
    if len(item_attr) < 30:
        raise SystemExit('item_attr.tsv: tylko %d bonusow - to nie jest zrzut world.item_attr' % len(item_attr))
    sheet = Sheet(sheet_path)
    errors = []

    rates = parse_rate_points(sheet)
    mults, count_mult = parse_socket_multipliers(sheet)
    bonus, tiers = parse_bonus_multipliers(sheet, item_attr, errors)
    materials = parse_goods(sheet, resolver, ['ULEPSZACZE'])
    extras = parse_goods(sheet, resolver, ['Szkatułki', 'Inne', 'Ulepszanie', 'Pasywne',
                                           'Koń', 'Łowienie', 'Zielarstwo', 'Gildia', 'RUDY'])
    overlap = set(v for v, _, _ in materials) & set(v for v, _, _ in extras)
    if overlap:
        errors.append('vnumy w ulepszaczach i w innych naraz: %s' % sorted(overlap))
    books = parse_books(sheet, resolver)
    scrolls = parse_scrolls(sheet, resolver)
    stones, grade_default = parse_soul_stones(sheet, resolver)
    marbles, (marble_min, marble_max) = parse_marbles(sheet, resolver)
    gear = parse_gear(sheet, resolver, errors)

    problems = resolver.missing + errors
    if problems:
        sys.stderr.write('generate_iwakura_prices: %d problemow:\n' % len(problems))
        for item in problems:
            sys.stderr.write('  %s\n' % item)
        raise SystemExit(1)

    body = HEADER % {
        'rates': ''.join('\t\t{ %5d, %5d },\n' % point for point in rates),
        'gear': ''.join('\t\t{ %5d, 0x%03x, { %s } },\t// %s\n'
                        % (vnum, mask, ', '.join('%d' % p for p in prices), ascii_comment(family))
                        for vnum, mask, prices, family in sorted(gear)),
        'materials': ''.join('\t\t{ %5d, %8d },\t// %s\n' % (v, p, ascii_comment(n)) for v, p, n in materials),
        'extras': ''.join('\t\t{ %5d, %8d },\t// %s\n' % (v, p, ascii_comment(n)) for v, p, n in extras),
        'books': ''.join('\t\t{ %3d, %7d },\t// %s\n' % (s, p, ascii_comment(n)) for s, p, n in books),
        'stones': ''.join('\t\t{ %2d, %d, %8d },\n' % row for row in stones),
        'grades': ', '.join('%d' % grade_default.get(g, 0) for g in range(5)),
        'counts': ', '.join('%d' % pct(count_mult[i]) for i in range(4)),
        'mults': ''.join('\t\t{ %2d, %d, %3d },\n' % (k, g, pct(f)) for k, g, f in sorted(mults)),
        'marbles': ''.join('\t\t{ %5d, %7d },\t// %s\n' % (v, p, ascii_comment(n)) for v, p, n in marbles),
        'marble_min': marble_min,
        'marble_max': marble_max,
        'scrolls': ''.join('\t\t{ %3d, %7d },\t// %s\n' % (s, p, ascii_comment(n)) for s, p, n in scrolls),
        'bonus': bonus_rows_text(bonus),
        'avg': ', '.join('{ %d, %d }' % t for t in tiers['avg']),
        'skill': ', '.join('{ %d, %d }' % t for t in tiers['skill']),
    }
    body.encode('ascii')  # the overlay is ASCII; fail here rather than in the build
    with io.open(out_path, 'w', encoding='ascii', newline='\n') as handle:
        handle.write(body)
    print('zapisano %s' % out_path)
    print('  punkty skalowania: %s' % ', '.join('%d%%=x%.2f' % (r, p / 100.0) for r, p in rates))
    print('  rodzin sprzetu: %d (z pasmami "do handlarki": %d)' % (len(gear), sum(1 for g in gear if g[1])))
    print('  ulepszaczy: %d vnumow, innych: %d, ksiag: %d, opasek: %d' % (len(materials), len(extras), len(books), len(scrolls)))
    print('  kamieni duszy (wyjatki): %d, mnoznikow socketow: %d, marmurow: %d' % (len(stones), len(mults), len(marbles)))
    print('  wierszy bonusow: %d (tylko mt2009: %d), progi srednich %d, umiejetnosci %d'
          % (len(bonus), sum(1 for k in bonus if k[0] in MT2009_ONLY), len(tiers['avg']), len(tiers['skill'])))


if __name__ == '__main__':
    if len(sys.argv) != 6:
        raise SystemExit(__doc__)
    main(*sys.argv[1:6])
