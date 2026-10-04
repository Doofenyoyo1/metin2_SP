import ui
import snd
import shop
import mouseModule
import player
import chr
import net
import shopautoprice # MT2009_PLUS_SHOP_AUTO_PRICE_V1
import uiCommon
import localeInfo
import chat
import item
import systemSetting
import player
import app
import ikashop
import eventManager
import uiShop
import colorInfo

g_isBuildingPrivateShop = False

g_itemPriceDict={}

if app.ENABLE_CHEQUE_SYSTEM:
	g_itemChequeDict={}

##### SHOP VISIT COLOR #####
SHOP_CURRENT_COLOR=0xFFFF8C00
SHOP_VISIT_COLOR=0xFFBAAF5D
SHOP_FLAGGED_COLOR=0xff0a95ff
###########################

g_privateShopAdvertisementBoardDict={"player": {}, "offline": {}}
g_privateShopAdvertisementSeenDict ={"player": {}, "offline": {}}

def ClearViews():
	global g_privateShopAdvertisementBoardDict
	global g_privateShopAdvertisementSeenDict

	g_privateShopAdvertisementSeenDict = {"player": {}, "offline": {}}
	for vid_map in g_privateShopAdvertisementBoardDict.itervalues():
		for board in vid_map.itervalues():
			board.RefreshState()

def Clear():
	global g_itemPriceDict
	global g_isBuildingPrivateShop
	if app.ENABLE_CHEQUE_SYSTEM:
		global g_itemChequeDict
		g_itemChequeDict={}
	g_itemPriceDict={}
	g_isBuildingPrivateShop = False
	# @fixme007 BEGIN
	global g_privateShopAdvertisementBoardDict
	global g_privateShopAdvertisementSeenDict
	g_privateShopAdvertisementBoardDict={"player": {}, "offline": {}}
	g_privateShopAdvertisementSeenDict={"player": {}, "offline": {}}
	# @fixme007 END

def IsPrivateShopItemPriceList():
	global g_itemPriceDict
	if g_itemPriceDict:
		return True
	else:
		return False

if app.ENABLE_CHEQUE_SYSTEM:
	def IsPrivateShopItemChequeList():
		global g_itemChequeDict
		if g_itemChequeDict:
			return True
		else:
			return False

def IsBuildingPrivateShop():
	global g_isBuildingPrivateShop
	if player.IsOpenPrivateShop() or g_isBuildingPrivateShop:
		return True
	else:
		return False

def SetPrivateShopItemPrice(itemVNum, itemPrice):
	global g_itemPriceDict
	g_itemPriceDict[int(itemVNum)]=itemPrice

def GetPrivateShopItemPrice(itemVNum):
	try:
		global g_itemPriceDict
		return g_itemPriceDict[itemVNum]
	except KeyError:
		return 0

if app.ENABLE_CHEQUE_SYSTEM:
	def SetPrivateShopItemCheque(itemVNum, itemPrice):
		global g_itemChequeDict
		g_itemChequeDict[int(itemVNum)]=itemPrice

	def GetPrivateShopItemCheque(itemVNum):
		try:
			global g_itemChequeDict
			return g_itemChequeDict[itemVNum]
		except KeyError:
			return 0

def UpdateADBoard(state):
	playerDict = g_privateShopAdvertisementBoardDict["player"]
	for key in playerDict.keys():
		item = playerDict[key]
		if state:
			item.Show()
		else:
			item.Hide()

	offlineDict = g_privateShopAdvertisementBoardDict["offline"]
	for key in offlineDict.keys():
		item = offlineDict[key]
		if state:
			item.Show()
		else:
			item.Hide()

def DeleteADBoard(vid, is_offline_shop=False):
	shop_type = "offline" if is_offline_shop else "player"
	if not g_privateShopAdvertisementBoardDict[shop_type].has_key(vid):
		return

	del g_privateShopAdvertisementBoardDict[shop_type][vid]


