#
# MT2009_PLUS_FLOWER_V1 - the Flower Event ("Dzieci Kwiaty").
#
# Owsap's uiFlowerEvent.py (v6.2.6) - FlowerEventUtil and the FlowerEvent
# window - with the python names of its exe part kept as they were
# (net.SendFlowerEventRequestInfo, net.SendFlowerEventExchange,
# net.FLOWER_EVENT_SUBHEADER_GC_*, player.SHOOT_*, player.FLOWER_EVENT_*,
# player.Set/GetFlowerEventEnable; the exe calls the game window's
# FlowerEventProcess(type, data) for HEADER_GC_FLOWER_EVENT 187), plus what
# Owsap's python had elsewhere: ui.ComboBoxImage (not in our ui.py), the
# question before a flower replaces another's buff (uiinventory.py), the
# buff's icon in the affect bar, and the button in our event hub
# (uiingameevent.RegisterOpener('flower', ...)).
#
# An exe without the flower packets (2.0.25) keeps working: the module
# imports with its own constants, the hub's button says the window comes with
# a client update, and the server tells such a player about seeds in the chat.
#
# The texts are localeInfo's when the locale has Owsap's keys, else the Polish
# ones below. Python 2.7, CP1250 escapes for the Polish letters.
#

import app
import chat
import net
import player
import ui
import uiminigameutil
import wndMgr
import localeInfo

# ------------------------------------------------------------ the constants
# The exe's (Owsap's names) when it has them, these values otherwise - the
# same as the server's packet.h (server-patches/flower).


def _const(module, name, default):
	return getattr(module, name, default)


SHOOT_ENVELOPE = _const(player, 'SHOOT_ENVELOPE', 0)
SHOOT_CHRYSANTHEMUM = _const(player, 'SHOOT_CHRYSANTHEMUM', 1)
SHOOT_MAY_BELL = _const(player, 'SHOOT_MAY_BELL', 2)
SHOOT_DAFFODIL = _const(player, 'SHOOT_DAFFODIL', 3)
SHOOT_LILY = _const(player, 'SHOOT_LILY', 4)
SHOOT_SUNFLOWER = _const(player, 'SHOOT_SUNFLOWER', 5)
SHOOT_TYPE_MAX = _const(player, 'SHOOT_TYPE_MAX', 6)
EXCHANGE_COOLTIME_SEC = _const(player, 'FLOWER_EVENT_EXCHANGE_COOLTIME_SEC', 1)

CHAT_NOT_ENOUGH_SHOOT_COUNT = _const(player, 'FLOWER_EVENT_CHAT_TYPE_NOT_ENOUGH_SHOOT_COUNT', 0)
CHAT_NOT_ENOUGH_EVENTORY_SPACE = _const(player, 'FLOWER_EVENT_CHAT_TYPE_NOT_ENOUGH_EVENTORY_SPACE', 1)
CHAT_NOT_ENOUGH_SHOOT_ENVELOPE = _const(player, 'FLOWER_EVENT_CHAT_TYPE_NOT_ENOUGH_SHOOT_ENVELOPE', 2)
CHAT_GET_SHOOT_ENVELOPE = _const(player, 'FLOWER_EVENT_CHAT_TYPE_GET_SHOOT_ENVELOPE', 3)
CHAT_GET_SHOOT_CHRYSANTHEMUM = _const(player, 'FLOWER_EVENT_CHAT_TYPE_GET_SHOOT_CHRYSANTHEMUM', 4)
CHAT_GET_SHOOT_MAY_BELL = _const(player, 'FLOWER_EVENT_CHAT_TYPE_GET_SHOOT_MAY_BELL', 5)
CHAT_GET_SHOOT_DAFFODIL = _const(player, 'FLOWER_EVENT_CHAT_TYPE_GET_SHOOT_DAFFODIL', 6)
CHAT_GET_SHOOT_LILY = _const(player, 'FLOWER_EVENT_CHAT_TYPE_GET_SHOOT_LILY', 7)
CHAT_GET_SHOOT_SUNFLOWER = _const(player, 'FLOWER_EVENT_CHAT_TYPE_GET_SHOOT_SUNFLOWER', 8)
CHAT_ITEM_FULL_AND_NOT_USE = _const(player, 'FLOWER_EVENT_CHAT_TYPE_ITEM_FULL_AND_NOT_USE', 9)
CHAT_GET_SHOOT_CHRYSANTHEMUM_COUNT = _const(player, 'FLOWER_EVENT_CHAT_TYPE_GET_SHOOT_CHRYSANTHEMUM_COUNT', 10)
CHAT_GET_SHOOT_MAY_BELL_COUNT = _const(player, 'FLOWER_EVENT_CHAT_TYPE_GET_SHOOT_MAY_BELL_COUNT', 11)
CHAT_GET_SHOOT_DAFFODIL_COUNT = _const(player, 'FLOWER_EVENT_CHAT_TYPE_GET_SHOOT_DAFFODIL_COUNT', 12)
CHAT_GET_SHOOT_LILY_COUNT = _const(player, 'FLOWER_EVENT_CHAT_TYPE_GET_SHOOT_LILY_COUNT', 13)
CHAT_GET_SHOOT_SUNFLOWER_COUNT = _const(player, 'FLOWER_EVENT_CHAT_TYPE_GET_SHOOT_SUNFLOWER_COUNT', 14)
CHAT_ENVELOPE_MAX = _const(player, 'FLOWER_EVENT_CHAT_TYPE_ENVELOPE_MAX', 15)
CHAT_MAX = _const(player, 'FLOWER_EVENT_CHAT_TYPE_MAX', 16)

