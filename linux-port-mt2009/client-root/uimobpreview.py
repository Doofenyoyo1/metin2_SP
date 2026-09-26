# What the target can drop (Gibon's window, 26 September): the "?" beside
# the target's health bar (uitarget.py) opens a strip of item icons under it.
#
# The target goes to the server as its VID, "/mob_drop_preview <vid>", and the
# server - which checks it is a monster or a Metin stone on the asker's map,
# within five thousand units - answers with what the tables this game process
# loaded give for its race (do_mob_drop_preview in cmd_general.cpp, and
# ITEM_MANAGER::SendMobDropPreview, playerbotify.py's apply_chest_preview):
#
#     MobPreviewBegin <race>
#     MobPreviewItem <vnum>|<count>
#     MobPreviewEnd <race>
#     MobPreviewError <why>
#
# A quest's drop and the common drop by level are not in it: the first
# depends on the quest, the second on the killer. A boss names a hundred and
# fifty items and more, so the strip has pages. It follows the health bar and
# closes when the target changes, dies or walks off (game.py's OnUpdate).
#
# The texts are CP1250 escapes so the file stays ASCII. Python 2.7 as the
# client has it; the windows it knows are weak proxies, made by
# uichestpreview.WindowProxy, which takes a proxy as it is.

import net
import ui

import uichestpreview

SLOT_IMAGE = uichestpreview.SLOT_IMAGE


class DropSlot(ui.Window):
	def __init__(self, owner, vnum):
		ui.Window.__init__(self)
		self.owner = uichestpreview.WindowProxy(owner)
		self.vnum = vnum

	def OnMouseOverIn(self):
		self.owner.ShowRewardTip(self.vnum)

	def OnMouseOverOut(self):
		self.owner.HideRewardTip()


