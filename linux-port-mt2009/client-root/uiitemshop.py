import eventManager
import itemshop_subscriptionwindow
import itemshop_usevoucher
import ui
import app
import item
import flamewindPath
import localeInfo
import itemshop_shopwindow
import colorInfo
import constInfo
import net
import chat
import uiCommon
import utils
import offlineshopsearch

EVENT_OPEN_ITEMSHOP = "EVENT_OPEN_ITEMSHOP"  # args |
EVENT_OPEN_CATEGORY = "EVENT_OPEN_CATEGORY"  # args | categoryIndex: number
EVENT_BUY_ITEM = "EVENT_BUY_ITEM"  # args | itemIndex: number

CURRENCY_DRAGON_COIN = 1
CURRENCY_DRAGON_MARK = 2

ITEMSHOP_SORT_TYPE_VNUM = 0
ITEMSHOP_SORT_TYPE_PRICE = 1

def GetCurrencyShortName(currency):
	if currency == CURRENCY_DRAGON_COIN:
		return "SM"
	else:
		return "SZ"

ITEMSHOP_CATEGORY_FEATURED_INDEX = 10
ITEMSHOP_CATEGORY_TIMED_INDEX = 11
ITEMSHOP_CATEGORY_MARK_INDEX = 6
ITEMSHOP_CATEGORY_DRAGON_VOUCHER_INDEX = 15
ITEMSHOP_CATEGORY_USE_VOUCHER = 16

ITEMSHOP_CATEGORIES_SORT = [
	ITEMSHOP_CATEGORY_FEATURED_INDEX,
	0,
	14,
	1,
	2,
	5,
	6,
	ITEMSHOP_CATEGORY_DRAGON_VOUCHER_INDEX,
	ITEMSHOP_CATEGORY_USE_VOUCHER,
	ITEMSHOP_CATEGORY_TIMED_INDEX,
	12,
	13
]



ITEMSHOP_CATEGORIES = {
	0: {
		"name": localeInfo.ITEMSHOP_CATEGORY_EQUIPMENT,
		"action" : {
			"type" : "items",
			"items": [],
			"range": "1-99",
		},
	},

	1: {
		"name": localeInfo.ITEMSHOP_CATEGORY_VIP,
		"action" : {
			"type" : "items",
			"items": [999, 101,102,103,104,105,106,107,108,109,110,111,112,113],
		},
	},

	2: {
		"name": localeInfo.ITEMSHOP_CATEGORY_HAIR,
		"action" : {
			"type" : "items",
			"items" : [],
			"range": "301-450",
		},
	},

	3: {
		"name": localeInfo.ITEMSHOP_CATEGORY_MARRIAGE,
		"action" : {
			"type" : "items",
			"items" : [],
			"range": "201-299",
		},
	},

	# 4: {
	# 	"name": localeInfo.ITEMSHOP_CATEGORY_BOOSTERS,
	# 	"action" : {
	# 		"type" : "items",
	# 		"items": [],
	# 		"range": "501-599",
	# 	},
	# },

	5: {
		"name": localeInfo.ITEMSHOP_CATEGORY_SCROLLS_AND_BOOKS,
		"action" : {
			"type" : "items",
			"items": [],
			"range": "601-699",
		},
	},

	6: {
		"name": localeInfo.ITEMSHOP_CATEGORY_DRAGON_MARK,
		"action" : {
			"type" : "items",
			"items": []
		},
	},

	ITEMSHOP_CATEGORY_FEATURED_INDEX: {
		"name": localeInfo.ITEMSHOP_CATEGORY_FEATURED,
		"color": colorInfo.DRAGON_COIN_COLOR,
		"action" : {
			"type" : "items",
			"items": []
		},
	},


	ITEMSHOP_CATEGORY_TIMED_INDEX: {
		"name": localeInfo.ITEMSHOP_CATEGORY_TIMED,
		"color": colorInfo.ITEMSHOP_TIMED_CATEGORY_COLOR,
		"action" : {
			"type" : "items",
			"items": []
		},
	},

	12: {
		"name": localeInfo.ITEMSHOP_CATEGORY_SUBSCRIPTION,
		"color": colorInfo.DRAGON_COIN_COLOR,
		"action" : {
			"type" : "open_content",
			"value" : "subscription"
		},
	},

	13: {
		"name": localeInfo.ITEMSHOP_CATEGORY_ADD_COINS,
		"color": colorInfo.DRAGON_COIN_COLOR,
		"action" : {
			"type" : "open_url",
			"value" : "https://buycoffee.to/metin2-playerbots"
		},
	},

	14: {
		"name": localeInfo.ITEMSHOP_CATEGORY_REFINE,
		"action" : {
			"type" : "items",
			"items" : [],
			"range": "451-459",
		},
	},

	ITEMSHOP_CATEGORY_DRAGON_VOUCHER_INDEX: {
		"name": localeInfo.ITEMSHOP_CATEGORY_DRAGON_VOUCHERS,
		"action" : {
			"type" : "items",
			"items" : [996],
			"range": "901-905"
		},
	},

	996: {
		"action": {
			"type" : "search_item",
		}
	},

	ITEMSHOP_CATEGORY_USE_VOUCHER: {
		"name": localeInfo.ITEMSHOP_CATEGORY_USE_VOUCHER,
		"action" : {
			"type": "open_content",
			"value": "use_voucher",
		}
	}
}

