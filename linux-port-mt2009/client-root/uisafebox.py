import ui
import mouseModule
import player
import net
import snd
import safebox
import chat
import app
import localeInfo
import uiScriptLocale
import eventManager
import uiInventory
import item

EVENT_QUICK_REMOVE_SAFEBOX_ITEM = "EVENT_QUICK_REMOVE_SAFEBOX_ITEM" # args | type: string, slotNumber: number

EVENT_OPEN_SAFEBOX = "EVENT_OPEN_SAFEBOX" # args |
EVENT_CLOSE_SAFEBOX = "EVENT_CLOSE_SAFEBOX" # args |

# MT2009_PLUS_SAFEBOX_ARRANGE_V1: the safebox's "Uloz i scal" and "Tylko scal
# stosy" buttons (uiscript/safeboxwindow.py), the two ways the inventory's
# button tidies the bag (inventoryarrange.py). One command each and the
# server does the work on every page of the box at once
# (playerbot_arrange.cpp): "/safebox_arrange" pours the stacks together and
# lays the pages out again, "/safebox_arrange merge" only pours
# (server-patches/safeboxmerge). The answer, "SafeboxArrangeResult <code>
# <moved> <merged> <units>", comes through game.py to OnArrangeResult.
#
# No move packets: the safebox's own move does not stack in this engine
# (ENABLE_MT2009_DISABLE_SAFEBOX_STACK), and a move per stack would run into
# the server's packet limits. A second click while a request is out does
# nothing; an answer that never comes frees the buttons after
# ARRANGE_PENDING_TIMEOUT seconds, and the server keeps two seconds between
# two requests. Nothing is sent while an item hangs on the cursor or a private
# shop is being built; the server refuses an exchange, a shop or another
# window itself (RESULT_BUSY).
#
# The texts are CP1250, the client's own, written as escapes so the file
# stays ASCII.
ARRANGE_PENDING_TIMEOUT = 5.0

# playerbot_arrange.h, EResult.
ARRANGE_DONE = 0
ARRANGE_NOTHING = 1
ARRANGE_BUSY = 2
ARRANGE_COOLDOWN = 3
ARRANGE_NO_LAYOUT = 4
ARRANGE_DEAD = 5
ARRANGE_INCONSISTENT = 6
ARRANGE_UNSUPPORTED = 7
ARRANGE_BAD_REQUEST = 8
ARRANGE_NO_SAFEBOX = 9

ARRANGE_MODE_SORT = 0
ARRANGE_MODE_MERGE = 1

ARRANGE_MSG_DONE = 'Uporz\xb9dkowano magazyn: przestawiono %d, scalono stos\xf3w: %d.'
ARRANGE_MSG_NOTHING = 'Magazyn jest ju\xbf uporz\xb9dkowany.'
ARRANGE_MSG_MERGED = 'Po\xb3\xb9czono stosy w magazynie: %d. Reszta przedmiot\xf3w zosta\xb3a na miejscu.'
ARRANGE_MSG_NOTHING_MERGE = 'W magazynie nie ma stos\xf3w do po\xb3\xb9czenia.'
ARRANGE_MSG_BUSY = 'Nie mo\xbfna teraz uporz\xb9dkowa\xe6 magazynu - zamknij handel, sklep lub inne okno.'
ARRANGE_MSG_COOLDOWN = 'Odczekaj chwil\xea przed kolejnym porz\xb9dkowaniem.'
ARRANGE_MSG_NO_LAYOUT = 'Nie uda\xb3o si\xea u\xb3o\xbfy\xe6 magazynu - nic nie zmieniono.'
ARRANGE_MSG_DEAD = 'Nie mo\xbfesz porz\xb9dkowa\xe6 magazynu po \x9cmierci.'
ARRANGE_MSG_INCONSISTENT = 'Magazyn jest w nieoczekiwanym stanie - nic nie zmieniono. Zg\xb3o\x9c to na Discordzie.'
ARRANGE_MSG_NO_SAFEBOX = 'Magazyn nie jest otwarty.'
ARRANGE_MSG_UNSUPPORTED = 'Serwer nie obs\xb3uguje porz\xb9dkowania magazynu.'
ARRANGE_MSG_ATTACHED = 'Od\xb3\xf3\xbf najpierw przedmiot trzymany kursorem.'
ARRANGE_MSG_SHOP = 'Nie mo\xbfna porz\xb9dkowa\xe6 magazynu podczas otwierania sklepu.'