class MobDropWindow(ui.ThinBoard):
	COLS = 12
	CELL = 32
	PAD = 10
	MAX_ROWS = 8
	MAX_REWARDS = 400
	PAGER_HEIGHT = 24

	def __init__(self, inventory, targetBoard):
		ui.ThinBoard.__init__(self)
		self.inventory = uichestpreview.WindowProxy(inventory)
		self.targetBoard = uichestpreview.WindowProxy(targetBoard)
		self.targetVID = 0
		self.pendingVnum = 0
		self.currentVnum = 0
		self.rewards = []
		self.pages = []
		self.page = 0
		self.widgets = []
		self.prevButton = self.__PageButton("<", self.PreviousPage)
		self.nextButton = self.__PageButton(">", self.NextPage)
		self.pageText = ui.TextLine()
		self.pageText.SetParent(self)
		self.pageText.SetHorizontalAlignCenter()
		self.pageText.Hide()
		self.SetSize(self.COLS * self.CELL + 2 * self.PAD, self.CELL + 2 * self.PAD)
		self.FollowTarget()
		self.Hide()

	def __PageButton(self, text, callback):
		button = ui.Button()
		button.SetParent(self)
		button.SetUpVisual("d:/ymir work/ui/public/small_button_01.sub")
		button.SetOverVisual("d:/ymir work/ui/public/small_button_02.sub")
		button.SetDownVisual("d:/ymir work/ui/public/small_button_03.sub")
		button.SetText(text)
		button.SAFE_SetEvent(callback)
		button.Hide()
		return button

	def FollowTarget(self):
		x, y = self.targetBoard.GetGlobalPosition()
		self.SetPosition(x + (self.targetBoard.GetWidth() - self.GetWidth()) // 2,
			y + self.targetBoard.GetHeight() - 3)

	def __Clear(self):
		self.HideRewardTip()
		for widget in self.widgets:
			widget.Hide()
		self.widgets = []
		self.prevButton.Hide()
		self.nextButton.Hide()
		self.pageText.Hide()

	def __Label(self, message):
		self.__Clear()
		self.SetSize(self.COLS * self.CELL + 2 * self.PAD, self.CELL + 2 * self.PAD)
		self.FollowTarget()
		label = ui.TextLine()
		label.SetParent(self)
		label.SetPosition(self.GetWidth() // 2, 18)
		label.SetHorizontalAlignCenter()
		label.SetText(message)
		label.Show()
		self.widgets.append(label)

	def OpenForMob(self, vid, vnum, name=""):
		if self.IsShow() and self.targetVID == vid:
			self.Close()
			return
		self.Close()
		if vnum <= 0:
			return
		self.targetVID = vid
		self.pendingVnum = vnum
		self.FollowTarget()
		self.Show()
		self.SetTop()
		self.__Label("Pobieram drop...")
		net.SendChatPacket("/mob_drop_preview %d" % vid)

	def ReceiveBegin(self, data):
		try:
			vnum = int(data)
		except ValueError:
			return
		if self.IsShow() and vnum == self.pendingVnum:
			self.currentVnum = vnum
			self.rewards = []

	def ReceiveItem(self, data):
		if not self.IsShow() or not self.currentVnum:
			return
		try:
			vnum, count = [int(value) for value in data.split("|", 1)]
		except ValueError:
			return
		if vnum <= 0 or count <= 0:
			return
		for index, (seenVnum, seenCount) in enumerate(self.rewards):
			if seenVnum == vnum:
				# The tables are separate draws: the same item from two of them
				# is shown once, with the bigger stack, and not summed.
				self.rewards[index] = (vnum, max(seenCount, count))
				return
		if len(self.rewards) < self.MAX_REWARDS:
			self.rewards.append((vnum, count))

	def ReceiveEnd(self, data):
		try:
			vnum = int(data)
		except ValueError:
			return
		if self.IsShow() and self.currentVnum == vnum:
			if not self.rewards:
				self.__Label("Nic nie wypada z tabel dropu")
				return
			self.pages = uichestpreview.LayOut(self.rewards, self.COLS, self.MAX_ROWS)
			self.page = 0
			self.__DrawPage()

	def ReceiveError(self, data):
		if self.IsShow():
			self.currentVnum = 0
			self.__Label("Brak danych o dropie")

	def __DrawPage(self):
		self.__Clear()
		placed = self.pages[self.page]
		rows = max([y + h for x, y, w, h, vnum, count in placed] or [1])
		paged = len(self.pages) > 1
		if paged:
			# Every page as tall as the grid, so the arrows stay where they are.
			rows = self.MAX_ROWS
		height = rows * self.CELL + 2 * self.PAD + (self.PAGER_HEIGHT if paged else 0)
		self.SetSize(self.COLS * self.CELL + 2 * self.PAD, height)
		self.FollowTarget()
		for y in range(rows):
			for x in range(self.COLS):
				bg = ui.ImageBox()
				bg.SetParent(self)
				bg.SetPosition(self.PAD + x * self.CELL, self.PAD + y * self.CELL)
				bg.LoadImage(SLOT_IMAGE)
				bg.AddFlag("not_pick")
				bg.Show()
				self.widgets.append(bg)
		for x, y, w, h, vnum, count in placed:
			slot = DropSlot(self, vnum)
			slot.SetParent(self)
			slot.SetPosition(self.PAD + x * self.CELL, self.PAD + y * self.CELL)
			slot.SetSize(w * self.CELL, h * self.CELL)
			slot.Show()
			self.widgets.append(slot)
			try:
				uichestpreview.item.SelectItem(vnum)
				icon = ui.ImageBox()
				icon.SetParent(slot)
				icon.LoadImage(uichestpreview.item.GetIconImageFileName())
				icon.AddFlag("not_pick")
				icon.Show()
				self.widgets.append(icon)
			except Exception:
				pass
			if count > 1:
				label = ui.TextLine()
				label.SetParent(slot)
				label.SetPosition(slot.GetWidth() - 15, slot.GetHeight() - 15)
				label.SetOutline(True)
				label.SetText(str(count))
				label.AddFlag("not_pick")
				label.Show()
				self.widgets.append(label)
		if paged:
			pagerY = self.PAD + rows * self.CELL + 2
			self.prevButton.SetPosition(self.PAD, pagerY)
			self.nextButton.SetPosition(self.GetWidth() - self.PAD - 43, pagerY)
			self.pageText.SetPosition(self.GetWidth() // 2, pagerY + 3)
			self.pageText.SetText("Strona %d/%d" % (self.page + 1, len(self.pages)))
			self.pageText.Show()
			if self.page > 0:
				self.prevButton.Show()
			if self.page + 1 < len(self.pages):
				self.nextButton.Show()

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

	def Close(self):
		self.__Clear()
		self.targetVID = 0
		self.pendingVnum = 0
		self.currentVnum = 0
		self.rewards = []
		self.pages = []
		self.page = 0
		self.Hide()
