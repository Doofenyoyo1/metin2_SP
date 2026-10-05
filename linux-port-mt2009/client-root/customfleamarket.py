# FLEA_UI_V2 FLEA_UI_V4 FLEA_UI_V5 FLEA_UI_V6 - nowe okno "Dom Towarowy" (kategorie po lewej, filtry, wyszukiwanie po stronie serwera)
import re
import bisect
import ui
import ikashop
import item
import player
import localeinfo_point
import skill
import net
import app
import wndMgr
import localeInfo
import uiCommon
import player
import chat
from _weakref import proxy

YANG_PER_CHEQUE = 100000000

# CUSTOM_FLEA_SEARCH_PL_V1: zwykle str.lower() (Python 2, bajty CP1250) sklada
# sie tylko z ASCII A-Z, wiec polskie znaki diakrytyczne (Z, O, L, C z akcentem)
# nie byly zamieniane na male litery - przedmiot zaczynajacy sie od wielkiej
# litery z akcentem (np. "Zolc Niedzwiedzia") nigdy nie pasowal do zapytania
# wpisanego mala litera, mimo ze serwer juz poprawnie dopasowywal ta sama
# nazwe (patrz StringToLower w ikarus_shop_manager.cpp).
_PL_LOWER_MAP = {
	'\xa5': '\xb9', '\xc6': '\xe6', '\xca': '\xea', '\xa3': '\xb3',
	'\xd1': '\xf1', '\xd3': '\xf3', '\x8c': '\x9c', '\x8f': '\x9f', '\xaf': '\xbf',
}
def PolishLower(text):
	lowered = text.lower()
	return "".join(_PL_LOWER_MAP.get(ch, ch) for ch in lowered)

# CUSTOM_FLEA_PLUS_SEARCH_V1: wyszukiwanie ulepszacza bez "+" w zapytaniu
# (np. "miecz") dawniej ciagnelo zarowno baze jak i wszystkie warianty
# +1..+9, wiec zeby trafic na baze trzeba bylo przewinac dziesiatki ofert -
# to byl jeden z najczesciej zglaszanych feedbackow. Teraz brak "+" w
# zapytaniu pokazuje tylko nieulepszone przedmioty; wpisanie "+" (np.
# "miecz+" albo "miecz+3") wraca do zwyklego, pelnego dopasowania substring.
_PLUS_SUFFIX_RE = re.compile(r'\+\d*$')
def _HasPlusSuffix(name):
	return bool(_PLUS_SUFFIX_RE.search(name))

COLOR_TEXT = 0xFFEDEDED
COLOR_DIM = 0xFF9A9A9A
COLOR_GOLD = 0xFFFFD56A
COLOR_GREEN = 0xFF6EDC82
COLOR_GREEN_DARK = 0xFF2A8F4E
COLOR_HEAD = 0xFFE8E8E8
COLOR_BONUS = 0xFF8FD6FF
COLOR_CARD_BORDER = 0xFF4A4A4A
COLOR_CARD_FILL = 0xE0101010

WINDOW_WIDTH = 880
WINDOW_HEIGHT = 592
SIDEBAR_X = 12
SIDEBAR_WIDTH = 176
SIDEBAR_Y = 38
CATEGORY_HEIGHT = 23
MAIN_X = 200
MAIN_RIGHT = WINDOW_WIDTH - 12
ROW_HEIGHT = 54
ROWS_PER_PAGE = 8
ROWS_Y = 116
ROW_WIDTH = MAIN_RIGHT - MAIN_X
MAX_LISTINGS = 6000
# MT2009_PLUS_UPSTREAM_2_0_76: a quicker search after typing, and the
# forgetting book (70037) searched by its skill like the skill book.
SEARCH_DELAY = 0.35
BOOK_VNUMS = (50300, 70037)
# Umiejetnosci, dla ktorych istnieja ksiegi (klient zna ich nazwy). skill.GetSkillName dla nieznanego numeru
# zapisuje blad w interpreterze i wywala pozniejszy, niezwiazany kod - wolno pytac tylko o te numery.
SKILL_IDS = tuple(range(1, 6) + range(16, 21) + range(31, 36) + range(46, 51) + range(61, 67) + range(76, 82) + range(91, 97) + range(106, 112))
APPLY_INTERVAL = 0.15
# "Kup wiele" (the operator, 28 September: "zaznaczam, ktore przedmioty chce
# kupic, potem 'Kup wszystko' i kupuja sie po kolei"). A box on every row, the
# header's box for the page; the offers go one at a time through /flea_buy, the
# single purchase's own command, the next one once the server has answered the
# last: the offer taken off the list (DeleteSearchResultItem) - bought when the
# Yang went down, gone to somebody else when it did not. No answer within
# MULTI_BUY_TIMEOUT is a refusal (no room in the bag, a changed price, a shop
# in edit - the server says why in the chat) and ends the run.
CHECK_SIZE = 16
ICON_X = 28
TEXT_X = 82
MULTI_BUY_TIMEOUT = 5.0
MULTI_BUY_GAP = 0.35
# MT2009_PLUS_UPSTREAM_2_0_76: the mouse wheel turns one page per notch.
WHEEL_GAP = 0.09
# MT2009_PLUS_UPSTREAM_2_0_76: the average price in an offer's tooltip, from
# the PRICE_SAMPLE cheapest loaded offers of the same item (vnum, sockets and
# bonuses; a book by its skill).
PRICE_SAMPLE = 20


def ParseMarketPrice(text):
    """MT2009_PLUS_UPSTREAM_2_0_76: "Cena od/do" takes 1500000, 1.5kk,
    1500k or 2kkk (k = thousand); anything else is no limit."""
    text = text.strip().lower().replace(" ", "").replace(",", ".")
    match = re.match(r"^(\d+)(?:\.(\d+))?(k{1,3})?$", text)
    if not match:
        return 0
    whole, fraction, suffix = match.groups()
    fraction = fraction or ""
    multiplier = 1000 ** len(suffix or "")
    value = int(whole + fraction) * multiplier // (10 ** len(fraction))
    return max(0, value)

# (nazwa, wciecie, stala/e typu z modulu item (string albo krotka stringow) albo "OTHER", stala
# podtypu, stala antyflagi klasy z modulu item albo None - filtr klasy dziala TYLKO po stronie
# klienta (IsAntiFlag na wybranym przedmiocie), serwer o nim nie wie, patrz __MatchesCategory)
CATEGORY_DEFS = (
    ("Wszystko", 0, None, None, None),
    ("Bron", 0, "ITEM_TYPE_WEAPON", None, None),
    ("Miecze jednoreczne", 1, "ITEM_TYPE_WEAPON", "WEAPON_SWORD", None),
    ("Miecze dwureczne", 1, "ITEM_TYPE_WEAPON", "WEAPON_TWO_HANDED", None),
    ("Luki", 1, "ITEM_TYPE_WEAPON", "WEAPON_BOW", None),
    ("Sztylety", 1, "ITEM_TYPE_WEAPON", "WEAPON_DAGGER", None),
    ("Dzwony", 1, "ITEM_TYPE_WEAPON", "WEAPON_BELL", None),
    ("Wachlarze", 1, "ITEM_TYPE_WEAPON", "WEAPON_FAN", None),
    ("Zbroje", 0, "ITEM_TYPE_ARMOR", "ARMOR_BODY", None),
    ("Zbroje - Wojownik", 1, "ITEM_TYPE_ARMOR", "ARMOR_BODY", "ANTIFLAG_WARRIOR"),
    ("Zbroje - Ninja", 1, "ITEM_TYPE_ARMOR", "ARMOR_BODY", "ANTIFLAG_ASSASSIN"),
    ("Zbroje - Sura", 1, "ITEM_TYPE_ARMOR", "ARMOR_BODY", "ANTIFLAG_SURA"),
    ("Zbroje - Szaman", 1, "ITEM_TYPE_ARMOR", "ARMOR_BODY", "ANTIFLAG_SHAMAN"),
    ("Helmy", 0, "ITEM_TYPE_ARMOR", "ARMOR_HEAD", None),
    ("Helmy - Wojownik", 1, "ITEM_TYPE_ARMOR", "ARMOR_HEAD", "ANTIFLAG_WARRIOR"),
    ("Helmy - Ninja", 1, "ITEM_TYPE_ARMOR", "ARMOR_HEAD", "ANTIFLAG_ASSASSIN"),
    ("Helmy - Sura", 1, "ITEM_TYPE_ARMOR", "ARMOR_HEAD", "ANTIFLAG_SURA"),
    ("Helmy - Szaman", 1, "ITEM_TYPE_ARMOR", "ARMOR_HEAD", "ANTIFLAG_SHAMAN"),
    ("Tarcze", 0, "ITEM_TYPE_ARMOR", "ARMOR_SHIELD", None),
    ("Buty", 0, "ITEM_TYPE_ARMOR", "ARMOR_FOOTS", None),
    ("Bransolety", 0, "ITEM_TYPE_ARMOR", "ARMOR_WRIST", None),
    ("Naszyjniki", 0, "ITEM_TYPE_ARMOR", "ARMOR_NECK", None),
    ("Kolczyki", 0, "ITEM_TYPE_ARMOR", "ARMOR_EAR", None),
    ("Ksiegi", 0, "ITEM_TYPE_SKILLBOOK", None, None),
    ("Ksiegi zapomnienia", 0, "ITEM_TYPE_SKILLFORGET", None, None),
    ("Kamienie duszy", 0, "ITEM_TYPE_METIN", None, None),
    ("Rudy i przetopy", 0, "ITEM_TYPE_RESOURCE", None, None),
    ("Dopalacze", 0, "ITEM_TYPE_POTION", None, None),
    ("Uzywalne", 0, "ITEM_TYPE_USE", None, None),
    ("Ulepszacze", 0, "ITEM_TYPE_MATERIAL", None, None),
    ("Inne", 0, "OTHER", None, None),
)

# Przedmioty, ktore w silniku siedza pod typem/podtypem dzielonym z mnostwem niezwiazanych
# rzeczy (wiec samo dopasowanie typu/podtypu by je zle skategoryzowalo albo zagarnelo za duzo
# innych przedmiotow) - wygrywaja przed normalnym dopasowaniem typu, w obie strony (jesli
# przedmiot ma tu wpis, liczy sie TYLKO ta kategoria, niezaleznie od jego prawdziwego typu).
CATEGORY_VNUM_OVERRIDES = {
    27987: "Ulepszacze",           # Malz - silnikowo ITEM_USE/USE_SPECIAL (dzielony z dziesiatkami niezwiazanych przedmiotow)
    27798: "Ulepszacze",           # Skamieniala Krewetka - silnikowo ITEM_TYPE_RESOURCE, ale to rybi odpad jak Rybia Osc
    27799: "Ulepszacze",           # Rybia Osc - jw.
    30379: "Rudy i przetopy",      # Przetopiony Kruszec - silnikowo ITEM_TYPE_MATERIAL (dzielony z ~470 innymi materialami)
    39005: "Rudy i przetopy",      # Magiczna Ruda Miedzi (wariant questowy)
    70035: "Rudy i przetopy",      # Magiczna Ruda Miedzi (wariant questowy)
    71026: "Ulepszacze",           # MT2009_PLUS_UPSTREAM_2_0_76: an upgrade item, as upstream files it
    72308: "Rudy i przetopy",      # Magiczna Ruda Miedzi (wariant questowy)
}
# (od, do wlacznie, kategoria) - prawdziwe rudy/kamienie siedza pod ITEM_TYPE_SPECIAL, ktory
# jest ogolnym koszem (jedzenie dla konia, bilety, obraczki...), wiec tylko ten ciagly zakres
# vnumow trafia do "Rudy i przetopy".
CATEGORY_VNUM_RANGE_OVERRIDES = (
    # MT2009_PLUS_UPSTREAM_2_0_76: to 50638 as upstream (the smelted ores after 50622).
    (50601, 50638, "Rudy i przetopy"),  # Ruda Miedzi .. Ruda Szafiru, Kawalek Bursztynu/Perly, Dusza Rudy Krysztalu, Bursztyn, Diamentowy Kamien, Skamienialy Pien
)

# (klucz, nazwa na przycisku, kod sortowania po stronie serwera: 0 cena rosnaco, 1 cena malejaco, 2 cena za sztuke)
SORT_MODES = (
    ("price_asc", "Cena: rosnaco", 0),
    ("price_desc", "Cena: malejaco", 1),
    ("unit_asc", "Cena za sztuke", 2),
    ("name", "Nazwa A-Z", 0),
    ("seller", "Sprzedawca A-Z", 0),
    ("count_asc", "Ilosc: rosnaco", 0),
    ("count_desc", "Ilosc: malejaco", 0),
)

# CUSTOM_FLEA_BONUS_FILTER_V1: (nazwa stalej w module "player", etykieta PL) -
# bonusy do filtra "Filtry" (do 5 naraz, kazdy z progiem minimalnym). Caly
# filtr dziala po stronie klienta na juz pobranych ofertach (kazda oferta ma
# "attrs" - krotke (bType, sValue) - wyslana przez natywny modul ikashop, wiec
# serwer nie musi nic wiedziec o tym filtrze). bType to numer POINT_* (ten sam,
# ktory zna localeinfo_point.AFFECT_DICT i dymek przedmiotu - w tym silniku nie
# ma osobnych APPLY_*), wiec porownanie attr[0] == player.POINT_* dziala dla
# kazdego bonusu z tej listy.
# MT2009_PLUS_FLEA_BONUS_SCROLL_V1: najpierw te 22, ktore gracze wybieraja
# najczesciej (kolejnosc jak dawniej), potem WSZYSTKIE pozostale bonusy, ktore
# klient umie nazwac (AFFECT_DICT) - alfabetycznie, bez punktow wewnetrznych
# (BONUS_FILTER_SKIP).
BONUS_FILTER_DEFS = (
    ("POINT_MAX_HP", "Max PZ"),
    ("POINT_MAX_SP", "Max PM"),
    ("POINT_NORMAL_HIT_DAMAGE_BONUS", "Srednie obrazenia"),
    ("POINT_ATT_SPEED", "Szybkosc ataku"),
    ("POINT_MOV_SPEED", "Szybkosc ruchu"),
    ("POINT_ATT_GRADE_BONUS", "Wartosc ataku"),
    ("POINT_CRITICAL_PCT", "Szansa na krytyk"),
    ("POINT_PENETRATE_PCT", "Szansa na przebicie"),
    ("POINT_BLOCK", "Blok"),
    ("POINT_DODGE", "Unik"),
    ("POINT_IMMUNE_STUN", "Odpornosc na omdlenia"),
    ("POINT_RESIST_BOW", "Odpornosc: strzaly"),
    ("POINT_RESIST_MAGIC", "Odpornosc: magia"),
    ("POINT_ATTBONUS_WARRIOR", "Bonus vs Wojownik"),
    ("POINT_ATTBONUS_ASSASSIN", "Bonus vs Ninja"),
    ("POINT_ATTBONUS_SURA", "Bonus vs Sura"),
    ("POINT_ATTBONUS_SHAMAN", "Bonus vs Szaman"),
    ("POINT_STEAL_HP", "Kradziez PZ"),
    ("POINT_STEAL_SP", "Kradziez PM"),
    ("POINT_EXP_DOUBLE_BONUS", "Bonus do expa"),
    ("POINT_GOLD_DOUBLE_BONUS", "Bonus do yang"),
    ("POINT_ITEM_DROP_BONUS", "Bonus do dropu"),
)

