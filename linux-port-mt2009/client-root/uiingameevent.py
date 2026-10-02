# MT2009_PLUS_EVENT_MANAGER_V1 - the in-game event hub (after Owsap's
# uiMiniGame.py, v6.2.6: the "E" button by the minimap and the list of the
# events that run, each a button that opens its window, its time left and up
# to three reward items).
#
# The list is the server's (ingameevent.py); everything shown about an event
# is here, in EVENTS - so a new event (a new scheduler kind on the server)
# needs one line in EVENTS and no new exe. A key the table does not know yet
# still shows, under its key, with the calendar behind its button.
#
# A mini game plugs in with RegisterOpener(key, func) - its button then opens
# its window; without one, the button says the game comes with an update.
#
# Built in code (no uiscript), from the client's own images; the minimap
# button takes the official "E" images (d:/ymir work/ui/minimap/e_open_*.tga,
# GF 26.1.11) once a pack carries them, the event calendar's button images
# until then. Python 2.7, CP1250 escapes for the Polish letters.

import app
import chat
import localeInfo
import net
import pack
import ui
import wndMgr

import ingameevent

CAL = 'mt2009_ui/calendar/'

# ---------------------------------------------------------------- the events
# name: the button's text (%d = the event's figure); icon: 45x45;
# open: 'goblin' (the Treasure Hunt's window), 'calendar' (F11), 'game' (a
# mini game's RegisterOpener, the message below until it has one) or 'info'
# (desc as a chat line); desc: the line under the name when the button has
# nothing to open; rewards: up to three (vnum, count) shown beside it.
EVENTS = {
	# MT2009_PLUS_RUMI_V1: the chests a game ends with (gold, silver, bronze; uiminigamerumi.py).
	'rumi': {'name': 'Rumi (Okey)', 'icon': CAL + 'bonus_event.tga', 'open': 'game',
			'rewards': ((50275, 1), (50276, 1), (50277, 1))},
	'rumi_xmas': {'name': '\x8cwi\xb9teczne Rumi (Okey)', 'icon': CAL + 'bonus_event.tga', 'open': 'game', 'game': 'rumi',
			'rewards': ((50267, 1), (50268, 1), (50269, 1))},
	# MT2009_PLUS_CATCH_KING_V1: the three King's Loots (uiminigamecatchking.py).
	'catchking': {'name': 'Z\xb3ap Kr\xf3la', 'icon': CAL + 'bonus_event.tga', 'open': 'game', 'rewards': ((50968, 1), (50969, 1), (50970, 1))},
	# MT2009_PLUS_FLOWER_V1: uiflowerevent.py registers the window; the rewards are
	# three of the five flower boxes (83023-83027, what they hold is the server's).
	'flower': {'name': 'Dzieci Kwiaty', 'icon': CAL + 'bonus_event.tga', 'open': 'game',
			'rewards': ((83023, 1), (83025, 1), (83027, 1))},
	# MT2009_PLUS_YUTNORI_V1: uiminigameyutnori.py registers the opener; the three boxes a game gives.
	'yutnori': {'name': 'Yut Nori', 'icon': CAL + 'bonus_event.tga', 'open': 'game',
			'rewards': ((83030, 1), (83031, 1), (83034, 1))},
	'easter': {'name': 'Event wielkanocny', 'icon': CAL + 'bonus_event.tga', 'open': 'info',
			'desc': 'Metiny wielkanocne dropi\xb9 jajka - wymie\xf1 je u Wielkanocnego Zaj\xb9ca w mie\x9ccie.'},
	'goblin': {'name': 'Poszukiwanie skarb\xf3w', 'icon': CAL + 'goblin_event.tga', 'open': 'goblin'},
	'chest': {'name': 'Szkatu\xb3ki Blasku Ksi\xea\xbfyca', 'icon': CAL + 'moonlight_event.tga', 'open': 'calendar'},
	'exp': {'name': 'Do\x9cwiadczenie +%d%%', 'icon': CAL + 'exp_event.tga', 'open': 'calendar'},
	'drop': {'name': 'Drop przedmiot\xf3w +%d%%', 'icon': CAL + 'item_drop_event.tga', 'open': 'calendar'},
	'yang': {'name': 'Yang z potwor\xf3w +%d%%', 'icon': CAL + 'money_drop_event.tga', 'open': 'calendar'},
	'tanaka': {'name': 'Pirat Tanaka', 'icon': CAL + 'npc_search.tga', 'open': 'calendar'},
	'zuo': {'name': 'Deszcz Metin\xf3w Zuo', 'icon': CAL + 'halloween_event.tga', 'open': 'calendar'},
	'bossloot': {'name': 'Podw\xf3jny loot z boss\xf3w', 'icon': CAL + 'double_boss_loot_event.tga', 'open': 'calendar'},
	'metinloot': {'name': 'Podw\xf3jny loot z Metin\xf3w', 'icon': CAL + 'double_metin_loot_event.tga', 'open': 'calendar'},
}
# The hub's order; keys not listed come after, as the server sends them.
ORDER = ('catchking', 'rumi', 'rumi_xmas', 'yutnori', 'flower', 'easter', 'goblin',
		'chest', 'bossloot', 'metinloot', 'tanaka', 'zuo', 'exp', 'drop', 'yang')
