# Ranking tygodniowy - the weekly ranking and its titles
# (MT2009_PLUS_WEEKLY_RANKING_V1; the owner, 3 October: "Bierzemy ranking i
# tytuly z Arezzo. Ranking widziany z poziomu samej gry ... Przy samym nicku
# widnialby napis Bot lub Gracz").
#
# On the basis of the weekly ranking of the Arezzo files (new_uiweeklyrank.py
# and uiscript/weeklyrank.py, byLUZER; its images in mt2009_ui/ranking/): the
# window there read packets the exe 2.0.25 does not have, so this one is our
# own, on the chat-command protocol of playerbot_weekly_rank.h:
#
#   window -> "/ranking info" | "/ranking lista <cat>"   (only on a click or
#             when the window opens, and only in the game phase - warpsafe)
#   server -> "WRANK season <season> <seconds left> <on> <days> <your pid>",
#             "WRANK holder <cat> <place> <pid> <bot> <level> <empire> <value> <name>",
#             "WRANK holdersend", "WRANK begin <cat> <season>",
#             "WRANK row <cat> <pos> <pid> <bot> <level> <empire> <value> <name>",
#             "WRANK me <cat> <pos> <value>", "WRANK end <cat>",
#             "WRANK tail <vid> <cat> <place>" (game.py -> playerbot_status_tail)
#
# Eight categories; the top 3 of a season hold its title for the whole next
# season, with a bonus (TITLE_BONUS). The character window shows the player's
# own titles in a strip under it (AttachCharacterWindow, interfacemodule.py).
# Every image is checked with pack.Exist first and has a plain fallback.
#
# Python 2.7 as the client has it; the texts are CP1250 escapes.

import app
import net
import player
import ui

import warpsafe

CAT_COUNT = 8
LIST_SIZE = 50
VISIBLE_ROWS = 15

IMG = "mt2009_ui/ranking/"

CATEGORY_NAMES = {
	1: "Zabite potwory",
	2: "Zniszczone Metiny",
	3: "Pokonane bossy",
	4: "Zabici gracze",
	5: "Uko\xf1czone wyprawy",
	6: "Udane ulepszenia",
	7: "Alchemia",
	8: "Poziom",
}

TITLE_NAMES = {
	1: "\xa3owca",
	2: "Niszczyciel",
	3: "Pogromca Boss\xf3w",
	4: "Zab\xf3jca",
	5: "Podr\xf3\xbfnik",
	6: "Kowal",
	7: "Alchemik",
	8: "Mistrz Poziom\xf3w",
}

ROMAN = {1: "I", 2: "II", 3: "III"}

PCT_BY_PLACE = {1: 15, 2: 8, 3: 4}
HP_BY_PLACE = {1: 2500, 2: 2000, 3: 1500}
ATT_BY_PLACE = {1: 75, 2: 75, 3: 75}

# what each category's title gives, short (rows) and long (the line under them)
def BonusShort(cat, place):
	if cat in (1, 2, 5):
		return "+%d%% potwory" % PCT_BY_PLACE.get(place, 0)
	if cat == 3:
		return "+%d%% bossy" % PCT_BY_PLACE.get(place, 0)
	if cat == 4:
		return "+%d%% ludzie" % PCT_BY_PLACE.get(place, 0)
	if cat == 8:
		return "+%d%% potwory i ludzie" % PCT_BY_PLACE.get(place, 0)
	if cat == 6:
		return "+%d P\xaf" % HP_BY_PLACE.get(place, 0)
	if cat == 7:
		return "+%d warto\x9c\xe6 ataku" % ATT_BY_PLACE.get(place, 0)
	return ""

BONUS_LONG = {
	1: "Silny przeciwko potworom +15% / +8% / +4%",
	2: "Silny przeciwko potworom +15% / +8% / +4%",
	3: "Silny przeciwko bossom +15% / +8% / +4%",
	4: "Silny przeciwko ludziom +15% / +8% / +4%",
	5: "Silny przeciwko potworom +15% / +8% / +4%",
	6: "Max P\xaf +2500 / +2000 / +1500",
	7: "Warto\x9c\xe6 ataku +75 (miejsca 1-3)",
	8: "Silny przeciwko potworom i ludziom +15% / +8% / +4%",
}