# MT2009_PLUS_FLEA_BONUS_SCROLL_V1: krotkie polskie nazwy reszty bonusow z
# localeinfo_point.AFFECT_DICT (w liscie ida alfabetycznie, po tych 22 wyzej).
# Bonus z AFFECT_DICT, ktorego tu nie ma (np. dopisany w przyszlosci), dostaje
# nazwe z tekstu dymka (_LabelFromTooltip) - tak zeby lista zawsze byla pelna.
BONUS_FILTER_EXTRA_LABELS = {
    "POINT_HT": "Witalnosc",
    "POINT_IQ": "Inteligencja",
    "POINT_ST": "Sila",
    "POINT_DX": "Zrecznosc",
    "POINT_CASTING_SPEED": "Szybkosc zaklec",
    "POINT_HP_REGEN": "Regeneracja PZ",
    "POINT_SP_REGEN": "Regeneracja PM",
    "POINT_POISON_PCT": "Szansa na otrucie",
    "POINT_STUN_PCT": "Szansa na omdlenie",
    "POINT_SLOW_PCT": "Szansa na spowolnienie",
    "POINT_FIRE_PCT": "Szansa na podpalenie",
    "POINT_ATTBONUS_MONSTER": "Bonus vs Potwory",
    "POINT_ATTBONUS_HUMAN": "Bonus vs Ludzie",
    "POINT_ATTBONUS_ANIMAL": "Bonus vs Zwierzeta",
    "POINT_ATTBONUS_ORC": "Bonus vs Orki",
    "POINT_ATTBONUS_MILGYO": "Bonus vs Mistycy",
    "POINT_ATTBONUS_UNDEAD": "Bonus vs Nieumarli",
    "POINT_ATTBONUS_DEVIL": "Bonus vs Diably",
    "POINT_ATTBONUS_STONE": "Bonus vs Metiny",
    "POINT_ATTBONUS_BOSS": "Bonus vs Bossy",
    "POINT_ATT_SPECIAL": "Bonus vs Bossy/Metiny",
    "POINT_ATTBONUS_ORC_VALLEY": "Bonus w Dolinie Seungryong",
    "POINT_MANA_BURN_PCT": "Szansa na kradziez PM",
    "POINT_DAMAGE_SP_RECOVER": "Transfer obrazen do PM",
    "POINT_RESIST_SWORD": "Odpornosc: miecze",
    "POINT_RESIST_TWOHAND": "Odpornosc: bron dwureczna",
    "POINT_RESIST_DAGGER": "Odpornosc: sztylety",
    "POINT_RESIST_BELL": "Odpornosc: dzwony",
    "POINT_RESIST_FAN": "Odpornosc: wachlarze",
    "POINT_RESIST_FIRE": "Odpornosc: ogien",
    "POINT_RESIST_ELEC": "Odpornosc: blyskawice",
    "POINT_RESIST_WIND": "Odpornosc: wiatr",
    "POINT_RESIST_ICE": "Odpornosc: lod",
    "POINT_RESIST_EARTH": "Odpornosc: ziemia",
    "POINT_RESIST_DARK": "Odpornosc: mrok",
    "POINT_RESIST_WARRIOR": "Odpornosc: Wojownik",
    "POINT_RESIST_ASSASSIN": "Odpornosc: Ninja",
    "POINT_RESIST_SURA": "Odpornosc: Sura",
    "POINT_RESIST_SHAMAN": "Odpornosc: Szaman",
    "POINT_RESIST_HUMAN": "Odpornosc: ludzie",
    "POINT_RESIST_MONSTER_1000PCT": "Odpornosc: potwory",
    "POINT_RESIST_CRITICAL": "Odpornosc: cios krytyczny",
    "POINT_RESIST_PENETRATE": "Odpornosc: przebicie",
    "POINT_RESIST_NORMAL_DAMAGE": "Odpornosc: ataki fizyczne",
    "POINT_NORMAL_HIT_DEFEND_BONUS": "Odpornosc: srednie obrazenia",
    "POINT_SKILL_DEFEND_BONUS": "Odpornosc: obrazenia umiejetnosci",
    "POINT_POISON_REDUCE": "Odpornosc: trucizny",
    "POINT_IMMUNE_SLOW": "Odpornosc na spowolnienie",
    "POINT_IMMUNE_FALL": "Odpornosc na upadek",
    "POINT_IMMUNE_STUN_BREAK": "Przebicie odpornosci na omdlenie",
    "POINT_REFLECT_MELEE": "Odbicie ciosu",
    "POINT_REFLECT_ARROW": "Odbicie pocisku",
    "POINT_KILL_SP_RECOVER": "Odzysk PM po zabiciu",
    "POINT_KILL_HP_RECOVERY": "Odzysk PZ po zabiciu",
    "POINT_HIT_HP_RECOVERY": "Zlodziej zycia",
    "POINT_POTION_BONUS": "Bonus mikstur",
    "POINT_BOW_DISTANCE": "Zasieg luku",
    "POINT_DEF_GRADE_BONUS": "Obrona",
    "POINT_DEF_BONUS": "Wzmocnienie obrony",
    "POINT_MAGIC_ATT_GRADE_BONUS": "Wartosc magicznego ataku",
    "POINT_MAGIC_DEF_GRADE_BONUS": "Magiczna obrona",
    "POINT_MAGIC_ATT": "Obrazenia magiczne",
    "POINT_MAGIC_ATT_MONSTER": "Obrazenia magiczne vs potwory",
    "POINT_MAGIC_ATT_GRADE_BONUS_MONSTER": "Magiczny atak vs potwory",
    "POINT_MAGIC_ATT_BONUS_PER": "Magiczny atak %",
    "POINT_MELEE_MAGIC_ATT_BONUS_PER": "Atak magiczny/fizyczny %",
    "POINT_ATT_GRADE_MONSTER": "Wartosc ataku vs potwory",
    "POINT_DAGGER_ATT_GRADE_MONSTER": "Atak sztyletem vs potwory",
    "POINT_SKILL_DAMAGE_BONUS": "Obrazenia umiejetnosci",
    "POINT_SKILL_DURATION": "Czas trwania umiejetnosci",
    "POINT_MAX_HP_PCT": "Max PZ %",
    "POINT_MAX_SP_PCT": "Max PM %",
    "POINT_MAX_STAMINA": "Max wytrzymalosc",
    "POINT_ST_REGEN": "Regeneracja wytrzymalosci",
    "POINT_ABSORB_DAMAGE": "Absorpcja obrazen",
    "POINT_ABSORB_DAMAGE_MONSTER": "Absorpcja obrazen od potworow",
    "POINT_BREAK_TEMPLE_CURSE": "Zlamanie klatwy swiatyni",
    "POINT_BREAK_RESIST_SWORD": "Zlamanie odpornosci: miecze",
    "POINT_BREAK_RESIST_TWOHAND": "Zlamanie odpornosci: bron 2-r.",
    "POINT_BREAK_RESIST_DAGGER": "Zlamanie odpornosci: sztylety",
    "POINT_BREAK_RESIST_BELL": "Zlamanie odpornosci: dzwony",
    "POINT_BREAK_RESIST_FAN": "Zlamanie odpornosci: wachlarze",
    "POINT_BREAK_RESIST_BOW": "Zlamanie odpornosci: luki",
    "POINT_DROP_RARE": "Szansa na przedmiot z bonusem",
    "POINT_MALL_ATTBONUS": "Wartosc ataku %",
    "POINT_MALL_DEFBONUS": "Obrona %",
    "POINT_MALL_EXPBONUS": "Punkty doswiadczenia %",
    "POINT_MALL_ITEMBONUS": "Mnoznik dropu przedmiotow",
    "POINT_MALL_GOLDBONUS": "Mnoznik dropu yang",
}

# MT2009_PLUS_FLEA_BONUS_SCROLL_V1: punkty z AFFECT_DICT, ktore nie sa bonusem
# przedmiotu (przemiana, iCafe, energia/kostium, szansa nauki z ksiegi, efekty
# umiejetnosci) - nie ma ich w liscie wyboru.
BONUS_FILTER_SKIP = (
    "POINT_POLYMORPH",
    "POINT_PC_BANG_EXP_BONUS",
    "POINT_PC_BANG_DROP_BONUS",
    "POINT_ENERGY",
    "POINT_COSTUME_ATTR_BONUS",
    "POINT_LEARN_CHANCE",
    "POINT_TERROR",
    "POINT_ATT_BONUS",
    "POINT_CURSE_PCT",
    "POINT_CONVERT_DAMAGE_TO_SP",
    "POINT_DEATH_PENALTY",
)
# klucze AFFECT_DICT, ktore nie sa stalymi POINT_* silnika (991 = klientowy
# "rare pct" dymka)
BONUS_FILTER_SKIP_VALUES = (991,)

# localeinfo_point.ATTR_MAX_VALUES (silnikowa tabela) nie ma wpisu dla kazdego
# z powyzszych - tu nadpisania/dopelnienia dla tych, ktore gracze podali z
# wlasnego doswiadczenia (np. Srednie obrazenia losuja sie do +60%).
BONUS_FILTER_MAX_OVERRIDES = {
    "POINT_NORMAL_HIT_DAMAGE_BONUS": 60,
}

_TOOLTIP_FORMAT_RE = re.compile(r'[+-]?%[-+ #0-9.]*[a-zA-Z]|%%')


def _LabelFromTooltip(value):
    # tekst dymka ("Maks. PZ: +%d", "Odpornosc na Ogien: %d%%") bez liczby -
    # tylko dla bonusu spoza BONUS_FILTER_EXTRA_LABELS
    text = localeinfo_point.AFFECT_DICT.get(value)
    if callable(text):
        try:
            text = text.func_closure[0].cell_contents
        except (AttributeError, IndexError, TypeError):
            text = None
    if not isinstance(text, str):
        return None
    text = _TOOLTIP_FORMAT_RE.sub("", text)
    text = " ".join(text.split()).strip(" :+-.")
    return text or None


def BuildBonusFilterOptions():
    skip = set(BONUS_FILTER_SKIP_VALUES)
    for constName in BONUS_FILTER_SKIP:
        value = getattr(player, constName, None)
        if value is not None:
            skip.add(value)

    maxByValue = {}
    for constName, maxValue in BONUS_FILTER_MAX_OVERRIDES.items():
        value = getattr(player, constName, None)
        if value is not None:
            maxByValue[value] = maxValue

    def WithMax(value, label):
        maxValue = maxByValue.get(value) or localeinfo_point.ATTR_MAX_VALUES.get(value)
        if maxValue and maxValue > 1:
            if value in localeinfo_point.POINT_1000PCT_Tuple:
                return "%s (maks. %.1f)" % (label, maxValue / 10.0)
            return "%s (maks. %d)" % (label, maxValue)
        return label

    options = []
    seen = set()
    for constName, label in BONUS_FILTER_DEFS:
        value = getattr(player, constName, None)
        if value is None or value in seen:
            continue
        seen.add(value)
        options.append((value, WithMax(value, label)))

    labelByValue = {}
    for constName, label in BONUS_FILTER_EXTRA_LABELS.items():
        value = getattr(player, constName, None)
        if value is not None:
            labelByValue[value] = label

    rest = []
    for value in localeinfo_point.AFFECT_DICT.keys():
        if value in seen or value in skip:
            continue
        label = labelByValue.get(value) or _LabelFromTooltip(value)
        if not label:
            continue
        seen.add(value)
        rest.append((PolishLower(label), value, label))
    rest.sort()
    for _, value, label in rest:
        options.append((value, WithMax(value, label)))
    return options


class _BonusFilterRowHandler:
    # ui.__mem_func__ (used by SAFE_SetEvent-style wrappers) needs a real
    # bound method (it reads im_func/im_class/im_self) - a lambda doesn't
    # have those and crashes the whole client on startup (AttributeError:
    # 'function' object has no attribute 'im_func'). One tiny object per row
    # gives each row's button its own genuine bound method instead.
    def __init__(self, dialog, rowIndex):
        self.dialog = proxy(dialog)
        self.rowIndex = rowIndex

    def OnClick(self):
        self.dialog.OpenPicker(self.rowIndex)