DEFAULT_ICON = CAL + 'bonus_event.tga'

TITLE = 'Wydarzenia w grze'
BUTTON_TOOLTIP = 'Wydarzenia w grze'
CALENDAR_BUTTON = 'Kalendarz (F11)'
TIME_LEFT = 'Pozosta\xb3o: %s'
NO_END = 'Bez limitu czasu'
FINISHED = 'Zako\xf1czone'
REWARD_LEFT = 'Odbi\xf3r nagr\xf3d: jeszcze %s'
COMING_SOON = '%s: okno tej gry pojawi si\xea z aktualizacj\xb9 klienta.'

E_BUTTON = 'd:/ymir work/ui/minimap/e_open_%s.tga'
FALLBACK_BUTTON = 'mt2009_ui/calendar_button_%02d.tga'

ROW_HEIGHT = 50
ROWS_SHOWN = 5
BOARD_WIDTH = 336
ROW_WIDTH = 300

_data = {'button': None, 'dialog': None, 'openers': {}, 'tooltip': None, 'hidden': False, 'started': False}


def Presentation(key):
	entry = EVENTS.get(key)
	if entry:
		return entry
	return {'name': key, 'icon': DEFAULT_ICON, 'open': 'calendar'}


def EventName(key):
	entry = Presentation(key)
	name = entry['name']
	if '%d' in name:
		try:
			return name % ingameevent.GetEventValue(key)
		except TypeError:
			return name.replace('%d', '').replace('%%', '%')
	return name


def ListedKeys():
	"""The events the hub shows: running ones, and those whose reward window
	is open, in ORDER."""
	keys = []
	for key in ingameevent.GetEventKeys():
		enable, start, end, rewardEnd, value = ingameevent.GetEventInfo(key)
		if enable or rewardEnd > ingameevent.GetServerTime():
			keys.append(key)

	def rank(key):
		if key in ORDER:
			return (0, ORDER.index(key))
		return (1, 0)
	keys.sort(key=rank)
	return keys


def FormatLeft(seconds):
	return localeInfo.SecondToDHM(seconds)


def TimeText(key):
	enable, start, end, rewardEnd, value = ingameevent.GetEventInfo(key)
	now = ingameevent.GetServerTime()
	if enable:
		if not end:
			return NO_END
		if end > now:
			return TIME_LEFT % FormatLeft(end - now)
		return FINISHED
	if rewardEnd > now:
		return REWARD_LEFT % FormatLeft(rewardEnd - now)
	return FINISHED


# ------------------------------------------------------------ what a click does

