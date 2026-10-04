import offlineshopui
import ui
import grp
import shop
import snd
import mouseModule
import item
import chat
import player
import localeInfo
import uiCommon
import simplejson as json
import file_utils
import uiScriptLocale
import colorinfo
import constInfo
import eventManager
import special_flags
import uiInventory
import net
import shopautoprice # MT2009_PLUS_SHOP_AUTO_PRICE_V1

g_isBuildingPrivateShop = False
g_itemPriceDict = {}

def LoadItemPriceDict():
	global g_itemPriceDict
	Clear()
	stringData = file_utils.GetFileContent("cache/item_price.json")
	if stringData != "":
		g_itemPriceDict = json.loads(stringData)

def SaveItemPriceDictFile():
	global g_itemPriceDict
	if len(g_itemPriceDict) > 0:
		file_utils.WriteFile("cache/item_price.json", json.dumps(g_itemPriceDict))

def Clear():
	global g_itemPriceDict
	global g_isBuildingPrivateShop
	g_itemPriceDict={}
	g_isBuildingPrivateShop = False

def IsBuildingPrivateShop():
	global g_isBuildingPrivateShop
	if player.IsOpenPrivateShop() or g_isBuildingPrivateShop:
		return True
	else:
		return False


def SetPrivateShopItemPrice(itemVnum, count, itemPrice, sockets=None):
	global g_itemPriceDict
	if not g_itemPriceDict.has_key(str(itemVnum)):
		g_itemPriceDict[str(itemVnum)] = { str(count): {} }

	if not g_itemPriceDict[str(itemVnum)].has_key(str(count)):
		g_itemPriceDict[str(itemVnum)][str(count)] = {}

	if sockets is None:
		sockets = tuple(0 for i in xrange(player.METIN_SOCKET_MAX_NUM))

	g_itemPriceDict[str(itemVnum)][str(count)][str(sockets)] = itemPrice


def GetPrivateShopItemPrice(itemVnum, count=1, sockets=None):
	try:
		global g_itemPriceDict

		if sockets is None:
			sockets = tuple(0 for i in xrange(player.METIN_SOCKET_MAX_NUM))

		return g_itemPriceDict[str(itemVnum)][str(count)][str(sockets)]
	except KeyError:
		return 0

SPECIAL_ITEM_COLOR = grp.GenerateColor(float(225) / 255, float(200) / 255, float(100) / 255, 1.0)
SHOP_TIME_OPTIONS = [
	{
		"time": 0,
		"cost": 0
	},
	{
		"time" : 8,
		"cost" : 6000
	},
	{
		"time": 12,
		"cost": 10000
	},
	{
		"time": 24,
		"cost": 20000
	},
	{
		"time": 48,
		"cost": 40000,
		"isSpecial": True,
	},
	{
		"time": 72,
		"cost": 60000,
		"isSpecial" : True,
	},
]

def GetTimeSelectString(index):
	data = SHOP_TIME_OPTIONS[index]
	hours = data["time"]
	days = data["time"] / 24
	output = ""
	if hours == 0:
		output = uiScriptLocale.SHOP_CREATE_SELECT_NORMAL % localeInfo.NumberToMoneyString(data["cost"])
	else:
		if days > 0:
			output = uiScriptLocale.SHOP_CREATE_SELECT_OFFLINE_DAYS % (
			days, localeInfo.NumberToMoneyString(data["cost"]))
		else:
			output = uiScriptLocale.SHOP_CREATE_SELECT_OFFLINE % (hours, localeInfo.NumberToMoneyString(data["cost"]))

	if data.has_key("isSpecial") and data["isSpecial"]:
		output = "* " + output + " *"
	return output

def PrepareComboBox(combobox, excludeIndices=()):
	for i in range(len(SHOP_TIME_OPTIONS)):
		if i in excludeIndices:
			continue
		data = SHOP_TIME_OPTIONS[i]
		combobox.InsertItem(i, GetTimeSelectString(i),SPECIAL_ITEM_COLOR if data.has_key("isSpecial") and data["isSpecial"] else None)