SUBHEADER_GC_INFO_ALL = _const(net, 'FLOWER_EVENT_SUBHEADER_GC_INFO_ALL', 0)
SUBHEADER_GC_GET_INFO = _const(net, 'FLOWER_EVENT_SUBHEADER_GC_GET_INFO', 1)
SUBHEADER_GC_UPDATE_INFO = _const(net, 'FLOWER_EVENT_SUBHEADER_GC_UPDATE_INFO', 2)

# Owsap's AFFECT_FLOWER_EVENT (chr.NEW_AFFECT_FLOWER_EVENT in its exe).
AFFECT_FLOWER_EVENT = 570
FLOWER_VNUMS = (25121, 25122, 25123, 25124, 25125)
EVENT_KEY = 'flower'

# ------------------------------------------------------------ the texts
TEXTS = {
	'FLOWER_EVENT_ENVELOPE_MAX': 'Nie mo\xbfesz ju\xbf dosta\xe6 nasion kwiat\xf3w.',
	'FLOWER_EVENT_GET_SHOOT_CHRYSANTHEMUM': 'Otrzymujesz latoro\x9cl chryzantemy.',
	'FLOWER_EVENT_GET_SHOOT_CHRYSANTHEMUM_COUNT': 'Liczba otrzymanych latoro\x9cli chryzantemy: %d.',
	'FLOWER_EVENT_GET_SHOOT_DAFFODIL': 'Otrzymujesz latoro\x9cl narcyza.',
	'FLOWER_EVENT_GET_SHOOT_DAFFODIL_COUNT': 'Liczba otrzymanych latoro\x9cli narcyza: %d.',
	'FLOWER_EVENT_GET_SHOOT_ENVELOPE': 'Otrzymujesz nasiona kwiat\xf3w.',
	'FLOWER_EVENT_GET_SHOOT_LILY': 'Otrzymujesz latoro\x9cl lilii.',
	'FLOWER_EVENT_GET_SHOOT_LILY_COUNT': 'Liczba otrzymanych latoro\x9cli lilii: %d.',
	'FLOWER_EVENT_GET_SHOOT_MAY_BELL': 'Otrzymujesz latoro\x9cl konwalii.',
	'FLOWER_EVENT_GET_SHOOT_MAY_BELL_COUNT': 'Liczba otrzymanych latoro\x9cli konwalii: %d.',
	'FLOWER_EVENT_GET_SHOOT_SUNFLOWER': 'Otrzymujesz latoro\x9cl s\xb3onecznika.',
	'FLOWER_EVENT_GET_SHOOT_SUNFLOWER_COUNT': 'Liczba otrzymanych latoro\x9cli s\xb3onecznika: %d.',
	'FLOWER_EVENT_ITEM_FULL_AND_NOT_USE': 'Nie mo\xbfesz tego wymieni\xe6 - masz ju\xbf najwi\xeaksz\xb9 liczb\xea latoro\x9cli.',
	'FLOWER_EVENT_NOT_ENOUGH_EVENTORY_SPACE': 'Masz za ma\xb3o miejsca w ekwipunku.',
	'FLOWER_EVENT_NOT_ENOUGH_SHOOT_COUNT': 'Masz za ma\xb3o latoro\x9cli tego kwiatu.',
	'FLOWER_EVENT_NOT_ENOUGH_SHOOT_ENVELOPE': 'Masz za ma\xb3o nasion kwiat\xf3w.',
	'FLOWER_EVENT_USE_ITEM_TEXT1': 'Czy chcesz wymieni\xe6 aktywny efekt kwiatu?',
	'FLOWER_EVENT_USE_ITEM_TEXT2': 'Pr\xf3ba mo\xbfe si\xea nie uda\xe6 - kwiat wtedy zwi\xeadnie.',
	'TOOLTIP_FLOWER_EVENT_SHOOT_ENVELOPE': 'Nasiona Kwiat\xf3w',
	# ours
	'FLOWER_EVENT_NEEDS_UPDATE': 'Dzieci Kwiaty: okno eventu pojawi si\xea z aktualizacj\xb9 klienta (nowy plik gry).',
	'FLOWER_EVENT_TOOLTIP_HELP': 'Nasiona daj\xb9 potwory w czasie eventu. 1 nasiono = 1 losowa latoro\x9cl, 10 latoro\x9cli kwiatu = nagroda.',
}


