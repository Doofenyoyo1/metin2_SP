# -*- coding: utf-8 -*-
"""Renderuje playerbot_shop_names.h z listy nazw sklepow Iwakury.

Jego plik (data/iwakura_nazwy_sklepow.txt, 14 wrzesnia) to reguly na gorze
i siedem list: NEUTRALNE, KU, RYBY, DO SPALENIA, ULEPSZACZE, INNE i EKWIPUNEK.
Nazwy trafiaja do naglowka w cp1250; reguly wyboru - wedlug towaru, 33% na
nazwe neutralna, nazwa z przedmiotu +7..+9, z kamienia duszy i z ulepszacza -
sa w playerbot_shop_name_rules.h. Liczby z regul (33%, x1.5, x1.4) skrypt czyta
z jego tekstu i przerywa, gdy sformulowanie sie zmieni.

Wejscie to jego plik i zrzut world.item_proto, ten sam co dla cennika:

    docker exec -i <db> sh -c 'exec mariadb -uroot -p"$MARIADB_ROOT_PASSWORD" -N' > item_proto_hex.tsv
      SELECT vnum, HEX(locale_name), type, subtype, COALESCE(limitvalue0,0) FROM world.item_proto ORDER BY vnum;

Uzycie (z katalogu linux-port/overlays/playerbot):
    python tools/generate_shop_names.py item_proto_hex.tsv data/iwakura_nazwy_sklepow.txt \\
        src/game/src/playerbot_shop_names.h

Nazwa jest zapisywana dokladnie tak, jak pokaze ja silnik.
ikashop::CShopManager::ParseShopName przepuszcza ja przez EscapeString, tnie
wynik do SHOP_SIGN_MAX_LEN (32) i odrzuca nazwe ze slowem z world.banword albo
ze znakiem spoza has_proper_characters (drukowalne ASCII i osiemnascie polskich
liter). Nazwe, ktorej silnik by nie przyjal albo pokazal inaczej, skrypt
najpierw obcina z ozdobnikow po obu stronach naraz (">>> ... <<<",
"@@@@@ ... @@@@@"), a gdy to nie wystarcza - pomija. Obie listy wypisuje na
koniec i zapisuje w komentarzu naglowka.

Nazwa, ktora mowi o konkretnym towarze ("ZEBY ORKA TANIO"), jest powiazana
z nim w POWIAZANIA i losuje sie tylko nad lada, na ktorej ten towar lezy.
Skrypt **przerywa z bledem**, gdy powiazanie wskazuje przedmiot, ktorego
item_proto nie zna, nazwe, ktorej nie ma na liscie, albo gdy nazwa z [INNE]
nie jest z niczym powiazana.
"""
import io
import re
import sys

SIGN_MAX = 32  # SHOP_SIGN_MAX_LEN i OFFLINE_SHOP_NAME_MAX_LEN, common/length.h

# has_proper_characters (common/utils.h): drukowalne ASCII i te bajty cp1250,
# czyli Ą ą Ć ć Ę ę Ł ł Ń ń Ó ó Ś ś Ź ź Ż ż.
PROPER = set(range(0x20, 0x7F)) | set(bytes.fromhex('A5B9C6E6CAEAA3B3D1F1D3F38C9C8F9FAFBF'))

# world.banword paczki mt2009, wszystkie 42 slowa. CBanwordManager::CheckString
# szuka kazdego jako podciagu nazwy zamienionej na male litery ASCII.
BANWORDS = [
    'admin', 'chuj', 'cunt', 'cwel', 'dziwk', 'gameadmin', 'gamemaster', 'jebac', 'jebal',
    'jebie', 'kurev', 'kurew', 'kurv', 'kurw', 'kutas', 'lcmlsizoni', 'lgalpawke', 'lgmlceres',
    'lgmlmioxim', 'lgmlxylerxr', 'niger', 'nigger', 'penis', 'porn', 'pussy', 'qrev', 'qrew',
    'qrva', 'qrve', 'qrvi', 'qrvy', 'qrwa', 'qrwe', 'qrwi', 'qrwy', 'shit', 'slut', 'sperma',
    'spermi', 'spermo', 'srac', 'zjeb',
]

