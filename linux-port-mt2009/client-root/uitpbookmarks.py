# Zapisane pozycje - six teleport bookmarks (MT2009_PLUS_TP_BOOKMARKS_V1; the
# owner, 3 October: "Panel zapisywania kordow TP ... 6 pozycji. Teleportacja
# na dana pozycje po 15 minutach od poprzedniej teleportacji. Teleportacja
# zabiera jeden zwoj teleportacji z itemshopu").
#
# The Arezzo client's root/uilocation.py (25 positions, a DB table and a
# net.SendLocationManagerPacket the exe 2.0.25 does not have) made into a
# window of six rows on our chat-command protocol. The server half is the
# quest tp_bookmarks (linux-port/docker/game/quest/tp_bookmarks.quest), which
# checks everything: the map, the kingdom, the level, the 15 minutes, the
# clean ItemShop Zwoj Powrotu (22010) it takes for a teleport.
#
#   window -> "/tpzapis lista" | "/tpzapis zapisz <1-6> [name]" |
#             "/tpzapis usun <1-6>" | "/tpzapis tp <1-6>"
#   server -> "TPBM begin <clean scrolls> <cooldown left s> <cooldown s>",
#             "TPBM slot <i> <map> <local x> <local y> <name>", "TPBM end"
#             (game.py passes them here); its messages are chat lines.
#
# A name travels with "_" for a space. Opened by the inventory sidebar's
# button (uiinventory.SidebarWindow); it sends only on a click, never by
# itself, and only in the game phase (warpsafe.InGame).
#
# Python 2.7 as the client has it; the texts are CP1250 escapes.

import app
import chat
import item
import net
import ui
import uiCommon

import warpsafe

SLOT_COUNT = 6
NAME_MAX = 15
SCROLL_VNUM = 22010

# the maps the quest lets a position be saved on (its tp_bookmarks.maps)
MAP_NAMES = {
	1: "Yongan (M1)", 3: "Jayang (M2)", 4: "Jungrang (M3)",
	21: "Joan (M1)", 23: "Bokjung (M2)", 24: "Waryong (M3)",
	41: "Pyungmoo (M1)", 43: "Bakra (M2)", 44: "Imha (M3)",
	61: "G\xf3ra Sohan", 62: "Doyyumhwaji", 63: "Pustynia Yongbi",
	64: "Dolina Seungryong", 65: "\x8cwi\xb9tynia Hwang", 67: "Las Duch\xf3w Lungsam",
	68: "Czerwony Las", 69: "W\xea\xbfowe Pole", 70: "Kraina Gigant\xf3w",
	104: "Kuahklo Dong", 209: "\x8cwi\xb9tynia Ochao",
	301: "Przyl\xb9dek Smoczego Ognia", 302: "Las Porannej Mg\xb3y",
	303: "Zatoka Czarnego Piasku", 304: "G\xf3ra Grzmot\xf3w",
	360: "Dolina Cyklop\xf3w", 361: "Pustkowie Faraona", 362: "Zaczarowany Las",
}

# the name's letters the server keeps: ASCII letters and digits, "_", "-", "."
# and the Polish letters (CP1250)
NAME_CHARS = set("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-."
	"\xa5\xb9\xc6\xe6\xca\xea\xa3\xb3\xd1\xf1\xd3\xf3\x8c\x9c\x8f\x9f\xaf\xbf")

TEXT_TITLE = "Zapisane pozycje"
TEXT_EMPTY = "Pozycja niezapisana"
TEXT_SCROLLS = "Czyste zwoje (%s) w ekwipunku: %d"
TEXT_READY = "Teleportacja: gotowa"
TEXT_WAIT = "Teleportacja za: %d:%02d"
TEXT_LOADING = "Wczytywanie..."
TEXT_HINT1 = "Teleport zu\xbfywa 1 czysty zw\xf3j (bez zapisanej w nim pozycji)."
TEXT_HINT2 = "Odst\xeap mi\xeadzy teleportami: %d min. Bez loch\xf3w i map event\xf3w."
TEXT_TP = "Teleport"
TEXT_SAVE = "Zapisz"
TEXT_DEL = "Usu\xf1"
TEXT_ASK_TP1 = "Teleportowa\xe6 si\xea do: %s?"
TEXT_ASK_TP2 = "Zu\xbfyje 1 czysty zw\xf3j: %s."
TEXT_ASK_OVERWRITE = "Nadpisa\xe6 pozycj\xea %d (%s) obecnym miejscem?"
TEXT_ASK_DEL = "Usun\xb9\xe6 pozycj\xea %d (%s)?"
TEXT_INPUT = "Nazwa pozycji (do %d znak\xf3w)"
TEXT_NO_SCROLL = "Zapisane pozycje dzia\xb3aj\xb9 tylko z czystym zwojem (%s) w ekwipunku."