def Text(key):
	return getattr(localeInfo, key, TEXTS.get(key, key))


# The buff in the affect bar (uiaffectbar.py): its point is the flower.
AFFECT_SHOW = {
	'icon': 'icon/item/25121.tga',
	40: {'icon': 'icon/item/25121.tga', 'description': 'Chryzantema: szansa na cios krytyczny +%d%%'},
	114: {'icon': 'icon/item/25122.tga', 'description': 'Konwalia: si\xb3a ataku +%d%%'},
	83: {'icon': 'icon/item/25123.tga', 'description': 'Narcyz: szansa na podw\xf3jne do\x9cwiadczenie +%d%%'},
	117: {'icon': 'icon/item/25124.tga', 'description': 'Lilia: szansa na przedmioty +%d%%'},
	115: {'icon': 'icon/item/25125.tga', 'description': 'S\xb3onecznik: obrona +%d%%'},
}

_data = {
	'window': None,
	'counts': [0] * 6,
	'requested': False,
	'question': None,
}


def HasExe():
	"""The exe has the flower packets (a new exe, client-patches/exe)."""
	return hasattr(net, 'SendFlowerEventRequestInfo') and hasattr(net, 'SendFlowerEventExchange')


# ------------------------------------------------------------ Owsap's helper
class FlowerEventUtil:
	FLOWER_EVENT_LOCA = {}

	GET_SHOOT_COUNT_MESSAGE_TYPE = {
		SHOOT_CHRYSANTHEMUM: CHAT_GET_SHOOT_CHRYSANTHEMUM_COUNT,
		SHOOT_MAY_BELL: CHAT_GET_SHOOT_MAY_BELL_COUNT,
		SHOOT_DAFFODIL: CHAT_GET_SHOOT_DAFFODIL_COUNT,
		SHOOT_LILY: CHAT_GET_SHOOT_LILY_COUNT,
		SHOOT_SUNFLOWER: CHAT_GET_SHOOT_SUNFLOWER_COUNT,
	}

	GET_SHOOT_MESSAGE_TYPE = {
		SHOOT_ENVELOPE: CHAT_GET_SHOOT_ENVELOPE,
		SHOOT_CHRYSANTHEMUM: CHAT_GET_SHOOT_CHRYSANTHEMUM,
		SHOOT_MAY_BELL: CHAT_GET_SHOOT_MAY_BELL,
		SHOOT_DAFFODIL: CHAT_GET_SHOOT_DAFFODIL,
		SHOOT_LILY: CHAT_GET_SHOOT_LILY,
		SHOOT_SUNFLOWER: CHAT_GET_SHOOT_SUNFLOWER,
	}

	EXCHANGE_COUNT_TYPE_TEXT = ('1', '10', '50', '100')

	@classmethod
	def InitializeLoca(cls):
		cls.FLOWER_EVENT_LOCA = {
			CHAT_NOT_ENOUGH_SHOOT_COUNT: Text('FLOWER_EVENT_NOT_ENOUGH_SHOOT_COUNT'),
			CHAT_NOT_ENOUGH_EVENTORY_SPACE: Text('FLOWER_EVENT_NOT_ENOUGH_EVENTORY_SPACE'),
			CHAT_NOT_ENOUGH_SHOOT_ENVELOPE: Text('FLOWER_EVENT_NOT_ENOUGH_SHOOT_ENVELOPE'),
			CHAT_GET_SHOOT_ENVELOPE: Text('FLOWER_EVENT_GET_SHOOT_ENVELOPE'),
			CHAT_GET_SHOOT_CHRYSANTHEMUM: Text('FLOWER_EVENT_GET_SHOOT_CHRYSANTHEMUM'),
			CHAT_GET_SHOOT_MAY_BELL: Text('FLOWER_EVENT_GET_SHOOT_MAY_BELL'),
			CHAT_GET_SHOOT_DAFFODIL: Text('FLOWER_EVENT_GET_SHOOT_DAFFODIL'),
			CHAT_GET_SHOOT_LILY: Text('FLOWER_EVENT_GET_SHOOT_LILY'),
			CHAT_GET_SHOOT_SUNFLOWER: Text('FLOWER_EVENT_GET_SHOOT_SUNFLOWER'),
			CHAT_ITEM_FULL_AND_NOT_USE: Text('FLOWER_EVENT_ITEM_FULL_AND_NOT_USE'),
			CHAT_GET_SHOOT_CHRYSANTHEMUM_COUNT: Text('FLOWER_EVENT_GET_SHOOT_CHRYSANTHEMUM_COUNT'),
			CHAT_GET_SHOOT_MAY_BELL_COUNT: Text('FLOWER_EVENT_GET_SHOOT_MAY_BELL_COUNT'),
			CHAT_GET_SHOOT_DAFFODIL_COUNT: Text('FLOWER_EVENT_GET_SHOOT_DAFFODIL_COUNT'),
			CHAT_GET_SHOOT_LILY_COUNT: Text('FLOWER_EVENT_GET_SHOOT_LILY_COUNT'),
			CHAT_GET_SHOOT_SUNFLOWER_COUNT: Text('FLOWER_EVENT_GET_SHOOT_SUNFLOWER_COUNT'),
			CHAT_ENVELOPE_MAX: Text('FLOWER_EVENT_ENVELOPE_MAX'),
		}

	@classmethod
	def IsFlowerEventEnd(cls):
		if hasattr(player, 'GetFlowerEventEnable'):
			return player.GetFlowerEventEnable()
		return 0

	@classmethod
	def FlowerEventGetItemMessage(cls, shoot_type, shoot_count):
		if shoot_type in cls.GET_SHOOT_COUNT_MESSAGE_TYPE and shoot_count > 1:
			cls.FlowerEventMessage(cls.GET_SHOOT_COUNT_MESSAGE_TYPE[shoot_type], shoot_count)
		elif shoot_type in cls.GET_SHOOT_MESSAGE_TYPE:
			cls.FlowerEventMessage(cls.GET_SHOOT_MESSAGE_TYPE[shoot_type])

	@classmethod
	def FlowerEventMessage(cls, chat_type, data=None):
		if not cls.FLOWER_EVENT_LOCA:
			cls.InitializeLoca()
		if chat_type in cls.FLOWER_EVENT_LOCA:
			message = cls.FLOWER_EVENT_LOCA[chat_type]
			if data is not None:
				try:
					message = message % data
				except TypeError:
					pass
			chat.AppendChat(chat.CHAT_TYPE_INFO, message)


