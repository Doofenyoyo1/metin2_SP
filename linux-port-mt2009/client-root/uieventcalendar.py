# Kalendarz eventow - F11 or the button on the task bar (the operator, 28
# September: "kalendarz eventow ... tylko okno kalendarza pod F11 i przycisk
# gdzies w gui, z eventami ustawionymi w panelu"), drawn the way dracaryS'
# "event manager" draws it ("popraw GUI dokladnie tak jak jest w tym event
# managerze"): the month on the mod's board, eight days a row, a tile a day -
# black, blue when it holds an event, the bright one for today - with the
# day's event icons fading one into the next and a tooltip of the day's
# events; and the mod's small movable icon on the screen with what runs now
# or starts within half an hour, counting down.
#
# The events are the ones the web panels schedule (the weekly windows and
# "activate now"): the server keeps them in /opt/m2spool/playerbot_events.tsv
# and "/kalendarz" asks the player's core for them (playerbot_manager.cpp,
# SendEventCalendar):
#   EventCalBegin <epoch> <year> <month> <mday> <weekday 0=Mon> <minute> <utc offset min>
#   EventCal <kind> <days, bit 0 = Mon> <start min> <end min> <value> <now> <since> <until> <map>
#   EventCalEnd <lines>
# The dates are the server's. A weekly window shows on every day of the month
# its days mask names; an "activate now" one on the days it spans.
#
# What the mod does with its own packets and C++ functions is done here in
# Python: the day list comes from the lines above, the countdowns from the
# server's clock of the last answer moved on by the client's, and the answer
# is asked for again every few minutes (Start, from game.py).
#
# Python 2.7 as the client has it; the texts are CP1250 escapes.

import app
import net
import ui
import wndMgr
import time
import uiToolTip

IMG = 'mt2009_ui/calendar/'

KIND_CHEST, KIND_EXP, KIND_DROP, KIND_YANG, KIND_TANAKA, KIND_ZUO, KIND_BOSS_LOOT, KIND_METIN_LOOT = range(8)
# MT2009_PLUS_GOBLIN_V1: the Treasure Hunt (playerbot_goblin.h, uigoblin.py).
KIND_GOBLIN = 8
# MT2009_PLUS_EVENT_MANAGER_V1: the mini games and the Easter event
# (playerbot_ingame_events.h); their names and icons are the event hub's
# (uiingameevent.py EVENTS, by the kind's name).
KIND_CATCHKING, KIND_RUMI, KIND_YUTNORI, KIND_FLOWER, KIND_EASTER = range(9, 14)
KIND_KEYS = {KIND_CATCHKING: 'catchking', KIND_RUMI: 'rumi', KIND_YUTNORI: 'yutnori',
		KIND_FLOWER: 'flower', KIND_EASTER: 'easter'}
# Every kind's name, the key the in-game event list knows it by.
KIND_NAMES = ('chest', 'exp', 'drop', 'yang', 'tanaka', 'zuo', 'bossloot', 'metinloot', 'goblin',
		'catchking', 'rumi', 'yutnori', 'flower', 'easter')
KIND_ICON = {
	KIND_CHEST: 'moonlight_event.tga',
	KIND_EXP: 'exp_event.tga',
	KIND_DROP: 'item_drop_event.tga',
	KIND_YANG: 'money_drop_event.tga',
	KIND_TANAKA: 'npc_search.tga',
	KIND_ZUO: 'halloween_event.tga',
	KIND_BOSS_LOOT: 'double_boss_loot_event.tga',
	KIND_METIN_LOOT: 'double_metin_loot_event.tga',
	KIND_GOBLIN: 'goblin_event.tga',
}
DEFAULT_ICON = 'bonus_event.tga'
WORLD_KINDS = (KIND_TANAKA, KIND_ZUO)

MONTHS = ('Stycze\xf1', 'Luty', 'Marzec', 'Kwiecie\xf1', 'Maj', 'Czerwiec', 'Lipiec',
		'Sierpie\xf1', 'Wrzesie\xf1', 'Pa\x9fdziernik', 'Listopad', 'Grudzie\xf1')