COLOR_NAME = 0xfff1e6c0
COLOR_EMPTY = 0xff8c8c8c
COLOR_POS = 0xffc8c8c8
COLOR_WARN = 0xffff6060
COLOR_OK = 0xff9be27c

_state = {
	'window': None,
	'slots': {},     # i -> (map, local x, local y, name with "_")
	'incoming': None,
	'scrolls': -1,   # -1 until the server answered
	'cdEnd': 0.0,
	'cdTotal': 900,
}


def ToInt(value, default=0):
	try:
		return int(value)
	except (TypeError, ValueError):
		return default


def ScrollName():
	try:
		item.SelectItem(SCROLL_VNUM)
		name = item.GetItemName()
		if name:
			return name
	except Exception:
		pass
	return "Zw\xf3j Powrotu"


def ShowName(name):
	return name.replace("_", " ")


def MapName(mapIndex):
	return MAP_NAMES.get(mapIndex, "Mapa %d" % mapIndex)


def CleanName(text):
	text = (text or "").strip().replace(" ", "_")
	return "".join([c for c in text if c in NAME_CHARS])[:NAME_MAX]


def Send(command):
	# MT2009_PLUS_SIDEKICK_WARP_SAFE_V1: only in the game phase (warpsafe.py).
	if not warpsafe.InGame():
		return False
	net.SendChatPacket("/tpzapis " + command)
	return True


def CooldownLeft():
	left = _state['cdEnd'] - app.GetTime()
	if left < 0:
		return 0
	return int(left + 0.999)