# ------------------------------------------------------------ Owsap's combo box
class ComboBoxImage(ui.Window):
	"""Owsap's ui.ComboBoxImage (v6.2.6, ui.py), which our ui.py lacks: an
	image with the current choice and a list under it."""

	class ListBoxWithBoard(ui.ListBox):
		def __init__(self, layer):
			ui.ListBox.__init__(self, layer)
			if hasattr(self, 'SetTextIgnoreAlign'):
				self.SetTextIgnoreAlign(True)

		def OnRender(self):
			import grp
			xRender, yRender = self.GetGlobalPosition()
			yRender -= self.TEMPORARY_PLACE
			widthRender = self.width
			heightRender = self.height + self.TEMPORARY_PLACE * 2
			grp.SetColor(ui.BACKGROUND_COLOR)
			grp.RenderBar(xRender, yRender, widthRender, heightRender)
			grp.SetColor(ui.DARK_COLOR)
			grp.RenderLine(xRender, yRender, widthRender, 0)
			grp.RenderLine(xRender, yRender, 0, heightRender)
			ui.ListBox.OnRender(self)

	def __init__(self, parent, name, x, y):
		ui.Window.__init__(self)
		self.isSelected = False
		self.isOver = False
		self.isListOpened = False
		self.event = lambda *arg: None
		self.enable = True

		image = ui.ImageBox()
		image.SetParent(parent)
		image.LoadImage(name)
		image.SetPosition(x, y)
		image.Show()
		self.imagebox = image

		self.x = x + 1
		self.y = y + 1
		self.width = self.imagebox.GetWidth() - 3
		self.height = self.imagebox.GetHeight() - 3
		self.SetParent(parent)

		self.textLine = ui.MakeTextLine(self)
		self.textLine.SetText('')

		self.listBox = self.ListBoxWithBoard('TOP_MOST')
		self.listBox.SetPickAlways()
		self.listBox.SetParent(self)
		self.listBox.SetEvent(ui.__mem_func__(self.OnSelectItem))
		self.listBox.Hide()

		ui.Window.SetPosition(self, self.x, self.y)
		ui.Window.SetSize(self, self.width, self.height)
		self.textLine.UpdateRect()
		self.__ArrangeListBox()

	def __del__(self):
		ui.Window.__del__(self)

	def Destroy(self):
		self.textLine = None
		self.listBox = None
		self.imagebox = None
		self.event = lambda *arg: None

	def __ArrangeListBox(self):
		self.listBox.SetPosition(0, self.height + 5)
		self.listBox.SetWidth(self.width)

	def SetEvent(self, event):
		self.event = event

	def InsertItem(self, index, name):
		self.listBox.InsertItem(index, name)
		self.listBox.ArrangeItem()

	def SetCurrentItem(self, text):
		self.textLine.SetText(text)

	def OnSelectItem(self, index, name):
		self.CloseListBox()
		self.event(index)

	def CloseListBox(self):
		self.isListOpened = False
		self.listBox.Hide()

	def OnMouseLeftButtonDown(self):
		if self.enable:
			self.isSelected = True

	def OnMouseLeftButtonUp(self):
		self.ToggleOpenCloseListBox()

	def OnUpdate(self):
		if self.enable:
			self.isOver = self.IsIn()

	def OnRender(self):
		import grp
		xRender, yRender = self.GetGlobalPosition()
		if self.isOver:
			grp.SetColor(ui.HALF_WHITE_COLOR)
			grp.RenderBar(xRender + 2, yRender + 3, self.width - 3, self.height - 5)
			if self.isSelected:
				grp.SetColor(ui.WHITE_COLOR)
				grp.RenderBar(xRender + 2, yRender + 3, self.width - 3, self.height - 5)

	def ToggleOpenCloseListBox(self):
		if not self.enable:
			return
		self.isSelected = False
		if self.isListOpened:
			self.CloseListBox()
		elif self.listBox.GetItemCount() > 0:
			self.isListOpened = True
			self.listBox.Show()
			self.__ArrangeListBox()