# The maps Tanaka and Zuo come to (playerbot_world_events.h).
MAP_NAMES = {
	21: 'Joan', 1: 'Yongan', 41: 'Pyungmoo', 23: 'Bokjung', 3: 'Jayang', 43: 'Bakra',
	64: 'Dolina Ork\xf3w', 63: 'Pustynia Yongbi', 61: 'G\xf3ra Sohan', 65: '\x8cwi\xb9tynia Hwang',
	62: 'Ognista Ziemia', 67: 'Las Duch\xf3w', 68: 'Czerwony Las',
}

# The mod's locale_game.txt lines, in Polish.
TITLE = 'Kalendarz event\xf3w: %s - %d'
COLORFUL_TEXT = '|cFF84A5B9%s|h|r'
TOOLTIP_TITLE = 'Eventy w dniu %s:'
TOOLTIP_NO_EVENTS = 'Brak event\xf3w w tym dniu.'
ALL_KINGDOMS = '|cFF97AE99Wszystkie kr\xf3lestwa:|h|r'
ALL_CHANNELS = '|cFF97AE99Wszystkie kana\xb3y|r'
NORMAL_EVENT_TIME = 'Od: %s Do: %s'
SPAWN_MAPS = 'Pojawia si\xea na mapie:'
RANDOM_MAP = 'losowa mapa'
RUNNING_NOW = 'TRWA TERAZ'
END_IN = 'Koniec za %s'
START_IN = 'Start za %s'
NEXT_EVENT = 'PPM - nast\xeapny event'

# The mini icon shows what starts within this many seconds.
SOON = 30 * 60


def KindName(kind, value):
	if kind == KIND_CHEST:
		return 'Szkatu\xb3ki Blasku Ksi\xea\xbfyca'
	if kind == KIND_EXP:
		return 'Event do\x9cwiadczenia: +%d%%' % value
	if kind == KIND_DROP:
		return 'Event dropu przedmiot\xf3w: +%d%%' % value
	if kind == KIND_YANG:
		return 'Event dropu yang: +%d%%' % value
	if kind == KIND_TANAKA:
		return 'Pirat Tanaka'
	if kind == KIND_ZUO:
		return 'Deszcz Metin\xf3w Zuo'
	if kind == KIND_BOSS_LOOT:
		return 'Podw\xf3jny loot z boss\xf3w'
	if kind == KIND_METIN_LOOT:
		return 'Podw\xf3jny loot z Metin\xf3w'
	if kind == KIND_GOBLIN:
		return 'Poszukiwanie skarb\xf3w (Goblin)'
	if kind in KIND_KEYS:
		return __import__('uiingameevent').Presentation(KIND_KEYS[kind])['name']
	return 'Event %d' % kind


def Colorful(text):
	return COLORFUL_TEXT % text