_arrangeState = {'pendingUntil': 0.0, 'mode': ARRANGE_MODE_SORT}


def _ArrangeInt(value):
	try:
		return int(value)
	except (TypeError, ValueError):
		return -1


def _ArrangeNow():
	import clientclock
	return clientclock.Now()


def IsArrangePending():
	return _ArrangeNow() < _arrangeState['pendingUntil']


def RequestArrange(mode=ARRANGE_MODE_SORT):
	if IsArrangePending():
		return False
	if mouseModule.mouseController.isAttached():
		chat.AppendChat(chat.CHAT_TYPE_INFO, ARRANGE_MSG_ATTACHED)
		return False
	# Asked here and not at import: the shop builder pulls half the
	# interface in behind it.
	import uiPrivateShopBuilder
	if uiPrivateShopBuilder.IsBuildingPrivateShop():
		chat.AppendChat(chat.CHAT_TYPE_INFO, ARRANGE_MSG_SHOP)
		return False
	_arrangeState['pendingUntil'] = _ArrangeNow() + ARRANGE_PENDING_TIMEOUT
	_arrangeState['mode'] = mode
	net.SendChatPacket('/safebox_arrange merge' if mode == ARRANGE_MODE_MERGE else '/safebox_arrange')
	return True


def ArrangeMessage(code, moved, merged, mode):
	if mode == ARRANGE_MODE_MERGE and code == ARRANGE_DONE:
		return ARRANGE_MSG_MERGED % merged
	if mode == ARRANGE_MODE_MERGE and code == ARRANGE_NOTHING:
		return ARRANGE_MSG_NOTHING_MERGE
	if code == ARRANGE_DONE:
		return ARRANGE_MSG_DONE % (moved, merged)
	if code == ARRANGE_NOTHING:
		return ARRANGE_MSG_NOTHING
	if code == ARRANGE_BUSY:
		return ARRANGE_MSG_BUSY
	if code == ARRANGE_COOLDOWN:
		return ARRANGE_MSG_COOLDOWN
	if code == ARRANGE_NO_LAYOUT:
		return ARRANGE_MSG_NO_LAYOUT
	if code == ARRANGE_DEAD:
		return ARRANGE_MSG_DEAD
	if code == ARRANGE_INCONSISTENT:
		return ARRANGE_MSG_INCONSISTENT
	if code == ARRANGE_NO_SAFEBOX:
		return ARRANGE_MSG_NO_SAFEBOX
	# RESULT_UNSUPPORTED, and RESULT_BAD_REQUEST from a server without
	# "/safebox_arrange merge".
	return ARRANGE_MSG_UNSUPPORTED


def OnArrangeResult(code='0', moved='0', merged='0', units='0'):
	_arrangeState['pendingUntil'] = 0.0
	chat.AppendChat(chat.CHAT_TYPE_INFO, ArrangeMessage(_ArrangeInt(code), max(0, _ArrangeInt(moved)),
		max(0, _ArrangeInt(merged)), _arrangeState['mode']))