class FleaMarketBonusFilterDialog(ui.BoardWithTitleBar):
    # CUSTOM_FLEA_BONUS_FILTER_V1 v3: ui.ComboBox turned out to never raise
    # itself above the rest of the dialog when 5 of them sit in the same
    # board (each dropdown rendered BEHIND the board's own card/other rows,
    # first items unreadable/unclickable - tried SetTop() from a few angles,
    # none of it changed anything). Replaced with the exact pattern this
    # file's own search-suggestions box already uses successfully: one
    # shared ui.ListBox, created once, positioned over the rows and raised
    # with SetTop() right as it's shown - proven to render on top in this
    # very window already.
    MAX_ROWS = 5
    ROW_HEIGHT = 34
    WIDTH = 470
    LEFT = 20
    PICK_WIDTH = 280
    # MT2009_PLUS_FLEA_BONUS_SCROLL_V1: lista wyboru pokazuje stala liczbe
    # wierszy z paskiem przewijania (i kolkiem myszy) zamiast jednej dlugiej
    # listy na wszystkie bonusy.
    PICKER_ROWS = 10
    PICKER_LINE = 17
    PICKER_WHEEL_ROWS = 2
    PICK_HEIGHT = 23
    EDIT_WIDTH = 86
    NONE_LABEL = "- brak -"

    def __init__(self, market):
        ui.BoardWithTitleBar.__init__(self)
        self.market = proxy(market)
        # indeks 0 = brak filtru na tym wierszu, potem realne opcje
        self.options = [(None, self.NONE_LABEL)] + BuildBonusFilterOptions()
        self.rows = []
        self.activeRow = -1
        self.__keepers = []
        self.pickerBase = 0
        self.pickerScrollGuard = False

        self.__top = 64
        # MT2009_PLUS_UPSTREAM_2_0_76: one more row, "Min. liczba bonusow".
        cardHeight = (self.MAX_ROWS + 1) * self.ROW_HEIGHT + 12
        # CUSTOM_FLEA_BONUS_FILTER_V1: dialog musi byc na tyle wysoki, zeby
        # w pelni zmiescic rozwinieta liste wyboru (patrz OpenPicker) - inaczej
        # dolna czesc listy wystaje poza wlasne okno, a kliki tam trafiaja w
        # przedmioty pod spodem zamiast w liste (wlasny pick-area okna konczy
        # sie na jego deklarowanym rozmiarze, mimo ze tekst renderuje sie dalej).
        # MT2009_PLUS_FLEA_BONUS_SCROLL_V1: lista ma stala wysokosc
        # (PICKER_ROWS), wiec okno jest tak wysokie, zeby otworzyla sie w
        # calosci pod kazdym z 5 wierszy - a nie na dlugosc wszystkich bonusow.
        self.pickerViewCount = min(self.PICKER_ROWS, len(self.options))
        self.pickerHeight = self.pickerViewCount * self.PICKER_LINE + 6
        lastRowBottom = self.__top + 6 + (self.MAX_ROWS - 1) * self.ROW_HEIGHT + self.PICK_HEIGHT
        bottom = max(self.__top + cardHeight + 50, lastRowBottom + 2 + self.pickerHeight + 10)
        self.SetSize(self.WIDTH, bottom)
        self.AddFlag("movable")
        self.AddFlag("float")
        self.SetTitleName("Filtry bonusow")
        self.SetCloseEvent(self.Close)

        hint = ui.TextLine()
        hint.SetParent(self)
        hint.SetPosition(self.LEFT, 36)
        hint.SetText("Do 5 bonusow naraz, kazdy z minimalnym progiem:")
        hint.SetPackedFontColor(COLOR_DIM)
        hint.Show()
        self.__keepers.append(hint)

        self.__MakeCard(self.LEFT - 4, self.__top, self.WIDTH - (self.LEFT - 4) * 2, cardHeight)

        y = self.__top + 6
        for index in range(self.MAX_ROWS):
            self.rows.append(self.__MakeRow(index, y))
            y += self.ROW_HEIGHT

        # MT2009_PLUS_UPSTREAM_2_0_76: only offers with at least this many
        # bonuses (CountBonuses), whichever they are; empty - no limit.
        countLabel = ui.TextLine()
        countLabel.SetParent(self)
        countLabel.SetPosition(self.LEFT + 4, y + 5)
        countLabel.SetText("Min. liczba bonusow:")
        countLabel.SetPackedFontColor(COLOR_DIM)
        countLabel.AddFlag("not_pick")
        countLabel.Show()
        self.__keepers.append(countLabel)
        countBar = ui.SlotBar()
        countBar.SetParent(self)
        countBar.SetPosition(self.LEFT + 4 + 16 + self.PICK_WIDTH + 30, y)
        countBar.SetSize(self.EDIT_WIDTH, self.PICK_HEIGHT)
        countBar.AddFlag("not_pick")
        countBar.Show()
        self.__keepers.append(countBar)
        self.countEdit = ui.EditLine()
        self.countEdit.SetParent(countBar)
        self.countEdit.SetPosition(4, 3)
        self.countEdit.SetSize(self.EDIT_WIDTH - 8, 17)
        self.countEdit.SetMax(1)
        self.countEdit.SetNumberMode()
        self.countEdit.SAFE_SetReturnEvent(self.Apply)
        self.countEdit.Show()
        y += self.ROW_HEIGHT

        self.applyButton = self.__MakeButton(self.WIDTH / 2 - 110, y + 14, "Zastosuj", self.Apply)
        self.clearButton = self.__MakeButton(self.WIDTH / 2 + 10, y + 14, "Wyczysc", self.ClearAll)

        # lista wyboru bonusu - jedna wspolna dla wszystkich wierszy
        # MT2009_PLUS_FLEA_BONUS_SCROLL_V1: PICKER_ROWS widocznych pozycji
        # (ListBox.SetBasePos) + ui.ScrollBar po prawej; kolko myszy nad
        # lista przewija ja (OnMouseWheel tego okna - zdarzenie kolka idzie
        # od okna pod kursorem w gore po rodzicach).
        pickerWidth = self.PICK_WIDTH + 8
        self.pickerBackground = ui.SlotBar()
        self.pickerBackground.SetParent(self)
        self.pickerBackground.SetPosition(self.LEFT + 4 + 16 - 4, self.__top)
        self.pickerBackground.SetSize(pickerWidth, self.pickerHeight)
        self.pickerBackground.AddFlag("not_pick")
        self.pickerBackground.Hide()
        self.pickerScroll = ui.ScrollBar()
        self.pickerScroll.SetParent(self.pickerBackground)
        self.pickerScroll.SetScrollBarSize(self.pickerHeight - 4)
        scrollWidth = self.pickerScroll.GetWidth()
        self.pickerScroll.SetPosition(pickerWidth - scrollWidth - 2, 2)
        self.pickerScroll.SetScrollEvent(ui.__mem_func__(self.OnPickerScroll))
        self.pickerScroll.Hide()
        self.pickerHasScroll = len(self.options) > self.pickerViewCount
        listWidth = self.PICK_WIDTH
        if self.pickerHasScroll:
            listWidth = pickerWidth - scrollWidth - 4 - 4
            maxBase = len(self.options) - self.pickerViewCount
            self.pickerScroll.SetMiddleBarSize(float(self.pickerViewCount) / len(self.options))
            # przycisk strzalki = 3 pozycje (pasek ma ~1-2 px na pozycje,
            # wiec krok o 1 pozycje czasem nic by nie przesunal)
            self.pickerScroll.SetScrollStep(min(1.0, 3.0 / maxBase))
        self.pickerList = ui.ListBox()
        self.pickerList.SetParent(self.pickerBackground)
        self.pickerList.SetPosition(4, 3)
        self.pickerList.SetSize(listWidth, self.pickerViewCount * self.PICKER_LINE)
        self.pickerList.SetTextCenterAlign(False)
        self.pickerList.SetEvent(self.OnPickerSelect)
        self.pickerList.Hide()
        for optionIndex, (_, label) in enumerate(self.options):
            self.pickerList.InsertItem(optionIndex, label)

        self.Hide()

    # CUSTOM_ENTER_CONFIRM_V1
    def OnPressReturnKey(self):
        self.Apply()
        return True

    def __MakeCard(self, x, y, width, height):
        outer = ui.Bar()
        outer.SetParent(self)
        outer.SetPosition(x, y)
        outer.SetSize(width, height)
        outer.SetColor(COLOR_CARD_BORDER)
        outer.AddFlag("not_pick")
        outer.Show()
        inner = ui.Bar()
        inner.SetParent(self)
        inner.SetPosition(x + 1, y + 1)
        inner.SetSize(width - 2, height - 2)
        inner.SetColor(COLOR_CARD_FILL)
        inner.AddFlag("not_pick")
        inner.Show()
        self.__keepers.append(outer)
        self.__keepers.append(inner)

    def __MakeButton(self, x, y, text, event):
        button = ui.Button()
        button.SetParent(self)
        button.SetPosition(x, y)
        button.SetSize(100, 25)
        button.SetUpVisual("d:/ymir work/ui/public/middle_button_01.sub")
        button.SetOverVisual("d:/ymir work/ui/public/middle_button_02.sub")
        button.SetDownVisual("d:/ymir work/ui/public/middle_button_03.sub")
        button.SetText(text)
        button.SetEvent(event)
        button.Show()
        self.__keepers.append(button)
        return button

    def __MakeRow(self, index, y):
        rowX = self.LEFT + 4

        number = ui.TextLine()
        number.SetParent(self)
        number.SetPosition(rowX, y + 5)
        number.SetText("%d." % (index + 1))
        number.SetPackedFontColor(COLOR_DIM)
        number.AddFlag("not_pick")
        number.Show()
        self.__keepers.append(number)

        pickX = rowX + 16
        pickButton = ui.Button()
        pickButton.SetParent(self)
        pickButton.SetPosition(pickX, y)
        pickButton.SetSize(self.PICK_WIDTH, self.PICK_HEIGHT)
        pickButton.SetUpVisual("d:/ymir work/ui/public/middle_button_01.sub")
        pickButton.SetOverVisual("d:/ymir work/ui/public/middle_button_02.sub")
        pickButton.SetDownVisual("d:/ymir work/ui/public/middle_button_03.sub")
        pickButton.SetText(self.NONE_LABEL)
        handler = _BonusFilterRowHandler(self, index)
        pickButton.SetEvent(handler.OnClick)
        pickButton.Show()
        self.__keepers.append(pickButton)
        self.__keepers.append(handler)

        geX = pickX + self.PICK_WIDTH + 10
        geText = ui.TextLine()
        geText.SetParent(self)
        geText.SetPosition(geX, y + 5)
        geText.SetText(">=")
        geText.SetPackedFontColor(COLOR_DIM)
        geText.AddFlag("not_pick")
        geText.Show()
        self.__keepers.append(geText)

        editX = geX + 20
        inputBar = ui.SlotBar()
        inputBar.SetParent(self)
        inputBar.SetPosition(editX, y)
        inputBar.SetSize(self.EDIT_WIDTH, self.PICK_HEIGHT)
        inputBar.AddFlag("not_pick")
        inputBar.Show()
        self.__keepers.append(inputBar)

        valueEdit = ui.EditLine()
        valueEdit.SetParent(inputBar)
        valueEdit.SetPosition(4, 3)
        valueEdit.SetSize(self.EDIT_WIDTH - 8, 17)
        valueEdit.SetMax(7)
        valueEdit.SetNumberMode()
        valueEdit.SAFE_SetReturnEvent(self.Apply)
        valueEdit.Show()

        return {"optionIndex": 0, "button": pickButton, "edit": valueEdit, "y": y}

    def OpenPicker(self, rowIndex):
        # ten sam wiersz klikniety drugi raz - zamyka liste
        if self.activeRow == rowIndex and self.pickerBackground.IsShow():
            self.HidePicker()
            return
        self.activeRow = rowIndex
        row = self.rows[rowIndex]

        # MT2009_PLUS_FLEA_BONUS_SCROLL_V1: pod wierszem, a gdy tam brak
        # miejsca (w oknie albo na ekranie) - nad nim; w ostatecznosci
        # dosuniete do krawedzi okna, nigdy poza nie (poza oknem kliki szly
        # w przedmioty pod spodem).
        height = self.pickerHeight
        minY = 32
        maxY = self.GetHeight() - 8 - height
        below = row["y"] + self.PICK_HEIGHT + 2
        above = row["y"] - 2 - height
        try:
            screenLimit = wndMgr.GetScreenHeight() - self.GetGlobalPosition()[1] - height
        except:
            screenLimit = maxY
        if below <= maxY and below <= screenLimit:
            y = below
        elif above >= minY:
            y = above
        else:
            y = max(minY, min(below, maxY))
        x, _ = self.pickerBackground.GetLocalPosition()
        self.pickerBackground.SetPosition(x, y)

        # biezacy wybor wiersza widoczny w liscie (mniej wiecej na srodku);
        # ClearSelection, bo ListBox nie wola zdarzenia dla ponownie
        # kliknietej tej samej pozycji
        self.pickerList.ClearSelection()
        self.SetPickerBase(row["optionIndex"] - self.pickerViewCount / 2)

        self.pickerBackground.Show()
        self.pickerList.Show()
        if self.pickerHasScroll:
            self.pickerScroll.Show()
        self.pickerBackground.SetTop()
        self.pickerList.SetTop()

    def __GetPickerMaxBase(self):
        return max(0, len(self.options) - self.pickerViewCount)

    def SetPickerBase(self, base):
        maxBase = self.__GetPickerMaxBase()
        base = max(0, min(int(base), maxBase))
        self.pickerBase = base
        self.pickerList.SetBasePos(base)
        if self.pickerHasScroll and maxBase > 0:
            # pasek idzie za lista; OnPickerScroll nie przelicza tego
            # z powrotem (zaokraglenie pikseli paska przesuneloby liste)
            self.pickerScrollGuard = True
            try:
                self.pickerScroll.SetPos(float(base) / maxBase)
            finally:
                self.pickerScrollGuard = False

    def OnPickerScroll(self):
        if self.pickerScrollGuard:
            return
        maxBase = self.__GetPickerMaxBase()
        base = int(self.pickerScroll.GetPos() * maxBase + 0.5)
        base = max(0, min(base, maxBase))
        if base != self.pickerBase:
            self.pickerBase = base
            self.pickerList.SetBasePos(base)

    def OnMouseWheel(self, length):
        if not self.pickerBackground.IsShow():
            return ui.BoardWithTitleBar.OnMouseWheel(self, length)
        if length > 0:
            self.SetPickerBase(self.pickerBase - self.PICKER_WHEEL_ROWS)
        elif length < 0:
            self.SetPickerBase(self.pickerBase + self.PICKER_WHEEL_ROWS)
        return True

    def HidePicker(self):
        self.pickerList.Hide()
        self.pickerScroll.Hide()
        self.pickerBackground.Hide()
        self.activeRow = -1

    def OnPickerSelect(self, optionIndex, name):
        if self.activeRow >= 0 and 0 <= optionIndex < len(self.options):
            row = self.rows[self.activeRow]
            row["optionIndex"] = optionIndex
            row["button"].SetText(self.options[optionIndex][1])
        self.HidePicker()

    def Apply(self):
        filters = []
        for row in self.rows:
            if row["optionIndex"] <= 0:
                continue
            # Puste/niepoprawne pole progu = "ma miec ten bonus, bez wzgledu
            # na wartosc" (prog 1), zamiast po cichu gubic caly wybrany
            # wiersz - user wybral bonus z listy, wiec filtr ma zadzialac.
            try:
                minValue = int(row["edit"].GetText())
            except:
                minValue = 1
            if minValue <= 0:
                minValue = 1
            attrType = self.options[row["optionIndex"]][0]
            # MT2009_PLUS_FLEA_BONUS_SCROLL_V1: "Odpornosc: potwory" siedzi na
            # przedmiocie w promilach (dymek dzieli przez 10) - prog wpisany
            # w procentach
            if attrType in localeinfo_point.POINT_1000PCT_Tuple:
                minValue *= 10
            filters.append((attrType, minValue))
        try:
            minCount = max(0, int(self.countEdit.GetText()))
        except:
            minCount = 0
        self.market.SetBonusFilters(filters, minCount)
        self.Close()

    def ClearAll(self):
        for row in self.rows:
            row["optionIndex"] = 0
            row["button"].SetText(self.NONE_LABEL)
            row["edit"].SetText("")
        self.countEdit.SetText("")
        self.HidePicker()
        self.market.SetBonusFilters([])

    def Open(self):
        self.Show()
        self.SetTop()
        self.SetCenterPosition()

    def Close(self):
        self.HidePicker()
        self.Hide()


# Etykiety kategorii, do ktorych vnum-owe wyjatki (patrz CATEGORY_VNUM_OVERRIDES/_RANGE_OVERRIDES
# powyzej) doklejaja przedmioty o INNYM prawdziwym typie silnika. Takie kategorie NIE MOGA
# wyslac do serwera filtra po pojedynczym typie - serwer odrzucilby te przedmioty juz przy
# budowaniu wynikow, zanim dopasowanie po vnum w __MatchesCategory w ogole je zobaczy.
_OVERRIDE_TARGET_LABELS = frozenset(
    list(CATEGORY_VNUM_OVERRIDES.values()) + [label for _, _, label in CATEGORY_VNUM_RANGE_OVERRIDES]
)


