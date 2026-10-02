# FLEA_UI_V2 FLEA_UI_V4 FLEA_UI_V5 FLEA_UI_V6 - nowe okno "Dom Towarowy" (kategorie po lewej, filtry, wyszukiwanie po stronie serwera)
import re
import ui
import ikashop
import item
import player
import localeinfo_point
import skill
import net
import app
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
SEARCH_DELAY = 0.7
BOOK_VNUMS = (50300,)
# Umiejetnosci, dla ktorych istnieja ksiegi (klient zna ich nazwy). skill.GetSkillName dla nieznanego numeru
# zapisuje blad w interpreterze i wywala pozniejszy, niezwiazany kod - wolno pytac tylko o te numery.
SKILL_IDS = tuple(range(1, 6) + range(16, 21) + range(31, 36) + range(46, 51) + range(61, 67) + range(76, 82) + range(91, 97) + range(106, 112))
APPLY_INTERVAL = 0.25
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
    71026: "Rudy i przetopy",      # Magiczna Ruda Miedzi (wariant questowy)
    72308: "Rudy i przetopy",      # Magiczna Ruda Miedzi (wariant questowy)
}
# (od, do wlacznie, kategoria) - prawdziwe rudy/kamienie siedza pod ITEM_TYPE_SPECIAL, ktory
# jest ogolnym koszem (jedzenie dla konia, bilety, obraczki...), wiec tylko ten ciagly zakres
# vnumow trafia do "Rudy i przetopy".
CATEGORY_VNUM_RANGE_OVERRIDES = (
    (50601, 50622, "Rudy i przetopy"),  # Ruda Miedzi .. Ruda Szafiru, Kawalek Bursztynu/Perly, Dusza Rudy Krysztalu, Bursztyn, Diamentowy Kamien, Skamienialy Pien
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
# wyselekcjonowany zestaw bonusow do filtra "Filtry" (do 5 naraz, kazdy z
# progiem minimalnym). Caly filtr dziala po stronie klienta na juz pobranych
# ofertach (kazda oferta ma "attrs" - krotke (bType, sValue) - wyslana przez
# natywny modul ikashop, wiec serwer nie musi nic wiedziec o tym filtrze).
# Pelna liste wszystkich mozliwych bonusow ma AFFECT_DICT w localeinfo_point.py
# - tu tylko te, ktore graczy faktycznie interesuja przy zakupie ekwipunku.
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

# localeinfo_point.ATTR_MAX_VALUES (silnikowa tabela) nie ma wpisu dla kazdego
# z powyzszych - tu nadpisania/dopelnienia dla tych, ktore gracze podali z
# wlasnego doswiadczenia (np. Srednie obrazenia losuja sie do +60%).
BONUS_FILTER_MAX_OVERRIDES = {
    "POINT_NORMAL_HIT_DAMAGE_BONUS": 60,
}


def BuildBonusFilterOptions():
    options = []
    for constName, label in BONUS_FILTER_DEFS:
        value = getattr(player, constName, None)
        if value is None:
            continue
        maxValue = BONUS_FILTER_MAX_OVERRIDES.get(constName) or localeinfo_point.ATTR_MAX_VALUES.get(value)
        if maxValue:
            label = "%s (maks. %d)" % (label, maxValue)
        options.append((value, label))
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
    WIDTH = 430
    LEFT = 20
    PICK_WIDTH = 240
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

        self.__top = 64
        cardHeight = self.MAX_ROWS * self.ROW_HEIGHT + 12
        # CUSTOM_FLEA_BONUS_FILTER_V1: dialog musi byc na tyle wysoki, zeby
        # w pelni zmiescic rozwinieta liste wyboru (patrz OpenPicker) - inaczej
        # dolna czesc listy wystaje poza wlasne okno, a kliki tam trafiaja w
        # przedmioty pod spodem zamiast w liste (wlasny pick-area okna konczy
        # sie na jego deklarowanym rozmiarze, mimo ze tekst renderuje sie dalej).
        pickerHeight = len(self.options) * 17 + 6
        bottom = self.__top + max(cardHeight + 50, pickerHeight + 20)
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

        self.applyButton = self.__MakeButton(self.WIDTH / 2 - 110, y + 14, "Zastosuj", self.Apply)
        self.clearButton = self.__MakeButton(self.WIDTH / 2 + 10, y + 14, "Wyczysc", self.ClearAll)

        # lista wyboru bonusu - jedna wspolna dla wszystkich wierszy
        self.pickerBackground = ui.SlotBar()
        self.pickerBackground.SetParent(self)
        self.pickerBackground.SetPosition(self.LEFT - 4, self.__top)
        self.pickerBackground.SetSize(self.PICK_WIDTH + 8, 1)
        self.pickerBackground.AddFlag("not_pick")
        self.pickerBackground.Hide()
        self.pickerList = ui.ListBox()
        self.pickerList.SetParent(self.pickerBackground)
        self.pickerList.SetPosition(4, 3)
        self.pickerList.SetSize(self.PICK_WIDTH, 1)
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

        return {"optionIndex": 0, "button": pickButton, "edit": valueEdit}

    def OpenPicker(self, rowIndex):
        self.activeRow = rowIndex
        lineHeight = 17
        height = len(self.options) * lineHeight + 6
        self.pickerList.SetSize(self.PICK_WIDTH, height - 6)
        self.pickerList.LocateItem()
        self.pickerBackground.SetSize(self.PICK_WIDTH + 8, height)
        self.pickerBackground.Show()
        self.pickerList.Show()
        self.pickerBackground.SetTop()
        self.pickerList.SetTop()

    def OnPickerSelect(self, optionIndex, name):
        if self.activeRow >= 0:
            row = self.rows[self.activeRow]
            row["optionIndex"] = optionIndex
            row["button"].SetText(self.options[optionIndex][1])
        self.pickerList.Hide()
        self.pickerBackground.Hide()
        self.activeRow = -1

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
            filters.append((attrType, minValue))
        self.market.SetBonusFilters(filters)
        self.Close()

    def ClearAll(self):
        for row in self.rows:
            row["optionIndex"] = 0
            row["button"].SetText(self.NONE_LABEL)
            row["edit"].SetText("")
        self.pickerList.Hide()
        self.pickerBackground.Hide()
        self.activeRow = -1
        self.market.SetBonusFilters([])

    def Open(self):
        self.Show()
        self.SetTop()
        self.SetCenterPosition()

    def Close(self):
        self.pickerList.Hide()
        self.pickerBackground.Hide()
        self.activeRow = -1
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
        self.searchEdit = self.__MakeEdit(MAIN_X + 46, SIDEBAR_Y, 330, 32)
        self.searchEdit.OnIMEUpdate = ui.__mem_func__(self.__OnSearchTextChanged)
        self.searchEdit.SAFE_SetReturnEvent(self.Search)
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
        self.priceMinEdit = self.__MakeEdit(MAIN_X + 56, SIDEBAR_Y + 28, 110, 12, True)
        label = self.__MakeText(MAIN_X + 176, SIDEBAR_Y + 31, "do:")
        label.SetPackedFontColor(COLOR_HEAD)
        self.priceMaxEdit = self.__MakeEdit(MAIN_X + 198, SIDEBAR_Y + 28, 110, 12, True)
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

    def __MakeEdit(self, x, y, width, maxLength, numeric=False):
        slot = ui.SlotBar()
        slot.SetParent(self)
        slot.SetPosition(x, y)
        slot.SetSize(width, 20)
        slot.AddFlag("not_pick")
        slot.Show()
        self.keepers.append(slot)

        edit = ui.EditLine()
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
                    skillName = skill.GetSkillName(skillVnum) if skillVnum else ""
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
        self.itemNames = names
        return names

    def __HideSuggestions(self):
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

    def __OnSelectSuggestion(self, index, name):
        if index < 0 or index >= len(self.suggestionNames):
            return
        self.searchEdit.SetText(self.suggestionNames[index])
        self.__HideSuggestions()
        self.Refresh()
        self.searchEdit.SetFocus()

    def __OnSearchTextChanged(self):
        ui.EditLine.OnIMEUpdate(self.searchEdit)
        self.searchAt = app.GetTime() + SEARCH_DELAY
        self.__UpdateSuggestions()
        self.ApplyFilters()

    # ---- zapytanie do serwera ---------------------------------------------------------
    def __ParsePrice(self, edit):
        text = edit.GetText().strip().replace(" ", "")
        if not text:
            return 0
        try:
            return max(0, int(text))
        except:
            return 0

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

    def Search(self):
        self.__HideSuggestions()
        self.__Request()

    def OnUpdate(self):
        now = app.GetTime()
        if self.searchAt is not None and now >= self.searchAt:
            self.searchAt = None
            if self.IsShow() and self.__BuildRequest() != self.lastRequest:
                self.__Request()
        if self.dirty and now >= self.nextApply:
            self.dirty = False
            self.nextApply = now + APPLY_INTERVAL
            self.ApplyFilters()
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

    def SetBonusFilters(self, filters):
        self.bonusFilters = filters
        count = len(filters)
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

    def ApplyFilters(self):
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
                if query not in PolishLower(name):
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
        self.page = 0
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

    def PreviousPage(self):
        if self.page > 0:
            self.page -= 1
            self.__RefreshRows()

    def NextPage(self):
        if (self.page + 1) * ROWS_PER_PAGE < len(self.items):
            self.page += 1
            self.__RefreshRows()

    def OnMouseWheel(self, length):
        if length > 0:
            self.PreviousPage()
        elif length < 0:
            self.NextPage()
        return True

    def ShowItemToolTip(self, data):
        if self.tooltip:
            self.tooltip.ClearToolTip()
            self.tooltip.AddItemData(data["vnum"], data["sockets"], data["attrs"])
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
        item.SelectItem(data["vnum"])
        dialog = uiCommon.QuestionDialog()
        dialog.SetText("Kupic %s za %s?" % (item.GetItemName(), self.FormatPrice(data)))
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
        self.ApplyFilters()

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
        self.ApplyFilters()

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
        dialog = uiCommon.QuestionDialog2()
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
                    self.ApplyFilters()
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
        self.ApplyFilters()
