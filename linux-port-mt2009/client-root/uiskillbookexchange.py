# MT2009_PLUS_DIGI_SERVER_QOL_V1 - Seon-Hae's book exchange. Autor: Digi Rasta
# (nowy-system v0.23.0, uiksiegi.py; his "SKILLBOOK_COMB_SYSTEM" of the Biore
# list). Ten skill books of any kind + 250 000 Yang = a random "Instr." book
# of the player's own class and skill group.
#
# The quest ksiegi_seonhae opens the window at Seon-Hae (20095) - "NOWY_KSIEGI
# open" (digiserverqol.py). The core checks everything again: the NPC, the
# books, the Yang, the room in the bag (playerbot_digi_qol.h).
#
# MT2009_PLUS_BOOK_EXCHANGE_V2: the window holds whole stacks (a stack of ten
# books is ten books) in 5 x 4 cells; a right click in the bag puts a book in
# (Ctrl + right click: every book of the bag), a right click on the window's
# cell takes it out. "Wymien" makes one exchange, "x10" ten, "Wszystko" as
# many as the books and the Yang allow (at most MAX_TIMES) - each one costs
# COST as before. It sends "/nowy_ksiegi x<times> <cells>"; the books are
# taken from the cells in their order. Answer: "NOWY_KSIEGI done <vnum>
# <count>", one per kind of book given.
#
# Python 2.7 as the client has it; the texts are CP1250 escapes.
import app
import chat
import item
import mouseModule
import net
import player
import ui

COST = 250000
BOOKS_PER = 10
COLUMNS = 5
ROWS = 4
SLOTS = COLUMNS * ROWS	# playerbot_digi_qol.h, BOOK_MAX_CELLS
MAX_TIMES = 100	# playerbot_digi_qol.h, BOOK_MAX_TIMES
SEND_PAUSE = 1.0	# seconds between two orders: a double click is not two exchanges
TEXT_TITLE = "Wymiana ksi\xb9g umiej\xeatno\x9cci"
TEXT_INFO = (
	"10 dowolnych ksi\xb9g + 250 000 Yang = losowa",
	"ksi\xeaga Twojej klasy i drogi. Stos liczy si\xea ca\xb3y.",
	"Prawy klik w ekwipunku dodaje ksi\xeag\xea,",
	"Ctrl + prawy klik: wszystkie ksi\xeagi z torby.",
)
TEXT_STATUS = "Ksi\xb9g w oknie: %d  (wymian: %d)"
TEXT_BUTTON = "Wymie\xf1"
TEXT_BUTTON_TEN = "x10"
TEXT_BUTTON_ALL = "Wszystko"
TEXT_NOT_BOOK = "To nie jest ksi\xeaga umiej\xeatno\x9cci."
TEXT_NEED = "Potrzeba %d ksi\xb9g (w oknie: %d)."
TEXT_NO_GOLD = "Za ma\xb3o Yang (potrzeba %s)."
TEXT_FULL = "Okno jest pe\xb3ne."
TEXT_ADDED_ALL = "Dodano ksi\xb9g: %d."
TEXT_NONE_ADDED = "Brak ksi\xb9g do dodania."
TEXT_ASK = "Wymieni\xe6 %d ksi\xb9g na %d nowych za %s Yang?"
TEXT_DONE = "Seon-Hae da\xb3 Ci: %s."
TEXT_DONE_MANY = "Seon-Hae da\xb3 Ci: %s x%d."


def IsSkillBook(vnum):
	if not vnum:
		return False
	item.SelectItem(vnum)
	return item.GetItemType() == item.ITEM_TYPE_SKILLBOOK


def Money(n):
	text = str(int(n))
	parts = []
	while len(text) > 3:
		parts.insert(0, text[-3:])
		text = text[:-3]
	parts.insert(0, text)
	return " ".join(parts)


def BagSize():
	return getattr(player, "INVENTORY_DEFAULT_MAX_NUM", 180)


def CtrlPressed():
	return app.IsPressed(app.DIK_LCONTROL) or app.IsPressed(getattr(app, "DIK_RCONTROL", app.DIK_LCONTROL))