def RegisterOpener(key, func):
	"""A mini game's window for its event's button (its key: 'catchking',
	'rumi', 'yutnori', 'flower' ...)."""
	_data['openers'][key] = func


def UnregisterOpener(key):
	if key in _data['openers']:
		del _data['openers'][key]


def OpenEvent(key):
	entry = Presentation(key)
	how = entry.get('open', 'calendar')
	if how == 'game':
		func = _data['openers'].get(entry.get('game', key))
		if func:
			CloseDialog()
			func()
		else:
			chat.AppendChat(chat.CHAT_TYPE_INFO, COMING_SOON % EventName(key))
	elif how == 'goblin':
		CloseDialog()
		net.SendChatPacket('/goblin')
	elif how == 'info':
		chat.AppendChat(chat.CHAT_TYPE_INFO, entry.get('desc', EventName(key)))
	else:
		__import__('uieventcalendar').ToggleWindow()


# ---------------------------------------------------------------- the windows

class EventRow(ui.Window):
	"""One event: its icon, its button with the name, the time left under it
	and up to three reward items."""

	def __init__(self, key):
		ui.Window.__init__(self)
		self.key = key
		self.rewards = []
		self.SetSize(ROW_WIDTH, ROW_HEIGHT - 4)

		background = ui.ThinBoard()
		background.SetParent(self)
		background.SetSize(ROW_WIDTH, ROW_HEIGHT - 4)
		background.SetPosition(0, 0)
		background.AddFlag('not_pick')
		background.Show()
		self.background = background

		icon = ui.ImageBox()
		icon.SetParent(self)
		icon.AddFlag('not_pick')
		icon.SetPosition(3, 1)
		icon.Show()
		self.icon = icon

		button = ui.Button()
		button.SetParent(self)
		button.SetUpVisual('d:/ymir work/ui/public/xlarge_button_01.sub')
		button.SetOverVisual('d:/ymir work/ui/public/xlarge_button_02.sub')
		button.SetDownVisual('d:/ymir work/ui/public/xlarge_button_03.sub')
		button.SetPosition(52, 4)
		button.SetEvent(ui.__mem_func__(self.__OnClick))
		button.Show()
		self.button = button

		timeLine = ui.TextLine()
		timeLine.SetParent(self)
		timeLine.SetPosition(56, 28)
		timeLine.SetPackedFontColor(0xFF948F82)
		timeLine.Show()
		self.timeLine = timeLine

		slot = ui.SlotWindow()
		slot.SetParent(self)
		slot.SetPosition(ROW_WIDTH - 3 * 32 - 4, 7)
		slot.SetSize(3 * 32, 32)
		slot.SetSlotBaseImage('d:/ymir work/ui/public/Slot_Base.sub', 1.0, 1.0, 1.0, 1.0)
		for i in xrange(3):
			slot.AppendSlot(i, i * 32, 0, 32, 32)
		slot.SetOverInItemEvent(ui.__mem_func__(self.__OnOverInItem))
		slot.SetOverOutItemEvent(ui.__mem_func__(self.__OnOverOutItem))
		self.slot = slot

		self.Refresh()

	def __del__(self):
		ui.Window.__del__(self)

	def Destroy(self):
		self.button = None
		self.icon = None
		self.timeLine = None
		self.slot = None
		self.background = None

	def Refresh(self):
		entry = Presentation(self.key)
		iconName = entry.get('icon', DEFAULT_ICON)
		if not pack.Exist(iconName):
			iconName = DEFAULT_ICON
		self.icon.LoadImage(iconName)
		self.button.SetText(EventName(self.key))
		self.rewards = list(entry.get('rewards', ()))[:3]
		if self.rewards:
			for i in xrange(3):
				if i < len(self.rewards):
					vnum, count = self.rewards[i]
					self.slot.SetItemSlot(i, vnum, count)
				else:
					self.slot.ClearSlot(i)
			self.slot.RefreshSlot()
			self.slot.Show()
		else:
			self.slot.Hide()
		self.UpdateTime()

	def UpdateTime(self):
		self.timeLine.SetText(TimeText(self.key))

	def __OnClick(self):
		OpenEvent(self.key)

	def __OnOverInItem(self, slotIndex):
		tooltip = _ItemToolTip()
		if not tooltip or slotIndex >= len(self.rewards):
			return
		tooltip.ClearToolTip()
		tooltip.SetItemToolTip(self.rewards[slotIndex][0])

	def __OnOverOutItem(self):
		tooltip = _ItemToolTip()
		if tooltip:
			tooltip.HideToolTip()