# ------------------------------------------------------------ Owsap's window
class FlowerEvent(ui.ScriptWindow):
	def __init__(self):
		ui.ScriptWindow.__init__(self)
		self.Initialize()
		self.LoadWindow()

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def Initialize(self):
		self.shoot_list_exchange_button = [None] * SHOOT_TYPE_MAX
		self.shoot_list_count_text = [None] * SHOOT_TYPE_MAX
		self.exchange_window_drop_down = None
		self.exchange_button_drop_down = None
		self.exchange_count_drop_down = 0
		self.is_data_requested = False
		self.last_exchange_time = 0.0
		self.tooltip = None

	def LoadWindow(self):
		try:
			pyScrLoader = ui.PythonScriptLoader()
			pyScrLoader.LoadScriptFile(self, "UIScript/FlowerEventWindow.py")
		except:
			uiminigameutil.LoadError("FlowerEvent.LoadWindow.LoadScript")

		try:
			self.board = self.GetChild("board")
			self.slot_image = self.GetChild("slot_image")

			self.shoot_list_exchange_button[SHOOT_ENVELOPE] = self.GetChild("main_exchange_button")
			self.shoot_list_exchange_button[SHOOT_CHRYSANTHEMUM] = self.GetChild("shoot_list_exchange_button_1")
			self.shoot_list_exchange_button[SHOOT_MAY_BELL] = self.GetChild("shoot_list_exchange_button_2")
			self.shoot_list_exchange_button[SHOOT_DAFFODIL] = self.GetChild("shoot_list_exchange_button_3")
			self.shoot_list_exchange_button[SHOOT_LILY] = self.GetChild("shoot_list_exchange_button_4")
			self.shoot_list_exchange_button[SHOOT_SUNFLOWER] = self.GetChild("shoot_list_exchange_button_5")

			self.shoot_list_count_text[SHOOT_ENVELOPE] = self.GetChild("main_shoot_count_text")
			self.shoot_list_count_text[SHOOT_CHRYSANTHEMUM] = self.GetChild("shoot_count_text_1")
			self.shoot_list_count_text[SHOOT_MAY_BELL] = self.GetChild("shoot_count_text_2")
			self.shoot_list_count_text[SHOOT_DAFFODIL] = self.GetChild("shoot_count_text_3")
			self.shoot_list_count_text[SHOOT_LILY] = self.GetChild("shoot_count_text_4")
			self.shoot_list_count_text[SHOOT_SUNFLOWER] = self.GetChild("shoot_count_text_5")
		except:
			uiminigameutil.LoadError("FlowerEvent.LoadWindow.BindObject")

		self.board.SetCloseEvent(ui.__mem_func__(self.Close))

		# Our ImageBox: the string events (Owsap's SetEvent(func, "mouse_over_in")
		# would hand the key to the function as an argument here).
		self.slot_image.SAFE_SetStringEvent("MOUSE_OVER_IN", self.__ImgOverIn)
		self.slot_image.SAFE_SetStringEvent("MOUSE_OVER_OUT", self.__ImgOverOut)

		for shoot_type in xrange(SHOOT_TYPE_MAX):
			self.shoot_list_exchange_button[shoot_type].SetEvent(ui.__mem_func__(self.__OnClickListExchangeButton), shoot_type)

		import uiToolTip
		self.tooltip = uiToolTip.ToolTip()
		self.tooltip.ClearToolTip()

		# The count's box: Owsap's cheque_slot.sub lies at rows 506-524 of
		# public.dds (256x524) and our exe samples the atlas as 512 rows high,
		# so it drew the red slot bar of row 0 (and the text looked too high).
		# parameter_slot_00.sub is the same black box (39x18) in rows 232-250;
		# 3 px to the left, so it still ends where the arrow button begins.
		self.exchange_window_drop_down = ComboBoxImage(self, "d:/ymir work/ui/public/parameter_slot_00.sub", 211, 91)
		self.exchange_window_drop_down.SetEvent(lambda key, parent=self: parent.ExchangeCountDropDown(key))
		for key, value in enumerate(FlowerEventUtil.EXCHANGE_COUNT_TYPE_TEXT):
			self.exchange_window_drop_down.InsertItem(key, value)
		self.exchange_window_drop_down.SetCurrentItem(FlowerEventUtil.EXCHANGE_COUNT_TYPE_TEXT[0])
		self.exchange_window_drop_down.Show()

		self.exchange_button_drop_down = ui.Button()
		self.exchange_button_drop_down.SetParent(self)
		self.exchange_button_drop_down.SetPosition(250, 93)
		self.exchange_button_drop_down.SetUpVisual("d:/ymir work/ui/minigame/flower_event/drop_down_btn_default.sub")
		self.exchange_button_drop_down.SetOverVisual("d:/ymir work/ui/minigame/flower_event/drop_down_btn_over.sub")
		self.exchange_button_drop_down.SetDownVisual("d:/ymir work/ui/minigame/flower_event/drop_down_btn_down.sub")
		self.exchange_button_drop_down.SetEvent(ui.__mem_func__(self.exchange_window_drop_down.ToggleOpenCloseListBox))
		self.exchange_button_drop_down.Show()

		for shoot_type in xrange(SHOOT_TYPE_MAX):
			self.__SetFlowerEventInfo(shoot_type, _data['counts'][shoot_type])

	def Open(self):
		# Every time: the counters may have changed in another window of the
		# game (a warp is a new game window anyway).
		if HasExe():
			net.SendFlowerEventRequestInfo()
		self.is_data_requested = True
		ui.ScriptWindow.Show(self)
		self.SetTop()

	def Close(self):
		if self.exchange_window_drop_down:
			self.exchange_window_drop_down.CloseListBox()
		self.Hide()
		if self.tooltip:
			self.tooltip.Hide()

	def Destroy(self):
		self.Close()
		self.ClearDictionary()
		if self.exchange_window_drop_down:
			self.exchange_window_drop_down.Destroy()
		self.board = None
		self.slot_image = None
		self.shoot_list_count_text = []
		self.shoot_list_exchange_button = []
		self.exchange_window_drop_down = None
		self.exchange_button_drop_down = None
		self.exchange_count_drop_down = 0
		self.is_data_requested = False
		self.last_exchange_time = 0.0
		self.tooltip = None

	def OnPressEscapeKey(self):
		self.Close()
		return True

	def ExchangeCountDropDown(self, key):
		if 0 <= key < len(FlowerEventUtil.EXCHANGE_COUNT_TYPE_TEXT):
			self.exchange_window_drop_down.SetCurrentItem(FlowerEventUtil.EXCHANGE_COUNT_TYPE_TEXT[key])
			self.exchange_count_drop_down = key

	def FlowerEventProcess(self, type, data):
		if type == SUBHEADER_GC_INFO_ALL:
			for shoot_type, shoot_count in enumerate(data):
				self.__SetFlowerEventInfo(shoot_type, shoot_count)
		elif type == SUBHEADER_GC_UPDATE_INFO:
			self.__SetFlowerEventInfo(*data)

	def __SetFlowerEventInfo(self, shoot_type, shoot_count):
		if self.shoot_list_count_text and 0 <= shoot_type < len(self.shoot_list_count_text):
			self.shoot_list_count_text[shoot_type].SetText(str(shoot_count))

	def __OnClickListExchangeButton(self, shoot_type):
		if not HasExe():
			return
		if app.GetTime() - self.last_exchange_time < float(EXCHANGE_COOLTIME_SEC):
			return
		self.last_exchange_time = app.GetTime()
		net.SendFlowerEventExchange(shoot_type, self.exchange_count_drop_down)

	def __ImgOverIn(self):
		if self.tooltip:
			self.tooltip.ClearToolTip()
			self.tooltip.AppendTextLine(Text('TOOLTIP_FLOWER_EVENT_SHOOT_ENVELOPE'))
			self.tooltip.AppendTextLine(Text('FLOWER_EVENT_TOOLTIP_HELP'))
			self.tooltip.Show()

	def __ImgOverOut(self):
		if self.tooltip:
			self.tooltip.Hide()


