# Podglad skrzynki - what a chest can hand out (Gibon's window, 26 September:
# "przeciagasz skrzynke, pokazuje ci co moze poleciec"; the operator: "kilka osob o
# tym pisalo").
#
# The small chest button beside the inventory's three opens it. A chest
# dragged from the bag onto its slot goes to the server as its cell,
# "/chest_preview <cell>", and the server answers with the group the game
# itself loaded for that chest - special_item_group.txt, nested groups
# followed - one line a reward (game.py hands them over):
#
#     ChestPreviewBegin <vnum>
#     ChestPreviewItem <vnum>|<count>
#     ChestPreviewEffect <kind>|<amount>    yang, experience, a monster...
#     ChestPreviewEnd <vnum>
#     ChestPreviewError <why>
#
# (do_chest_preview in cmd_general.cpp, playerbotify.py's apply_chest_preview.)
# Nothing about a chest is kept in the client, so what the window shows is
# what the running server would hand out, an operator's edited group included.
#
# The texts are CP1250 escapes so the file stays ASCII. Python 2.7 as the
# client has it: every window here holds another through a weak proxy and
# every event through ui.__mem_func__, because two windows with __del__ in a
# cycle are never collected there and their C++ windows would stay for good.
# And a window handed in may be a weak proxy already: ui.__mem_func__ runs an
# event with self as one, and a proxy of a proxy is a TypeError - which is
# how upstream's client 2.0.39's chest button opened nothing (WindowProxy, Gibon's fix).

import math
from _weakref import CallableProxyType, ProxyType, proxy

import chat
import item
import mouseModule
import net
import player
import ui

def WindowProxy(window):
	"""A weak proxy of the window - the window itself when it is one already.
	The inventory's chest button runs through ui.__mem_func__, so the
	inventory it hands over is a proxy, and proxy() of that raised "cannot
	create weak reference to 'weakproxy' object" before the window was made
	(Gibon, 26 September)."""
	if type(window) in (ProxyType, CallableProxyType):
		return window
	return proxy(window)


SLOT_IMAGE = "playerbot_ui/chest_slot.tga"

# CSpecialItemGroup::EGiveType - what an effect line gives.
EFFECT_NAMES = {1: "Yang", 2: "EXP", 3: "Potw\xf3r", 4: "Spowol.", 5: "Wyssanie",
	6: "Trucizna", 7: "Grupa", 8: "Krwaw."}

# The server stops at four hundred lines (playerbotify's apply_chest_preview).
MAX_REWARDS = 400


class ChestInputSlot(ui.Window):
	"""The slot a chest is dropped on."""

	def __init__(self, owner):
		ui.Window.__init__(self)
		self.owner = WindowProxy(owner)

	def OnMouseLeftButtonUp(self):
		mouse = mouseModule.mouseController
		if not mouse.isAttached():
			return
		if mouse.GetAttachedType() != player.SLOT_TYPE_INVENTORY:
			chat.AppendChat(chat.CHAT_TYPE_INFO, "Przeci\xb9gnij skrzynk\xea z ekwipunku.")
			return
		slot = mouse.GetAttachedSlotNumber()
		vnum = mouse.GetAttachedItemIndex()
		mouse.DeattachObject()
		if slot < 0 or player.GetItemIndex(slot) != vnum:
			chat.AppendChat(chat.CHAT_TYPE_INFO, "Skrzynka musi by\xe6 w ekwipunku.")
			return
		self.owner.SetChest(slot, vnum)


class RewardSlot(ui.Window):
	def __init__(self, owner, vnum):
		ui.Window.__init__(self)
		self.owner = WindowProxy(owner)
		self.vnum = vnum

	def OnMouseOverIn(self):
		self.owner.ShowRewardTip(self.vnum)

	def OnMouseOverOut(self):
		self.owner.HideRewardTip()


def ItemSize(vnum, maxRows):
	"""(width, height) in cells; the client's items are one cell wide."""
	if vnum <= 0:
		return (1, 1)
	try:
		item.SelectItem(vnum)
		width, height = item.GetItemSize()
		return (max(1, int(width)), max(1, min(maxRows, int(height))))
	except Exception:
		return (1, 1)


