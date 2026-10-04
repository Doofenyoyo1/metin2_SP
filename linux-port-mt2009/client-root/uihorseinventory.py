import ui
import eventManager
import localeInfo
import flamewindPath
import special_flags
from _weakref import proxy
import uiInventory
import uiToolTip
import item
import colorInfo
import player
import utils
import skill
import uiCommon
import net

EVENT_OPEN_HORSE_INVENTORY = "EVENT_OPEN_HORSE_INVENTORY"

UNLOCK_REQUIREMENT_DATA = {
	0: {"horse": 1, "items":[]},
	# MT2009_PLUS_HORSE30_V1: the rows to horse level 30 (Digi Rasta) - the same
	# table as quest horse_inventory and the bots' PLAYERBOT_SADDLEBAG_ROWS.
	1: {"horse": 4, "items": [50050, 1, 30378, 20]},
	2: {"horse": 8, "items": [50050, 1, 30378, 30]},
	3: {"horse": 12, "items": [50050, 2, 30378, 40]},
	4: {"horse": 16, "items": [50050, 2, 30378, 60]},
	5: {"horse": 20, "items": [50050, 3, 30378, 80]},
	6: {"horse": 24, "items": [50050, 4, 30378, 100]},
	7: {"horse": 27, "items": [50050, 5, 30378, 150]},
	8: {"horse": 30, "items": [50050, 6, 30378, 200]},
}