def HHMM(minutes):
	minutes = minutes % (24 * 60)
	return '%02d:%02d' % (minutes // 60, minutes % 60)


def FormatTime(seconds):
	if seconds <= 0:
		return ''
	m, s = divmod(int(seconds), 60)
	h, m = divmod(m, 60)
	return '%02dh %02dm %02ds' % (h, m, s)


# Calendar arithmetic without the datetime module: days since 1970-01-01.
def DaysFromCivil(y, m, d):
	y -= m <= 2
	era = (y if y >= 0 else y - 399) // 400
	yoe = y - era * 400
	doy = (153 * (m + (-3 if m > 2 else 9)) + 2) // 5 + d - 1
	doe = yoe * 365 + yoe // 4 - yoe // 100 + doy
	return era * 146097 + doe - 719468


def CivilFromDays(z):
	z += 719468
	era = (z if z >= 0 else z - 146096) // 146097
	doe = z - era * 146097
	yoe = (doe - doe // 1460 + doe // 36524 - doe // 146096) // 365
	y = yoe + era * 400
	doy = doe - (365 * yoe + yoe // 4 - yoe // 100)
	mp = (5 * doy + 2) // 153
	d = doy - (153 * mp + 2) // 5 + 1
	m = mp + (3 if mp < 10 else -9)
	return (y + (m <= 2), m, d)


def DaysInMonth(y, m):
	if m == 12:
		return DaysFromCivil(y + 1, 1, 1) - DaysFromCivil(y, 12, 1)
	return DaysFromCivil(y, m + 1, 1) - DaysFromCivil(y, m, 1)


def Weekday(days):
	# 1970-01-01 was a Thursday; 0 = Monday.
	return (days + 3) % 7


_data = {'now': None, 'lines': [], 'pending': [], 'window': None, 'icon': None, 'ticker': None, 'tooltip': None}


def OnBegin(epoch='0', year='1970', month='1', mday='1', weekday='0', minute='0', offset='0', *rest):
	try:
		_data['pending'] = []
		_data['begin'] = (int(epoch), int(year), int(month), int(mday), int(weekday), int(minute), int(offset))
		_data['beginAt'] = app.GetTime()
	except ValueError:
		_data['begin'] = None


def OnLine(*args):
	try:
		kind, days, start, end, value, now, since, until, mapIndex = [int(a) for a in args[:9]]
	except (ValueError, TypeError):
		return
	_data['pending'].append({'kind': kind, 'days': days, 'start': start, 'end': end, 'value': value,
			'now': now, 'since': since, 'until': until, 'map': mapIndex})


def OnEnd(*rest):
	if not _data.get('begin'):
		return
	_data['now'] = _data['begin']
	_data['receivedAt'] = _data.get('beginAt', app.GetTime())
	_data['lines'] = _data['pending']
	_data['pending'] = []
	wnd = _data['window']
	if wnd and wnd.IsShow():
		wnd.Refresh()
	RefreshIcon()


def Request():
	net.SendChatPacket('/kalendarz')


def ServerNow():
	"""The server's epoch second: the last answer's, moved on by the time
	since it came."""
	if not _data['now']:
		return None
	return _data['now'][0] + int(app.GetTime() - _data.get('receivedAt', app.GetTime()))


def LocalDay(epoch):
	return (epoch + _data['now'][6] * 60) // 86400


def LocalMinute(epoch):
	return (epoch + _data['now'][6] * 60) % 86400 // 60


def WindowLength(line):
	"""A weekly window's minutes; one whose end is not after its start runs
	past midnight."""
	if line['end'] > line['start']:
		return line['end'] - line['start']
	return 24 * 60 - line['start'] + line['end']


def EventsOnDay(day):
	"""(start epoch, end epoch, kind, value, map) of the events on the
	server's day `day` (days since 1970): the weekly windows that open that
	day and the "activate now" ones that span it."""
	result = []
	if not _data['now']:
		return result
	offset = _data['now'][6] * 60
	dayStart = day * 86400 - offset
	wd = Weekday(day)
	for line in _data['lines']:
		if line['now']:
			since = line['since'] or _data['now'][0]
			until = max(line['until'], since)
			first = (since + offset) // 86400
			last = (until + offset - 1) // 86400
			if first <= day <= last:
				result.append((since, until, line['kind'], line['value'], line['map']))
		elif line['days'] & (1 << wd):
			start = dayStart + line['start'] * 60
			result.append((start, start + WindowLength(line) * 60, line['kind'], line['value'], line['map']))
	result.sort()
	return result


def CurrentEvents():
	"""What the mini icon shows: (key, start, end, running, icon) of the
	events running now and those starting within SOON, the running ones
	first."""
	now = ServerNow()
	if now is None:
		return []
	today = LocalDay(now)
	seen = {}
	running = []
	soon = []
	for day in (today - 1, today, today + 1):
		for start, end, kind, value, mapIndex in EventsOnDay(day):
			key = (kind, start, end, mapIndex)
			if key in seen:
				continue
			seen[key] = True
			icon = KIND_ICON.get(kind, DEFAULT_ICON)
			if start <= now < end:
				running.append((end, (key, start, end, True, icon)))
			elif 0 < start - now <= SOON:
				soon.append((start, (key, start, end, False, icon)))
	AddListedEvents(now, running)
	running.sort()
	soon.sort()
	return [entry for sortKey, entry in running] + [entry for sortKey, entry in soon]


def AddListedEvents(now, running):
	"""MT2009_PLUS_EVENT_MANAGER_V1: an event the server's list says runs
	while no window of the schedule does - a GM's flag, the classic panel's
	Easter page - on the mini icon too (ingameevent.py)."""
	try:
		import ingameevent
		import uiingameevent
	except ImportError:
		return
	shown = {}
	for sortKey, entry in running:
		kind = entry[0][0]
		if isinstance(kind, int) and 0 <= kind < len(KIND_NAMES):
			shown[KIND_NAMES[kind]] = True
	for key in ingameevent.GetActiveEvents():
		if key in shown:
			continue
		enable, start, end, rewardEnd, value = ingameevent.GetEventInfo(key)
		icon = uiingameevent.Presentation(key).get('icon', '')
		icon = icon[len(IMG):] if icon.startswith(IMG) else DEFAULT_ICON
		start = start or now
		running.append((end or now + 365 * 86400, ((key, start, end, 0), start, end, True, icon)))


def TimeText(start, end):
	"""The mod's "From: x To: y": the hours, with the day before them when
	the event does not end on the day it starts."""
	startDay, endDay = LocalDay(start), LocalDay(end)
	startMin, endMin = LocalMinute(start), LocalMinute(end)
	if endDay == startDay + 1 and endMin == 0:
		return NORMAL_EVENT_TIME % (Colorful(HHMM(startMin)), Colorful('24:00'))
	if startDay != endDay:
		s = CivilFromDays(startDay)
		e = CivilFromDays(endDay)
		return NORMAL_EVENT_TIME % (Colorful('%02d/%02d %s' % (s[2], s[1], HHMM(startMin))),
				Colorful('%02d/%02d %s' % (e[2], e[1], HHMM(endMin))))
	return NORMAL_EVENT_TIME % (Colorful(HHMM(startMin)), Colorful(HHMM(endMin)))


def _ToolTip():
	if not _data['tooltip']:
		_data['tooltip'] = uiToolTip.ToolTip()
	return _data['tooltip']


def ShowDayToolTip(day):
	tip = _ToolTip()
	tip.ClearToolTip()
	tip.toolTipWidth = tip.TOOL_TIP_WIDTH
	y, m, d = CivilFromDays(day)
	tip.AppendTextLine(TOOLTIP_TITLE % Colorful('%04d-%02d-%02d' % (y, m, d)))
	tip.AppendSpace(5)
	events = EventsOnDay(day)
	now = ServerNow()
	if events:
		for start, end, kind, value, mapIndex in events:
			tip.AppendTextLine(Colorful(KindName(kind, value)))
			if kind in WORLD_KINDS:
				tip.AppendTextLine(Colorful(SPAWN_MAPS))
				tip.AppendTextLine(Colorful(MAP_NAMES.get(mapIndex, RANDOM_MAP) if mapIndex else RANDOM_MAP))
			else:
				tip.AppendTextLine(ALL_KINGDOMS + ' ' + ALL_CHANNELS)
			tip.AppendTextLine(TimeText(start, end))
			if now is not None and start <= now < end:
				tip.AppendTextLine(RUNNING_NOW, tip.POSITIVE_COLOR)
			tip.AppendSpace(5 if kind in WORLD_KINDS else 10)
	else:
		tip.AppendTextLine(TOOLTIP_NO_EVENTS, tip.NEGATIVE_COLOR)
	tip.ShowToolTip()


def HideToolTip():
	if _data['tooltip']:
		_data['tooltip'].HideToolTip()


class IconCycler(ui.ImageBox):
	"""The mod's ImageBoxSpecial: an event icon at (6, 6) of a 57x57 tile;
	with more than one event the icon fades in, stays two seconds, fades
	out to 0.3 and the next one comes. The mod steps 0.05 twice a frame
	(its window calls the tiles' OnUpdate and the engine calls it again)."""
	WAITING_TIME = 2.0
	MIN_ALPHA = 0.3
	MAX_ALPHA = 1.0
	INCREASE_VALUE = 0.1

	def __init__(self):
		ui.ImageBox.__init__(self)
		self.icons = []
		self.imageIndex = 0
		self.alphaValue = self.MIN_ALPHA
		self.alphaStatus = False
		self.sleepTime = 0.0
		self.icon = None

	def MakeIcon(self):
		icon = ui.ImageBox()
		icon.SetParent(self)
		icon.AddFlag('not_pick')
		icon.SetPosition(6, 6)
		self.icon = icon

	def SetIcons(self, icons):
		if icons == self.icons:
			return
		self.icons = list(icons)
		self.imageIndex = 0
		if self.icons:
			self.LoadIcon(self.icons[0])
			self.icon.SetAlpha(1.0)
			self.icon.Show()
		else:
			self.icon.Hide()

	def LoadIcon(self, name):
		self.icon.LoadImage(IMG + name)
		self.alphaValue = self.MIN_ALPHA
		self.alphaStatus = False

	def NextIcon(self):
		if len(self.icons) > 1:
			self.sleepTime = 0.0
			self.alphaValue = self.MAX_ALPHA
			self.alphaStatus = True

	def OnUpdate(self):
		if len(self.icons) <= 1:
			self.imageIndex = 0
			return
		if self.sleepTime > app.GetTime():
			return
		if self.alphaStatus:
			self.alphaValue -= self.INCREASE_VALUE
			if self.alphaValue < self.MIN_ALPHA:
				self.imageIndex = (self.imageIndex + 1) % len(self.icons)
				self.LoadIcon(self.icons[self.imageIndex])
		else:
			self.alphaValue += self.INCREASE_VALUE
			if self.alphaValue > self.MAX_ALPHA:
				self.alphaStatus = True
				self.sleepTime = app.GetTime() + self.WAITING_TIME
		self.icon.SetAlpha(min(self.alphaValue, 1.0))


class DayCell(IconCycler):
	"""A day's tile: its background, its number in the corner and its
	events' icons; the tooltip lists the events."""

	def __init__(self, number):
		IconCycler.__init__(self)
		self.day = None
		self.background = 'black_bg.tga'
		self.LoadImage(IMG + self.background)
		numberLine = ui.TextLine()
		numberLine.SetParent(self)
		numberLine.AddFlag('not_pick')
		numberLine.SetPosition(8, 8)
		numberLine.SetOutline()
		numberLine.SetText(str(number))
		numberLine.Show()
		self.numberLine = numberLine
		# The icon after the number, so it draws over it as in the mod.
		self.MakeIcon()

	def SetBackground(self, name):
		if name != self.background:
			self.background = name
			self.LoadImage(IMG + name)

	def OnMouseOverIn(self):
		if self.day is not None:
			ShowDayToolTip(self.day)

	def OnMouseOverOut(self):
		HideToolTip()

	def OnMouseLeftButtonUp(self):
		pass


class EventCalendarWindow(ui.BoardWithTitleBar):
	"""The mod's window: the month on its board, eight days a row, the day
	tiles 57x57 every 66 pixels across and 62 down."""
	BOARD_WIDTH = 535
	BOARD_HEIGHT = 259
	COLUMNS = 8

	def __init__(self):
		ui.BoardWithTitleBar.__init__(self)
		self.cells = []
		self.AddFlag('movable')
		self.AddFlag('attach')

		board = ui.ImageBox()
		board.SetParent(self)
		board.AddFlag('not_pick')
		board.LoadImage(IMG + 'board.tga')
		board.SetPosition(8, 28)
		board.Show()
		self.board = board

		self.SetSize(8 + self.BOARD_WIDTH + 8, 294)
		self.SetCloseEvent(ui.__mem_func__(self.Close))

		for i in xrange(31):
			row, column = divmod(i, self.COLUMNS)
			cell = DayCell(i + 1)
			cell.SetParent(board)
			cell.SetPosition(8 + column * 66, 8 + row * 62)
			cell.Show()
			self.cells.append(cell)

		self.SetCenterPosition()
		self.Refresh()

	def Open(self):
		self.Refresh()
		self.Show()
		self.SetTop()
		Request()

	def Close(self):
		HideToolTip()
		self.Hide()

	def OnPressEscapeKey(self):
		self.Close()
		return True

	def __Today(self):
		if _data['now']:
			return LocalDay(ServerNow())
		# No answer yet: the client's own date, as the mod has it.
		t = time.localtime()
		return DaysFromCivil(t[0], t[1], t[2])

	def Refresh(self):
		today = self.__Today()
		y, m, d = CivilFromDays(today)
		self.SetTitleName(TITLE % (MONTHS[m - 1], y))
		count = DaysInMonth(y, m)
		for i, cell in enumerate(self.cells):
			if i >= count:
				cell.day = None
				cell.Hide()
				continue
			day = DaysFromCivil(y, m, i + 1)
			events = EventsOnDay(day)
			if day == today:
				cell.SetBackground('today_bg.tga')
			elif events:
				cell.SetBackground('blue_bg.tga')
			else:
				cell.SetBackground('black_bg.tga')
			icons = []
			for start, end, kind, value, mapIndex in events:
				icon = KIND_ICON.get(kind, DEFAULT_ICON)
				if icon not in icons:
					icons.append(icon)
			cell.day = day
			cell.SetIcons(icons)
			cell.Show()


class EventIcon(IconCycler):
	"""The mod's MovableImage: a movable event icon on the screen with what
	runs now ("Koniec za") or starts within half an hour ("Start za");
	a right click shows the next one, a double click opens the calendar."""

	def __init__(self):
		IconCycler.__init__(self)
		self.entries = []
		self.AddFlag('attach')
		self.AddFlag('movable')
		self.SetSize(0, 0)
		self.SetPosition(wndMgr.GetScreenWidth() - 150, 200)
		self.MakeIcon()

		timeText = ui.TextLine()
		timeText.SetParent(self)
		timeText.AddFlag('not_pick')
		timeText.SetHorizontalAlignCenter()
		timeText.SetPosition(25, 55)
		timeText.SetOutline()
		timeText.Show()
		self.timeText = timeText

		nextText = ui.TextLine()
		nextText.SetParent(self)
		nextText.AddFlag('not_pick')
		nextText.SetHorizontalAlignCenter()
		nextText.SetPosition(25, 70)
		nextText.SetText(NEXT_EVENT)
		nextText.SetOutline()
		nextText.Hide()
		self.nextText = nextText

	def SetEntries(self, entries):
		self.entries = entries
		self.SetIcons([entry[4] for entry in entries])
		if self.imageIndex >= len(entries):
			self.imageIndex = 0
		if entries:
			self.SetSize(57, 57)
			if len(entries) > 1:
				self.nextText.Show()
			else:
				self.nextText.Hide()
			self.UpdateTime()
			self.Show()
		else:
			self.timeText.SetText('')
			self.nextText.Hide()
			self.SetSize(0, 0)
			self.Hide()

	def UpdateTime(self):
		now = ServerNow()
		if now is None or self.imageIndex >= len(self.entries):
			return
		key, start, end, running, icon = self.entries[self.imageIndex]
		if running:
			text = END_IN % FormatTime(end - now) if end > now else ''
		else:
			text = START_IN % FormatTime(start - now) if start > now else ''
		self.timeText.SetText(text)

	def OnUpdate(self):
		IconCycler.OnUpdate(self)
		self.UpdateTime()

	def OnMoveWindow(self, x, y):
		screenWidth, screenHeight = wndMgr.GetScreenWidth(), wndMgr.GetScreenHeight()
		if x < 0:
			x = 0
		elif x + self.GetWidth() >= screenWidth - 70:
			x = screenWidth - 80 - self.GetWidth()
		if y < 0:
			y = 0
		elif y + self.GetHeight() >= screenHeight - 100:
			y = screenHeight - 100 - self.GetHeight()
		self.SetPosition(x, y)

	def OnMouseRightButtonDown(self):
		self.NextIcon()
		return True

	def OnMouseLeftButtonDoubleClick(self):
		ToggleWindow()

	def OnMouseLeftButtonUp(self):
		pass


class Ticker(ui.Window):
	"""Asks for the schedule every five minutes (every minute while the
	calendar is open) and moves the mini icon on every second."""

	def __init__(self):
		ui.Window.__init__(self)
		self.AddFlag('not_pick')
		self.SetSize(0, 0)
		self.nextRequest = 0.0
		self.nextCheck = 0.0

	def OnUpdate(self):
		now = app.GetTime()
		if now >= self.nextRequest:
			wnd = _data['window']
			self.nextRequest = now + (60.0 if wnd and wnd.IsShow() else 300.0)
			Request()
		if now >= self.nextCheck:
			self.nextCheck = now + 1.0
			RefreshIcon()


def RefreshIcon():
	icon = _data['icon']
	if icon:
		icon.SetEntries(CurrentEvents())


def Start():
	"""From game.py, a little after entering the world: the ticker and the
	mini icon, and the first ask."""
	if not _data['ticker']:
		ticker = Ticker()
		ticker.Show()
		_data['ticker'] = ticker
	if not _data['icon']:
		_data['icon'] = EventIcon()
		RefreshIcon()


def GetWindow():
	if not _data['window']:
		_data['window'] = EventCalendarWindow()
	return _data['window']


def ToggleWindow():
	Start()
	wnd = GetWindow()
	if wnd.IsShow():
		wnd.Close()
	else:
		wnd.Open()
		ticker = _data['ticker']
		if ticker:
			ticker.nextRequest = app.GetTime() + 60.0


def DestroyWindow():
	for name in ('window', 'icon', 'ticker', 'tooltip'):
		wnd = _data[name]
		if wnd:
			wnd.Hide()
		_data[name] = None