def BuildCategories():
    categories = []
    for label, indent, typeName, subName, classFlagName in CATEGORY_DEFS:
        if typeName is None:
            types = (-1,)
        elif typeName == "OTHER":
            types = (-2,)
        elif isinstance(typeName, tuple):
            types = tuple(t for t in (getattr(item, name, None) for name in typeName) if t is not None)
            if not types:
                continue
        else:
            resolved = getattr(item, typeName, None)
            if resolved is None:
                continue
            types = (resolved,)
        subType = -1
        if subName:
            subType = getattr(item, subName, None)
            if subType is None:
                continue
        classFlag = -1
        if classFlagName:
            classFlag = getattr(item, classFlagName, None)
            if classFlag is None:
                continue
        # "type" to pojedyncza wartosc wysylana do filtra po stronie serwera (-1 = brak
        # filtra typu): dziala tylko dla kategorii z JEDNYM typem silnika, ktora nie przyjmuje
        # zadnych vnum-owych wyjatkow. Kategoria laczaca kilka typow (np. dawniej Ulepszacze)
        # albo bedaca celem override'u wysyla -1 (serwer nie filtruje po typie) i polega na
        # dopasowaniu "types"/override wykonanym w calosci po stronie klienta w __MatchesCategory.
        if label in _OVERRIDE_TARGET_LABELS:
            serverType = -1
        else:
            serverType = types[0] if len(types) == 1 else -1
        categories.append({"label": label, "indent": indent, "types": types, "type": serverType, "sub": subType, "classFlag": classFlag})

    # CUSTOM_FLEA_COLLAPSIBLE_CATEGORIES_V1: parentIndex/hasChildren computed
    # from the built list (not CATEGORY_DEFS) since a missing constant above
    # can skip an entry and shift every index after it.
    lastTopLevel = None
    for index, category in enumerate(categories):
        category["hasChildren"] = False
        if category["indent"] == 0:
            category["parentIndex"] = None
            lastTopLevel = index
        else:
            category["parentIndex"] = lastTopLevel
            if lastTopLevel is not None:
                categories[lastTopLevel]["hasChildren"] = True
    return categories


class FleaCategoryButton(ui.Window):
    def __init__(self, market, index, label, indent):
        ui.Window.__init__(self)
        self.market = proxy(market)
        self.index = index
        self.selected = False
        self.hover = False
        width = SIDEBAR_WIDTH - 12
        self.SetSize(width, CATEGORY_HEIGHT - 1)

        self.bg = ui.Bar()
        self.bg.SetParent(self)
        self.bg.SetPosition(0, 0)
        self.bg.SetSize(width, CATEGORY_HEIGHT - 1)
        self.bg.AddFlag("not_pick")
        self.bg.Show()

        self.accent = ui.Bar()
        self.accent.SetParent(self)
        self.accent.SetPosition(0, 0)
        self.accent.SetSize(3, CATEGORY_HEIGHT - 1)
        self.accent.SetColor(COLOR_GREEN)
        self.accent.AddFlag("not_pick")

        self.text = ui.TextLine()
        self.text.SetParent(self)
        self.text.SetPosition(10 + indent * 14, 4)
        self.text.AddFlag("not_pick")
        self.text.Show()
        self.indent = indent
        # CUSTOM_FLEA_COLLAPSIBLE_CATEGORIES_V1: baseLabel keeps the plain
        # text - SetExpanded prefixes it with +/- for a category with children.
        self.baseLabel = label
        self.expanded = None
        self.text.SetText(label)
        self.__Refresh()

    def SetExpanded(self, expanded):
        self.expanded = expanded
        if expanded is None:
            self.text.SetText(self.baseLabel)
        else:
            self.text.SetText(("- " if expanded else "+ ") + self.baseLabel)

    def SetSelected(self, selected):
        self.selected = selected
        self.__Refresh()

    def __Refresh(self):
        if self.selected:
            self.bg.SetColor(0x406EDC82)
            self.accent.Show()
            self.text.SetPackedFontColor(COLOR_GREEN)
        else:
            self.bg.SetColor(0x30FFFFFF if self.hover else (0x00000000 if self.indent else 0x14FFFFFF))
            self.accent.Hide()
            self.text.SetPackedFontColor(COLOR_TEXT if self.indent == 0 else COLOR_DIM)

    def OnMouseOverIn(self):
        self.hover = True
        self.__Refresh()

    def OnMouseOverOut(self):
        self.hover = False
        self.__Refresh()

    def OnMouseLeftButtonUp(self):
        self.market.SetCategory(self.index)
        return True


class _EnterConfirm:
    """MT2009_PLUS_UPSTREAM_2_0_76: Enter confirms the purchase question as
    "Tak" does (upstream's FleaMarketConfirmDialog, in the game's own look)."""

    def Open(self):
        self.BaseDialog.Open(self)
        self.SetFocus()

    def OnPressReturnKey(self):
        if self.IsShow():
            self.acceptButton.CallEvent()
        return True

    def Close(self):
        self.KillFocus()
        self.BaseDialog.Close(self)


class FleaMarketConfirmDialog(_EnterConfirm, uiCommon.QuestionDialog):
    BaseDialog = uiCommon.QuestionDialog


class FleaMarketConfirmDialog2(_EnterConfirm, uiCommon.QuestionDialog2):
    BaseDialog = uiCommon.QuestionDialog2


class MarketEditLine(ui.EditLine):
    """MT2009_PLUS_UPSTREAM_2_0_76: Ctrl+A marks the whole search text, and
    the next key replaces it (Backspace/Delete clear it)."""

    def __init__(self):
        ui.EditLine.__init__(self)
        self.allSelected = False
        self.selectionBar = ui.Bar()
        self.selectionBar.SetParent(self)
        self.selectionBar.SetPosition(0, 0)
        self.selectionBar.SetColor(0x604A91D1)
        self.selectionBar.AddFlag("not_pick")
        self.selectionBar.Hide()

    def ClearSelection(self):
        self.allSelected = False
        self.selectionBar.Hide()

    def __SelectAll(self):
        self.allSelected = True
        self.selectionBar.SetSize(self.GetWidth(), self.GetHeight())
        self.selectionBar.Show()

    def OnIMEKeyDown(self, key):
        if self.IsFocus():
            control = app.IsPressed(app.DIK_LCONTROL) or app.IsPressed(app.DIK_RCONTROL)
            if control and key == 0x41:
                self.__SelectAll()
                return True
            if self.allSelected and key not in (0x10, 0x11, 0x12):
                if key in (0x25, 0x27, 0x24, 0x23, 0x1B, 0x0D):
                    self.ClearSelection()
                else:
                    self.SetText("")
                    self.ClearSelection()
                    if key in (0x08, 0x2E):
                        return True
        return ui.EditLine.OnIMEKeyDown(self, key)

    def OnKeyDown(self, key):
        control = app.IsPressed(app.DIK_LCONTROL) or app.IsPressed(app.DIK_RCONTROL)
        if control and key == app.DIK_A:
            self.__SelectAll()
            return True
        modifiers = (app.DIK_LCONTROL, app.DIK_RCONTROL, app.DIK_LSHIFT, app.DIK_RSHIFT, app.DIK_LALT)
        if self.allSelected and key not in modifiers:
            if key in (app.DIK_LEFT, app.DIK_RIGHT, app.DIK_HOME, app.DIK_END, app.DIK_ESCAPE, app.DIK_RETURN):
                self.ClearSelection()
            else:
                self.SetText("")
                self.ClearSelection()
                self.OnIMEUpdate()
                if key in (app.DIK_BACK, app.DIK_DELETE):
                    return True
        return ui.EditLine.OnKeyDown(self, key)

    def OnKillFocus(self):
        self.ClearSelection()
        ui.EditLine.OnKillFocus(self)

    def OnMouseLeftButtonDown(self):
        self.ClearSelection()
        return ui.EditLine.OnMouseLeftButtonDown(self)


