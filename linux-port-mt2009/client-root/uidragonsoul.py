import ui
import player
import mouseModule
import net
import app
import snd
import item
import chat
import grp
import uiScriptLocale
import uiRefine
import uiAttachMetin
import uiPickMoney
import uiCommon
import uiPrivateShopBuilder
import localeInfo
import constInfo
import ime
import uiInventory
import sys
import dbg
if app.ENABLE_DS_GRADE_MYTH:
	import wndMgr, uiToolTip
ITEM_FLAG_APPLICABLE = 1 << 14

# MT2009_PLUS_VEKIRION_V1 (Autor: Vekirion), MT2009_PLUS_REFINE_ALL_V1:
# texts of the "Wszystkie" button (cp1250).
REFINE_ALL_TEXT = "Wszystkie"
REFINE_ALL_STOP_TEXT = "Stop"
REFINE_ALL_NOTHING = "Brak kamieni do uszlachetnienia."
REFINE_ALL_ONLY_MODES = "Wszystkie: tylko %s lub %s."
REFINE_ALL_QUESTION = "Uszlachetni\xe6 %d razy za %s?"
REFINE_ALL_SUMMARY = "Wszystkie: %d pr\xf3b (udane: %d, nieudane: %d)."

# MT2009_PLUS_ALCHEMY_MSG_V1: the Alchemy's message window shows the latest
# message only. The stock uiCommon.PopupDialog.SetText adds a line to the
# lines already there (even after Close), so every refusal of the refine
# window grew the box by a line for good. Each message here replaces the last,
# and the box goes back to its size first.
class OneMessagePopupDialog(uiCommon.PopupDialog):
	def __init__(self):
		uiCommon.PopupDialog.__init__(self)
		self.baseSize = (self.GetWidth(), self.GetHeight())

	def SetText(self, text):
		for message in self.messages:
			message.Hide()
		self.messages = []
		(width, height) = self.baseSize
		self.SetSize(width, height)
		self.board.SetSize(width, height)
		uiCommon.PopupDialog.SetText(self, text)

def _IsItemUsedForDragonSoul(srcWindow, srcSlot, dstWindow, dstSlot):
	itemVnum = player.GetItemIndex(srcWindow, srcSlot)
	if not itemVnum:
		return False
	item.SelectItem(itemVnum)
	itemType = item.GetItemType()
	itemSubType = item.GetItemSubType()
	if itemType == item.ITEM_TYPE_USE and itemSubType in (item.USE_TIME_CHARGE_PER, item.USE_TIME_CHARGE_FIX):
		return dstWindow in (player.DRAGON_SOUL_INVENTORY, player.INVENTORY)
	if itemType == item.ITEM_TYPE_EXTRACT:
		if itemSubType == item.EXTRACT_DRAGON_SOUL:
			return dstWindow == player.INVENTORY
		if itemSubType == item.EXTRACT_DRAGON_HEART:
			return dstWindow == player.DRAGON_SOUL_INVENTORY
	return False

# 용혼석 Vnum에 대한 comment
# ITEM VNUM을 10만 자리부터, FEDCBA라고 한다면
# FE : 용혼석 종류.	D : 등급
# C : 단계			B : 강화
# A : 여벌의 번호들...

