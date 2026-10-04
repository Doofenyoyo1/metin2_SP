# MT2009_PLUS_VEKIRION_V1 - Skroty klawiszowe (Autor: Vekirion): the window of
# keybind.py, behind the Esc menu's button above "Sklep z przedmiotami"
# (uisystem.py, uiscript/systemdialog.py).
#
# Every action game.py runs, by category, with its two keys. A click on a key
# waits for the next key pressed (taken in the game window, keybind.CaptureKey,
# so no action runs meanwhile): with Ctrl, Shift and/or Alt held it becomes
# a combination (Ctrl+G), alone a single key; a Shift pressed and let go
# alone is the Shift itself (the sprint's default). Esc cancels, Backspace (or a
# right click on the key) clears it. A key another action had moves here and
# that action is named below the list. The keys the exe keeps (Esc, Enter),
# the modifiers' own doings and the windows' clicks (Alt + LPM in the bag...)
# are listed at the end, not rebindable.
#
# Changes are a draft (the owner, 3 October: "Before clicking Zapisz the
# changes shouldn't apply"): "Zapisz" makes them the bindings and saves them
# (keybind.CommitBindings, autohunt/klawisze.cfg); closing the window any
# other way drops them. "Domyslne" puts the defaults in the draft.
#
# A core action (keybind.CORE: walking, attack, pick-up, slots 1-5, the main
# windows, the horse) with no key at all has its name in red.
#
# Python 2.7 as the client has it; the texts are CP1250 escapes.

import app
import ui
import uiCommon

import keybind

COLOR_HEADER = 0xFFF8BF24
COLOR_LABEL = 0xFFE6E6E6
COLOR_MISSING = 0xFFFF4040
COLOR_FIXED = 0xFFA0A0A0
COLOR_WAIT = 0xFFFF8040

# The two footer buttons are the same 88x21 button ("Zamknij" had it).
KEY_BUTTON = 'd:/ymir work/ui/public/large_button_%02d.sub'

_state = {'window': None}


def OpenWindow():
	window = _state['window']
	if window is None:
		window = KeybindWindow()
		_state['window'] = window
	window.Open()


def DestroyWindow():
	window = _state['window']
	_state['window'] = None
	if window is not None:
		try:
			window.Destroy()
		except Exception:
			pass