class PasswordDialog(ui.ScriptWindow):
	def __init__(self):
		ui.ScriptWindow.__init__(self)
		self.__LoadDialog()

		self.sendMessage = "/safebox_password "

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def __LoadDialog(self):
		try:
			pyScrLoader = ui.PythonScriptLoader()
			pyScrLoader.LoadScriptFile(self, uiScriptLocale.LOCALE_UISCRIPT_PATH + "passworddialog.py")
		except:
			import exception
			exception.Abort("PasswordDialog.__LoadDialog.LoadObject")

		try:
			self.passwordValue = self.GetChild("password_value")
			self.acceptButton = self.GetChild("accept_button")
			self.cancelButton = self.GetChild("cancel_button")
			self.titleName = self.GetChild("TitleName")
			self.GetChild("titlebar").SetCloseEvent(ui.__mem_func__(self.CloseDialog))
		except:
			import exception
			exception.Abort("PasswordDialog.__LoadDialog.BindObject")

		self.passwordValue.OnIMEReturn = self.OnAccept
		self.passwordValue.OnPressEscapeKey = self.OnCancel
		self.acceptButton.SetEvent(ui.__mem_func__(self.OnAccept))
		self.cancelButton.SetEvent(ui.__mem_func__(self.OnCancel))

	@ui.WindowDestroy
	def Destroy(self):
		self.ClearDictionary()
		self.passwordValue = None
		self.acceptButton = None
		self.cancelButton = None
		self.titleName = None

	def SetTitle(self, title):
		self.titleName.SetText(title)

	def SetSendMessage(self, msg):
		self.sendMessage = msg

	def ShowDialog(self):
		self.passwordValue.SetText("")
		self.passwordValue.SetFocus()
		self.SetCenterPosition()
		self.Show()

	def CloseDialog(self):
		self.passwordValue.KillFocus()
		self.Hide()

	def OnAccept(self):
		net.SendChatPacket(self.sendMessage + self.passwordValue.GetText())
		self.CloseDialog()
		return True

	def OnCancel(self):
		self.CloseDialog()
		return True

class ChangePasswordDialog(ui.ScriptWindow):
	def __init__(self):
		ui.ScriptWindow.__init__(self)
		self.__LoadDialog()

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def __LoadDialog(self):
		self.dlgMessage = ui.ScriptWindow()
		try:
			pyScrLoader = ui.PythonScriptLoader()
			pyScrLoader.LoadScriptFile(self.dlgMessage, "uiscript/popupdialog.py")
			self.dlgMessage.GetChild("message").SetText(localeInfo.SAFEBOX_WRONG_PASSWORD)
			self.dlgMessage.GetChild("accept").SetEvent(ui.__mem_func__(self.OnCloseMessageDialog))
		except:
			import exception
			exception.Abort("SafeboxWindow.__LoadDialog.LoadObject")

	def LoadDialog(self):
		try:
			pyScrLoader = ui.PythonScriptLoader()
			pyScrLoader.LoadScriptFile(self, "uiscript/changepassworddialog.py")

		except:
			import exception
			exception.Abort("ChangePasswordDialog.LoadDialog.LoadObject")

		try:
			self.GetChild("accept_button").SetEvent(ui.__mem_func__(self.OnAccept))
			self.GetChild("cancel_button").SetEvent(ui.__mem_func__(self.OnCancel))
			self.GetChild("titlebar").SetCloseEvent(ui.__mem_func__(self.OnCancel))
			oldPassword = self.GetChild("old_password_value")
			newPassword = self.GetChild("new_password_value")
			newPasswordCheck = self.GetChild("new_password_check_value")
		except:
			import exception
			exception.Abort("ChangePasswordDialog.LoadDialog.BindObject")

		oldPassword.SetTabEvent(lambda arg=1: self.OnNextFocus(arg))
		newPassword.SetTabEvent(lambda arg=2: self.OnNextFocus(arg))
		newPasswordCheck.SetTabEvent(lambda arg=3: self.OnNextFocus(arg))
		oldPassword.SetReturnEvent(lambda arg=1: self.OnNextFocus(arg))
		newPassword.SetReturnEvent(lambda arg=2: self.OnNextFocus(arg))
		newPasswordCheck.SetReturnEvent(ui.__mem_func__(self.OnAccept))
		oldPassword.OnPressEscapeKey = self.OnCancel
		newPassword.OnPressEscapeKey = self.OnCancel
		newPasswordCheck.OnPressEscapeKey = self.OnCancel

		self.oldPassword = oldPassword
		self.newPassword = newPassword
		self.newPasswordCheck = newPasswordCheck

	def OnNextFocus(self, arg):
		if 1 == arg:
			self.oldPassword.KillFocus()
			self.newPassword.SetFocus()
		elif 2 == arg:
			self.newPassword.KillFocus()
			self.newPasswordCheck.SetFocus()
		elif 3 == arg:
			self.newPasswordCheck.KillFocus()
			self.oldPassword.SetFocus()

	@ui.WindowDestroy
	def Destroy(self):
		self.ClearDictionary()
		if self.dlgMessage:
			self.dlgMessage.ClearDictionary()
		self.oldPassword = None
		self.newPassword = None
		self.newPasswordCheck = None

	def Open(self):
		self.oldPassword.SetText("")
		self.newPassword.SetText("")
		self.newPasswordCheck.SetText("")
		self.oldPassword.SetFocus()
		self.SetCenterPosition()
		self.SetTop()
		self.Show()

	def Close(self):
		self.oldPassword.SetText("")
		self.newPassword.SetText("")
		self.newPasswordCheck.SetText("")
		self.oldPassword.KillFocus()
		self.newPassword.KillFocus()
		self.newPasswordCheck.KillFocus()
		self.Hide()

	def OnAccept(self):
		oldPasswordText = self.oldPassword.GetText()
		newPasswordText = self.newPassword.GetText()
		newPasswordCheckText = self.newPasswordCheck.GetText()
		if newPasswordText != newPasswordCheckText:
			self.dlgMessage.SetCenterPosition()
			self.dlgMessage.SetTop()
			self.dlgMessage.Show()
			return True
		net.SendChatPacket("/safebox_change_password %s %s" % (oldPasswordText, newPasswordText))
		self.Close()
		return True

	def OnCancel(self):
		self.Close()
		return True

	def OnCloseMessageDialog(self):
		self.newPassword.SetText("")
		self.newPasswordCheck.SetText("")
		self.newPassword.SetFocus()
		self.dlgMessage.Hide()