# Ozdobniki, ktore wolno obciac z obu koncow naraz. Bez nawiasow, dwukropka
# i srednika - to sa buzki, a nie ramka.
DECOR = set('@<>-~$!=*#/\\|.')

SECTIONS = [
    ('NEUTRALNE', 'SIGN_KIND_NEUTRAL'),
    ('KU', 'SIGN_KIND_BOOKS'),
    ('RYBY', 'SIGN_KIND_FISH'),
    ('DO SPALENIA', 'SIGN_KIND_SCRAP'),
    ('ULEPSZACZE', 'SIGN_KIND_MATERIALS'),
    ('INNE', 'SIGN_KIND_OTHER'),
    ('EKWIPUNEK', 'SIGN_KIND_GEAR'),
]

NEEDS = [
    'SIGN_NEED_NONE', 'SIGN_NEED_ALL_GOODS', 'SIGN_NEED_ANY_GOODS', 'SIGN_NEED_ORES',
    'SIGN_NEED_BOOK_SKILLS', 'SIGN_NEED_BOOK_CLASSES', 'SIGN_NEED_TOP_GEAR', 'SIGN_NEED_LOW_GEAR',
    'SIGN_NEED_HIGH_GEAR', 'SIGN_NEED_BODY_ARMOUR', 'SIGN_NEED_GEAR_KINDS', 'SIGN_NEED_FIRST_VILLAGE',
]

# Klasy jako bity, w kolejnosci JOB_* silnika.
CLASS_BITS = {'wojownik': 1, 'ninja': 2, 'sura': 4, 'szaman': 8}

# Nazwa, pod ktora gra ma kilka przedmiotow, a lista mowi o jednym.
VNUM_OVERRIDES = {
    # 25041 nazywa sie tak samo, ale to Magiczny Kamien (patrz cennik).
    'zwój błogosławieństwa': [25040],
}