class FleaCheckBox(ui.Window):
    def __init__(self, event):
        ui.Window.__init__(self)
        self.event = event
        self.checked = False
        self.hover = False
        self.SetSize(CHECK_SIZE, CHECK_SIZE)
        self.border = self.__MakeBar(0, 0, CHECK_SIZE, CHECK_SIZE)
        self.fill = self.__MakeBar(1, 1, CHECK_SIZE - 2, CHECK_SIZE - 2)
        self.mark = ui.ImageBox()
        self.mark.SetParent(self)
        self.mark.LoadImage("d:/ymir work/ui/public/check_image.sub")
        self.mark.SetPosition((CHECK_SIZE - self.mark.GetWidth()) // 2, (CHECK_SIZE - self.mark.GetHeight()) // 2)
        self.mark.AddFlag("not_pick")
        self.__Refresh()

    def __MakeBar(self, x, y, width, height):
        bar = ui.Bar()
        bar.SetParent(self)
        bar.SetPosition(x, y)
        bar.SetSize(width, height)
        bar.AddFlag("not_pick")
        bar.Show()
        return bar

    def SetChecked(self, checked):
        checked = bool(checked)
        if checked != self.checked:
            self.checked = checked
            self.__Refresh()

    def __Refresh(self):
        self.border.SetColor(COLOR_GREEN if self.checked or self.hover else 0xFF8A8A8A)
        self.fill.SetColor(0xFF1E3A26 if self.checked else 0xFF0A0A0A)
        if self.checked:
            self.mark.Show()
        else:
            self.mark.Hide()

    def OnMouseOverIn(self):
        self.hover = True
        self.__Refresh()

    def OnMouseOverOut(self):
        self.hover = False
        self.__Refresh()

    def OnMouseLeftButtonUp(self):
        if self.event:
            self.event()
        return True


class FleaRow(ui.Window):
    ICON_BOX = 44

    def __init__(self, market):
        ui.Window.__init__(self)
        self.market = proxy(market)
        self.data = None
        self.index = 0
        self.hover = False
        self.SetSize(ROW_WIDTH, ROW_HEIGHT - 2)

        self.bg = self.__MakeBar(0, 0, ROW_WIDTH, ROW_HEIGHT - 2, 0x14FFFFFF)
        self.frame = self.__MakeBar(ICON_X, 4, self.ICON_BOX, self.ICON_BOX, 0xAA000000)

        # "Kup wiele": the offer's box.
        self.check = FleaCheckBox(ui.__mem_func__(self.__OnCheck))
        self.check.SetParent(self)
        self.check.SetPosition(6, (ROW_HEIGHT - 2 - CHECK_SIZE) // 2)
        self.check.Show()

        self.icon = ui.ExpandedImageBox()
        self.icon.SetParent(self)
        self.icon.SetPosition(ICON_X, 4)
        self.icon.AddFlag("not_pick")
        self.icon.Show()

        self.countText = self.__MakeText(ICON_X, 4)
        self.countText.SetOutline()
        self.countText.SetPackedFontColor(COLOR_TEXT)

        self.nameText = self.__MakeText(TEXT_X, 6)
        self.nameText.SetPackedFontColor(COLOR_TEXT)
        self.sellerText = self.__MakeText(TEXT_X, 21)
        self.sellerText.SetPackedFontColor(COLOR_DIM)
        self.bonusText = self.__MakeText(TEXT_X, 35)
        self.bonusText.SetPackedFontColor(COLOR_BONUS)

        self.quantityText = self.__MakeText(340, 18)
        self.quantityText.SetPackedFontColor(COLOR_TEXT)

        self.priceText = self.__MakeText(102, 10)
        self.priceText.SetWindowHorizontalAlignRight()
        self.priceText.SetHorizontalAlignRight()
        self.priceText.SetPosition(102, 10)
        self.priceText.SetPackedFontColor(COLOR_GOLD)
        self.unitText = self.__MakeText(102, 28)
        self.unitText.SetWindowHorizontalAlignRight()
        self.unitText.SetHorizontalAlignRight()
        self.unitText.SetPosition(102, 28)
        self.unitText.SetPackedFontColor(COLOR_DIM)

        self.buyButton = ui.Button()
        self.buyButton.SetParent(self)
        self.buyButton.SetPosition(ROW_WIDTH - 88, 13)
        self.buyButton.SetUpVisual("d:/ymir work/ui/public/middle_button_01.sub")
        self.buyButton.SetOverVisual("d:/ymir work/ui/public/middle_button_02.sub")
        self.buyButton.SetDownVisual("d:/ymir work/ui/public/middle_button_03.sub")
        self.buyButton.SetText("Kup")
        self.buyButton.SetEvent(self.__OnBuy)
        self.buyButton.Show()

    def __MakeBar(self, x, y, width, height, color):
        bar = ui.Bar()
        bar.SetParent(self)
        bar.SetPosition(x, y)
        bar.SetSize(width, height)
        bar.SetColor(color)
        bar.AddFlag("not_pick")
        bar.Show()
        return bar

    def __MakeText(self, x, y):
        line = ui.TextLine()
        line.SetParent(self)
        line.SetPosition(x, y)
        line.AddFlag("not_pick")
        line.Show()
        return line

    def SetData(self, data, index):
        self.data = data
        self.index = index
        vnum = data["vnum"]
        item.SelectItem(vnum)

        self.icon.SetScale(1.0, 1.0)
        self.icon.LoadImage(item.GetIconImageFileName())
        width = self.icon.GetWidth()
        height = self.icon.GetHeight()
        scale = 1.0
        biggest = max(width, height)
        if biggest > self.ICON_BOX - 2:
            scale = float(self.ICON_BOX - 2) / biggest
        self.icon.SetScale(scale, scale)
        self.icon.SetPosition(ICON_X + int((self.ICON_BOX - width * scale) / 2), 4 + int((self.ICON_BOX - height * scale) / 2))

        count = data["count"]
        if count > 1:
            self.countText.SetText("%d" % count)
            self.countText.SetPosition(ICON_X + self.ICON_BOX - 6 - len(self.countText.GetText()) * 6, 4 + self.ICON_BOX - 14)
            self.countText.Show()
        else:
            self.countText.Hide()

        self.nameText.SetText(self.market.GetItemName(data))
        self.sellerText.SetText("Sprzedawca: %s" % data["seller_name"])
        bonuses = self.market.CountBonuses(data)
        if bonuses > 0:
            self.bonusText.SetText("Bonusy: %d" % bonuses)
        else:
            self.bonusText.SetText("")
        self.quantityText.SetText("x%d" % count if count > 1 else "")
        self.priceText.SetText(self.market.FormatPrice(data))
        if count > 1:
            self.unitText.SetText("%s / szt." % self.market.FormatUnitPrice(data))
        else:
            self.unitText.SetText("")
        self.check.SetChecked(self.market.IsSelected(data))
        self.__RefreshBackground()
        self.Show()

    def RefreshCheck(self):
        if self.data:
            self.check.SetChecked(self.market.IsSelected(self.data))

    def __OnCheck(self):
        if self.data:
            self.market.ToggleSelected(self.data)

    def Clear(self):
        self.data = None
        self.hover = False
        self.Hide()

    def __RefreshBackground(self):
        if self.hover:
            self.bg.SetColor(0x306EDC82)
        elif self.index % 2 == 0:
            self.bg.SetColor(0x14FFFFFF)
        else:
            self.bg.SetColor(0x08FFFFFF)

    def OnMouseOverIn(self):
        self.hover = True
        self.__RefreshBackground()
        if self.data:
            self.market.ShowItemToolTip(self.data)

    def OnMouseOverOut(self):
        self.hover = False
        self.__RefreshBackground()
        self.market.HideItemToolTip()

    def __OnBuy(self):
        if self.data:
            self.market.AskBuy(self.data)

    # MT2009_PLUS_UPSTREAM_2_0_76: a right click on the offer ticks it for
    # "Kup wszystko" as its box does.
    def OnMouseRightButtonUp(self):
        if self.data:
            self.market.ToggleSelected(self.data)
        return True


class FleaMarketWindow(ui.BoardWithTitleBar):
    SUGGESTION_LIMIT = 5

    def __init__(self):
        ui.BoardWithTitleBar.__init__(self)
        import offlineshopsearch
        self.categories = BuildCategories()
        self.category = 0
        self.expandedParent = None
        self.sortIndex = 0
        # CUSTOM_FLEA_MULTI_SORT_V1: Cena i Ilosc licza sie NIEZALEZNIE od
        # siebie (kazda ma wlasny kierunek asc/desc/brak) zamiast jednego
        # wspolnego trybu - dzieki temu mozna posortowac np. rownoczesnie po
        # najnizszej cenie i (przy remisie) najwiekszej ilosci. Kolumna
        # klikniet jako OSTATNIA jest sortem glownym, druga (jesli ustawiona)
        # dobija remisy.
        self.priceSortDir = "asc"
        self.countSortDir = None
        self.primarySortColumn = "price"
        self.allItems = []
        self.pendingItems = []
        self.items = []
        self.page = 0
        self.isLoading = False
        self.seenIds = set()
        self.lastRequest = None
        self.searchAt = None
        self.nextApply = 0.0
        self.dirty = False
        self.tooltip = None
        self.questionDialog = None
        self.buyData = None
        self.guestBoard = None
        self.rows = []
        self.categoryButtons = []
        self.keepers = []
        self.itemNames = None
        self.suggestionNames = []
        # "Kup wiele": the ticked offers by (owner, item id), and the run.
        self.selected = {}
        self.multiBuy = None
        self.popupDialog = None
        self.bonusFilters = []  # CUSTOM_FLEA_BONUS_FILTER_V1: [(attrType, minValue), ...]
        # MT2009_PLUS_UPSTREAM_2_0_76: the bonus filter's minimum number of
        # bonuses, the page kept while offers stream in, the tooltip's price
        # sample, Tab through the suggestions, Enter after a purchase.
        self.minBonusCount = 0
        self.streamPage = None
        self.marketPriceStats = None
        self.lastWheelAt = -1.0
        self.suggestionTabIndex = -1
        self.tabCompletionText = None
        self.purchaseEnterHeld = False
        self.purchaseEnterUntil = 0.0
        self.quantityDialog = offlineshopsearch.FleaMarketQuantityDialog(self)
        self.bonusFilterDialog = FleaMarketBonusFilterDialog(self)
        self.__Build()
        self.Hide()

    def __del__(self):
        ui.BoardWithTitleBar.__del__(self)

    # ---- budowa okna ----------------------------------------------------------
    def __Build(self):
        self.SetSize(WINDOW_WIDTH, WINDOW_HEIGHT)
        self.AddFlag("movable")
        self.AddFlag("float")
        self.SetTitleName("Dom Towarowy")
        self.SetCloseEvent(self.Close)

        # panel kategorii
        # CUSTOM_FLEA_COLLAPSIBLE_CATEGORIES_V1: tylko jeden rodzic naraz moze
        # byc rozwiniety, wiec karcie wystarczy miejsce na najwieksza mozliwa
        # liczbe widocznych na raz wierszy (wszystkie najwyzszego poziomu plus
        # dzieci najwiekszego rodzica), a nie na WSZYSTKIE wiersze naraz.
        topLevelCount = sum(1 for c in self.categories if c["indent"] == 0)
        maxChildren = 0
        for parentIndex, c in enumerate(self.categories):
            if c["hasChildren"]:
                childCount = sum(1 for other in self.categories if other["parentIndex"] == parentIndex)
                maxChildren = max(maxChildren, childCount)
        self.__MakeCard(SIDEBAR_X, SIDEBAR_Y, SIDEBAR_WIDTH, (topLevelCount + maxChildren) * CATEGORY_HEIGHT + 8)
        for index, category in enumerate(self.categories):
            button = FleaCategoryButton(self, index, category["label"], category["indent"])
            button.SetParent(self)
            if category["hasChildren"]:
                button.SetExpanded(False)
            self.categoryButtons.append(button)
        self.categoryButtons[0].SetSelected(True)
        self.__RelayoutCategories()

        # pasek wyszukiwania
        label = self.__MakeText(MAIN_X, SIDEBAR_Y + 3, "Nazwa:")
        label.SetPackedFontColor(COLOR_HEAD)
        self.searchEdit = self.__MakeEdit(MAIN_X + 46, SIDEBAR_Y, 330, 32, editClass=MarketEditLine)
        self.searchEdit.OnIMEUpdate = ui.__mem_func__(self.__OnSearchTextChanged)
        self.searchEdit.SAFE_SetReturnEvent(self.Search)
        # MT2009_PLUS_UPSTREAM_2_0_76: Tab puts the next suggestion in.
        self.searchEdit.SetTabEvent(ui.__mem_func__(self.__CycleSuggestion))
        self.searchButton = self.__MakeButton(MAIN_X + 384, SIDEBAR_Y - 2, 84, "Szukaj", self.Search)

        # CUSTOM_FLEA_COMPACT_TOOLBAR_V1: "Odswiez"/"Wyczysc filtry" jako male
        # ikonki przy przycisku zamkniecia okna. SetWindowHorizontalAlignRight
        # okazal sie zawodny (przyciski ladowaly sie jeden na drugim niezaleznie
        # od przekazywanych offsetow) - liczymy wiec ich pozycje wprost z
        # PRAWDZIWEJ pozycji przycisku zamkniecia (global -> lokalne wspolrzedne
        # tego okna), co dziala niezaleznie od tamtych niejasnosci.
        # d:/ymir work/ui/public/small_button_*.sub ma wlasny minimalny
        # rozmiar tiled-tekstury i ignoruje mniejszy SetSize (dlatego "C"
        # wychodzilo wieksze niz prosiles i zachodzilo na "odswiez") - "C"
        # dostaje wiec ZERO tla/tekstury, tylko tekst, w DOKLADNYM rozmiarze
        # przycisku zamkniecia okna (zeby wyglad sie zgadzal 1:1).
        closeButton = self.titleBar.btnClose
        closeGlobalX, closeGlobalY = closeButton.GetGlobalPosition()
        myGlobalX, myGlobalY = self.GetGlobalPosition()
        closeLocalX = closeGlobalX - myGlobalX
        closeLocalY = closeGlobalY - myGlobalY
        iconW = closeButton.GetWidth()
        iconH = closeButton.GetHeight()
        ICON_GAP = 10
        refreshX = closeLocalX - iconW - ICON_GAP
        clearX = refreshX - iconW - ICON_GAP

        # close_button_*.sub ma wpalony w tekstura sam ksztalt "X" - uzywajac
        # go dla odswiez/wyczysc wygladalo to jak DRUGI przycisk zamkniecia
        # okna, myllace. Wracamy wiec do wlasnego tla (ui.Bar, dokladny
        # rozmiar 1:1 z przyciskiem zamkniecia) bez zadnej tekstury-przycisku,
        # z samym tekstem na wierzchu - jednolity styl dla obu ikonek, zero
        # ryzyka pomylki z "X" i zero problemu z minimalnym rozmiarem
        # tiled-tekstur (small_button_*.sub/middle_button_*.sub).
        def MakeIconBox(x, y, text, event, tooltip):
            outer = ui.Bar()
            outer.SetParent(self)
            outer.SetPosition(x, y)
            outer.SetSize(iconW, iconH)
            outer.SetColor(COLOR_CARD_BORDER)
            outer.AddFlag("not_pick")
            outer.Show()
            self.keepers.append(outer)
            inner = ui.Bar()
            inner.SetParent(self)
            inner.SetPosition(x + 1, y + 1)
            inner.SetSize(iconW - 2, iconH - 2)
            inner.SetColor(COLOR_CARD_FILL)
            inner.AddFlag("not_pick")
            inner.Show()
            self.keepers.append(inner)
            button = ui.Button()
            button.SetParent(self)
            button.SetPosition(x, y)
            button.SetSize(iconW, iconH)
            button.SetText(text)
            button.SetEvent(event)
            button.SetToolTipText(tooltip, 0, -23)
            button.Show()
            return button

        self.refreshButton = MakeIconBox(refreshX, closeLocalY, "R", self.Refresh, "Odswiez")
        self.clearButton = MakeIconBox(clearX, closeLocalY, "C", self.ClearFilters, "Wyczysc filtry")

        label = self.__MakeText(MAIN_X, SIDEBAR_Y + 31, "Cena od:")
        label.SetPackedFontColor(COLOR_HEAD)
        # MT2009_PLUS_UPSTREAM_2_0_76: not numbers only - 1.5kk, 500k (ParseMarketPrice).
        self.priceMinEdit = self.__MakeEdit(MAIN_X + 56, SIDEBAR_Y + 28, 110, 12)
        label = self.__MakeText(MAIN_X + 176, SIDEBAR_Y + 31, "do:")
        label.SetPackedFontColor(COLOR_HEAD)
        self.priceMaxEdit = self.__MakeEdit(MAIN_X + 198, SIDEBAR_Y + 28, 110, 12)
        # Stary cykl-sort zostaje (Nazwa A-Z/Sprzedawca A-Z go jeszcze uzywaja
        # wewnetrznie do przechowywania stanu), ale nie pokazujemy go juz jako
        # osobny przycisk - Cena/Ilosc sortuje sie teraz klikajac naglowki
        # kolumn. "Filtry" zajmuje to miejsce w pasku.
        self.sortButton = self.__MakeButton(MAIN_X + 330, SIDEBAR_Y + 26, 150, SORT_MODES[0][1], self.CycleSort)
        self.sortButton.Hide()
        self.bonusFilterButton = self.__MakeButton(MAIN_X + 330, SIDEBAR_Y + 26, 150, "Filtry", self.OpenBonusFilters)
        # "Kup wiele"
        self.buyAllButton = self.__MakeButton(MAIN_X + 488, SIDEBAR_Y + 26, 120, "Kup wszystko (0)", self.AskBuySelected)
        self.unselectButton = self.__MakeButton(MAIN_X + 612, SIDEBAR_Y + 26, 56, "Odznacz", self.ClearSelection)

        # podpowiedzi nazw
        self.suggestionBackground = ui.SlotBar()
        self.suggestionBackground.SetParent(self)
        self.suggestionBackground.SetPosition(MAIN_X + 46, SIDEBAR_Y + 22)
        self.suggestionBackground.SetSize(330, 1)
        self.suggestionBackground.AddFlag("not_pick")
        self.suggestionBackground.Hide()
        self.suggestionList = ui.ListBox()
        self.suggestionList.SetParent(self.suggestionBackground)
        self.suggestionList.SetPosition(4, 3)
        self.suggestionList.SetSize(322, 1)
        self.suggestionList.SetTextCenterAlign(False)
        self.suggestionList.SetEvent(self.__OnSelectSuggestion)
        self.suggestionList.Hide()

        # naglowek listy - "Ilosc"/"Cena" klikalne, sortuja po tej kolumnie
        # (CUSTOM_FLEA_HEADER_SORT_V1); strzalka v/^ dopisywana do tekstu
        # pokazuje, czy to aktualny sort i w ktora strone.
        self.__MakeBar(MAIN_X, ROWS_Y - 20, ROW_WIDTH, 18, 0x33FFFFFF)
        self.pageCheck = FleaCheckBox(ui.__mem_func__(self.SelectPage))
        self.pageCheck.SetParent(self)
        self.pageCheck.SetPosition(MAIN_X + 6, ROWS_Y - 19)
        self.pageCheck.Show()
        self.__MakeText(MAIN_X + TEXT_X, ROWS_Y - 18, "Przedmiot").SetPackedFontColor(COLOR_HEAD)

        self.countHeaderLabel = self.__MakeText(MAIN_X + 340, ROWS_Y - 18, "Ilosc")
        self.countHeaderLabel.SetPackedFontColor(COLOR_HEAD)
        self.countHeaderButton = ui.Button()
        self.countHeaderButton.SetParent(self)
        self.countHeaderButton.SetPosition(MAIN_X + 335, ROWS_Y - 20)
        self.countHeaderButton.SetSize(70, 18)
        self.countHeaderButton.SetEvent(self.OnClickCountHeader)
        self.countHeaderButton.Show()
        self.keepers.append(self.countHeaderButton)

        self.priceHeaderLabel = self.__MakeText(102, ROWS_Y - 18, "Cena")
        self.priceHeaderLabel.SetWindowHorizontalAlignRight()
        self.priceHeaderLabel.SetHorizontalAlignRight()
        self.priceHeaderLabel.SetPosition(114, ROWS_Y - 18)
        self.priceHeaderLabel.SetPackedFontColor(COLOR_HEAD)
        self.priceHeaderButton = ui.Button()
        self.priceHeaderButton.SetParent(self)
        self.priceHeaderButton.SetPosition(MAIN_RIGHT - 150, ROWS_Y - 20)
        self.priceHeaderButton.SetSize(140, 18)
        self.priceHeaderButton.SetEvent(self.OnClickPriceHeader)
        self.priceHeaderButton.Show()
        self.keepers.append(self.priceHeaderButton)
        self.__UpdateSortHeaders()

        # wiersze ofert
        self.__MakeCard(MAIN_X - 2, ROWS_Y - 2, ROW_WIDTH + 4, ROWS_PER_PAGE * ROW_HEIGHT + 2)
        for index in xrange(ROWS_PER_PAGE):
            row = FleaRow(self)
            row.SetParent(self)
            row.SetPosition(MAIN_X, ROWS_Y + index * ROW_HEIGHT)
            row.Hide()
            self.rows.append(row)
        self.emptyText = self.__MakeText(0, 0, "")
        self.emptyText.SetWindowHorizontalAlignCenter()
        self.emptyText.SetHorizontalAlignCenter()
        self.emptyText.SetPosition(SIDEBAR_WIDTH // 2 + 6, ROWS_Y + 80)
        self.emptyText.SetPackedFontColor(COLOR_DIM)

        # stopka
        footerY = ROWS_Y + ROWS_PER_PAGE * ROW_HEIGHT + 8
        # statusText usuniety z widoku (za duzo tekstu na dole) - __UpdateStatus
        # nadal go ustawia, ale okno go nie pokazuje.
        self.statusText = self.__MakeText(MAIN_X + 4, footerY + 4, "")
        self.statusText.SetPackedFontColor(COLOR_DIM)
        self.statusText.Hide()
        # CUSTOM_FLEA_COMPACT_TOOLBAR_V1: "<< N/Total >>" wysrodkowane wzgledem
        # obszaru listy (MAIN_X..MAIN_RIGHT), NIE calego okna - okno liczy sie
        # razem z panelem kategorii po lewej, wiec centrowanie na WINDOW_WIDTH
        # wygladalo przesuniete w lewo wzgledem tego, co faktycznie widac nad
        # stopka. middle_button_*.sub (uzywany przez __MakeButton) ma
        # wlasny minimalny rozmiar kafelkowej tekstury i ignoruje maly
        # SetSize (przycisk renderowal sie szerzej niz prosilismy, nachodzac
        # na tekst strony) - "<<"/">>" dostaja wiec, jak "C" wyzej, zero
        # tla/tekstury, tylko tekst w dokladnie zadanym rozmiarze.
        PAGE_BUTTON_WIDTH = 30
        PAGE_TEXT_WIDTH = 60
        PAGE_GAP = 10
        clusterWidth = PAGE_BUTTON_WIDTH + PAGE_GAP + PAGE_TEXT_WIDTH + PAGE_GAP + PAGE_BUTTON_WIDTH
        clusterLeft = MAIN_X + (ROW_WIDTH - clusterWidth) // 2

        self.previousButton = ui.Button()
        self.previousButton.SetParent(self)
        self.previousButton.SetPosition(clusterLeft, footerY)
        self.previousButton.SetSize(PAGE_BUTTON_WIDTH, 20)
        self.previousButton.SetText("<<")
        self.previousButton.SetEvent(self.PreviousPage)
        self.previousButton.Show()
        self.keepers.append(self.previousButton)

        # SetHorizontalAlignCenter() na TextLine NIE centruje tekstu w obrebie
        # SetSize (zmierzone pikselowo: tekst nadal przykleja sie do lewej
        # krawedzi slotu, klaster wyglada jakby "1 / 752" bylo duzo blizej
        # "<<" niz ">>"). Jedyny pewny sposob to przeliczac pozycje
        # RECZNIE po kazdym SetText(), na podstawie faktycznej, zmierzonej
        # szerokosci tekstu (GetTextSize) - patrz __RefreshRows.
        self.pageTextCenterX = clusterLeft + PAGE_BUTTON_WIDTH + PAGE_GAP + PAGE_TEXT_WIDTH // 2
        self.pageTextY = footerY + 4
        self.pageText = self.__MakeText(self.pageTextCenterX, self.pageTextY, "")
        self.pageText.SetPackedFontColor(COLOR_TEXT)

        self.nextButton = ui.Button()
        self.nextButton.SetParent(self)
        self.nextButton.SetPosition(clusterLeft + PAGE_BUTTON_WIDTH + PAGE_GAP + PAGE_TEXT_WIDTH + PAGE_GAP, footerY)
        self.nextButton.SetSize(PAGE_BUTTON_WIDTH, 20)
        self.nextButton.SetText(">>")
        self.nextButton.SetEvent(self.NextPage)
        self.nextButton.Show()
        self.keepers.append(self.nextButton)

    def __MakeCard(self, x, y, width, height):
        outer = ui.Bar()
        outer.SetParent(self)
        outer.SetPosition(x, y)
        outer.SetSize(width, height)
        outer.SetColor(COLOR_CARD_BORDER)
        outer.AddFlag("not_pick")
        outer.Show()
        inner = ui.Bar()
        inner.SetParent(self)
        inner.SetPosition(x + 1, y + 1)
        inner.SetSize(width - 2, height - 2)
        inner.SetColor(COLOR_CARD_FILL)
        inner.AddFlag("not_pick")
        inner.Show()
        self.keepers.append(outer)
        self.keepers.append(inner)

    def __MakeBar(self, x, y, width, height, color):
        bar = ui.Bar()
        bar.SetParent(self)
        bar.SetPosition(x, y)
        bar.SetSize(width, height)
        bar.SetColor(color)
        bar.AddFlag("not_pick")
        bar.Show()
        self.keepers.append(bar)
        return bar

    def __MakeText(self, x, y, text):
        line = ui.TextLine()
        line.SetParent(self)
        line.SetPosition(x, y)
        line.SetText(text)
        line.Show()
        self.keepers.append(line)
        return line

    def __MakeButton(self, x, y, width, text, event):
        button = ui.Button()
        button.SetParent(self)
        button.SetPosition(x, y)
        button.SetSize(width, 25)
        button.SetUpVisual("d:/ymir work/ui/public/middle_button_01.sub")
        button.SetOverVisual("d:/ymir work/ui/public/middle_button_02.sub")
        button.SetDownVisual("d:/ymir work/ui/public/middle_button_03.sub")
        button.SetText(text)
        button.SetEvent(event)
        button.Show()
        self.keepers.append(button)
        return button

    def __MakeEdit(self, x, y, width, maxLength, numeric=False, editClass=ui.EditLine):
        slot = ui.SlotBar()
        slot.SetParent(self)
        slot.SetPosition(x, y)
        slot.SetSize(width, 20)
        slot.AddFlag("not_pick")
        slot.Show()
        self.keepers.append(slot)

        edit = editClass()
        edit.SetParent(slot)
        edit.SetPosition(3, 3)
        edit.SetSize(width - 6, 17)
        edit.SetMax(maxLength)
        if numeric:
            edit.SetNumberMode()
        edit.SAFE_SetReturnEvent(self.Search)
        edit.Show()
        self.keepers.append(edit)
        return edit

    # ---- sprawy techniczne ------------------------------------------------------
    def SetToolTip(self, tooltip):
        self.tooltip = proxy(tooltip) if tooltip else None

    def SetGuestBoard(self, board):
        # Natywny modul ikashop ma jeden cel pakietow: wyniki wyszukiwania i otwarcie
        # sklepu. Flea Market przejmuje ten cel, wiec przekazuje sklep do okna goscia.
        self.guestBoard = proxy(board) if board else None

    @ui.WindowDestroy
    def Destroy(self):
        self.multiBuy = None
        self.selected = {}
        if self.popupDialog:
            self.popupDialog.Hide()
            self.popupDialog = None
        if self.questionDialog:
            self.questionDialog.Close()
            self.questionDialog = None
        if self.quantityDialog:
            self.quantityDialog.Close()
            self.quantityDialog = None
        if self.bonusFilterDialog:
            self.bonusFilterDialog.Close()
            self.bonusFilterDialog = None
        self.allItems = []
        self.pendingItems = []
        self.items = []
        self.rows = []
        self.categoryButtons = []
        self.keepers = []
        self.suggestionNames = []
        self.Hide()

    def GetItemName(self, data):
        name = data.get("_flea_name", None)
        if name is None:
            item.SelectItem(data["vnum"])
            name = item.GetItemName()
            # Ksiega umiejetnosci to jeden vnum, a konkretna umiejetnosc siedzi w gniezdzie 0.
            if data["vnum"] in (50300, 70037, 70055):
                try:
                    skillVnum = int(data.get("sockets", [0])[0])
                    skillName = skill.GetSkillName(skillVnum) if skillVnum in SKILL_IDS else ""
                    if skillName:
                        name = "%s - %s" % (skillName, name)
                except:
                    pass
            data["_flea_name"] = name
        return name

    def CountBonuses(self, data):
        count = data.get("_flea_bonus", None)
        if count is None:
            count = 0
            try:
                for attr in data.get("attrs", []):
                    if attr[0]:
                        count += 1
            except:
                count = 0
            data["_flea_bonus"] = count
        return count

    def __GetTypes(self, data):
        itemType = data.get("_flea_type", None)
        subType = data.get("_flea_sub", None)
        if itemType is None or subType is None:
            item.SelectItem(data["vnum"])
            itemType = item.GetItemType()
            subType = item.GetItemSubType()
            data["_flea_type"] = itemType
            data["_flea_sub"] = subType
        return itemType, subType

    def __IsAntiFlag(self, data, classFlag):
        # CUSTOM_FLEA_CLASS_FILTER_V1: cached per (item, flag) so switching
        # between "Zbroje - Wojownik/Ninja/Sura/Szaman" doesn't re-select
        # every offer's item on each click.
        cache = data.get("_flea_antiflag")
        if cache is None:
            cache = {}
            data["_flea_antiflag"] = cache
        cached = cache.get(classFlag)
        if cached is None:
            item.SelectItem(data["vnum"])
            cached = bool(item.IsAntiFlag(classFlag))
            cache[classFlag] = cached
        return cached

    def __GetTotalPrice(self, data):
        return data["price"] + data.get("cheque", 0) * YANG_PER_CHEQUE

    def __GetUnitPrice(self, data):
        return self.__GetTotalPrice(data) / max(1, data["count"])

    def FormatPrice(self, data):
        price = localeInfo.NumberToMoneyString(data["price"])
        cheque = data.get("cheque", 0)
        if cheque:
            price += ", %s" % localeInfo.NumberToCheque(cheque)
        return price

    def FormatUnitPrice(self, data):
        return self.FormatPrice(self.GetStackPurchasePrice(data, 1))

    def GetStackPurchasePrice(self, data, quantity):
        available = max(1, data["count"])
        quantity = max(1, min(int(quantity), available))
        # Podglad identyczny z serwerem: czesciowy zakup zaokraglany w gore o 1 Yang.
        return {
            "price": (data["price"] * quantity + available - 1) / available,
            "cheque": (data.get("cheque", 0) * quantity + available - 1) / available,
        }

    # ---- podpowiedzi nazw -------------------------------------------------------
    def __GetItemNames(self):
        if self.itemNames is not None:
            return self.itemNames
        names = []
        seen = set()
        try:
            for name in ikashop.GetItemNames():
                plus = name.find("+")
                if plus != -1 and plus + 1 < len(name) and name[plus + 1:].isdigit():
                    name = name[:plus]
                key = PolishLower(name)
                if key not in seen:
                    seen.add(key)
                    names.append(name)
        except:
            pass
        # MT2009_PLUS_UPSTREAM_2_0_76: "<skill> - <book>" for every book that
        # is one vnum with the skill in its socket (BOOK_VNUMS).
        for bookVnum in BOOK_VNUMS:
            try:
                item.SelectItem(bookVnum)
                bookName = item.GetItemName()
            except:
                continue
            for skillVnum in SKILL_IDS:
                try:
                    skillName = skill.GetSkillName(skillVnum)
                except:
                    continue
                if not skillName:
                    continue
                name = "%s - %s" % (skillName, bookName)
                key = PolishLower(name)
                if key not in seen:
                    seen.add(key)
                    names.append(name)
        self.itemNames = names
        return names

    def __HideSuggestions(self):
        self.suggestionTabIndex = -1
        self.tabCompletionText = None
        self.suggestionNames = []
        self.suggestionList.ClearItem()
        self.suggestionList.Hide()
        self.suggestionBackground.Hide()

    def __UpdateSuggestions(self):
        query = PolishLower(self.searchEdit.GetText().strip())
        if len(query) < 2:
            self.__HideSuggestions()
            return
        startsWith = []
        contains = []
        for name in self.__GetItemNames():
            lowerName = PolishLower(name)
            if lowerName.startswith(query):
                startsWith.append(name)
            elif query in lowerName:
                contains.append(name)
        startsWith.sort(key=lambda name: PolishLower(name))
        contains.sort(key=lambda name: PolishLower(name))
        self.suggestionNames = (startsWith + contains)[:self.SUGGESTION_LIMIT]
        if not self.suggestionNames:
            self.__HideSuggestions()
            return
        lineHeight = 17
        height = len(self.suggestionNames) * lineHeight + 6
        self.suggestionList.ClearItem()
        self.suggestionList.SetSize(322, height - 6)
        for index, name in enumerate(self.suggestionNames):
            self.suggestionList.InsertItem(index, name)
        self.suggestionList.LocateItem()
        self.suggestionBackground.SetSize(330, height)
        self.suggestionBackground.Show()
        self.suggestionList.Show()
        self.suggestionBackground.SetTop()
        self.suggestionList.SetTop()

    # MT2009_PLUS_UPSTREAM_2_0_76: Tab in the search line takes the next
    # suggestion into it, round the list; the search follows.
    def __CycleSuggestion(self):
        if not self.suggestionNames:
            self.__UpdateSuggestions()
        if not self.suggestionNames:
            return
        index = (self.suggestionTabIndex + 1) % len(self.suggestionNames)
        self.suggestionTabIndex = index
        self.tabCompletionText = self.suggestionNames[index]
        try:
            self.searchEdit.ClearSelection()
        except AttributeError:
            pass
        self.searchEdit.SetText(self.tabCompletionText)
        self.searchEdit.SetEndPosition()
        self.searchAt = app.GetTime() + SEARCH_DELAY
        self.ApplyFilters()

    def __OnSelectSuggestion(self, index, name):
        if index < 0 or index >= len(self.suggestionNames):
            return
        self.searchEdit.SetText(self.suggestionNames[index])
        self.__HideSuggestions()
        self.Refresh()
        self.searchEdit.SetFocus()

    def __OnSearchTextChanged(self):
        ui.EditLine.OnIMEUpdate(self.searchEdit)
        if self.tabCompletionText is not None and self.searchEdit.GetText() == self.tabCompletionText:
            return
        self.suggestionTabIndex = -1
        self.tabCompletionText = None
        self.searchAt = app.GetTime() + SEARCH_DELAY
        self.__UpdateSuggestions()
        self.ApplyFilters()

    # ---- zapytanie do serwera ---------------------------------------------------------
    def __ParsePrice(self, edit):
        return ParseMarketPrice(edit.GetText())

    def __GetServerQuery(self):
        query = self.searchEdit.GetText().strip()
        return query.replace('"', "").replace("'", "")[:60]

    def __FindSkillIds(self, query):
        # Ksiega umiejetnosci to jeden vnum, a umiejetnosc siedzi w gniezdzie 0 - serwer nie zna jej nazwy,
        # wiec klient podaje mu, ktore umiejetnosci pasuja do wpisanego tekstu.
        key = PolishLower(query)
        if len(key) < 2:
            return []
        bookNames = []
        for vnum in BOOK_VNUMS:
            item.SelectItem(vnum)
            bookNames.append(item.GetItemName())
        ids = []
        for skillVnum in SKILL_IDS:
            try:
                name = skill.GetSkillName(skillVnum)
            except:
                continue
            if not name:
                continue
            for bookName in bookNames:
                if key in PolishLower("%s - %s" % (name, bookName)):
                    ids.append(skillVnum)
                    break
            if len(ids) >= 40:
                break
        return ids

    def __BuildQueryText(self, query):
        if not query:
            return ""
        ids = self.__FindSkillIds(query)
        if ids:
            return "%s|%s" % (query, ",".join([str(skillVnum) for skillVnum in ids]))
        return query
    def __GetServerSortCode(self):
        # Serwer i tak dostaje tylko przyblizona podpowiedz sortowania (0/1/2)
        # - prawdziwe, kombinowane sortowanie (cena+ilosc) liczy sie zawsze
        # lokalnie w ApplyFilters.
        if self.primarySortColumn == "price" and self.priceSortDir == "desc":
            return 1
        return 0

    def __BuildRequest(self):
        category = self.categories[self.category]
        return (category["type"], category["sub"], self.__GetServerSortCode(),
                self.__ParsePrice(self.priceMinEdit), self.__ParsePrice(self.priceMaxEdit), self.__GetServerQuery())

    def __Request(self):
        request = self.__BuildRequest()
        # MT2009_PLUS_UPSTREAM_2_0_76: "Odswiez" of the same search stays on
        # its page while the offers stream in; a new search starts at 1.
        self.streamPage = self.page if request == self.lastRequest else None
        if request != self.lastRequest:
            self.page = 0
        self.lastRequest = request
        self.searchAt = None
        self.pendingItems = []
        self.seenIds = set()
        self.isLoading = True
        self.dirty = False
        net.SendChatPacket("/flea_filter %d %d %d %d %d" % request[:5])
        net.SendChatPacket("/flea_query %s" % self.__BuildQueryText(request[5]))
        ikashop.SendRandomSearchFillRequest()
        self.__UpdateStatus()

    def Refresh(self):
        self.__HideSuggestions()
        self.__Request()

    # MT2009_PLUS_UPSTREAM_2_0_76: the Enter that confirmed a purchase is no
    # search in the line behind the question.
    def SuppressPurchaseEnter(self):
        self.purchaseEnterUntil = app.GetTime() + 0.20
        self.purchaseEnterHeld = True
        for edit in (self.searchEdit, self.priceMinEdit, self.priceMaxEdit):
            edit.KillFocus()

    def Search(self):
        if self.purchaseEnterHeld:
            if app.IsPressed(app.DIK_RETURN) or app.IsPressed(getattr(app, "DIK_NUMPADENTER", app.DIK_RETURN)):
                return
            self.purchaseEnterHeld = False
        if app.GetTime() < self.purchaseEnterUntil:
            return
        if ((self.questionDialog and self.questionDialog.IsShow()) or
                (self.quantityDialog and self.quantityDialog.IsShow())):
            return
        self.__HideSuggestions()
        self.__Request()

    def OnUpdate(self):
        # MT2009_PLUS_SIDEKICK_WARP_SAFE_V1: no query or purchase on the way
        # to another core (warpsafe.py).
        import warpsafe
        if not warpsafe.InGame():
            return
        now = app.GetTime()
        if self.searchAt is not None and now >= self.searchAt:
            self.searchAt = None
            if self.IsShow() and self.__BuildRequest() != self.lastRequest:
                self.__Request()
        if self.dirty and now >= self.nextApply:
            self.dirty = False
            self.nextApply = now + APPLY_INTERVAL
            self.ApplyFilters(resetPage=False)
        if self.multiBuy is not None:
            self.__UpdateMultiBuy(now)

    # ---- kategorie, sortowanie, filtry ------------------------------------------------
    def __RelayoutCategories(self):
        # CUSTOM_FLEA_COLLAPSIBLE_CATEGORIES_V1: pokazuje wszystkie wiersze
        # najwyzszego poziomu, plus dzieci TYLKO rozwinietego rodzica (jesli
        # jest), jedne pod drugimi bez dziur po ukrytych wierszach.
        visibleIndex = 0
        for index, category in enumerate(self.categories):
            button = self.categoryButtons[index]
            if category["hasChildren"]:
                button.SetExpanded(self.expandedParent == index)
            visible = category["indent"] == 0 or category["parentIndex"] == self.expandedParent
            if not visible:
                button.Hide()
                continue
            button.SetPosition(SIDEBAR_X + 6, SIDEBAR_Y + 4 + visibleIndex * CATEGORY_HEIGHT)
            button.Show()
            visibleIndex += 1

    def SetCategory(self, index):
        category = self.categories[index]
        if index == self.category:
            # Klikniecie juz wybranej kategorii z dziecmi zwija/rozwija ja -
            # inaczej klikniecie nie robi nic, tak jak zawsze.
            if category["hasChildren"]:
                self.expandedParent = None if self.expandedParent == index else index
                self.__RelayoutCategories()
            return
        self.categoryButtons[self.category].SetSelected(False)
        self.category = index
        self.categoryButtons[index].SetSelected(True)
        if category["hasChildren"]:
            self.expandedParent = index
        elif category["parentIndex"] is not None:
            self.expandedParent = category["parentIndex"]
        else:
            self.expandedParent = None
        self.__RelayoutCategories()
        self.ApplyFilters()
        self.__Request()

    def CycleSort(self):
        self.sortIndex = (self.sortIndex + 1) % len(SORT_MODES)
        self.sortButton.SetText(SORT_MODES[self.sortIndex][1])
        self.__UpdateSortHeaders()
        self.ApplyFilters()
        if self.__BuildRequest() != self.lastRequest:
            self.__Request()

    def __FindSortIndex(self, modeName):
        for index, mode in enumerate(SORT_MODES):
            if mode[0] == modeName:
                return index
        return 0

    def __ToggleColumn(self, column):
        # CUSTOM_FLEA_MULTI_SORT_V1: kazdy klik na danej kolumnie przelacza
        # jej WLASNY kierunek (brak -> rosnaco -> malejaco -> rosnaco...) i
        # czyni ja sortem GLOWNYM; druga kolumna, jesli ma ustawiony
        # kierunek, zostaje sortem pomocniczym (dobija remisy). Dzieki temu
        # da sie miec naraz np. "Cena: rosnaco" + "Ilosc: malejaco".
        if column == "price":
            self.priceSortDir = {None: "asc", "asc": "desc", "desc": "asc"}[self.priceSortDir]
        else:
            self.countSortDir = {None: "asc", "asc": "desc", "desc": "asc"}[self.countSortDir]
        self.primarySortColumn = column
        self.__UpdateSortHeaders()
        self.ApplyFilters()
        if self.__BuildRequest() != self.lastRequest:
            self.__Request()

    def __UpdateSortHeaders(self):
        def Arrow(dir):
            return " v" if dir == "asc" else (" ^" if dir == "desc" else "")
        countSuffix = Arrow(self.countSortDir) + ("*" if self.primarySortColumn == "count" and self.priceSortDir else "")
        priceSuffix = Arrow(self.priceSortDir) + ("*" if self.primarySortColumn == "price" and self.countSortDir else "")
        self.countHeaderLabel.SetText("Ilosc" + countSuffix)
        self.priceHeaderLabel.SetText("Cena" + priceSuffix)

    def OnClickCountHeader(self):
        self.__ToggleColumn("count")

    def OnClickPriceHeader(self):
        self.__ToggleColumn("price")

    def ClearFilters(self):
        self.searchEdit.SetText("")
        self.priceMinEdit.SetText("")
        self.priceMaxEdit.SetText("")
        self.categoryButtons[self.category].SetSelected(False)
        self.category = 0
        self.categoryButtons[0].SetSelected(True)
        self.expandedParent = None
        self.__RelayoutCategories()
        self.sortIndex = 0
        self.sortButton.SetText(SORT_MODES[0][1])
        self.priceSortDir = "asc"
        self.countSortDir = None
        self.primarySortColumn = "price"
        self.__UpdateSortHeaders()
        self.__HideSuggestions()
        self.bonusFilterDialog.ClearAll()
        self.ApplyFilters()
        self.__Request()

    # ---- filtr bonusow (CUSTOM_FLEA_BONUS_FILTER_V1) ----------------------------------
    def OpenBonusFilters(self):
        self.bonusFilterDialog.Open()

    def SetBonusFilters(self, filters, minCount=0):
        self.bonusFilters = filters
        self.minBonusCount = minCount
        count = len(filters) + (1 if minCount else 0)
        self.bonusFilterButton.SetText("Filtry (%d)" % count if count else "Filtry")
        self.ApplyFilters()

    def __MatchesBonusFilters(self, data):
        if not self.bonusFilters:
            return True
        attrs = data.get("attrs", [])
        for attrType, minValue in self.bonusFilters:
            hit = False
            for attr in attrs:
                try:
                    if attr[0] == attrType and attr[1] >= minValue:
                        hit = True
                        break
                except (TypeError, IndexError):
                    continue
            if not hit:
                return False
        return True

    def __GetVnumOverride(self, vnum):
        override = CATEGORY_VNUM_OVERRIDES.get(vnum)
        if override is not None:
            return override
        for low, high, label in CATEGORY_VNUM_RANGE_OVERRIDES:
            if low <= vnum <= high:
                return label
        return None

    def __MatchesCategory(self, data):
        category = self.categories[self.category]
        wanted = category["types"]
        if wanted == (-1,):
            return True
        override = self.__GetVnumOverride(data["vnum"])
        if override is not None:
            return category["label"] == override
        itemType, subType = self.__GetTypes(data)
        if wanted == (-2,):
            for other in CATEGORY_DEFS:
                typeName = other[2]
                if not typeName or typeName == "OTHER":
                    continue
                otherNames = typeName if isinstance(typeName, tuple) else (typeName,)
                for otherName in otherNames:
                    if getattr(item, otherName, None) == itemType:
                        return False
            return True
        if itemType not in wanted:
            return False
        if category["sub"] != -1 and subType != category["sub"]:
            return False
        classFlag = category.get("classFlag", -1)
        if classFlag != -1 and self.__IsAntiFlag(data, classFlag):
            return False
        return True

    def ApplyFilters(self, resetPage=True):
        self.marketPriceStats = None
        query = PolishLower(self.searchEdit.GetText().strip())
        minimum = self.__ParsePrice(self.priceMinEdit)
        maximum = self.__ParsePrice(self.priceMaxEdit)
        items = []
        for data in self.allItems:
            price = self.__GetTotalPrice(data)
            if minimum and price < minimum:
                continue
            if maximum and price > maximum:
                continue
            if query:
                name = self.GetItemName(data)
                lowerName = data.get("_flea_lower_name")
                if lowerName is None:
                    lowerName = PolishLower(name)
                    data["_flea_lower_name"] = lowerName
                if query not in lowerName:
                    continue
                # CUSTOM_FLEA_PLUS_SEARCH_V1: tylko dla Ulepszaczy (zwoje z
                # +1..+9 zalewaly wyniki) - bez "+" w zapytaniu szukamy tylko
                # zwyklej wersji; wpisanie "+" jawnie wraca do zwyklego
                # dopasowania substring ("miecz+3" nadal znajdzie ta wersje).
                # Bron/zbroja itp. maja normalne dopasowanie zawsze - tam nikt
                # nie chce wpisywac pelnej nazwy, a stopni ulepszenia jest
                # wiele wiecej niz tylko ulepszaczowe +1..+9.
                if '+' not in query and _HasPlusSuffix(name):
                    itemType, _ = self.__GetTypes(data)
                    if itemType == getattr(item, "ITEM_TYPE_MATERIAL", None):
                        continue
            if not self.__MatchesCategory(data):
                continue
            if not self.__MatchesBonusFilters(data):
                continue
            if self.minBonusCount and self.CountBonuses(data) < self.minBonusCount:
                continue
            items.append(data)

        # CUSTOM_FLEA_MULTI_SORT_V1: kolumna sortu GLOWNEGO stosowana jest
        # JAKO OSTATNIA (Python sort jest stabilny), wiec przy remisach
        # zostaje kolejnosc z sortu POMOCNICZEGO - dzieki temu "Cena:
        # rosnaco" + "Ilosc: malejaco" naraz daje: najpierw najtansze, a przy
        # tej samej cenie - te z najwieksza iloscia.
        columnKeys = {"price": self.__GetTotalPrice, "count": lambda data: data["count"]}
        columnDirs = {"price": self.priceSortDir, "count": self.countSortDir}
        secondaryColumn = "count" if self.primarySortColumn == "price" else "price"
        if columnDirs[secondaryColumn]:
            items.sort(key=columnKeys[secondaryColumn], reverse=(columnDirs[secondaryColumn] == "desc"))
        primaryDir = columnDirs[self.primarySortColumn] or "asc"
        items.sort(key=columnKeys[self.primarySortColumn], reverse=(primaryDir == "desc"))

        self.items = items
        # MT2009_PLUS_UPSTREAM_2_0_76: a purchase, an offer gone or the next
        # batch of offers keeps the page (resetPage=False); a new filter goes
        # back to the first.
        if resetPage:
            self.page = 0
            self.streamPage = None
        else:
            lastPage = max(0, (len(items) - 1) // ROWS_PER_PAGE)
            target = self.streamPage if self.streamPage is not None else self.page
            self.page = min(target, lastPage)
        if not self.isLoading:
            self.streamPage = None
        if self.selected:
            # A fresh catalogue's offer stands for the ticked one (its price,
            # its stack); one no longer listed keeps what was seen.
            for data in self.allItems:
                key = (data["owner"], data["id"])
                if key in self.selected:
                    self.selected[key] = data
        self.__RefreshRows()

    # ---- wyswietlanie -------------------------------------------------------------------
    def __UpdateStatus(self):
        if self.isLoading:
            self.statusText.SetText("Wczytywanie ofert... pobrano %d" % len(self.pendingItems))
        elif len(self.allItems) >= MAX_LISTINGS:
            self.statusText.SetText("Pokazano %d ofert (limit) - zawez wyszukiwanie, zeby zobaczyc reszte." % len(self.allItems))
        else:
            self.statusText.SetText("Ofert: %d (pasuje do filtrow: %d)" % (len(self.allItems), len(self.items)))

    def __RefreshRows(self):
        first = self.page * ROWS_PER_PAGE
        for index in xrange(ROWS_PER_PAGE):
            itemIndex = first + index
            if itemIndex < len(self.items):
                self.rows[index].SetData(self.items[itemIndex], itemIndex)
            else:
                self.rows[index].Clear()
        pageCount = (len(self.items) + ROWS_PER_PAGE - 1) // ROWS_PER_PAGE
        self.pageText.SetText("%d / %d" % (self.page + 1 if pageCount else 0, pageCount))
        textWidth, _textHeight = self.pageText.GetTextSize()
        self.pageText.SetPosition(self.pageTextCenterX - textWidth // 2, self.pageTextY)
        if self.page > 0:
            self.previousButton.Enable()
        else:
            self.previousButton.Disable()
        if (self.page + 1) < pageCount:
            self.nextButton.Enable()
        else:
            self.nextButton.Disable()
        self.__RefreshSelection()
        if self.items:
            self.emptyText.SetText("")
        elif self.isLoading:
            self.emptyText.SetText("Wczytywanie ofert...")
        else:
            self.emptyText.SetText("Brak ofert spelniajacych kryteria")
        self.__UpdateStatus()

    def __ChangePage(self, direction):
        lastPage = max(0, (len(self.items) - 1) // ROWS_PER_PAGE)
        target = max(0, min(lastPage, self.page + direction))
        if target == self.page:
            return
        self.page = target
        if self.isLoading:
            self.streamPage = target
        self.__RefreshRows()

    def PreviousPage(self):
        self.__ChangePage(-1)

    def NextPage(self):
        self.__ChangePage(1)

    def OnMouseWheel(self, length):
        now = app.GetTime()
        if now - self.lastWheelAt < WHEEL_GAP:
            return True
        self.lastWheelAt = now
        if length > 0:
            self.PreviousPage()
        elif length < 0:
            self.NextPage()
        return True

    # MT2009_PLUS_UPSTREAM_2_0_76: the average of the cheapest offers of the
    # same item among the loaded ones, in the offer's tooltip.
    def __PriceComparisonKey(self, data):
        vnum = data["vnum"]
        sockets = tuple(data.get("sockets", ()))
        if vnum in BOOK_VNUMS:
            return (vnum, sockets[0] if sockets else 0, ())
        attrs = tuple(sorted((int(attr[0]), int(attr[1])) for attr in data.get("attrs", ()) if attr[0]))
        return (vnum, sockets, attrs)

    def __GetMarketPriceStats(self, data):
        if self.marketPriceStats is None:
            cheapest = {}
            for offer in self.allItems:
                count = int(offer.get("count", 0))
                if count <= 0 or offer.get("is_auction", False):
                    continue
                key = self.__PriceComparisonKey(offer)
                unitPrice = float(self.__GetTotalPrice(offer)) / count
                prices = cheapest.setdefault(key, [])
                if len(prices) < PRICE_SAMPLE or unitPrice < prices[-1]:
                    bisect.insort(prices, unitPrice)
                    if len(prices) > PRICE_SAMPLE:
                        prices.pop()
            self.marketPriceStats = dict((key, [sum(prices), len(prices), prices[0], prices[-1]])
                for key, prices in cheapest.items() if prices)
        return self.marketPriceStats.get(self.__PriceComparisonKey(data))

    def ShowItemToolTip(self, data):
        if self.tooltip:
            self.tooltip.ClearToolTip()
            self.tooltip.AddItemData(data["vnum"], data["sockets"], data["attrs"])
            try:
                stats = self.__GetMarketPriceStats(data)
            except:
                stats = None
            self.tooltip.AppendSpace(7)
            if stats:
                total, samples, lowest, highest = stats
                average = int(total / samples + 0.5)
                self.tooltip.AppendTextLine("Srednia: %s / szt." % localeInfo.NumberToMoneyString(average), COLOR_GOLD)
                self.tooltip.AppendTextLine("Najtansze oferty: %d / %d" % (samples, PRICE_SAMPLE), COLOR_DIM)
                if self.isLoading:
                    self.tooltip.AppendTextLine("Wycena czesciowa - trwa wczytywanie", COLOR_DIM)
                elif samples < 3:
                    self.tooltip.AppendTextLine("Mala liczba ofert - wycena orientacyjna", COLOR_DIM)
            else:
                self.tooltip.AppendTextLine("Brak danych do wyceny", COLOR_DIM)
            self.tooltip.AppendTextLine("Probka: wczytane oferty (filtry, limit %d)" % MAX_LISTINGS, COLOR_DIM)
            self.tooltip.ShowToolTip()

    def HideItemToolTip(self):
        if self.tooltip:
            self.tooltip.HideToolTip()

    # ---- otwieranie / zamykanie ---------------------------------------------------------
    def Open(self):
        ikashop.SetSearchShopBoard(self)
        self.Show()
        self.SetTop()
        self.SetCenterPosition()
        self.Refresh()

    def Close(self):
        if self.multiBuy is not None:
            self.__StopMultiBuy("Okno zamkniete - zakupy przerwane.", False)
        if self.questionDialog:
            self.questionDialog.Close()
            self.questionDialog = None
        if self.quantityDialog:
            self.quantityDialog.Close()
        if self.bonusFilterDialog:
            self.bonusFilterDialog.Close()
        self.buyData = None
        self.Hide()
        self.__HideSuggestions()
        self.HideItemToolTip()

    def OnPressEscapeKey(self):
        self.Close()
        return True

    # ---- zakup -----------------------------------------------------------------------------
    def AskBuy(self, data):
        if self.multiBuy is not None:
            chat.AppendChat(chat.CHAT_TYPE_INFO, "Dom Towarowy: trwa kupowanie zaznaczonych ofert.")
            return
        if self.questionDialog:
            self.questionDialog.Close()
        if data["count"] > 1:
            self.quantityDialog.Open(data)
            return
        dialog = FleaMarketConfirmDialog()
        dialog.SetText("Kupic %s za %s?" % (self.GetItemName(data), self.FormatPrice(data)))
        dialog.acceptButton.SAFE_SetEvent(self.__AcceptBuy)
        dialog.SetDefaultCancelEvent()
        dialog.Open()
        self.questionDialog = dialog
        self.buyData = data

    def __AcceptBuy(self):
        if not self.buyData:
            return
        data = self.buyData
        self.buyData = None
        self.SuppressPurchaseEnter()
        if self.questionDialog:
            self.questionDialog.Close()
            self.questionDialog = None
        self.SendStackPurchase(data, 1)

    def SendStackPurchase(self, data, quantity):
        # Ilosc idzie komenda serwera, bo natywny pakiet nie ma pola ilosci.
        # Serwer jeszcze raz sprawdza mape, zapas, cene i platnosc.
        quantity = max(1, min(int(quantity), data["count"]))
        seenPrice = self.__GetTotalPrice(data)
        net.SendChatPacket("/flea_buy %d %d %d %d" % (data["owner"], data["id"], quantity, seenPrice))

    def UpdateStackOffer(self, ownerID, itemID, remainingCount, remainingYang, remainingCheque):
        for data in self.allItems:
            if data["owner"] == ownerID and data["id"] == itemID:
                data["count"] = remainingCount
                data["price"] = remainingYang
                data["cheque"] = remainingCheque
                break
        self.ApplyFilters(resetPage=False)

    def DeleteSearchResultItem(self, itemID):
        state = self.multiBuy
        if state is not None and state["current"] is not None and state["current"]["id"] == itemID:
            # The server's answer to the run's purchase: the offer is off the
            # list. The Yang went down first (the lock takes it), so a lower
            # purse is a purchase and an unchanged one somebody else's.
            offer = state["current"]
            state["current"] = None
            state["next"] = app.GetTime() + MULTI_BUY_GAP
            if player.GetElk() < state["gold"] or (offer["price"] == 0 and offer.get("cheque", 0)):
                state["bought"] += 1
                state["yang"] += offer["price"]
                state["cheque"] += offer.get("cheque", 0)
            else:
                state["gone"] += 1
        for key in [key for key in self.selected if key[1] == itemID]:
            del self.selected[key]
        self.allItems = [data for data in self.allItems if data["id"] != itemID]
        self.ApplyFilters(resetPage=False)

    # ---- kup wiele ---------------------------------------------------------------------
    def IsSelected(self, data):
        return (data["owner"], data["id"]) in self.selected

    def ToggleSelected(self, data):
        if self.multiBuy is not None:
            return
        key = (data["owner"], data["id"])
        if key in self.selected:
            del self.selected[key]
        else:
            self.selected[key] = data
        self.__RefreshSelection()

    def __PageOffers(self):
        return [row.data for row in self.rows if row.data]

    def SelectPage(self):
        # The header's box: the whole page ticked, or - all of it ticked
        # already - the whole page cleared.
        if self.multiBuy is not None:
            return
        offers = self.__PageOffers()
        if offers and all(self.IsSelected(data) for data in offers):
            for data in offers:
                self.selected.pop((data["owner"], data["id"]), None)
        else:
            for data in offers:
                self.selected[(data["owner"], data["id"])] = data
        self.__RefreshSelection()

    def ClearSelection(self):
        if self.multiBuy is not None:
            return
        self.selected = {}
        self.__RefreshSelection()

    def __RefreshSelection(self):
        for row in self.rows:
            row.RefreshCheck()
        offers = self.__PageOffers()
        self.pageCheck.SetChecked(bool(offers) and all(self.IsSelected(data) for data in offers))
        if self.multiBuy is not None:
            state = self.multiBuy
            left = len(state["queue"]) + (1 if state["current"] is not None else 0)
            self.buyAllButton.SetText("Przerwij (%d)" % left)
        else:
            self.buyAllButton.SetText("Kup wszystko (%d)" % len(self.selected))

    def __SelectedInOrder(self):
        # As the list shows them, then the ticked ones the filters now hide.
        offers = []
        seen = set()
        for data in self.items + self.allItems + self.selected.values():
            key = (data["owner"], data["id"])
            if key in self.selected and key not in seen:
                seen.add(key)
                offers.append(self.selected[key])
        return offers

    def AskBuySelected(self):
        if self.multiBuy is not None:
            self.__StopMultiBuy("Przerwano.")
            return
        offers = self.__SelectedInOrder()
        if not offers:
            chat.AppendChat(chat.CHAT_TYPE_INFO, "Dom Towarowy: zaznacz oferty (kwadrat przy ofercie), a potem Kup wszystko.")
            return
        if self.questionDialog:
            self.questionDialog.Close()
        yang = sum([data["price"] for data in offers])
        cheque = sum([data.get("cheque", 0) for data in offers])
        dialog = FleaMarketConfirmDialog2()
        dialog.SetText1("Kupic %d ofert za lacznie %s?" % (len(offers), self.FormatPrice({"price": yang, "cheque": cheque})))
        if yang > player.GetElk():
            dialog.SetText2("Masz za malo Yang na wszystkie - zakupy stana, gdy zabraknie.")
        else:
            dialog.SetText2("Kupuja sie po kolei; blad przerywa zakupy.")
        dialog.acceptButton.SAFE_SetEvent(self.__AcceptBuySelected)
        dialog.SetDefaultCancelEvent()
        dialog.Open()
        self.questionDialog = dialog

    def __AcceptBuySelected(self):
        self.SuppressPurchaseEnter()
        if self.questionDialog:
            self.questionDialog.Close()
            self.questionDialog = None
        if self.multiBuy is not None:
            return
        offers = self.__SelectedInOrder()
        if not offers:
            return
        self.multiBuy = {"queue": offers, "total": len(offers), "current": None, "deadline": 0.0,
                         "next": 0.0, "gold": 0, "bought": 0, "gone": 0, "yang": 0, "cheque": 0}
        self.__RefreshSelection()

    def __UpdateMultiBuy(self, now):
        state = self.multiBuy
        current = state["current"]
        if current is not None:
            if now >= state["deadline"]:
                self.__StopMultiBuy("Serwer nie potwierdzil zakupu: %s - powod jest na czacie "
                                    "(brak miejsca, zmieniona cena, sklep w edycji)." % self.GetItemName(current))
            return
        if now < state["next"]:
            return
        while state["queue"]:
            data = state["queue"].pop(0)
            if (data["owner"], data["id"]) not in self.selected:
                # Gone from the list since it was ticked: somebody bought it.
                state["gone"] += 1
                continue
            if data["price"] > player.GetElk():
                self.__StopMultiBuy("Brakuje Yang na: %s (%s)." % (self.GetItemName(data), self.FormatPrice(data)))
                return
            if data.get("cheque", 0) and data["cheque"] > player.GetCheque():
                self.__StopMultiBuy("Brakuje Won na: %s (%s)." % (self.GetItemName(data), self.FormatPrice(data)))
                return
            state["current"] = data
            state["gold"] = player.GetElk()
            state["deadline"] = now + MULTI_BUY_TIMEOUT
            # The whole listing, at the price seen - /flea_buy as the single purchase sends it.
            net.SendChatPacket("/flea_buy %d %d %d %d" % (data["owner"], data["id"], max(1, data["count"]),
                                                          self.__GetTotalPrice(data)))
            self.__RefreshSelection()
            return
        self.__StopMultiBuy(None)

    def __StopMultiBuy(self, reason, popup=True):
        state = self.multiBuy
        self.multiBuy = None
        if state is None:
            return
        lines = ["Kupiono %d z %d ofert za %s." % (state["bought"], state["total"],
                                                   self.FormatPrice({"price": state["yang"], "cheque": state["cheque"]}))]
        if state["gone"]:
            lines.append("Juz sprzedane (pominiete): %d." % state["gone"])
        if reason:
            lines.append(reason)
            if state["current"] is not None:
                lines.append("Ostatni zakup moze sie jeszcze dokonczyc.")
        for line in lines:
            chat.AppendChat(chat.CHAT_TYPE_INFO, "Dom Towarowy: %s" % line)
        self.__RefreshSelection()
        if not popup or not self.IsShow():
            return
        if self.popupDialog:
            self.popupDialog.Hide()
        popup = uiCommon.PopupDialog()
        popup.SetText("[ENTER]".join(lines))
        popup.Open()
        self.popupDialog = popup

    # Serwer wprowadzil juz gracza do sklepu, gdy ten callback dochodzi. Nigdy go nie porzucamy:
    # niewidoczny sklep gosci blokuje NPC, zmiane kanalu i wylogowanie do ponownego polaczenia.
    def OpenShopGuest(self, data):
        self.Close()
        try:
            if self.guestBoard:
                self.guestBoard.OpenShopGuest(data)
                return
        except:
            pass
        ikashop.SendCloseShopGuestBoard()

    def ShopExpiredGuesting(self, id):
        if self.guestBoard:
            self.guestBoard.ShopExpiredGuesting(id)

    def ShopGuestRemoveItem(self, itemID):
        if self.guestBoard:
            self.guestBoard.ShopGuestRemoveItem(itemID)

    def ShopGuestEditItem(self, itemID, price):
        if self.guestBoard:
            self.guestBoard.ShopGuestEditItem(itemID, price)

    # Callback z natywnego modulu ikashop.
    def SetSearchResultItems(self, items):
        if self.isLoading:
            if items:
                fresh = []
                for data in items:
                    if data["is_auction"] or data["id"] in self.seenIds:
                        continue
                    self.seenIds.add(data["id"])
                    fresh.append(data)
                if not self.pendingItems:
                    # pierwsza paczka nowego zapytania zastepuje stare oferty
                    self.allItems = []
                self.pendingItems.extend(fresh)
                self.allItems = self.pendingItems
                self.itemNames = None
                if len(self.pendingItems) == len(fresh):
                    self.ApplyFilters(resetPage=False)
                    self.nextApply = app.GetTime() + APPLY_INTERVAL
                else:
                    self.dirty = True
                    self.__UpdateStatus()
                return
            self.isLoading = False
            self.allItems = self.pendingItems
            self.pendingItems = []
        else:
            self.allItems = [data for data in items if not data["is_auction"]]
        self.dirty = False
        self.ApplyFilters(resetPage=False)