class InGameEventDialog(ui.BoardWithTitleBar):
	"""The list: five rows at a time, a scroll bar past that, and the event
	calendar's button at the bottom."""

	def __init__(self):
		ui.BoardWithTitleBar.__init__(self)
		self.AddFlag('movable')
		self.AddFlag('float')
		self.SetTitleName(TITLE)
		self.SetCloseEvent(ui.__mem_func__(self.Close))
		self.rows = []
		self.keys = []
		self.first = 0
		self.nextTime = 0.0

		scroll = ui.ScrollBar()
		scroll.SetParent(self)
		scroll.SetScrollEvent(ui.__mem_func__(self.__OnScroll))
		scroll.Hide()
		self.scroll = scroll

		calendar = ui.Button()
		calendar.SetParent(self)
		calendar.SetUpVisual('d:/ymir work/ui/public/large_button_01.sub')
		calendar.SetOverVisual('d:/ymir work/ui/public/large_button_02.sub')
		calendar.SetDownVisual('d:/ymir work/ui/public/large_button_03.sub')
		calendar.SetText(CALENDAR_BUTTON)
		calendar.SetEvent(ui.__mem_func__(self.__OnCalendar))
		calendar.Show()
		self.calendar = calendar

		self.SetPosition(wndMgr.GetScreenWidth() - 136 - BOARD_WIDTH - 10, 40)
		self.Refresh()

	def __del__(self):
		ui.BoardWithTitleBar.__del__(self)

	def Destroy(self):
		for row in self.rows:
			row.Destroy()
			row.Hide()
		self.rows = []
		self.scroll = None
		self.calendar = None

	def Refresh(self):
		keys = ListedKeys()
		if keys != self.keys:
			for row in self.rows:
				row.Destroy()
				row.Hide()
			self.rows = []
			for key in keys:
				row = EventRow(key)
				row.SetParent(self)
				self.rows.append(row)
			self.keys = keys
			self.first = 0
		else:
			for row in self.rows:
				row.Refresh()
		self.__Layout()

	def __Layout(self):
		count = len(self.rows)
		shown = min(max(count, 1), ROWS_SHOWN)
		height = 34 + shown * ROW_HEIGHT + 36
		self.SetSize(BOARD_WIDTH, height)
		if count > ROWS_SHOWN:
			self.scroll.SetPosition(BOARD_WIDTH - 28, 34)
			self.scroll.SetScrollBarSize(shown * ROW_HEIGHT - 4)
			self.scroll.SetMiddleBarSize(float(ROWS_SHOWN) / count)
			self.scroll.SetScrollStep(1.0 / (count - ROWS_SHOWN))
			self.scroll.Show()
		else:
			self.first = 0
			self.scroll.Hide()
		for i, row in enumerate(self.rows):
			index = i - self.first
			if 0 <= index < ROWS_SHOWN:
				row.SetPosition(12, 34 + index * ROW_HEIGHT)
				row.Show()
			else:
				row.Hide()
		self.calendar.SetPosition(BOARD_WIDTH / 2 - 44, height - 32)

	def __OnScroll(self):
		extra = len(self.rows) - ROWS_SHOWN
		if extra <= 0:
			return
		self.first = int(round(self.scroll.GetPos() * extra))
		self.__Layout()

	def __OnCalendar(self):
		__import__('uieventcalendar').ToggleWindow()

	def Open(self):
		self.Refresh()
		self.Show()
		self.SetTop()

	def Close(self):
		tooltip = _data['tooltip']
		if tooltip:
			tooltip.HideToolTip()
		self.Hide()

	def OnPressEscapeKey(self):
		self.Close()
		return True

	def OnUpdate(self):
		now = app.GetTime()
		if now < self.nextTime:
			return
		self.nextTime = now + 1.0
		for row in self.rows:
			row.UpdateTime()


