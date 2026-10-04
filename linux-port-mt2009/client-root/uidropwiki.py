# Drop wiki - which monsters drop an item, and what a monster drops (the
# owner, 3 October: "Player should be able to type in an item name and see
# which monsters drop that item, at what rarity and amount ... as well as be
# able to type in a monster name to see what they drop"; chests stay with the
# chest preview, uichestpreview.py).
#
# Everything comes from the server (do_drop_wiki in cmd_general.cpp,
# ITEM_MANAGER::GetDropWikiRows in item_manager.cpp, MT2009_PLUS_DROP_WIKI_V1),
# so what the window shows is what the running server's tables give:
#
#     /drop_wiki find <i|m> <text>    ->  DropWiki find_begin <i|m>
#                                         DropWiki match <vnum> <level>
#                                         DropWiki find_end <i|m> <total>
#     /drop_wiki show <i|m> <vnum>    ->  DropWiki show_begin <i|m> <vnum>
#                                         DropWiki row <vnum> <countMin> <countMax> <chance> <level>
#                                         DropWiki show_end <i|m> <vnum>
#     /drop_wiki                      ->  DropWiki open
#                                         DropWiki error <short|busy|bad>
#
# chance is per kill in hundred-millionths (100000000 = 100%), for a player at
# the monster's own level with no premium, gloves or event; level is the
# monster's (0 on an item's row). The search is the server's: case and Polish
# letters do not matter, a number is also a vnum.
#
# Opened with its key (keybind.py "drop_wiki", "/" by default) or /drop_wiki.
# A click on a row goes the other way: an item's row shows who drops it, a
# monster's row what it drops.
#
# The texts are CP1250 escapes so the file stays ASCII. Python 2.7 as the
# client has it; the rows hold the window through a weak proxy and every event
# goes through ui.__mem_func__, so nothing keeps a cycle alive.

import item
import net
import nonplayer
import ui
import uiToolTip

import uichestpreview

try:
	import dropwikiportraits
except Exception:
	dropwikiportraits = None

# ---------------------------------------------------------------------------
# The columns. False hides one: the window narrows itself and nothing else
# needs changing (the server still sends the numbers).
SHOW_COUNT = True	# "Ilosc" - how many drop at once
SHOW_CHANCE = True	# "Szansa" - the chance per kill
# ---------------------------------------------------------------------------

MARGIN = 12
ICON_W = 34
NAME_W = 230
COUNT_W = 60
CHANCE_W = 80
SCROLL_W = 17

MATCH_ROWS = 5
MATCH_ROW_H = 17
ROWS = 9
ROW_H = 34

TEXT_HINT = 'Wpisz nazw\xea przedmiotu lub potwora i naci\x9cnij Enter.'
TEXT_SEARCHING = 'Szukam...'
TEXT_NONE = 'Nic nie znaleziono.'
TEXT_SHORT = 'Wpisz co najmniej 2 litery.'
TEXT_BUSY = 'Chwil\xea... spr\xf3buj jeszcze raz.'
TEXT_BAD = 'B\xb3\xb9d zapytania.'
TEXT_MANY = 'Znaleziono %d - pokazano %d, zaw\xea\x9f wyszukiwanie.'
TEXT_FOUND = 'Znaleziono: %d - wybierz z listy.'
TEXT_LOADING = 'Pobieram...'
TEXT_NO_DROP = 'Nic nie wypada z tabel dropu.'
TEXT_NO_MOB = '\xa3aden potw\xf3r tego nie dropi.'
TEXT_FOOTER = 'Szansa od jednego zab\xf3jstwa, dla gracza na poziomie potwora (bez premium i event\xf3w).'

COLOR_HEADER = 0xffc8aa80
# The Przedmiot / Potwor tabs: the chosen one in the pressed (darker) look.
TAB_UP = 'd:/ymir work/ui/public/middle_button_01.sub'
TAB_OVER = 'd:/ymir work/ui/public/middle_button_02.sub'
TAB_CHOSEN = 'd:/ymir work/ui/public/middle_button_03.sub'
COLOR_TAB = 0xffffffff
COLOR_TAB_CHOSEN = 0xfff0c860
# The rows: a darker shade of the board under each, a line above each, and the
# highlight under the mouse (ARGB - the first two digits are the opacity).
ROW_SHADE = 0x90000000
ROW_SEPARATOR = 0xff6b5a46
ROW_HOVER = 0x20ffffff
COLOR_DIM = 0xffa0a0a0
# Chance colours: common, uncommon, rare, very rare.
COLOR_CHANCES = ((10.0, 0xffffffff), (1.0, 0xff9be36b), (0.1, 0xff6bb6ff), (0.0, 0xffffb050))

