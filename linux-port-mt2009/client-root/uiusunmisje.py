# Usun misje - the /usunmisje window (MT2009_PLUS_CLEAR_MISSIONS_V1; the
# owner, 3 October: "gracz ... wpisuje komende /usunmisje. Ale tak, aby gracz
# mogl jakos zaznaczyc, ktore misje usuwa, a ktorych nie ... nagrody nie sa
# przyznawane").
#
# The server half is the quest usun_misje
# (linux-port/docker/game/quest/usun_misje.quest): it lists the missions the
# player has open now (the story, side quests, the Biologist, Baek-Go's herbs
# - never the Companion or Cor Draconis), sets the checked ones to their
# finished state without a reward and takes their letters, letter buttons and
# arrows off at once (pc.clear_quest_letter, server-patches/clearmissions) -
# no warp, no loading screen.
#
#   chat "/usunmisje" (the player) or "/usunmisje lista" -> the list
#   server -> "MISJE begin <0 asked | 1 after a removal>",
#             "MISJE m <id> <category> <level> <letter text, _ for a space>",
#             "MISJE end", "MISJE gone <quest index>" - the removed quest's
#             letter button goes (the interface's BINARY_ClearQuest)
#             (game.py passes them here, with its interface)
#   window -> "/usunmisje usun <id> ..." (up to 20 ids a line), then
#             "/usunmisje gotowe"
#
# After a removal the server sends the list again (mode 1): the window opens
# only for missions that were not in the last window - the next part of a
# chain the player's level already opens - never for the ones he kept.
# It sends only on a click, never by itself, and only in the game phase
# (warpsafe.InGame).
#
# Python 2.7 as the client has it; the texts are CP1250 escapes.

import chat
import net
import ui
import uiCommon

import warpsafe

ROWS = 12
IDS_PER_LINE = 20

CHECKBOX_IMAGE = "mt2009_ui/checkbox/checkbox.tga"
CHECKED_IMAGE = "mt2009_ui/checkbox/checked.tga"

TEXT_TITLE = "Usu\xf1 misje"
TEXT_INFO1 = "Zaznaczone misje znikn\xb9 z listy tak, jakby ich nigdy nie by\xb3o."
TEXT_INFO2 = "Nagrody nie s\xb9 przyznawane. Nowe misje pojawi\xb9 si\xea z kolejnym poziomem."
TEXT_INFO3 = "Towarzysz, Cor Draconis i misje system\xf3w nie s\xb9 tu pokazywane."
TEXT_AFTER = "Pojawi\xb3y si\xea kolejne misje (np. nast\xeapna cz\xea\x9c\xe6 \xb3a\xf1cucha):"
TEXT_PAGE = "Strona %d / %d"
TEXT_COUNT = "Zaznaczone: %d z %d"
TEXT_ALL = "Zaznacz wszystkie"
TEXT_NONE = "Odznacz wszystkie"
TEXT_REMOVE = "Usu\xf1 zaznaczone"
TEXT_CANCEL = "Anuluj"
TEXT_ASK1 = "Usun\xb9\xe6 zaznaczone misje (%d)?"
TEXT_ASK2 = "Bez nagr\xf3d - tego nie mo\xbfna cofn\xb9\xe6."
TEXT_NOTHING = "Nie masz teraz misji, kt\xf3re mo\xbfna usun\xb9\xe6."
TEXT_NONE_CHECKED = "Nie zaznaczono \xbfadnej misji."

TEXT_BIO_ASK1 = "Czy na pewno chcesz usun\xb9\xe6 misj\xea Biologa? (zaznaczone: %d)"
TEXT_BIO_ASK2 = "Biolog daje sta\xb3e bonusy, a usuni\xeatego kroku nie da si\xea cofn\xb9\xe6."
CATEGORY = {
	1: "Fabu\xb3a",
	2: "Poboczna",
	3: "Biolog",
	4: "Zio\xb3a",
}

COLOR_INFO = 0xffc8c8c8
COLOR_NOTE = 0xff9be27c
COLOR_CAT = 0xfff1e6c0