ITEMSHOP_CUSTOM_ITEM_DATA = {
	101: {"itemName" : "$ITEM_NAME (1 " + localeInfo.ONE_DAY.lower() + ")"},
	102: {"itemName" : "$ITEM_NAME (3 " + localeInfo.DAY.lower() + ")"},
	103: {"itemName" : "$ITEM_NAME (1 " + localeInfo.ONE_DAY.lower() + ")"},
	104: {"itemName" : "$ITEM_NAME (3 " + localeInfo.DAY.lower() + ")"},
	105: {"itemName" : "$ITEM_NAME (1 " + localeInfo.ONE_DAY.lower() + ")"},
	106: {"itemName" : "$ITEM_NAME (3 " + localeInfo.DAY.lower() + ")"},
	107: {"itemName" : "$ITEM_NAME (1 " + localeInfo.ONE_DAY.lower() + ")"},
	108: {"itemName" : "$ITEM_NAME (3 " + localeInfo.DAY.lower() + ")"},
	109: {"itemName" : "$ITEM_NAME (1 " + localeInfo.ONE_DAY.lower() + ")"},
	110: {"itemName" : "$ITEM_NAME (3 " + localeInfo.DAY.lower() + ")"},
	111: {
		"itemName" : "$ITEM_NAME (30 dni)",
	},
	112: {
		"itemName" : "$ITEM_NAME (30 dni)",
	},

	608: {"itemName": "$ITEM_NAME (3+1)"},
	610: {"itemName": "$ITEM_NAME (Paczka 25)"},

	1: {"itemName": "$ITEM_NAME (30 godzin)"},
	5: {"itemName": "$ITEM_NAME (7 dni)"},

}