class KeybindWindow(ui.BoardWithTitleBar):
	WIDTH = 482
	ROW_HEIGHT = 24
	VISIBLE_ROWS = 15
	LIST_X = 12			# the list board's inside, from the window's left
	LIST_Y = 56			# the list board's top
	LIST_PAD = 9		# rows start below the board's top border
	LABEL_WIDTH = 190
	KEY_X = (LIST_X + LABEL_WIDTH, LIST_X + LABEL_WIDTH + 124)	# in the window
	SCROLL_INSET = 10	# the scroll bar's gap to the board's right border

	def __init__(self):
		ui.BoardWithTitleBar.__init__(self)
		self.widgets = []
		self.rows = []			# [widgets] per list row
		self.keyButtons = {}	# (action, slot) -> button
		self.labels = {}		# action -> its name's TextLine
		self.startRow = 0
		self.capture = None		# (action, slot) waiting for a key
		self.question = None
		self.draft = {}
		self.dirty = False

		listHeight = self.VISIBLE_ROWS * self.ROW_HEIGHT
		self.boardHeight = listHeight + 2 * self.LIST_PAD
		self.height = self.LIST_Y + self.boardHeight + 90
		self.AddFlag('movable')
		self.AddFlag('float')
		self.SetSize(self.WIDTH, self.height)
		self.SetTitleName('Skr\xf3ty klawiszowe')
		self.SetCloseEvent(ui.__mem_func__(self.Close))
		self.SetMouseWheelEvent(self.OnWheel)
		self.__Build()
		self.SetCenterPosition()

	def __del__(self):
		ui.BoardWithTitleBar.__del__(self)

	def Destroy(self):
		self.__EndCapture()
		self.__OnCloseQuestion()
		for widget in self.widgets:
			widget.Hide()
		self.widgets = []
		self.rows = []
		self.keyButtons = {}
		self.labels = {}
		self.scrollBar = None
		self.statusLine = None
		self.dirtyLine = None
		self.Hide()

	def __Build(self):
		self._Text(self, self.LIST_X + 4, 36, 'Funkcja', COLOR_HEADER)
		self._Text(self, self.KEY_X[0] + 20, 36, 'Klawisz 1', COLOR_HEADER)
		self._Text(self, self.KEY_X[1] + 20, 36, 'Klawisz 2', COLOR_HEADER)

		boardX = self.LIST_X - 4
		boardWidth = self.WIDTH - 2 * boardX
		board = ui.ThinBoard()
		board.SetParent(self)
		board.SetPosition(boardX, self.LIST_Y)
		board.SetSize(boardWidth, self.boardHeight)
		board.SetMouseWheelEvent(self.OnWheel)
		board.Show()
		self.widgets.append(board)
		self.listBoard = board

		for category, actions in keybind.Actions():
			self.rows.append([self._Text(board, 8, 0, category, COLOR_HEADER, True)])
			for action, label in actions:
				labelLine = self._Text(board, 16, 0, label, COLOR_LABEL)
				self.labels[action] = labelLine
				row = [labelLine]
				for slot in xrange(keybind.SLOT_COUNT):
					button = self._Button(board, '', self.OnClickKey, action, slot)
					button.SAFE_SetStringEvent('MOUSE_RIGHT_BUTTON', self.OnClearKey, action, slot)
					self.keyButtons[(action, slot)] = button
					row.append(button)
				self.rows.append(row)
		self.rows.append([self._Text(board, 8, 0, 'Sta\xb3e (nie do zmiany)', COLOR_HEADER, True)])
		# One line each: some of the clicks' texts are longer than the name column.
		for keyText, label in keybind.FIXED:
			self.rows.append([self._Text(board, 16, 0, '%s  -  %s' % (keyText, label), COLOR_FIXED)])

		# Inside the board, clear of its right border.
		scrollHeight = self.boardHeight - 2 * (self.LIST_PAD - 2)
		scrollBar = ui.ScrollBar()
		scrollBar.SetParent(self)
		scrollBar.SetPosition(boardX + boardWidth - ui.ScrollBar.SCROLLBAR_WIDTH - self.SCROLL_INSET,
			self.LIST_Y + self.LIST_PAD - 2)
		scrollBar.SetScrollBarSize(scrollHeight)
		scrollBar.SetScrollEvent(ui.__mem_func__(self.OnScroll))
		if len(self.rows) > self.VISIBLE_ROWS:
			scrollBar.SetMiddleBarSize(float(self.VISIBLE_ROWS) / float(len(self.rows)))
			scrollBar.Show()
		self.widgets.append(scrollBar)
		self.scrollBar = scrollBar

		y = self.LIST_Y + self.boardHeight + 8
		self.statusLine = self._Text(self, self.LIST_X + 4, y, '', COLOR_LABEL)
		self._Text(self, self.LIST_X + 4, y + 16,
			'Ctrl/Shift/Alt + klawisz = kombinacja, sam Shift te\xbf. Esc - anuluj, Backspace / PPM - wyczy\x9c\xe6.', COLOR_FIXED)
		y += 40
		resetButton = self._Button(self, 'Domy\x9clne', self.OnClickReset)
		resetButton.SetPosition(self.LIST_X, y)
		resetButton.SetToolTipText('Przywr\xf3\xe6 domy\x9clne klawisze (do zapisania)')
		saveButton = self._Button(self, 'Zapisz', self.OnClickSave)
		saveButton.SetPosition(self.WIDTH - self.LIST_X - saveButton.GetWidth(), y)
		self.dirtyLine = self._Text(self, self.WIDTH - self.LIST_X - saveButton.GetWidth() - 10, y + 4,
			'Niezapisane zmiany', COLOR_WAIT)
		self.dirtyLine.SetHorizontalAlignRight()
		self.dirtyLine.Hide()

		self.__Layout()

	def _Text(self, parent, x, y, text, color, bold=False):
		line = ui.TextLine()
		line.SetParent(parent)
		line.SetPosition(x, y)
		line.SetPackedFontColor(color)
		if bold:
			line.SetBold()
		line.SetText(text)
		line.Show()
		self.widgets.append(line)
		return line

	def _Button(self, parent, text, event, *args):
		button = ui.Button()
		button.SetParent(parent)
		button.SetUpVisual(KEY_BUTTON % 1)
		button.SetOverVisual(KEY_BUTTON % 2)
		button.SetDownVisual(KEY_BUTTON % 3)
		button.SetText(text)
		button.SAFE_SetEvent(event, *args)
		button.SetMouseWheelEvent(self.OnWheel)
		button.Show()
		self.widgets.append(button)
		return button

	def __Layout(self):
		boardX = self.LIST_X - 4
		y = self.LIST_PAD
		for idx, row in enumerate(self.rows):
			if idx < self.startRow or idx >= self.startRow + self.VISIBLE_ROWS:
				for widget in row:
					widget.Hide()
				continue
			for col, widget in enumerate(row):
				x, unused = widget.GetLocalPosition()
				if isinstance(widget, ui.Button):
					widget.SetPosition(self.KEY_X[col - 1] - boardX, y + 1)
				else:
					widget.SetPosition(x, y + 4)
				widget.Show()
			y += self.ROW_HEIGHT

	def Refresh(self):
		for (action, slot), button in self.keyButtons.items():
			if self.capture == (action, slot):
				button.SetText('...')
				button.SetTextColor(COLOR_WAIT)
				continue
			slots = self.draft.get(action) or [None] * keybind.SLOT_COUNT
			button.SetText(keybind.BindingText(slots[slot]) or '-')
			button.SetTextColor(COLOR_LABEL)
		for action, line in self.labels.items():
			missing = action in keybind.CORE and not keybind.TextOf(self.draft, action)
			line.SetPackedFontColor(COLOR_MISSING if missing else COLOR_LABEL)

	def SetStatus(self, text):
		if self.statusLine:
			self.statusLine.SetText(text)

	def __Changed(self, text):
		self.__SetDirty(True)
		self.SetStatus(text)
		self.Refresh()

	def __SetDirty(self, dirty):
		self.dirty = dirty
		if self.dirtyLine:
			if dirty:
				self.dirtyLine.Show()
			else:
				self.dirtyLine.Hide()

	def OnScroll(self):
		scrollable = max(0, len(self.rows) - self.VISIBLE_ROWS)
		startRow = int(round(scrollable * self.scrollBar.GetPos()))
		if startRow != self.startRow:
			self.startRow = startRow
			self.__Layout()

	def OnWheel(self, direction):
		scrollable = max(0, len(self.rows) - self.VISIBLE_ROWS)
		if not scrollable:
			return
		startRow = max(0, min(scrollable, self.startRow - 3 * direction))
		self.scrollBar.SetPos(float(startRow) / float(scrollable))

	def Open(self):
		self.__EndCapture()
		self.draft = keybind.CopyBindings()
		self.__SetDirty(False)
		self.Refresh()
		self.SetStatus('Kliknij klawisz, by go zmieni\xe6.')
		self.Show()
		self.SetTop()

	# Closing without "Zapisz" drops the draft (Open starts from the
	# bindings in use).
	def Close(self):
		self.__EndCapture()
		self.__OnCloseQuestion()
		self.draft = {}
		self.__SetDirty(False)
		self.Hide()

	def OnClickSave(self):
		self.__EndCapture()
		keybind.CommitBindings(self.draft)
		self.Close()

	# Waiting for a key: keybind takes the next key in the game window
	# (keybind.CaptureKey) and hands it to OnCapturedKey; no action runs.
	def OnClickKey(self, action, slot):
		self.__EndCapture()
		self.capture = (action, slot)
		keybind.StartCapture(ui.__mem_func__(self.OnCapturedKey))
		self.Refresh()
		self.SetStatus('Naci\x9cnij klawisz dla: %s' % keybind.ActionLabel(action))
		self.SetTop()

	def __EndCapture(self):
		keybind.StopCapture()
		if self.capture is None:
			return
		self.capture = None
		self.Refresh()

	def OnClearKey(self, action, slot):
		self.__EndCapture()
		keybind.AssignInto(self.draft, action, slot, None)
		self.__Changed('Wyczyszczono: %s' % keybind.ActionLabel(action))

	def OnCapturedKey(self, key, mods=0):
		if self.capture is None:
			keybind.StopCapture()
			return
		action, slot = self.capture
		if key == app.DIK_ESC:
			self.__EndCapture()
			self.SetStatus('Anulowano.')
			return
		if key == app.DIK_BACK:
			self.OnClearKey(action, slot)
			return
		binding = (mods, key)
		if not keybind.IsValidBinding(binding):
			self.SetStatus('Tego klawisza nie mo\xbfna przypisa\xe6 - wybierz inny.')
			return

		self.__EndCapture()
		previous = keybind.AssignInto(self.draft, action, slot, binding)
		text = '%s: %s' % (keybind.ActionLabel(action), keybind.BindingText(binding))
		if previous:
			text += '  (zabrane z: %s)' % keybind.ActionLabel(previous)
		self.__Changed(text)

	def OnPressEscapeKey(self):
		# Esc while waiting cancels the wait only, whichever of the game
		# window and this one hears it first.
		if self.capture is not None:
			keybind.NoteEscape()
			self.__EndCapture()
			self.SetStatus('Anulowano.')
			return True
		if keybind.TookEscape():
			return True
		self.Close()
		return True

	def OnClickReset(self):
		self.__EndCapture()
		self.__OnCloseQuestion()
		question = uiCommon.QuestionDialog()
		question.SetText('Przywr\xf3ci\xe6 wszystkie domy\x9clne klawisze?')
		question.SAFE_SetAcceptEvent(self.__OnAcceptReset)
		question.SAFE_SetCancelEvent(self.__OnCloseQuestion)
		question.Open()
		self.question = question

	def __OnAcceptReset(self):
		self.draft = keybind.DefaultBindings()
		self.__OnCloseQuestion()
		self.__Changed('Przywr\xf3cono domy\x9clne klawisze')

	def __OnCloseQuestion(self):
		if self.question:
			self.question.Close()
			self.question = None