class TpBookmarksWindow(ui.BoardWithTitleBar):
	WIDTH = 470
	ROW_X = 10
	ROW_Y = 70
	ROW_W = 450
	ROW_H = 40
	ROW_GAP = 4
	BTN_W = 61

	def __init__(self):
		ui.BoardWithTitleBar.__init__(self)
		self.widgets = []
		self.rows = []
		self.dialog = None
		self.pendingSlot = 0
		self.lastSecond = -1
		self.height = self.ROW_Y + SLOT_COUNT * (self.ROW_H + self.ROW_GAP) + 40
		self.AddFlag('movable')
		self.AddFlag('float')
		self.SetSize(self.WIDTH, self.height)
		self.SetTitleName(TEXT_TITLE)
		self.SetCloseEvent(ui.__mem_func__(self.Close))
		self.__Build()
		self.SetCenterPosition()

	def __Label(self, parent, x, y, text, color=None):
		line = ui.TextLine()
		line.SetParent(parent)
		line.SetPosition(x, y)
		line.SetText(text)
		if color is not None:
			line.SetPackedFontColor(color)
		line.Show()
		self.widgets.append(line)
		return line

	def __Button(self, parent, x, y, text, event, *args):
		button = ui.Button()
		button.SetParent(parent)
		button.SetPosition(x, y)
		button.SetUpVisual('d:/ymir work/ui/public/middle_button_01.sub')
		button.SetOverVisual('d:/ymir work/ui/public/middle_button_02.sub')
		button.SetDownVisual('d:/ymir work/ui/public/middle_button_03.sub')
		button.SetText(text)
		button.SAFE_SetEvent(event, *args)
		button.Show()
		self.widgets.append(button)
		return button

	def __Build(self):
		self.scrollLine = self.__Label(self, 16, 36, TEXT_LOADING)
		self.cooldownLine = self.__Label(self, 16, 52, "")
		y = self.ROW_Y
		bx = self.ROW_W - 8 - 3 * self.BTN_W - 2 * 4
		for i in xrange(1, SLOT_COUNT + 1):
			board = ui.ThinBoard()
			board.SetParent(self)
			board.SetPosition(self.ROW_X, y)
			board.SetSize(self.ROW_W, self.ROW_H)
			board.Show()
			self.widgets.append(board)
			nameLine = self.__Label(board, 10, 5, "")
			posLine = self.__Label(board, 10, 21, "", COLOR_POS)
			tpButton = self.__Button(board, bx, 10, TEXT_TP, self.OnClickTeleport, i)
			saveButton = self.__Button(board, bx + self.BTN_W + 4, 10, TEXT_SAVE, self.OnClickSave, i)
			delButton = self.__Button(board, bx + 2 * (self.BTN_W + 4), 10, TEXT_DEL, self.OnClickDelete, i)
			self.rows.append((nameLine, posLine, tpButton, saveButton, delButton))
			y += self.ROW_H + self.ROW_GAP
		self.__Label(self, 16, y + 2, TEXT_HINT1, COLOR_POS)
		self.hintLine2 = self.__Label(self, 16, y + 17, "", COLOR_POS)

	def Refresh(self):
		scrolls = _state['scrolls']
		if scrolls < 0:
			self.scrollLine.SetText(TEXT_LOADING)
			self.scrollLine.SetPackedFontColor(COLOR_POS)
		else:
			self.scrollLine.SetText(TEXT_SCROLLS % (ScrollName(), scrolls))
			self.scrollLine.SetPackedFontColor(COLOR_OK if scrolls > 0 else COLOR_WARN)
		self.hintLine2.SetText(TEXT_HINT2 % max(1, (_state['cdTotal'] + 59) // 60))
		for i in xrange(1, SLOT_COUNT + 1):
			nameLine, posLine, tpButton, saveButton, delButton = self.rows[i - 1]
			slot = _state['slots'].get(i)
			if slot:
				mapIndex, x, y, name = slot
				nameLine.SetText("%d. %s" % (i, ShowName(name)))
				nameLine.SetPackedFontColor(COLOR_NAME)
				posLine.SetText("%s (%d, %d)" % (MapName(mapIndex), x, y))
				tpButton.Show()
				delButton.Show()
			else:
				nameLine.SetText("%d. %s" % (i, TEXT_EMPTY))
				nameLine.SetPackedFontColor(COLOR_EMPTY)
				posLine.SetText("")
				tpButton.Hide()
				delButton.Hide()
		self.lastSecond = -1
		self.__RefreshCooldown()

	def __RefreshCooldown(self):
		left = CooldownLeft()
		if left == self.lastSecond:
			return
		self.lastSecond = left
		if left > 0:
			self.cooldownLine.SetText(TEXT_WAIT % (left // 60, left % 60))
			self.cooldownLine.SetPackedFontColor(COLOR_WARN)
		else:
			self.cooldownLine.SetText(TEXT_READY)
			self.cooldownLine.SetPackedFontColor(COLOR_OK)

	def OnUpdate(self):
		self.__RefreshCooldown()

	def __SlotTitle(self, i):
		slot = _state['slots'].get(i)
		if not slot:
			return "%d" % i
		return ShowName(slot[3])

	def __CloseDialog(self):
		dialog = self.dialog
		self.dialog = None
		if dialog:
			try:
				dialog.Close()
			except Exception:
				pass

	def __NoScroll(self):
		if _state['scrolls'] == 0:
			chat.AppendChat(chat.CHAT_TYPE_INFO, TEXT_NO_SCROLL % ScrollName())
			return True
		return False

	def OnClickTeleport(self, i):
		if not _state['slots'].get(i) or self.__NoScroll():
			return
		self.__CloseDialog()
		self.pendingSlot = i
		dialog = uiCommon.QuestionDialog2()
		dialog.SetText1(TEXT_ASK_TP1 % self.__SlotTitle(i))
		dialog.SetText2(TEXT_ASK_TP2 % ScrollName())
		dialog.SetAcceptEvent(ui.__mem_func__(self.OnAcceptTeleport))
		dialog.SetCancelEvent(ui.__mem_func__(self.OnCancelDialog))
		dialog.Open()
		self.dialog = dialog

	def OnAcceptTeleport(self):
		i = self.pendingSlot
		self.__CloseDialog()
		if Send("tp %d" % i):
			self.Close()

	def OnClickSave(self, i):
		if self.__NoScroll():
			return
		self.__CloseDialog()
		self.pendingSlot = i
		if _state['slots'].get(i):
			dialog = uiCommon.QuestionDialog()
			dialog.SetText(TEXT_ASK_OVERWRITE % (i, self.__SlotTitle(i)))
			dialog.SetAcceptEvent(ui.__mem_func__(self.OnAcceptOverwrite))
			dialog.SetCancelEvent(ui.__mem_func__(self.OnCancelDialog))
			dialog.Open()
			self.dialog = dialog
		else:
			self.__OpenNameInput()

	def OnAcceptOverwrite(self):
		self.__CloseDialog()
		self.__OpenNameInput()

	def __OpenNameInput(self):
		dialog = uiCommon.InputDialog()
		dialog.SetTitle(TEXT_INPUT % NAME_MAX)
		dialog.SetMaxLength(NAME_MAX)
		dialog.SetAcceptEvent(ui.__mem_func__(self.OnAcceptName))
		dialog.SetCancelEvent(ui.__mem_func__(self.OnCancelDialog))
		dialog.Open()
		self.dialog = dialog

	def OnAcceptName(self):
		i = self.pendingSlot
		name = ""
		if self.dialog:
			try:
				name = CleanName(self.dialog.GetText())
			except Exception:
				name = ""
		self.__CloseDialog()
		if name:
			Send("zapisz %d %s" % (i, name))
		else:
			Send("zapisz %d" % i)

	def OnClickDelete(self, i):
		if not _state['slots'].get(i):
			return
		self.__CloseDialog()
		self.pendingSlot = i
		dialog = uiCommon.QuestionDialog()
		dialog.SetText(TEXT_ASK_DEL % (i, self.__SlotTitle(i)))
		dialog.SetAcceptEvent(ui.__mem_func__(self.OnAcceptDelete))
		dialog.SetCancelEvent(ui.__mem_func__(self.OnCancelDialog))
		dialog.Open()
		self.dialog = dialog

	def OnAcceptDelete(self):
		i = self.pendingSlot
		self.__CloseDialog()
		Send("usun %d" % i)

	def OnCancelDialog(self):
		self.__CloseDialog()

	def Open(self):
		self.Refresh()
		self.Show()
		self.SetTop()
		Send("lista")

	def Close(self):
		self.__CloseDialog()
		self.Hide()

	def OnPressEscapeKey(self):
		self.Close()
		return True

	def Destroy(self):
		self.__CloseDialog()
		self.Hide()
		self.rows = []
		self.widgets = []


def OnCommand(*args):
	if not args:
		return
	sub, rest = args[0], args[1:]
	try:
		if sub == 'begin':
			_state['incoming'] = {}
			_state['scrolls'] = ToInt(rest[0] if len(rest) > 0 else 0)
			_state['cdEnd'] = app.GetTime() + ToInt(rest[1] if len(rest) > 1 else 0)
			_state['cdTotal'] = ToInt(rest[2] if len(rest) > 2 else 900, 900)
		elif sub == 'slot':
			incoming = _state['incoming']
			if incoming is None or len(rest) < 5:
				return
			i = ToInt(rest[0])
			if 1 <= i <= SLOT_COUNT:
				incoming[i] = (ToInt(rest[1]), ToInt(rest[2]), ToInt(rest[3]), rest[4])
		elif sub == 'end':
			if _state['incoming'] is not None:
				_state['slots'] = _state['incoming']
				_state['incoming'] = None
			window = _state['window']
			if window is not None and window.IsShow():
				window.Refresh()
	except Exception:
		import dbg
		import sys
		dbg.TraceError('uitpbookmarks.OnCommand %s: %s' % (str(args), str(sys.exc_info()[1])))


def ToggleWindow():
	window = _state['window']
	if window is None:
		import uiminigameutil
		window = uiminigameutil.SafeCreate(TpBookmarksWindow, TEXT_TITLE)
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
	_state['incoming'] = None
	if window is not None:
		window.Destroy()