ITEMSHOP_BASE_DATA = {
	996: {
		"description": localeInfo.ITEMSHOP_SEARCH_DRAGON_VOUCHER_DESC,  # client only
		"itemName" : localeInfo.ITEMSHOP_SEARCH_DRAGON_VOUCHER_TITLE,
		"custom" : True,
		"icon" : "icon/item/search_item.tga",
		"buttonColor" : (76, 212, 112),
		"open_index" : 996,
		"itemPrice" : 0,
	},

	997: {
		"description": localeInfo.ITEMSHOP_TIME_EVENT_ITEM_DESCRIPTION,  # client only
		"itemName" : localeInfo.ITEMSHOP_CATEGORY_TIMED,
		"custom" : True,
		"icon" : "icon/item/auctions.tga",
		"buttonColor" : colorInfo.ITEMSHOP_TIMED_CATEGORY_COLOR,
		"open_index" : 11,
		"endTime" : 0,
		"itemPrice" : 0,
	},
	998: {
		"description": localeInfo.ITEMSHOP_HAPPYHOUR_ITEM_DESCRIPTION,  # client only
		"itemName" : localeInfo.ITEMSHOP_EVENT_HAPPYHOUR,
		"custom" : True,
		"icon" : "icon/item/happy_hour.tga",
		"open_index" : 13,
		"buttonColor" : colorInfo.ITEMSHOP_HAPPYHOUR_COLOR,
		"value" : 30,
		"endTime" : 0,
		"itemPrice" : 0,
	},
	999: {
		"description": localeInfo.ITEMSHOP_SUBSCRIPTION_ITEM_DESCRIPTION,  # client only
		"itemName" : localeInfo.ITEMSHOP_CATEGORY_SUBSCRIPTION,
		"custom" : True,
		"icon" : "icon/item/premium.tga",
		"buttonColor" : colorInfo.DRAGON_COIN_COLOR,
		"open_index" : 12,
		"itemPrice" : 0,
	}
}

def ResetItemShopData():
	constInfo.ITEMSHOP_DATA = ITEMSHOP_BASE_DATA

def search_similar(input):
	similar_indices = []
	input = input.lower()
	inputWords = input.split()
	for itemIdx,itemData in constInfo.ITEMSHOP_DATA.iteritems():
		if not itemData.has_key("itemVnum"):
			continue

		item.SelectItem(itemData["itemVnum"])
		itemName = item.GetItemName().lower()
		if len(inputWords) == 1:
			words = itemName.split()
			for word in words:
				if utils.check_string_similarity(input, word):
					similar_indices.append(itemIdx)
		else:
			if utils.check_string_similarity(input, itemName):
				similar_indices.append(itemIdx)
	return similar_indices