class DragonSoulWindow(ui.ScriptWindow):

	if app.ENABLE_MAILBOX:
		interface = None

	def __init__(self):
		self.KIND_TAP_TITLES = [
			uiScriptLocale.DRAGONSOUL_TAP_TITLE_1,
			uiScriptLocale.DRAGONSOUL_TAP_TITLE_2,
			uiScriptLocale.DRAGONSOUL_TAP_TITLE_3,
			uiScriptLocale.DRAGONSOUL_TAP_TITLE_4,
			uiScriptLocale.DRAGONSOUL_TAP_TITLE_5,
			uiScriptLocale.DRAGONSOUL_TAP_TITLE_6,
			uiScriptLocale.DRAGONSOUL_TAP_TITLE_7
		]

		if app.ENABLE_DS_GRADE_MYTH:
			self.PAGE_TAB_TITLES = [
				uiScriptLocale.DRAGONSOUL_PAGE_BUTTON_1,
				uiScriptLocale.DRAGONSOUL_PAGE_BUTTON_2,
				uiScriptLocale.DRAGONSOUL_PAGE_BUTTON_3,
				uiScriptLocale.DRAGONSOUL_PAGE_BUTTON_4,
				uiScriptLocale.DRAGONSOUL_PAGE_BUTTON_5,
				uiScriptLocale.DRAGONSOUL_PAGE_BUTTON_6
			]

		ui.ScriptWindow.__init__(self)

		self.questionDialog = None
		self.tooltipItem = None
		if app.ENABLE_DS_GRADE_MYTH:
			self.toolTip = uiToolTip.ToolTip()
		self.sellingSlotNumber = -1
		self.isLoaded = 0
		self.isActivated = False
		self.DSKindIndex = 0
		self.tabDict = None
		self.tabButtonDict = None
		self.deckPageIndex = 0
		self.inventoryPageIndex = 0
		self.SetWindowName("DragonSoulWindow")
		self.__LoadWindow()
		self.interface = None

		if app.ENABLE_DS_SET:
			self.setGrade = 0

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def Show(self):
		self.__LoadWindow()

		ui.ScriptWindow.Show(self)
		self.SetTop()

		if app.ENABLE_MAILBOX:
			self.RefreshItemSlot()

	def __LoadWindow(self):
		if self.isLoaded == 1:
			return
		self.isLoaded = 1

		try:
			pyScrLoader = ui.PythonScriptLoader()
			pyScrLoader.LoadScriptFile(self, "UIScript/DragonSoulWindow.py")
		except:
			import exception
			exception.Abort("dragonsoulwindow.LoadWindow.LoadObject")

		try:
			if localeInfo.IsARABIC():
				self.board = self.GetChild("Equipment_Base")
				self.board.SetScale(-1.0, 1.0)
				self.board.SetRenderingRect(-1.0, 0.0, 1.0, 0.0)
				self.board = self.GetChild("Tab_01")
				self.board.SetScale(-1.0, 1.0)
				self.board.SetRenderingRect(-1.0, 0.0, 1.0, 0.0)
				self.board = self.GetChild("Tab_02")
				self.board.SetScale(-1.0, 1.0)
				self.board.SetRenderingRect(-1.0, 0.0, 1.0, 0.0)
				self.board = self.GetChild("Tab_03")
				self.board.SetScale(-1.0, 1.0)
				self.board.SetRenderingRect(-1.0, 0.0, 1.0, 0.0)
				self.board = self.GetChild("Tab_04")
				self.board.SetScale(-1.0, 1.0)
				self.board.SetRenderingRect(-1.0, 0.0, 1.0, 0.0)
				self.board = self.GetChild("Tab_05")
				self.board.SetScale(-1.0, 1.0)
				self.board.SetRenderingRect(-1.0, 0.0, 1.0, 0.0)
				self.board = self.GetChild("Tab_06")
				self.board.SetScale(-1.0, 1.0)
				self.board.SetRenderingRect(-1.0, 0.0, 1.0, 0.0)
				self.board = self.GetChild("Tab_07")
				self.board.SetScale(-1.0, 1.0)
				self.board.SetRenderingRect(-1.0, 0.0, 1.0, 0.0)

			wndItem = self.GetChild("ItemSlot")
			wndEquip = self.GetChild("EquipmentSlot")
			self.activateButton = self.GetChild("activate")
			self.deckTab = []
			self.deckTab.append(self.GetChild("deck1"))
			self.deckTab.append(self.GetChild("deck2"))
			self.GetChild("TitleBar").SetCloseEvent(ui.__mem_func__(self.Close))
			self.inventoryTab = []
			self.inventoryTab.append(self.GetChild("Inventory_Tab_01"))
			self.inventoryTab.append(self.GetChild("Inventory_Tab_02"))
			self.inventoryTab.append(self.GetChild("Inventory_Tab_03"))
			self.inventoryTab.append(self.GetChild("Inventory_Tab_04"))
			self.inventoryTab.append(self.GetChild("Inventory_Tab_05"))
			if app.ENABLE_DS_GRADE_MYTH:
				self.inventoryTab.append(self.GetChild("Inventory_Tab_06"))
			self.tabDict = {
				0 : self.GetChild("Tab_01"),
				1 : self.GetChild("Tab_02"),
				2 : self.GetChild("Tab_03"),
				3 : self.GetChild("Tab_04"),
				4 : self.GetChild("Tab_05"),
				5 : self.GetChild("Tab_06"),
				6 : self.GetChild("Tab_07"),
			}
			self.tabButtonDict = {
				0 : self.GetChild("Tab_Button_01"),
				1 : self.GetChild("Tab_Button_02"),
				2 : self.GetChild("Tab_Button_03"),
				3 : self.GetChild("Tab_Button_04"),
				4 : self.GetChild("Tab_Button_05"),
				5 : self.GetChild("Tab_Button_06"),
				6 : self.GetChild("Tab_Button_07"),
			}
			self.tabText = self.GetChild("tab_text_area")

		except:
			import exception
			exception.Abort("InventoryWindow.LoadWindow.BindObject")

		## DragonSoul Kind Tap
		for (tabKey, tabButton) in self.tabButtonDict.items():
			tabButton.SetEvent(ui.__mem_func__(self.SetDSKindIndex), tabKey)

		## Item
		wndItem.SetOverInItemEvent(ui.__mem_func__(self.OverInItem))
		wndItem.SetOverOutItemEvent(ui.__mem_func__(self.OverOutItem))
		wndItem.SetSelectItemSlotEvent(ui.__mem_func__(self.SelectItemSlot))
		wndItem.SetSelectEmptySlotEvent(ui.__mem_func__(self.SelectEmptySlot))
		wndItem.SetUnselectItemSlotEvent(ui.__mem_func__(self.UseItemSlot))
		wndItem.SetUseSlotEvent(ui.__mem_func__(self.UseItemSlot))

		## Equipment
		wndEquip.SetSelectEmptySlotEvent(ui.__mem_func__(self.SelectEmptyEquipSlot))
		wndEquip.SetSelectItemSlotEvent(ui.__mem_func__(self.SelectEquipItemSlot))
		wndEquip.SetUnselectItemSlotEvent(ui.__mem_func__(self.UseEquipItemSlot))
		wndEquip.SetUseSlotEvent(ui.__mem_func__(self.UseEquipItemSlot))
		wndEquip.SetOverInItemEvent(ui.__mem_func__(self.OverInEquipItem))
		wndEquip.SetOverOutItemEvent(ui.__mem_func__(self.OverOutEquipItem))

		## Deck
		self.deckTab[0].SetToggleDownEvent(ui.__mem_func__(self.SetDeckPage), 0)
		self.deckTab[1].SetToggleDownEvent(ui.__mem_func__(self.SetDeckPage), 1)
		self.deckTab[0].SetToggleUpEvent(ui.__mem_func__(self.__DeckButtonDown), 0)
		self.deckTab[1].SetToggleUpEvent(ui.__mem_func__(self.__DeckButtonDown), 1)
		self.deckTab[0].Down()

		## Grade button
		self.inventoryTab[0].SetEvent(ui.__mem_func__(self.SetInventoryPage), 0)
		self.inventoryTab[1].SetEvent(ui.__mem_func__(self.SetInventoryPage), 1)
		self.inventoryTab[2].SetEvent(ui.__mem_func__(self.SetInventoryPage), 2)
		self.inventoryTab[3].SetEvent(ui.__mem_func__(self.SetInventoryPage), 3)
		self.inventoryTab[4].SetEvent(ui.__mem_func__(self.SetInventoryPage), 4)
		if app.ENABLE_DS_GRADE_MYTH:
			self.inventoryTab[5].SetEvent(ui.__mem_func__(self.SetInventoryPage), 5)
		self.inventoryTab[0].Down()

		## Etc
		self.wndItem = wndItem
		self.wndEquip = wndEquip

		self.dlgQuestion = uiCommon.QuestionDialog2()
		self.dlgQuestion.Close()

		self.activateButton.SetToggleDownEvent(ui.__mem_func__(self.ActivateButtonClick))
		self.activateButton.SetToggleUpEvent(ui.__mem_func__(self.ActivateButtonClick))
		self.wndPopupDialog = OneMessagePopupDialog()	# MT2009_PLUS_ALCHEMY_MSG_V1

		##
		self.listHighlightedSlot = []

		## Refresh
		self.SetInventoryPage(0)
		self.RefreshItemSlot()
		self.RefreshEquipSlotWindow()
		self.RefreshBagSlotWindow()
		self.SetDSKindIndex(0)
		self.activateButton.Enable()
		self.deckTab[self.deckPageIndex].Down()
		self.activateButton.SetUp()

	def Destroy(self):
		self.ClearDictionary()
		self.tooltipItem = None
		if app.ENABLE_DS_GRADE_MYTH:
			self.toolTip = None
		self.wndItem = 0
		self.wndEquip = 0
		self.activateButton = 0
		self.questionDialog = None
		self.mallButton = None
		self.inventoryTab = []
		self.deckTab = []
		self.equipmentTab = []
		self.tabDict = None
		self.tabButtonDict = None

	def Close(self):
		if None != self.tooltipItem:
			self.tooltipItem.HideToolTip()

		if app.ENABLE_DS_GRADE_MYTH:
			if None != self.toolTip:
				self.toolTip.Hide()

		self.Hide()

	def __DeckButtonDown(self, deck):
		self.deckTab[deck].Down()

	def SetInventoryPage(self, page):
		if self.inventoryPageIndex != page:
			self.__HighlightSlot_ClearCurrentPage()
		self.inventoryPageIndex = page
		if app.ENABLE_DS_GRADE_MYTH:
			self.inventoryTab[(page + 1) % 6].SetUp()
			self.inventoryTab[(page + 2) % 6].SetUp()
			self.inventoryTab[(page + 3) % 6].SetUp()
			self.inventoryTab[(page + 4) % 6].SetUp()
			self.inventoryTab[(page + 5) % 6].SetUp()
		else:
			self.inventoryTab[(page + 1) % 5].SetUp()
			self.inventoryTab[(page + 2) % 5].SetUp()
			self.inventoryTab[(page + 3) % 5].SetUp()
			self.inventoryTab[(page + 4) % 5].SetUp()
		self.RefreshBagSlotWindow()

	def SetItemToolTip(self, tooltipItem):
		self.tooltipItem = tooltipItem

	def RefreshItemSlot(self):
		self.RefreshBagSlotWindow()
		self.RefreshEquipSlotWindow()

	if app.ENABLE_DS_GRADE_MYTH:
		def OverInInventoryTab(self, page):
			if self.toolTip:
				tooltip = self.PAGE_TAB_TITLES[page]
				arglen = len(tooltip)
				pos_x, pos_y = wndMgr.GetMousePosition()
				self.toolTip.ClearToolTip()
				self.toolTip.SetToolTipPosition(pos_x, pos_y)
				self.toolTip.AppendTextLine(tooltip)
				self.toolTip.Show()

		def OverOutInventoryTab(self, page):
			if self.toolTip:
				self.toolTip.Hide()

		def OnUpdate(self):
			if self.toolTip:
				for i in xrange(len(self.PAGE_TAB_TITLES)):
					if self.inventoryTab[i].IsIn():
						self.OverInInventoryTab(i)
						break
					else:
						self.OverOutInventoryTab(i)

	def RefreshEquipSlotWindow(self):
		for i in xrange(player.DRAGON_SOUL_EQUIPMENT_FIRST_SIZE):
			slotNumber = self.__GetEquipSlotPos(player.DRAGON_SOUL_EQUIPMENT_SLOT_START + i)
			itemVnum = player.GetItemIndex(player.INVENTORY, slotNumber)

			self.wndEquip.SetItemSlot(player.DRAGON_SOUL_EQUIPMENT_SLOT_START + i, itemVnum, 0)
			self.wndEquip.EnableSlot(player.DRAGON_SOUL_EQUIPMENT_SLOT_START + i)

			if itemVnum != 0:
				item.SelectItem(itemVnum)
				for j in xrange(item.LIMIT_MAX_NUM):
					(limitType, limitValue) = item.GetLimit(j)

					# 밑에서 remain_time이 0이하인지 체크 하기 때문에 임의의 양수로 초기화
					remain_time = 999
					# 일단 현재 타이머는 이 세개 뿐이다.
					if item.LIMIT_REAL_TIME == limitType:
						remain_time = player.GetItemMetinSocket(player.INVENTORY, slotNumber, 0) - app.GetGlobalTimeStamp()
					elif item.LIMIT_REAL_TIME_START_FIRST_USE == limitType:
						remain_time = player.GetItemMetinSocket(player.INVENTORY, slotNumber, 0) - app.GetGlobalTimeStamp()
					elif item.LIMIT_TIMER_BASED_ON_WEAR == limitType:
						remain_time = player.GetItemMetinSocket(player.INVENTORY, slotNumber, 0)

					if remain_time <= 0:
						self.wndEquip.DisableSlot(player.DRAGON_SOUL_EQUIPMENT_SLOT_START + i)
						break

		self.wndEquip.RefreshSlot()

	#def ActivateEquipSlotWindow(self, deck):
	#	for i in xrange(player.DRAGON_SOUL_EQUIPMENT_FIRST_SIZE):
	#		if deck == 2:
	#			plusCount = player.DRAGON_SOUL_EQUIPMENT_FIRST_SIZE
	#		else:
	#			plusCount = 0
	#		self.wndEquip.ActivateSlot(player.DRAGON_SOUL_EQUIPMENT_SLOT_START + i + plusCount)

	#def DeactivateEquipSlotWindow(self):
	#	for i in xrange(12):
	#		self.wndEquip.DeactivateSlot(player.DRAGON_SOUL_EQUIPMENT_SLOT_START + i)

	def RefreshStatus(self):
		self.RefreshItemSlot()

	def __GetEquipSlotPos(self, pos):
		return self.deckPageIndex * player.DRAGON_SOUL_EQUIPMENT_FIRST_SIZE + pos

	def __InventoryLocalSlotPosToGlobalSlotPos(self, window_type, local_slot_pos):
		if player.INVENTORY == window_type:
			return self.deckPageIndex * player.DRAGON_SOUL_EQUIPMENT_FIRST_SIZE + local_slot_pos

		if app.ENABLE_DS_GRADE_MYTH:
			return (self.DSKindIndex * player.DRAGON_SOUL_PAGE_COUNT * player.DRAGON_SOUL_PAGE_SIZE) + self.inventoryPageIndex * player.DRAGON_SOUL_PAGE_SIZE + local_slot_pos
		else:
			return (self.DSKindIndex * 5 * player.DRAGON_SOUL_PAGE_SIZE) + self.inventoryPageIndex * player.DRAGON_SOUL_PAGE_SIZE + local_slot_pos

	def RefreshBagSlotWindow(self):
		if not self.wndItem:
			return

		getItemVNum = player.GetItemIndex
		getItemCount = player.GetItemCount
		setItemVnum = self.wndItem.SetItemSlot

		if app.ENABLE_MAILBOX:
			if self.interface:
				onTopWindow = self.interface.GetOnTopWindow()

		for i in xrange(player.DRAGON_SOUL_PAGE_SIZE):
			self.wndItem.EnableSlot(i)
			#<- dragon soul kind
			slotNumber = self.__InventoryLocalSlotPosToGlobalSlotPos(player.DRAGON_SOUL_INVENTORY, i)

			itemCount = getItemCount(player.DRAGON_SOUL_INVENTORY, slotNumber)
			if 0 == itemCount:
				self.wndItem.ClearSlot(i)
				continue
			elif 1 == itemCount:
				itemCount = 0

			itemVnum = getItemVNum(player.DRAGON_SOUL_INVENTORY, slotNumber)
			setItemVnum(i, itemVnum, itemCount)

			if itemVnum != 0:
				item.SelectItem(itemVnum)
				for j in xrange(item.LIMIT_MAX_NUM):
					(limitType, limitValue) = item.GetLimit(j)

					# 밑에서 remain_time이 음수인지 체크 하기 때문에 임의의 양수로 초기화
					remain_time = 999
					if item.LIMIT_REAL_TIME == limitType:
						remain_time = player.GetItemMetinSocket(player.DRAGON_SOUL_INVENTORY, slotNumber, 0)
					elif item.LIMIT_REAL_TIME_START_FIRST_USE == limitType:
						remain_time = player.GetItemMetinSocket(player.DRAGON_SOUL_INVENTORY, slotNumber, 0)
					elif item.LIMIT_TIMER_BASED_ON_WEAR == limitType:
						remain_time = player.GetItemMetinSocket(player.DRAGON_SOUL_INVENTORY, slotNumber, 0)

					if remain_time <= 0:
						self.wndItem.DisableSlot(i)
						break

			if app.ENABLE_MAILBOX:
				if itemVnum and self.interface and onTopWindow:
					if self.interface.MarkUnusableDSInvenSlotOnTopWnd(onTopWindow, slotNumber):
						self.wndItem.SetUnusableSlotOnTopWnd(i)
					else:
						self.wndItem.SetUsableSlotOnTopWnd(i)
				else:
					self.wndItem.SetUsableSlotOnTopWnd(i)

		self.__HighlightSlot_RefreshCurrentPage()
		self.wndItem.RefreshSlot()

	def ShowToolTip(self, window_type, slotIndex):
		if None != self.tooltipItem:
			self.tooltipItem.SetInventoryItem(slotIndex, window_type)

	def OnPressEscapeKey(self):
		self.Close()
		return True

	def OnTop(self):
		if None != self.tooltipItem:
			self.tooltipItem.SetTop()

	# item slot 관련 함수
	def OverOutItem(self):
		self.wndItem.SetUsableItem(False)
		if None != self.tooltipItem:
			self.tooltipItem.HideToolTip()

	def OverInItem(self, overSlotPos):
		self.wndItem.DeactivateSlot(overSlotPos)
		overSlotPos = self.__InventoryLocalSlotPosToGlobalSlotPos(player.DRAGON_SOUL_INVENTORY, overSlotPos)

		try:
			self.listHighlightedSlot.remove(overSlotPos)
		except:
			pass

		if mouseModule.mouseController.isAttached():
			attachedItemType = mouseModule.mouseController.GetAttachedType()
			if player.SLOT_TYPE_INVENTORY == attachedItemType:

				attachedSlotType = mouseModule.mouseController.GetAttachedType()
				attachedSlotPos = mouseModule.mouseController.GetAttachedSlotNumber()
				attachedInvenType = player.SlotTypeToInvenType(attachedSlotType)

				if player.RESERVED_WINDOW != attachedInvenType:
					if _IsItemUsedForDragonSoul(attachedInvenType, attachedSlotPos, player.DRAGON_SOUL_INVENTORY, overSlotPos):
						self.wndItem.SetUsableItem(True)
					else:
						self.wndItem.SetUsableItem(False)

		self.ShowToolTip(player.DRAGON_SOUL_INVENTORY, overSlotPos)

	def __UseItem(self, slotIndex):
		ItemVNum = player.GetItemIndex(player.DRAGON_SOUL_INVENTORY, slotIndex)
		if 0 == player.GetItemMetinSocket(player.DRAGON_SOUL_INVENTORY, slotIndex, 0):
			self.wndPopupDialog.SetText(localeInfo.DRAGON_SOUL_EXPIRED)
			self.wndPopupDialog.Open()
			return

		self.__EquipItem(slotIndex)

	def __EquipItem(self, slotIndex):
		ItemVNum = player.GetItemIndex(player.DRAGON_SOUL_INVENTORY, slotIndex)
		item.SelectItem(ItemVNum)
		subType = item.GetItemSubType()
		equipSlotPos = player.DRAGON_SOUL_EQUIPMENT_SLOT_START + self.deckPageIndex * player.DRAGON_SOUL_EQUIPMENT_FIRST_SIZE + subType
		srcItemPos = (player.DRAGON_SOUL_INVENTORY, slotIndex)

		dstItemPos = (player.INVENTORY, equipSlotPos)

		self.__OpenQuestionDialog_DS_Equip(True, srcItemPos, dstItemPos)

	def SelectItemSlot(self, itemSlotIndex):
		if constInfo.GET_ITEM_QUESTION_DIALOG_STATUS() == 1:
			return

		itemSlotIndex = self.__InventoryLocalSlotPosToGlobalSlotPos(player.DRAGON_SOUL_INVENTORY, itemSlotIndex)

		if mouseModule.mouseController.isAttached():
			if self.wndDragonSoulRefine.IsShow():
				mouseModule.mouseController.DeattachObject()
				return

			attachedSlotType = mouseModule.mouseController.GetAttachedType()
			attachedSlotPos = mouseModule.mouseController.GetAttachedSlotNumber()
			attachedInvenType = player.SlotTypeToInvenType(attachedSlotType)

			if player.RESERVED_WINDOW != attachedInvenType:
				if _IsItemUsedForDragonSoul(attachedInvenType, attachedSlotPos, player.DRAGON_SOUL_INVENTORY, itemSlotIndex):
					self.__OpenQuestionDialog(attachedInvenType, attachedSlotPos, player.DRAGON_SOUL_INVENTORY, itemSlotIndex)

			mouseModule.mouseController.DeattachObject()
		else:
			## 상점에서 팔도록 추가
			## 20140220
			curCursorNum = app.GetCursor()

			if app.SELL == curCursorNum:
				self.__SellItem(itemSlotIndex)
			elif app.BUY == curCursorNum:
				chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.SHOP_BUY_INFO)
			elif app.IsPressed(app.DIK_LALT):
				link = player.GetItemLink(player.DRAGON_SOUL_INVENTORY, itemSlotIndex)
				ime.PasteString(link)
			else:
				selectedItemVNum = player.GetItemIndex(player.DRAGON_SOUL_INVENTORY, itemSlotIndex)
				itemCount = player.GetItemCount(player.DRAGON_SOUL_INVENTORY, itemSlotIndex)
				mouseModule.mouseController.AttachObject(self, player.SLOT_TYPE_DRAGON_SOUL_INVENTORY, itemSlotIndex, selectedItemVNum, itemCount)
				self.interface.SetUseItemMode(False)
				snd.PlaySound("sound/ui/pick.wav")

	if app.ENABLE_MAILBOX:
		def SetCantMouseEventSlot(self, index):
			slot_index = index
			if app.ENABLE_DS_GRADE_MYTH:
				slot_index = index - (self.DSKindIndex * player.DRAGON_SOUL_PAGE_COUNT * player.DRAGON_SOUL_PAGE_SIZE) - self.inventoryPageIndex * player.DRAGON_SOUL_PAGE_SIZE
			else:
				slot_index = index - (self.DSKindIndex * 5 * player.DRAGON_SOUL_PAGE_SIZE) - self.inventoryPageIndex * player.DRAGON_SOUL_PAGE_SIZE

			if slot_index < 0 or slot_index >= player.DRAGON_SOUL_PAGE_SIZE:
				return

			self.wndItem.SetCantMouseEventSlot(slot_index)

	## 상점에 팔기
	## 2014.02.20 추가
	def __SellItem(self, itemSlotPos):
		if not player.IsDSEquipmentSlot(player.DRAGON_SOUL_INVENTORY, itemSlotPos):
			self.sellingSlotNumber = itemSlotPos
			itemIndex = player.GetItemIndex(player.DRAGON_SOUL_INVENTORY, itemSlotPos)
			itemCount = player.GetItemCount(player.DRAGON_SOUL_INVENTORY, itemSlotPos)

			item.SelectItem(itemIndex)

			if item.IsAntiFlag(item.ANTIFLAG_SELL):
				popup = uiCommon.PopupDialog()
				popup.SetText(localeInfo.SHOP_CANNOT_SELL_ITEM)
				popup.SetAcceptEvent(self.__OnClosePopupDialog)
				popup.Open()
				self.popup = popup
				return

			itemPrice = item.GetISellItemPrice()

			if item.Is1GoldItem():
				itemPrice = itemCount / itemPrice / 5
			else:
				itemPrice = itemPrice * itemCount / 5

			item.GetItemName(itemIndex)
			itemName = item.GetItemName()

			self.questionDialog = uiCommon.QuestionDialog()
			self.questionDialog.SetText(localeInfo.DO_YOU_SELL_ITEM(itemName, itemCount, itemPrice))
			self.questionDialog.SetAcceptEvent(ui.__mem_func__(self.SellItem))
			self.questionDialog.SetCancelEvent(ui.__mem_func__(self.OnCloseQuestionDialog))
			self.questionDialog.Open()
			self.questionDialog.count = itemCount

	## 상점에 팔기
	def SellItem(self):
		net.SendShopSellPacketNew(self.sellingSlotNumber, self.questionDialog.count, player.DRAGON_SOUL_INVENTORY)
		snd.PlaySound("sound/ui/money.wav")
		self.OnCloseQuestionDialog()

	## 상점에 팔기
	def OnCloseQuestionDialog(self):
		if self.questionDialog:
			self.questionDialog.Close()

		self.questionDialog = None

	## 상점에 팔기
	def __OnClosePopupDialog(self):
		self.pop = None

	def SelectEmptySlot(self, selectedSlotPos):
		if constInfo.GET_ITEM_QUESTION_DIALOG_STATUS() == 1:
			return

		selectedSlotPos = self.__InventoryLocalSlotPosToGlobalSlotPos(player.DRAGON_SOUL_INVENTORY, selectedSlotPos)
		if mouseModule.mouseController.isAttached():
			attachedSlotType = mouseModule.mouseController.GetAttachedType()
			attachedSlotPos = mouseModule.mouseController.GetAttachedSlotNumber()
			attachedItemCount = mouseModule.mouseController.GetAttachedItemCount()
			attachedItemIndex = mouseModule.mouseController.GetAttachedItemIndex()

			attachedInvenType = player.SlotTypeToInvenType(attachedSlotType)
			if player.SLOT_TYPE_PRIVATE_SHOP == attachedSlotType:
				mouseModule.mouseController.RunCallBack("INVENTORY")
			elif player.SLOT_TYPE_SHOP == attachedSlotType:
				net.SendShopBuyPacket(attachedSlotPos)
			elif player.SLOT_TYPE_SAFEBOX == attachedSlotType:
				if player.ITEM_MONEY == attachedItemIndex:
					net.SendSafeboxWithdrawMoneyPacket(mouseModule.mouseController.GetAttachedItemCount())
					snd.PlaySound("sound/ui/money.wav")
				else:
					net.SendSafeboxCheckoutPacket(attachedSlotPos, player.DRAGON_SOUL_INVENTORY, selectedSlotPos)
			elif player.SLOT_TYPE_MALL == attachedSlotType:
				net.SendMallCheckoutPacket(attachedSlotPos, player.DRAGON_SOUL_INVENTORY, selectedSlotPos)
			elif player.RESERVED_WINDOW != attachedInvenType:
				if player.IsDSEquipmentSlot(attachedInvenType, attachedSlotPos):
					srcItemPos = (attachedInvenType, attachedSlotPos)
					dstItemPos = (player.DRAGON_SOUL_INVENTORY, selectedSlotPos)
					self.__OpenQuestionDialog_DS_Equip(False, srcItemPos, dstItemPos)
				else:
					itemCount = player.GetItemCount(attachedInvenType, attachedSlotPos)
					attachedCount = mouseModule.mouseController.GetAttachedItemCount()

					self.__SendMoveItemPacket(attachedInvenType, attachedSlotPos, player.DRAGON_SOUL_INVENTORY, selectedSlotPos, attachedCount)

			mouseModule.mouseController.DeattachObject()

	def UseItemSlot(self, slotIndex):
		if constInfo.GET_ITEM_QUESTION_DIALOG_STATUS():
			return

		slotIndex = self.__InventoryLocalSlotPosToGlobalSlotPos(player.DRAGON_SOUL_INVENTORY, slotIndex)
		try:
			# 용혼석 강화창이 열려있으면, 아이템 우클릭 시 자동으로 강화창으로 들어감.
			if self.wndDragonSoulRefine.IsShow():
				if uiPrivateShopBuilder.IsBuildingPrivateShop():
					chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.USE_ITEM_FAILURE_PRIVATE_SHOP)
					return

				if self.interface.IsShowDlgQuestionWindow():
					chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.DONT_USE_ITEM_WHEN_SHOW_CONFIRM)
					return

				self.wndDragonSoulRefine.AutoSetItem((player.DRAGON_SOUL_INVENTORY, slotIndex), 1)
				return
		except:
			pass

		self.__UseItem(slotIndex)

		mouseModule.mouseController.DeattachObject()
		self.OverOutItem()

	def __SendMoveItemPacket(self, srcSlotWindow, srcSlotPos, dstSlotWindow, dstSlotPos, srcItemCount):
		if uiPrivateShopBuilder.IsBuildingPrivateShop():
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.MOVE_ITEM_FAILURE_PRIVATE_SHOP)
			return

		# This function is also called by the accept callback of dlgQuestion.
		# Checking all question windows here sees that very dialog and silently
		# suppresses the move.  The original client deliberately sends directly.
		dbg.TraceError("[DS_EQUIP_SEND] %d:%d -> %d:%d count=%d" % (srcSlotWindow, srcSlotPos, dstSlotWindow, dstSlotPos, srcItemCount))
		net.SendItemMovePacket(srcSlotWindow, srcSlotPos, dstSlotWindow, dstSlotPos, srcItemCount)

	# equip 슬롯 관련 함수들.
	def OverOutEquipItem(self):
		self.OverOutItem()

	def OverInEquipItem(self, overSlotPos):
		overSlotPos = self.__GetEquipSlotPos(overSlotPos)

		if mouseModule.mouseController.isAttached():
			attachedItemType = mouseModule.mouseController.GetAttachedType()
			if player.SLOT_TYPE_INVENTORY == attachedItemType:

				attachedSlotType = mouseModule.mouseController.GetAttachedType()
				attachedSlotPos = mouseModule.mouseController.GetAttachedSlotNumber()
				attachedInvenType = player.SlotTypeToInvenType(attachedSlotType)

				if player.RESERVED_WINDOW != attachedInvenType:
					if _IsItemUsedForDragonSoul(attachedInvenType, attachedSlotPos, player.INVENTORY, overSlotPos):
						self.wndItem.SetUsableItem(True)
					else:
						self.wndItem.SetUsableItem(False)

		self.ShowToolTip(player.INVENTORY, overSlotPos)

	def UseEquipItemSlot(self, slotIndex):
		if constInfo.GET_ITEM_QUESTION_DIALOG_STATUS():
			return

		slotIndex = self.__GetEquipSlotPos(slotIndex)

		self.__UseEquipItem(slotIndex)
		mouseModule.mouseController.DeattachObject()
		self.OverOutEquipItem()

	def __UseEquipItem(self, slotIndex):
		if uiPrivateShopBuilder.IsBuildingPrivateShop():
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.USE_ITEM_FAILURE_PRIVATE_SHOP)
			return

		self.__OpenQuestionDialog_DS_Equip(False, (player.INVENTORY, slotIndex), (player.DRAGON_SOUL_INVENTORY, 0))

	def SelectEquipItemSlot(self, itemSlotIndex):
		## 마우스 버튼이 sell buy 체크 해서 return
		curCursorNum = app.GetCursor()
		if app.SELL == curCursorNum:
			return
		elif app.BUY == curCursorNum:
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.SHOP_BUY_INFO)
			return

		if constInfo.GET_ITEM_QUESTION_DIALOG_STATUS() == 1:
			return

		itemSlotIndex = self.__GetEquipSlotPos(itemSlotIndex)

		if mouseModule.mouseController.isAttached():
			if self.wndDragonSoulRefine.IsShow():
				mouseModule.mouseController.DeattachObject()
				return

			attachedSlotType = mouseModule.mouseController.GetAttachedType()
			attachedSlotPos = mouseModule.mouseController.GetAttachedSlotNumber()
			attachedInvenType = player.SlotTypeToInvenType(attachedSlotType)

			if player.RESERVED_WINDOW != attachedInvenType:
				if _IsItemUsedForDragonSoul(attachedInvenType, attachedSlotPos, player.INVENTORY, itemSlotIndex):
					self.__OpenQuestionDialog(attachedInvenType, attachedSlotPos, player.INVENTORY, itemSlotIndex)

			mouseModule.mouseController.DeattachObject()
		else:
			selectedItemVNum = player.GetItemIndex(player.INVENTORY, itemSlotIndex)
			itemCount = player.GetItemCount(player.INVENTORY, itemSlotIndex)
			mouseModule.mouseController.AttachObject(self, player.SLOT_TYPE_INVENTORY, itemSlotIndex, selectedItemVNum, itemCount)

			self.interface.SetUseItemMode(False)
			snd.PlaySound("sound/ui/pick.wav")

	def SelectEmptyEquipSlot(self, selectedSlot):
		if constInfo.GET_ITEM_QUESTION_DIALOG_STATUS() == 1:
			return

		selectedSlotPos = self.__GetEquipSlotPos(selectedSlot)

		if mouseModule.mouseController.isAttached():
			attachedSlotType = mouseModule.mouseController.GetAttachedType()
			attachedSlotPos = mouseModule.mouseController.GetAttachedSlotNumber()
			attachedItemCount = mouseModule.mouseController.GetAttachedItemCount()
			attachedItemIndex = mouseModule.mouseController.GetAttachedItemIndex()

			if player.SLOT_TYPE_DRAGON_SOUL_INVENTORY == attachedSlotType:
				if 0 == player.GetItemMetinSocket(player.DRAGON_SOUL_INVENTORY, attachedSlotPos, 0):
					self.wndPopupDialog.SetText(localeInfo.DRAGON_SOUL_EXPIRED)
					self.wndPopupDialog.Open()
					return

				item.SelectItem(attachedItemIndex)
				subType = item.GetItemSubType()
				if subType != (selectedSlot - player.DRAGON_SOUL_EQUIPMENT_SLOT_START):
					self.wndPopupDialog.SetText(localeInfo.DRAGON_SOUL_UNMATCHED_SLOT)
					self.wndPopupDialog.Open()
				else:
					srcItemPos = (player.DRAGON_SOUL_INVENTORY, attachedSlotPos)
					dstItemPos = (player.INVENTORY, selectedSlotPos)
					self.__OpenQuestionDialog_DS_Equip(True, srcItemPos, dstItemPos)

			mouseModule.mouseController.DeattachObject()
	# equip 슬롯 관련 함수들 끝.

	# 경고창 관련
	def __OpenQuestionDialog_DS_Equip(self, Equip, srcItemPos, dstItemPos):
		if self.interface.IsShowDlgQuestionWindow():
			self.interface.CloseDlgQuestionWindow()

		self.srcItemPos = srcItemPos
		self.dstItemPos = dstItemPos

		self.dlgQuestion.SetAcceptEvent(ui.__mem_func__(self.__Accept_DS_Equip))
		self.dlgQuestion.SetCancelEvent(ui.__mem_func__(self.__Cancel))

		if Equip:
			self.dlgQuestion.SetText1(localeInfo.DRAGON_SOUL_EQUIP_WARNING1)
			self.dlgQuestion.SetText2(localeInfo.DRAGON_SOUL_EQUIP_WARNING2)
		else:
			self.dlgQuestion.SetText1(localeInfo.DRAGON_SOUL_UNEQUIP_WARNING1)
			self.dlgQuestion.SetText2(localeInfo.DRAGON_SOUL_UNEQUIP_WARNING2)

		self.dlgQuestion.Open()

	def __Accept_DS_Equip(self):
		if (-1, -1) == self.dstItemPos:
			net.SendItemUsePacket(*self.srcItemPos)
		else:
			self.__SendMoveItemPacket(*(self.srcItemPos + self.dstItemPos + (0,)))
		self.dlgQuestion.Close()

	def __OpenQuestionDialog(self, srcItemInvenType, srcItemPos, dstItemInvenType, dstItemPos):
		if self.interface.IsShowDlgQuestionWindow():
			self.interface.CloseDlgQuestionWindow()

		if srcItemInvenType == dstItemInvenType:
			if srcItemPos == dstItemPos:
				return

		self.srcItemPos = (srcItemInvenType, srcItemPos)
		self.dstItemPos = (dstItemInvenType, dstItemPos)

		self.dlgQuestion.SetAcceptEvent(ui.__mem_func__(self.__Accept))
		self.dlgQuestion.SetCancelEvent(ui.__mem_func__(self.__Cancel))

		getItemVNum = player.GetItemIndex
		item.SelectItem(getItemVNum(self.srcItemPos[0], self.srcItemPos[1]))
		self.dlgQuestion.SetText1("%s" % item.GetItemName())
		self.dlgQuestion.SetText2(localeInfo.INVENTORY_REALLY_USE_ITEM)

		self.dlgQuestion.Open()

	def __Accept(self):
		self.dlgQuestion.Close()
		# 개인상점 열고 있는 동안 아이템 사용 방지
		if uiPrivateShopBuilder.IsBuildingPrivateShop():
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.USE_ITEM_FAILURE_PRIVATE_SHOP)
			return

		if self.interface.IsShowDlgQuestionWindow():
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.DONT_USE_ITEM_WHEN_SHOW_CONFIRM)
			return

		net.SendItemUseToItemPacket(self.srcItemPos[0], self.srcItemPos[1], self.dstItemPos[0], self.dstItemPos[1])
		self.srcItemPos = (0, 0)
		self.dstItemPos = (0, 0)

	def __Cancel(self):
		self.srcItemPos = (0, 0)
		self.dstItemPos = (0, 0)
		self.dlgQuestion.Close()

	# 경고창 관련 끝

	def SetDSKindIndex(self, kindIndex):
		if self.DSKindIndex != kindIndex:
			self.__HighlightSlot_ClearCurrentPage()

		self.DSKindIndex = kindIndex

		for (tabKey, tabButton) in self.tabButtonDict.items():
			if kindIndex!=tabKey:
				tabButton.SetUp()

		for tabValue in self.tabDict.itervalues():
			tabValue.Hide()

		self.tabDict[kindIndex].Show()
		self.tabText.SetText(self.KIND_TAP_TITLES[kindIndex])

		self.RefreshBagSlotWindow()

	def SetDeckPage(self, page):
		if page == self.deckPageIndex:
			return

		if self.isActivated:
			self.DeactivateDragonSoul()
			net.SendChatPacket("/dragon_soul deactivate")

		self.deckPageIndex = page
		self.deckTab[page].Down()
		self.deckTab[(page + 1) % 2].SetUp()

		self.RefreshEquipSlotWindow()

	if app.ENABLE_DS_SET:
		def SetDSSetGrade(self, grade):
			self.setGrade = grade

		def GetDSSetGrade(self):
			if not self.isActivated:
				return 0

			return self.setGrade

	# 용혼석 활성화 관련
	def ActivateDragonSoulByExtern(self, deck):
		#self.ActivateEquipSlotWindow(deck)
		self.isActivated = True
		self.activateButton.Down()
		self.deckPageIndex = deck
		self.deckTab[deck].Down()
		self.deckTab[(deck + 1) % 2].SetUp()
		self.RefreshEquipSlotWindow()

	def DragonSoulKeySelect(self, deck):
		self.SetDeckPage(deck)
		self.ActivateButtonClick()

	def DeactivateDragonSoul(self):
		#self.DeactivateEquipSlotWindow()
		self.isActivated = False
		self.activateButton.SetUp()

	def ActivateButtonClick(self):
		self.isActivated = self.isActivated ^ True
		if self.isActivated:
			if self.__CanActivateDeck():
				net.SendChatPacket("/dragon_soul activate " + str(self.deckPageIndex))
			else:
				self.isActivated = False
				self.activateButton.SetUp()
		else:
			net.SendChatPacket("/dragon_soul deactivate")

	def __CanActivateDeck(self):
		canActiveNum = 0
		for i in xrange(player.DRAGON_SOUL_EQUIPMENT_FIRST_SIZE):
			slotNumber = self.__GetEquipSlotPos(player.DRAGON_SOUL_EQUIPMENT_SLOT_START + i)
			itemVnum = player.GetItemIndex(player.INVENTORY, slotNumber)

			if itemVnum != 0:
				item.SelectItem(itemVnum)
				isNoLimit = True
				for i in xrange(item.LIMIT_MAX_NUM):
					(limitType, limitValue) = item.GetLimit(i)

					# LIMIT_TIMER_BASED_ON_WEAR는 소켓0에 남은 시간을 박는다.
					# LIMIT_REAL_TIME은 시간 다 되면 아이템이 사라지므로 할 필요가 없다.
					# LIMIT_REAL_TIME_START_FIRST_USE는 서버에 제대로 정의되지 않아 일단 냅둔다.
					if item.LIMIT_TIMER_BASED_ON_WEAR == limitType:
						isNoLimit = False

						remain_time = player.GetItemMetinSocket(player.INVENTORY, slotNumber, 0)

						if 0 != remain_time:
							canActiveNum += 1
							break
				# 타이머가 없다면 Activate할 수 있는 용혼석.
				if isNoLimit:
					canActiveNum += 1

		return canActiveNum > 0

	# 활성화 관련 끝

	# 슬롯 highlight 관련
	def __HighlightSlot_ClearCurrentPage(self):
		for i in xrange(self.wndItem.GetSlotCount()):
			slotNumber = self.__InventoryLocalSlotPosToGlobalSlotPos(player.DRAGON_SOUL_INVENTORY, i)
			if slotNumber in self.listHighlightedSlot:
				self.wndItem.DeactivateSlot(i)
				self.listHighlightedSlot.remove(slotNumber)

	def __HighlightSlot_RefreshCurrentPage(self):
		for i in xrange(self.wndItem.GetSlotCount()):
			slotNumber = self.__InventoryLocalSlotPosToGlobalSlotPos(player.DRAGON_SOUL_INVENTORY, i)
			if slotNumber in self.listHighlightedSlot:
				self.wndItem.ActivateSlot(i)

	def HighlightSlot(self, slot):
		if not slot in self.listHighlightedSlot:
			self.listHighlightedSlot.append(slot)
	# 슬롯 highlight 관련 끝

	def SetDragonSoulRefineWindow(self, DragonSoulRefine):
		from _weakref import proxy
		self.wndDragonSoulRefine = proxy(DragonSoulRefine)

	# wj.2014.12.2. 인벤토리와 DS인벤 간의 상태를 확인 및 설정하기 위한 함수.
	def BindInterfaceClass(self, interface):
		from _weakref import proxy
		self.interface = proxy(interface)

	def IsDlgQuestionShow(self):
		if self.dlgQuestion.IsShow():
			return True
		else:
			return False

	def CancelDlgQuestion(self):
		self.__Cancel()

	def SetUseItemMode(self, bUse):
		self.wndItem.SetUseMode(bUse)