# Nazwa z listy (male litery, pojedyncze spacje) -> czego wymaga nad lada.
#   ('ALL', grupy)  - kazda grupa ma linie na ladzie
#   ('ANY', grupy)  - co najmniej jedna grupa ma
#   grupa to nazwy przedmiotow z item_proto; kazdy vnum o tej nazwie sie liczy
#   ('ORES',), ('TOP_GEAR',), ('BODY_ARMOUR',), ('GEAR_KINDS',), ('FIRST_VILLAGE',)
#   ('BOOK_SKILLS', [vnumy umiejetnosci]), ('BOOK_CLASSES', [klasy])
#   ('LOW_GEAR', poziom) - sprzet ponizej tego poziomu; ('HIGH_GEAR', poziom) - od niego
# "i" i "|" miedzy towarami to ALL, "itp." i warianty jednego towaru to ANY.
POWIAZANIA = {
    # [ULEPSZACZE]
    'kawalki klejonu | ulepy z m2': ('ANY', [['kawałek klejnotu']]),
    'kawalki klejnotu najtaniej': ('ANY', [['kawałek klejnotu']]),
    'kawalek klejnotu i zardzewiale ostrze': ('ALL', [['kawałek klejnotu'], ['zardzewiałe ostrze']]),
    'zardzewiale ostrze | czarny uniform': ('ALL', [['zardzewiałe ostrze'], ['czarny uniform', 'czarny uniform+']]),
    '@@@@ księgi klątw @@@@': ('ANY', [['księga klątw', 'księga klątw+']]),
    '$$$ ksiegi klatw do biologa $$$': ('ANY', [['księga klątw', 'księga klątw+']]),
    'zęby orka zęby orka zęby orka': ('ANY', [['ząb orka', 'ząb orka+']]),
    'zęby orka tanio okazja!!!': ('ANY', [['ząb orka', 'ząb orka+']]),
    'pajecze sieci i oczy pajaka': ('ALL', [['sieć pająka pustynnego'], ['oczy pająka', 'oko pająka']]),
    'oczy pająka tanio!': ('ANY', [['oczy pająka', 'oko pająka']]),
    '@ @ @ amulety orka @ @ @': ('ANY', [['amulet orka', 'amulet orka+']]),
    'biała wstęga | kłąb itp.': ('ANY', [['biała wstęga', 'biała wstęga+'], ['kłąb']]),
    '>>> żółć niedźwiedzia <<<': ('ANY', [['żółć niedźwiedzia', 'żółć niedźwiedzia+']]),
    'matowe lody dla ochłody': ('ANY', [['matowy lód', 'matowy lód+']]),
    '@ @ @ shurikeny @ @ @': ('ANY', [['shuriken', 'shuriken+']]),
    'ogon węża z plusem !!!': ('ANY', [['ogon węża+']]),
    'worek z pajęczą trucizną': ('ANY', [['worek z pajęczą trucizną']]),
    'oczy, sieci, worki - pająki': ('ANY', [['oczy pająka', 'oko pająka'], ['sieć pająka pustynnego'],
                                            ['worek z pajęczą trucizną', 'worek z pajęcz. jajami']]),
    'pamiątki po demonie': ('ANY', [['pamiątka po demonie', 'pamiątka po demonie+']]),
    '@@@@@ pamiątki po demonie @@@@@': ('ANY', [['pamiątka po demonie', 'pamiątka po demonie+']]),
    'nieznane leki z + i bez': ('ANY', [['nieznane lekarstwo', 'nieznane lekarstwo+']]),
    'klejnoty demona i pamiątki': ('ALL', [['klejnot demona', 'klejnot demona+'],
                                           ['pamiątka po demonie', 'pamiątka po demonie+']]),
    'klejnoty pamiątki dt ulepy': ('ANY', [['klejnot demona', 'klejnot demona+'],
                                           ['pamiątka po demonie', 'pamiątka po demonie+']]),
    'krem do twarzy': ('ANY', [['krem do twarzy']]),
    'liście i języki żab': ('ALL', [['liść'], ['język żaby']]),
    # [RYBY]
    'malze taniej niż obok >>>>>': ('ANY', [['małż']]),
    'traf białą perłę!': ('ANY', [['małż'], ['biała perła']]),
    'małże z nocnego boc---- łowienia': ('ANY', [['małż']]),
    '@ @ @ perły będą twoje @ @ @': ('ANY', [['małż'], ['biała perła', 'niebieska perła', 'krwawa perła']]),
    'świeży połów karpia': ('ANY', [['karp', 'lustrzany karp']]),
    # [INNE]
    'rudy i przetopy': ('ORES',),
    '@ @ @ medale konne @ @ @': ('ANY', [['medal konny']]),
    'm e d a l e k o n n e': ('ANY', [['medal konny']]),
    'zwoje blogoslawienstwa': ('ANY', [['zwój błogosławieństwa']]),
    'zwoje blogoslawienstwa tanio!': ('ANY', [['zwój błogosławieństwa']]),
    'z w o j e błogosławieństwa :)': ('ANY', [['zwój błogosławieństwa']]),
    'błogosławieństwo od alicji ;p': ('ANY', [['zwój błogosławieństwa']]),
    # [KU]
    'aura miecza czarowane silne ciało': ('BOOK_SKILLS', [4, 63, 19]),
    '@@@@@@ ku woj sura ninja @@@@@': ('BOOK_CLASSES', ['wojownik', 'sura', 'ninja']),
    'ku dla każdej klasy postaci': ('BOOK_CLASSES', ['wojownik', 'ninja', 'sura', 'szaman']),
    'biblioteka publiczna m1': ('FIRST_VILLAGE',),
    # [EKWIPUNEK]
    'eq +7/+8/+9': ('TOP_GEAR',),
    'tarcze zbroje bronie i inne': ('GEAR_KINDS',),
    # Gora Sohan to na tym swiecie potwory od 49 do 66 poziomu.
    '----- nie marznij na sohan -----': ('HIGH_GEAR', 45),
    # Dzikie psy to pierwsza wioska; "niski" sprzet to na targu wszystko ponizej 30.
    'zestaw przetrwania na dzikie psy': ('LOW_GEAR', 30),
    'zmień szmaty na zbroje!': ('BODY_ARMOUR',),
}