KINGDOMS = {1: "Shinsoo", 2: "Chunjo", 3: "Jinno"}
KINGDOM_COLORS = {1: 0xffff6a6a, 2: 0xffffd75a, 3: 0xff6aa8ff}

TEXT_TITLE = "Ranking tygodniowy"
TEXT_CATEGORIES = "Kategorie"
TEXT_SEASON = "Sezon"
TEXT_SEASON_N = "Sezon %d"
TEXT_LEFT = "Koniec za: %s"
TEXT_OFF = "Ranking wy\xb3\xb9czony"
TEXT_MY_TITLES = "Twoje tytu\xb3y:"
TEXT_NO_TITLES = "brak"
TEXT_LOADING = "Wczytywanie..."
TEXT_EMPTY = "Nikt jeszcze nie ma wyniku w tym sezonie."
TEXT_ME = "Twoje miejsce: %d   (wynik: %s)"
TEXT_ME_NONE = "Nie masz jeszcze miejsca w tej kategorii."
TEXT_HOLDERS = "Tytu\xb3: %s  -  zwyci\xeazcy poprzedniego sezonu"
TEXT_NO_HOLDERS = "Brak posiadaczy - tytu\xb3y dostaje top 3 po ko\xf1cu sezonu."
TEXT_REFRESH_NOTE = "Wyniki od\x9cwie\xbfaj\xb9 si\xea co minut\xea."
TEXT_BOT = "Bot"
TEXT_PLAYER = "Gracz"
TEXT_HEAD_POS = "#"
TEXT_HEAD_NAME = "Posta\xe6"
TEXT_HEAD_LEVEL = "Poz."
TEXT_HEAD_KINGDOM = "Kr\xf3lestwo"
TEXT_HEAD_VALUE = "Wynik"
TEXT_HEAD_EXP = "Do\x9cw."
TEXT_STRIP = "Tytu\xb3y rankingu:"

COLOR_GOLD = 0xffffd040
COLOR_SILVER = 0xffd8dde8
COLOR_BRONZE = 0xffe0a060
COLOR_TEXT = 0xfff1e6c0
COLOR_DIM = 0xff9a9a9a
COLOR_BOT = 0xff9ab4d0
COLOR_PLAYER = 0xff8be37a
COLOR_ME = 0xffffe680
COLOR_OFF = 0xffff6060
PLACE_COLORS = {1: COLOR_GOLD, 2: COLOR_SILVER, 3: COLOR_BRONZE}

_state = {
	'window': None,
	'season': 0,
	'left': 0,
	'leftAt': 0.0,
	'enabled': True,
	'days': 7,
	'myPid': 0,
	'holders': {},      # cat -> {place: (pid, bot, level, empire, value, name)}
	'incomingHolders': None,
	'lists': {},        # cat -> [(pos, pid, bot, level, empire, value, name)]
	'incoming': {},     # cat -> list being received
	'me': {},           # cat -> (pos, value)
	'strips': [],
	'infoAskedAt': -1000.0,
}


def ToInt(value, default=0):
	try:
		return int(value)
	except (TypeError, ValueError):
		return default


def ToLong(value, default=0):
	try:
		return long(value)
	except (TypeError, ValueError):
		return default


def Number(value):
	text = str(ToLong(value))
	negative = text.startswith('-')
	if negative:
		text = text[1:]
	parts = []
	while len(text) > 3:
		parts.insert(0, text[-3:])
		text = text[:-3]
	parts.insert(0, text)
	return ('-' if negative else '') + '.'.join(parts)


def TitleText(cat, place):
	return "%s %s" % (TITLE_NAMES.get(cat, "?"), ROMAN.get(place, ""))


def ImageExists(path):
	try:
		import pack
		return bool(pack.Exist(path))
	except Exception:
		return True


def Send(command):
	# MT2009_PLUS_SIDEKICK_WARP_SAFE_V1: only in the game phase (warpsafe.py).
	if not warpsafe.InGame():
		return False
	net.SendChatPacket("/ranking " + command)
	return True


def AskInfo(force=False):
	now = app.GetTime()
	if not force and now - _state['infoAskedAt'] < 20.0:
		return
	if Send("info"):
		_state['infoAskedAt'] = now


def SecondsLeft():
	left = _state['left'] - (app.GetTime() - _state['leftAt'])
	if left < 0:
		return 0
	return int(left)


