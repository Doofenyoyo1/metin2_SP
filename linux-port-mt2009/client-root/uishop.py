import net
import player
import item
import snd
import shop
import net
import wndMgr
import app
import chat

import ui
import uiCommon
import mouseModule
import localeInfo
import constInfo
from _weakref import proxy
import colorinfo

import eventManager

EVENT_OPEN_SHOP_DIALOG = "EVENT_OPEN_SHOP_DIALOG" # args |
EVENT_CLOSE_SHOP_DIALOG = "EVENT_CLOSE_SHOP_DIALOG" # args |

EVENT_OPEN_PRIVATE_SHOP = "EVENT_OPEN_PRIVATE_SHOP" # args | vid: number
EVENT_CLICK_PRIVATE_SHOP = "EVENT_CLICK_PRIVATE_SHOP" # args | vid: number, is_offline: boolean

#EVENT_START_MASS_SELL = "EVENT_START_MASS_SELL" # args |
EVENT_ADD_MASS_SELL = "EVENT_ADD_MASS_SELL" # args | sourceSlotPos: number, sourceWindowType: number
EVENT_STOP_MASS_SELL = "EVENT_STOP_MASS_SELL" # args |

###################################################################################################
## Shop
# MT2009_PLUS_NPC_CHEAP_MARK_V1: the cheap NPC goods' mark, unless switched off in
# "Opcje dodatkowe" (uiopcjedodatkowe.py, "npccheap_off").
def _CheapMarkOn():
	try:
		import uiopcjedodatkowe
		return not uiopcjedodatkowe.Settings().get("npccheap_off", False)
	except Exception:
		return True