class SafeboxWindow(ui.ScriptWindow):

	BOX_WIDTH = 176

	def __init__(self):
		ui.ScriptWindow.__init__(self)
		self.tooltipItem = None
		self.sellingSlotNumber = -1
		self.pageButtonList = []
		self.curPageIndex = 0
		self.isLoaded = 0
		self.xSafeBoxStart = 0
		self.ySafeBoxStart = 0

		eventManager.EventManager().add_observer(uiInventory.EVENT_QUICK_ADD_INVENTORY_ITEM,
												 self.OnQuickAddInventoryItem)

		self.__LoadWindow()

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def Show(self):
		self.__LoadWindow()

		ui.ScriptWindow.Show(self)

	@ui.WindowDestroy
	def Destroy(self):
		self.ClearDictionary()
		if self.dlgPickMoney:
			self.dlgPickMoney.Destroy()
			self.dlgPickMoney = None
		if self.dlgChangePassword:
			self.dlgChangePassword.Destroy()
			self.dlgChangePassword = None

		self.tooltipItem = None
		self.wndMoneySlot = None
		self.wndMoney = None
		self.wndBoard = None
		self.wndItem = None

		self.pageButtonList = []

	def __LoadWindow(self):
		if self.isLoaded == 1:
			return

		self.isLoaded = 1

		pyScrLoader = ui.PythonScriptLoader()
		pyScrLoader.LoadScriptFile(self, "UIScript/SafeboxWindow.py")

		from _weakref import proxy

		## Item
		wndItem = ui.GridSlotWindow()
		wndItem.SetParent(self)
		wndItem.SetPosition(8, 35)
		wndItem.SetSelectEmptySlotEvent(ui.__mem_func__(self.SelectEmptySlot))
		wndItem.SetSelectItemSlotEvent(ui.__mem_func__(self.SelectItemSlot))
		wndItem.SetUnselectItemSlotEvent(ui.__mem_func__(self.UseItemSlot))
		wndItem.SetUseSlotEvent(ui.__mem_func__(self.UseItemSlot))
		wndItem.SetOverInItemEvent(ui.__mem_func__(self.OverInItem))
		wndItem.SetOverOutItemEvent(ui.__mem_func__(self.OverOutItem))
		wndItem.Show()

		## PickMoneyDialog
		import uiPickMoney
		dlgPickMoney = uiPickMoney.PickMoneyDialog()
		dlgPickMoney.LoadDialog()
		dlgPickMoney.SetAcceptEvent(ui.__mem_func__(self.OnPickMoney))
		dlgPickMoney.Hide()

		## ChangePasswrod
		dlgChangePassword = ChangePasswordDialog()
		dlgChangePassword.LoadDialog()
		dlgChangePassword.Hide()

		## Close Button
		self.GetChild("TitleBar").SetCloseEvent(ui.__mem_func__(self.Close))
		self.GetChild("ChangePasswordButton").SetEvent(ui.__mem_func__(self.OnChangePassword))
		self.GetChild("ExitButton").SetEvent(ui.__mem_func__(self.Close))
		# MT2009_PLUS_SAFEBOX_ARRANGE_V1: the title bar's two buttons.
		self.GetChild("SortButton").SetEvent(ui.__mem_func__(self.__OnSortButton))
		self.GetChild("StackButton").SetEvent(ui.__mem_func__(self.__OnStackButton))

		self.wndItem = wndItem
		self.dlgPickMoney = dlgPickMoney
		self.dlgChangePassword = dlgChangePassword
		self.wndBoard = self.GetChild("board")
		#self.wndMoney = self.GetChild("Money")
		#self.wndMoneySlot = self.GetChild("Money_Slot")
		#self.wndMoneySlot.SetEvent(ui.__mem_func__(self.OpenPickMoneyDialog))

		## Initialize
		self.SetTableSize(3)
		self.RefreshSafeboxMoney()

	def OpenPickMoneyDialog(self):

		if mouseModule.mouseController.isAttached():

			attachedSlotPos = mouseModule.mouseController.GetAttachedSlotNumber()
			if player.SLOT_TYPE_INVENTORY == mouseModule.mouseController.GetAttachedType():

				if player.ITEM_MONEY == mouseModule.mouseController.GetAttachedItemIndex():
					net.SendSafeboxSaveMoneyPacket(mouseModule.mouseController.GetAttachedItemCount())
					snd.PlaySound("sound/ui/money.wav")

			mouseModule.mouseController.DeattachObject()

		else:
			curMoney = safebox.GetMoney()

			if curMoney <= 0:
				return

			self.dlgPickMoney.Open(curMoney)

	def ShowWindow(self, size):

		(self.xSafeBoxStart, self.ySafeBoxStart, z) = player.GetMainCharacterPosition()

		self.SetTableSize(size)
		self.Show()
		eventManager.EventManager().send_event(EVENT_OPEN_SAFEBOX)

	def __MakePageButton(self, pageCount):

		self.curPageIndex = 0
		self.pageButtonList = []

		text = "I"
		pos = -int(float(pageCount-1)/2 * 52)
		for i in xrange(pageCount):
			button = ui.RadioButton()
			button.SetParent(self)
			button.SetUpVisual("d:/ymir work/ui/game/windows/tab_button_middle_01.sub")
			button.SetOverVisual("d:/ymir work/ui/game/windows/tab_button_middle_02.sub")
			button.SetDownVisual("d:/ymir work/ui/game/windows/tab_button_middle_03.sub")
			button.SetWindowHorizontalAlignCenter()
			button.SetWindowVerticalAlignBottom()
			button.SetPosition(pos, 85)
			button.SetText(text)
			button.SetEvent(lambda arg=i: self.SelectPage(arg))
			button.Show()
			self.pageButtonList.append(button)

			pos += 52
			text += "I"

		self.pageButtonList[0].Down()

	def SelectPage(self, index):

		self.curPageIndex = index

		for btn in self.pageButtonList:
			btn.SetUp()

		self.pageButtonList[index].Down()
		self.RefreshSafebox()

	def __LocalPosToGlobalPos(self, local):
		return self.curPageIndex*safebox.SAFEBOX_PAGE_SIZE + local

	def SetTableSize(self, pageCount):
		size = safebox.SAFEBOX_SLOT_Y_COUNT
		self.__MakePageButton(pageCount)

		self.wndItem.ArrangeSlot(0, safebox.SAFEBOX_SLOT_X_COUNT, size, 32, 32, 0, 0)
		self.wndItem.RefreshSlot()
		self.wndItem.SetSlotBaseImage("d:/ymir work/ui/public/Slot_Base.sub", 1.0, 1.0, 1.0, 1.0)

		wnd_height = 130 + 32 * size
		self.wndBoard.SetSize(self.BOX_WIDTH, wnd_height)
		self.SetSize(self.BOX_WIDTH, wnd_height)
		self.UpdateRect()

	def RefreshSafebox(self):
		getItemID=safebox.GetItemID
		getItemCount=safebox.GetItemCount
		setItemID=self.wndItem.SetItemSlot

		for i in xrange(safebox.SAFEBOX_PAGE_SIZE):
			slotIndex = self.__LocalPosToGlobalPos(i)
			socket = tuple(safebox.GetItemMetinSocket(slotIndex, j) for j in range(player.METIN_SOCKET_MAX_NUM))
			itemCount = getItemCount(slotIndex)
			if itemCount <= 1:
				itemCount = 0
			setItemID(i, getItemID(slotIndex), itemCount, socket=socket)

		self.wndItem.RefreshSlot()

	def RefreshSafeboxMoney(self):
		pass
		#self.wndMoney.SetText(str(safebox.GetMoney()))

	def SetItemToolTip(self, tooltip):
		self.tooltipItem = tooltip

	def Close(self):
		net.SendChatPacket("/safebox_close")
		self.Hide() # @fixme009
		eventManager.EventManager().send_event(EVENT_CLOSE_SAFEBOX)

	def CommandCloseSafebox(self):
		if self.IsShow():
			eventManager.EventManager().send_event(EVENT_CLOSE_SAFEBOX)

		if self.tooltipItem:
			self.tooltipItem.HideToolTip()

		self.dlgPickMoney.Close()
		self.dlgChangePassword.Close()
		self.Hide()

	## Slot Event
	def SelectEmptySlot(self, selectedSlotPos):

		selectedSlotPos = self.__LocalPosToGlobalPos(selectedSlotPos)

		if mouseModule.mouseController.isAttached():

			attachedSlotType = mouseModule.mouseController.GetAttachedType()
			attachedSlotPos = mouseModule.mouseController.GetAttachedSlotNumber()

			if player.SLOT_TYPE_SAFEBOX == attachedSlotType:

				net.SendSafeboxItemMovePacket(attachedSlotPos, selectedSlotPos)
				#snd.PlaySound("sound/ui/drop.wav")
			else:
				attachedInvenType = player.SlotTypeToInvenType(attachedSlotType)
				if player.RESERVED_WINDOW == attachedInvenType:
					return

				if player.ITEM_MONEY == mouseModule.mouseController.GetAttachedItemIndex():
					net.SendSafeboxSaveMoneyPacket(mouseModule.mouseController.GetAttachedItemCount())
					snd.PlaySound("sound/ui/money.wav")

				else:
					self.AddItemToSafebox(attachedInvenType, attachedSlotPos, selectedSlotPos)
					#snd.PlaySound("sound/ui/drop.wav")

			mouseModule.mouseController.DeattachObject()

	def SelectItemSlot(self, selectedSlotPos):

		selectedSlotPos = self.__LocalPosToGlobalPos(selectedSlotPos)

		if mouseModule.mouseController.isAttached():

			attachedSlotType = mouseModule.mouseController.GetAttachedType()

			if player.SLOT_TYPE_INVENTORY == attachedSlotType:

				if player.ITEM_MONEY == mouseModule.mouseController.GetAttachedItemIndex():
					net.SendSafeboxSaveMoneyPacket(mouseModule.mouseController.GetAttachedItemCount())
					snd.PlaySound("sound/ui/money.wav")

				else:
					attachedSlotPos = mouseModule.mouseController.GetAttachedSlotNumber()
					#net.SendSafeboxCheckinPacket(attachedSlotPos, selectedSlotPos)
					#snd.PlaySound("sound/ui/drop.wav")
			elif player.SLOT_TYPE_SAFEBOX == attachedSlotType:
				attachedSlotPos = mouseModule.mouseController.GetAttachedSlotNumber()
				net.SendSafeboxItemMovePacket(attachedSlotPos, selectedSlotPos)

			mouseModule.mouseController.DeattachObject()

		else:

			curCursorNum = app.GetCursor()
			if app.SELL == curCursorNum:
				chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.SAFEBOX_SELL_DISABLE_SAFEITEM)

			elif app.BUY == curCursorNum:
				chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.SHOP_BUY_INFO)

			else:
				selectedItemID = safebox.GetItemID(selectedSlotPos)
				mouseModule.mouseController.AttachObject(self, player.SLOT_TYPE_SAFEBOX, selectedSlotPos, selectedItemID)
				snd.PlaySound("sound/ui/pick.wav")

	def AddItemToSafebox(self, window_type, sourceSlot, destSlot):
		net.SendSafeboxCheckinPacket(window_type, sourceSlot, destSlot)

	# MT2009_PLUS_SAFEBOX_ARRANGE_V1
	def __OnSortButton(self):
		RequestArrange(ARRANGE_MODE_SORT)

	def __OnStackButton(self):
		RequestArrange(ARRANGE_MODE_MERGE)

	def RemoveItemFromSafebox(self, slotPos):
		pass

	def OnQuickAddInventoryItem(self, type, slotNumber):
		if type != "safebox":
			return

		itemVnum = player.GetItemIndex(slotNumber)
		if itemVnum > 0:
			item.SelectItem(itemVnum)
			(itemWidth, itemHeight) = item.GetItemSize()

			freeSlot = self.wndItem.FindEmptySlot(itemHeight, self.curPageIndex * safebox.SAFEBOX_PAGE_SIZE)
			if freeSlot >= 0:
				self.AddItemToSafebox(player.INVENTORY, slotNumber, freeSlot)

	def UseItemSlot(self, slotIndex):
		mouseModule.mouseController.DeattachObject()
		slotIndex = self.__LocalPosToGlobalPos(slotIndex)
		eventManager.EventManager().send_event(EVENT_QUICK_REMOVE_SAFEBOX_ITEM, "safebox", slotIndex)

	def __ShowToolTip(self, slotIndex):
		if self.tooltipItem:
			self.tooltipItem.SetSafeBoxItem(slotIndex)

	def OverInItem(self, slotIndex):
		slotIndex = self.__LocalPosToGlobalPos(slotIndex)
		self.wndItem.SetUsableItem(False)
		self.__ShowToolTip(slotIndex)

	def OverOutItem(self):
		self.wndItem.SetUsableItem(False)
		if self.tooltipItem:
			self.tooltipItem.HideToolTip()

	def OnPickMoney(self, money):
		mouseModule.mouseController.AttachMoney(self, player.SLOT_TYPE_SAFEBOX, money)

	def OnChangePassword(self):
		self.dlgChangePassword.Open()

	def OnPressEscapeKey(self):
		self.Close()
		return True

	def OnUpdate(self):

		USE_SAFEBOX_LIMIT_RANGE = 1000

		(x, y, z) = player.GetMainCharacterPosition()
		if abs(x - self.xSafeBoxStart) > USE_SAFEBOX_LIMIT_RANGE or abs(y - self.ySafeBoxStart) > USE_SAFEBOX_LIMIT_RANGE:
			self.Close()

