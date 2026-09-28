# FLEA_UI_V2 FLEA_UI_V4 - nowe okno Flea Market (kategorie po lewej, filtry, wyszukiwanie po stronie serwera)
import ui
import ikashop
import item
import skill
import net
import app
import localeInfo
import uiCommon
from _weakref import proxy

YANG_PER_CHEQUE = 100000000

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

# (nazwa, wciecie, stala/e typu z modulu item (string albo krotka stringow) albo "OTHER", stala podtypu)
CATEGORY_DEFS = (
    ("Wszystko", 0, None, None),
    ("Bron", 0, "ITEM_TYPE_WEAPON", None),
    ("Miecze jednoreczne", 1, "ITEM_TYPE_WEAPON", "WEAPON_SWORD"),
    ("Miecze dwureczne", 1, "ITEM_TYPE_WEAPON", "WEAPON_TWO_HANDED"),
    ("Luki", 1, "ITEM_TYPE_WEAPON", "WEAPON_BOW"),
    ("Sztylety", 1, "ITEM_TYPE_WEAPON", "WEAPON_DAGGER"),
    ("Dzwony", 1, "ITEM_TYPE_WEAPON", "WEAPON_BELL"),
    ("Wachlarze", 1, "ITEM_TYPE_WEAPON", "WEAPON_FAN"),
    ("Zbroje", 0, "ITEM_TYPE_ARMOR", "ARMOR_BODY"),
    ("Helmy", 0, "ITEM_TYPE_ARMOR", "ARMOR_HEAD"),
    ("Tarcze", 0, "ITEM_TYPE_ARMOR", "ARMOR_SHIELD"),
    ("Buty", 0, "ITEM_TYPE_ARMOR", "ARMOR_FOOTS"),
    ("Bransolety", 0, "ITEM_TYPE_ARMOR", "ARMOR_WRIST"),
    ("Naszyjniki", 0, "ITEM_TYPE_ARMOR", "ARMOR_NECK"),
    ("Kolczyki", 0, "ITEM_TYPE_ARMOR", "ARMOR_EAR"),
    ("Ksiegi", 0, "ITEM_TYPE_SKILLBOOK", None),
    ("Ksiegi zapomnienia", 0, "ITEM_TYPE_SKILLFORGET", None),
    ("Kamienie duszy", 0, "ITEM_TYPE_METIN", None),
    ("Rudy i przetopy", 0, "ITEM_TYPE_RESOURCE", None),
    ("Dopalacze", 0, "ITEM_TYPE_POTION", None),
    ("Uzywalne", 0, "ITEM_TYPE_USE", None),
    ("Ulepszacze", 0, "ITEM_TYPE_MATERIAL", None),
    ("Inne", 0, "OTHER", None),
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
)


# Etykiety kategorii, do ktorych vnum-owe wyjatki (patrz CATEGORY_VNUM_OVERRIDES/_RANGE_OVERRIDES
# powyzej) doklejaja przedmioty o INNYM prawdziwym typie silnika. Takie kategorie NIE MOGA
# wyslac do serwera filtra po pojedynczym typie - serwer odrzucilby te przedmioty juz przy
# budowaniu wynikow, zanim dopasowanie po vnum w __MatchesCategory w ogole je zobaczy.
_OVERRIDE_TARGET_LABELS = frozenset(
    list(CATEGORY_VNUM_OVERRIDES.values()) + [label for _, _, label in CATEGORY_VNUM_RANGE_OVERRIDES]
)