class ShopDialog(ui.ScriptWindow):

	def __init__(self):
		ui.ScriptWindow.__init__(self)
		self.tooltipItem = 0
		self.xShopStart = 0
		self.yShopStart = 0
		self.vid = 0
		self.isPlayerShop = False
		self.questionDialog = None
		self.popup = None
		self.itemBuyQuestionDialog = None

		self.totalSellValue = 0
		self.massSellData = {}

		eventManager.EventManager().add_observer(EVENT_ADD_MASS_SELL, self.OnAddItemMassSell)

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def __GetRealIndex(self, i):
		return self.tabIdx * shop.SHOP_NPC_SLOT_COUNT + i

	def Refresh(self):
		getItemID=shop.GetItemID
		getItemCount=shop.GetItemCount
		setItemID=self.itemSlotWindow.SetItemSlot
		for i in xrange(shop.SHOP_NPC_SLOT_COUNT):
			idx = self.__GetRealIndex(i)
			itemCount = getItemCount(idx)
			if itemCount <= 1:
				itemCount = 0
			setItemID(i, getItemID(idx), itemCount)
			# MT2009_PLUS_DIGI_CLIENT_QOL_V1 (Autor: Digi Rasta): NPC goods under 500 Yang lit up
			# MT2009_PLUS_SHOP_CHEAP_MARK_V1: no longer at the NPC merchants - the cheap
			# offers are lit up in the offline shops instead (offlineshopguest.py).
			cheap = False
			if cheap:
				self.itemSlotWindow.ActivateSlot(i)
			else:
				self.itemSlotWindow.DeactivateSlot(i)
			self.__SetCheapMark(i, cheap)

		wndMgr.RefreshSlot(self.itemSlotWindow.GetWindowHandle())

	def __SetCheapMark(self, i, on):
		marks = self.__dict__.setdefault("cheapMarks", {})
		bar = marks.get(i)
		if not on:
			if bar:
				bar.Hide()
			return
		if not bar:
			bar = ui.Bar()
			bar.SetParent(self.itemSlotWindow)
			bar.AddFlag("not_pick")
			bar.SetPosition((i % 5) * 32 + 1, (i // 5) * 32 + 1)
			bar.SetSize(30, 30)
			bar.SetColor(0x4033ff33)
			marks[i] = bar
		bar.Show()

	def SetItemData(self, pos, itemID, itemCount, itemPrice):
		shop.SetItemData(pos, itemID, itemCount, itemPrice)

	def LoadDialog(self):
		try:
			PythonScriptLoader = ui.PythonScriptLoader()
			PythonScriptLoader.LoadScriptFile(self, "UIScript/shopdialog.py")
		except:
			import exception
			exception.Abort("ShopDialog.LoadDialog.LoadObject")

		smallTab1 = None
		smallTab2 = None
		smallTab3 = None
		middleTab1 = None
		middleTab2 = None

		try:
			GetObject = self.GetChild
			self.itemSlotWindow = GetObject("ItemSlot")
			self.board = GetObject("board")
			self.btnBuy = GetObject("BuyButton")
			self.btnSell = GetObject("SellButton")
			self.btnClose = GetObject("CloseButton")
			self.titleBar = GetObject("TitleBar")
			self.btnMassSell = GetObject("MassSellButton")
			self.valueBar = GetObject("ValueBar")
			self.valueLabel = GetObject("ValueLabel")

			middleTab1 = GetObject("MiddleTab1")
			middleTab2 = GetObject("MiddleTab2")
			smallTab1 = GetObject("SmallTab1")
			smallTab2 = GetObject("SmallTab2")
			smallTab3 = GetObject("SmallTab3")
		except:
			import exception
			exception.Abort("ShopDialog.LoadDialog.BindObject")

		self.itemSlotWindow.SetSlotStyle(wndMgr.SLOT_STYLE_NONE)
		self.itemSlotWindow.SAFE_SetButtonEvent("LEFT", "EMPTY", self.SelectEmptySlot)
		self.itemSlotWindow.SAFE_SetButtonEvent("LEFT", "EXIST", self.SelectItemSlot)
		self.itemSlotWindow.SAFE_SetButtonEvent("RIGHT", "EXIST", self.UnselectItemSlot)

		self.itemSlotWindow.SetOverInItemEvent(ui.__mem_func__(self.OverInItem))
		self.itemSlotWindow.SetOverOutItemEvent(ui.__mem_func__(self.OverOutItem))

		self.btnBuy.SetToggleUpEvent(ui.__mem_func__(self.CancelShopping))
		self.btnBuy.SetToggleDownEvent(ui.__mem_func__(self.OnBuy))

		self.btnSell.SetToggleUpEvent(ui.__mem_func__(self.CancelShopping))
		self.btnSell.SetToggleDownEvent(ui.__mem_func__(self.OnSell))

		self.btnClose.SetEvent(ui.__mem_func__(self.AskClosePrivateShop))

		self.titleBar.SetCloseEvent(ui.__mem_func__(self.Close))

		self.btnMassSell.SAFE_SetEvent(self.MassSell)

		self.smallRadioButtonGroup = ui.RadioButtonGroup.Create([[smallTab1, lambda argSelf=proxy(self): argSelf.OnClickTabButton(0), None], [smallTab2, lambda argSelf=proxy(self): argSelf.OnClickTabButton(1), None], [smallTab3, lambda argSelf=proxy(self): argSelf.OnClickTabButton(2), None]])
		self.middleRadioButtonGroup = ui.RadioButtonGroup.Create([[middleTab1, lambda argSelf=proxy(self): argSelf.OnClickTabButton(0), None], [middleTab2, lambda argSelf=proxy(self): argSelf.OnClickTabButton(1), None]])

		self.__HideMiddleTabs()
		self.__HideSmallTabs()

		self.tabIdx = 0
		self.coinType = shop.SHOP_COIN_TYPE_GOLD

		self.Refresh()

	def __ShowBuySellButton(self):
		self.btnBuy.Show()
		self.btnSell.Show()
		self.btnMassSell.Hide()

	def __ShowMiddleTabs(self):
		self.middleRadioButtonGroup.Show()

	def __ShowSmallTabs(self):
		self.smallRadioButtonGroup.Show()

	def __HideBuySellButton(self):
		self.btnBuy.Hide()
		self.btnSell.Hide()
		self.btnMassSell.Hide()

	def __HideMiddleTabs(self):
		if self.middleRadioButtonGroup:
			self.middleRadioButtonGroup.Hide()

	def __HideSmallTabs(self):
		if self.smallRadioButtonGroup:
			self.smallRadioButtonGroup.Hide()

	def __SetTabNames(self):
		if shop.GetTabCount() == 2:
			self.middleRadioButtonGroup.SetText(0, shop.GetTabName(0))
			self.middleRadioButtonGroup.SetText(1, shop.GetTabName(1))
		elif shop.GetTabCount() == 3:
			self.smallRadioButtonGroup.SetText(0, shop.GetTabName(0))
			self.smallRadioButtonGroup.SetText(1, shop.GetTabName(1))
			self.smallRadioButtonGroup.SetText(2, shop.GetTabName(2))

	@ui.WindowDestroy
	def Destroy(self):
		self.ClearDictionary()

		self.tooltipItem = 0
		self.itemSlotWindow = 0
		self.btnBuy = 0
		self.btnSell = 0
		self.btnClose = 0
		self.titleBar = 0
		self.questionDialog = None
		self.popup = None
		self.xShopStart = 0
		self.yShopStart = 0
		self.vid = 0
		self.isPlayerShop = False
		self.smallRadioButtonGroup = None
		self.middleRadioButtonGroup = None
		self.totalSellValue = 0
		self.massSellData = {}

	def Open(self, vid):
		self.isPlayerShop = False
		self.vid = vid
		isMainPlayerPrivateShop = False

		import chr
		if chr.IsNPC(vid):
			self.isPlayerShop = False
		else:
			self.isPlayerShop = True

		if player.IsMainCharacterIndex(vid):

			isMainPlayerPrivateShop = True

			self.btnBuy.Hide()
			self.btnSell.Hide()
			self.btnClose.Show()

		else:

			isMainPlayerPrivateShop = False
			self.btnBuy.Show()
			self.btnSell.Show()
			self.btnClose.Hide()

		self.HideValueBar()

		shop.Open(self.isPlayerShop, isMainPlayerPrivateShop)

		self.tabIdx = 0

		if self.isPlayerShop:
			self.__HideMiddleTabs()
			self.__HideSmallTabs()
		else:
			if shop.GetTabCount() == 1:
				self.__ShowBuySellButton()
				self.__HideMiddleTabs()
				self.__HideSmallTabs()
			elif shop.GetTabCount() == 2:
				self.__HideBuySellButton()
				self.__ShowMiddleTabs()
				self.__HideSmallTabs()
				self.__SetTabNames()
				self.middleRadioButtonGroup.OnClick(0)
			elif shop.GetTabCount() == 3:
				self.__HideBuySellButton()
				self.__HideMiddleTabs()
				self.__ShowSmallTabs()
				self.__SetTabNames()
				self.smallRadioButtonGroup.OnClick(0) # @fixme002 (middleRadioButtonGroup(1) -> smallRadioButtonGroup(0))

		self.Refresh()
		self.SetTop()

		self.Show()

		eventManager.EventManager().send_event(EVENT_OPEN_SHOP_DIALOG)

		(self.xShopStart, self.yShopStart, z) = player.GetMainCharacterPosition()

	def Close(self):
		if self.itemBuyQuestionDialog:
			self.itemBuyQuestionDialog.Close()
			self.itemBuyQuestionDialog = None
			constInfo.SET_ITEM_QUESTION_DIALOG_STATUS(0)
		if self.questionDialog:
			self.OnCloseQuestionDialog()
		shop.Close()
		net.SendShopEndPacket()
		self.CancelShopping()
		self.tooltipItem.HideToolTip()

		if self.isPlayerShop and self.vid != player.GetMainCharacterIndex():
			eventManager.EventManager().send_event(eventManager.EVENT_MARK_SHOP_VIEWED, self.vid, False)

		eventManager.EventManager().send_event(EVENT_CLOSE_SHOP_DIALOG)

		self.totalSellValue = 0
		self.massSellData = {}
		constInfo.SHOP_MASS_SELL = False

		self.Hide()

	def GetIndexFromSlotPos(self, slotPos):
		return self.tabIdx * shop.SHOP_NPC_SLOT_COUNT + slotPos

	def OnClickTabButton(self, idx):
		self.tabIdx = idx
		self.Refresh()

	def AskClosePrivateShop(self):
		questionDialog = uiCommon.QuestionDialog()
		questionDialog.SetText(localeInfo.PRIVATE_SHOP_CLOSE_QUESTION)
		questionDialog.SetAcceptEvent(ui.__mem_func__(self.OnClosePrivateShop))
		questionDialog.SetCancelEvent(ui.__mem_func__(self.OnCloseQuestionDialog))
		questionDialog.Open()
		self.questionDialog = questionDialog

		constInfo.SET_ITEM_QUESTION_DIALOG_STATUS(1)

		return True

	def OnClosePrivateShop(self):
		net.SendChatPacket("/close_shop")
		self.OnCloseQuestionDialog()
		return True

	def OnPressEscapeKey(self):
		self.Close()
		return True

	def OnPressExitKey(self):
		self.Close()
		return True

	def OnBuy(self):
		chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.SHOP_BUY_INFO)
		app.SetCursor(app.BUY)
		self.btnSell.SetUp()
		self.OnSellModeDisable()

	def OnSell(self):
		chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.SHOP_SELL_INFO)
		app.SetCursor(app.SELL)
		self.btnBuy.SetUp()
		self.OnSellMode()

	def CancelShopping(self):
		self.btnBuy.SetUp()
		self.btnSell.SetUp()
		self.OnSellModeDisable()
		app.SetCursor(app.NORMAL)

	def __OnClosePopupDialog(self):
		self.pop = None
		constInfo.SET_ITEM_QUESTION_DIALOG_STATUS(0)

	def SellAttachedItem(self):
		if shop.IsPrivateShop():
			mouseModule.mouseController.DeattachObject()
			return

		attachedSlotType = mouseModule.mouseController.GetAttachedType()
		attachedSlotPos = mouseModule.mouseController.GetAttachedSlotNumber()
		attachedCount = mouseModule.mouseController.GetAttachedItemCount()
		attachedItemIndex = mouseModule.mouseController.GetAttachedItemIndex()

		if player.SLOT_TYPE_INVENTORY == attachedSlotType or player.SLOT_TYPE_DRAGON_SOUL_INVENTORY == attachedSlotType:

			item.SelectItem(attachedItemIndex)

			if item.IsAntiFlag(item.ANTIFLAG_SELL):
				popup = uiCommon.PopupDialog()
				popup.SetText(localeInfo.SHOP_CANNOT_SELL_ITEM)
				popup.SetAcceptEvent(self.__OnClosePopupDialog)
				popup.Open()
				self.popup = popup
				return

			itemtype = player.INVENTORY

			if player.SLOT_TYPE_DRAGON_SOUL_INVENTORY == attachedSlotType:
				itemtype = player.DRAGON_SOUL_INVENTORY

			if player.IsValuableItem(itemtype, attachedSlotPos):

				itemPrice = item.GetISellItemPrice()

				if item.Is1GoldItem():
					itemPrice = attachedCount / itemPrice
				else:
					itemPrice = itemPrice * max(1, attachedCount)

				if not app.ENABLE_NO_SELL_PRICE_DIVIDED_BY_5:
					isGoldBar = attachedItemIndex >= 80003 and attachedItemIndex <= 80008

					if not isGoldBar:
						itemPrice /= 5

				itemName = item.GetItemName()

				questionDialog = uiCommon.QuestionDialog()
				questionDialog.SetText(localeInfo.DO_YOU_SELL_ITEM(itemName, attachedCount, itemPrice))

				questionDialog.SetAcceptEvent(lambda arg1=attachedSlotPos, arg2=attachedCount, arg3 = itemtype: self.OnSellItem(arg1, arg2, arg3))
				questionDialog.SetCancelEvent(ui.__mem_func__(self.OnCloseQuestionDialog))
				questionDialog.Open()
				self.questionDialog = questionDialog

				constInfo.SET_ITEM_QUESTION_DIALOG_STATUS(1)

			else:
				self.OnSellItem(attachedSlotPos, attachedCount, itemtype)

		else:
			snd.PlaySound("sound/ui/loginfail.wav")

		mouseModule.mouseController.DeattachObject()

	def OnSellItem(self, slotPos, count, itemtype):
		net.SendShopSellPacketNew(slotPos, count, itemtype)
		snd.PlaySound("sound/ui/money.wav")
		self.OnCloseQuestionDialog()

	def OnCloseQuestionDialog(self):
		if not self.questionDialog:
			return

		self.questionDialog.Close()
		self.questionDialog = None
		constInfo.SET_ITEM_QUESTION_DIALOG_STATUS(0)

	def SelectEmptySlot(self, selectedSlotPos):
		isAttached = mouseModule.mouseController.isAttached()
		if isAttached:
			self.SellAttachedItem()

	def UnselectItemSlot(self, selectedSlotPos):
		if constInfo.GET_ITEM_QUESTION_DIALOG_STATUS() == 1:
			return
		if shop.IsPrivateShop():
			self.AskBuyItem(selectedSlotPos)
		else:
			net.SendShopBuyPacket(self.__GetRealIndex(selectedSlotPos))

	def SelectItemSlot(self, selectedSlotPos):
		if constInfo.GET_ITEM_QUESTION_DIALOG_STATUS() == 1:
			return

		isAttached = mouseModule.mouseController.isAttached()
		selectedSlotPos = self.__GetRealIndex(selectedSlotPos)
		if isAttached:
			self.SellAttachedItem()
		else:
			if True == shop.IsMainPlayerPrivateShop():
				return

			curCursorNum = app.GetCursor()
			if app.BUY == curCursorNum:
				self.AskBuyItem(selectedSlotPos)

			elif app.SELL == curCursorNum:
				chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.SHOP_SELL_INFO)

			else:
				selectedItemID = shop.GetItemID(selectedSlotPos)
				itemCount = shop.GetItemCount(selectedSlotPos)

				type = player.SLOT_TYPE_SHOP
				if shop.IsPrivateShop():
					type = player.SLOT_TYPE_PRIVATE_SHOP

				mouseModule.mouseController.AttachObject(self, type, selectedSlotPos, selectedItemID, itemCount)
				mouseModule.mouseController.SetCallBack("INVENTORY", ui.__mem_func__(self.DropToInventory))
				snd.PlaySound("sound/ui/pick.wav")

	def DropToInventory(self):
		attachedSlotPos = mouseModule.mouseController.GetAttachedSlotNumber()
		self.AskBuyItem(attachedSlotPos)

	def AskBuyItem(self, slotPos):
		slotPos = self.__GetRealIndex(slotPos)

		itemIndex = shop.GetItemID(slotPos)
		itemPrice = shop.GetItemPrice(slotPos)
		itemCount = shop.GetItemCount(slotPos)
		if app.ENABLE_CHEQUE_SYSTEM:
			itemCheque = shop.GetItemCheque(slotPos)

		item.SelectItem(itemIndex)
		itemName = item.GetItemName()

		itemBuyQuestionDialog = uiCommon.QuestionDialog()
		if app.ENABLE_CHEQUE_SYSTEM:
			if itemCheque > 0 and itemPrice <=0:
				itemBuyQuestionDialog.SetText(localeInfo.DO_YOU_BUY_ITEM_CHEQUE_SIN_YANG(itemName, itemCount, localeInfo.NumberToCheque(itemCheque)))
			elif itemCheque <= 0 and itemPrice > 0:
				itemBuyQuestionDialog.SetText(localeInfo.DO_YOU_BUY_ITEM(itemName, itemCount, localeInfo.NumberToGoldNotText(itemPrice)))
			elif itemCheque > 0 and itemPrice > 0:
				itemBuyQuestionDialog.SetText(localeInfo.DO_YOU_BUY_ITEM_YANG_CHEQUE(itemName, itemCount, localeInfo.NumberToGoldNotText(itemPrice),localeInfo.NumberToGoldNotText(itemCheque) ))
		else:
			itemBuyQuestionDialog.SetText(localeInfo.DO_YOU_BUY_ITEM(itemName, itemCount, localeInfo.NumberToMoneyString(itemPrice)))
		itemBuyQuestionDialog.SetAcceptEvent(lambda arg=True: self.AnswerBuyItem(arg))
		itemBuyQuestionDialog.SetCancelEvent(lambda arg=False: self.AnswerBuyItem(arg))
		itemBuyQuestionDialog.Open()
		itemBuyQuestionDialog.pos = slotPos
		self.itemBuyQuestionDialog = itemBuyQuestionDialog

		constInfo.SET_ITEM_QUESTION_DIALOG_STATUS(1)

	def AnswerBuyItem(self, flag):
		if flag:
			pos = self.itemBuyQuestionDialog.pos
			net.SendShopBuyPacket(pos)

		self.itemBuyQuestionDialog.Close()
		self.itemBuyQuestionDialog = None

		constInfo.SET_ITEM_QUESTION_DIALOG_STATUS(0)

	def HideValueBar(self):
		self.valueBar.Hide()

	def UpdateTotalSellPrice(self):
		self.valueLabel.SetText(localeInfo.NumberToMoneyString(self.totalSellValue))

	def OnAddItemMassSell(self, sourceSlotPos, sourceWindowType):
		itemPrice = player.GetISellItemPrice(sourceWindowType, sourceSlotPos)
		if not self.massSellData.has_key(sourceWindowType):
			self.massSellData[sourceWindowType] = {sourceSlotPos: 0}
		else:
			if self.massSellData[sourceWindowType].has_key(sourceSlotPos):
				self.OnRemoveItemMassSell(sourceSlotPos, sourceWindowType, itemPrice)
				return

			self.massSellData[sourceWindowType][sourceSlotPos] = 0

		self.totalSellValue += itemPrice
		self.UpdateTotalSellPrice()

	def OnRemoveItemMassSell(self, sourceSlotPos, sourceWindowType, itemPrice):
		if not self.massSellData.has_key(sourceWindowType):
			return

		if not self.massSellData[sourceWindowType].has_key(sourceSlotPos):
			return

		self.totalSellValue -= itemPrice
		self.UpdateTotalSellPrice()
		self.massSellData[sourceWindowType].pop(sourceSlotPos)

	def IsItemValuable(self, window, slot):
		itemVnum = player.GetItemIndex(window, slot)
		print "check", itemVnum
		item.SelectItem(itemVnum)

		if item.GetItemType() in (item.ITEM_TYPE_WEAPON, item.ITEM_TYPE_ARMOR):
			refineLevel = itemVnum - (itemVnum / 10) * 10
			print "check refine", refineLevel
			if refineLevel >= 7:
				return ("refine_level", item.GetItemName())

			(type, value) = player.GetItemAttribute(window, slot, 0)
			if type != 0:
				return ("attribute", item.GetItemName())

			(type, value) = item.GetLimit(0)
			if type == item.LIMIT_LEVEL and (value in [30, 70, 75] or value > 75):
				return ("good_item", item.GetItemName())
		elif itemVnum in (50513,):
			return ("high_price", item.GetItemName())

		# price = player.GetISellItemPrice(window, slot)
		# if price >= 100000:
		# 	return ("high_price", item.GetItemName())

		return ("good_to_go", "")

	def MassSell(self):
		questionDialog = uiCommon.QuestionDialog()
		questionDialog.SetText(localeInfo.MASS_SELL_QUESTION)
		questionDialog.SetAcceptEvent(self.AcceptMassSell)
		questionDialog.SetCancelEvent(ui.__mem_func__(self.OnCloseQuestionDialog))
		questionDialog.Open()
		self.questionDialog = questionDialog
		constInfo.SET_ITEM_QUESTION_DIALOG_STATUS(1)

	def AcceptMassSell(self):
		self.OnCloseQuestionDialog()
		self.CheckMassSell()

	def AcceptCheckMassSell(self, *alreadyChecked):
		self.OnCloseQuestionDialog()
		self.CheckMassSell(*alreadyChecked)

	def CheckMassSell(self, *alreadyChecked):
		if len(self.massSellData) > 0:
			for window in self.massSellData.keys():
				for slot in self.massSellData[window].keys():
					if slot in alreadyChecked:
						continue

					message, itemName = self.IsItemValuable(window, slot)
					if message != "good_to_go":
						questionDialog = uiCommon.QuestionDialog2()
						if message == "good_item":
							questionDialog.SetText1(localeInfo.MASS_SELL_WARNING)
						elif message == "refine_level":
							questionDialog.SetText1(localeInfo.MASS_SELL_WARNING_REFINE_LEVEL)
						elif message == "attribute":
							questionDialog.SetText1(localeInfo.MASS_SELL_WARNING_ATTRIBUTE)
						elif message == "high_price":
							questionDialog.SetText1(localeInfo.MASS_SELL_WARNING_HIGH_PRICE)

						questionDialog.SetText2(itemName)
						questionDialog.SetAcceptEvent(lambda arg1=alreadyChecked: self.AcceptCheckMassSell(slot, *arg1))
						questionDialog.SetCancelEvent(ui.__mem_func__(self.OnCloseQuestionDialog))
						questionDialog.Open()
						self.questionDialog = questionDialog
						constInfo.SET_ITEM_QUESTION_DIALOG_STATUS(1)
						return

			self.OnMassSell()

	def OnMassSell(self):
		if len(self.massSellData) > 0:
			for window in self.massSellData.keys():
				for slot in self.massSellData[window].keys():
					net.SendShopSellPacketNew(slot, player.GetItemCount(window, slot), window)
			snd.PlaySound("sound/ui/money.wav")
			self.massSellData = {}
			self.CancelShopping()
			eventManager.EventManager().send_event(EVENT_STOP_MASS_SELL)

	def OnSellMode(self):
		if not self.valueBar.IsShow():
			self.valueBar.Show()
			width = self.GetWidth()
			height = self.GetHeight()
			self.SetSize(width, height + 24)
			self.board.SetSize(width, height + 24)
			self.valueBar.RefreshPosition()
			self.btnBuy.RefreshPosition()
			self.btnSell.RefreshPosition()
			self.btnMassSell.RefreshPosition()
			self.btnMassSell.Show()
			self.totalSellValue = 0
			self.massSellData = {}
			self.UpdateTotalSellPrice()
			constInfo.SHOP_MASS_SELL = True

	def OnSellModeDisable(self):
		if self.valueBar.IsShow():
			self.valueBar.Hide()
			width = self.GetWidth()
			height = self.GetHeight()
			self.SetSize(width, height - 24)
			self.board.SetSize(width, height - 24)
			self.btnBuy.RefreshPosition()
			self.btnSell.RefreshPosition()
			self.btnMassSell.Hide()
			constInfo.SHOP_MASS_SELL = False
			eventManager.EventManager().send_event(EVENT_STOP_MASS_SELL)

	def SetItemToolTip(self, tooltipItem):
		self.tooltipItem = tooltipItem

	def OverInItem(self, slotIndex):
		slotIndex = self.__GetRealIndex(slotIndex)
		if mouseModule.mouseController.isAttached():
			return

		if 0 != self.tooltipItem:
			if shop.SHOP_COIN_TYPE_GOLD == shop.GetTabCoinType(self.tabIdx):
				self.tooltipItem.SetShopItem(slotIndex)
			else:
				self.tooltipItem.SetShopItemBySecondaryCoin(slotIndex)

	def OverOutItem(self):
		if 0 != self.tooltipItem:
			self.tooltipItem.HideToolTip()

	def OnUpdate(self):
		USE_SHOP_LIMIT_RANGE = 1000

		(x, y, z) = player.GetMainCharacterPosition()
		if abs(x - self.xShopStart) > USE_SHOP_LIMIT_RANGE or abs(y - self.yShopStart) > USE_SHOP_LIMIT_RANGE:
			self.Close()


class MallPageDialog(ui.ScriptWindow):
	def __init__(self):
		ui.ScriptWindow.__init__(self)

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	@ui.WindowDestroy
	def Destroy(self):
		self.ClearDictionary()

	def Open(self):
		scriptLoader = ui.PythonScriptLoader()
		scriptLoader.LoadScriptFile(self, "uiscript/mallpagedialog.py")

		self.GetChild("titlebar").SetCloseEvent(ui.__mem_func__(self.Close))

		(x, y)=self.GetGlobalPosition()
		x+=10
		y+=30

		MALL_PAGE_WIDTH = 600
		MALL_PAGE_HEIGHT = 480

		self.Lock()
		self.Show()

	def Close(self):
		app.HideWebPage()
		self.Unlock()
		self.Hide()

	def OnPressEscapeKey(self):
		self.Close()
		return True