_ASCII = {
    0x104: 'A', 0x105: 'a', 0x106: 'C', 0x107: 'c', 0x118: 'E', 0x119: 'e',
    0x141: 'L', 0x142: 'l', 0x143: 'N', 0x144: 'n', 0xd3: 'O', 0xf3: 'o',
    0x15a: 'S', 0x15b: 's', 0x179: 'Z', 0x17a: 'z', 0x17b: 'Z', 0x17c: 'z',
}


def ascii_comment(text):
    return ''.join(_ASCII.get(ord(ch), ch if ord(ch) < 128 else '?') for ch in text)


def norm(text):
    return re.sub(r'\s+', ' ', text.replace(' ', ' ')).strip().lower()


def load_items(path):
    names = {}
    for line in io.open(path, encoding='latin-1'):
        parts = line.rstrip('\n').split('\t')
        if len(parts) < 2:
            continue
        try:
            vnum = int(parts[0])
            name = bytes.fromhex(parts[1]).decode('cp1250')
        except ValueError:
            continue
        names.setdefault(norm(name), []).append(vnum)
    return names


def parse_list(path):
    text = io.open(path, encoding='utf-8-sig').read().replace('\r\n', '\n')
    known = dict(SECTIONS)
    intro, sections, current = [], {}, None
    for line in text.split('\n'):
        stripped = line.strip()
        m = re.match(r'^\[([^\]]+)\]$', stripped)
        if m:
            title = m.group(1).strip()
            if title not in known:
                raise SystemExit('nieznana kategoria [%s]' % title)
            if title in sections:
                raise SystemExit('kategoria [%s] wystepuje dwa razy' % title)
            sections[title] = []
            current = title
            continue
        if current is None:
            intro.append(line)
        elif stripped:
            sections[current].append(stripped)
    missing = [t for t, _ in SECTIONS if t not in sections]
    if missing:
        raise SystemExit('brak kategorii: %s' % ', '.join(missing))
    return '\n'.join(intro), sections


def parse_rules(intro):
    flat = re.sub(r'\s+', ' ', intro)
    m = re.search(r'istnieje (\d+)% szans', flat)
    if not m:
        raise SystemExit('reguly: nie ma "istnieje N% szans" - sformulowanie sie zmienilo')
    neutral = int(m.group(1))
    mins = re.findall(r'min\. ?([\d.]+)x', flat)
    if len(mins) != 2:
        raise SystemExit('reguly: oczekiwane dwa progi "min. N.Nx" (bonus, KD), jest %r' % mins)
    bonus_pct, stones_pct = [int(round(float(v) * 100)) for v in mins]
    for needle in ('+7, +8 lub +9', 'od +0 do +3', 'od +0 do +4', 'TANIO lub OKAZJA',
                   'Tanio/TANIO/tanio lub Okazja/OKAZJA/okazja'):
        if needle not in flat:
            raise SystemExit('reguly: brak "%s" - regula sie zmienila, sprawdz generator i playerbot_shop_name_rules.h'
                             % needle)
    return neutral, bonus_pct, stones_pct


def shown(raw):
    """Co zostaje z nazwy po ParseShopName: EscapeString i obciecie do 32."""
    out = bytearray()
    for b in raw:
        if b in (0x5C, 0x27, 0x22):
            out.append(0x5C)
        out.append(b)
    return bytes(out[:SIGN_MAX])