class EventButton(ui.Button):
	"""The "E" button beside the minimap: there while any event is listed,
	with their number on it; a click opens or closes the list."""

	def __init__(self):
		ui.Button.__init__(self)
		if pack.Exist(E_BUTTON % 'default'):
			self.SetUpVisual(E_BUTTON % 'default')
			self.SetOverVisual(E_BUTTON % 'over')
			self.SetDownVisual(E_BUTTON % 'down')
		else:
			self.SetUpVisual(FALLBACK_BUTTON % 1)
			self.SetOverVisual(FALLBACK_BUTTON % 2)
			self.SetDownVisual(FALLBACK_BUTTON % 3)
		self.SetToolTipText(BUTTON_TOOLTIP, 0, 34)
		self.SetEvent(ui.__mem_func__(self.__OnClick))
		self.SetPosition(wndMgr.GetScreenWidth() - 136 - self.GetWidth() - 4, 6)

		count = ui.TextLine()
		count.SetParent(self)
		count.AddFlag('not_pick')
		count.SetOutline()
		count.SetPosition(self.GetWidth() - 8, self.GetHeight() - 14)
		count.Show()
		self.count = count

	def __del__(self):
		ui.Button.__del__(self)

	def SetCount(self, number):
		self.count.SetText(str(number) if number else '')

	def __OnClick(self):
		Toggle()


# ------------------------------------------------------------ the module's face

def _ItemToolTip():
	tooltip = _data['tooltip']
	if not tooltip:
		import uiToolTip
		tooltip = uiToolTip.ItemToolTip()
		tooltip.HideToolTip()
		_data['tooltip'] = tooltip
	return tooltip


def SetItemToolTip(tooltip):
	"""The interface's item tooltip for the reward slots."""
	_data['tooltip'] = tooltip


def GetDialog():
	if not _data['dialog']:
		_data['dialog'] = InGameEventDialog()
	return _data['dialog']


def Refresh():
	"""The list changed (ingameevent's listener) - the button and the rows."""
	keys = ListedKeys()
	button = _data['button']
	if button:
		button.SetCount(len(keys))
		if keys and not _data['hidden']:
			button.Show()
		else:
			button.Hide()
	dialog = _data['dialog']
	if dialog:
		if not keys:
			dialog.Close()
		elif dialog.IsShow():
			dialog.Refresh()


def Toggle():
	dialog = GetDialog()
	if dialog.IsShow():
		dialog.Close()
	elif ListedKeys():
		dialog.Open()


def CloseDialog():
	dialog = _data['dialog']
	if dialog:
		dialog.Close()


def SetButtonHidden(hidden):
	"""The interface's HideAllWindows / ShowDefaultWindows."""
	_data['hidden'] = hidden
	if hidden:
		if _data['button']:
			_data['button'].Hide()
		CloseDialog()
	else:
		Refresh()


def Start():
	"""From game.py, a little after entering the world: the button, the
	listener and the hello to the server (ingameevent.Hello)."""
	if not _data['button']:
		_data['button'] = EventButton()
		_data['button'].Hide()
	ingameevent.AddListener(Refresh)
	ingameevent.Hello()
	_data['started'] = True
	Refresh()


def DestroyWindow():
	ingameevent.RemoveListener(Refresh)
	ingameevent.Destroy()
	dialog = _data['dialog']
	if dialog:
		dialog.Close()
		dialog.Destroy()
	button = _data['button']
	if button:
		button.Hide()
	_data['dialog'] = None
	_data['button'] = None
	_data['tooltip'] = None
	_data['started'] = False
	_data['hidden'] = False
