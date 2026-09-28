import ui
import offlineShopUi
import ikashop
import shop
import wndMgr
import app
import uiCommon
import localeInfo
import item
import eventManager
import player
import net
import chr
import constInfo

def IsPressingCTRL():
	return app.IsPressed(app.DIK_LCONTROL) or app.IsPressed(app.DIK_RCONTROL)

class OfflineShopGuest(ui.ScriptWindow):
	def __init__(self):
		ui.ScriptWindow.__init__(self)
		self.__Initialize()
		self.LoadScript()
		ikashop.SetSearchShopBoard(self)

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def __Initialize(self):
		self.tooltip = None
		self.tooltipItem = None
		self.questionDialog = None
		self.popupDialog = None
		self.isNormalShop = False
		self.shopInfo = None
		self.xShopStart = 0
		self.yShopStart = 0

	@ui.WindowDestroy
	def Destroy(self):
		self.__Initialize()
		ikashop.SetSearchShopBoard(None)

	def SetToolTip(self, tooltip, tooltipItem):
		self.tooltip = tooltip
		self.tooltipItem = tooltipItem

	def LoadScript(self):
		try:
			PythonScriptLoader = ui.PythonScriptLoader()
			PythonScriptLoader.LoadScriptFile(self, "UIScript/PrivateShopGuest.py")

			self.board = self.GetChild("board")
			self.titleLabel = self.GetChild("TitleName")
			self.titleBar = self.GetChild("TitleBar")
			self.timeBar = self.GetChild("TimeBar")
			self.timeLeftLabel = self.GetChild("TimeLeftLabel")
			self.whisperButton = self.GetChild("WhisperButton")
			self.offlineOwner = self.GetChild("OfflineOwner")
			self.closeNormalShopButton = self.GetChild("CloseNormalShopButton")

			self.titleBar.SetCloseEvent(ui.__mem_func__(self.Close))

			self.myShopSlotRenderer = offlineShopUi.ShopSlotRenderer(itemSlotParent=self.board)
			self.itemSlot = self.myShopSlotRenderer.itemSlot
			self.itemSlot.SetWindowHorizontalAlignCenter()
			self.itemSlot.SetPosition(0, 30)

			self.itemSlot.SetSlotStyle(wndMgr.SLOT_STYLE_NONE)
			self.itemSlot.SAFE_SetButtonEvent("LEFT", "EXIST", self.__SelectItemSlot)
			self.itemSlot.SAFE_SetButtonEvent("RIGHT", "EXIST", self.__SelectItemSlot)

			self.itemSlot.SetOverInItemEvent(ui.__mem_func__(self.__OverInItem))
			self.itemSlot.SetOverOutItemEvent(ui.__mem_func__(self.__OverOutItem))

			self.whisperButton.SAFE_SetEvent(self.__SendOwnerWhisper)
			self.closeNormalShopButton.SAFE_SetEvent(self.__AskCloseNormalShop)

			self.timeBar.SAFE_SetEvent("MOUSE_OVER_IN", self.__ShowTimeToolTip)
			self.timeBar.SAFE_SetEvent("MOUSE_OVER_OUT", self.__HideNormalToolTip)

		except:
			import exception
			exception.Abort("OfflineShopManage.LoadDialog.BindObject")

	def __ShowTimeToolTip(self):
		if self.tooltip:
			self.tooltip.ClearToolTip()
			self.tooltip.AppendTextLine(offlineShopUi.GetDurationTooltipText(self.shopInfo["duration"]))
			self.tooltip.ShowToolTip()

	def __HideNormalToolTip(self):
		if self.tooltip:
			self.tooltip.HideToolTip()

	def __SendOwnerWhisper(self):
		eventManager.EventManager().send_event(eventManager.OPEN_WHISPER_EVENT, self.shopInfo["ownerName"])

	def __GetItemData(self, slot):
		if slot in self.shopInfo['items']:
			return self.shopInfo['items'][slot]
		return None

	def __SelectItemSlot(self, slot):
		data = self.__GetItemData(slot)
		if data is None:
			return

		if IsPressingCTRL():
			self.__AskBuyAllItem(slot)
		else:
			self.__AskBuyItem(slot)

	def __GetSimilarItemData(self, itemData):
		itemSlotList = []
		totalCount = 0
		totalPrice = 0
		itemTuple = (itemData["vnum"], itemData["count"], itemData["price"])
		for slotIdx, data in self.shopInfo["items"].items():
			compareTuple = (data["vnum"], data["count"], data["price"])
			if itemTuple == compareTuple:
				itemSlotList.append(slotIdx)
				totalCount += data["count"]
				totalPrice += data["price"]

		return itemSlotList, totalCount, totalPrice

	def __AskCloseNormalShop(self):
		if self.questionDialog:
			self.questionDialog.Close()

		dialog = uiCommon.QuestionDialog()
		dialog.SetText(localeInfo.PRIVATE_SHOP_CLOSE_QUESTION)
		dialog.acceptButton.SAFE_SetEvent(self.__CloseNormalShop)
		dialog.SetDefaultCancelEvent()
		dialog.Open()
		self.questionDialog = dialog

	def __AskBuyItem(self, slot):
		data = self.__GetItemData(slot)
		if data is None:
			return

		if self.questionDialog:
			self.questionDialog.Close()

		item.SelectItem(data["vnum"])

		dialog = uiCommon.QuestionDialog()
		dialog.SetText(localeInfo.DO_YOU_BUY_ITEM2 % (str(data["count"]) + "x", item.GetItemName(), localeInfo.NumberToMoneyString(data["price"])))
		dialog.acceptButton.SAFE_SetEvent(self.__SendBuyItem, slot, False)
		dialog.closeEvent = lambda arg1=[slot]: self.DeactivateItems(arg1)
		dialog.SetDefaultCancelEvent()
		dialog.Open()
		self.questionDialog = dialog
		self.ActivateItems([slot])

	def __AskBuyAllItem(self, slot):
		data = self.__GetItemData(slot)
		if data is None:
			return

		if self.questionDialog:
			self.questionDialog.Close()

		itemSlotList, totalCount, totalPrice = self.__GetSimilarItemData(data)
		item.SelectItem(data["vnum"])

		dialog = uiCommon.QuestionDialog2()
		countString = "%dx (po %d)" % (totalCount, data["count"])
		dialog.SetText1(localeInfo.DO_YOU_BUY_ITEM2 % (countString, item.GetItemName(), localeInfo.NumberToMoneyString(totalPrice)))
		dialog.SetText2(localeInfo.DO_YOU_BUY_ITEM_ALL_SIMILAR)
		dialog.acceptButton.SAFE_SetEvent(self.__SendBuyItem, slot, True)
		dialog.closeEvent = lambda arg1=itemSlotList: self.DeactivateItems(arg1)
		dialog.SetDefaultCancelEvent()
		dialog.Open()
		self.questionDialog = dialog
		self.ActivateItems(itemSlotList)

	def __SendBuyItem(self, slot, all_similar=False):
		if self.questionDialog:
			self.questionDialog.Close()
			self.questionDialog = None

			data = self.__GetItemData(slot)
			if data is None:
				return

			if all_similar:
				for check_slot, check_item in self.shopInfo['items'].items():
					item1 = (data['vnum'], data['count'], data['price'])
					item2 = (check_item['vnum'], check_item['count'], check_item['price'])
					if item1 == item2:
						self.__SendBuyItemPacket(check_slot)
			else:
				self.__SendBuyItemPacket(slot)

	def __SendBuyItemPacket(self, slotPos):
		if not self.isNormalShop:
			itemData = self.__GetItemData(slotPos)
			ikashop.SendBuyItem(self.shopInfo['id'], itemData["id"], itemData["price"])
		else:
			net.SendShopBuyPacket(slotPos)

	def __OverInItem(self, slot):
		data = self.__GetItemData(slot)
		if data is None:
			return

		if self.tooltipItem:
			self.tooltipItem.ClearToolTip()
			self.tooltipItem.SetMyShopItem(data["vnum"], data["count"], data["sockets"], data["attrs"], data["price"])
			self.tooltipItem.AppendSpace(5)
			self.tooltipItem.AppendTextLine(localeInfo.QUICK_BUY_SHOP)
			self.tooltipItem.AppendTextLine(localeInfo.QUICK_BUY_ALL_SIMILAR)
			self.tooltipItem.ShowToolTip()

	def __OverOutItem(self):
		if self.tooltipItem:
			self.tooltipItem.HideToolTip()

	def __OpenPopup(self, message):
		if self.popupDialog:
			self.popupDialog.Close()

		dialog = uiCommon.PopupDialog()
		dialog.SetText(message)
		dialog.SetAutoClose(3)
		dialog.Open()
		self.popupDialog = dialog

	def RefreshItemDeposit(self):
		self.ClearItemStock()
		for slotIdx,data in self.shopInfo["items"].iteritems():
			count = data["count"]
			if count < 2:
				count = 0

			socket = tuple(data["sockets"])
			self.itemSlot.SetItemSlot(slotIdx, data["vnum"], count, socket=socket)


		self.itemSlot.RefreshSlot()

	def ClearItemStock(self):
		for x in range(shop.SHOP_PLAYER_WIDTH):
			for y in range(shop.SHOP_PLAYER_HEIGHT):
				self.itemSlot.ClearSlot(x + shop.SHOP_PLAYER_WIDTH * y)

	def SetTitle(self, ownerName):
		self.titleLabel.SetText(localeInfo.PLAYER_SHOP_TITLE % ownerName)

	def SetTime(self, timeIndex):
		if timeIndex < 0:
			self.timeBar.Hide()
		else:
			self.timeBar.Show()
			offlineShopUi.SetDuration(timeIndex, self.timeLeftLabel)

	def RefreshOwnerMessageButton(self, isOnline):
		if isOnline:
			self.offlineOwner.Hide()
			self.whisperButton.Show()
		else:
			self.offlineOwner.Show()
			self.whisperButton.Hide()

	def ActivateItems(self, itemSlots):
		for slot in itemSlots:
			if self.shopInfo["items"].has_key(slot):
				self.itemSlot.ActivateSlot(slot)
		self.itemSlot.RefreshSlot()

	def DeactivateItems(self, itemSlots):
		if self.shopInfo and self.shopInfo.has_key("items"):
			for slot in itemSlots:
				if self.shopInfo["items"].has_key(slot):
					self.itemSlot.DeactivateSlot(slot)
			self.itemSlot.RefreshSlot()

	def OnPressEscapeKey(self):
		self.Close()
		return True

	def Open(self):
		self.Show()
		self.SetTop()
		(self.xShopStart, self.yShopStart, z) = player.GetMainCharacterPosition()

	def Close(self):
		eventManager.EventManager().send_event(eventManager.EVENT_MARK_SHOP_VIEWED, self.shopInfo["vid"], not self.isNormalShop)
		if self.isNormalShop:
			shop.Close()
			net.SendShopEndPacket()
		else:
			ikashop.SendCloseShopGuestBoard()

		self.shopInfo = None
		self.Hide()
		self.isNormalShop = False

		if self.questionDialog:
			self.questionDialog.Close()
			self.questionDialog = None

	def __CloseNormalShop(self):
		if self.questionDialog:
			self.questionDialog.Close()
			self.questionDialog = None

		net.SendChatPacket("/close_shop")
		self.Close()
		return True

	def CloseNormal(self):
		if self.IsShow() and self.isNormalShop:
			self.Close()

	def RefreshNormal(self):
		if self.IsShow() and self.isNormalShop:
			self.__PrepareNormalShopInfo()
			self.RefreshItemDeposit()

	def __PrepareNormalShopInfo(self, vid=0):
		data = {"items": {}}

		if self.shopInfo and self.shopInfo.has_key("ownerName") and vid == 0:
			data["ownerName"] = self.shopInfo["ownerName"]
			data["vid"] = self.shopInfo["vid"]
		elif vid > 0:
			data["ownerName"] = chr.GetNameByVID(vid)
			data["vid"] = vid

		for slotPos in range(shop.SHOP_SLOT_COUNT):
			itemVnum = shop.GetItemID(slotPos)
			if itemVnum > 0:
				itemPrice = shop.GetItemPrice(slotPos)
				itemCount = shop.GetItemCount(slotPos)
				sockets = tuple(shop.GetItemMetinSocket(slotPos, i) for i in xrange(player.METIN_SOCKET_MAX_NUM))
				attrs = tuple(shop.GetItemAttribute(slotPos, i) for i in xrange(player.ATTRIBUTE_SLOT_MAX_NUM))
				data["items"][slotPos] = {	"vnum": itemVnum,
												"price": itemPrice,
												"count": itemCount,
												"sockets": sockets,
												"attrs": attrs,
												}
		self.shopInfo = data

	def SetShopHeight(self, isNormalShop, isOwner):
		if isNormalShop and not isOwner:
			self.SetHeight(298)
			self.board.SetHeight(298)
		else:
			self.SetHeight(328)
			self.board.SetHeight(328)

		self.RefreshPosition()

	def SetView(self, isNormalShop, isOwner=False):
		if not isNormalShop:
			self.whisperButton.Show()
			self.offlineOwner.Show()
			self.timeBar.Show()
			self.closeNormalShopButton.Hide()
		else:
			self.whisperButton.Hide()
			self.offlineOwner.Hide()
			self.timeBar.Hide()
			if isOwner:
				self.closeNormalShopButton.Show()
			else:
				self.closeNormalShopButton.Hide()

		self.SetShopHeight(isNormalShop, isOwner)

	def IsShowNormal(self):
		return self.isNormalShop and self.IsShow()

	def OpenNormal(self, vid, unlockCount, isPremium):
		isOwner = player.IsMainCharacterIndex(vid)
		shop.Open(True, isOwner)
		self.isNormalShop = True
		self.__PrepareNormalShopInfo(vid)
		self.SetTitle(self.shopInfo["ownerName"])
		self.RefreshItemDeposit()
		self.SetView(True, isOwner)
		self.myShopSlotRenderer.UpdateSlotCurtain(unlockCount, isPremium)
		self.Open()
		eventManager.EventManager().send_event(eventManager.EVENT_MARK_SHOP_CURRENT, self.shopInfo["vid"], False)

	def OnUpdate(self):
		USE_SHOP_LIMIT_RANGE = 1000
		(x, y, z) = player.GetMainCharacterPosition()
		if abs(x - self.xShopStart) > USE_SHOP_LIMIT_RANGE or abs(y - self.yShopStart) > USE_SHOP_LIMIT_RANGE:
			self.Close()


	def HighlightFoundItems(self):
		slotList = []
		for slotIdx, data in self.shopInfo["items"].items():
			if constInfo.OFFLINESHOP_LAST_SEARCHED_CATEGORY == ikashop.SHOP_SEARCH_CATEGORY_ARMOR:
				if constInfo.OFFLINESHOP_LAST_SEARCH_IS_ATTR and len(data["attrs"]) > 0 and data["attrs"][0][0] == 0:
					continue

				item.SelectItem(data["vnum"])
				if item.GetItemType() == item.ITEM_TYPE_ARMOR:
					if constInfo.OFFLINESHOP_LAST_SEARCHED_SUBCATEGORY == ikashop.SHOP_SEARCH_SUB_ARMOR_BODY:
						if item.GetItemSubType() == item.ARMOR_BODY:
							slotList.append(slotIdx)
					elif constInfo.OFFLINESHOP_LAST_SEARCHED_SUBCATEGORY == ikashop.SHOP_SEARCH_SUB_ARMOR_SHIELD:
						if item.GetItemSubType() == item.ARMOR_SHIELD:
							slotList.append(slotIdx)
					elif constInfo.OFFLINESHOP_LAST_SEARCHED_SUBCATEGORY == ikashop.SHOP_SEARCH_SUB_ARMOR_HEAD:
						if item.GetItemSubType() == item.ARMOR_HEAD:
							slotList.append(slotIdx)

			elif constInfo.OFFLINESHOP_LAST_SEARCHED_CATEGORY == ikashop.SHOP_SEARCH_CATEGORY_JEWELRY:
				if constInfo.OFFLINESHOP_LAST_SEARCH_IS_ATTR and len(data["attrs"]) > 0 and data["attrs"][0][0] == 0:
					continue

				item.SelectItem(data["vnum"])
				if item.GetItemType() == item.ITEM_TYPE_ARMOR:
					if constInfo.OFFLINESHOP_LAST_SEARCHED_SUBCATEGORY == ikashop.SHOP_SEARCH_SUB_JEWELRY_EAR:
						if item.GetItemSubType() == item.ARMOR_EAR:
							slotList.append(slotIdx)
					elif constInfo.OFFLINESHOP_LAST_SEARCHED_SUBCATEGORY == ikashop.SHOP_SEARCH_SUB_JEWELRY_NECK:
						if item.GetItemSubType() == item.ARMOR_NECK:
							slotList.append(slotIdx)
					elif constInfo.OFFLINESHOP_LAST_SEARCHED_SUBCATEGORY == ikashop.SHOP_SEARCH_SUB_JEWELRY_WRIST:
						if item.GetItemSubType() == item.ARMOR_WRIST:
							slotList.append(slotIdx)
					elif constInfo.OFFLINESHOP_LAST_SEARCHED_SUBCATEGORY == ikashop.SHOP_SEARCH_SUB_JEWELRY_BOOTS:
						if item.GetItemSubType() == item.ARMOR_FOOTS:
							slotList.append(slotIdx)

			elif constInfo.OFFLINESHOP_LAST_SEARCHED_CATEGORY == ikashop.SHOP_SEARCH_CATEGORY_WEAPON:
				if constInfo.OFFLINESHOP_LAST_SEARCH_IS_ATTR and len(data["attrs"]) > 0 and data["attrs"][0][0] == 0:
					continue

				item.SelectItem(data["vnum"])
				if item.GetItemType() == item.ITEM_TYPE_WEAPON:
					if constInfo.OFFLINESHOP_LAST_SEARCHED_SUBCATEGORY == ikashop.SHOP_SEARCH_SUB_WEAPON_ONEHAND:
						if item.GetItemSubType() == item.WEAPON_SWORD:
							slotList.append(slotIdx)
					elif constInfo.OFFLINESHOP_LAST_SEARCHED_SUBCATEGORY == ikashop.SHOP_SEARCH_SUB_WEAPON_TWOHAND:
						if item.GetItemSubType() == item.WEAPON_TWO_HANDED:
							slotList.append(slotIdx)
					elif constInfo.OFFLINESHOP_LAST_SEARCHED_SUBCATEGORY == ikashop.SHOP_SEARCH_SUB_WEAPON_DAGGER:
						if item.GetItemSubType() == item.WEAPON_DAGGER:
							slotList.append(slotIdx)
					elif constInfo.OFFLINESHOP_LAST_SEARCHED_SUBCATEGORY == ikashop.SHOP_SEARCH_SUB_WEAPON_BOW:
						if item.GetItemSubType() == item.WEAPON_BOW:
							slotList.append(slotIdx)
					elif constInfo.OFFLINESHOP_LAST_SEARCHED_SUBCATEGORY == ikashop.SHOP_SEARCH_SUB_WEAPON_BELL:
						if item.GetItemSubType() == item.WEAPON_BELL:
							slotList.append(slotIdx)
					elif constInfo.OFFLINESHOP_LAST_SEARCHED_SUBCATEGORY == ikashop.SHOP_SEARCH_SUB_WEAPON_FAN:
						if item.GetItemSubType() == item.WEAPON_FAN:
							slotList.append(slotIdx)
			else:
				for searchData in constInfo.OFFLINESHOP_LAST_SEARCHED_ITEMS:
					searchItemVnum = searchData[0]
					searchSocket0 = searchData[1]
					if data["vnum"] != searchItemVnum:
						continue

					if searchSocket0 > 0 and data["sockets"][0] != searchSocket0:
						continue

					slotList.append(slotIdx)

		self.ActivateItems(slotList)

	def SetWholeOwnerName(self, pid, name):
		try:
			pid = int(pid)
		except ValueError:
			return
		if not hasattr(self, "wholeOwnerNames"):
			self.wholeOwnerNames = {}
		self.wholeOwnerNames[pid] = name

	def __WholeOwnerName(self, data):
		shortName = data.get("ownerName", "")
		name = getattr(self, "wholeOwnerNames", {}).get(data.get("id", 0), "")
		if shortName and name.startswith(shortName):
			return name
		return shortName

	## BINARY CALLS
	def OpenShopGuest(self, data):
		data["ownerName"] = self.__WholeOwnerName(data)
		data['items'] = {item['cell']: item for item in data['items']}
		self.shopInfo = data
		self.SetView(False)
		self.SetTitle(data["ownerName"])
		self.SetTime(data["duration"])
		self.RefreshOwnerMessageButton(data["isOwnerOnline"])
		self.RefreshItemDeposit()
		self.Open()
		self.myShopSlotRenderer.UpdateSlotCurtain(data["unlockCount"], data["isPremium"])
		eventManager.EventManager().send_event(eventManager.EVENT_MARK_SHOP_CURRENT, self.shopInfo["vid"], True)

		if ikashop.IsFoundShopFromSearchItem(self.shopInfo["vid"]):
			self.HighlightFoundItems()

	def ShopExpiredGuesting(self, id):
		self.__OpenPopup(localeInfo.SHOP_EXPIRE_NOTIFY_GUEST)
		self.Close()

	def ShopGuestRemoveItem(self, itemid):
		for i, v in self.shopInfo["items"].iteritems():
			if v["id"] == itemid:
				self.itemSlot.ClearSlot(i)
				self.shopInfo["items"].pop(i)
				break

	def ShopGuestEditItem(self, itemid, price):
		for i, v in self.shopInfo["items"].iteritems():
			if v["id"] == itemid:
				self.shopInfo["items"][i]["price"] = price

				if self.questionDialog:
					self.questionDialog.Close()
					self.questionDialog = None

				self.__OpenPopup(localeInfo.SHOP_HAS_BEEN_EDITED)
				self.Close()
				break