class HorseInventoryWindow(ui.ScriptWindow):
	class HorseInventoryButton(ui.Window):
		def __init__(self, index):
			ui.Window.__init__(self)

			self.index = index
			self.SetSize(5*32, 32)

			slot = ui.ImageBox()
			slot.SetParent(self)
			slot.LoadImage(flamewindPath.GetPublic("horse_inv_lock_slot"))
			slot.Show()
			self.slotBg = slot

			btn = ui.Button()
			btn.SetParent(self)
			btn.SetWindowHorizontalAlignCenter()
			btn.SetWindowVerticalAlignCenter()
			btn.SetUpVisual(flamewindPath.GetPublic("horse_inv_lock_01"))
			btn.SetOverVisual(flamewindPath.GetPublic("horse_inv_lock_02"))
			btn.SetDownVisual(flamewindPath.GetPublic("horse_inv_lock_01"))
			btn.SetPosition(0, 0)
			btn.SAFE_SetEvent(self.OnButtonClick)
			btn.SAFE_SetStringEvent("MOUSE_OVER_IN", self.__ShowToolTip)
			btn.SAFE_SetStringEvent("MOUSE_OVER_OUT", self.HideToolTip)
			btn.Show()

			self.btn = btn

		def __del__(self):
			ui.Window.__del__(self)

		def Destroy(self):
			self.toolTip = None

		def SetToolTip(self, tooltip):
			self.toolTip = tooltip

		def __ShowToolTip(self):
			if self.toolTip:
				data = UNLOCK_REQUIREMENT_DATA[self.index]
				self.toolTip.ClearToolTip()
				self.toolTip.AppendTextLine(localeInfo.HORSE_INVENTORY_UNLOCK_INFO)
				self.toolTip.AppendSpace(5)
				self.toolTip.AppendHorizontalLine()
				hasLevel = (player.GetSkillGrade(109) * 20 + player.GetSkillLevel(109)) >= data["horse"]
				self.toolTip.AppendTextLine(localeInfo.HORSE_INVENTORY_UNLOCK_LEVEL_REQ % data["horse"],
												colorInfo.POSITIVE_COLOR if hasLevel else colorInfo.NEGATIVE_COLOR)

				self.toolTip.AppendSpace(5)
				self.toolTip.AppendTextLine(localeInfo.SPECIAL_SHOP_REQ_ITEMS, isBold=True)

				reqItems = data["items"]
				column_block = ui.ColumnBlock(2)
				i = 0
				for i in range(0, len(reqItems), 2):
					itemVnum = reqItems[i]
					count = reqItems[i+1]

					item.SelectItem(itemVnum)
					countInInventory = utils.CountItemCountInInventory(itemVnum)
					hasEnoughItems = countInInventory >= count

					itemInfo = uiToolTip.ToolTip(isNoBackground=True)
					itemInfo.followFlag = False
					itemInfo.AppendImage(item.GetIconImageFileName())
					itemInfo.AppendSpace(1)

					itemName = ("%dx " + item.GetItemName() + " (%d na %d)") % (count, countInInventory, count)
					itemInfo.AppendTextLine(itemName,
											colorInfo.POSITIVE_COLOR if hasEnoughItems else colorInfo.NEGATIVE_COLOR)
					itemInfo.ShowToolTip()

					column_block.AddItem(i/2, itemInfo)
					i += 1
				column_block.Show()
				column_block.SetColumnWidth(150)
				self.toolTip.AppendColumnBlock(column_block)

				self.toolTip.ShowToolTip()

		def HideToolTip(self):
			if self.toolTip:
				self.toolTip.HideToolTip()

		def OnButtonClick(self):
			net.SendChatPacket("/horse_inventory_unlock")

	def __init__(self):
		ui.ScriptWindow.__init__(self)
		self.__Initialize()
		self.LoadDialog()
		eventManager.EventManager().add_observer(EVENT_OPEN_HORSE_INVENTORY, self.Open)
		eventManager.EventManager().add_observer(uiInventory.EVENT_CLOSE_INVENTORY, self.Close)
		eventManager.EventManager().add_observer(eventManager.SPECIAL_FLAG_UPDATE, self.OnSpecialFlagUpdate)
		eventManager.EventManager().add_observer(eventManager.EVENT_HORSE_SUMMON, self.OnHorseSummon)

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def SetToolTip(self, toolTip):
		self.toolTip = proxy(toolTip)
		for i in self.lockSlots:
			i.SetToolTip(self.toolTip)

	def __Initialize(self):
		self.lockSlots = []
		self.itemToolTip = None

	@ui.WindowDestroy
	def Destroy(self):
		for i in self.lockSlots:
			i.Destroy()

		self.__Initialize()
		self.ClearDictionary()

	def LoadDialog(self):
		try:
			PythonScriptLoader = ui.PythonScriptLoader()
			PythonScriptLoader.LoadScriptFile(self, "UIScript/horseinventorywindow.py")

			self.titleBar = self.GetChild("TitleBar")
			self.itemSlot = self.GetChild("ItemSlot")
			self.noHorseBackground = self.GetChild("NoHorseBackground")

			self.titleBar.SetCloseEvent(ui.__mem_func__(self.Close))

			# self.itemSlot.SetSlotStyle(wndMgr.SLOT_STYLE_NONE)
			# self.itemSlot.SetOverInItemEvent(ui.__mem_func__(self.__ShowToolTip))
			# self.itemSlot.SetOverOutItemEvent(ui.__mem_func__(self.__HideToolTip))

			self.__CreateLockedSlots()

		except:
			import exception
			exception.Abort("HorseInventoryWindow.LoadDialog.BindObject")

	def __CreateLockedSlots(self):
		MAX_LOCK_SLOTS = 9
		for i in range(MAX_LOCK_SLOTS):
			btn = self.HorseInventoryButton(i)
			btn.SetParent(self.itemSlot)
			btn.SetPosition(0, 32 * i)
			btn.Show()
			self.lockSlots.append(btn)

	def __RefreshLockedSlots(self):
		unlockCount = special_flags.GetFlag(special_flags.HORSE_INVENTORY_SLOT)

		MAX_LOCK_SLOTS = 9
		lock_slots = max(MAX_LOCK_SLOTS - unlockCount, 0)
		for i in range(MAX_LOCK_SLOTS):
			slot = self.lockSlots[i]
			if i < MAX_LOCK_SLOTS - lock_slots:
				slot.Hide()
			else:
				if i == unlockCount:
					slot.btn.Show()
				else:
					slot.btn.Hide()

				slot.Show()

	def OnSpecialFlagUpdate(self, flag, value):
		if flag == special_flags.HORSE_INVENTORY_SLOT:
			self.__RefreshLockedSlots()

	def OnHorseSummon(self, isSummoned):
		if isSummoned:
			self.noHorseBackground.Hide()
		else:
			self.noHorseBackground.Show()

	def Open(self):
		if special_flags.GetFlag(special_flags.HORSE_INVENTORY_SLOT) < 1:
			eventManager.EventManager().send_event(uiCommon.POPUP_EVENT, localeInfo.HORSE_INVENTORY_LOCKED)
			return

		if self.IsShow():
			self.Close()
			return

		self.Show()
		self.__RefreshLockedSlots()

	def Close(self):
		self.Hide()

		for i in self.lockSlots:
			i.HideToolTip()

	def OnPressEscapeKey(self):
		self.Close()
		return True