class PrivateShopAdvertisementBoard(ui.ThinBoard):
	def __init__(self):
		ui.ThinBoard.__init__(self, "UI_BOTTOM")
		self.vid = None
		self.is_offline = False
		self.is_visible = True
		self.is_flagged = False
		self.is_current = False
		self.is_viewed = False
		self.__MakeTextLine()

	def __del__(self):
		ui.ThinBoard.__del__(self)

	def __MakeTextLine(self):
		self.textLine = ui.TextLine()
		self.textLine.SetParent(self)
		self.textLine.SetWindowHorizontalAlignCenter()
		self.textLine.SetWindowVerticalAlignCenter()
		self.textLine.SetHorizontalAlignCenter()
		self.textLine.SetVerticalAlignCenter()
		self.textLine.Show()

	def Open(self, vid, text, is_offline_shop):
		self.vid = vid
		self.is_offline = is_offline_shop

		self.textLine.SetText(text)
		self.textLine.UpdateRect()
		self.SetSize(len(text)*6 + 10*2, 20)
		self.Show()

		self.RefreshState()

		g_privateShopAdvertisementBoardDict[self.__GetShopType()][vid] = self

	def RefreshState(self):
		if g_privateShopAdvertisementSeenDict[self.__GetShopType()].has_key(self.vid):
			data = g_privateShopAdvertisementSeenDict[self.__GetShopType()][self.vid]

			if data.has_key("flagged"):
				self.is_flagged = data["flagged"]

			if data.has_key("viewed"):
				self.is_viewed = True
				self.MarkAsViewed()
		else:
			self.is_flagged = False
			self.is_current = False
			self.is_viewed = False
			self.SetSkin("default")

		self.RefreshColor()

	def SetVisible(self, state):
		if not systemSetting.IsShowSalesText():
			return

		self.is_visible = state

		if self.is_visible:
			self.Show()
		else:
			self.Hide()

	def __GetShopType(self):
		if self.is_offline:
			return "offline"
		else:
			return "player"

	def OnMouseLeftButtonUp(self):
		if not self.vid:
			return False
		eventManager.EventManager().send_event(uiShop.EVENT_CLICK_PRIVATE_SHOP, self.vid, self.is_offline)
		return True

	def OnMouseRightButtonUp(self):
		self.is_flagged = not self.is_flagged
		self.RefreshColor()

		if not g_privateShopAdvertisementSeenDict[self.__GetShopType()].has_key(self.vid):
			g_privateShopAdvertisementSeenDict[self.__GetShopType()][self.vid] = {"flagged": True}
		else:
			g_privateShopAdvertisementSeenDict[self.__GetShopType()][self.vid]["flagged"] = True

		return True

	def MarkAsViewed(self):
		if self.vid != player.GetMainCharacterIndex():
			self.SetSkin("goldtint")
			if not g_privateShopAdvertisementSeenDict[self.__GetShopType()].has_key(self.vid):
				g_privateShopAdvertisementSeenDict[self.__GetShopType()][self.vid] = {"viewed": True}
			else:
				g_privateShopAdvertisementSeenDict[self.__GetShopType()][self.vid]["viewed"] = True

			self.is_current = False
			self.is_viewed = True
			self.RefreshColor()

	def MarkAsCurrent(self):
		if self.vid != player.GetMainCharacterIndex():
			self.MarkAsViewed()
			self.is_current = True
			self.RefreshColor()

	def RefreshColor(self):
		if self.is_flagged:
			self.textLine.SetPackedFontColor(SHOP_FLAGGED_COLOR)
		elif self.is_current:
			self.textLine.SetPackedFontColor(SHOP_CURRENT_COLOR)
		elif self.is_viewed:
			self.textLine.SetPackedFontColor(SHOP_VISIT_COLOR)
		else:
			self.textLine.SetPackedFontColor(colorInfo.FONT_COLOR)

	def OnUpdate(self):
		if not self.vid:
			return

		if not self.is_visible:
			return

		if systemSetting.IsShowSalesText():
			self.Show()
			x = 0
			y = 0
			if self.is_offline:
				x, y = ikashop.GetProjectPosition(self.vid, 220)
			else:
				x, y = chr.GetProjectPosition(self.vid, 220)

			self.SetPosition(x - self.GetWidth()/2, y - self.GetHeight()/2)

		else:
			ad_board_dict = g_privateShopAdvertisementBoardDict[self.__GetShopType()]
			for key in ad_board_dict.keys():
				if  player.GetMainCharacterIndex() == key:
					ad_board_dict[key].Show()
					x, y = chr.GetProjectPosition(player.GetMainCharacterIndex(), 220)
					ad_board_dict[key].SetPosition(x - self.GetWidth()/2, y - self.GetHeight()/2)
				else:
					ad_board_dict[key].Hide()