_state = {
	'window': None,
	'incoming': None,
	'mode': 0,
	'kept': set(),    # ids the player left unchecked in the last removal
	'shown': set(),   # ids of the last window
	'interface': None,
}


def SetInterface(interface):
	try:
		from _weakref import proxy
		_state['interface'] = proxy(interface)
	except Exception:
		_state['interface'] = None


def ToInt(value, default=0):
	try:
		return int(value)
	except (TypeError, ValueError):
		return default


def Send(command):
	# MT2009_PLUS_SIDEKICK_WARP_SAFE_V1: only in the game phase (warpsafe.py).
	if not warpsafe.InGame():
		return False
	net.SendChatPacket("/usunmisje " + command)
	return True


def RowText(mission):
	mid, cat, level, title = mission
	label = CATEGORY.get(cat, "")
	if level > 0:
		label = "%s, poz. %d" % (label, level)
	return "[%s] %s" % (label, title.replace("_", " "))


class MissionRow(ui.Window):
	def __init__(self, owner):
		ui.Window.__init__(self)
		self.owner = owner
		self.mid = 0
		self.textMark = None
		self.box = None
		self.mark = None
		try:
			box = ui.ImageBox()
			box.SetParent(self)
			box.AddFlag("not_pick")
			box.LoadImage(CHECKBOX_IMAGE)
			box.SetPosition(0, 3)
			box.Show()
			mark = ui.ImageBox()
			mark.SetParent(self)
			mark.AddFlag("not_pick")
			mark.LoadImage(CHECKED_IMAGE)
			mark.SetPosition(0, -1)
			self.box = box
			self.mark = mark
		except Exception:
			# No picture: a text mark, so a missing file never stops the window.
			self.box = self.mark = None
			textMark = ui.TextLine()
			textMark.SetParent(self)
			textMark.AddFlag("not_pick")
			textMark.SetPosition(0, 1)
			textMark.SetText("[ ]")
			textMark.SetOutline()
			textMark.Show()
			self.textMark = textMark
		label = ui.TextLine()
		label.SetParent(self)
		label.AddFlag("not_pick")
		label.SetPosition(20, 1)
		label.SetOutline()
		label.Show()
		self.label = label

	def __del__(self):
		ui.Window.__del__(self)

	def SetMission(self, mission, checked):
		self.mid = mission[0]
		self.label.SetText(RowText(mission))
		self.SetChecked(checked)

	def SetChecked(self, checked):
		if self.textMark:
			self.textMark.SetText("[x]" if checked else "[ ]")
		elif self.mark:
			if checked:
				self.mark.Show()
			else:
				self.mark.Hide()

	def OnMouseLeftButtonUp(self):
		if self.owner and self.mid:
			self.owner.Toggle(self.mid)
		return True

	def Destroy(self):
		self.owner = None