class ItemShopWindow(ui.ScriptWindow):
	class ItemShopQuestion:
		def __init__(self, parent):
			self.parent = parent
			self.buyQuestion = parent.GetChild("BuyQuestion")
			self.board = parent.GetChild("BuyQuestionBoard")
			self.itemIcon = parent.GetChild("ItemIcon")
			self.itemName = parent.GetChild("ItemName")
			self.itemCount = parent.GetChild("ItemCount")
			self.buy1 = parent.GetChild("Buy1")
			self.buy5 = parent.GetChild("Buy5")
			self.buy10 = parent.GetChild("Buy10")

			self.buy1.SAFE_SetEvent(self.__OnBuyButton, 1)
			self.buy5.SAFE_SetEvent(self.__OnBuyButton, 5)
			self.buy10.SAFE_SetEvent(self.__OnBuyButton, 10)

			self.buyQuestion.SAFE_SetStringEvent("MOUSE_LEFT_BUTTON_UP", self.Hide)

			self.questionDialog = None
			self.itemIndex = None
			eventManager.EventManager().add_observer(EVENT_BUY_ITEM, self.__OnBuyItemEvent)
			self.Hide()

		def Destroy(self):
			self.parent = None
			self.buyQuestion = None
			self.itemIcon = None
			self.itemName = None
			self.itemCount = None
			self.buy1 = None
			self.buy5 = None
			self.buy10 = None
			self.itemIndex = None

			if self.questionDialog:
				self.questionDialog.Close()
				self.questionDialog = None

			self.basePrice = None

		def GetPriceText(self, itemPrice, currency):
			color = ""
			if currency == CURRENCY_DRAGON_COIN:
				color = "ffc948"
			else:
				color = "cfa696"

			priceText = "|cff%s%d %s|r" % (color, itemPrice, GetCurrencyShortName(currency))
			return priceText


		def GetBuyButtonText(self, itemPrice, currency, itemCount, quantity):
			return localeInfo.ITEMSHOP_BUY_BUTTON % (quantity*itemCount, self.GetPriceText(itemPrice*quantity, currency))

		def SetButtons(self, itemPrice, currency, itemCount, maxStack):
			self.buy1.SetText(self.GetBuyButtonText(itemPrice, currency, itemCount, 1))

			if maxStack >= 5:
				self.buy5.SetText(self.GetBuyButtonText(itemPrice, currency, itemCount, 5))
				self.buy5.Show()
			else:
				self.buy5.Hide()

			if maxStack >= 10:
				self.buy10.SetText(self.GetBuyButtonText(itemPrice, currency, itemCount, 10))
				self.buy10.Show()
			else:
				self.buy10.Hide()

		def __OnBuyButton(self, quantity):
			if self.questionDialog:
				self.questionDialog.Close()

			self.board.Hide()
			dialog = uiCommon.QuestionDialog()
			dialog.SetParent(self.buyQuestion)

			itemData = constInfo.ITEMSHOP_DATA[self.itemIndex]
			itemName = itemData["itemName"]
			price = self.basePrice * quantity
			currency = itemData["currency"]
			itemCount = itemData["itemCount"]

			dialog.SetText(localeInfo.ITEMSHOP_CONFIM_BUY % (itemName, itemCount*quantity, self.GetPriceText(price, currency)))
			dialog.acceptButton.SAFE_SetEvent(self.__OnBuyItem, quantity)
			dialog.SAFE_SetCancelEvent(self.Hide)

			dialog.SetWindowHorizontalAlignCenter()
			dialog.SetWindowVerticalAlignCenter()
			dialog.Open()
			dialog.SetPosition(0, 0)
			self.questionDialog = dialog

		def __OnBuyItemEvent(self, itemIndex):
			self.itemIndex = itemIndex
			itemData = constInfo.ITEMSHOP_DATA[itemIndex]

			price = itemData["itemPrice"]
			if itemData.has_key("promotion"):
				current_time = app.GetGlobalTimeStamp()
				end_promotion_time = itemData["promotion"]["endTime"]
				if current_time < end_promotion_time:
					price = itemData["promotion"]["itemPrice"]

			self.basePrice = price
			self.Open(itemData["itemVnum"], itemData["itemName"], itemData["itemCount"], price, itemData["currency"])

		def __OnBuyItem(self, quantity):
			net.SendChatPacket("/itemshop buy %d %d" % (self.itemIndex, quantity))
			self.Hide()

		def Open(self, itemVnum, itemName, itemCount, price, currency):
			item.SelectItem(itemVnum)
			if item.IsAntiFlag(item.ANTIFLAG_STACK) or not item.IsFlag(item.ITEM_FLAG_STACKABLE):
				self.Show()
				self.__OnBuyButton(1)
				return

			self.itemName.SetText(itemName)
			self.itemCount.SetText("Sztuk: %d" % itemCount)
			item.SelectItem(itemVnum)
			self.itemIcon.LoadImage(item.GetIconImageFileName())
			self.SetButtons(price, currency, itemCount, item.GetItemMaxStack())
			self.Show()

		def Show(self):
			self.buyQuestion.Show()
			self.board.Show()

		def Hide(self):
			self.buyQuestion.Hide()

			if self.questionDialog:
				self.questionDialog.Close()

	def __init__(self):
		ui.ScriptWindow.__init__(self)
		self.tooltipItem = None
		self.tooltip = None
		self.shopContent = None
		self.subscriptionContent = None
		self.useVoucherContent = None
		self.currentContent = None
		self.categoryButtons = {"buttons": [], "titles": []}
		self.selectedCategoryIndex = -1
		self.happyHourValue = 0
		self.happyHourEndTime = 0
		self.timeAuctionEndTime = 0
		self.isLoaded = False
		self.LoadScript()

		eventManager.EventManager().add_observer(EVENT_OPEN_CATEGORY, self.__OnCategoryButtonClick)
		eventManager.EventManager().add_observer(EVENT_OPEN_ITEMSHOP, self.__OnOpenItemShop)

	def __del__(self):
		# import dbg
		# dbg.LogBox("uiItemShop")
		ui.ScriptWindow.__del__(self)

	def Destroy(self):
		self.ClearDictionary()
		self.ResetWindow()
		self.tooltipItem = None
		self.tooltip = None

		if self.itemShopQuestion:
			self.itemShopQuestion.Destroy()
			self.itemShopQuestion = None


	def ResetWindow(self):
		if self.shopContent:
			self.shopContent.Hide()
			self.shopContent.Destroy()
			self.shopContent = None

		if self.subscriptionContent:
			self.subscriptionContent.Hide()
			self.subscriptionContent.Destroy()
			self.subscriptionContent = None

		if self.useVoucherContent:
			self.useVoucherContent.Hide()
			self.useVoucherContent.Destroy()
			self.useVoucherContent = None

		if self.itemShopQuestion:
			self.itemShopQuestion.Hide()

		for btn in self.categoryButtons["buttons"]:
			btn.Hide()
		self.categoryButtons = {}

	def SetItemToolTip(self, tooltipItem):
		self.tooltipItem = tooltipItem

	def SetToolTip(self, tooltip):
		self.tooltip = tooltip

	def LoadScript(self):
		try:
			PythonScriptLoader = ui.PythonScriptLoader()
			PythonScriptLoader.LoadScriptFile(self, "UIScript/itemshopwindow.py")
			self.titleBar = self.GetChild("TitleBar")
			self.menuPanel = self.GetChild("MenuPanel")
			self.content = self.GetChild("Content")
			self.dragonCoinLabel = self.GetChild("DragonCoinAmount")
			self.dragonMarkLabel = self.GetChild("DragonMarkAmount")
			self.refreshButton = self.GetChild("RefreshButton")

			self.titleBar.SetCloseEvent(ui.__mem_func__(self.Close))
			self.refreshButton.SAFE_SetEvent(self.__OnRefreshButton)
			self.itemShopQuestion = self.ItemShopQuestion(self)
			self.__CreateCategories()

		except:
			import exception
			exception.Abort("ItemShopWindow.LoadDialog.BindObject")

	def SetBalance(self, dragon_coin, dragon_mark):
		self.dragonCoinLabel.SetText(localeInfo.NumberToString(dragon_coin))
		self.dragonMarkLabel.SetText(localeInfo.NumberToString(dragon_mark))
		if self.shopContent:
			self.shopContent.RefreshBuyButtons(dragon_coin, dragon_mark)

	def PrepareShop(self):
		self.__ResetCategoryItems(ITEMSHOP_CATEGORY_MARK_INDEX)
		self.__ResetCategoryItems(ITEMSHOP_CATEGORY_FEATURED_INDEX)
		self.__AddItemToCategory(ITEMSHOP_CATEGORY_FEATURED_INDEX, 999) # sub
		self.__AddItemToCategory(ITEMSHOP_CATEGORY_DRAGON_VOUCHER_INDEX, 996) # search dragon voucher
		self.SetHappyHour()
		self.SetTimeAuctionFeature()
		self.SetItems()

		self.shopContent = itemshop_shopwindow.ItemShopShopContent(self, self.tooltipItem, self.tooltip)
		self.subscriptionContent = itemshop_subscriptionwindow.ItemShopSubscriptionContent(self, self.tooltip)
		self.useVoucherContent = itemshop_usevoucher.ItemShopUseVoucher(self)

		for index,category in ITEMSHOP_CATEGORIES.iteritems():
			if category["action"]["type"] == "open_url":
				category_button_index = ITEMSHOP_CATEGORIES_SORT.index(index)
				self.categoryButtons["buttons"][category_button_index].SAFE_SetBetterToolTip(localeInfo.OPEN_URL_INFO, self.tooltip)

		self.__OnCategoryButtonClick(ITEMSHOP_CATEGORY_FEATURED_INDEX)


		self.shopContent.RefreshBuyButtons(int(self.dragonCoinLabel.GetText().replace(".", "")), int(self.dragonMarkLabel.GetText().replace(".", "")))
		self.isLoaded = True

	def SetTimedAuctions(self):
		self.__ResetCategoryItems(ITEMSHOP_CATEGORY_TIMED_INDEX)
		for key, item in constInfo.ITEMSHOP_DATA.iteritems():
			if item.has_key("timed"):
				self.__AddItemToCategory(ITEMSHOP_CATEGORY_TIMED_INDEX, key)

		self.__ShowCategoryItems(ITEMSHOP_CATEGORY_TIMED_INDEX)
		self.currentContent.Show()

	def SetItems(self):
		for key, item in constInfo.ITEMSHOP_DATA.iteritems():
			if item.has_key("promotion"):
				self.__AddItemToCategory(ITEMSHOP_CATEGORY_FEATURED_INDEX, key)

			if item.has_key("currency") and item["currency"] == CURRENCY_DRAGON_MARK:
				self.__AddItemToCategory(ITEMSHOP_CATEGORY_MARK_INDEX, key)

	def SetHappyHour(self):
		if self.happyHourValue > 0:
			current_time = app.GetGlobalTimeStamp()
			if current_time < self.happyHourEndTime:
				constInfo.ITEMSHOP_DATA[998]["endTime"] = self.happyHourEndTime
				constInfo.ITEMSHOP_DATA[998]["value"] = self.happyHourValue
				self.__AddItemToCategory(ITEMSHOP_CATEGORY_FEATURED_INDEX, 998)

	def SetTimeAuctionFeature(self):
		current_time = app.GetGlobalTimeStamp()
		if current_time < self.timeAuctionEndTime:
			constInfo.ITEMSHOP_DATA[997]["endTime"] = self.timeAuctionEndTime
			self.__AddItemToCategory(ITEMSHOP_CATEGORY_FEATURED_INDEX, 997)

	def __AddItemToCategory(self, categoryIndex, itemIndex):
		ITEMSHOP_CATEGORIES[categoryIndex]["action"]["items"].append(itemIndex)

	def __ResetCategoryItems(self, categoryIndex):
		ITEMSHOP_CATEGORIES[categoryIndex]["action"]["items"] = []

	def Open(self):
		self.Show()

		if self.selectedCategoryIndex == ITEMSHOP_CATEGORY_TIMED_INDEX:
			net.SendChatPacket("/itemshop auction_open")

	def Close(self):
		net.SendChatPacket("/itemshop close")
		self.Hide()

	def OnPressEscapeKey(self):
		self.Close()
		return True

	def __CreateCategories(self):
		for catIndex in ITEMSHOP_CATEGORIES_SORT:
			self.__CreateCategoryButton(catIndex)

			actionData = ITEMSHOP_CATEGORIES[catIndex]["action"]
			if actionData["type"] == "items":
				if actionData.has_key("range"):
					rangesplit = actionData["range"].split("-")
					self.__ResetCategoryItems(catIndex)
					for i in range(int(rangesplit[0]), int(rangesplit[1])+1):
						actionData["items"].append(i)


	def __CreateCategoryButton(self, catIndex):
		categoryData = ITEMSHOP_CATEGORIES[catIndex]
		category_button_start_position_y = 88
		category_button_height = 23
		category_button_step_y = category_button_height + 2.5

		btn = ui.Button()
		btn.SetParent(self.menuPanel)
		btn.SetUpVisual(flamewindPath.GetPublic("big_button_01"))
		btn.SetOverVisual(flamewindPath.GetPublic("big_button_02"))
		btn.SetDownVisual(flamewindPath.GetPublic("big_button_03"))
		btn.SetWindowHorizontalAlignCenter()

		addPos = 0
		if catIndex >= 11 and catIndex <= 13 or catIndex == ITEMSHOP_CATEGORY_USE_VOUCHER:
			addPos = category_button_height + 2

		btn.SetPosition(0, category_button_start_position_y + len(self.categoryButtons["buttons"]) * category_button_step_y + addPos)
		btn.SAFE_SetEvent(self.__OnCategoryButtonClick, catIndex)
		btn.Show()

		title = ui.TextLine()
		title.SetParent(btn)
		title.SetAllAlignCenter()
		title.SetBold()
		if categoryData.has_key("color"):
			title.SetPackedFontColor(colorInfo.GetColorFromColorTuple(categoryData["color"]))
		title.SetText(categoryData["name"])
		title.SetPosition(0, 0)
		title.Show()

		self.categoryButtons["buttons"].append(btn)
		self.categoryButtons["titles"].append(title)

	def __GetCategoryAction(self, categoryIndex):
		return ITEMSHOP_CATEGORIES[categoryIndex]["action"]

	def __GetCategoryActionType(self, categoryIndex):
		return self.__GetCategoryAction(categoryIndex)["type"]

	def __OnCategoryButtonClick(self, categoryIndex):
		action = self.__GetCategoryAction(categoryIndex)
		if action["type"] == "open_url":
			utils.open_url(action["value"])
			return
		elif categoryIndex == ITEMSHOP_CATEGORY_TIMED_INDEX:
			if self.timeAuctionEndTime == 0:
				chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.ITEMSHOP_NO_AUCTIONS)
				return
		elif categoryIndex == 996: # search dragon voucher
			import ikashop
			offlineshopsearch.search_category(ikashop.SHOP_SEARCH_CATEGORY_DRAGON_VOCHER)

		self.__SelectCategory(categoryIndex)

	def __SelectCategory(self, categoryIndex):
		if not categoryIndex in ITEMSHOP_CATEGORIES_SORT:
			print "ITEMSHOP_CATEGORIES_SORT NO INDEX[%d]" % categoryIndex
			return

		category_button_index = ITEMSHOP_CATEGORIES_SORT.index(categoryIndex)
		for btn in self.categoryButtons["buttons"]:
			btn.SetUp()
			btn.Enable()

		if self.selectedCategoryIndex == ITEMSHOP_CATEGORY_TIMED_INDEX:
			net.SendChatPacket("/itemshop auction_close")

		self.categoryButtons["buttons"][category_button_index].Down()
		self.categoryButtons["buttons"][category_button_index].Disable()
		self.selectedCategoryIndex = categoryIndex

		action = self.__GetCategoryAction(categoryIndex)
		if self.currentContent:
			self.currentContent.Hide()

		if self.selectedCategoryIndex == ITEMSHOP_CATEGORY_TIMED_INDEX:
			net.SendChatPacket("/itemshop auction_open")
			return

		actionType = action["type"]
		if actionType == "items":
			self.__ShowCategoryItems(categoryIndex)
		elif actionType == "open_content":
			self.__OpenContentByValue(action["value"])

		self.currentContent.Show()

	def __OpenContentByValue(self, value):
		if value == "subscription":
			self.currentContent = self.subscriptionContent
		elif value == "use_voucher":
			self.currentContent = self.useVoucherContent

		self.currentContent.Show()

	def __ShowCategoryItems(self, categoryIndex):
		isDragonVouchersCategory = categoryIndex == ITEMSHOP_CATEGORY_DRAGON_VOUCHER_INDEX
		self.shopContent.ShowItemList(self.__GetCategoryAction(categoryIndex)["items"], ITEMSHOP_SORT_TYPE_PRICE if isDragonVouchersCategory else 0)
		self.currentContent = self.shopContent

	def ShowItemsByInput(self, input):
		itemList = []
		if len(input) > 0:
			itemList = search_similar(input)

		if len(itemList) > 0:
			self.shopContent.ShowItemList(itemList, ITEMSHOP_SORT_TYPE_PRICE)
		else:
			self.__SelectCategory(self.selectedCategoryIndex)

	def __OnOpenItemShop(self):
		if self.IsShow():
			self.Close()
			return

		net.SendChatPacket("/itemshop open %s" % ("false" if self.isLoaded else "true"))

	def __OnRefreshButton(self):
		net.SendChatPacket("/itemshop refresh")