def LayOut(entries, cols, maxRows):
	"""Pages of (x, y, width, height, vnum, count): the entries in order, each
	in the first place its size fits, a new page when none does."""
	pages = []
	occupied = [[False] * cols for row in range(maxRows)]
	placed = []
	for vnum, count in entries:
		width, height = ItemSize(vnum, maxRows)
		width = min(width, cols)
		spot = None
		for attempt in range(2):
			for y in range(maxRows - height + 1):
				for x in range(cols - width + 1):
					if all(not occupied[row][col] for row in range(y, y + height) for col in range(x, x + width)):
						spot = (x, y)
						break
				if spot:
					break
			if spot or not placed:
				break
			pages.append(placed)
			placed = []
			occupied = [[False] * cols for row in range(maxRows)]
		if not spot:
			continue
		x, y = spot
		for row in range(y, y + height):
			for col in range(x, x + width):
				occupied[row][col] = True
		placed.append((x, y, width, height, vnum, count))
	pages.append(placed)
	return pages


class ChestPreviewWindow(ui.BoardWithTitleBar):
	WIDTH = 280
	HEIGHT = 192
	MIN_COLS = 4
	MAX_COLS = 14
	MAX_ROWS = 8
	GRID_Y = 146
	CELL = 32

	def __init__(self, inventory):
		ui.BoardWithTitleBar.__init__(self)
		self.inventory = WindowProxy(inventory)
		self.currentVnum = 0
		self.pendingVnum = 0
		self.rewards = []
		self.effects = []
		self.pages = []
		self.page = 0
		self.rewardWidgets = []
		self.backgroundWidgets = []
		self.cols = self.MIN_COLS
		self.widgets = []
		self.__Build()
		self.Hide()

	def __del__(self):
		ui.BoardWithTitleBar.__del__(self)

	def __Text(self, x, y, text):
		line = ui.TextLine()
		line.SetParent(self)
		line.SetPosition(x, y)
		line.SetText(text)
		line.Show()
		self.widgets.append(line)
		return line

	def __Button(self, x, text, callback):
		button = ui.Button()
		button.SetParent(self)
		button.SetPosition(x, 123)
		button.SetUpVisual("d:/ymir work/ui/public/small_button_01.sub")
		button.SetOverVisual("d:/ymir work/ui/public/small_button_02.sub")
		button.SetDownVisual("d:/ymir work/ui/public/small_button_03.sub")
		button.SetText(text)
		button.SAFE_SetEvent(callback)
		button.Show()
		self.widgets.append(button)
		return button

	def __Build(self):
		self.SetSize(self.WIDTH, self.HEIGHT)
		self.SetCenterPosition()
		self.AddFlag("movable")
		self.AddFlag("float")
		self.SetTitleName("Podgl\xb9d skrzynki")
		self.SetCloseEvent(ui.__mem_func__(self.Close))
		self.headerText = self.__Text(self.WIDTH // 2, 38, "Przeci\xb9gnij tu skrzynk\xea z ekwipunku")
		self.headerText.SetHorizontalAlignCenter()
		self.chestSlot = ChestInputSlot(self)
		self.chestSlot.SetParent(self)
		self.chestSlot.SetPosition(self.WIDTH // 2 - 20, 61)
		self.chestSlot.SetSize(40, 40)
		self.chestSlot.Show()
		self.widgets.append(self.chestSlot)
		self.chestSlotBackground = ui.ImageBox()
		self.chestSlotBackground.SetParent(self.chestSlot)
		self.chestSlotBackground.SetPosition(4, 4)
		self.chestSlotBackground.LoadImage(SLOT_IMAGE)
		self.chestSlotBackground.AddFlag("not_pick")
		self.chestSlotBackground.Show()
		self.widgets.append(self.chestSlotBackground)
		self.chestIcon = ui.ImageBox()
		self.chestIcon.SetParent(self.chestSlot)
		self.chestIcon.SetPosition(4, 4)
		self.chestIcon.AddFlag("not_pick")
		self.widgets.append(self.chestIcon)
		self.chestName = self.__Text(self.WIDTH // 2, 103, "")
		self.chestName.SetHorizontalAlignCenter()
		self.gridPanel = ui.Window()
		self.gridPanel.SetParent(self)
		self.gridPanel.SetPosition((self.WIDTH - self.cols * self.CELL) // 2, self.GRID_Y)
		self.gridPanel.SetSize(self.cols * self.CELL, self.CELL)
		self.gridPanel.Show()
		self.widgets.append(self.gridPanel)
		self.prevButton = self.__Button(16, "<", self.PreviousPage)
		self.nextButton = self.__Button(self.WIDTH - 43, ">", self.NextPage)
		self.pageText = self.__Text(self.WIDTH // 2, 126, "")
		self.pageText.SetHorizontalAlignCenter()
		self.status = self.__Text(self.WIDTH // 2, 126, "")
		self.status.SetHorizontalAlignCenter()
		self.prevButton.Hide()
		self.nextButton.Hide()
		self.pageText.Hide()
		self.status.Hide()

	def __ClearPage(self):
		self.HideRewardTip()
		for window in self.rewardWidgets:
			window.Hide()
		self.rewardWidgets = []
		for background in self.backgroundWidgets:
			background.Hide()
		self.backgroundWidgets = []

	def __Arrange(self):
		entries = self.rewards + [(-kind, count) for kind, count in self.effects]
		area = 0
		for vnum, count in entries:
			width, height = ItemSize(vnum, self.MAX_ROWS)
			area += width * height
		# Wider for more rewards, so a big group is a page or two and not a column.
		self.cols = max(self.MIN_COLS, min(self.MAX_COLS, int(math.ceil(math.sqrt(max(1, area) * 1.5)))))
		self.pages = LayOut(entries, self.cols, self.MAX_ROWS)
		self.page = 0
		self.__DrawPage()

	def __Resize(self, width, height, gridCols, gridRows):
		self.SetSize(width, height)
		self.headerText.SetPosition(width // 2, 38)
		self.chestSlot.SetPosition(width // 2 - 20, 61)
		self.chestName.SetPosition(width // 2, 103)
		self.status.SetPosition(width // 2, 126)
		self.pageText.SetPosition(width // 2, 126)
		self.nextButton.SetPosition(width - 43, 123)
		self.gridPanel.SetPosition((width - gridCols * self.CELL) // 2, self.GRID_Y)
		self.gridPanel.SetSize(gridCols * self.CELL, gridRows * self.CELL)

	def __DrawPage(self):
		self.__ClearPage()
		if not self.pages:
			return
		placed = self.pages[self.page]
		usedRows = max([y + height for x, y, width, height, vnum, count in placed] or [1])
		visibleCols = max(self.MIN_COLS, max([x + width for x, y, width, height, vnum, count in placed] or [1]))
		newWidth = max(self.WIDTH, visibleCols * self.CELL + 28)
		self.__Resize(newWidth, self.GRID_Y + usedRows * self.CELL + 14, visibleCols, usedRows)
		for y in range(usedRows):
			for x in range(visibleCols):
				background = ui.ImageBox()
				background.SetParent(self.gridPanel)
				background.SetPosition(x * self.CELL, y * self.CELL)
				background.LoadImage(SLOT_IMAGE)
				background.AddFlag("not_pick")
				background.Show()
				self.backgroundWidgets.append(background)
		for x, y, width, height, vnum, count in placed:
			slot = RewardSlot(self, vnum)
			slot.SetParent(self.gridPanel)
			slot.SetPosition(x * self.CELL, y * self.CELL)
			slot.SetSize(width * self.CELL, height * self.CELL)
			slot.Show()
			self.rewardWidgets.append(slot)
			iconPath = ""
			if vnum > 0:
				try:
					item.SelectItem(vnum)
					iconPath = item.GetIconImageFileName()
				except Exception:
					iconPath = ""
			if iconPath:
				icon = ui.ImageBox()
				icon.SetParent(slot)
				icon.SetPosition(0, 0)
				icon.LoadImage(iconPath)
				icon.AddFlag("not_pick")
				icon.Show()
				self.rewardWidgets.append(icon)
			else:
				label = ui.TextLine()
				label.SetParent(slot)
				label.SetPosition(2, 9)
				label.SetText(str(vnum) if vnum > 0 else EFFECT_NAMES.get(-vnum, "Inne"))
				label.AddFlag("not_pick")
				label.Show()
				self.rewardWidgets.append(label)
			if count > 1:
				label = ui.TextLine()
				label.SetParent(slot)
				label.SetPosition(slot.GetWidth() - 14, slot.GetHeight() - 15)
				label.SetOutline(True)
				label.SetText(str(count))
				label.AddFlag("not_pick")
				label.Show()
				self.rewardWidgets.append(label)
		if len(self.pages) > 1:
			self.pageText.SetText("Strona %d/%d" % (self.page + 1, len(self.pages)))
			self.pageText.Show()
		else:
			self.pageText.Hide()
		if self.page > 0:
			self.prevButton.Show()
		else:
			self.prevButton.Hide()
		if self.page + 1 < len(self.pages):
			self.nextButton.Show()
		else:
			self.nextButton.Hide()

	def PreviousPage(self):
		if self.page > 0:
			self.page -= 1
			self.__DrawPage()

	def NextPage(self):
		if self.page + 1 < len(self.pages):
			self.page += 1
			self.__DrawPage()

	def __ToolTip(self):
		try:
			return self.inventory.tooltipItem
		except (AttributeError, ReferenceError):
			return None

	def ShowRewardTip(self, vnum):
		if vnum <= 0:
			return
		tooltip = self.__ToolTip()
		if tooltip:
			try:
				tooltip.SetItemToolTip(vnum)
			except Exception:
				pass

	def HideRewardTip(self):
		tooltip = self.__ToolTip()
		if tooltip:
			try:
				tooltip.HideToolTip()
			except Exception:
				pass

	def __Reset(self):
		self.__ClearPage()
		self.currentVnum = 0
		self.pendingVnum = 0
		self.rewards = []
		self.effects = []
		self.pages = []
		self.page = 0
		self.cols = self.MIN_COLS
		self.pageText.SetText("")
		self.pageText.Hide()
		self.prevButton.Hide()
		self.nextButton.Hide()
		self.status.Hide()
		self.__Resize(self.WIDTH, self.HEIGHT, self.MIN_COLS, 1)

	def SetChest(self, inventorySlot, vnum):
		self.__Reset()
		self.pendingVnum = vnum
		self.status.SetText("Pobieram zawarto\x9c\xe6 z serwera...")
		self.status.Show()
		try:
			item.SelectItem(vnum)
			self.chestName.SetText(item.GetItemName())
			self.chestIcon.LoadImage(item.GetIconImageFileName())
			self.chestIcon.Show()
		except Exception:
			self.chestName.SetText("Skrzynka")
			self.chestIcon.Hide()
		net.SendChatPacket("/chest_preview %d" % inventorySlot)

	def ReceiveBegin(self, data):
		try:
			vnum = int(data)
		except ValueError:
			return
		if not self.IsShow() or vnum != self.pendingVnum:
			return
		self.currentVnum = vnum
		self.rewards = []
		self.effects = []

	def ReceiveItem(self, data):
		if not self.IsShow() or not self.currentVnum:
			return
		try:
			vnum, count = [int(value) for value in data.split("|", 1)]
		except ValueError:
			return
		if vnum > 0 and count > 0 and len(self.rewards) < MAX_REWARDS:
			self.rewards.append((vnum, count))

	def ReceiveEffect(self, data):
		if not self.IsShow() or not self.currentVnum:
			return
		try:
			kind, amount = [int(value) for value in data.split("|", 1)]
		except ValueError:
			return
		if len(self.effects) < MAX_REWARDS:
			self.effects.append((kind, amount))

	def ReceiveEnd(self, data):
		try:
			vnum = int(data)
		except ValueError:
			return
		if not self.IsShow() or self.currentVnum != vnum:
			return
		self.status.Hide()
		if not self.rewards and not self.effects:
			self.status.SetText("Ta skrzynka nic nie wydaje.")
			self.status.Show()
			return
		self.__Arrange()

	def ReceiveError(self, data):
		if not self.IsShow():
			return
		self.__Reset()
		self.chestIcon.Hide()
		self.chestName.SetText("")
		if data == "missing":
			self.status.SetText("Serwer nie zna zawarto\x9cci tej skrzynki.")
		else:
			self.status.SetText("To nie jest skrzynka.")
		self.status.Show()

	def Open(self):
		self.Show()
		self.SetCenterPosition()
		self.SetTop()

	def Close(self):
		self.__Reset()
		self.chestIcon.Hide()
		self.chestName.SetText("")
		self.Hide()

	def Toggle(self):
		if self.IsShow():
			self.Close()
		else:
			self.Open()

	def OnPressEscapeKey(self):
		self.Close()
		return True