# ------------------------------------------------------------ the module's face

def Process(type, data=None):
	"""game.FlowerEventProcess: the exe's HEADER_GC_FLOWER_EVENT (Owsap's
	uiMiniGame.FlowerEventProcess)."""
	if type == SUBHEADER_GC_INFO_ALL and data:
		for shoot_type, shoot_count in enumerate(data):
			if 0 <= shoot_type < len(_data['counts']):
				_data['counts'][shoot_type] = shoot_count
	elif type == SUBHEADER_GC_UPDATE_INFO and data:
		shoot_type, shoot_count = data
		if 0 <= shoot_type < len(_data['counts']):
			_data['counts'][shoot_type] = shoot_count
	elif type == SUBHEADER_GC_GET_INFO:
		if isinstance(data, tuple):
			FlowerEventUtil.FlowerEventGetItemMessage(*data)
		elif data is not None:
			FlowerEventUtil.FlowerEventMessage(data)

	window = _data['window']
	if window:
		window.FlowerEventProcess(type, data)


def Open():
	"""The hub's button (uiingameevent.RegisterOpener)."""
	if not HasExe():
		chat.AppendChat(chat.CHAT_TYPE_INFO, Text('FLOWER_EVENT_NEEDS_UPDATE'))
		return
	window = _data['window']
	if not window:
		window = uiminigameutil.SafeCreate(FlowerEvent, "Dzieci Kwiaty")
		if not window:
			return
		_data['window'] = window
	if window.IsShow():
		window.Close()
	else:
		window.Open()