_data = {'window': None}


# CP1250 Polish letters to plain ones (the server folds as well, CP1250 or UTF-8).
_FOLD = {}
for _src, _dst in zip('\xb9\xe6\xea\xb3\xf1\xf3\x9c\x9f\xbf\xa5\xc6\xca\xa3\xd1\xd3\x8c\x8f\xaf',
		'acelnoszzacelnoszz'):
	_FOLD[_src] = _dst


def Fold(text):
	"""Lower case, Polish letters to plain ones."""
	return ''.join([_FOLD.get(ch, ch) for ch in text]).lower()


def ItemName(vnum):
	try:
		item.SelectItem(vnum)
		return item.GetItemName()
	except Exception:
		return str(vnum)


def ItemIcon(vnum):
	try:
		item.SelectItem(vnum)
		return item.GetIconImageFileName()
	except Exception:
		return ''


def MobName(vnum):
	try:
		name = nonplayer.GetMonsterName(vnum)
		if name:
			return name
	except Exception:
		pass
	return str(vnum)


def MobIcon(vnum):
	"""The monster's portrait (dropwikiportraits.py, icon/monster/), or the
	boss / Metin / monster icon when it has none."""
	if not dropwikiportraits:
		return ''
	try:
		return dropwikiportraits.Icon(vnum)
	except Exception:
		return ''


def FormatChance(value):
	pct = value / 1000000.0
	if pct >= 100.0:
		return '100%'
	if pct >= 1.0:
		text = '%.2f' % pct
	else:
		# three significant digits under one percent
		digits = 2
		limit = 0.1
		while pct < limit and digits < 7:
			digits += 1
			limit /= 10.0
		text = '%.*f' % (digits, pct)
	if '.' in text:
		text = text.rstrip('0').rstrip('.')
	return text + '%'


def ChanceColor(value):
	pct = value / 1000000.0
	for limit, color in COLOR_CHANCES:
		if pct >= limit:
			return color
	return COLOR_CHANCES[-1][1]


def FormatCount(countMin, countMax):
	if countMin == countMax:
		return str(countMin)
	return '%d-%d' % (countMin, countMax)


def _Columns():
	"""x of each column inside a row, and the row's width."""
	x = ICON_W + NAME_W
	countX = chanceX = None
	if SHOW_COUNT:
		countX = x
		x += COUNT_W
	if SHOW_CHANCE:
		chanceX = x
		x += CHANCE_W
	return countX, chanceX, x