class MallWindow(ui.ScriptWindow):

	BOX_WIDTH = 176

	def __init__(self):
		ui.ScriptWindow.__init__(self)
		self.tooltipItem = None
		self.sellingSlotNumber = -1
		self.pageButtonList = []
		self.curPageIndex = 0
		self.isLoaded = 0
		self.xSafeBoxStart = 0
		self.ySafeBoxStart = 0

		self.__LoadWindow()

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def Show(self):
		self.__LoadWindow()

		ui.ScriptWindow.Show(self)

	@ui.WindowDestroy
	def Destroy(self):
		self.ClearDictionary()

		self.tooltipItem = None
		self.wndBoard = None
		self.wndItem = None

		self.pageButtonList = []

	def __LoadWindow(self):
		if self.isLoaded == 1:
			return

		self.isLoaded = 1

		pyScrLoader = ui.PythonScriptLoader()
		pyScrLoader.LoadScriptFile(self, "UIScript/MallWindow.py")

		from _weakref import proxy

		## Item
		wndItem = ui.GridSlotWindow()
		wndItem.SetParent(self)
		wndItem.SetPosition(8, 35)
		wndItem.SetSelectEmptySlotEvent(ui.__mem_func__(self.SelectEmptySlot))
		wndItem.SetSelectItemSlotEvent(ui.__mem_func__(self.SelectItemSlot))
		wndItem.SetUnselectItemSlotEvent(ui.__mem_func__(self.UseItemSlot))
		wndItem.SetUseSlotEvent(ui.__mem_func__(self.UseItemSlot))
		wndItem.SetOverInItemEvent(ui.__mem_func__(self.OverInItem))
		wndItem.SetOverOutItemEvent(ui.__mem_func__(self.OverOutItem))
		wndItem.Show()

		## Close Button
		self.GetChild("TitleBar").SetCloseEvent(ui.__mem_func__(self.Close))
		self.GetChild("ExitButton").SetEvent(ui.__mem_func__(self.Close))

		self.wndItem = wndItem
		self.wndBoard = self.GetChild("board")

		## Initialize
		self.SetTableSize(3)

	def ShowWindow(self, size):

		(self.xSafeBoxStart, self.ySafeBoxStart, z) = player.GetMainCharacterPosition()

		self.SetTableSize(size)
		self.Show()

	def SetTableSize(self, pageCount):
		size = safebox.SAFEBOX_SLOT_Y_COUNT

		self.wndItem.ArrangeSlot(0, safebox.SAFEBOX_SLOT_X_COUNT, size, 32, 32, 0, 0)
		self.wndItem.RefreshSlot()
		self.wndItem.SetSlotBaseImage("d:/ymir work/ui/public/Slot_Base.sub", 1.0, 1.0, 1.0, 1.0)

		self.wndBoard.SetSize(self.BOX_WIDTH, 82 + 32*size)
		self.SetSize(self.BOX_WIDTH, 85 + 32*size)
		self.UpdateRect()

	def RefreshMall(self):
		getItemID=safebox.GetMallItemID
		getItemCount=safebox.GetMallItemCount
		setItemID=self.wndItem.SetItemSlot

		for i in xrange(safebox.GetMallSize()):
			itemID = getItemID(i)
			itemCount = getItemCount(i)
			socket = tuple(safebox.GetMallItemMetinSocket(i, j) for j in range(player.METIN_SOCKET_MAX_NUM))
			if itemCount <= 1:
				itemCount = 0
			setItemID(i, itemID, itemCount, socket=socket)

		self.wndItem.RefreshSlot()

	def SetItemToolTip(self, tooltip):
		self.tooltipItem = tooltip

	def Close(self):
		net.SendChatPacket("/mall_close")
		self.Hide() # @fixme009

	def CommandCloseMall(self):
		if self.tooltipItem:
			self.tooltipItem.HideToolTip()

		self.Hide()

	## Slot Event
	def SelectEmptySlot(self, selectedSlotPos):

		if mouseModule.mouseController.isAttached():

			chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.MALL_CANNOT_INSERT)
			mouseModule.mouseController.DeattachObject()

	def SelectItemSlot(self, selectedSlotPos):

		if mouseModule.mouseController.isAttached():

			chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.MALL_CANNOT_INSERT)
			mouseModule.mouseController.DeattachObject()

		else:

			curCursorNum = app.GetCursor()
			selectedItemID = safebox.GetMallItemID(selectedSlotPos)
			mouseModule.mouseController.AttachObject(self, player.SLOT_TYPE_MALL, selectedSlotPos, selectedItemID)
			snd.PlaySound("sound/ui/pick.wav")

	def UseItemSlot(self, slotIndex):
		mouseModule.mouseController.DeattachObject()
		eventManager.EventManager().send_event(EVENT_QUICK_REMOVE_SAFEBOX_ITEM, "mall", slotIndex)

	def __ShowToolTip(self, slotIndex):
		if self.tooltipItem:
			self.tooltipItem.SetMallItem(slotIndex)

	def OverInItem(self, slotIndex):
		self.__ShowToolTip(slotIndex)

	def OverOutItem(self):
		self.wndItem.SetUsableItem(False)
		if self.tooltipItem:
			self.tooltipItem.HideToolTip()

	def OnPressEscapeKey(self):
		self.Close()
		return True

	def OnUpdate(self):

		USE_SAFEBOX_LIMIT_RANGE = 1000

		(x, y, z) = player.GetMainCharacterPosition()
		if abs(x - self.xSafeBoxStart) > USE_SAFEBOX_LIMIT_RANGE or abs(y - self.ySafeBoxStart) > USE_SAFEBOX_LIMIT_RANGE:
			self.Close()