class OfflineShopBuilder(ui.ScriptWindow):

	def __init__(self):
		ui.ScriptWindow.__init__(self)
		LoadItemPriceDict()

		self.itemStock = {}
		self.savedShopConfiguration = {}
		self.tooltipItem = None
		self.selectedTimeOption = -1
		self.totalGold = 0
		self.tax = 0
		self.serverTax = 0

		self.questionDlg = None
		self.priceInputBoard = None

		eventManager.EventManager().add_observer(eventManager.OPEN_PRIVATE_SHOP, self.__ClearShopConfiguration)
		eventManager.EventManager().add_observer(uiInventory.EVENT_QUICK_ADD_INVENTORY_ITEM, self.OnQuickAddInventoryItem)

		self.LoadScript()

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	@ui.WindowDestroy
	def Destroy(self):
		self.ClearDictionary()

		self.savedShopConfiguration.clear()
		self.shopSignEditLine = None
		self.itemSlot = None
		self.btnOk = None
		self.btnClose = None
		self.earningsLabel = None
		self.timeSelect = None
		self.tooltip = None
		self.tooltipItem = None

		self.shopSlotRenderer.Destroy()
		self.shopSlotRenderer = None

		if self.questionDlg:
			self.questionDlg.Close()
			self.questionDlg = None

		self.__ClosePriceInputBoard()

	def LoadScript(self):
		try:
			PythonScriptLoader = ui.PythonScriptLoader()
			PythonScriptLoader.LoadScriptFile(self, "UIScript/PrivateShopBuilder.py")

			self.board = self.GetChild("board")
			self.shopSignEditLine = self.GetChild("ShopSignEditLine")
			# self.itemSlot = self.GetChild("ItemSlot")
			self.earningsLabel = self.GetChild("TotalEarningLabel")
			self.btnOk = self.GetChild("OkButton")
			self.btnClose = self.GetChild("CloseButton")
			self.timeSelect = self.GetChild("TimeSelectBar")
			self.titleBar = self.GetChild("TitleBar")
			self.taxMark = self.GetChild("QuestionMark")

			self.btnOk.SetEvent(ui.__mem_func__(self.OnOk))
			self.btnClose.SetEvent(ui.__mem_func__(self.OnClose))
			self.titleBar.SetCloseEvent(ui.__mem_func__(self.OnClose))

			self.shopSlotRenderer = offlineshopui.MyShopSlots(bindInternalEvents=False, itemSlotParent=self.board)

			self.itemSlot = self.shopSlotRenderer.itemSlot
			self.itemSlot.SetPosition(12, 34+26)
			self.itemSlot.checkSlotEvent = ui.__mem_func__(self.CanPlaceOnShopSlot)

			self.itemSlot.SetSelectEmptySlotEvent(ui.__mem_func__(self.OnSelectEmptySlot))
			self.itemSlot.SetSelectItemSlotEvent(ui.__mem_func__(self.OnSelectItemSlot))
			self.itemSlot.SetOverInItemEvent(ui.__mem_func__(self.OnOverInItem))
			self.itemSlot.SetOverOutItemEvent(ui.__mem_func__(self.OnOverOutItem))

			self.taxMark.SAFE_SetEvent("MOUSE_OVER_IN", self.ShowTaxTooltip)
			self.taxMark.SAFE_SetEvent("MOUSE_OVER_OUT", self.HideTooltip)

			self.timeSelect.SAFE_SetEvent(self.OnTimeSelect)
			PrepareComboBox(self.timeSelect)


		except:
			import exception
			exception.Abort("OfflineShopBuilder.LoadDialog.BindObject")

	def OnTimeSelect(self, index):
		data = SHOP_TIME_OPTIONS[index]
		isPremiumOption = data.has_key("isSpecial") and data["isSpecial"]

		# if isPremiumOption and not offlineshopui.HasShopPremium():
		# 	chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.SHOP_NO_PREMIUM)
		# 	self.ClearSelection()
		# 	self.SetTax(0)
		# 	return

		self.timeSelect.SetCurrentItem(GetTimeSelectString(index), SPECIAL_ITEM_COLOR if isPremiumOption else None)
		self.selectedTimeOption = index

		if index == 0: # normal shop
			self.SetTax(0)
		else:
			self.SetTax(self.serverTax)

		self.RefreshEarningsLabel()

	def RefreshEarningsLabel(self):
		self.earningsLabel.SetText(localeInfo.NumberToMoneyString(self.GetTotalGold()))

	def AddTotalGold(self, gold):
		self.totalGold += gold
		self.RefreshEarningsLabel()

		if self.totalGold > player.GOLD_MAX:
			self.earningsLabel.SetPackedFontColor(colorinfo.NEGATIVE_COLOR)
		else:
			self.earningsLabel.SetPackedFontColor(colorinfo.FONT_COLOR)

	def GetTotalGold(self):
		if self.tax > 0:
			return self.totalGold - self.GetTaxValue()
		else:
			return self.totalGold

	def GetTaxValue(self):
		return self.totalGold * self.tax/100

	def Open(self, tax):
		self.serverTax = tax
		self.SetTax(0)
		self.AddTotalGold(-self.totalGold)
		self.itemStock = {}
		shop.ClearPrivateShopStock()
		self.SetCenterPosition()
		self.Refresh()
		self.Show()

		if len(self.savedShopConfiguration) < 1:
			self.__LoadShopConfigurationFromFile()
		else:
			self.OpenLoadConfigurationQuestion()

		global g_isBuildingPrivateShop
		g_isBuildingPrivateShop = True

		eventManager.EventManager().send_event(eventManager.OPEN_PRIVATE_SHOP_BUILDER)

	def Close(self):
		global g_isBuildingPrivateShop
		g_isBuildingPrivateShop = False
		SaveItemPriceDictFile()

		self.itemStock = {}
		self.totalGold = 0
		self.ClearSelection()
		self.shopSignEditLine.Clear()
		shop.ClearPrivateShopStock()
		self.Hide()

		if self.questionDlg:
			self.questionDlg.Close()
			self.questionDlg = None

		self.__ClosePriceInputBoard()

		eventManager.EventManager().send_event(eventManager.CLOSE_PRIVATE_SHOP_BUILDER)

	def ClearSelection(self):
		self.selectedTimeOption = -1
		self.timeSelect.listBox.ClearSelection()
		self.timeSelect.SetCurrentItem(uiScriptLocale.SHOP_CREATE_SELECT_DEFAULT)

	def SetTax(self, value):
		self.tax = value
		if value > 0:
			self.taxMark.Show()
		else:
			self.taxMark.Hide()

	def ShowTaxTooltip(self):
		if self.tooltip:
			self.tooltip.ClearToolTip()
			self.tooltip.AppendDescription(localeInfo.SHOP_TAX_INFO1)
			self.tooltip.AppendSpace(5)
			self.tooltip.AppendTextLine("%d%%" % self.tax, colorinfo.POSITIVE_COLOR)
			self.tooltip.AppendSpace(5)
			self.tooltip.AppendDescription(localeInfo.SHOP_TAX_INFO2 % localeInfo.NumberToMoneyString(self.GetTaxValue()))
			self.tooltip.ShowToolTip()

	def HideTooltip(self):
		if self.tooltip:
			self.tooltip.HideToolTip()

	def Refresh(self):
		getitemVNum=player.GetItemIndex
		getItemCount=player.GetItemCount
		setitemVNum=self.itemSlot.SetItemSlot
		delItem=self.itemSlot.ClearSlot
		for i in xrange(shop.SHOP_SLOT_COUNT):

			if not self.itemStock.has_key(i):
				delItem(i)
				continue

			pos = self.itemStock[i][1]

			itemCount = getItemCount(pos)
			if itemCount <= 1:
				itemCount = 0

			socket = tuple(player.GetItemMetinSocket(pos, j) for j in range(player.METIN_SOCKET_MAX_NUM))

			setitemVNum(i, getitemVNum(pos), itemCount, socket=socket)

		self.itemSlot.RefreshSlot()

	def __CheckItem(self, itemVnum):
		item.SelectItem(itemVnum)

		if item.IsAntiFlag(item.ANTIFLAG_GIVE) or item.IsAntiFlag(item.ANTIFLAG_MYSHOP):
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.PRIVATE_SHOP_CANNOT_SELL_ITEM)
			return False

		return True

	def OnSelectEmptySlot(self, selectedSlotPos):

		isAttached = mouseModule.mouseController.isAttached()
		if isAttached:
			attachedSlotType = mouseModule.mouseController.GetAttachedType()
			attachedSlotPos = mouseModule.mouseController.GetAttachedSlotNumber()
			mouseModule.mouseController.DeattachObject()

			if player.SLOT_TYPE_INVENTORY != attachedSlotType and player.SLOT_TYPE_DRAGON_SOUL_INVENTORY != attachedSlotType:
				return
			attachedInvenType = player.SlotTypeToInvenType(attachedSlotType)

			self.CreatePriceInput(attachedInvenType, attachedSlotPos, selectedSlotPos)

	def OnQuickAddInventoryItem(self, type, slotNumber):
		if type != "myshop":
			return

		itemVnum = player.GetItemIndex(slotNumber)
		if itemVnum > 0:
			item.SelectItem(itemVnum)
			(itemWidth, itemHeight) = item.GetItemSize()

			freeSlot = self.itemSlot.FindEmptySlot(itemHeight)
			if freeSlot >= 0:
				self.CreatePriceInput(player.INVENTORY, slotNumber, freeSlot)

	def CanPlaceOnShopSlot(self, destSlotPos, itemHeight):
		return offlineshopui.CanPlaceItemOnShopSlot(destSlotPos, itemHeight,
													special_flags.GetFlag(special_flags.SHOP_SLOT_UNLOCK_PROGRESS_FLAG),
													offlineshopui.HasShopPremium())

	def CreatePriceInput(self, window_type, sourceSlotPos, destSlotPos):
		itemVNum = player.GetItemIndex(window_type, sourceSlotPos)
		count = player.GetItemCount(window_type, sourceSlotPos)
		sockets = tuple(player.GetItemMetinSocket(window_type, sourceSlotPos, i) for i in xrange(player.METIN_SOCKET_MAX_NUM))

		if not self.__CheckItem(itemVNum):
			return

		(width, height) = item.GetItemSize()
		if not self.CanPlaceOnShopSlot(destSlotPos, height):
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.SHOP_PLACE_ITEM_ON_LOCKED_SLOT)
			return

		if self.questionDlg:
			self.__ClearShopConfiguration()
			self.questionDlg.Close()
			self.questionDlg = None

		# The price window of the item before goes, its Ceny window with it.
		self.__ClosePriceInputBoard()

		priceInputBoard = uiCommon.NewMoneyInputDialog()
		priceInputBoard.SetTitle(localeInfo.PRIVATE_SHOP_INPUT_PRICE_DIALOG_TITLE)
		priceInputBoard.SetAcceptEvent(ui.__mem_func__(self.AcceptInputPrice))
		priceInputBoard.SetCancelEvent(ui.__mem_func__(self.CancelInputPrice))
		priceInputBoard.SetMaxLength(10)
		priceInputBoard.Open()

		itemPrice = GetPrivateShopItemPrice(itemVNum, count, sockets)

		if itemPrice > 0:
			priceInputBoard.SetValue(itemPrice)

		self.priceInputBoard = priceInputBoard
		self.priceInputBoard.itemVNum = itemVNum
		self.priceInputBoard.sourceWindowType = window_type
		self.priceInputBoard.sourceSlotPos = sourceSlotPos
		self.priceInputBoard.targetSlotPos = destSlotPos
		# MT2009_PLUS_SHOP_AUTO_PRICE_V1: the Dom Towarowy's hint and the
		# automatic price, as the open shop's counter has them.
		shopautoprice.RequestDialog(self.priceInputBoard, window_type, sourceSlotPos)

	def __ClosePriceInputBoard(self):
		if self.priceInputBoard:
			shopautoprice.CancelDialog(self.priceInputBoard)
			self.priceInputBoard.Close()
			self.priceInputBoard = None

	def OnSelectItemSlot(self, selectedSlotPos):

		isAttached = mouseModule.mouseController.isAttached()
		if isAttached:
			snd.PlaySound("sound/ui/loginfail.wav")
			mouseModule.mouseController.DeattachObject()

		else:
			if not selectedSlotPos in self.itemStock:
				return

			invenType, invenPos, price = self.itemStock[selectedSlotPos]
			shop.DelPrivateShopItemStock(invenType, invenPos)
			snd.PlaySound("sound/ui/drop.wav")

			eventManager.EventManager().send_event(eventManager.REMOVE_ITEM_FROM_SHOP_BUILDER, invenPos, invenType)
			self.__DelShopConfiguration(selectedSlotPos)

			self.AddTotalGold(-price)
			del self.itemStock[selectedSlotPos]

			self.Refresh()

	def AcceptInputPrice(self):
		if not self.priceInputBoard:
			return True

		text = self.priceInputBoard.GetText()

		if not text:
			return True

		inputPrice = localeInfo.MoneyStringToNumber(self.priceInputBoard.GetText())
		if inputPrice <= 0:
			return True	# MT2009_PLUS_VEKIRION_V1: Enter handled, no chat behind the window

		# MT2009_PLUS_UPSTREAM_2_0_76: the server sells for less than GOLD_MAX only.
		if inputPrice >= player.GOLD_MAX:
			inputPrice = player.GOLD_MAX - 1

		attachedInvenType = self.priceInputBoard.sourceWindowType
		sourceSlotPos = self.priceInputBoard.sourceSlotPos
		targetSlotPos = self.priceInputBoard.targetSlotPos

		for privatePos, (itemWindowType, itemSlotIndex, price) in self.itemStock.items():
			if itemWindowType == attachedInvenType and itemSlotIndex == sourceSlotPos:
				shop.DelPrivateShopItemStock(itemWindowType, itemSlotIndex)
				del self.itemStock[privatePos]
				self.AddTotalGold(-price)

		price = inputPrice
		count = player.GetItemCount(attachedInvenType, sourceSlotPos)
		vnum = player.GetItemIndex(attachedInvenType, sourceSlotPos)
		sockets = tuple(player.GetItemMetinSocket(attachedInvenType, sourceSlotPos, i) for i in xrange(player.METIN_SOCKET_MAX_NUM))

		SetPrivateShopItemPrice(self.priceInputBoard.itemVNum, count, price, sockets)
		eventManager.EventManager().send_event(eventManager.ADD_ITEM_TO_SHOP_BUILDER, sourceSlotPos, attachedInvenType)
		self.__AddItemToPrivateShop(targetSlotPos, sourceSlotPos, attachedInvenType, price)
		self.__AddShopConfiguration(targetSlotPos, sourceSlotPos, price)
		snd.PlaySound("sound/ui/drop.wav")

		self.__ClosePriceInputBoard()
		return True

	def __AddItemToPrivateShop(self, targetSlotPos, sourceSlotPos, sourceInvenType, price):
		itemVNum = player.GetItemIndex(sourceInvenType, sourceSlotPos)
		if itemVNum < 1:
			return

		if not self.__CheckItem(itemVNum):
			return

		self.AddTotalGold(price)
		eventManager.EventManager().send_event(eventManager.ADD_ITEM_TO_SHOP_BUILDER, sourceSlotPos, sourceInvenType)
		self.itemStock[targetSlotPos] = (sourceInvenType, sourceSlotPos, price)
		shop.AddPrivateShopItemStock(sourceInvenType,  sourceSlotPos, targetSlotPos, price)
		self.Refresh()

	def CancelInputPrice(self):
		self.__ClosePriceInputBoard()
		return True

	def CanCreateShop(self):
		if self.shopSignEditLine.GetText() == "":
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.SHOP_ERROR_EMPTY_SIGN)
			return False

		if net.IsInsultIn(self.shopSignEditLine.GetText()):
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.NAME_PROFANE_WORDS)
			return False

		if 0 == len(self.itemStock):
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.SHOP_ERROR_EMPTY_STOCK)
			return False

		if self.selectedTimeOption < 0:
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.SHOP_ERROR_SELECT_TIME)
			return False

		if self.GetTotalGold() > player.GOLD_MAX:
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.SHOP_ERROR_TOO_MUCH_GOLD)
			return False

		return True

	def OnOk(self):
		if not self.CanCreateShop():
			return

		shop.BuildPrivateShop(self.shopSignEditLine.GetText(), self.selectedTimeOption)
		self.Close()

	def OnClose(self):
		self.Close()

	def OnPressEscapeKey(self):
		self.Close()
		return True

	def SetToolTip(self, tooltip, tooltipItem):
		self.tooltip = tooltip
		self.tooltipItem = tooltipItem

	def OnOverInItem(self, slotIndex):
		if self.tooltipItem:
			if self.itemStock.has_key(slotIndex):
				(windowType, itemPos, price) = self.itemStock[slotIndex]
				self.tooltipItem.SetPrivateShopBuilderItem(windowType, itemPos, slotIndex)

	def OnOverOutItem(self):
		if self.tooltipItem:
			self.tooltipItem.HideToolTip()

	def __AddShopConfiguration(self, targetSlotPos, sourceSlotPos, price):
		self.savedShopConfiguration[str(targetSlotPos)] = (sourceSlotPos, price)
		self.__SaveShopConfigurationToFile()

	def __DelShopConfiguration(self, shopSlotPos):
		self.savedShopConfiguration.pop(str(shopSlotPos))
		self.__SaveShopConfigurationToFile()

	def __SaveShopConfigurationToFile(self):
		playerName = player.GetName()
		file_utils.WriteFile("cache/shop_configuration.json", json.dumps({playerName: self.savedShopConfiguration}))

	def __LoadShopConfigurationFromFile(self):
		stringData = file_utils.GetFileContent("cache/shop_configuration.json")
		if stringData != "":
			loadedConfiguration = json.loads(stringData)

			playerName = player.GetName()
			if loadedConfiguration.has_key(playerName):
				self.savedShopConfiguration = loadedConfiguration[playerName]

			if len(self.savedShopConfiguration):
				self.OpenLoadConfigurationQuestion()

	def __LoadShopConfigurationToShopBuilder(self):
		if len(self.savedShopConfiguration) > 0 and len(self.itemStock) == 0:
			for targetSlotPos, data in self.savedShopConfiguration.items():
				sourceSlotPos = data[0]
				price = data[1]
				self.__AddItemToPrivateShop(int(targetSlotPos), sourceSlotPos, player.INVENTORY, price)
			return True
		return False

	def __ClearShopConfiguration(self):
		self.savedShopConfiguration.clear()
		self.__SaveShopConfigurationToFile()

	def OpenLoadConfigurationQuestion(self):
		dlg = uiCommon.QuestionDialog2()
		dlg.SetText1(localeInfo.SHOP_LOAD_CONFIG_QUESTION1_1)
		dlg.SetText2(localeInfo.SHOP_LOAD_CONFIG_QUESTION1_2)
		dlg.SAFE_SetAcceptEvent(self.OpenLoadConfigurationQuestion2)
		dlg.SAFE_SetCancelEvent(self.CloseLoadConfigurationQuestion)
		dlg.SetWidth(340)
		dlg.Open()
		self.questionDlg = dlg

	def OpenLoadConfigurationQuestion2(self):
		self.questionDlg.SetText1(localeInfo.SHOP_LOAD_CONFIG_QUESTION2_1)
		self.questionDlg.SetText2(localeInfo.SHOP_LOAD_CONFIG_QUESTION2_2)
		self.questionDlg.SAFE_SetAcceptEvent(self.AcceptLoadConfigurationQuestion)

	def AcceptLoadConfigurationQuestion(self):
		if self.questionDlg:
			self.__LoadShopConfigurationToShopBuilder()
			self.questionDlg.Close()
			self.questionDlg = None

	def CloseLoadConfigurationQuestion(self):
		if self.questionDlg:
			self.__ClearShopConfiguration()
			self.questionDlg.Close()
			self.questionDlg = None