def BuildCategories():
    categories = []
    for label, indent, typeName, subName in CATEGORY_DEFS:
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
        # "type" to pojedyncza wartosc wysylana do filtra po stronie serwera (-1 = brak
        # filtra typu): dziala tylko dla kategorii z JEDNYM typem silnika, ktora nie przyjmuje
        # zadnych vnum-owych wyjatkow. Kategoria laczaca kilka typow (np. dawniej Ulepszacze)
        # albo bedaca celem override'u wysyla -1 (serwer nie filtruje po typie) i polega na
        # dopasowaniu "types"/override wykonanym w calosci po stronie klienta w __MatchesCategory.
        if label in _OVERRIDE_TARGET_LABELS:
            serverType = -1
        else:
            serverType = types[0] if len(types) == 1 else -1
        categories.append({"label": label, "indent": indent, "types": types, "type": serverType, "sub": subType})
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
        self.text.SetText(label)
        self.text.AddFlag("not_pick")
        self.text.Show()
        self.indent = indent
        self.__Refresh()

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
        self.frame = self.__MakeBar(6, 4, self.ICON_BOX, self.ICON_BOX, 0xAA000000)

        self.icon = ui.ExpandedImageBox()
        self.icon.SetParent(self)
        self.icon.SetPosition(6, 4)
        self.icon.AddFlag("not_pick")
        self.icon.Show()

        self.countText = self.__MakeText(6, 4)
        self.countText.SetOutline()
        self.countText.SetPackedFontColor(COLOR_TEXT)

        self.nameText = self.__MakeText(60, 6)
        self.nameText.SetPackedFontColor(COLOR_TEXT)
        self.sellerText = self.__MakeText(60, 21)
        self.sellerText.SetPackedFontColor(COLOR_DIM)
        self.bonusText = self.__MakeText(60, 35)
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
        self.icon.SetPosition(6 + int((self.ICON_BOX - width * scale) / 2), 4 + int((self.ICON_BOX - height * scale) / 2))

        count = data["count"]
        if count > 1:
            self.countText.SetText("%d" % count)
            self.countText.SetPosition(6 + self.ICON_BOX - 6 - len(self.countText.GetText()) * 6, 4 + self.ICON_BOX - 14)
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
        self.__RefreshBackground()
        self.Show()

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
        self.sortIndex = 0
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
        self.quantityDialog = offlineshopsearch.FleaMarketQuantityDialog(self)
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
        self.__MakeCard(SIDEBAR_X, SIDEBAR_Y, SIDEBAR_WIDTH, len(self.categories) * CATEGORY_HEIGHT + 8)
        for index, category in enumerate(self.categories):
            button = FleaCategoryButton(self, index, category["label"], category["indent"])
            button.SetParent(self)
            button.SetPosition(SIDEBAR_X + 6, SIDEBAR_Y + 4 + index * CATEGORY_HEIGHT)
            button.Show()
            self.categoryButtons.append(button)
        self.categoryButtons[0].SetSelected(True)

        # pasek wyszukiwania
        label = self.__MakeText(MAIN_X, SIDEBAR_Y + 3, "Nazwa:")
        label.SetPackedFontColor(COLOR_HEAD)
        self.searchEdit = self.__MakeEdit(MAIN_X + 46, SIDEBAR_Y, 330, 32)
        self.searchEdit.OnIMEUpdate = ui.__mem_func__(self.__OnSearchTextChanged)
        self.searchEdit.SAFE_SetReturnEvent(self.Search)
        self.searchButton = self.__MakeButton(MAIN_X + 384, SIDEBAR_Y - 2, 84, "Szukaj", self.Search)
        self.refreshButton = self.__MakeButton(MAIN_X + 474, SIDEBAR_Y - 2, 84, "Odswiez", self.Refresh)
        self.clearButton = self.__MakeButton(MAIN_X + 564, SIDEBAR_Y - 2, 104, "Wyczysc filtry", self.ClearFilters)

        label = self.__MakeText(MAIN_X, SIDEBAR_Y + 31, "Cena od:")
        label.SetPackedFontColor(COLOR_HEAD)
        self.priceMinEdit = self.__MakeEdit(MAIN_X + 56, SIDEBAR_Y + 28, 110, 12, True)
        label = self.__MakeText(MAIN_X + 176, SIDEBAR_Y + 31, "do:")
        label.SetPackedFontColor(COLOR_HEAD)
        self.priceMaxEdit = self.__MakeEdit(MAIN_X + 198, SIDEBAR_Y + 28, 110, 12, True)
        self.sortButton = self.__MakeButton(MAIN_X + 330, SIDEBAR_Y + 26, 150, SORT_MODES[0][1], self.CycleSort)

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

        # naglowek listy
        self.__MakeBar(MAIN_X, ROWS_Y - 20, ROW_WIDTH, 18, 0x33FFFFFF)
        self.__MakeText(MAIN_X + 60, ROWS_Y - 18, "Przedmiot").SetPackedFontColor(COLOR_HEAD)
        self.__MakeText(MAIN_X + 340, ROWS_Y - 18, "Ilosc").SetPackedFontColor(COLOR_HEAD)
        priceHead = self.__MakeText(102, ROWS_Y - 18, "Cena")
        priceHead.SetWindowHorizontalAlignRight()
        priceHead.SetHorizontalAlignRight()
        priceHead.SetPosition(114, ROWS_Y - 18)
        priceHead.SetPackedFontColor(COLOR_HEAD)

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
        self.statusText = self.__MakeText(MAIN_X + 4, footerY + 4, "")
        self.statusText.SetPackedFontColor(COLOR_DIM)
        self.previousButton = self.__MakeButton(MAIN_RIGHT - 190, footerY, 90, "< Poprzednia", self.PreviousPage)
        self.nextButton = self.__MakeButton(MAIN_RIGHT - 94, footerY, 90, "Nastepna >", self.NextPage)
        self.pageText = self.__MakeText(MAIN_RIGHT - 300, footerY + 4, "")
        self.pageText.SetPackedFontColor(COLOR_TEXT)

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
        if self.questionDialog:
            self.questionDialog.Close()
            self.questionDialog = None
        if self.quantityDialog:
            self.quantityDialog.Close()
            self.quantityDialog = None
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
                key = name.lower()
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
        query = self.searchEdit.GetText().strip().lower()
        if len(query) < 2:
            self.__HideSuggestions()
            return
        startsWith = []
        contains = []
        for name in self.__GetItemNames():
            lowerName = name.lower()
            if lowerName.startswith(query):
                startsWith.append(name)
            elif query in lowerName:
                contains.append(name)
        startsWith.sort(key=lambda name: name.lower())
        contains.sort(key=lambda name: name.lower())
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
        key = query.lower()
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
                if key in ("%s - %s" % (name, bookName)).lower():
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
    def __BuildRequest(self):
        category = self.categories[self.category]
        return (category["type"], category["sub"], SORT_MODES[self.sortIndex][2],
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

    # ---- kategorie, sortowanie, filtry ------------------------------------------------
    def SetCategory(self, index):
        if index == self.category:
            return
        self.categoryButtons[self.category].SetSelected(False)
        self.category = index
        self.categoryButtons[index].SetSelected(True)
        self.ApplyFilters()
        self.__Request()

    def CycleSort(self):
        self.sortIndex = (self.sortIndex + 1) % len(SORT_MODES)
        self.sortButton.SetText(SORT_MODES[self.sortIndex][1])
        self.ApplyFilters()
        if self.__BuildRequest() != self.lastRequest:
            self.__Request()

    def ClearFilters(self):
        self.searchEdit.SetText("")
        self.priceMinEdit.SetText("")
        self.priceMaxEdit.SetText("")
        self.categoryButtons[self.category].SetSelected(False)
        self.category = 0
        self.categoryButtons[0].SetSelected(True)
        self.sortIndex = 0
        self.sortButton.SetText(SORT_MODES[0][1])
        self.__HideSuggestions()
        self.ApplyFilters()
        self.__Request()

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
        return True

    def ApplyFilters(self):
        query = self.searchEdit.GetText().strip().lower()
        minimum = self.__ParsePrice(self.priceMinEdit)
        maximum = self.__ParsePrice(self.priceMaxEdit)
        items = []
        for data in self.allItems:
            price = self.__GetTotalPrice(data)
            if minimum and price < minimum:
                continue
            if maximum and price > maximum:
                continue
            if query and query not in self.GetItemName(data).lower():
                continue
            if not self.__MatchesCategory(data):
                continue
            items.append(data)

        mode = SORT_MODES[self.sortIndex][0]
        if mode == "price_desc":
            items.sort(key=self.__GetTotalPrice, reverse=True)
        elif mode == "unit_asc":
            items.sort(key=self.__GetUnitPrice)
        elif mode == "name":
            items.sort(key=lambda data: self.GetItemName(data).lower())
        elif mode == "seller":
            items.sort(key=lambda data: data["seller_name"].lower())
        else:
            items.sort(key=self.__GetTotalPrice)

        self.items = items
        self.page = 0
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
        self.pageText.SetText("Strona %d / %d" % (self.page + 1 if pageCount else 0, pageCount))
        if self.page > 0:
            self.previousButton.Enable()
        else:
            self.previousButton.Disable()
        if (self.page + 1) < pageCount:
            self.nextButton.Enable()
        else:
            self.nextButton.Disable()
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
        if self.questionDialog:
            self.questionDialog.Close()
            self.questionDialog = None
        if self.quantityDialog:
            self.quantityDialog.Close()
        self.buyData = None
        self.Hide()
        self.__HideSuggestions()
        self.HideItemToolTip()

    def OnPressEscapeKey(self):
        self.Close()
        return True

    # ---- zakup -----------------------------------------------------------------------------
    def AskBuy(self, data):
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
        self.allItems = [data for data in self.allItems if data["id"] != itemID]
        self.ApplyFilters()

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