def LeftText(seconds):
	days = seconds // 86400
	seconds %= 86400
	return "%dd %02d:%02d:%02d" % (days, seconds // 3600, (seconds // 60) % 60, seconds % 60)


def MyTitles():
	"""[(cat, place)] the player holds this season."""
	out = []
	pid = _state['myPid']
	name = ""
	try:
		name = player.GetName()
	except Exception:
		pass
	for cat in xrange(1, CAT_COUNT + 1):
		for place, holder in _state['holders'].get(cat, {}).items():
			if (pid and holder[0] == pid) or (not pid and name and holder[5] == name):
				out.append((cat, place))
	out.sort(key=lambda t: (t[1], t[0]))
	return out


class RankRow(ui.Window):
	"""One line of the list: the place, a crown for the top 3, the name with
	its Bot/Gracz tag, the level, the kingdom and the value."""

	WIDTH = 397
	HEIGHT = 21

	def __init__(self, parent):
		ui.Window.__init__(self)
		self.SetParent(parent)
		self.SetSize(self.WIDTH, self.HEIGHT)
		self.AddFlag("not_pick")
		self.backs = {}
		for kind in (1, 2, 3, 4):
			path = IMG + "row_%d.png" % kind
			if ImageExists(path):
				image = ui.ImageBox()
				image.SetParent(self)
				image.AddFlag("not_pick")
				image.LoadImage(path)
				image.SetPosition(0, 0)
				self.backs[kind] = image
		if not self.backs:
			bar = ui.Bar()
			bar.SetParent(self)
			bar.AddFlag("not_pick")
			bar.SetPosition(0, 0)
			bar.SetSize(self.WIDTH, self.HEIGHT - 1)
			bar.SetColor(0x80201a10)
			bar.Show()
			self.fallbackBar = bar
		self.highlight = ui.Bar()
		self.highlight.SetParent(self)
		self.highlight.AddFlag("not_pick")
		self.highlight.SetPosition(0, 0)
		self.highlight.SetSize(self.WIDTH, self.HEIGHT)
		self.highlight.SetColor(0x40ffd040)
		self.crowns = {}
		for place in (1, 2, 3):
			path = IMG + "crown_%d.png" % place
			if ImageExists(path):
				crown = ui.ImageBox()
				crown.SetParent(self)
				crown.AddFlag("not_pick")
				crown.LoadImage(path)
				crown.SetPosition(28, 0)
				self.crowns[place] = crown
		self.posLine = self.__Text(16, True)
		self.nameLine = self.__Text(70)
		self.tagLine = self.__Text(184)
		# MT2009_PLUS_WEEKLY_RANKING_UX_V1: the level and the kingdom moved left,
		# so a long value (the experience) never runs into the kingdom.
		self.levelLine = self.__Text(240, True)
		self.kingdomLine = self.__Text(288, True)
		self.valueLine = self.__Text(390)
		self.valueLine.SetHorizontalAlignRight()

	def __Text(self, x, center=False):
		line = ui.TextLine()
		line.SetParent(self)
		line.SetPosition(x, 4)
		if center:
			line.SetHorizontalAlignCenter()
		line.Show()
		return line

	def SetRow(self, pos, bot, level, empire, value, name, mine, valueText=None):
		kind = pos if pos in (1, 2, 3) else 4
		for k, image in self.backs.items():
			if k == kind:
				image.Show()
			else:
				image.Hide()
		for place, crown in self.crowns.items():
			if place == pos:
				crown.Show()
			else:
				crown.Hide()
		self.posLine.SetText(str(pos))
		self.posLine.SetPackedFontColor(PLACE_COLORS.get(pos, COLOR_TEXT))
		self.nameLine.SetText(name)
		self.nameLine.SetPackedFontColor(COLOR_ME if mine else PLACE_COLORS.get(pos, COLOR_TEXT))
		self.tagLine.SetText("[%s]" % (TEXT_BOT if bot else TEXT_PLAYER))
		self.tagLine.SetPackedFontColor(COLOR_BOT if bot else COLOR_PLAYER)
		self.levelLine.SetText(str(level))
		self.kingdomLine.SetText(KINGDOMS.get(empire, "-"))
		self.kingdomLine.SetPackedFontColor(KINGDOM_COLORS.get(empire, COLOR_DIM))
		self.valueLine.SetText(valueText if valueText is not None else Number(value))
		if mine:
			self.highlight.Show()
		else:
			self.highlight.Hide()
		self.Show()


class HolderRow(ui.Window):
	"""A title holder: the crown, the name and tag, the title and its bonus."""

	WIDTH = 397
	HEIGHT = 21

	def __init__(self, parent, place):
		ui.Window.__init__(self)
		self.SetParent(parent)
		self.SetSize(self.WIDTH, self.HEIGHT)
		self.AddFlag("not_pick")
		path = IMG + "row_%d.png" % place
		if ImageExists(path):
			back = ui.ImageBox()
			back.SetParent(self)
			back.AddFlag("not_pick")
			back.LoadImage(path)
			back.Show()
			self.back = back
		else:
			bar = ui.Bar()
			bar.SetParent(self)
			bar.AddFlag("not_pick")
			bar.SetSize(self.WIDTH, self.HEIGHT - 1)
			bar.SetColor(0x80201a10)
			bar.Show()
			self.back = bar
		path = IMG + "crown_%d.png" % place
		if ImageExists(path):
			crown = ui.ImageBox()
			crown.SetParent(self)
			crown.AddFlag("not_pick")
			crown.LoadImage(path)
			crown.SetPosition(2, 0)
			crown.Show()
			self.crown = crown
		self.place = place
		# MT2009_PLUS_WEEKLY_RANKING_UX_V1: the name, the tag and the title -
		# the bonus of each place is said once, under the three rows; in the
		# row it ran over the title.
		self.nameLine = self.__Text(46)
		self.tagLine = self.__Text(176)
		self.titleLine = self.__Text(390)
		self.titleLine.SetHorizontalAlignRight()
		self.bonusLine = self.__Text(0)
		self.bonusLine.Hide()

	def __Text(self, x):
		line = ui.TextLine()
		line.SetParent(self)
		line.SetPosition(x, 4)
		line.Show()
		return line

	def SetHolder(self, cat, holder, mine):
		if not holder:
			self.nameLine.SetText("-")
			self.nameLine.SetPackedFontColor(COLOR_DIM)
			self.tagLine.SetText("")
			self.titleLine.SetText(TitleText(cat, self.place))
			self.titleLine.SetPackedFontColor(COLOR_DIM)
			return
		pid, bot, level, empire, value, name = holder
		self.nameLine.SetText("%s (%d)" % (name, level))
		self.nameLine.SetPackedFontColor(COLOR_ME if mine else KINGDOM_COLORS.get(empire, COLOR_TEXT))
		self.tagLine.SetText("[%s]" % (TEXT_BOT if bot else TEXT_PLAYER))
		self.tagLine.SetPackedFontColor(COLOR_BOT if bot else COLOR_PLAYER)
		self.titleLine.SetText(TitleText(cat, self.place))
		self.titleLine.SetPackedFontColor(PLACE_COLORS.get(self.place, COLOR_TEXT))



class WeeklyRankWindow(ui.BoardWithTitleBar):
	WIDTH = 618
	HEIGHT = 600
	LEFT_X = 10
	LEFT_Y = 34
	LEFT_W = 166
	LIST_X = 186
	HEAD_Y = 36
	ROWS_Y = 60
	ROW_STEP = 22

	def __init__(self):
		ui.BoardWithTitleBar.__init__(self)
		self.widgets = []
		self.catButtons = {}
		self.rows = []
		self.holderRows = []
		self.myTitleLines = []
		self.cat = 1
		self.startRow = 0
		self.lastSecond = -1
		self.AddFlag('movable')
		self.AddFlag('float')
		self.SetSize(self.WIDTH, self.HEIGHT)
		self.SetTitleName(TEXT_TITLE)
		self.SetCloseEvent(ui.__mem_func__(self.Close))
		self.SetMouseWheelEvent(self.OnWheel)
		self.__Build()
		self.SetCenterPosition()

	# ------------------------------------------------------------ building

	def __Label(self, parent, x, y, text, color=None, center=False):
		line = ui.TextLine()
		line.SetParent(parent)
		line.SetPosition(x, y)
		if center:
			line.SetHorizontalAlignCenter()
		line.SetText(text)
		if color is not None:
			line.SetPackedFontColor(color)
		line.Show()
		self.widgets.append(line)
		return line

	def __Header(self, parent, x, y, width, small):
		path = IMG + ("header_small.png" if small else "header_big.png")
		if ImageExists(path):
			image = ui.ImageBox()
			image.SetParent(parent)
			image.LoadImage(path)
			image.SetPosition(x, y)
			image.Show()
			self.widgets.append(image)
		else:
			bar = ui.Bar()
			bar.SetParent(parent)
			bar.SetPosition(x, y)
			bar.SetSize(width, 21)
			bar.SetColor(0xc0302820)
			bar.Show()
			self.widgets.append(bar)

	def __Build(self):
		left = ui.ThinBoard()
		left.SetParent(self)
		left.SetPosition(self.LEFT_X, self.LEFT_Y)
		left.SetSize(self.LEFT_W, self.HEIGHT - self.LEFT_Y - 12)
		left.Show()
		self.widgets.append(left)

		self.__Header(left, 5, 5, 156, True)
		self.__Label(left, 83, 8, TEXT_CATEGORIES, COLOR_TEXT, True)
		catImages = [IMG + "cat_%d.tga" % i for i in (0, 1, 2)]
		useImages = all([ImageExists(p) for p in catImages])
		y = 32
		for cat in xrange(1, CAT_COUNT + 1):
			button = ui.Button()
			button.SetParent(left)
			button.SetPosition(9, y)
			if useImages:
				button.SetUpVisual(catImages[0])
				button.SetOverVisual(catImages[1])
				button.SetDownVisual(catImages[2])
			else:
				button.SetUpVisual('d:/ymir work/ui/public/large_button_01.sub')
				button.SetOverVisual('d:/ymir work/ui/public/large_button_02.sub')
				button.SetDownVisual('d:/ymir work/ui/public/large_button_03.sub')
			button.SetText(CATEGORY_NAMES[cat])
			button.SAFE_SetEvent(self.OnClickCategory, cat)
			button.Show()
			self.widgets.append(button)
			self.catButtons[cat] = button
			y += 26

		y += 6
		self.__Header(left, 5, y, 156, True)
		self.__Label(left, 83, y + 3, TEXT_SEASON, COLOR_TEXT, True)
		y += 28
		self.seasonLine = self.__Label(left, 10, y, "", COLOR_GOLD)
		self.leftLine = self.__Label(left, 10, y + 16, "", COLOR_TEXT)
		y += 42
		self.__Label(left, 10, y, TEXT_MY_TITLES, COLOR_TEXT)
		y += 17
		for i in xrange(CAT_COUNT):
			line = self.__Label(left, 14, y + i * 15, "", COLOR_DIM)
			self.myTitleLines.append(line)

		x = self.LIST_X
		self.__Header(self, x, self.HEAD_Y, 405, False)
		self.__Label(self, x + 16, self.HEAD_Y + 4, TEXT_HEAD_POS, COLOR_TEXT, True)
		self.__Label(self, x + 70, self.HEAD_Y + 4, TEXT_HEAD_NAME, COLOR_TEXT)
		self.__Label(self, x + 240, self.HEAD_Y + 4, TEXT_HEAD_LEVEL, COLOR_TEXT, True)
		self.__Label(self, x + 288, self.HEAD_Y + 4, TEXT_HEAD_KINGDOM, COLOR_TEXT, True)
		self.valueHead = self.__Label(self, x + 390, self.HEAD_Y + 4, TEXT_HEAD_VALUE, COLOR_TEXT)
		self.valueHead.SetHorizontalAlignRight()

		for i in xrange(VISIBLE_ROWS):
			row = RankRow(self)
			row.SetPosition(x, self.ROWS_Y + i * self.ROW_STEP)
			row.Hide()
			self.rows.append(row)
		listHeight = VISIBLE_ROWS * self.ROW_STEP
		self.emptyLine = self.__Label(self, x + 200, self.ROWS_Y + 100, "", COLOR_DIM, True)

		scrollBar = ui.ScrollBar()
		scrollBar.SetParent(self)
		scrollBar.SetPosition(x + 400, self.ROWS_Y)
		scrollBar.SetScrollBarSize(listHeight - 2)
		scrollBar.SetScrollEvent(ui.__mem_func__(self.OnScroll))
		scrollBar.Hide()
		self.scrollBar = scrollBar
		self.widgets.append(scrollBar)

		y = self.ROWS_Y + listHeight + 4
		meBoard = ui.ThinBoard()
		meBoard.SetParent(self)
		meBoard.SetPosition(x, y)
		meBoard.SetSize(417, 26)
		meBoard.Show()
		self.widgets.append(meBoard)
		self.meLine = self.__Label(meBoard, 12, 6, "", COLOR_ME)

		y += 34
		self.__Header(self, x, y, 405, False)
		self.holdersHead = self.__Label(self, x + 202, y + 4, "", COLOR_GOLD, True)
		y += 25
		for place in (1, 2, 3):
			row = HolderRow(self, place)
			row.SetPosition(x, y)
			row.Show()
			self.holderRows.append(row)
			y += self.ROW_STEP
		self.bonusLine = self.__Label(self, x + 202, y + 4, "", COLOR_GOLD, True)
		self.noHoldersLine = self.__Label(self, x + 202, y + 22, "", COLOR_DIM, True)
		self.__Label(self, x + 202, y + 40, TEXT_REFRESH_NOTE, COLOR_DIM, True)
		self.__SelectButton()

	# ------------------------------------------------------------ refreshing

	def __SelectButton(self):
		for cat, button in self.catButtons.items():
			if cat == self.cat:
				button.Down()
			else:
				button.SetUp()

	def RefreshSeason(self):
		if not _state['season']:
			self.seasonLine.SetText(TEXT_LOADING)
			self.leftLine.SetText("")
		elif not _state['enabled']:
			self.seasonLine.SetText(TEXT_SEASON_N % _state['season'])
			self.leftLine.SetText(TEXT_OFF)
			self.leftLine.SetPackedFontColor(COLOR_OFF)
		else:
			self.seasonLine.SetText(TEXT_SEASON_N % _state['season'])
			self.leftLine.SetPackedFontColor(COLOR_TEXT)
			self.lastSecond = -1
			self.__RefreshLeft()
		titles = MyTitles() if _state['enabled'] else []
		for i, line in enumerate(self.myTitleLines):
			if i < len(titles):
				cat, place = titles[i]
				line.SetText("%s (%s)" % (TitleText(cat, place), BonusShort(cat, place)))
				line.SetPackedFontColor(PLACE_COLORS.get(place, COLOR_TEXT))
			elif i == 0 and not titles:
				line.SetText(TEXT_NO_TITLES)
				line.SetPackedFontColor(COLOR_DIM)
			else:
				line.SetText("")

	def __RefreshLeft(self):
		if not _state['season'] or not _state['enabled']:
			return
		left = SecondsLeft()
		if left == self.lastSecond:
			return
		self.lastSecond = left
		self.leftLine.SetText(TEXT_LEFT % LeftText(left))

	def RefreshHolders(self):
		cat = self.cat
		holders = _state['holders'].get(cat, {})
		self.holdersHead.SetText(TEXT_HOLDERS % TITLE_NAMES.get(cat, "?"))
		myPid = _state['myPid']
		for i, row in enumerate(self.holderRows):
			holder = holders.get(i + 1)
			row.SetHolder(cat, holder, bool(holder and myPid and holder[0] == myPid))
		self.noHoldersLine.SetText("" if holders else TEXT_NO_HOLDERS)
		self.bonusLine.SetText(BONUS_LONG.get(cat, ""))

	def RefreshList(self):
		cat = self.cat
		rows = _state['lists'].get(cat)
		self.valueHead.SetText(TEXT_HEAD_EXP if cat == 8 else TEXT_HEAD_VALUE)
		if rows is None:
			self.emptyLine.SetText(TEXT_LOADING)
			rows = []
		elif not rows:
			self.emptyLine.SetText(TEXT_EMPTY)
		else:
			self.emptyLine.SetText("")
		scrollable = max(0, len(rows) - VISIBLE_ROWS)
		if scrollable:
			self.scrollBar.SetMiddleBarSize(float(VISIBLE_ROWS) / float(len(rows)))
			self.scrollBar.Show()
		else:
			self.scrollBar.Hide()
		self.startRow = min(self.startRow, scrollable)
		self.__LayoutRows()
		me = _state['me'].get(cat)
		if me is None:
			self.meLine.SetText(TEXT_LOADING if _state['lists'].get(cat) is None else "")
		elif me[0] > 0:
			self.meLine.SetText(TEXT_ME % (me[0], Number(me[1])))
		else:
			self.meLine.SetText(TEXT_ME_NONE)

	def __LayoutRows(self):
		rows = _state['lists'].get(self.cat) or []
		myPid = _state['myPid']
		for i, widget in enumerate(self.rows):
			index = self.startRow + i
			if index < len(rows):
				pos, pid, bot, level, empire, value, name = rows[index]
				widget.SetRow(pos, bot, level, empire, value, name, bool(myPid and pid == myPid))
			else:
				widget.Hide()

	def Refresh(self):
		self.RefreshSeason()
		self.RefreshHolders()
		self.RefreshList()

	# ------------------------------------------------------------ events

	def OnScroll(self):
		rows = _state['lists'].get(self.cat) or []
		scrollable = max(0, len(rows) - VISIBLE_ROWS)
		startRow = int(round(scrollable * self.scrollBar.GetPos()))
		if startRow != self.startRow:
			self.startRow = startRow
			self.__LayoutRows()

	def OnWheel(self, direction):
		rows = _state['lists'].get(self.cat) or []
		scrollable = max(0, len(rows) - VISIBLE_ROWS)
		if not scrollable:
			return
		startRow = max(0, min(scrollable, self.startRow - 3 * direction))
		self.scrollBar.SetPos(float(startRow) / float(scrollable))

	def OnClickCategory(self, cat):
		self.cat = cat
		self.startRow = 0
		self.scrollBar.SetPos(0.0)
		self.__SelectButton()
		self.RefreshHolders()
		self.RefreshList()
		Send("lista %d" % cat)

	def OnUpdate(self):
		self.__RefreshLeft()

	def Open(self):
		self.Refresh()
		self.Show()
		self.SetTop()
		AskInfo(True)
		Send("lista %d" % self.cat)

	def Close(self):
		self.Hide()

	def OnPressEscapeKey(self):
		self.Close()
		return True

	def Destroy(self):
		self.Hide()
		for row in self.rows + self.holderRows:
			row.Hide()
		self.rows = []
		self.holderRows = []
		self.catButtons = {}
		self.myTitleLines = []
		self.widgets = []


# ---------------------------------------------------------------- the strip
# under the character window: the player's own titles (interfacemodule.py
# calls AttachCharacterWindow once the window is made).

class CharacterTitleStrip(ui.Window):
	LINE_H = 15

	def __init__(self, wndCharacter):
		ui.Window.__init__(self)
		self.SetParent(wndCharacter)
		self.width = wndCharacter.GetWidth()
		self.SetPosition(0, wndCharacter.GetHeight() - 2)
		self.SetSize(self.width, 4)
		self.AddFlag("not_pick")
		self.lastSeen = -1000.0
		self.shown = None
		board = ui.ThinBoard()
		board.SetParent(self)
		board.SetPosition(0, 0)
		board.SetSize(self.width, 26)
		board.Hide()
		self.board = board
		self.head = ui.TextLine()
		self.head.SetParent(board)
		self.head.SetPosition(10, 6)
		self.head.SetText(TEXT_STRIP)
		self.head.SetPackedFontColor(COLOR_TEXT)
		self.head.Show()
		self.lines = []
		self.Show()

	def __Line(self, i):
		while len(self.lines) <= i:
			line = ui.TextLine()
			line.SetParent(self.board)
			line.SetPosition(14, 22 + len(self.lines) * self.LINE_H)
			line.Show()
			self.lines.append(line)
		return self.lines[i]

	def Refresh(self):
		titles = MyTitles() if _state['enabled'] else []
		if titles == self.shown:
			return
		self.shown = titles
		if not titles:
			self.board.Hide()
			return
		for i, (cat, place) in enumerate(titles):
			line = self.__Line(i)
			line.SetText("%s  -  %s" % (TitleText(cat, place), BonusShort(cat, place)))
			line.SetPackedFontColor(PLACE_COLORS.get(place, COLOR_TEXT))
			line.Show()
		for line in self.lines[len(titles):]:
			line.Hide()
		self.board.SetSize(self.width, 28 + len(titles) * self.LINE_H)
		self.board.Show()

	def OnUpdate(self):
		# Updated only while the character window shows: a gap means it was
		# just opened - ask for the titles (at most every 20 s).
		now = app.GetTime()
		if now - self.lastSeen > 1.0:
			AskInfo()
		self.lastSeen = now

	def Destroy(self):
		self.Hide()
		self.lines = []
		self.board = None


def AttachCharacterWindow(wndCharacter):
	try:
		strip = CharacterTitleStrip(wndCharacter)
		_state['strips'].append(strip)
	except Exception:
		import dbg
		import sys
		dbg.TraceError('uiweeklyrank.AttachCharacterWindow: %s' % str(sys.exc_info()[1]))


def RefreshStrips():
	for strip in _state['strips']:
		try:
			strip.Refresh()
		except Exception:
			pass


# ---------------------------------------------------------------- commands

def OnCommand(*args):
	if not args:
		return
	sub, rest = args[0], args[1:]
	try:
		window = _state['window']
		if sub == 'season':
			_state['season'] = ToInt(rest[0] if len(rest) > 0 else 0)
			_state['left'] = ToInt(rest[1] if len(rest) > 1 else 0)
			_state['leftAt'] = app.GetTime()
			_state['enabled'] = ToInt(rest[2] if len(rest) > 2 else 1) != 0
			_state['days'] = ToInt(rest[3] if len(rest) > 3 else 7, 7)
			_state['myPid'] = ToInt(rest[4] if len(rest) > 4 else 0)
			_state['incomingHolders'] = {}
		elif sub == 'holder':
			incoming = _state['incomingHolders']
			if incoming is None or len(rest) < 8:
				return
			cat, place = ToInt(rest[0]), ToInt(rest[1])
			if 1 <= cat <= CAT_COUNT and 1 <= place <= 3:
				incoming.setdefault(cat, {})[place] = (ToInt(rest[2]), ToInt(rest[3]) != 0, ToInt(rest[4]),
						ToInt(rest[5]), ToLong(rest[6]), rest[7])
		elif sub == 'holdersend':
			if _state['incomingHolders'] is not None:
				_state['holders'] = _state['incomingHolders']
				_state['incomingHolders'] = None
			if window is not None and window.IsShow():
				window.RefreshSeason()
				window.RefreshHolders()
				window.RefreshList()
			RefreshStrips()
		elif sub == 'begin':
			cat = ToInt(rest[0] if rest else 0)
			if 1 <= cat <= CAT_COUNT:
				_state['incoming'][cat] = []
		elif sub == 'row':
			if len(rest) < 8:
				return
			cat = ToInt(rest[0])
			incoming = _state['incoming'].get(cat)
			if incoming is None or len(incoming) >= LIST_SIZE:
				return
			incoming.append((ToInt(rest[1]), ToInt(rest[2]), ToInt(rest[3]) != 0, ToInt(rest[4]), ToInt(rest[5]),
					ToLong(rest[6]), rest[7]))
		elif sub == 'me':
			cat = ToInt(rest[0] if rest else 0)
			if 1 <= cat <= CAT_COUNT and len(rest) >= 3:
				_state['me'][cat] = (ToInt(rest[1]), ToLong(rest[2]))
		elif sub == 'end':
			cat = ToInt(rest[0] if rest else 0)
			incoming = _state['incoming'].pop(cat, None)
			if incoming is not None:
				_state['lists'][cat] = incoming
			if window is not None and window.IsShow() and window.cat == cat:
				window.RefreshList()
	except Exception:
		import dbg
		import sys
		dbg.TraceError('uiweeklyrank.OnCommand %s: %s' % (str(args), str(sys.exc_info()[1])))


def ToggleWindow():
	window = _state['window']
	if window is None:
		import uiminigameutil
		window = uiminigameutil.SafeCreate(WeeklyRankWindow, TEXT_TITLE)
		if window is None:
			return
		_state['window'] = window
	if window.IsShow():
		window.Close()
	else:
		window.Open()


def DestroyWindow():
	window = _state['window']
	_state['window'] = None
	_state['incoming'] = {}
	_state['incomingHolders'] = None
	for strip in _state['strips']:
		try:
			strip.Destroy()
		except Exception:
			pass
	_state['strips'] = []
	if window is not None:
		window.Destroy()