class ClearMissionsWindow(ui.BoardWithTitleBar):
	WIDTH = 460
	TOP = 34
	ROW_H = 20

	def __init__(self):
		ui.BoardWithTitleBar.__init__(self)
		self.widgets = []
		self.rows = []
		self.missions = []
		self.checked = set()
		self.page = 0
		self.dialog = None
		self.AddFlag('movable')
		self.AddFlag('float')
		self.SetTitleName(TEXT_TITLE)
		self.SetCloseEvent(ui.__mem_func__(self.Close))
		self.__Build()
		self.SetCenterPosition()

	def __Label(self, x, y, text, color=None):
		line = ui.TextLine()
		line.SetParent(self)
		line.SetPosition(x, y)
		line.SetText(text)
		if color is not None:
			line.SetPackedFontColor(color)
		line.Show()
		self.widgets.append(line)
		return line

	def __Button(self, x, y, text, event, width=0):
		button = ui.Button()
		button.SetParent(self)
		button.SetPosition(x, y)
		if width > 70:
			button.SetUpVisual('d:/ymir work/ui/public/large_button_01.sub')
			button.SetOverVisual('d:/ymir work/ui/public/large_button_02.sub')
			button.SetDownVisual('d:/ymir work/ui/public/large_button_03.sub')
		else:
			button.SetUpVisual('d:/ymir work/ui/public/middle_button_01.sub')
			button.SetOverVisual('d:/ymir work/ui/public/middle_button_02.sub')
			button.SetDownVisual('d:/ymir work/ui/public/middle_button_03.sub')
		button.SetText(text)
		button.SAFE_SetEvent(event)
		button.Show()
		self.widgets.append(button)
		return button

	def __Build(self):
		y = self.TOP
		self.__Label(16, y, TEXT_INFO1, COLOR_INFO)
		self.__Label(16, y + 15, TEXT_INFO2, COLOR_INFO)
		self.__Label(16, y + 30, TEXT_INFO3, COLOR_INFO)
		self.noteLine = self.__Label(16, y + 48, "", COLOR_NOTE)
		y += 66
		board = ui.ThinBoard()
		board.SetParent(self)
		board.SetPosition(10, y)
		board.SetSize(self.WIDTH - 20, ROWS * self.ROW_H + 12)
		board.Show()
		self.widgets.append(board)
		for i in xrange(ROWS):
			row = MissionRow(self)
			row.SetParent(self)
			row.SetPosition(20, y + 7 + i * self.ROW_H)
			row.SetSize(self.WIDTH - 40, self.ROW_H)
			self.rows.append(row)
		y += ROWS * self.ROW_H + 18
		self.prevButton = self.__Button(16, y, "<", self.OnPrev)
		self.pageLine = self.__Label(84, y + 3, "")
		self.nextButton = self.__Button(160, y, ">", self.OnNext)
		self.countLine = self.__Label(self.WIDTH - 150, y + 3, "")
		y += 30
		self.__Button(16, y, TEXT_ALL, self.OnAll, 88)
		self.__Button(108, y, TEXT_NONE, self.OnNone, 88)
		self.__Button(self.WIDTH - 16 - 88 - 4 - 61, y, TEXT_REMOVE, self.OnRemove, 88)
		self.__Button(self.WIDTH - 16 - 61, y, TEXT_CANCEL, self.Close)
		self.SetSize(self.WIDTH, y + 40)

	def SetMissions(self, missions, after):
		self.__CloseDialog()
		self.missions = missions
		self.checked = set(m[0] for m in missions)
		self.page = 0
		self.noteLine.SetText(TEXT_AFTER if after else "")
		self.Refresh()

	def Pages(self):
		return max(1, (len(self.missions) + ROWS - 1) // ROWS)

	def Refresh(self):
		self.page = min(self.page, self.Pages() - 1)
		start = self.page * ROWS
		for i in xrange(ROWS):
			row = self.rows[i]
			if start + i < len(self.missions):
				mission = self.missions[start + i]
				row.SetMission(mission, mission[0] in self.checked)
				row.Show()
			else:
				row.mid = 0
				row.Hide()
		self.pageLine.SetText(TEXT_PAGE % (self.page + 1, self.Pages()))
		self.countLine.SetText(TEXT_COUNT % (len(self.checked), len(self.missions)))
		if self.Pages() > 1:
			self.prevButton.Show()
			self.nextButton.Show()
			self.pageLine.Show()
		else:
			self.prevButton.Hide()
			self.nextButton.Hide()
			self.pageLine.Hide()

	def Toggle(self, mid):
		if mid in self.checked:
			self.checked.discard(mid)
		else:
			self.checked.add(mid)
		self.Refresh()

	def OnPrev(self):
		if self.page > 0:
			self.page -= 1
			self.Refresh()

	def OnNext(self):
		if self.page + 1 < self.Pages():
			self.page += 1
			self.Refresh()

	def OnAll(self):
		self.checked = set(m[0] for m in self.missions)
		self.Refresh()

	def OnNone(self):
		self.checked = set()
		self.Refresh()

	def __CloseDialog(self):
		dialog = self.dialog
		self.dialog = None
		if dialog:
			try:
				dialog.Close()
			except Exception:
				pass

	def OnRemove(self):
		if not self.checked:
			chat.AppendChat(chat.CHAT_TYPE_INFO, TEXT_NONE_CHECKED)
			return
		self.__CloseDialog()
		# MT2009_PLUS_CLEAR_MISSIONS_BIOLOGIST_V1: a Biologist's mission checked asks
		# first on its own (the owner, 4 October) - his chain gives the lasting
		# bonuses, and a removed step is not given back.
		bio = [m for m in self.missions if m[0] in self.checked and m[1] == 3]
		if bio:
			dialog = uiCommon.QuestionDialog2()
			dialog.SetText1(TEXT_BIO_ASK1 % len(bio))
			dialog.SetText2(TEXT_BIO_ASK2)
			dialog.SetAcceptEvent(ui.__mem_func__(self.OnAcceptBiologist))
			dialog.SetCancelEvent(ui.__mem_func__(self.OnCancelDialog))
			dialog.Open()
			self.dialog = dialog
			return
		self.__AskRemove()

	def OnAcceptBiologist(self):
		self.__CloseDialog()
		self.__AskRemove()

	def __AskRemove(self):
		dialog = uiCommon.QuestionDialog2()
		dialog.SetText1(TEXT_ASK1 % len(self.checked))
		dialog.SetText2(TEXT_ASK2)
		dialog.SetAcceptEvent(ui.__mem_func__(self.OnAcceptRemove))
		dialog.SetCancelEvent(ui.__mem_func__(self.OnCancelDialog))
		dialog.Open()
		self.dialog = dialog

	def OnAcceptRemove(self):
		self.__CloseDialog()
		ids = [m[0] for m in self.missions if m[0] in self.checked]
		if not ids or not warpsafe.InGame():
			return
		for i in xrange(0, len(ids), IDS_PER_LINE):
			Send("usun " + " ".join([str(x) for x in ids[i:i + IDS_PER_LINE]]))
		Send("gotowe")
		_state['kept'] = set(m[0] for m in self.missions if m[0] not in self.checked)
		self.Close()

	def OnCancelDialog(self):
		self.__CloseDialog()

	def Open(self):
		self.Refresh()
		self.Show()
		self.SetTop()

	def Close(self):
		self.__CloseDialog()
		self.Hide()

	def OnPressEscapeKey(self):
		self.Close()
		return True

	def Destroy(self):
		self.__CloseDialog()
		self.Hide()
		for row in self.rows:
			row.Destroy()
		self.rows = []
		self.widgets = []


def __ShowList(missions, mode):
	if mode == 1:
		# after a removal: only what was not in the last window
		missions = [m for m in missions if m[0] not in _state['kept'] and m[0] not in _state['shown']]
		if not missions:
			return
	elif not missions:
		chat.AppendChat(chat.CHAT_TYPE_INFO, TEXT_NOTHING)
		window = _state['window']
		if window is not None:
			window.Close()
		return
	window = _state['window']
	if window is None:
		import uiminigameutil
		window = uiminigameutil.SafeCreate(ClearMissionsWindow, TEXT_TITLE)
		if window is None:
			return
		_state['window'] = window
	_state['shown'] = set(m[0] for m in missions)
	window.SetMissions(missions, mode == 1)
	window.Open()


def OnCommand(*args):
	if not args:
		return
	sub, rest = args[0], args[1:]
	try:
		if sub == 'begin':
			_state['incoming'] = []
			_state['mode'] = ToInt(rest[0] if rest else 0)
		elif sub == 'm':
			incoming = _state['incoming']
			if incoming is None or len(rest) < 4:
				return
			incoming.append((ToInt(rest[0]), ToInt(rest[1]), ToInt(rest[2]), "_".join(rest[3:])))
		elif sub == 'gone':
			# the removed quest's letter button (its quest list row went with
			# the server's quest-info packet)
			interface = _state['interface']
			if interface is not None and rest:
				interface.BINARY_ClearQuest(ToInt(rest[0]))
		elif sub == 'end':
			missions = _state['incoming']
			_state['incoming'] = None
			if missions is not None:
				__ShowList(missions, _state['mode'])
	except Exception:
		import dbg
		import sys
		dbg.TraceError('uiusunmisje.OnCommand %s: %s' % (str(args), str(sys.exc_info()[1])))


def DestroyWindow():
	window = _state['window']
	_state['window'] = None
	_state['incoming'] = None
	if window is not None:
		window.Destroy()