def refusal(text):
    try:
        raw = text.encode('cp1250')
    except UnicodeEncodeError as e:
        return 'znak spoza cp1250: %r' % text[e.start]
    bad = sorted(set(chr(b) if b < 0x80 else raw[i:i + 1].decode('cp1250')
                     for i, b in enumerate(raw) if b not in PROPER))
    if bad:
        return 'znak, ktorego has_proper_characters nie przyjmie: %r' % ''.join(bad)
    if shown(raw) != raw:
        return 'silnik pokazalby %d z %d znakow' % (len(shown(raw)), len(raw))
    low = bytes(b + 32 if 65 <= b <= 90 else b for b in raw)
    for word in BANWORDS:
        if word.encode('ascii') in low:
            return 'zakazane slowo "%s"' % word
    return None


def fit(text):
    why = refusal(text)
    if why is None:
        return text, None
    first = why
    trimmed = text
    while why is not None and len(trimmed) > 2 and trimmed[0] in DECOR and trimmed[-1] in DECOR:
        trimmed = trimmed[1:-1].strip()
        why = refusal(trimmed)
    return (trimmed, None) if why is None else (None, first)


def resolve(binding, items, name, errors):
    if binding is None:
        return 'SIGN_NEED_NONE', []
    need = binding[0]
    if need in ('ALL', 'ANY'):
        groups = []
        for group in binding[1]:
            vnums = []
            for item in group:
                key = norm(item)
                found = VNUM_OVERRIDES.get(key) or items.get(key)
                if not found:
                    errors.append('"%s": przedmiotu "%s" nie ma w item_proto' % (name, item))
                    continue
                vnums.extend(found)
            vnums = sorted(set(vnums))
            if len(vnums) > 4:
                errors.append('"%s": grupa %r to %d vnumow, miesci sie 4' % (name, group, len(vnums)))
            groups.append(vnums[:4])
        if len(groups) > 3:
            errors.append('"%s": %d grup, mieszcza sie 3' % (name, len(groups)))
        return ('SIGN_NEED_ALL_GOODS' if need == 'ALL' else 'SIGN_NEED_ANY_GOODS'), groups[:3]
    if need == 'BOOK_SKILLS':
        return 'SIGN_NEED_BOOK_SKILLS', [list(binding[1])[:4]]
    if need == 'BOOK_CLASSES':
        return 'SIGN_NEED_BOOK_CLASSES', [[sum(CLASS_BITS[c] for c in binding[1])]]
    if need in ('LOW_GEAR', 'HIGH_GEAR'):
        return 'SIGN_NEED_' + need, [[int(binding[1])]]
    if 'SIGN_NEED_' + need not in NEEDS:
        errors.append('"%s": nieznany wymog %s' % (name, need))
        return 'SIGN_NEED_NONE', []
    return 'SIGN_NEED_' + need, []


def c_literal(raw):
    out = '"'
    after_hex = False
    for b in raw:
        if b >= 0x80:
            out += '\\x%02X' % b
            after_hex = True
            continue
        ch = chr(b)
        if after_hex and ch in '0123456789abcdefABCDEF':
            out += '" "'
        after_hex = False
        if ch == '\\':
            out += '\\\\'
        elif ch == '"':
            out += '\\"'
        elif ch == '?':
            out += '\\?'
        else:
            out += ch
    return out + '"'


def goods_init(groups):
    cells = []
    for g in range(3):
        vnums = (groups[g] if g < len(groups) else []) + [0, 0, 0, 0]
        cells.append('{ { %s } }' % ', '.join(str(v) for v in vnums[:4]))
    return '{ %s }' % ', '.join(cells)