def SetEnable(value):
	"""Owsap's "e_flower_drop <value>" (game.py)."""
	try:
		value = int(value)
	except (TypeError, ValueError):
		return
	if hasattr(player, 'SetFlowerEventEnable'):
		player.SetFlowerEventEnable(value)


def RegisterAffect():
	"""The flower buff's icon and text in the affect bar."""
	try:
		import uiaffectbar
	except ImportError:
		return
	data = getattr(uiaffectbar, 'AFFECT_SHOW_DATA', None)
	if isinstance(data, dict) and AFFECT_FLOWER_EVENT not in data:
		data[AFFECT_FLOWER_EVENT] = AFFECT_SHOW


def Start():
	"""From game.py, a little after entering the world (with the event hub):
	the hub's button, the affect icon, and with the new exe the counters - the
	first CG packet also tells the server this exe takes HEADER_GC_FLOWER_EVENT."""
	RegisterAffect()
	FlowerEventUtil.InitializeLoca()
	if HasExe():
		try:
			import uiingameevent
			uiingameevent.RegisterOpener(EVENT_KEY, Open)
		except ImportError:
			pass
		net.SendFlowerEventRequestInfo()
		_data['requested'] = True


def DestroyWindow():
	window = _data['window']
	if window:
		window.Destroy()
		window.Hide()
	_data['window'] = None
	_data['requested'] = False
	_data['counts'] = [0] * 6
	question = _data['question']
	if question:
		question.Close()
	_data['question'] = None
	try:
		import uiingameevent
		uiingameevent.UnregisterOpener(EVENT_KEY)
	except ImportError:
		pass


