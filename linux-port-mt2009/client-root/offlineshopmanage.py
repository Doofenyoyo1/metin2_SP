import app
import localeInfo
import shop
import colorinfo
import eventManager
import offlineshopui
import ui
import uiScriptLocale
import constInfo
import net
import chat
import offlineShopUi
import offlineShopBuilder
import offlineShopHistory
import uiCommon
import item
import uiInventory
import special_flags
import player
import ikashop
import shoppricepump
import mouseModule
import offlineShopSearch
import flamewindPath

EVENT_OPEN_MYSHOP_SHOP_MANAGE = "EVENT_OPEN_MYSHOP_SHOP_MANAGE" # args |
EVENT_CLOSE_MYSHOP_SHOP_MANAGE = "EVENT_CLOSE_MYSHOP_SHOP_MANAGE" # args |

g_isEditingPrivateShop = False
def IsEditingPrivateShop():
	global g_isEditingPrivateShop
	if g_isEditingPrivateShop:
		return True
	else:
		return False

def GetEarnings():
	return 0 if not constInfo.myshop_data.has_key("gold") else constInfo.myshop_data["gold"]

class OfflineShopManage(ui.ScriptWindow):
	WINDOW_WIDTH = 344
	WINDOW_HEIGHT = 380
	ITEM_SLOT_HEIGHT = 8*32
	SIGN_BAR_HEIGHT = 26


	class ShopState:
		def OnEnter(self, owner):
			return

		def Refresh(self, owner):
			return

		def OnExit(self, owner):
			return

		def OnSelectItemSlot(self, owner, slotIndex, click):
			pass

		def OnSelectEmptySlot(self, owner, inventorySlotIndex, shopSlotIndex, inventoryWindowType, itemVnum, itemCount):
			pass

		def OnQuickAddInventoryItem(self, owner, type, slotNumber):
			pass

		def OnFixedUpdate(self, owner):
			pass

	class OpenShopState(ShopState):
		def OnEnter(self, owner):
			owner.closeShopButton.Show()
			owner.itemSlot.Show()
			owner.signBar.Show()
			owner.withdrawItemsButton.Hide()
			owner.SetSize(owner.WINDOW_WIDTH, owner.WINDOW_HEIGHT)
			owner.board.SetSize(owner.WINDOW_WIDTH, owner.WINDOW_HEIGHT)
			owner.board.RefreshPosition()
			owner.titlebar.SetWidth(owner.WINDOW_WIDTH - 15)
			owner.RefreshPosition()
			owner.findShopButton.Enable()
			owner.findShopButton.SetUp()
			owner.findShopButton.Show()
			owner.editShopButton.Enable()
			owner.editShopButton.Show()
			owner.reopenButton.Hide()
			owner.logsButton.SetPosition(190, 35)

		def OnSelectItemSlot(self, owner, slotIndex, click):
			if click == "left":
				owner.ShowRemoveItemQuestionDialog(slotIndex)
			elif click == "right":
				if app.IsPressed(app.DIK_LCONTROL) or app.IsPressed(app.DIK_RCONTROL):
					owner.ShowEditAllItemQuestionDialog(slotIndex)
				else:
					owner.ShowEditItemQuestionDialog(slotIndex)

		def OnSelectEmptySlot(self, owner, inventorySlotIndex, shopSlotIndex, inventoryWindowType, itemVnum, itemCount):
			owner.ShowAddItemDialog(inventorySlotIndex, shopSlotIndex, inventoryWindowType, itemVnum, itemCount)

		def OnQuickAddInventoryItem(self, owner, type, slotNumber):
			itemVnum = player.GetItemIndex(slotNumber)
			if itemVnum > 0:
				item.SelectItem(itemVnum)
				(itemWidth, itemHeight) = item.GetItemSize()

				freeSlot = owner.itemSlot.FindEmptySlot(itemHeight)
				if freeSlot >= 0:
					itemVnum = player.GetItemIndex(slotNumber)
					itemCount = player.GetItemCount(slotNumber)
					self.OnSelectEmptySlot(owner, slotNumber, freeSlot, player.INVENTORY, itemVnum, itemCount)

		def OnFixedUpdate(self, owner):
			data = constInfo.myshop_data
			if data.has_key("time"):
				owner.SetTime(data["time"])

	class ClosedShopState(ShopState):
		def OnEnter(self, owner):
			owner.closeShopButton.Hide()
			owner.signBar.Hide()
			owner.withdrawItemsButton.Show()
			owner.SetTimeClosed()
			owner.itemSlot.MovePosition(0, -26)
			owner.findShopButton.Down()
			owner.findShopButton.Disable()
			owner.editShopButton.Disable()
			self.Refresh(owner)

		def Refresh(self, owner):
			destWidth = owner.WINDOW_WIDTH
			destHeight = owner.WINDOW_HEIGHT - owner.SIGN_BAR_HEIGHT
			if owner.IsDepositEmpty():
				owner.itemSlot.Hide()
				destHeight -= owner.ITEM_SLOT_HEIGHT
				destWidth -= 96
				owner.withdrawItemsButton.Down()
				owner.withdrawItemsButton.Disable()
				owner.reopenButton.Hide()
				owner.findShopButton.Hide()
				owner.editShopButton.Hide()
				owner.logsButton.SetPosition(100, 35)
			else:
				owner.itemSlot.SetPosition(0, 34)
				owner.itemSlot.Show()
				owner.withdrawItemsButton.Enable()
				owner.withdrawItemsButton.SetUp()
				owner.reopenButton.Show()
				owner.logsButton.SetPosition(190, 35)

			owner.SetSize(destWidth, destHeight)
			owner.board.SetSize(destWidth, destHeight)
			owner.titlebar.SetWidth(destWidth - 15)
			owner.RefreshPosition()
			if not owner.CanOpen():
				owner.Close()

		def OnExit(self, owner):
			owner.itemSlot.MovePosition(0, 26)

		def OnSelectItemSlot(self, owner, slotIndex, click):
			if click == "left":
				owner.RemoveItem(slotIndex)
			elif click == "right":
				if app.IsPressed(app.DIK_LCONTROL) or app.IsPressed(app.DIK_RCONTROL):
					owner.ShowEditAllItemQuestionDialog(slotIndex)
				else:
					owner.ShowEditItemQuestionDialog(slotIndex)

	def __init__(self):
		ui.ScriptWindow.__init__(self)
		self.__InitVariables()
		self.STATE_CLOSED = self.ClosedShopState()
		self.STATE_OPEN = self.OpenShopState()
		self.LoadScript()

		ikashop.SetBusinessBoard(self)

	def __InitVariables(self):
		self.tooltipItem = None
		self.questionDialog = None
		self.addItemDialog = None
		self.closeShopDialog = None
		self.editSignDialog = None
		self.endTime = 0
		self.visualState = None
		self.STATE_CLOSED = None
		self.STATE_OPEN = None
		self.itemSlot = None
		self.myShopSlotRenderer = None
		self.tooltip = None

		self.tax = 0
		self.board = 0
		self.tooltipBoard = 0
		self.tooltipLabel = 0
		self.titlebar = 0
		self.signBar = 0
		self.signLabel = 0
		self.editSignButton = 0
		self.earningsLabel = 0
		self.timeLeftLabel = 0
		self.findShopButton = 0
		self.reopenButton = 0
		self.editShopButton = 0
		self.logsButton = 0
		self.withdrawGoldButton = 0
		self.withdrawItemsButton = 0
		self.closeShopButton = 0
		self.clockIcon = 0
		self.clockAnimation = 0
		self.isEditMode = False

	def SetToolTip(self, tooltip, item_tooltip):
		# self.tooltipItem = item_tooltip
		self.tooltip = tooltip
		if self.myShopSlotRenderer:
			self.myShopSlotRenderer.tooltipItem = item_tooltip

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def Destroy(self):
		if self.myShopSlotRenderer:
			if self.itemSlot:
				self.itemSlot.checkSlotEvent = None
			self.myShopSlotRenderer.Destroy()

		if self.addItemDialog:
			self.addItemDialog.Close()

		if self.questionDialog:
			self.questionDialog.Close()

		if self.closeShopDialog:
			self.closeShopDialog.Close()

		if self.editSignDialog:
			self.editSignDialog.Close()

		self.__InitVariables()
		self.ClearDictionary()
		ikashop.SetBusinessBoard(None)

	def LoadScript(self):
		try:
			PythonScriptLoader = ui.PythonScriptLoader()
			PythonScriptLoader.LoadScriptFile(self, "UIScript/PrivateShopManage.py")

			self.board = self.GetChild("board")
			self.tooltipBoard = self.GetChild("TooltipInfo")
			self.tooltipLabel = self.GetChild("TooltipLabel")
			self.titlebar = self.GetChild("TitleBar")
			self.signBar = self.GetChild("NameSlot")
			self.signLabel = self.GetChild("NameLine")
			self.editSignButton = self.GetChild("ChangeShopSignButton")
			# self.itemSlot = self.GetChild("ItemSlot")
			self.totalEarningBar = self.GetChild("TotalEarningBar")
			self.earningsLabel = self.GetChild("TotalEarningLabel")
			self.timeLeftLabel = self.GetChild("TimeLeftLabel")
			self.findShopButton = self.GetChild("FindShopButton")
			self.reopenButton = self.GetChild("ReopenButton")
			self.editShopButton = self.GetChild("EditShopButton")
			self.logsButton = self.GetChild("LogsButton")
			self.withdrawGoldButton = self.GetChild("WithdrawGoldButton")
			self.withdrawItemsButton = self.GetChild("WithdrawItemsButton")
			self.closeShopButton = self.GetChild("CloseButton")
			self.clockIcon = self.GetChild("ClockIcon")
			self.clockAnimation = self.GetChild("ClockAnimation")
			self.shopSearchButton = self.GetChild("ShopSearchButton")

			self.titlebar.SetCloseEvent(ui.__mem_func__(self.Close))

			self.editShopButton.SAFE_SetEvent(self.ToggleEditMode)
			self.editShopButton.SAFE_SetStringEvent("MOUSE_OVER_IN", self.ShowTooltipBoard, uiScriptLocale.SHOP_MANAGE_TOOLTIP_EDIT_SHOP)
			self.editShopButton.SAFE_SetStringEvent("MOUSE_OVER_OUT", self.HideTooltipBoard)

			self.reopenButton.SAFE_SetEvent(self.ShowReopenDialog)
			self.reopenButton.SAFE_SetStringEvent("MOUSE_OVER_IN", self.ShowTooltipBoard, uiScriptLocale.SHOP_MANAGE_TOOLTIP_REOPEN)
			self.reopenButton.SAFE_SetStringEvent("MOUSE_OVER_OUT", self.HideTooltipBoard)

			self.logsButton.SAFE_SetEvent(self.OpenHistory)
			self.logsButton.SAFE_SetStringEvent("MOUSE_OVER_IN", self.ShowTooltipBoard,uiScriptLocale.SHOP_MANAGE_TOOLTIP_LOGS)
			self.logsButton.SAFE_SetStringEvent("MOUSE_OVER_OUT", self.HideTooltipBoard)

			self.findShopButton.SAFE_SetEvent(self.FindShop)
			self.findShopButton.SAFE_SetStringEvent("MOUSE_OVER_IN", self.ShowTooltipBoard, uiScriptLocale.SHOP_MANAGE_TOOLTIP_FIND_SHOP)
			self.findShopButton.SAFE_SetStringEvent("MOUSE_OVER_OUT", self.HideTooltipBoard)

			self.editSignButton.SAFE_SetEvent(self.ShowEditSignDialog)
			self.editSignButton.SAFE_SetStringEvent("MOUSE_OVER_IN", self.ShowTooltipBoard, uiScriptLocale.SHOP_MANAGE_TOOLTIP_EDIT_SIGN)
			self.editSignButton.SAFE_SetStringEvent("MOUSE_OVER_OUT", self.HideTooltipBoard)

			self.withdrawGoldButton.SAFE_SetEvent(self.WithdrawGold)
			self.withdrawGoldButton.SAFE_SetStringEvent("MOUSE_OVER_IN", self.ShowTooltipBoard, uiScriptLocale.SHOP_MANAGE_TOOLTIP_WITHDRAW)
			self.withdrawGoldButton.SAFE_SetStringEvent("MOUSE_OVER_OUT", self.HideTooltipBoard)

			self.withdrawItemsButton.SAFE_SetEvent(self.RemoveAllItems)
			self.withdrawItemsButton.SAFE_SetStringEvent("MOUSE_OVER_IN", self.ShowTooltipBoard, uiScriptLocale.SHOP_MANAGE_TOOLTIP_WITHDRAW_ITEMS)
			self.withdrawItemsButton.SAFE_SetStringEvent("MOUSE_OVER_OUT", self.HideTooltipBoard)

			self.closeShopButton.SAFE_SetEvent(self.ShowCloseShopQuestionDialog)
			self.closeShopButton.SAFE_SetStringEvent("MOUSE_OVER_IN", self.ShowTooltipBoard, uiScriptLocale.SHOP_MANAGE_TOOLTIP_CLOSE_SHOP)
			self.closeShopButton.SAFE_SetStringEvent("MOUSE_OVER_OUT", self.HideTooltipBoard)

			self.totalEarningBar.SAFE_SetStringEvent("MOUSE_OVER_IN", self.ShowRevenueToolTip)
			self.totalEarningBar.SAFE_SetStringEvent("MOUSE_OVER_OUT", self.HideToolTip)

			self.shopSearchButton.SAFE_SetEvent(self.ShopSearch)

			self.myShopSlotRenderer = offlineShopUi.MyShopSlots(itemSlotParent=self.board)
			self.itemSlot = self.myShopSlotRenderer.itemSlot
			self.itemSlot.SetWindowHorizontalAlignCenter()
			self.itemSlot.SetPosition(0, 56)
			# self.myShopSlotRenderer.SAFE_SetSelectItemSlotEvent(self.OnSelectItemSlot)
			self.myShopSlotRenderer.SAFE_SetSelectEmptySlotEvent(self.OnSelectEmptySlot)
			self.myShopSlotRenderer.itemSlot.SAFE_SetButtonEvent("LEFT", "EXIST", self.OnSelectItemSlotLeftClick)
			self.myShopSlotRenderer.itemSlot.SAFE_SetButtonEvent("RIGHT", "EXIST", self.OnSelectItemSlotRightClick)

			self.itemSlot.checkSlotEvent = ui.__mem_func__(self.__CanPlaceOnShopSlot)

			eventManager.EventManager().add_observer(uiInventory.EVENT_QUICK_ADD_INVENTORY_ITEM, self.OnQuickAddInventoryItem)

			self.HideTooltipBoard()
			self.SetEarnings()
			self.SwitchState(self.STATE_CLOSED)

		except:
			import exception
			exception.Abort("OfflineShopManage.LoadDialog.BindObject")

	def __CanPlaceOnShopSlot(self, destSlotPos, itemHeight):
		return offlineshopui.CanPlaceItemOnShopSlot(destSlotPos, itemHeight,
													special_flags.GetFlag(
														special_flags.SHOP_SLOT_UNLOCK_PROGRESS_FLAG),
													offlineshopui.HasShopPremium())

	def OnQuickAddInventoryItem(self, type, slotNumber):
		if type != "myshop_manage":
			return

		self.visualState.OnQuickAddInventoryItem(self, type, slotNumber)

	def ShopSearch(self):
		eventManager.EventManager().send_event(offlineShopSearch.EVENT_OPEN_SHOP_SEARCH)

	def OpenHistory(self):
		eventManager.EventManager().send_event(offlineShopHistory.EVENT_OPEN_SHOP_HISTORY)

	def FindShop(self):
		ikashop.SendFindMyShop()

	def ShowEditSignDialog(self):
		# if not offlineshopui.HasShopPremium():
		# 	chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.SHOP_NO_PREMIUM)
		# 	return

		if self.editSignDialog:
			self.editSignDialog.Close()

		dialog = uiCommon.InputDialog()
		dialog.SetTitle(localeInfo.PRIVATE_SHOP_INPUT_NAME_DIALOG_TITLE)
		dialog.acceptButton.SAFE_SetEvent(self.AcceptEditSignDialog)
		dialog.cancelButton.SAFE_SetEvent(dialog.Close)
		dialog.SetMaxLength(32)
		dialog.Open()
		self.editSignDialog = dialog

	def ToggleEditMode(self):
		ikashop.SendRequestEdit(not self.isEditMode)

	def SetEditMode(self, state):
		self.isEditMode = state

		if state:
			self.editShopButton.SetUpVisual(flamewindPath.GetPublic("edit_button_03"))
			self.editShopButton.SetOverVisual(flamewindPath.GetPublic("edit_button_03"))
			self.editShopButton.SetDownVisual(flamewindPath.GetPublic("edit_button_03"))
		else:
			self.editShopButton.SetUpVisual(flamewindPath.GetPublic("edit_button_01"))
			self.editShopButton.SetOverVisual(flamewindPath.GetPublic("edit_button_02"))
			self.editShopButton.SetDownVisual(flamewindPath.GetPublic("edit_button_02"))

	def ShowReopenDialog(self):
		if self.editSignDialog:
			self.editSignDialog.Close()

		dialog = uiCommon.InputDialog()
		dialog.SetHeight(140)
		dialog.board.SetHeight(140)
		dialog.SetTitle(localeInfo.MYSHOP_REOPEN_TITLE)
		dialog.acceptButton.MovePosition(0, 40)
		dialog.cancelButton.MovePosition(0, 40)
		dialog.acceptButton.SAFE_SetEvent(self.AcceptReopenDialog)
		dialog.cancelButton.SAFE_SetEvent(dialog.Close)
		dialog.inputValue.SetPlaceholder(localeInfo.MYSHOP_REOPEN_INPUT_PLACEHOLDER)
		dialog.inputSlot.SetPosition(0,66)
		dialog.SetMaxLength(32)
		dialog.Open()

		self.editSignDialog = dialog

		combo = ui.ComboBox()
		combo.SetParent(self.editSignDialog.board)
		combo.SetWindowHorizontalAlignCenter()
		combo.SetSize(162, 19)
		combo.SetPosition(0, 35)
		combo.SetCurrentItem(uiScriptLocale.SHOP_CREATE_SELECT_DEFAULT)
		combo.SAFE_SetEvent(self.OnTimeSelect)
		combo.Show()
		offlineShopBuilder.PrepareComboBox(combo, (0,))
		self.editSignDialog.ElementDictionary["TimeSelect"] = combo


	def OnTimeSelect(self, index):
		if self.editSignDialog:
			data = offlineShopBuilder.SHOP_TIME_OPTIONS[index]
			isPremiumOption = data.has_key("isSpecial") and data["isSpecial"]

			self.editSignDialog.ElementDictionary["TimeSelect"].SetCurrentItem(offlineShopBuilder.GetTimeSelectString(index), offlineShopBuilder.SPECIAL_ITEM_COLOR if isPremiumOption else None)
			self.editSignDialog.ElementDictionary["TimeSelect"].SetIndex(index)

			self.editSignDialog.SetTop()

	def ShowCloseShopQuestionDialog(self):
		dialog = uiCommon.QuestionDialog()
		dialog.SetText(localeInfo.PRIVATE_SHOP_CLOSE_QUESTION)
		dialog.acceptButton.SAFE_SetEvent(self.AcceptCloseShopDialog)
		dialog.cancelButton.SAFE_SetEvent(dialog.Close)
		dialog.Open()
		self.closeShopDialog = dialog

	def ShowRemoveItemQuestionDialog(self, slotIndex):
		if constInfo.myshop_data["items"].has_key(slotIndex):
			if self.questionDialog:
				self.questionDialog.Close()
			dialog = uiCommon.QuestionDialog()
			item.SelectItem(constInfo.myshop_data["items"][slotIndex]["vnum"])
			dialog.SetText(localeInfo.SHOP_REMOVE_ITEM_QUESTION % (constInfo.myshop_data["items"][slotIndex]["count"], item.GetItemName()))
			dialog.acceptButton.SAFE_SetEvent(self.AcceptRemoveItemDialog, slotIndex)
			dialog.SAFE_SetCancelEvent(dialog.Close)
			dialog.Open()
			self.questionDialog = dialog

	def ShowEditAllItemQuestionDialog(self, shopSlotIndex):
		self.ShowEditItemQuestionDialog(shopSlotIndex, massEdit=True)

	def ShowEditItemQuestionDialog(self, shopSlotIndex, massEdit=False):
		if constInfo.myshop_data["items"].has_key(shopSlotIndex):
			if self.addItemDialog:
				self.addItemDialog.Close()

			itemData = constInfo.myshop_data["items"][shopSlotIndex]
			itemSlotList = []
			if massEdit:
				itemTuple = (itemData["vnum"], itemData["count"], itemData["price"], itemData["sockets"])
				for slotIdx, data in constInfo.myshop_data["items"].items():
					compareTuple = (data["vnum"], data["count"], data["price"], data["sockets"])
					if itemTuple == compareTuple:
						itemSlotList.append(slotIdx)
			else:
				itemSlotList.append(shopSlotIndex)

			dialog = uiCommon.NewMoneyInputDialog()
			if massEdit:
				dialog.SetTitle(localeInfo.PRIVATE_SHOP_INPUT_EDIT_ALL_PRICE_DIALOG_TITLE)
			else:
				dialog.SetTitle(localeInfo.PRIVATE_SHOP_INPUT_EDIT_PRICE_DIALOG_TITLE)
			dialog.SetMaxLength(10)
			dialog.SetCancelEvent(ui.__mem_func__(dialog.Close))
			dialog.acceptButton.SAFE_SetEvent(self.AcceptEditItemDialog, shopSlotIndex, massEdit)
			dialog.closeEvent = lambda arg1=itemSlotList: self.DeactivateItems(arg1)
			dialog.inputValue.OnPressEscapeKey = ui.__mem_func__(self.__CloseAddInput)
			dialog.Open()

			# ikashop.SendRequestEdit(True)
			self.addItemDialog = dialog
			self.ActivateItems(itemSlotList)

	def __CloseAddInput(self):
		self.addItemDialog.Close()
		return True

	def ShowAddItemDialog(self, inventorySlotIndex, shopSlotIndex, inventoryWindowType, itemVnum, itemCount):
		if not constInfo.myshop_data["items"].has_key(shopSlotIndex):
			if self.addItemDialog:
				self.addItemDialog.Close()

			dialog = uiCommon.NewMoneyInputDialog()
			dialog.SetTitle(localeInfo.PRIVATE_SHOP_INPUT_PRICE_DIALOG_TITLE)
			dialog.SetMaxLength(10)
			dialog.SetAcceptEvent(lambda arg1=inventorySlotIndex, arg2=shopSlotIndex, arg3=inventoryWindowType: self.AcceptAddItemDialog(arg1, arg2, arg3))
			# dialog.acceptButton.SAFE_SetEvent(self.AcceptAddItemDialog, inventorySlotIndex, shopSlotIndex, inventoryWindowType)
			dialog.SetCancelEvent(ui.__mem_func__(dialog.Close))
			dialog.inputValue.OnPressEscapeKey = ui.__mem_func__(self.__CloseAddInput)
			dialog.Open()

			sockets = tuple(player.GetItemMetinSocket(inventoryWindowType, inventorySlotIndex, i) for i in xrange(player.METIN_SOCKET_MAX_NUM))
			itemPrice = offlineShopBuilder.GetPrivateShopItemPrice(itemVnum, itemCount, sockets)

			if itemPrice > 0:
				dialog.SetValue(itemPrice)

			self.addItemDialog = dialog
		else:
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.OFFLINE_SHOP_CANNOT_PLACE_ITEM_ON_ITEM)

	def AcceptRemoveItemDialog(self, slotIndex):
		if self.questionDialog:
			self.questionDialog.Close()
		self.RemoveItem(slotIndex)

	def AcceptAddItemDialog(self, inventorySlotIndex, shopSlotIndex, inventoryWindowType):
		if not self.addItemDialog:
			return

		text = self.addItemDialog.GetText()

		if not text:
			return True

		inputPrice = localeInfo.MoneyStringToNumber(self.addItemDialog.GetText())
		if inputPrice <= 0:
			return

		if inputPrice > player.GOLD_MAX:
			inputPrice = player.GOLD_MAX

		self.addItemDialog.Close()

		itemVnum = player.GetItemIndex(inventoryWindowType, inventorySlotIndex)
		itemCount = player.GetItemCount(inventoryWindowType, inventorySlotIndex)
		sockets = tuple(player.GetItemMetinSocket(inventoryWindowType, inventorySlotIndex, i) for i in xrange(player.METIN_SOCKET_MAX_NUM))
		offlineShopBuilder.SetPrivateShopItemPrice(itemVnum, itemCount, inputPrice, sockets)
		self.AddItem(inventorySlotIndex, shopSlotIndex, inventoryWindowType, inputPrice)

	def AcceptEditItemDialog(self, shopSlotIndex, massEdit):
		if not self.addItemDialog:
			return

		text = self.addItemDialog.GetText()
		if not text:
			return True

		inputPrice = localeInfo.MoneyStringToNumber(self.addItemDialog.GetText())
		if inputPrice <= 0:
			return

		if inputPrice > player.GOLD_MAX:
			inputPrice = player.GOLD_MAX

		shop_total_value = 0
		for i,v in constInfo.myshop_data["items"].items():
			shop_total_value += v["price"]

		self.addItemDialog.Close()
		data = constInfo.myshop_data["items"]
		if data.has_key(shopSlotIndex):
			itemData = data[shopSlotIndex]
			if massEdit:
				item_list = []
				itemTuple = (itemData["vnum"], itemData["count"], itemData["price"], itemData["sockets"])
				for i,v in data.items():
					if (v["vnum"], v["count"], v["price"], v["sockets"]) == itemTuple:
						item_list.append(v)
						shop_total_value = shop_total_value - v["price"] + inputPrice
						if shop_total_value > player.GOLD_MAX:
							chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.TOO_HIGH_SHOP_VALUE)
							return
				shoppricepump.Queue([(i, inputPrice) for i in item_list])
			else:
				self.__SendEditItemPricePacket(itemData, inputPrice)

	def __SendEditItemPricePacket(self, itemData, itemPrice):
		ikashop.SendEditItem(itemData["id"], itemPrice)
		offlineShopBuilder.SetPrivateShopItemPrice(itemData["vnum"], itemData["count"], itemPrice, itemData["sockets"])

	def AcceptEditSignDialog(self):
		if self.editSignDialog:
			self.EditShopSign(self.editSignDialog.GetText())
			self.editSignDialog.Close()
			self.editSignDialog = None

	def AcceptCloseShopDialog(self):
		if self.closeShopDialog:
			self.closeShopDialog.Close()
			self.CloseShop()

	def AcceptReopenDialog(self):
		if self.editSignDialog:
			if self.editSignDialog.ElementDictionary["TimeSelect"].index < 0:
				return
			if len(self.editSignDialog.GetText()) == 0:
				return
			self.ReopenShop(self.editSignDialog.GetText(), self.editSignDialog.ElementDictionary["TimeSelect"].index)
			self.editSignDialog.Close()
			self.editSignDialog = None

	def Load(self):
		data = constInfo.myshop_data
		if data.has_key("vid"):
			self.RefreshShopSign()
			self.SetTime(data["time"])
			if self.IsOpened():
				self.SwitchState(self.STATE_OPEN)
			else:
				self.SwitchState(self.STATE_CLOSED)

			self.RefreshItemDeposit()

	def RefreshShopSign(self):
		self.signLabel.SetText(constInfo.myshop_data["name"])

	def ActivateItems(self, itemSlots):
		for slot in itemSlots:
			if constInfo.myshop_data["items"].has_key(slot):
				self.itemSlot.ActivateSlot(slot)
		self.itemSlot.RefreshSlot()

	def DeactivateItems(self, itemSlots):
		for slot in itemSlots:
			if constInfo.myshop_data["items"].has_key(slot):
				self.itemSlot.DeactivateSlot(slot)
		self.itemSlot.RefreshSlot()

	def RefreshItemDeposit(self):
		self.ClearItemStock()
		for slotIdx,data in constInfo.myshop_data["items"].iteritems():
			count = data["count"]
			if count < 2:
				count = 0

			socket = tuple(data["sockets"])
			self.itemSlot.SetItemSlot(slotIdx, data["vnum"], count, socket=socket)

		self.visualState.Refresh(self)
		self.itemSlot.RefreshSlot()

	def ClearItemStock(self):
		for x in range(shop.SHOP_PLAYER_WIDTH):
			for y in range(shop.SHOP_PLAYER_HEIGHT):
				self.itemSlot.ClearSlot(x + shop.SHOP_PLAYER_WIDTH * y)

	def IsDepositEmpty(self):
		if not constInfo.myshop_data.has_key("items"):
			return False
		return len(constInfo.myshop_data["items"]) < 1

	def Toggle(self):
		if self.IsShow():
			self.Close()
		else:
			self.Open()

	def SetEarnings(self):
		self.earningsLabel.SetText(localeInfo.NumberToMoneyString(GetEarnings()))
		if GetEarnings() < 1:
			self.withdrawGoldButton.Down()
			self.withdrawGoldButton.Disable()
		else:
			self.withdrawGoldButton.SetUp()
			self.withdrawGoldButton.Enable()

		if not self.CanOpen():
			self.Close()

	def IsOpened(self):
		if constInfo.myshop_data.has_key("time"):
			return max(0, constInfo.myshop_data["time"] - app.GetGlobalTimeStamp())
		else:
			return False

	def SetTime(self, endTime):

		if offlineShopUi.SetTimeLabel(endTime, self.timeLeftLabel):
			self.SetTimeNearClosed()
		else:
			self.clockIcon.Hide()
			self.clockAnimation.Show()

	def SetTimeNearClosed(self):
		self.clockIcon.Hide()
		self.clockAnimation.Show()

	def SetTimeClosed(self):
		self.timeLeftLabel.SetText(uiScriptLocale.SHOP_CLOSED)
		self.timeLeftLabel.SetPackedFontColor(colorinfo.NEGATIVE_COLOR)
		self.clockIcon.Show()
		self.clockAnimation.Hide()

	def OnFixedUpdate(self):
		self.visualState.OnFixedUpdate(self)

	def ShowTooltipBoard(self, text):
		self.tooltipBoard.SetSize(int(len(text)*6.5) + 20, self.tooltipBoard.GetHeight())
		self.tooltipLabel.SetText(text)
		self.tooltipBoard.RefreshPosition()
		self.tooltipBoard.Show()

	def HideTooltipBoard(self):
		if self.tooltipBoard:
			self.tooltipBoard.Hide()

	def SwitchState(self, state):
		if self.visualState != None:
			self.visualState.OnExit(self)
		self.visualState = state
		self.visualState.OnEnter(self)

	def RemoveAllItems(self):
		ikashop.SendRemoveAllItem()

	def RemoveItem(self, slotIndex):
		itemData = constInfo.myshop_data["items"].get(slotIndex)
		if not itemData:
			return
		ikashop.SendRemoveItem(itemData["id"])

	def AddItem(self, inventorySlotIndex, shopSlotIndex, inventoryWindowType, inputPrice):
		# isLocked = offlineShopUi.IsInLockedArea(shopSlotIndex)
		# if isLocked:
		# 	print "locked area"

		# isPremium = offlineShopUi.IsInPremiumArea(shopSlotIndex)
		# if isPremium:
		# 	print "premium area"

		# if not isPremium and not isLocked:
		# 	print "normal area"

		# print "/update_shop_item add|%d|%d|%d|%ld" % (shopSlotIndex, inventorySlotIndex, inventoryWindowType, inputPrice)
		# net.SendChatPacket("/update_shop_item add|%d|%d|%d|%ld" % (shopSlotIndex, inventorySlotIndex, inventoryWindowType, inputPrice))
		ikashop.SendAddItem(inventoryWindowType, inventorySlotIndex, shopSlotIndex, inputPrice)

	def WithdrawGold(self):
		ikashop.SendSafeboxGetValutes()

	def CloseShop(self):
		ikashop.SendForceCloseShop()

	def EditShopSign(self, shopSign):
		# net.SendChatPacket("/shop_name %s" % (shopSign.replace(" ", "\\")))
		ikashop.SendChangeName(shopSign)

	def ReopenShop(self, name, time):
		ikashop.SendShopReopen(name, time)

	def CanOpen(self):
		return True
		# canOpen = False
		# if self.IsOpened():
		# 	canOpen = True
		#
		# if not self.IsDepositEmpty():
		# 	canOpen = True
		#
		# if GetEarnings() > 0:
		# 	canOpen = True
		#
		# return canOpen

	def OnSelectItemSlotLeftClick(self, slotIndex):
		self.OnSelectItemSlot(slotIndex, click="left")

	def OnSelectItemSlotRightClick(self, slotIndex):
		self.OnSelectItemSlot(slotIndex, click="right")

	def OnSelectItemSlot(self, slotIndex, click):
		if mouseModule.mouseController.isAttached():
			return

		self.visualState.OnSelectItemSlot(self, slotIndex, click)

	def OnSelectEmptySlot(self, inventorySlotIndex, shopSlotIndex, inventoryWindowType, itemVnum, itemCount):
		self.visualState.OnSelectEmptySlot(self, inventorySlotIndex, shopSlotIndex, inventoryWindowType, itemVnum, itemCount)

	def ShowRevenueToolTip(self):
		if self.tooltip:
			if self.visualState is self.STATE_CLOSED:
				return

			total_revenue = 0
			for i,v in constInfo.myshop_data["items"].items():
				total_revenue += v["price"] * (100 - self.tax) / 100

			self.tooltip.ClearToolTip()
			self.tooltip.AppendTextLine(localeInfo.OFFLINE_SHOP_ESTIMATED_REVENUE, isBold=True, isLarge=True)
			self.tooltip.AppendSpace(5)
			self.tooltip.AppendTextLine(localeInfo.NumberToMoneyString(total_revenue), color=colorinfo.POSITIVE_COLOR)
			self.tooltip.ShowToolTip()


	def HideToolTip(self):
		if self.tooltip:
			self.tooltip.HideToolTip()

	def Open(self):
		if self.IsShow():
			return
		# if not self.CanOpen():
		# 	chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.MYSHOP_MANAGE_OPEN_ERROR)
		# 	return
		ikashop.SendOpenShopOwner()

	def Close(self):
		if not self.IsShow():
			return
		self.Hide()
		ikashop.SendCloseMyShopBoard()
		ikashop.SendSafeboxClose()

		self.SetEditMode(False)

		global g_isEditingPrivateShop
		g_isEditingPrivateShop = False
		eventManager.EventManager().send_event(EVENT_CLOSE_MYSHOP_SHOP_MANAGE)
		self.HideTooltipBoard()

		offlineShopBuilder.SaveItemPriceDictFile()

		if self.editSignDialog:
			self.editSignDialog.Close()
		if self.closeShopDialog:
			self.closeShopDialog.Close()
		if self.addItemDialog:
			self.addItemDialog.Close()
		if self.questionDialog:
			self.questionDialog.Close()

	def OnPressEscapeKey(self):
		self.Close()
		return True

	def IsCreatingAuction(self):
		return False

	def Show(self):
		ui.Window.Show(self)
		global g_isEditingPrivateShop
		g_isEditingPrivateShop = True
		eventManager.EventManager().send_event(EVENT_OPEN_MYSHOP_SHOP_MANAGE)
		ikashop.SendSafeboxOpen()

	#### BINARY CALLS ####
	def OpenShopOwner(self, data, tax): # name, duration, items
		self.Show()
		if data["duration"] < 1:
			self.SwitchState(self.STATE_CLOSED)
		else:
			self.SwitchState(self.STATE_OPEN)
			self.SetTime(data["duration"])

		# ordering items by cell and storing info
		data['items'] = {item['cell']: item for item in data['items']}
		constInfo.myshop_data = data

		self.tax = tax

		self.RefreshShopSign()
		self.RefreshItemDeposit()

	def OpenShopOwnerEmpty(self):
		self.Show()
		self.SetTime(0)
		self.SwitchState(self.STATE_CLOSED)

	def ShopOwnerEditItem(self, id, data):
		for i,v in constInfo.myshop_data["items"].items():
			if v["id"] == id:
				v["price"] = data["price"]
				break

	def ShopOwnerRemoveItem(self, itemid):
		if 'items' in constInfo.myshop_data:
			for cell,item in constInfo.myshop_data["items"].items():
				if item["id"] == itemid:
					constInfo.myshop_data["items"].pop(cell)
					self.RefreshItemDeposit()
					break

	# only for yang refresh
	def SetupSafebox(self, yang, items):
		constInfo.myshop_data["gold"] = yang
		self.SetEarnings()

	def SetNotifications(self, notifications):
		cached = ikashop.GetNotifications()
		for i, notification in enumerate(notifications):
			type = notification['type']
			who = notification['who']
			what = notification['what']
			format = notification['format']
			datetime = notification['datetime']
			save = i == len(notifications) - 1
			ikashop.RegisterNotification(type, who, what, format, datetime, save)
		notifications = cached + notifications
		notifications = sorted(notifications, key=lambda val: val['datetime'], reverse=True)
		eventManager.EventManager().send_event(offlineShopHistory.EVENT_SET_SHOP_HISTORY, notifications)