class SkillBookExchangeWindow(ui.BoardWithTitleBar):
	WIDTH = 250

	def __init__(self):
		ui.BoardWithTitleBar.__init__(self)
		self.cells = [-1] * SLOTS
		self.shown = [None] * SLOTS	# (vnum, count) each cell shows, to see a change
		self.widgets = []
		self.question = None
		self.questionTimes = 0
		self.lastSend = -SEND_PAUSE
		self.AddFlag("movable")
		self.AddFlag("float")
		self.SetTitleName(TEXT_TITLE)
		self.SetCloseEvent(ui.__mem_func__(self.Close))
		y = 34
		for line in TEXT_INFO:
			text = ui.TextLine()
			text.SetParent(self)
			text.SetPosition(self.WIDTH // 2, y)
			text.SetHorizontalAlignCenter()
			text.SetText(line)
			text.Show()
			self.widgets.append(text)
			y += 15
		grid = ui.GridSlotWindow()
		grid.SetParent(self)
		grid.SetPosition((self.WIDTH - COLUMNS * 32) // 2, y + 6)
		grid.ArrangeSlot(0, COLUMNS, ROWS, 32, 32, 0, 0)
		grid.SetSlotBaseImage("d:/ymir work/ui/public/Slot_Base.sub", 1.0, 1.0, 1.0, 1.0)
		grid.SetSelectEmptySlotEvent(ui.__mem_func__(self.__SelectEmpty))
		grid.SetSelectItemSlotEvent(ui.__mem_func__(self.__SelectItem))
		grid.SetUnselectItemSlotEvent(ui.__mem_func__(self.__SelectItem))
		grid.Show()
		self.grid = grid
		y += 6 + ROWS * 32 + 6
		status = ui.TextLine()
		status.SetParent(self)
		status.SetPosition(self.WIDTH // 2, y)
		status.SetHorizontalAlignCenter()
		status.SetPackedFontColor(0xffffcc66)
		status.Show()
		self.status = status
		y += 20
		self.buttons = []
		left = (self.WIDTH - 3 * 61 - 2 * 8) // 2
		events = ((TEXT_BUTTON, self.__ExchangeOne), (TEXT_BUTTON_TEN, self.__ExchangeTen), (TEXT_BUTTON_ALL, self.__ExchangeAll))
		for i in xrange(len(events)):
			button = ui.Button()
			button.SetParent(self)
			button.SetPosition(left + i * (61 + 8), y)
			button.SetUpVisual("d:/ymir work/ui/public/middle_button_01.sub")
			button.SetOverVisual("d:/ymir work/ui/public/middle_button_02.sub")
			button.SetDownVisual("d:/ymir work/ui/public/middle_button_03.sub")
			button.SetText(events[i][0])
			button.SAFE_SetEvent(events[i][1])
			button.Show()
			self.buttons.append(button)
		self.SetSize(self.WIDTH, y + 21 + 14)
		self.Hide()

	def __del__(self):
		ui.BoardWithTitleBar.__del__(self)

	def Destroy(self):
		self.__CloseQuestion()
		self.Hide()
		self.grid = None
		self.status = None
		self.buttons = []
		self.widgets = []

	def Open(self):
		self.cells = [-1] * SLOTS
		self.Refresh()
		self.SetCenterPosition()
		self.SetTop()
		self.Show()

	def Close(self):
		self.__CloseQuestion()
		self.Hide()

	def OnPressEscapeKey(self):
		self.Close()
		return True

	# ---------------------------------------------------------------- cells
	def AddCell(self, pos):
		"""A book of the bag into the first free cell (a right click)."""
		if pos in self.cells:
			return
		if not IsSkillBook(player.GetItemIndex(pos)):
			chat.AppendChat(chat.CHAT_TYPE_INFO, TEXT_NOT_BOOK)
			return
		if -1 not in self.cells:
			chat.AppendChat(chat.CHAT_TYPE_INFO, TEXT_FULL)
			return
		self.cells[self.cells.index(-1)] = pos
		self.Refresh()

	def AddAll(self):
		"""Every skill book of the bag, as long as there are free cells."""
		added = 0
		for pos in xrange(BagSize()):
			if -1 not in self.cells:
				break
			if pos in self.cells or not IsSkillBook(player.GetItemIndex(pos)):
				continue
			self.cells[self.cells.index(-1)] = pos
			added += 1
		self.Refresh()
		if added:
			chat.AppendChat(chat.CHAT_TYPE_INFO, TEXT_ADDED_ALL % added)
		elif -1 not in self.cells:
			chat.AppendChat(chat.CHAT_TYPE_INFO, TEXT_FULL)
		else:
			chat.AppendChat(chat.CHAT_TYPE_INFO, TEXT_NONE_ADDED)

	def __SelectEmpty(self, slotIndex):
		if not mouseModule.mouseController.isAttached():
			return
		if mouseModule.mouseController.GetAttachedType() != player.SLOT_TYPE_INVENTORY:
			return
		pos = mouseModule.mouseController.GetAttachedSlotNumber()
		if not IsSkillBook(player.GetItemIndex(pos)):
			chat.AppendChat(chat.CHAT_TYPE_INFO, TEXT_NOT_BOOK)
			return
		mouseModule.mouseController.DeattachObject()
		if pos in self.cells:
			return
		self.cells[slotIndex] = pos
		self.Refresh()

	def __SelectItem(self, slotIndex):
		if mouseModule.mouseController.isAttached():
			return
		self.cells[slotIndex] = -1
		self.Refresh()

	def BookCount(self):
		return sum([player.GetItemCount(pos) for pos in self.cells if pos >= 0])

	def Refresh(self):
		if not self.grid:
			return
		for i in xrange(SLOTS):
			pos = self.cells[i]
			vnum = player.GetItemIndex(pos) if pos >= 0 else 0
			if pos >= 0 and IsSkillBook(vnum):
				count = player.GetItemCount(pos)
				if count > 1:
					self.grid.SetItemSlot(i, vnum, count)
				else:
					self.grid.SetItemSlot(i, vnum, 0)
				self.shown[i] = (vnum, count)
			else:
				self.cells[i] = -1
				self.shown[i] = None
				self.grid.ClearSlot(i)
		self.grid.RefreshSlot()
		if self.status:
			books = self.BookCount()
			self.status.SetText(TEXT_STATUS % (books, books // BOOKS_PER))

	def OnUpdate(self):
		# A book moved, used or a stack changed meanwhile: the cells follow.
		for i in xrange(SLOTS):
			pos = self.cells[i]
			if pos < 0:
				continue
			vnum = player.GetItemIndex(pos)
			if not IsSkillBook(vnum) or self.shown[i] != (vnum, player.GetItemCount(pos)):
				self.Refresh()
				return

	# ---------------------------------------------------------------- exchange
	def __ExchangeOne(self):
		self.__Exchange(1)

	def __ExchangeTen(self):
		self.__Exchange(10)

	def __ExchangeAll(self):
		# As many as the books and the Yang allow; none possible - one, whose
		# check says what is missing.
		times = min(self.BookCount() // BOOKS_PER, int(player.GetGold() // COST), MAX_TIMES)
		self.__Exchange(max(1, times))

	def __Exchange(self, times):
		books = self.BookCount()
		if books < times * BOOKS_PER:
			chat.AppendChat(chat.CHAT_TYPE_INFO, TEXT_NEED % (times * BOOKS_PER, books))
			return
		if player.GetGold() < times * COST:
			chat.AppendChat(chat.CHAT_TYPE_INFO, TEXT_NO_GOLD % Money(times * COST))
			return
		if times == 1:
			self.__Send(1)
			return
		self.__CloseQuestion()
		try:
			import uiCommon
			question = uiCommon.QuestionDialog()
			question.SetText(TEXT_ASK % (times * BOOKS_PER, times, Money(times * COST)))
			question.SetAcceptEvent(ui.__mem_func__(self.__OnAskAccept))
			question.SetCancelEvent(ui.__mem_func__(self.__CloseQuestion))
			question.Open()
			self.question = question
			self.questionTimes = times
		except Exception:
			self.__Send(times)

	def __OnAskAccept(self):
		times = self.questionTimes if self.question else 0
		self.__CloseQuestion()
		if times:
			self.__Send(times)

	def __CloseQuestion(self):
		if self.question:
			self.question.Close()
		self.question = None
		self.questionTimes = 0

	def __Send(self, times):
		now = app.GetTime()
		if now - self.lastSend < SEND_PAUSE:
			return
		self.lastSend = now
		cells = [str(pos) for pos in self.cells if pos >= 0]
		net.SendChatPacket("/nowy_ksiegi x%d %s" % (times, " ".join(cells)))

	def OnDone(self, vnum, count=1):
		item.SelectItem(vnum)
		if count > 1:
			chat.AppendChat(chat.CHAT_TYPE_INFO, TEXT_DONE_MANY % (item.GetItemName(), count))
		else:
			chat.AppendChat(chat.CHAT_TYPE_INFO, TEXT_DONE % item.GetItemName())
		self.cells = [-1] * SLOTS
		self.Refresh()


_window = []


def Window():
	if not _window:
		_window.append(SkillBookExchangeWindow())
	return _window[0]


def QuickPut(cell):
	"""MT2009_PLUS_BOOK_EXCHANGE_V2: a right click on the bag with the window
	open (uiinventory.py, OnRightClickBagItem) puts the book in - Ctrl: every
	book of the bag. True when the click was the window's; a thing that is not
	a skill book is left to the bag (used as ever)."""
	if not _window or not _window[0].IsShow():
		return False
	if mouseModule.mouseController.isAttached():
		return False
	if not (0 <= cell < BagSize()) or not IsSkillBook(player.GetItemIndex(cell)):
		return False
	if CtrlPressed():
		_window[0].AddAll()
	else:
		_window[0].AddCell(cell)
	return True


def OnServer(sub="", data="0", *rest):
	if sub == "open":
		Window().Open()
	elif sub == "done":
		try:
			vnum = int(data)
			count = int(rest[0]) if rest else 1
		except ValueError:
			return
		if _window:
			_window[0].OnDone(vnum, count)


def DestroyWindow():
	"""The game window closes (a warp, a channel change, the logout)."""
	while _window:
		_window.pop().Destroy()