class DropRow(ui.Window):
	"""One line of the list: an item (icon, name) or a monster (name, level)."""

	def __init__(self, owner, index):
		ui.Window.__init__(self)
		self.owner = uichestpreview.WindowProxy(owner)
		self.index = index
		countX, chanceX, width = _Columns()
		self.SetSize(width, ROW_H)

		# a darker strip under every row, a line above it, and a lighter one
		# over it while the mouse is on the row
		self.shade = ui.Bar()
		self.shade.SetParent(self)
		self.shade.SetPosition(0, 1)
		self.shade.SetSize(width, ROW_H - 2)
		self.shade.SetColor(ROW_SHADE)
		self.shade.AddFlag('not_pick')
		self.shade.Show()

		self.separator = ui.Line()
		self.separator.SetParent(self)
		self.separator.SetPosition(0, 0)
		self.separator.SetSize(width, 0)
		self.separator.SetColor(ROW_SEPARATOR)
		self.separator.AddFlag('not_pick')
		self.separator.Show()

		self.back = ui.Bar()
		self.back.SetParent(self)
		self.back.SetPosition(0, 1)
		self.back.SetSize(width, ROW_H - 2)
		self.back.SetColor(ROW_HOVER)
		self.back.AddFlag('not_pick')
		self.back.Hide()

		self.icon = ui.ExpandedImageBox()
		self.icon.SetParent(self)
		self.icon.AddFlag('not_pick')
		self.icon.Hide()

		self.nameText = self.__Text(ICON_W, False)
		self.countText = self.__Text(countX + COUNT_W // 2, True) if countX is not None else None
		self.chanceText = self.__Text(chanceX + CHANCE_W // 2, True) if chanceX is not None else None

	def __Text(self, x, center):
		text = ui.TextLine()
		text.SetParent(self)
		text.SetPosition(x, ROW_H // 2 - 7)
		if center:
			text.SetHorizontalAlignCenter()
		text.AddFlag('not_pick')
		text.Show()
		return text

	def SetRow(self, isItem, vnum, countMin, countMax, chance, level):
		if isItem:
			self.nameText.SetText(ItemName(vnum))
			self.__SetIcon(ItemIcon(vnum))
		else:
			self.__SetIcon(MobIcon(vnum))
			if level > 0:
				self.nameText.SetText('%s (Lv %d)' % (MobName(vnum), level))
			else:
				self.nameText.SetText(MobName(vnum))
		if self.countText:
			self.countText.SetText(FormatCount(countMin, countMax))
		if self.chanceText:
			self.chanceText.SetText(FormatChance(chance))
			self.chanceText.SetPackedFontColor(ChanceColor(chance))
		self.Show()

	def __SetIcon(self, path):
		if not path:
			self.icon.Hide()
			return
		try:
			self.icon.LoadImage(path)
			width = max(1, self.icon.GetWidth())
			height = max(1, self.icon.GetHeight())
			# 32x64 and 32x96 icons shrink to the row's height.
			scale = min(1.0, 32.0 / height, 32.0 / width)
			self.icon.SetScale(scale, scale)
			self.icon.SetPosition(int((32 - width * scale) // 2), int((ROW_H - 2 - height * scale) // 2))
			self.icon.Show()
		except Exception:
			self.icon.Hide()

	def OnMouseOverIn(self):
		self.back.Show()
		self.owner.OnRowOver(self.index)

	def OnMouseOverOut(self):
		self.back.Hide()
		self.owner.OnRowOut(self.index)

	def OnMouseLeftButtonUp(self):
		self.owner.OnRowClick(self.index)

	def OnMouseWheel(self, delta):
		self.owner.OnRowsWheel(1 if delta > 0 else -1)
		return True


class MatchListBox(ui.ListBox):
	"""The search results. The stock list draws its highlight from x+2 but its
	text from x 0, so the text is moved under the highlight; and a click on
	the line already chosen opens it again (the stock list ignores it), which
	is the way back after clicking through the rows to another item or
	monster."""
	TEXT_X = 5

	def _LocateItem(self):
		ui.ListBox._LocateItem(self)
		for textLine in self.itemList:
			x, y = textLine.GetLocalPosition()
			textLine.SetPosition(x + self.TEXT_X, y)

	def SelectItem(self, line):
		if not self.keyDict.has_key(line):
			return
		self.selectedLine = line
		self.event(self.keyDict.get(line, 0), self.textDict.get(line, 'None'))


class DropWikiWindow(ui.BoardWithTitleBar):

	def __init__(self):
		ui.BoardWithTitleBar.__init__(self)
		self.AddFlag('movable')
		self.AddFlag('float')
		countX, chanceX, rowWidth = _Columns()
		self.rowWidth = rowWidth
		self.width = MARGIN * 2 + rowWidth + SCROLL_W + 4
		self.mode = 'i'
		self.pendingFind = None
		self.matches = []
		self.matchTotal = 0
		self.showKind = None
		self.showVnum = 0
		self.rows = []
		self.rowBase = 0
		self.matchBase = 0
		self.scrollLock = False
		self.levels = {}
		self.widgets = []

		self.SetTitleName('Drop wiki')
		self.SetCloseEvent(ui.__mem_func__(self.Close))

		y = 34
		self.itemButton = self.__ModeButton('Przedmiot', MARGIN, y, 'i')
		self.mobButton = self.__ModeButton('Potw\xf3r', MARGIN + 64, y, 'm')

		searchX = MARGIN + 132
		searchW = self.width - MARGIN - 61 - 6 - searchX
		slot = ui.SlotBar()
		slot.SetParent(self)
		slot.SetPosition(searchX, y + 1)
		slot.SetSize(searchW, 19)
		slot.Show()
		self.widgets.append(slot)

		self.searchEdit = ui.EditLine()
		self.searchEdit.SetParent(slot)
		self.searchEdit.SetPosition(3, 3)
		self.searchEdit.SetSize(searchW - 6, 16)
		self.searchEdit.SetMax(30)
		try:
			self.searchEdit.SetPlaceholder('Szukaj...')
		except Exception:
			pass
		self.searchEdit.SetReturnEvent(ui.__mem_func__(self.Search))
		self.searchEdit.SetEscapeEvent(ui.__mem_func__(self.Close))
		self.searchEdit.Show()
		self.widgets.append(self.searchEdit)

		self.searchButton = self.__Button('Szukaj', self.width - MARGIN - 61, y, ui.__mem_func__(self.Search))

		# the matches
		y += 28
		listH = MATCH_ROWS * MATCH_ROW_H
		self.matchList = MatchListBox()
		self.matchList.SetParent(self)
		self.matchList.SetPosition(MARGIN, y)
		self.matchList.SetSize(self.width - MARGIN * 2 - SCROLL_W - 4, listH)
		self.matchList.SetTextCenterAlign(False)
		self.matchList.SetEvent(ui.__mem_func__(self.OnMatchSelect))
		self.matchList.SetMouseWheelEvent(self.OnMatchWheel)
		self.matchList.Show()
		self.widgets.append(self.matchList)

		self.matchScroll = ui.ScrollBar()
		self.matchScroll.SetParent(self)
		self.matchScroll.SetPosition(self.width - MARGIN - SCROLL_W, y - 3)
		self.matchScroll.SetScrollBarSize(listH + 4)
		self.matchScroll.SetScrollEvent(ui.__mem_func__(self.OnMatchScroll))
		self.matchScroll.Hide()
		self.widgets.append(self.matchScroll)

		self.statusText = self.__Label(MARGIN + 2, y + listH // 2 - 7, COLOR_DIM)
		self.statusText.SetText(TEXT_HINT)

		# the chosen one
		y += listH + 8
		line = ui.Line()
		line.SetParent(self)
		line.SetPosition(MARGIN, y)
		line.SetSize(self.width - MARGIN * 2, 0)
		line.SetColor(0xff5a4a3a)
		line.Show()
		self.widgets.append(line)

		y += 6
		self.headerText = self.__Label(MARGIN, y, 0xffffffff)
		y += 18
		self.nameHeader = self.__Label(MARGIN + ICON_W, y, COLOR_HEADER)
		self.countHeader = None
		self.chanceHeader = None
		if countX is not None:
			self.countHeader = self.__Label(MARGIN + countX + COUNT_W // 2, y, COLOR_HEADER, True)
			self.countHeader.SetText('Ilo\x9c\xe6')
		if chanceX is not None:
			self.chanceHeader = self.__Label(MARGIN + chanceX + CHANCE_W // 2, y, COLOR_HEADER, True)
			self.chanceHeader.SetText('Szansa')

		y += 18
		self.rowsTop = y
		self.rowWindows = []
		for index in xrange(ROWS):
			row = DropRow(self, index)
			row.SetParent(self)
			row.SetPosition(MARGIN, y + index * ROW_H)
			row.Hide()
			self.rowWindows.append(row)

		rowsH = ROWS * ROW_H
		self.rowScroll = ui.ScrollBar()
		self.rowScroll.SetParent(self)
		self.rowScroll.SetPosition(self.width - MARGIN - SCROLL_W, y - 2)
		self.rowScroll.SetScrollBarSize(rowsH)
		self.rowScroll.SetScrollEvent(ui.__mem_func__(self.OnRowScroll))
		self.rowScroll.Hide()
		self.widgets.append(self.rowScroll)

		self.emptyText = self.__Label(MARGIN + ICON_W, y + 8, COLOR_DIM)

		y += rowsH + 4
		self.footerText = None
		if SHOW_CHANCE:
			self.footerText = self.__Label(MARGIN, y, COLOR_DIM)
			self.footerText.SetText(TEXT_FOOTER)
			y += 16

		self.SetSize(self.width, y + 10)
		self.tooltipItem = uiToolTip.ItemToolTip()
		self.tooltipItem.HideToolTip()
		self.__SetMode('i')
		self.__ClearShow()
		self.SetCenterPosition()
		self.Hide()

	# -- building ------------------------------------------------------------
	def __Label(self, x, y, color, center=False):
		text = ui.TextLine()
		text.SetParent(self)
		text.SetPosition(x, y)
		if center:
			text.SetHorizontalAlignCenter()
		text.SetPackedFontColor(color)
		text.Show()
		self.widgets.append(text)
		return text

	def __Button(self, text, x, y, event):
		button = ui.Button()
		button.SetParent(self)
		button.SetPosition(x, y)
		button.SetUpVisual('d:/ymir work/ui/public/middle_button_01.sub')
		button.SetOverVisual('d:/ymir work/ui/public/middle_button_02.sub')
		button.SetDownVisual('d:/ymir work/ui/public/middle_button_03.sub')
		button.SetText(text)
		button.SetEvent(event)
		button.Show()
		self.widgets.append(button)
		return button

	def __ModeButton(self, text, x, y, mode):
		return self.__Button(text, x, y, ui.__mem_func__(self.OnModeItem if mode == 'i' else self.OnModeMob))

	# -- the search ----------------------------------------------------------
	def __SetMode(self, mode):
		self.mode = mode
		self.__MarkTab(self.itemButton, mode == 'i')
		self.__MarkTab(self.mobButton, mode == 'm')

	def __MarkTab(self, button, chosen):
		# The chosen tab keeps the pressed (darker) look even under the mouse,
		# with gold text; a button's own Down() lasts only until the mouse
		# passes over it.
		if chosen:
			button.SetUpVisual(TAB_CHOSEN)
			button.SetOverVisual(TAB_CHOSEN)
			button.SetDownVisual(TAB_CHOSEN)
			button.SetTextColor(COLOR_TAB_CHOSEN)
		else:
			button.SetUpVisual(TAB_UP)
			button.SetOverVisual(TAB_OVER)
			button.SetDownVisual(TAB_CHOSEN)
			button.SetTextColor(COLOR_TAB)

	def OnModeItem(self):
		self.__ChangeMode('i')

	def OnModeMob(self):
		self.__ChangeMode('m')

	def __ChangeMode(self, mode):
		changed = mode != self.mode
		self.__SetMode(mode)
		if changed and self.searchEdit.GetText().strip():
			self.Search()

	def Search(self):
		query = Fold(self.searchEdit.GetText().strip()).replace('"', '')
		if len(query) < 2 and not query.isdigit():
			self.__SetMatches([])
			self.__Status(TEXT_SHORT)
			return
		self.pendingFind = self.mode
		self.__SetMatches([])
		self.__Status(TEXT_SEARCHING)
		net.SendChatPacket('/drop_wiki find %s %s' % (self.mode, query))

	def __Status(self, text):
		self.statusText.SetText(text)
		if text:
			self.statusText.Show()
		else:
			self.statusText.Hide()

	def __MatchLabel(self, kind, vnum, level):
		if kind == 'i':
			return ItemName(vnum)
		if level > 0:
			return '%s (Lv %d)' % (MobName(vnum), level)
		return MobName(vnum)

	def __SetMatches(self, matches):
		self.matches = matches
		for line in self.matchList.itemList:
			line.Hide()
		self.matchList.ClearItem()
		for index, (kind, vnum, level) in enumerate(matches):
			self.matchList.InsertItem(index, self.__MatchLabel(kind, vnum, level))
		self.matchBase = 0
		self.matchList.SetBasePos(0)
		if len(matches) > MATCH_ROWS:
			self.matchScroll.SetMiddleBarSize(float(MATCH_ROWS) / len(matches))
			self.matchScroll.SetPos(0.0)
			self.matchScroll.Show()
		else:
			self.matchScroll.Hide()

	# The wheel moves the list a line at a time and puts the bar where the list
	# is - not the other way round: a short bar moves in whole pixels, so a
	# step of less than one pixel down was lost and the list never went down.
	def __SetScrollPos(self, scroll, base, extra):
		self.scrollLock = True
		try:
			scroll.SetPos(float(base) / extra)
		finally:
			self.scrollLock = False

	def OnMatchScroll(self):
		extra = len(self.matches) - MATCH_ROWS
		if extra > 0 and not self.scrollLock:
			self.matchBase = int(round(self.matchScroll.GetPos() * extra))
			self.matchList.SetBasePos(self.matchBase)

	def OnMatchWheel(self, direction):
		extra = len(self.matches) - MATCH_ROWS
		if extra > 0:
			self.matchBase = max(0, min(extra, self.matchBase - direction))
			self.matchList.SetBasePos(self.matchBase)
			self.__SetScrollPos(self.matchScroll, self.matchBase, extra)

	def OnMatchSelect(self, index, name):
		if 0 <= index < len(self.matches):
			kind, vnum, level = self.matches[index]
			self.RequestShow(kind, vnum)

	def RequestShow(self, kind, vnum):
		self.__ClearShow()
		self.headerText.SetText(TEXT_LOADING)
		net.SendChatPacket('/drop_wiki show %s %d' % (kind, vnum))

	# -- the list ------------------------------------------------------------
	def __ClearShow(self):
		self.showKind = None
		self.showVnum = 0
		self.rows = []
		self.rowBase = 0
		self.headerText.SetText('')
		self.nameHeader.SetText('')
		for header in (self.countHeader, self.chanceHeader):
			if header:
				header.Hide()
		self.emptyText.Hide()
		self.rowScroll.Hide()
		self.__HideTip()
		for row in self.rowWindows:
			row.Hide()

	def __DrawRows(self):
		isItem = self.showKind == 'm'	# a monster's list holds items
		self.__HideTip()
		for index, row in enumerate(self.rowWindows):
			line = self.rowBase + index
			if line < len(self.rows):
				vnum, countMin, countMax, chance, level = self.rows[line]
				row.SetRow(isItem, vnum, countMin, countMax, chance, level)
			else:
				row.Hide()

	def OnRowScroll(self):
		extra = len(self.rows) - ROWS
		if extra > 0 and not self.scrollLock:
			base = int(round(self.rowScroll.GetPos() * extra))
			if base != self.rowBase:
				self.rowBase = base
				self.__DrawRows()

	def OnRowsWheel(self, direction):
		extra = len(self.rows) - ROWS
		if extra > 0:
			base = max(0, min(extra, self.rowBase - direction * 2))
			if base != self.rowBase:
				self.rowBase = base
				self.__DrawRows()
			self.__SetScrollPos(self.rowScroll, base, extra)

	def __RowAt(self, index):
		line = self.rowBase + index
		if 0 <= line < len(self.rows):
			return self.rows[line]
		return None

	def OnRowOver(self, index):
		row = self.__RowAt(index)
		if row and self.showKind == 'm':
			try:
				self.tooltipItem.SetItemToolTip(row[0])
				self.tooltipItem.ShowToolTip()
			except Exception:
				pass

	def OnRowOut(self, index):
		self.__HideTip()

	def OnRowClick(self, index):
		row = self.__RowAt(index)
		if not row:
			return
		# the other way round: an item's monsters, a monster's items
		other = 'i' if self.showKind == 'm' else 'm'
		if row[4] > 0:
			self.levels[row[0]] = row[4]
		self.__SetMode(other)
		self.RequestShow(other, row[0])

	def __HideTip(self):
		try:
			self.tooltipItem.HideToolTip()
		except Exception:
			pass

	# -- the server ----------------------------------------------------------
	def OnFindBegin(self, kind):
		if kind == self.pendingFind:
			self.matches = []

	def OnMatch(self, vnum, level):
		if self.pendingFind:
			self.matches.append((self.pendingFind, vnum, level))
			if level > 0:
				self.levels[vnum] = level

	def OnFindEnd(self, kind, total):
		if kind != self.pendingFind:
			return
		self.pendingFind = None
		matches = self.matches
		self.__SetMatches(matches)
		if not matches:
			self.__Status(TEXT_NONE)
		elif total > len(matches):
			self.__Status('')
			self.headerText.SetText(TEXT_MANY % (total, len(matches)))
		elif len(matches) > 1:
			self.__Status('')
			self.headerText.SetText(TEXT_FOUND % len(matches))
		else:
			self.__Status('')
			# one match: the server shows it straight away

	def OnShowBegin(self, kind, vnum):
		self.__ClearShow()
		self.showKind = kind
		self.showVnum = vnum
		self.headerText.SetText(TEXT_LOADING)

	def OnRow(self, vnum, countMin, countMax, chance, level):
		if self.showKind:
			self.rows.append((vnum, countMin, countMax, chance, level))
			if level > 0:
				self.levels[vnum] = level

	def OnShowEnd(self, kind, vnum):
		if kind != self.showKind or vnum != self.showVnum:
			return
		if kind == 'i':
			self.headerText.SetText('%s - wypada z:' % ItemName(vnum))
			self.nameHeader.SetText('Potw\xf3r')
		else:
			level = self.levels.get(vnum, 0)
			name = MobName(vnum)
			if level > 0:
				name = '%s (Lv %d)' % (name, level)
			self.headerText.SetText('%s - drop:' % name)
			self.nameHeader.SetText('Przedmiot')
		for header in (self.countHeader, self.chanceHeader):
			if header:
				header.Show()
		self.rowBase = 0
		if len(self.rows) > ROWS:
			self.rowScroll.SetMiddleBarSize(float(ROWS) / len(self.rows))
			self.rowScroll.SetPos(0.0)
			self.rowScroll.Show()
		else:
			self.rowScroll.Hide()
		if not self.rows:
			self.emptyText.SetText(TEXT_NO_MOB if kind == 'i' else TEXT_NO_DROP)
			self.emptyText.Show()
		self.__DrawRows()

	def OnError(self, why):
		if why == 'short':
			self.__Status(TEXT_SHORT)
		elif why == 'busy':
			self.__Status(TEXT_BUSY)
		else:
			self.__Status(TEXT_BAD)
		self.pendingFind = None

	# -- the window ----------------------------------------------------------
	def Open(self):
		self.Show()
		self.SetTop()
		self.searchEdit.SetFocus()

	def Close(self):
		self.__HideTip()
		self.searchEdit.KillFocus()
		self.Hide()

	def OnPressEscapeKey(self):
		self.Close()
		return True

	def Destroy(self):
		self.Close()
		for row in self.rowWindows:
			row.Hide()
		self.rowWindows = []
		self.widgets = []
		self.tooltipItem = None


def GetWindow():
	if not _data['window']:
		_data['window'] = DropWikiWindow()
	return _data['window']


def ToggleWindow():
	wnd = GetWindow()
	if wnd.IsShow():
		wnd.Close()
	else:
		wnd.Open()


def OpenWindow():
	GetWindow().Open()


def DestroyWindow():
	wnd = _data['window']
	if wnd:
		wnd.Destroy()
	_data['window'] = None


def _Int(value, default=0):
	try:
		return int(value)
	except (ValueError, TypeError):
		return default


# game.py "DropWiki": the server's answers.
def OnCommand(sub='', *args):
	if sub == 'open':
		OpenWindow()
		return
	wnd = _data['window']
	if not wnd:
		return
	if sub == 'find_begin' and args:
		wnd.OnFindBegin(args[0])
	elif sub == 'match' and len(args) >= 2:
		wnd.OnMatch(_Int(args[0]), _Int(args[1]))
	elif sub == 'find_end' and len(args) >= 2:
		wnd.OnFindEnd(args[0], _Int(args[1]))
	elif sub == 'show_begin' and len(args) >= 2:
		wnd.OnShowBegin(args[0], _Int(args[1]))
	elif sub == 'row' and len(args) >= 5:
		wnd.OnRow(_Int(args[0]), _Int(args[1]), _Int(args[2]), _Int(args[3]), _Int(args[4]))
	elif sub == 'show_end' and len(args) >= 2:
		wnd.OnShowEnd(args[0], _Int(args[1]))
	elif sub == 'error' and args:
		wnd.OnError(args[0])