class PrivateShopBuilder(ui.ScriptWindow):

	def __init__(self):
		#print "NEW MAKE_PRIVATE_SHOP_WINDOW ----------------------------------------------------------------"
		ui.ScriptWindow.__init__(self)

		self.__LoadWindow()
		self.itemStock = {}
		self.tooltipItem = None
		self.priceInputBoard = None
		self.title = ""

	def __del__(self):
		#print "------------------------------------------------------------- DELETE MAKE_PRIVATE_SHOP_WINDOW"
		ui.ScriptWindow.__del__(self)

	def __LoadWindow(self):
		try:
			pyScrLoader = ui.PythonScriptLoader()
			pyScrLoader.LoadScriptFile(self, "UIScript/PrivateShopBuilder.py")
		except:
			import exception
			exception.Abort("PrivateShopBuilderWindow.LoadWindow.LoadObject")

		try:
			GetObject = self.GetChild
			self.nameLine = GetObject("NameLine")
			self.itemSlot = GetObject("ItemSlot")
			self.btnOk = GetObject("OkButton")
			self.btnClose = GetObject("CloseButton")
			self.titleBar = GetObject("TitleBar")
		except:
			import exception
			exception.Abort("PrivateShopBuilderWindow.LoadWindow.BindObject")

		self.btnOk.SetEvent(ui.__mem_func__(self.OnOk))
		self.btnClose.SetEvent(ui.__mem_func__(self.OnClose))
		self.titleBar.SetCloseEvent(ui.__mem_func__(self.OnClose))

		self.itemSlot.SetSelectEmptySlotEvent(ui.__mem_func__(self.OnSelectEmptySlot))
		self.itemSlot.SetSelectItemSlotEvent(ui.__mem_func__(self.OnSelectItemSlot))
		self.itemSlot.SetOverInItemEvent(ui.__mem_func__(self.OnOverInItem))
		self.itemSlot.SetOverOutItemEvent(ui.__mem_func__(self.OnOverOutItem))

	@ui.WindowDestroy
	def Destroy(self):
		self.__ClosePriceInputBoard()
		self.ClearDictionary()

		self.nameLine = None
		self.itemSlot = None
		self.btnOk = None
		self.btnClose = None
		self.titleBar = None
		self.priceInputBoard = None

	def Open(self, title):

		self.title = title

		if len(title) > 25:
			title = title[:22] + "..."

		self.itemStock = {}
		shop.ClearPrivateShopStock()
		self.nameLine.SetText(title)
		self.SetCenterPosition()
		self.Refresh()
		self.Show()

		global g_isBuildingPrivateShop
		g_isBuildingPrivateShop = True

	def Close(self):
		global g_isBuildingPrivateShop
		g_isBuildingPrivateShop = False

		self.title = ""
		self.itemStock = {}
		shop.ClearPrivateShopStock()
		self.__ClosePriceInputBoard()
		self.Hide()

	def SetItemToolTip(self, tooltipItem):
		self.tooltipItem = tooltipItem

	def Refresh(self):
		getitemVNum=player.GetItemIndex
		getItemCount=player.GetItemCount
		setitemVNum=self.itemSlot.SetItemSlot
		delItem=self.itemSlot.ClearSlot

		for i in xrange(shop.SHOP_SLOT_COUNT):

			if not self.itemStock.has_key(i):
				delItem(i)
				continue

			pos = self.itemStock[i]

			itemCount = getItemCount(*pos)
			if itemCount <= 1:
				itemCount = 0
			setitemVNum(i, getitemVNum(*pos), itemCount)

		self.itemSlot.RefreshSlot()

	def OnSelectEmptySlot(self, selectedSlotPos):

		isAttached = mouseModule.mouseController.isAttached()
		if isAttached:
			attachedSlotType = mouseModule.mouseController.GetAttachedType()
			attachedSlotPos = mouseModule.mouseController.GetAttachedSlotNumber()
			mouseModule.mouseController.DeattachObject()

			if player.SLOT_TYPE_INVENTORY != attachedSlotType and player.SLOT_TYPE_DRAGON_SOUL_INVENTORY != attachedSlotType:
				return
			attachedInvenType = player.SlotTypeToInvenType(attachedSlotType)

			itemVNum = player.GetItemIndex(attachedInvenType, attachedSlotPos)
			item.SelectItem(itemVNum)

			if item.IsAntiFlag(item.ANTIFLAG_GIVE) or item.IsAntiFlag(item.ANTIFLAG_MYSHOP):
				chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.PRIVATE_SHOP_CANNOT_SELL_ITEM)
				return

			# The price window of the item before goes, its Ceny window with it.
			self.__ClosePriceInputBoard()

			priceInputBoard = uiCommon.MoneyInputDialog()
			priceInputBoard.SetTitle(localeInfo.PRIVATE_SHOP_INPUT_PRICE_DIALOG_TITLE)
			priceInputBoard.SetAcceptEvent(ui.__mem_func__(self.AcceptInputPrice))
			priceInputBoard.SetCancelEvent(ui.__mem_func__(self.CancelInputPrice))
			priceInputBoard.Open()

			itemPrice=GetPrivateShopItemPrice(itemVNum)

			if itemPrice>0:
				priceInputBoard.SetValue(itemPrice)

			if app.ENABLE_CHEQUE_SYSTEM:
				itemCheque=GetPrivateShopItemCheque(itemVNum)
				if itemCheque>0:
					priceInputBoard.SetValueCheque(itemCheque)

			self.priceInputBoard = priceInputBoard
			self.priceInputBoard.itemVNum = itemVNum
			self.priceInputBoard.sourceWindowType = attachedInvenType
			self.priceInputBoard.sourceSlotPos = attachedSlotPos
			self.priceInputBoard.targetSlotPos = selectedSlotPos
			# MT2009_PLUS_SHOP_AUTO_PRICE_V1: the Dom Towarowy's hint and the
			# automatic price; this window takes nine digits (SetMaxLength(9)).
			self.priceInputBoard.fleaPriceMax = 999999999
			shopautoprice.RequestDialog(self.priceInputBoard, attachedInvenType, attachedSlotPos)

	def OnSelectItemSlot(self, selectedSlotPos):

		isAttached = mouseModule.mouseController.isAttached()
		if isAttached:
			snd.PlaySound("sound/ui/loginfail.wav")
			mouseModule.mouseController.DeattachObject()

		else:
			if not selectedSlotPos in self.itemStock:
				return

			invenType, invenPos = self.itemStock[selectedSlotPos]
			shop.DelPrivateShopItemStock(invenType, invenPos)
			snd.PlaySound("sound/ui/drop.wav")

			del self.itemStock[selectedSlotPos]

			self.Refresh()

	def AcceptInputPrice(self):

		if not self.priceInputBoard:
			return True

		if app.ENABLE_CHEQUE_SYSTEM:
			text = self.priceInputBoard.GetText()
			cheque = self.priceInputBoard.GetTextCheque()
			# MT2009_PLUS_VEKIRION_V1: True - Enter handled, no chat behind the window.
			if not text:
				return True
			if not text.isdigit():
				return True

			if not cheque:
				return True
			if not cheque.isdigit():
				return True

			if int(cheque) <=0 and int(text)<=0:
				chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.CHEQUE_NO_ADD_SALE_PRICE)
				return True
		else:
			text = self.priceInputBoard.GetText()

			if not text:
				return True

			if not text.isdigit():
				return True

			if int(text) <= 0:
				return True

		attachedInvenType = self.priceInputBoard.sourceWindowType
		sourceSlotPos = self.priceInputBoard.sourceSlotPos
		targetSlotPos = self.priceInputBoard.targetSlotPos

		for privatePos, (itemWindowType, itemSlotIndex) in self.itemStock.items():
			if itemWindowType == attachedInvenType and itemSlotIndex == sourceSlotPos:
				shop.DelPrivateShopItemStock(itemWindowType, itemSlotIndex)
				del self.itemStock[privatePos]

		price = int(self.priceInputBoard.GetText())

		if IsPrivateShopItemPriceList():
			SetPrivateShopItemPrice(self.priceInputBoard.itemVNum, price)

		if app.ENABLE_CHEQUE_SYSTEM:
			chequep = int(self.priceInputBoard.GetTextCheque())
			if IsPrivateShopItemChequeList():
				SetPrivateShopItemCheque(self.priceInputBoard.itemVNum, chequep)
			shop.AddPrivateShopItemStock(attachedInvenType, sourceSlotPos, targetSlotPos, price, chequep)
		else:
			shop.AddPrivateShopItemStock(attachedInvenType, sourceSlotPos, targetSlotPos, price)
		self.itemStock[targetSlotPos] = (attachedInvenType, sourceSlotPos)
		snd.PlaySound("sound/ui/drop.wav")

		self.Refresh()

		#####

		self.__ClosePriceInputBoard()
		return True

	def CancelInputPrice(self):
		self.__ClosePriceInputBoard()
		return True

	def __ClosePriceInputBoard(self):
		if self.priceInputBoard:
			shopautoprice.CancelDialog(self.priceInputBoard)
			self.priceInputBoard.Close()
			self.priceInputBoard = None

	def OnOk(self):

		if not self.title:
			return

		if 0 == len(self.itemStock):
			return

		shop.BuildPrivateShop(self.title)
		self.Close()

	def OnClose(self):
		self.Close()

	def OnPressEscapeKey(self):
		self.Close()
		return True

	def OnOverInItem(self, slotIndex):

		if self.tooltipItem:
			if self.itemStock.has_key(slotIndex):
				self.tooltipItem.SetPrivateShopBuilderItem(*self.itemStock[slotIndex] + (slotIndex,))

	def OnOverOutItem(self):

		if self.tooltipItem:
			self.tooltipItem.HideToolTip()
