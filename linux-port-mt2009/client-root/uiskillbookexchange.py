# MT2009_PLUS_DIGI_SERVER_QOL_V1 - Seon-Hae's book exchange. Autor: Digi Rasta
# (nowy-system v0.23.0, uiksiegi.py; his "SKILLBOOK_COMB_SYSTEM" of the Biore
# list). Ten skill books of any kind + 250 000 Yang = a random "Instr." book
# of the player's own class and skill group.
#
# The quest ksiegi_seonhae opens the window at Seon-Hae (20095) - "NOWY_KSIEGI
# open" (digiserverqol.py). "Wymien" sends "/nowy_ksiegi <10 cells>" and the core
# checks everything again: the NPC, the books, the Yang (playerbot_digi_qol.h).
# Its answer: "NOWY_KSIEGI done <vnum>".
#
# Python 2.7 as the client has it; the texts are CP1250 escapes.
import chat
import item
import mouseModule
import net
import player
import ui

COST = 250000
SLOTS = 10
TEXT_TITLE = "Wymiana ksi\xb9g umiej\xeatno\x9cci"
TEXT_INFO = (
	"Przeci\xb9gnij 10 dowolnych ksi\xb9g umiej\xeatno\x9cci.",
	"Seon-Hae da za nie losow\xb9 ksi\xeag\xea Twojej klasy",
	"i drogi. Op\xb3ata: 250 000 Yang.",
)
TEXT_BUTTON = "Wymie\xf1"
TEXT_NOT_BOOK = "To nie jest ksi\xeaga umiej\xeatno\x9cci."
TEXT_NEED_TEN = "Potrzeba 10 ksi\xb9g."
TEXT_NO_GOLD = "Za ma\xb3o Yang (250 000)."
TEXT_DONE = "Seon-Hae da\xb3 Ci: %s."


def IsSkillBook(vnum):
	if not vnum:
		return False
	item.SelectItem(vnum)
	return item.GetItemType() == item.ITEM_TYPE_SKILLBOOK


class SkillBookExchangeWindow(ui.BoardWithTitleBar):
	WIDTH = 240

	def __init__(self):
		ui.BoardWithTitleBar.__init__(self)
		self.cells = [-1] * SLOTS
		self.widgets = []
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
		grid.SetPosition((self.WIDTH - 5 * 32) // 2, y + 6)
		grid.ArrangeSlot(0, 5, 2, 32, 32, 0, 0)
		grid.SetSlotBaseImage("d:/ymir work/ui/public/Slot_Base.sub", 1.0, 1.0, 1.0, 1.0)
		grid.SetSelectEmptySlotEvent(ui.__mem_func__(self.__SelectEmpty))
		grid.SetSelectItemSlotEvent(ui.__mem_func__(self.__SelectItem))
		grid.Show()
		self.grid = grid
		button = ui.Button()
		button.SetParent(self)
		button.SetPosition((self.WIDTH - 61) // 2, y + 6 + 64 + 10)
		button.SetUpVisual("d:/ymir work/ui/public/middle_button_01.sub")
		button.SetOverVisual("d:/ymir work/ui/public/middle_button_02.sub")
		button.SetDownVisual("d:/ymir work/ui/public/middle_button_03.sub")
		button.SetText(TEXT_BUTTON)
		button.SAFE_SetEvent(self.__Exchange)
		button.Show()
		self.button = button
		self.SetSize(self.WIDTH, y + 6 + 64 + 10 + 21 + 14)
		self.Hide()

	def __del__(self):
		ui.BoardWithTitleBar.__del__(self)

	def Destroy(self):
		self.Hide()
		self.grid = None
		self.button = None
		self.widgets = []

	def Open(self):
		self.cells = [-1] * SLOTS
		self.Refresh()
		self.SetCenterPosition()
		self.SetTop()
		self.Show()

	def Close(self):
		self.Hide()

	def OnPressEscapeKey(self):
		self.Close()
		return True

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

	def Refresh(self):
		if not self.grid:
			return
		for i in xrange(SLOTS):
			pos = self.cells[i]
			vnum = player.GetItemIndex(pos) if pos >= 0 else 0
			if pos >= 0 and IsSkillBook(vnum):
				self.grid.SetItemSlot(i, vnum, 0)
			else:
				self.cells[i] = -1
				self.grid.ClearSlot(i)
		self.grid.RefreshSlot()

	def OnUpdate(self):
		# A book moved or used meanwhile leaves its slot.
		for pos in self.cells:
			if pos >= 0 and not IsSkillBook(player.GetItemIndex(pos)):
				self.Refresh()
				return

	def __Exchange(self):
		if -1 in self.cells:
			chat.AppendChat(chat.CHAT_TYPE_INFO, TEXT_NEED_TEN)
			return
		if player.GetGold() < COST:
			chat.AppendChat(chat.CHAT_TYPE_INFO, TEXT_NO_GOLD)
			return
		net.SendChatPacket("/nowy_ksiegi " + " ".join([str(pos) for pos in self.cells]))

	def OnDone(self, vnum):
		item.SelectItem(vnum)
		chat.AppendChat(chat.CHAT_TYPE_INFO, TEXT_DONE % item.GetItemName())
		self.cells = [-1] * SLOTS
		self.Refresh()


_window = []


def Window():
	if not _window:
		_window.append(SkillBookExchangeWindow())
	return _window[0]


def OnServer(sub="", data="0", *rest):
	if sub == "open":
		Window().Open()
	elif sub == "done":
		try:
			vnum = int(data)
		except ValueError:
			return
		if _window:
			_window[0].OnDone(vnum)


def DestroyWindow():
	"""The game window closes (a warp, a channel change, the logout)."""
	while _window:
		_window.pop().Destroy()