# ------------------------------------------------------------ using a flower

def _ActiveFlowerPoints():
	try:
		import constInfo
		return constInfo.AFFECT_DICT.get(AFFECT_FLOWER_EVENT, {}).keys()
	except Exception:
		return []


def _SendUse(slotIndex):
	try:
		import uiPrivateShopBuilder
		if uiPrivateShopBuilder.IsBuildingPrivateShop():
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.USE_ITEM_FAILURE_PRIVATE_SHOP)
			return
	except ImportError:
		pass
	net.SendItemUsePacket(slotIndex)


def _CloseQuestion():
	question = _data['question']
	if question:
		question.Close()
	_data['question'] = None
	try:
		import constInfo
		constInfo.SET_ITEM_QUESTION_DIALOG_STATUS(0)
	except Exception:
		pass


def ConfirmFlowerUse(slotIndex, itemVnum):
	"""uiinventory.__UseItem: Owsap's question before a flower replaces the
	buff of another flower. True - the question is open (the use waits for
	it); False - the inventory uses the item as always."""
	if itemVnum not in FLOWER_VNUMS:
		return False
	import item
	item.SelectItem(itemVnum)
	point = item.GetValue(1)
	active = _ActiveFlowerPoints()
	if not active or point in active:
		return False
	import uiCommon
	question = uiCommon.QuestionDialog2()
	question.SetText1(Text('FLOWER_EVENT_USE_ITEM_TEXT1'))
	question.SetText2(Text('FLOWER_EVENT_USE_ITEM_TEXT2'))

	def accept(slot=slotIndex):
		_CloseQuestion()
		_SendUse(slot)

	question.SetAcceptEvent(accept)
	question.SetCancelEvent(_CloseQuestion)
	question.Open()
	_data['question'] = question
	try:
		import constInfo
		constInfo.SET_ITEM_QUESTION_DIALOG_STATUS(1)
	except Exception:
		pass
	return True