def main(argv):
    if len(argv) != 4:
        raise SystemExit(__doc__)
    items = load_items(argv[1])
    intro, sections = parse_list(argv[2])
    neutral_pct, bonus_pct, stones_pct = parse_rules(intro)

    errors, entries, trimmed, dropped = [], [], [], []
    used = set()
    for title, kind in SECTIONS:
        seen = set()
        for name in sections[title]:
            key = norm(name)
            binding = POWIAZANIA.get(key)
            if binding is not None:
                used.add(key)
            elif title == 'INNE':
                errors.append('[INNE] "%s": nazwa bez powiazania z towarem' % name)
            need, groups = resolve(binding, items, name, errors)
            final, why = fit(name)
            if final is None:
                dropped.append((title, name, why))
                continue
            if final != name:
                trimmed.append((title, name, final))
            if final in seen:
                continue
            seen.add(final)
            entries.append((kind, need, groups, final, title))
    for key in POWIAZANIA:
        if key not in used:
            errors.append('powiazanie dla nazwy, ktorej nie ma na liscie: "%s"' % key)
    if errors:
        raise SystemExit('\n'.join(errors))

    lines = []
    w = lines.append
    w("// Rendered by linux-port/overlays/playerbot/tools/generate_shop_names.py from Iwakura's list")
    w('// of shop names (data/iwakura_nazwy_sklepow.txt). DO NOT EDIT; edit the list and re-run.')
    w('//')
    w('// Every name is his, byte for byte in CP1250 - the engine\'s own encoding for a shop')
    w('// name (player.ikashop_offlineshop.name is cp1250_polish_ci) - and every one is what')
    w('// ikashop::CShopManager::ParseShopName leaves of it unchanged: at most SHOP_SIGN_MAX_LEN')
    w('// characters after EscapeString, nothing has_proper_characters refuses, no word from')
    w('// world.banword. A name that failed was trimmed of the decoration on both of its ends')
    w('// when that was enough and left out when it was not; both are listed below, so the list')
    w('// can be fixed where it is written. Which name a counter gets is decided in')
    w('// playerbot_shop_name_rules.h.')
    if trimmed:
        w('//')
        w('// Trimmed to fit a sign:')
        for title, name, final in trimmed:
            w('//   [%s] "%s" -> "%s"' % (title, ascii_comment(name), ascii_comment(final)))
    if dropped:
        w('//')
        w('// Left out - the engine would refuse or cut them:')
        for title, name, why in dropped:
            w('//   [%s] "%s" (%s)' % (title, ascii_comment(name), ascii_comment(why)))
    w('#ifndef PLAYERBOT_SHOP_NAMES_H')
    w('#define PLAYERBOT_SHOP_NAMES_H')
    w('#include <cstddef>')
    w('#include <cstdint>')
    w('')
    w('namespace playerbot_shop_names')
    w('{')
    w('\t// His seven lists.')
    w('\tenum ESignKind : uint8_t')
    w('\t{')
    for title, kind in SECTIONS:
        w('\t\t%s,\t// [%s]' % (kind, title))
    w('\t\tSIGN_KIND_COUNT')
    w('\t};')
    w('')
    w('\t// What a name says about the goods, so that it is drawn only over a counter that')
    w('\t// has them. The one number some of them need is aGoods[0].adwVnum[0].')
    w('\tenum ESignNeed : uint8_t')
    w('\t{')
    comments = {
        'SIGN_NEED_NONE': 'anything of its list',
        'SIGN_NEED_ALL_GOODS': 'a line of every group in aGoods',
        'SIGN_NEED_ANY_GOODS': 'a line of one of them',
        'SIGN_NEED_ORES': 'ore, raw or smelted',
        'SIGN_NEED_BOOK_SKILLS': 'a book of one of the skills in aGoods[0]',
        'SIGN_NEED_BOOK_CLASSES': 'books of every class in the mask (warrior 1, ninja 2, sura 4, shaman 8)',
        'SIGN_NEED_TOP_GEAR': 'a weapon or armour at +7 to +9',
        'SIGN_NEED_LOW_GEAR': 'a weapon or armour under that level',
        'SIGN_NEED_HIGH_GEAR': 'a weapon or armour from that level',
        'SIGN_NEED_BODY_ARMOUR': 'a body armour',
        'SIGN_NEED_GEAR_KINDS': 'a weapon, a body armour and a shield',
        'SIGN_NEED_FIRST_VILLAGE': 'the counter stands in a first village',
    }
    for need in NEEDS:
        w('\t\t%s,\t// %s' % (need, comments[need]))
    w('\t\tSIGN_NEED_COUNT')
    w('\t};')
    w('')
    w('\tstruct TSignGoods { uint32_t adwVnum[4]; };')
    w('\tstruct TSignName')
    w('\t{')
    w('\t\tuint8_t bKind;')
    w('\t\tuint8_t bNeed;')
    w('\t\tuint8_t bGoods;\t\t// groups used in aGoods')
    w('\t\tconst char* szName;\t// CP1250')
    w('\t\tTSignGoods aGoods[3];')
    w('\t};')
    w('')
    w('\t// The numbers in the rules at the head of his list.')
    w('\tconst size_t SIGN_MAX_LEN = %d;' % SIGN_MAX)
    w('\tconst int SIGN_NEUTRAL_OVERRIDE_PERCENT = %d;\t// "istnieje %d%% szans"' % (neutral_pct, neutral_pct))
    w('\tconst int SIGN_BONUS_MIN_PCT = %d;\t// a bonus line named over +7..+9 gear' % bonus_pct)
    w('\tconst int SIGN_STONES_MIN_PCT = %d;\t// "KD" named over it' % stones_pct)
    w('\tconst int SIGN_TOP_GEAR_MIN_PLUS = 7;')
    w('\tconst int SIGN_SCRAP_MAX_PLUS = 3;')
    w('\tconst char* const SIGN_GEAR_SUFFIXES[] = { "TANIO", "OKAZJA" };')
    w('\tconst char* const SIGN_GOODS_SUFFIXES[] = { "Tanio", "TANIO", "tanio", "Okazja", "OKAZJA", "okazja" };')
    w('')
    w('\tconst TSignName SIGN_NAMES[] = {')
    for kind, need, groups, final, title in entries:
        raw = final.encode('cp1250')
        # W cudzyslowie: "zakupy \\\" na koncu komentarza // bez niego to
        # kontynuacja linii i kompilator polknalby nastepna nazwe.
        w('\t\t{ %s, %s, %d, %s, %s },\t// "%s"' % (kind, need, len(groups), c_literal(raw),
                                                    goods_init(groups), ascii_comment(final)))
    w('\t};')
    w('\tconst size_t SIGN_NAME_COUNT = sizeof(SIGN_NAMES) / sizeof(SIGN_NAMES[0]);')
    w('}')
    w('')
    w('#endif')
    io.open(argv[3], 'w', encoding='ascii', newline='\n').write('\n'.join(lines) + '\n')

    per_kind = {}
    for kind, need, groups, final, title in entries:
        per_kind.setdefault(title, [0, 0])
        per_kind[title][0] += 1
        if need != 'SIGN_NEED_NONE':
            per_kind[title][1] += 1
    print('zapisano %s: %d nazw' % (argv[3], len(entries)))
    for title, _ in SECTIONS:
        total, bound = per_kind.get(title, [0, 0])
        print('  [%s] %d nazw, %d powiazanych z towarem' % (title, total, bound))
    print('  reguly: %d%% neutralnych, bonus od x%.2f, KD od x%.2f' % (neutral_pct, bonus_pct / 100.0, stones_pct / 100.0))
    for title, name, final in trimmed:
        print('  przycieta [%s] "%s" -> "%s"' % (title, name, final))
    for title, name, why in dropped:
        print('  pominieta [%s] "%s": %s' % (title, name, why))


if __name__ == '__main__':
    main(sys.argv)