## 강화할 수 없는 경우 날리는 예외
#class DragonSoulRefineException(Exception):
	#pass

class DragonSoulRefineWindow(ui.ScriptWindow):
	REFINE_TYPE_GRADE, REFINE_TYPE_STEP, REFINE_TYPE_STRENGTH = xrange(3)
	DS_SUB_HEADER_DIC = {
		REFINE_TYPE_GRADE : player.DS_SUB_HEADER_DO_UPGRADE,
		REFINE_TYPE_STEP : player.DS_SUB_HEADER_DO_IMPROVEMENT,
		REFINE_TYPE_STRENGTH : player.DS_SUB_HEADER_DO_REFINE 
	}
	if app.ENABLE_DS_CHANGE_ATTR:
		REFINE_TYPE_CHANGE_ATTR = 3
		DS_SUB_HEADER_DIC.update({ REFINE_TYPE_CHANGE_ATTR : player.DS_SUB_HEADER_DO_CHANGE_ATTR })

	REFINE_STONE_SLOT, DRAGON_SOUL_SLOT = xrange(2)

	INVALID_DRAGON_SOUL_INFO = -1

	def __init__(self):
		ui.ScriptWindow.__init__(self)
		self.tooltipItem = None
		self.sellingSlotNumber = -1
		self.isLoaded = 0
		self.refineChoiceButtonDict = None
		if app.ENABLE_DS_CHANGE_ATTR:
			self.refineChoiceButtonTitleDict = None
		self.doRefineButton = None
		self.wndMoney = None
		# MT2009_PLUS_REFINE_ALL_V1
		self.refineAllButton = None
		self.refineAllQuestion = None
		self.refineAllPending = None
		self.refineAllQueue = None
		self.refineAllType = 0
		self.refineAllWaiting = False
		self.refineAllSentTime = 0.0
		self.refineAllNextTime = 0.0
		self.refineAllInFlight = []
		self.refineAllDone = 0
		self.refineAllSucceeded = 0
		self.SetWindowName("DragonSoulRefineWindow")
		self.__LoadWindow()

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def Show(self):
		self.__LoadWindow()
		ui.ScriptWindow.Show(self)

	def __LoadWindow(self):
		if self.isLoaded == 1:
			return
		self.isLoaded = 1
		try:
			pyScrLoader = ui.PythonScriptLoader()
			pyScrLoader.LoadScriptFile(self, uiScriptLocale.LOCALE_COMMON_UISCRIPT_PATH + "dragonsoulrefinewindow.py")
		except:
			import exception
			exception.Abort("dragonsoulrefinewindow.LoadWindow.LoadObject")
		try:
			if localeInfo.IsARABIC():
				self.board = self.GetChild("DragonSoulRefineWindowBaseImage")
				self.board.SetScale(-1.0, 1.0)
				self.board.SetRenderingRect(-1.0, 0.0, 1.0, 0.0)

			wndRefineSlot = self.GetChild("RefineSlot")
			wndResultSlot = self.GetChild("ResultSlot")
			self.GetChild("TitleBar").SetCloseEvent(ui.__mem_func__(self.Close))
			self.refineChoiceButtonDict = {
				self.REFINE_TYPE_GRADE : self.GetChild("GradeButton"),
				self.REFINE_TYPE_STEP : self.GetChild("StepButton"),
				self.REFINE_TYPE_STRENGTH : self.GetChild("StrengthButton"),
			}
			self.doRefineButton = self.GetChild("DoRefineButton")
			self.wndMoney = self.GetChild("Money_Slot")

			if app.ENABLE_DS_CHANGE_ATTR:
				self.refineChoiceButtonTitleDict = {
					self.REFINE_TYPE_GRADE : self.GetChild("GradeSlotTitle"),
					self.REFINE_TYPE_STEP : self.GetChild("StepSlotTitle"),
					self.REFINE_TYPE_STRENGTH : self.GetChild("RefineSlotTitle")
				}
		except:
			import exception
			exception.Abort("DragonSoulRefineWindow.LoadWindow.BindObject")

		## Item Slots
		wndRefineSlot.SetOverInItemEvent(ui.__mem_func__(self.__OverInRefineItem))
		wndRefineSlot.SetOverOutItemEvent(ui.__mem_func__(self.__OverOutItem))
		wndRefineSlot.SetSelectEmptySlotEvent(ui.__mem_func__(self.__SelectRefineEmptySlot))
		wndRefineSlot.SetSelectItemSlotEvent(ui.__mem_func__(self.__SelectRefineItemSlot))
		wndRefineSlot.SetUseSlotEvent(ui.__mem_func__(self.__SelectRefineItemSlot))
		wndRefineSlot.SetUnselectItemSlotEvent(ui.__mem_func__(self.__SelectRefineItemSlot))

		wndResultSlot.SetOverInItemEvent(ui.__mem_func__(self.__OverInResultItem))
		wndResultSlot.SetOverOutItemEvent(ui.__mem_func__(self.__OverOutItem))
		self.wndRefineSlot = wndRefineSlot
		self.wndResultSlot = wndResultSlot

		## Button
		self.refineChoiceButtonDict[self.REFINE_TYPE_GRADE].SetToggleDownEvent(ui.__mem_func__(self.__ToggleDownGradeButton))
		self.refineChoiceButtonDict[self.REFINE_TYPE_STEP].SetToggleDownEvent(ui.__mem_func__(self.__ToggleDownStepButton))
		self.refineChoiceButtonDict[self.REFINE_TYPE_STRENGTH].SetToggleDownEvent(ui.__mem_func__(self.__ToggleDownStrengthButton))
		self.refineChoiceButtonDict[self.REFINE_TYPE_GRADE].SetToggleUpEvent(ui.__mem_func__(self.__ToggleUpButton), self.REFINE_TYPE_GRADE)
		self.refineChoiceButtonDict[self.REFINE_TYPE_STEP].SetToggleUpEvent(ui.__mem_func__(self.__ToggleUpButton), self.REFINE_TYPE_STEP)
		self.refineChoiceButtonDict[self.REFINE_TYPE_STRENGTH].SetToggleUpEvent(ui.__mem_func__(self.__ToggleUpButton), self.REFINE_TYPE_STRENGTH)
		self.doRefineButton.SetEvent(ui.__mem_func__(self.__PressDoRefineButton))
		self.__CreateRefineAllButton()	# MT2009_PLUS_REFINE_ALL_V1

		## Dialog
		self.wndPopupDialog = OneMessagePopupDialog()	# MT2009_PLUS_ALCHEMY_MSG_V1

		self.currentRefineType = self.REFINE_TYPE_GRADE
		self.refineItemInfo = {}
		self.resultItemInfo = {}
		self.currentRecipe = {}

		self.wndMoney.SetText(localeInfo.NumberToMoneyString(0))
		self.refineChoiceButtonDict[self.REFINE_TYPE_GRADE].Down()

		self.__Initialize()

	def Destroy(self):
		self.ClearDictionary()
		self.tooltipItem = None
		self.wndItem = 0
		self.wndEquip = 0
		self.activateButton = 0
		self.questionDialog = None
		self.mallButton = None
		self.inventoryTab = []
		self.deckTab = []
		self.equipmentTab = []
		self.tabDict = None
		self.tabButtonDict = None

	def Close(self):
		if None != self.tooltipItem:
			self.tooltipItem.HideToolTip()

		self.__StopRefineAll()	# MT2009_PLUS_REFINE_ALL_V1
		if self.refineAllQuestion:
			self.refineAllQuestion.Close()
		self.__FlushRefineItemSlot()
		player.SendDragonSoulRefine(player.DRAGON_SOUL_REFINE_CLOSE)
		self.Hide()

	def Show(self):
		if app.ENABLE_DS_CHANGE_ATTR:
			ui.ScriptWindow.Show(self)
			return

		self.currentRefineType = self.REFINE_TYPE_GRADE
		self.wndMoney.SetText(localeInfo.NumberToMoneyString(0))
		self.refineChoiceButtonDict[self.REFINE_TYPE_GRADE].Down()
		self.refineChoiceButtonDict[self.REFINE_TYPE_STEP].SetUp()
		self.refineChoiceButtonDict[self.REFINE_TYPE_STRENGTH].SetUp()

		self.Refresh()

		ui.ScriptWindow.Show(self)

	def SetItemToolTip(self, tooltipItem):
		self.tooltipItem = tooltipItem

	# 버튼 눌려 있는 상태를 제외한 모든 강화창 관련 변수들을 초기화.
	def __Initialize(self):
		self.currentRecipe = {}
		self.refineItemInfo = {}
		self.refineItemVnums = {}	# MT2009_PLUS_ALCHEMY_MSG_V1
		self.resultItemInfo = {}

		if self.REFINE_TYPE_STRENGTH == self.currentRefineType:
			self.refineSlotLockStartIndex = 2
		else:
			self.refineSlotLockStartIndex = 1

		for i in xrange(self.refineSlotLockStartIndex):
			self.wndRefineSlot.HideSlotBaseImage(i)

		self.wndMoney.SetText(localeInfo.NumberToMoneyString(0))

	def __FlushRefineItemSlot(self):
		## Item slot settings
		# 원래 인벤의 아이템 카운트 회복
		if app.ENABLE_DS_CHANGE_ATTR:
			if DragonSoulRefineWindow.REFINE_TYPE_CHANGE_ATTR == self.currentRefineType:
				if self.refineItemInfo:
					invenType, invenPos, itemCount = self.refineItemInfo[self.REFINE_STONE_SLOT]
					remainCount = player.GetItemCount(invenType, invenPos)
					player.SetItemCount(invenType, invenPos, remainCount + itemCount)

				self.__Initialize()
				return

		for invenType, invenPos, itemCount in self.refineItemInfo.values():
			remainCount = player.GetItemCount(invenType, invenPos)
			player.SetItemCount(invenType, invenPos, remainCount + itemCount)

		self.__Initialize()

	def __ToggleUpButton(self, idx):
		#if self.REFINE_TYPE_GRADE == self.currentRefineType:
		self.refineChoiceButtonDict[idx].Down()

	def __ToggleDownGradeButton(self):
		if self.REFINE_TYPE_GRADE == self.currentRefineType:
			return

		self.__StopRefineAll()	# MT2009_PLUS_REFINE_ALL_V1
		self.refineChoiceButtonDict[self.currentRefineType].SetUp()
		self.currentRefineType = self.REFINE_TYPE_GRADE
		self.__FlushRefineItemSlot()
		self.Refresh()

	def __ToggleDownStepButton(self):
		if self.REFINE_TYPE_STEP == self.currentRefineType:
			return

		self.__StopRefineAll()	# MT2009_PLUS_REFINE_ALL_V1
		self.refineChoiceButtonDict[self.currentRefineType].SetUp()
		self.currentRefineType = self.REFINE_TYPE_STEP
		self.__FlushRefineItemSlot()
		self.Refresh()

	def __ToggleDownStrengthButton(self):
		if self.REFINE_TYPE_STRENGTH == self.currentRefineType:
			return

		self.__StopRefineAll()	# MT2009_PLUS_REFINE_ALL_V1
		self.refineChoiceButtonDict[self.currentRefineType].SetUp()
		self.currentRefineType = self.REFINE_TYPE_STRENGTH
		self.__FlushRefineItemSlot()
		self.Refresh()

	def __PopUp(self, message):
		self.wndPopupDialog.SetText(message)
		self.wndPopupDialog.Open()

	def __SetItem(self, inven, dstSlotIndex, itemCount):
		if self.__IsRefineAllRunning():	# MT2009_PLUS_REFINE_ALL_V1
			return False
		invenType, invenPos = inven

		if app.ENABLE_DS_CHANGE_ATTR:
			if DragonSoulRefineWindow.REFINE_TYPE_CHANGE_ATTR == self.currentRefineType:
				if dstSlotIndex >= self.DRAGON_SOUL_SLOT:
					return False

		if dstSlotIndex >= self.refineSlotLockStartIndex:
			return False

		itemVnum = player.GetItemIndex(invenType, invenPos)
		maxCount = player.GetItemCount(invenType, invenPos)

		if itemCount > maxCount:
			raise Exception, ("Invalid attachedItemCount(%d). (base pos (%d, %d), base itemCount(%d))" % (itemCount, invenType, invenPos, maxCount))
			#return False

		# strength 강화일 경우, 0번엔 강화석, 1번엔 용혼석을 놓도록 강제함.
		if DragonSoulRefineWindow.REFINE_TYPE_STRENGTH == self.currentRefineType:
			if self.__IsDragonSoul(itemVnum):
				dstSlotIndex = 1
			else:
				dstSlotIndex = 0

		# 빈 슬롯이어야함.
		if dstSlotIndex in self.refineItemInfo:
			return False

		# 강화창에 올릴 수 있는 아이템인지 검사.
		if False == self.__CheckCanRefine(itemVnum):
			return False

		# 끌어다 놓은 아이템 카운트만큼 원래 자리의 아이템 카운트 감소
		player.SetItemCount(invenType, invenPos, maxCount - itemCount)
		self.refineItemInfo[dstSlotIndex] = (invenType, invenPos, itemCount)
		# MT2009_PLUS_ALCHEMY_MSG_V1: what lay there, asked again on the button.
		self.refineItemVnums[dstSlotIndex] = itemVnum
		self.Refresh()

		return True

	# 강화 가능한 아이템인지 체크
	# 용혼석 강화는 강화 레시피를 정해놓고 시작하는 것이 아니라,
	# 처음에 강화창에 올린 용혼석에 의해 강화 레시피가 결정된다.
	# 그래서 __CanRefineGrade, __CanRefineStep, __CanRefineStrength 함수에서
	# 강화 레시피가 없다면(처음 올리는 아이템이라면), 강화 레시피를 설정해주는 역할도 한다.
	def __CheckCanRefine(self, vnum):
		if self.REFINE_TYPE_GRADE == self.currentRefineType:
			return self.__CanRefineGrade(vnum)

		elif self.REFINE_TYPE_STEP == self.currentRefineType:
			return self.__CanRefineStep(vnum)

		elif self.REFINE_TYPE_STRENGTH == self.currentRefineType:
			return self.__CanRefineStrength(vnum)

		else:
			if app.ENABLE_DS_CHANGE_ATTR:
				if self.REFINE_TYPE_CHANGE_ATTR == self.currentRefineType:
					return self.__CanRefineChangeAttr(vnum)

			return False

	def __CanRefineGrade (self, vnum):
		ds_info = self.__GetDragonSoulTypeInfo(vnum)

		if DragonSoulRefineWindow.INVALID_DRAGON_SOUL_INFO == ds_info:
			self.__PopUp(localeInfo.DRAGON_SOUL_IS_NOT_DRAGON_SOUL)
			return False

		if self.currentRecipe:
			ds_type, grade, step, strength = ds_info
			cur_refine_ds_type, cur_refine_grade, cur_refine_step, cur_refine_strength = self.currentRecipe["ds_info"]
			if not (cur_refine_ds_type == ds_type and cur_refine_grade == grade):
				self.__PopUp(localeInfo.DRAGON_SOUL_INVALID_DRAGON_SOUL)
				return False
		# 강화 창에 처음 아이템을 올리는 경우, 강화 재료에 관한 정보가 없다.
		# 용혼석 강화가, 레시피를 가지고 시작하는 것이 아니라, 강화창에 처음 올리는 아이템이 무엇이냐에 따라,
		# 무엇을 강화하고, 재료가 무엇인지(이하 레시피)가 정해진다.
		# 레시피가 없다면, 처음 올린 아이템이라 생각하고, vnum을 바탕으로 레시피를 셋팅.
		else:
			self.currentRecipe = self.__GetRefineGradeRecipe(vnum)

			if self.currentRecipe:
				self.refineSlotLockStartIndex = self.currentRecipe["need_count"]
				self.wndMoney.SetText(localeInfo.NumberToMoneyString(self.currentRecipe["fee"]))
				return True
			else:
			# 강화 정보 셋팅에 실패하면 올릴 수 없는 아이템으로 판단.
				self.__PopUp(localeInfo.DRAGON_SOUL_CANNOT_REFINE)
				return False

	def __CanRefineStep (self, vnum):
		ds_info = self.__GetDragonSoulTypeInfo(vnum)

		if DragonSoulRefineWindow.INVALID_DRAGON_SOUL_INFO == ds_info:
			self.__PopUp(localeInfo.DRAGON_SOUL_IS_NOT_DRAGON_SOUL)
			return False

		if self.currentRecipe:
			ds_type, grade, step, strength = ds_info
			cur_refine_ds_type, cur_refine_grade, cur_refine_step, cur_refine_strength = self.currentRecipe["ds_info"]
			if not (cur_refine_ds_type == ds_type and cur_refine_grade == grade and cur_refine_step == step):
				self.__PopUp(localeInfo.DRAGON_SOUL_INVALID_DRAGON_SOUL)
				return False
		# 강화 창에 처음 아이템을 올리는 경우, 재료에 관한 정보가 없다.
		# 용혼석 강화가, 레시피를 가지고 시작하는 것이 아니라, 강화창에 처음 올리는 아이템이 무엇이냐에 따라,
		# 무엇을 강화하고, 재료가 무엇인지(이하 레시피)가 정해진다.
		# 레시피가 없다면, 처음 올린 아이템이라 생각하고, vnum을 바탕으로 레시피를 셋팅.
		else:
			self.currentRecipe = self.__GetRefineStepRecipe(vnum)

			if self.currentRecipe:
				self.refineSlotLockStartIndex = self.currentRecipe["need_count"]
				self.wndMoney.SetText(localeInfo.NumberToMoneyString(self.currentRecipe["fee"]))
				return True
			else:
			# 강화 정보 셋팅에 실패하면 올릴 수 없는 아이템으로 판단.
				self.__PopUp(localeInfo.DRAGON_SOUL_CANNOT_REFINE)
				return False

	def __CanRefineStrength (self, vnum):
		# 용혼석인 경우, 더 이상 strength 강화를 할 수 없는지 체크해야함.
		if self.__IsDragonSoul(vnum):
			ds_type, grade, step, strength = self.__GetDragonSoulTypeInfo(vnum)

			import dragon_soul_refine_settings
			if strength >= dragon_soul_refine_settings.dragon_soul_refine_info[ds_type]["strength_max_table"][grade][step]:
				self.__PopUp(localeInfo.DRAGON_SOUL_CANNOT_REFINE_MORE)
				return False
			else:
				return True

		# strength 강화의 경우, refine_recipe가 용혼석의 종류가 아닌, 강화석의 종류에 따라 달라진다.
		# 따라서 용혼석이 아니라면, 
		# 이미 레시피가 있는 경우는, 강화석이 강화창에 있다는 것이므로, return False
		# 레시피가 없는 경우는, 강화석인지 확인하고, 레시피를 셋팅한다.
		else:
			if self.currentRecipe:
				self.__PopUp(localeInfo.DRAGON_SOUL_IS_NOT_DRAGON_SOUL)
				return False
			else:
				refineRecipe = self.__GetRefineStrengthInfo(vnum)
				if refineRecipe:
					self.currentRecipe = refineRecipe
					self.wndMoney.SetText(localeInfo.NumberToMoneyString(self.currentRecipe["fee"]))
					return True
				else:
				# 레시피를 셋팅할 수 없는 경우
					self.__PopUp(localeInfo.DRAGON_SOUL_NOT_DRAGON_SOUL_REFINE_STONE)
					return False

	def __GetRefineGradeRecipe (self, vnum):
		ds_type, grade, step, strength = self.__GetDragonSoulTypeInfo(vnum)
		try:
			import dragon_soul_refine_settings

			return {
				"ds_info" : (ds_type, grade, step, strength),
				"need_count" : dragon_soul_refine_settings.dragon_soul_refine_info[ds_type]["grade_need_count"][grade],
				"fee" : dragon_soul_refine_settings.dragon_soul_refine_info[ds_type]["grade_fee"][grade]
			}
		except:
			return None

	def __GetRefineStepRecipe (self, vnum):
		ds_type, grade, step, strength = self.__GetDragonSoulTypeInfo(vnum)
		try:
			import dragon_soul_refine_settings

			return {
				"ds_info" : (ds_type, grade, step, strength),
				"need_count" : dragon_soul_refine_settings.dragon_soul_refine_info[ds_type]["step_need_count"][step],
				"fee" : dragon_soul_refine_settings.dragon_soul_refine_info[ds_type]["step_fee"][step]
			}
		except:
			return None

	# strength 강화의 경우, refineInfo는 강화석에 따라 달라진다.
	def __GetRefineStrengthInfo (self, itemVnum):
		try:
			# 이놈의 위치를 어찌하지....
			# 강화석이 아니면 안됨.
			item.SelectItem(itemVnum)
			if not (item.ITEM_TYPE_MATERIAL == item.GetItemType() \
					and (item.MATERIAL_DS_REFINE_NORMAL <= item.GetItemSubType() and item.GetItemSubType() <= item.MATERIAL_DS_REFINE_HOLLY)):
				return None

			import dragon_soul_refine_settings
			return { "fee" : dragon_soul_refine_settings.strength_fee[item.GetItemSubType()] }
		except:
			return None

	def __IsDragonSoul(self, vnum):
		item.SelectItem(vnum)
		return item.GetItemType() == item.ITEM_TYPE_DS

	# 용혼석 Vnum에 대한 comment
	# ITEM VNUM을 10만 자리부터, FEDCBA라고 한다면
	# FE : 용혼석 종류.	D : 등급
	# C : 단계			B : 강화
	# A : 여벌의 번호들...
	def __GetDragonSoulTypeInfo(self, vnum):
		if not self.__IsDragonSoul(vnum):
			return DragonSoulRefineWindow.INVALID_DRAGON_SOUL_INFO 
		ds_type = vnum / 10000
		grade = vnum % 10000 /1000
		step = vnum % 1000 / 100
		strength = vnum % 100 / 10

		return (ds_type, grade, step, strength)

	def __MakeDragonSoulVnum(self, ds_type, grade, step, strength):
		return ds_type * 10000 + grade * 1000 + step * 100 + strength * 10

	## 빈 슬롯 선택 Event
	def __SelectRefineEmptySlot(self, selectedSlotPos):
		try:
			if constInfo.GET_ITEM_QUESTION_DIALOG_STATUS() == 1:
				return

			if selectedSlotPos >= self.refineSlotLockStartIndex:
				return

			if mouseModule.mouseController.isAttached():
				attachedSlotType = mouseModule.mouseController.GetAttachedType()
				attachedSlotPos = mouseModule.mouseController.GetAttachedSlotNumber()
				attachedItemCount = mouseModule.mouseController.GetAttachedItemCount()
				attachedItemIndex = mouseModule.mouseController.GetAttachedItemIndex()
				mouseModule.mouseController.DeattachObject()

				if uiPrivateShopBuilder.IsBuildingPrivateShop():
					chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.MOVE_ITEM_FAILURE_PRIVATE_SHOP)
					return

				attachedInvenType = player.SlotTypeToInvenType(attachedSlotType)

				if player.INVENTORY == attachedInvenType and player.IsEquipmentSlot(attachedInvenType, attachedSlotPos):
					return

				if player.INVENTORY != attachedInvenType and player.DRAGON_SOUL_INVENTORY != attachedInvenType:
					return

				if True == self.__SetItem((attachedInvenType, attachedSlotPos), selectedSlotPos, attachedItemCount):
					self.Refresh()

		except Exception, e:
			import dbg
			dbg.TraceError("Exception : __SelectRefineEmptySlot, %s" % e)

	# 클릭으로 슬롯에서 삭제.
	def __SelectRefineItemSlot(self, selectedSlotPos):
		if constInfo.GET_ITEM_QUESTION_DIALOG_STATUS() == 1:
			return

		try:
			if not selectedSlotPos in self.refineItemInfo:
				# 새로운 아이템을 강화창에 올리는 작업.
				if mouseModule.mouseController.isAttached():
					attachedSlotType = mouseModule.mouseController.GetAttachedType()
					attachedSlotPos = mouseModule.mouseController.GetAttachedSlotNumber()
					attachedItemCount = mouseModule.mouseController.GetAttachedItemCount()
					attachedItemIndex = mouseModule.mouseController.GetAttachedItemIndex()
					mouseModule.mouseController.DeattachObject()

					if uiPrivateShopBuilder.IsBuildingPrivateShop():
						chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.MOVE_ITEM_FAILURE_PRIVATE_SHOP)
						return

					attachedInvenType = player.SlotTypeToInvenType(attachedSlotType)

					if player.INVENTORY == attachedInvenType and player.IsEquipmentSlot(attachedInvenType, attachedSlotPos):
						return

					if player.INVENTORY != attachedInvenType and player.DRAGON_SOUL_INVENTORY != attachedInvenType:
						return

					self.AutoSetItem((attachedInvenType, attachedSlotPos), 1)
				return
			elif mouseModule.mouseController.isAttached():
				return

			if app.ENABLE_DS_CHANGE_ATTR:
				if DragonSoulRefineWindow.REFINE_TYPE_CHANGE_ATTR == self.currentRefineType:
					if selectedSlotPos == self.DRAGON_SOUL_SLOT:
						return

			attachedInvenType, attachedSlotPos, attachedItemCount = self.refineItemInfo[selectedSlotPos]
			selectedItemVnum = player.GetItemIndex(attachedInvenType, attachedSlotPos)

			# 강화창에서 삭제 및 원래 인벤의 아이템 카운트 회복
			invenType, invenPos, itemCount = self.refineItemInfo[selectedSlotPos]
			remainCount = player.GetItemCount(invenType, invenPos)
			player.SetItemCount(invenType, invenPos, remainCount + itemCount)
			del self.refineItemInfo[selectedSlotPos]

			if app.ENABLE_DS_CHANGE_ATTR:
				if DragonSoulRefineWindow.REFINE_TYPE_CHANGE_ATTR == self.currentRefineType:
					if selectedSlotPos == self.REFINE_STONE_SLOT:
						self.__Initialize()
						self.Refresh()
						return

			# 강화창이 비었다면, 초기화
			if not self.refineItemInfo:
				self.__Initialize()
			else:
				item.SelectItem(selectedItemVnum)
				# 없앤 아이템이 강화석이었다면 강화 레피시 초기화
				if (item.ITEM_TYPE_MATERIAL == item.GetItemType() \
					and (item.MATERIAL_DS_REFINE_NORMAL <= item.GetItemSubType() and item.GetItemSubType() <= item.MATERIAL_DS_REFINE_HOLLY)):
					self.currentRecipe = {}
					self.wndMoney.SetText(localeInfo.NumberToMoneyString(0))
				# 용혼석이었다면, 
				# strength강화가 아닌 경우, 강화창에 다른 용혼석이 남아있으므로, 레시피를 초기화하면 안됨.
				# strength강화의 경우, 강화 레시피는 강화석에 종속된 것이므로 다른 처리할 필요가 없음.
				else:
					pass

		except Exception, e:
			import dbg
			dbg.TraceError("Exception : __SelectRefineItemSlot, %s" % e)

		self.Refresh()

	def __OverInRefineItem(self, slotIndex):
		if self.refineItemInfo.has_key(slotIndex):
			inven_type, inven_pos, item_count = self.refineItemInfo[slotIndex]
			self.tooltipItem.SetInventoryItem(inven_pos, inven_type)

	def __OverInResultItem(self, slotIndex):
		if self.resultItemInfo.has_key(slotIndex):
			inven_type, inven_pos, item_count = self.resultItemInfo[slotIndex]
			self.tooltipItem.SetInventoryItem(inven_pos, inven_type)

	def __OverOutItem(self):
		if self.tooltipItem:
			self.tooltipItem.HideToolTip()

	# MT2009_PLUS_ALCHEMY_MSG_V1: the checks DSManager::DoRefineGrade, Step and
	# Strength make (DragonSoul.cpp, with dragon_soul_table.txt as the client's
	# dragon_soul_refine_settings mirrors it), asked before the request goes
	# out, so a refine the server would refuse anyway is refused here at once
	# with its reason: every slot filled, a stone counted once, the stones
	# still what was put in, one kind and class (and step, for the step), a
	# class or step the table still refines, the count the recipe wants, the
	# clarity under its cap, and the fee in the purse. The locale's key, or
	# None when the server would take it.
	def __ValidateRefine(self):
		for i in xrange(self.refineSlotLockStartIndex):
			if not i in self.refineItemInfo:
				return "DRAGON_SOUL_NOT_ENOUGH_MATERIAL"
		try:
			import dragon_soul_refine_settings as settings
		except Exception:
			return None
		seen = set()
		stones = []
		beans = []
		for slot, (invenType, invenPos, itemCount) in self.refineItemInfo.items():
			if (invenType, invenPos) in seen:
				return "DRAGON_SOUL_NOT_ENOUGH_MATERIAL"
			seen.add((invenType, invenPos))
			vnum = player.GetItemIndex(invenType, invenPos)
			if not vnum or (slot in self.refineItemVnums and self.refineItemVnums[slot] != vnum):
				return "DRAGON_SOUL_INVALID_DRAGON_SOUL"
			if invenType == player.INVENTORY and player.IsEquipmentSlot(invenType, invenPos):
				return "DRAGON_SOUL_INVALID_DRAGON_SOUL"
			if self.__IsDragonSoul(vnum):
				stones.append(self.__GetDragonSoulTypeInfo(vnum))
			else:
				item.SelectItem(vnum)
				if item.GetItemType() != item.ITEM_TYPE_MATERIAL or not item.GetItemSubType() in settings.strength_fee:
					return "DRAGON_SOUL_INVALID_DRAGON_SOUL"
				beans.append(item.GetItemSubType())
		try:
			if self.REFINE_TYPE_STRENGTH == self.currentRefineType:
				if len(stones) != 1 or len(beans) != 1:
					return "DRAGON_SOUL_NOT_ENOUGH_MATERIAL"
				ds_type, grade, step, strength = stones[0]
				if strength >= settings.dragon_soul_refine_info[ds_type]["strength_max_table"][grade][step]:
					return "DRAGON_SOUL_CANNOT_REFINE_MORE"
				fee = settings.strength_fee[beans[0]]
			elif self.currentRefineType in (self.REFINE_TYPE_GRADE, self.REFINE_TYPE_STEP):
				if beans or not stones:
					return "DRAGON_SOUL_INVALID_DRAGON_SOUL"
				ds_type, grade, step, strength = stones[0]
				recipe = settings.dragon_soul_refine_info[ds_type]
				if self.REFINE_TYPE_GRADE == self.currentRefineType:
					counts, fees, tier = recipe["grade_need_count"], recipe["grade_fee"], grade
				else:
					counts, fees, tier = recipe["step_need_count"], recipe["step_fee"], step
				if tier >= len(counts) or tier >= len(fees):
					return "DRAGON_SOUL_CANNOT_REFINE_MORE"
				for other in stones:
					if other[0] != ds_type or other[1] != grade or \
							(self.REFINE_TYPE_STEP == self.currentRefineType and other[2] != step):
						return "DRAGON_SOUL_INVALID_DRAGON_SOUL"
				if len(stones) != counts[tier]:
					return "DRAGON_SOUL_NOT_ENOUGH_MATERIAL"
				fee = fees[tier]
			else:
				return None
		except (KeyError, IndexError, TypeError, ValueError):
			return "DRAGON_SOUL_INVALID_DRAGON_SOUL"
		if player.GetElk() < fee:
			return "DRAGON_SOUL_NOT_ENOUGH_MONEY"
		return None

	def __ValidationText(self, key):
		if key == "DRAGON_SOUL_NOT_ENOUGH_MONEY":
			return getattr(localeInfo, key, None) or getattr(localeInfo, "SHOP_NOT_ENOUGH_MONEY", "") or \
				"Masz za ma\xb3o Yang na to uszlachetnianie."
		return getattr(localeInfo, key, None) or localeInfo.DRAGON_SOUL_CANNOT_REFINE

	def __PressDoRefineButton(self):
		if self.__IsRefineAllRunning():	# MT2009_PLUS_REFINE_ALL_V1
			return
		# MT2009_PLUS_ALCHEMY_MSG_V1: refused here what the server would refuse.
		changeAttr = app.ENABLE_DS_CHANGE_ATTR and DragonSoulRefineWindow.REFINE_TYPE_CHANGE_ATTR == self.currentRefineType
		if not changeAttr:
			error = self.__ValidateRefine()
			if error:
				self.__PopUp(self.__ValidationText(error))
				return

		for i in xrange(self.refineSlotLockStartIndex):
			if not i in self.refineItemInfo:
				self.wndPopupDialog.SetText(localeInfo.DRAGON_SOUL_NOT_ENOUGH_MATERIAL)
				self.wndPopupDialog.Open()
				return

		if app.ENABLE_DS_CHANGE_ATTR:
			if DragonSoulRefineWindow.REFINE_TYPE_CHANGE_ATTR == self.currentRefineType:
				if not self.__CheckDoRefineChangeAttr():
					self.wndPopupDialog.SetText(localeInfo.DRAGON_SOUL_NOT_ENOUGH_MATERIAL)
					self.wndPopupDialog.Open()
					return

		player.SendDragonSoulRefine(DragonSoulRefineWindow.DS_SUB_HEADER_DIC[self.currentRefineType], self.refineItemInfo)

	# MT2009_PLUS_COR_ENTER_V1: Enter (routed from game.OnIMEReturn) acts like the
	# refine button, but only while the window is shown and every required
	# material slot is filled; otherwise returns False so Enter opens the chat.
	def PressDoRefineByEnter(self):
		if not self.isLoaded or not self.IsShow() or not self.refineItemInfo:
			return False
		for i in xrange(self.refineSlotLockStartIndex):
			if not i in self.refineItemInfo:
				return False
		# ignore auto-repeat of a held Enter key (one refine packet per press)
		now = app.GetTime()
		if now - getattr(self, "lastEnterRefineTime", -10.0) < 0.5:
			return True
		self.lastEnterRefineTime = now
		self.__PressDoRefineButton()
		return True

	# MT2009_PLUS_VEKIRION_V1 (Autor: Vekirion) - "Ulepsz wszystkie", his
	# MT2009_PLUS_REFINE_ALL_V1: the "Wszystkie" button left of "Uszlachetnij".
	# In the Klasa or Stopien mode it refines every stone on the page the
	# Alchemy window shows (the kind tab and the class page picked there), in
	# as many full recipes as the stones make: Klasa groups by kind and class
	# (the stones of lowest step and strength go first, so the best are kept
	# for last), Stopien by kind, class and step. The groups are taken from the
	# page as it is on the click, so stones a refine gives back are not
	# refined again in the same run. MT2009_PLUS_REFINE_ALL_V3: up to
	# REFINE_ALL_IN_FLIGHT requests are on their way at once (each uses its
	# own stones, and the server has no rate limit on alchemy refines), a new
	# one going out as each answer comes back, at most REFINE_ALL_PER_FRAME
	# per frame (V1/V2 sent one, waited for its answer, then 0.15 s more).
	# The Yang of the requests still on their way is kept aside before the
	# next goes out. A refusal (no Yang, a stone moved) or no answer in
	# REFINE_ALL_TIMEOUT stops the run, and the button reads "Stop" while it
	# runs. Poziom is not batched: a failed strength refine destroys the stone
	# at strength 0.
	REFINE_ALL_IN_FLIGHT = 10
	REFINE_ALL_PER_FRAME = 5
	REFINE_ALL_TIMEOUT = 5.0

	def __CreateRefineAllButton(self):
		try:
			button = ui.Button()
			button.SetParent(self.GetChild("board"))
			button.SetUpVisual("d:/ymir work/ui/dragonsoul/l_button01.tga")
			button.SetOverVisual("d:/ymir work/ui/dragonsoul/l_button02.tga")
			button.SetDownVisual("d:/ymir work/ui/dragonsoul/l_button03.tga")
			(x, y) = self.doRefineButton.GetLocalPosition()
			button.SetPosition(x - button.GetWidth() - 4, y)
			button.SetText(REFINE_ALL_TEXT)
			button.SetEvent(ui.__mem_func__(self.__PressRefineAllButton))
			button.Show()
			self.refineAllButton = button
		except Exception, e:
			dbg.TraceError("Exception : __CreateRefineAllButton, %s" % e)
			self.refineAllButton = None

	def __IsRefineAllRunning(self):
		return None != self.refineAllQueue

	def __PressRefineAllButton(self):
		if self.__IsRefineAllRunning():
			return	# MT2009_PLUS_REFINE_ALL_V3: a click while it runs is ignored

		if not self.currentRefineType in (self.REFINE_TYPE_GRADE, self.REFINE_TYPE_STEP):
			self.__PopUp(REFINE_ALL_ONLY_MODES % (uiScriptLocale.GRADE_SELECT, uiScriptLocale.STEP_SELECT))
			return

		try:
			wndDS = self.wndDragonSoul
			kind = wndDS.DSKindIndex
			page = wndDS.inventoryPageIndex
		except Exception:
			return

		# whatever was put in the slots by hand goes back first
		self.__FlushRefineItemSlot()
		self.Refresh()

		queue = self.__BuildRefineAllQueue(kind, page)
		if not queue:
			self.__PopUp(REFINE_ALL_NOTHING)
			return

		totalFee = 0
		for stones, fee in queue:
			totalFee += fee

		# MT2009_PLUS_REFINE_ALL_V2: starts at once, no question (the owner, 2 October).
		self.refineAllPending = (self.currentRefineType, queue)
		self.__AcceptRefineAll()

	def __CancelRefineAll(self):
		self.refineAllPending = None
		if self.refineAllQuestion:
			self.refineAllQuestion.Close()

	def __AcceptRefineAll(self):
		if self.refineAllQuestion:
			self.refineAllQuestion.Close()
		pending = self.refineAllPending
		self.refineAllPending = None
		if not pending or not self.IsShow():
			return
		refineType, queue = pending
		if refineType != self.currentRefineType:
			return

		self.refineAllType = refineType
		self.refineAllQueue = queue
		self.refineAllWaiting = False
		self.refineAllNextTime = 0.0
		self.refineAllInFlight = []	# the fee of each request on its way
		self.refineAllSentTime = app.GetTime()
		self.refineAllDone = 0
		self.refineAllSucceeded = 0
		# MT2009_PLUS_REFINE_ALL_V3: the button keeps its "Wszystkie" label
		# while the run goes (the owner, 3 October: too fast to need "Stop").
		self.__SendNextRefineAll()

	def __BuildRefineAllQueue(self, kind, page):
		try:
			import dragon_soul_refine_settings as settings
		except Exception:
			return []

		if app.ENABLE_DS_GRADE_MYTH:
			pageCount = player.DRAGON_SOUL_PAGE_COUNT
		else:
			pageCount = 5
		start = (kind * pageCount + page) * player.DRAGON_SOUL_PAGE_SIZE

		isGrade = self.REFINE_TYPE_GRADE == self.currentRefineType
		groups = {}
		order = []
		for pos in xrange(start, start + player.DRAGON_SOUL_PAGE_SIZE):
			vnum = player.GetItemIndex(player.DRAGON_SOUL_INVENTORY, pos)
			if not vnum or not self.__IsDragonSoul(vnum):
				continue
			if player.GetItemCount(player.DRAGON_SOUL_INVENTORY, pos) <= 0:
				continue
			ds_type, grade, step, strength = self.__GetDragonSoulTypeInfo(vnum)
			if isGrade:
				key = (ds_type, grade)
			else:
				key = (ds_type, grade, step)
			if not key in groups:
				groups[key] = []
				order.append(key)
			groups[key].append((step, strength, pos, vnum))

		queue = []
		for key in order:
			ds_type, grade = key[0], key[1]
			try:
				recipe = settings.dragon_soul_refine_info[ds_type]
				if isGrade:
					need = recipe["grade_need_count"][grade]
					fee = recipe["grade_fee"][grade]
				else:
					need = recipe["step_need_count"][key[2]]
					fee = recipe["step_fee"][key[2]]
			except (KeyError, IndexError, TypeError, ValueError):
				continue
			if need <= 0:
				continue

			stones = groups[key]
			stones.sort()
			for i in xrange(0, len(stones) - need + 1, need):
				queue.append(([(pos, vnum) for (step, strength, pos, vnum) in stones[i:i + need]], fee))

		return queue

	def __SendNextRefineAll(self):
		sent = 0
		while self.refineAllQueue and len(self.refineAllInFlight) < self.REFINE_ALL_IN_FLIGHT \
				and sent < self.REFINE_ALL_PER_FRAME:
			stones, fee = self.refineAllQueue[0]

			stillThere = True
			for pos, vnum in stones:
				if player.GetItemIndex(player.DRAGON_SOUL_INVENTORY, pos) != vnum:
					stillThere = False
					break
			if not stillThere:
				self.refineAllQueue.pop(0)
				continue

			# the Yang of the requests on their way is not spent yet
			if player.GetElk() - sum(self.refineAllInFlight) < fee:
				if not self.refineAllInFlight:
					self.__StopRefineAll()
					self.__PopUp(self.__ValidationText("DRAGON_SOUL_NOT_ENOUGH_MONEY"))
				return

			self.refineAllQueue.pop(0)
			refineInfo = {}
			for i in xrange(len(stones)):
				refineInfo[i] = (player.DRAGON_SOUL_INVENTORY, stones[i][0], 1)

			if not self.refineAllInFlight:
				self.refineAllSentTime = app.GetTime()
			self.refineAllInFlight.append(fee)
			sent += 1
			player.SendDragonSoulRefine(DragonSoulRefineWindow.DS_SUB_HEADER_DIC[self.refineAllType], refineInfo)

		if not self.refineAllQueue and not self.refineAllInFlight:
			self.__StopRefineAll()

	def __OnRefineAllResult(self, succeeded, refused):
		if not self.__IsRefineAllRunning() or not self.refineAllInFlight:
			return
		self.refineAllInFlight.pop(0)
		self.refineAllSentTime = app.GetTime()
		if refused:
			self.__StopRefineAll()
			return
		self.refineAllDone += 1
		if succeeded:
			self.refineAllSucceeded += 1

	def __StopRefineAll(self):
		if not self.__IsRefineAllRunning():
			return
		self.refineAllQueue = None
		self.refineAllInFlight = []
		self.refineAllWaiting = False
		if self.refineAllButton:
			self.refineAllButton.SetText(REFINE_ALL_TEXT)
		if self.refineAllDone > 0:
			chat.AppendChat(chat.CHAT_TYPE_INFO, REFINE_ALL_SUMMARY % (self.refineAllDone, self.refineAllSucceeded, self.refineAllDone - self.refineAllSucceeded))

	def OnUpdate(self):
		if not self.__IsRefineAllRunning():
			return
		# MT2009_PLUS_VEKIRION_V1: no request goes out on the way to another core.
		if not __import__("warpsafe").InGame():
			self.__StopRefineAll()
			return
		if self.refineAllInFlight and app.GetTime() - self.refineAllSentTime > self.REFINE_ALL_TIMEOUT:
			self.__StopRefineAll()
			return
		self.__SendNextRefineAll()

	def OnPressEscapeKey(self):
		self.Close()
		return True

	def Refresh(self):
		self.__RefreshRefineItemSlot()
		self.__ClearResultItemSlot()

	def __RefreshRefineItemSlot(self):
		try:
			for slotPos in xrange(self.wndRefineSlot.GetSlotCount()):
				self.wndRefineSlot.ClearSlot(slotPos)
				if slotPos < self.refineSlotLockStartIndex:
					# self.refineItemInfo[slotPos]의 정보확인
					# (실제로 아이템이 존재하는지 확인)
					# 존재 -> 아이템 아이콘을 슬롯에 셋팅.
					# 비존재 -> 아이템이 없으므로 강화창에서 삭제.
					if slotPos in self.refineItemInfo:
						invenType, invenPos, itemCount = self.refineItemInfo[slotPos]
						itemVnum = player.GetItemIndex(invenType, invenPos)
						if itemVnum:
							self.wndRefineSlot.SetItemSlot(slotPos, player.GetItemIndex(invenType, invenPos), itemCount)
						else:
							del self.refineItemInfo[slotPos]

					# 빈 슬롯에 reference 아이콘을 alpha 0.5로 셋팅.
					if not slotPos in self.refineItemInfo:
						try:
							reference_vnum = 0
							# strength 강화일 때는,
							# 0번 슬롯에 강화석을, 1번 슬롯에 용혼석을 놓는다.
							if DragonSoulRefineWindow.REFINE_TYPE_STRENGTH == self.currentRefineType:
								if DragonSoulRefineWindow.REFINE_STONE_SLOT == slotPos:
									reference_vnum = 100300
							else:
								reference_vnum = self.__MakeDragonSoulVnum(*self.currentRecipe["ds_info"])

							if app.ENABLE_DS_CHANGE_ATTR:
								if DragonSoulRefineWindow.REFINE_TYPE_CHANGE_ATTR == self.currentRefineType:
									if DragonSoulRefineWindow.DRAGON_SOUL_SLOT == slotPos:
										reference_vnum = 100700

							if 0 != reference_vnum:
								item.SelectItem(reference_vnum)
								itemIcon = item.GetIconImage()
								(width, height) = item.GetItemSize()
								self.wndRefineSlot.SetSlot(slotPos, 0, width, height, itemIcon, (1.0, 1.0, 1.0, 0.5))
								# slot 우측 하단에 숫자 뜨면 안 예쁨...
								self.wndRefineSlot.SetSlotCount(slotPos, 0)
						except:
							pass
					# refineSlotLockStartIndex 보다 작은 슬롯은 닫힌 이미지를 보여주면 안됨.
					self.wndRefineSlot.HideSlotBaseImage(slotPos)
				# slotPos >= self.refineSlotLockStartIndex:
				else:
					# 정상적인 경우라면 이 if문에 들어갈 일은 없겠지만,
					# (애초에 인덱스가 refineSlotLockStartIndex 이상인 슬롯에는 아이템을 넣지 못하게 했기 때문)
					# 혹시 모를 에러에 대비함.
					if slotPos in self.refineItemInfo:
						invenType, invenPos, itemCount = self.refineItemInfo[slotPos]
						remainCount = player.GetItemCount(invenType, invenPos)
						player.SetItemCount(invenType, invenPos, remainCount + itemCount)
						del self.refineItemInfo[slotPos]
					# refineSlotLockStartIndex 이상인 슬롯은 닫힌 이미지를 보여줘야함.
					self.wndRefineSlot.ShowSlotBaseImage(slotPos)

			# 강화창에 아무런 아이템이 없다면, 초기화해줌.
			# 위에서 중간 중간에 "del self.refineItemInfo[slotPos]"를 했기 때문에,
			# 여기서 한번 체크해줘야함.
			if not self.refineItemInfo:
				self.__Initialize()

			self.wndRefineSlot.RefreshSlot()
		except Exception, e:
			import dbg
			dbg.TraceError("Exception : __RefreshRefineItemSlot, %s" % e)

	def __GetEmptySlot(self, itemVnum = 0):
		# STRENGTH 강화의 경우, 용혼석 슬롯과 강화석 슬롯이 구분되어있기 떄문에
		# vnum을 알아야 한다.
		if DragonSoulRefineWindow.REFINE_TYPE_STRENGTH == self.currentRefineType:
			if 0 == itemVnum:
				return -1

			if self.__IsDragonSoul(itemVnum):
				if not DragonSoulRefineWindow.DRAGON_SOUL_SLOT in self.refineItemInfo:
					return DragonSoulRefineWindow.DRAGON_SOUL_SLOT
			else:
				if not DragonSoulRefineWindow.REFINE_STONE_SLOT in self.refineItemInfo:
					return DragonSoulRefineWindow.REFINE_STONE_SLOT
		else:
			for slotPos in xrange(self.wndRefineSlot.GetSlotCount()):
				if not slotPos in self.refineItemInfo:
					return slotPos

		return -1

	def AutoSetItem(self, inven, itemCount):
		invenType, invenPos = inven
		itemVnum = player.GetItemIndex(invenType, invenPos)
		emptySlot = self.__GetEmptySlot(itemVnum)
		if -1 == emptySlot:
			return

		# MT2009_PLUS_STRENGTH_HOLD_V1: in Poziom the strength stone (bean)
		# goes in with its whole stack, as a dragged stack does; the server
		# takes one per refine whatever the stack holds.
		if DragonSoulRefineWindow.REFINE_TYPE_STRENGTH == self.currentRefineType and itemVnum and not self.__IsDragonSoul(itemVnum):
			itemCount = max(itemCount, player.GetItemCount(invenType, invenPos))

		self.__SetItem((invenType, invenPos), emptySlot, itemCount)

	def __ClearResultItemSlot(self):
		self.wndResultSlot.ClearSlot(0)
		self.resultItemInfo = {}

	def RefineSucceed(self, inven_type, inven_pos):
		held = self.__RememberStrengthBean()	# MT2009_PLUS_STRENGTH_HOLD_V1
		self.__Initialize()
		self.Refresh()
		self.__RearmStrength(held, inven_type, inven_pos)

		itemCount = player.GetItemCount(inven_type, inven_pos)
		if itemCount > 0:
			self.resultItemInfo[0] = (inven_type, inven_pos, itemCount)
			self.wndResultSlot.SetItemSlot(0, player.GetItemIndex(inven_type, inven_pos), itemCount)
		self.__OnRefineAllResult(True, False)	# MT2009_PLUS_REFINE_ALL_V1

	def	RefineFail(self, reason, inven_type, inven_pos):
		if net.DS_SUB_HEADER_REFINE_FAIL == reason:
			held = self.__RememberStrengthBean()	# MT2009_PLUS_STRENGTH_HOLD_V1
			self.__Initialize()
			self.Refresh()
			self.__RearmStrength(held, inven_type, inven_pos)
			itemCount = player.GetItemCount(inven_type, inven_pos)
			if itemCount > 0:
				self.resultItemInfo[0] = (inven_type, inven_pos, itemCount)
				self.wndResultSlot.SetItemSlot(0, player.GetItemIndex(inven_type, inven_pos), itemCount)
			self.__OnRefineAllResult(False, False)	# MT2009_PLUS_REFINE_ALL_V1
		else:
			self.Refresh()
			self.__OnRefineAllResult(False, True)	# MT2009_PLUS_REFINE_ALL_V1

	# MT2009_PLUS_VEKIRION_V1 (Autor: Vekirion), his
	# MT2009_PLUS_STRENGTH_HOLD_V1: in Poziom the window keeps what it can
	# after a refine: the strength stone stack (less the one the server took)
	# goes back in slot 0, and the dragon stone the refine gave back (one
	# strength up on success, one down on a failure) goes back in slot 1 while
	# it can still be strengthened, so the next refine is one more click on
	# "Uszlachetnij" (or Enter). A stone at its cap, or one a failure at +0
	# destroyed, leaves slot 1 empty for the next stone.
	def __RememberStrengthBean(self):
		if DragonSoulRefineWindow.REFINE_TYPE_STRENGTH != self.currentRefineType:
			return None
		if self.__IsRefineAllRunning():
			return None
		if not DragonSoulRefineWindow.REFINE_STONE_SLOT in self.refineItemInfo:
			return None
		invenType, invenPos, itemCount = self.refineItemInfo[DragonSoulRefineWindow.REFINE_STONE_SLOT]
		vnum = self.refineItemVnums.get(DragonSoulRefineWindow.REFINE_STONE_SLOT, 0)
		return (invenType, invenPos, vnum)

	def __CanStrengthenQuietly(self, vnum):
		try:
			import dragon_soul_refine_settings
			ds_type, grade, step, strength = self.__GetDragonSoulTypeInfo(vnum)
			return strength < dragon_soul_refine_settings.dragon_soul_refine_info[ds_type]["strength_max_table"][grade][step]
		except Exception:
			return False

	def __RearmStrength(self, held, inven_type, inven_pos):
		if not held or DragonSoulRefineWindow.REFINE_TYPE_STRENGTH != self.currentRefineType:
			return
		try:
			invenType, invenPos, vnum = held
			count = player.GetItemCount(invenType, invenPos)
			if not vnum or player.GetItemIndex(invenType, invenPos) != vnum or count <= 0:
				return
			if not self.__SetItem((invenType, invenPos), DragonSoulRefineWindow.REFINE_STONE_SLOT, count):
				return

			if inven_type != player.DRAGON_SOUL_INVENTORY:
				return
			dsVnum = player.GetItemIndex(inven_type, inven_pos)
			if dsVnum and self.__IsDragonSoul(dsVnum) and player.GetItemCount(inven_type, inven_pos) > 0 \
					and self.__CanStrengthenQuietly(dsVnum):
				self.__SetItem((inven_type, inven_pos), DragonSoulRefineWindow.DRAGON_SOUL_SLOT, 1)
		except Exception, e:
			dbg.TraceError("Exception : __RearmStrength, %s" % e)

	def SetInventoryWindows(self, Inventory, DragonSoul):
		from _weakref import proxy
		self.wndInventory = proxy(Inventory)
		self.wndDragonSoul = proxy(DragonSoul)

	if app.ENABLE_DS_CHANGE_ATTR:
		def SetWindowType(self, type):
			self.__StopRefineAll()	# MT2009_PLUS_REFINE_ALL_V1
			self.currentRefineType = type

			if type == self.REFINE_TYPE_CHANGE_ATTR:
				for btn in self.refineChoiceButtonDict.values():
					btn.SetUp()
					btn.Disable()

				for btnTitle in self.refineChoiceButtonTitleDict.values():
					btnTitle.SetText("")

				self.refineChoiceButtonDict[self.REFINE_TYPE_STRENGTH].Down()
				self.refineChoiceButtonTitleDict[self.REFINE_TYPE_STRENGTH].SetText(uiScriptLocale.CHANGE_ATTR_SELECT)
			else:
				for btn in self.refineChoiceButtonDict.values():
					btn.SetUp()
					btn.Enable()

				self.refineChoiceButtonDict[self.REFINE_TYPE_GRADE].Down()
				self.refineChoiceButtonTitleDict[self.REFINE_TYPE_GRADE].SetText(uiScriptLocale.GRADE_SELECT)
				self.refineChoiceButtonTitleDict[self.REFINE_TYPE_STEP].SetText(uiScriptLocale.STEP_SELECT)
				self.refineChoiceButtonTitleDict[self.REFINE_TYPE_STRENGTH].SetText(uiScriptLocale.STRENGTH_SELECT)

			self.__Initialize()
			self.Refresh()

		def __CanRefineChangeAttr(self, vnum):
			if self.currentRecipe:
				return False

			ds_info = self.__GetDragonSoulTypeInfo(vnum)

			if DragonSoulRefineWindow.INVALID_DRAGON_SOUL_INFO == ds_info:
				self.__PopUp(localeInfo.DRAGON_SOUL_IS_NOT_DRAGON_SOUL)
				return False

			ds_type, grade, step, strength = ds_info
			if grade < 5:
				return False

			self.currentRecipe = { "ds_info" : ds_info, "need_count" : 2, "fee" : 500000, "material_count" : (1, 3, 9, 27, 81) }
			if self.currentRecipe:
				target_vnum = 100700
				material_count = self.currentRecipe["material_count"][step]
				for slotNumber in xrange(player.INVENTORY_MAX_NUM):
					itemVNum = player.GetItemIndex(slotNumber)
					itemCount = player.GetItemCount(slotNumber)

					if target_vnum == itemVNum and itemCount != 0:
						item.SelectItem(target_vnum)

						self.refineItemInfo[self.DRAGON_SOUL_SLOT] = (player.INVENTORY, slotNumber, material_count)

						chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.DS_SYSTEM_MSG_1 % (item.GetItemName(), material_count))
						break

				self.refineSlotLockStartIndex = self.currentRecipe["need_count"]
				self.wndMoney.SetText(localeInfo.NumberToMoneyString(self.currentRecipe["fee"]))
				return True
			else:
				self.__PopUp(localeInfo.DRAGON_SOUL_CANNOT_REFINE)
				return False

		def __CheckDoRefineChangeAttr(self):
			if not self.currentRecipe:
				return False

			ds_type, grade, step, strength = self.currentRecipe["ds_info"]
			invenType, invenPos, itemCount = self.refineItemInfo[self.DRAGON_SOUL_SLOT]
			if (grade >= 5) and (player.GetItemCountByVnum(100700) >= itemCount):
				return True

			return False
