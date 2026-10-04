import ui
import player
import mouseModule
import net
import app
import snd
import item
import player
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
import wndMgr
import dbg

import uiShop
import eventManager
import uiSafeBox
import uiExchange
import uiItemExchange
import safebox

import offlineShopManage
import uiPotionRecharge
import uiHorseInventory

if app.ENABLE_CHEQUE_SYSTEM:
	import uiToolTip
	import uiPickETC

if app.ENABLE_ACCE_COSTUME_SYSTEM:
	import acce
	
if app.ENABLE_IKASHOP_RENEWAL:
	import ikashop
	
from _weakref import proxy

EVENT_QUICK_ADD_INVENTORY_ITEM = "EVENT_QUICK_ADD_INVENTORY_ITEM" # args | destination: string, slotNumber: number
EVENT_CLOSE_INVENTORY = "EVENT_CLOSE_INVENTORY"

ITEM_MALL_BUTTON_ENABLE = True

ITEM_FLAG_APPLICABLE = 1 << 14

def GetLocalSlotAndInventoryPageFromGlobalSlot(globalSlot):
	if globalSlot >= player.INVENTORY_DEFAULT_MAX_NUM: # horse inventory
		return (globalSlot, 0)

	inventoryPage = int(globalSlot / player.INVENTORY_PAGE_SIZE)
	firstSlotOnPage = inventoryPage * player.INVENTORY_PAGE_SIZE
	localSlot = globalSlot - firstSlotOnPage
	return (localSlot, inventoryPage)

def GetGlobalSlotFromLocalSlotAndTab(localSlot, tab):
	if localSlot >= player.INVENTORY_DEFAULT_MAX_NUM: # horse inventory
		return localSlot

	return localSlot + player.INVENTORY_PAGE_SIZE * tab

def CanAccessHorseInventory():
	"""Allow horse bags while riding either a horse or a seal mount."""
	try:
		horseLevel = player.GetSkillGrade(109) * 20 + player.GetSkillLevel(109)
		return horseLevel >= 1 and (constInfo.IS_HORSE_SUMMONED or player.IsMountingHorse())
	except:
		return False

class CostumeWindow(ui.ScriptWindow):

	def __init__(self, wndInventory):
		import exception

		if not app.ENABLE_COSTUME_SYSTEM:
			exception.Abort("What do you do?")
			return

		if not wndInventory:
			exception.Abort("wndInventory parameter must be set to InventoryWindow")
			return

		ui.ScriptWindow.__init__(self)

		self.isLoaded = 0
		self.wndInventory = wndInventory;

		self.__LoadWindow()

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	@ui.WindowDestroy
	def Destroy(self):
		if getattr(self, "hideButton", None):
			import uicostumehide
			uicostumehide.RemoveButton(self.hideButton)
			self.hideButton = None
		self.ClearDictionary()
		self.wndInventory = None

	def Show(self):
		self.__LoadWindow()
		if self.isLoaded != 1 or not hasattr(self, "wndEquip"):
			return
		self.RefreshCostumeSlot()

		ui.ScriptWindow.Show(self)

	def Close(self):
		self.Hide()

	def __LoadWindow(self):
		if self.isLoaded == 1:
			return

		self.isLoaded = 1

		try:
			pyScrLoader = ui.PythonScriptLoader()
			pyScrLoader.LoadScriptFile(self, "UIScript/CostumeWindow.py")
		except Exception, e:
			dbg.TraceError("CostumeWindow.LoadWindow.LoadObject: %s" % str(e))
			self.isLoaded = 0
			return

		try:
			wndEquip = self.GetChild("CostumeSlot")
			self.GetChild("TitleBar").SetCloseEvent(ui.__mem_func__(self.Close))

		except Exception, e:
			dbg.TraceError("CostumeWindow.LoadWindow.BindObject: %s" % str(e))
			self.isLoaded = 0
			return

		## Equipment
		wndEquip.SetOverInItemEvent(ui.__mem_func__(self.wndInventory.OverInItem))
		wndEquip.SetOverOutItemEvent(ui.__mem_func__(self.wndInventory.OverOutItem))
		wndEquip.SetUnselectItemSlotEvent(ui.__mem_func__(self.wndInventory.UseItemSlot))
		wndEquip.SetUseSlotEvent(ui.__mem_func__(self.wndInventory.UseItemSlot))
		wndEquip.SetSelectEmptySlotEvent(ui.__mem_func__(self.wndInventory.SelectEmptySlot))
		wndEquip.SetSelectItemSlotEvent(ui.__mem_func__(self.wndInventory.SelectItemSlot))

		self.wndEquip = wndEquip
		self.__AddHideButton()

	# Ukryj kostiumy (uicostumehide.py): a button under the slots; the window
	# and its board grow to hold it.
	def __AddHideButton(self):
		try:
			import uicostumehide
			board = self.GetChild("board")
			width = self.GetWidth()
			height = self.GetHeight()
			self.SetSize(width, height + 26)
			board.SetSize(width, height + 26)
			button = ui.Button()
			button.SetParent(board)
			button.SetUpVisual("d:/ymir work/ui/public/large_button_01.sub")
			button.SetOverVisual("d:/ymir work/ui/public/large_button_02.sub")
			button.SetDownVisual("d:/ymir work/ui/public/large_button_03.sub")
			button.SetPosition((width - button.GetWidth()) / 2, height - 4)
			button.SetEvent(uicostumehide.Toggle)
			button.Show()
			uicostumehide.AddButton(button)
			self.hideButton = button
		except Exception, e:
			dbg.TraceError("CostumeWindow.AddHideButton: %s" % str(e))

	def RefreshCostumeSlot(self):
		getItemVNum=player.GetItemIndex

		for i in xrange(item.COSTUME_SLOT_COUNT):
			slotNumber = item.COSTUME_SLOT_START + i
			self.wndEquip.SetItemSlot(slotNumber, getItemVNum(slotNumber), 0)

		if app.ENABLE_WEAPON_COSTUME_SYSTEM:
			self.wndEquip.SetItemSlot(item.COSTUME_SLOT_WEAPON, getItemVNum(item.COSTUME_SLOT_WEAPON), 0)

		self.wndEquip.RefreshSlot()

class BeltInventoryWindow(ui.ScriptWindow):

	def __init__(self, wndInventory):
		import exception

		if not app.ENABLE_NEW_EQUIPMENT_SYSTEM:
			exception.Abort("What do you do?")
			return

		if not wndInventory:
			exception.Abort("wndInventory parameter must be set to InventoryWindow")
			return

		ui.ScriptWindow.__init__(self)

		self.isLoaded = 0
		self.wndInventory = wndInventory

		self.wndBeltInventoryLayer = None
		self.wndBeltInventorySlot = None
		self.expandBtn = None
		self.minBtn = None

		self.__LoadWindow()

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	@ui.WindowDestroy
	def Destroy(self):
		self.wndInventory = None

	def Show(self, openBeltSlot = False):
		self.__LoadWindow()
		self.RefreshSlot()

		ui.ScriptWindow.Show(self)

		if openBeltSlot:
			self.OpenInventory()
		else:
			self.CloseInventory()

	def Close(self):
		self.Hide()

	def IsOpeningInventory(self):
		if self.wndBeltInventoryLayer:
			return self.wndBeltInventoryLayer.IsShow()
		return False

	def OpenInventory(self):
		self.wndBeltInventoryLayer.Show()
		self.expandBtn.Hide()

		if localeInfo.IsARABIC() == 0:
			self.AdjustPositionAndSize()

	def CloseInventory(self):
		self.wndBeltInventoryLayer.Hide()
		self.expandBtn.Show()

		if localeInfo.IsARABIC() == 0:
			self.AdjustPositionAndSize()

	def GetBasePosition(self):
		x, y = self.wndInventory.GetGlobalPosition()
		# The icon sidebar stands between the inventory and the belt.
		wndSideBar = getattr(self.wndInventory, "wndSideBar", None)
		if wndSideBar:
			x -= wndSideBar.GetLeftOffset()
		return x - 148, y + 241

	def AdjustPositionAndSize(self):
		bx, by = self.GetBasePosition()

		if self.IsOpeningInventory():
			self.SetPosition(bx, by)
			self.SetSize(self.ORIGINAL_WIDTH, self.GetHeight())

		else:
			self.SetPosition(bx + 138, by);
			self.SetSize(10, self.GetHeight())

	def __LoadWindow(self):
		if self.isLoaded == 1:
			return

		self.isLoaded = 1

		try:
			pyScrLoader = ui.PythonScriptLoader()
			pyScrLoader.LoadScriptFile(self, "UIScript/BeltInventoryWindow.py")
		except:
			import exception
			exception.Abort("CostumeWindow.LoadWindow.LoadObject")

		try:
			self.ORIGINAL_WIDTH = self.GetWidth()
			wndBeltInventorySlot = self.GetChild("BeltInventorySlot")
			self.wndBeltInventoryLayer = self.GetChild("BeltInventoryLayer")
			self.expandBtn = self.GetChild("ExpandBtn")
			self.minBtn = self.GetChild("MinimizeBtn")

			self.expandBtn.SetEvent(ui.__mem_func__(self.OpenInventory))
			self.minBtn.SetEvent(ui.__mem_func__(self.CloseInventory))

			if localeInfo.IsARABIC() :
				self.expandBtn.SetPosition(self.expandBtn.GetWidth() - 2, 15)
				self.wndBeltInventoryLayer.SetPosition(self.wndBeltInventoryLayer.GetWidth() - 5, 0)
				self.minBtn.SetPosition(self.minBtn.GetWidth() + 3, 15)

			for i in xrange(item.BELT_INVENTORY_SLOT_COUNT):
				slotNumber = item.BELT_INVENTORY_SLOT_START + i
				wndBeltInventorySlot.SetCoverButton(slotNumber,	"d:/ymir work/ui/game/quest/slot_button_01.sub",\
												"d:/ymir work/ui/game/quest/slot_button_01.sub",\
												"d:/ymir work/ui/game/quest/slot_button_01.sub",\
												"d:/ymir work/ui/game/belt_inventory/slot_disabled.tga", False, False)

		except:
			import exception
			exception.Abort("CostumeWindow.LoadWindow.BindObject")

		## Equipment
		wndBeltInventorySlot.SetOverInItemEvent(ui.__mem_func__(self.wndInventory.OverInItem))
		wndBeltInventorySlot.SetOverOutItemEvent(ui.__mem_func__(self.wndInventory.OverOutItem))
		wndBeltInventorySlot.SetUnselectItemSlotEvent(ui.__mem_func__(self.wndInventory.UseItemSlot))
		wndBeltInventorySlot.SetUseSlotEvent(ui.__mem_func__(self.wndInventory.UseItemSlot))
		wndBeltInventorySlot.SetSelectEmptySlotEvent(ui.__mem_func__(self.wndInventory.SelectEmptySlot))
		wndBeltInventorySlot.SetSelectItemSlotEvent(ui.__mem_func__(self.wndInventory.SelectItemSlot))

		self.wndBeltInventorySlot = wndBeltInventorySlot

	def RefreshSlot(self):
		getItemVNum=player.GetItemIndex

		for i in xrange(item.BELT_INVENTORY_SLOT_COUNT):
			slotNumber = item.BELT_INVENTORY_SLOT_START + i
			self.wndBeltInventorySlot.SetItemSlot(slotNumber, getItemVNum(slotNumber), player.GetItemCount(slotNumber))
			self.wndBeltInventorySlot.SetAlwaysRenderCoverButton(slotNumber, True)

			avail = "0"

			if player.IsAvailableBeltInventoryCell(slotNumber):
				self.wndBeltInventorySlot.EnableCoverButton(slotNumber)
			else:
				self.wndBeltInventorySlot.DisableCoverButton(slotNumber)

		self.wndBeltInventorySlot.RefreshSlot()

# Pasek ikon przy ekwipunku (the operator, 28 September: "zajmij sie tym
# sidebarem z boku ... Towarzysz, Autolowy, Sortowanie autopickup, Kosz,
# Wyszukiwarka sklepow, Battlepass, Kalendarz eventow"). A board on the
# inventory's left, as the reference sidebar has it: one 32x32 button per
# window, opened the way its hotkey opens it. It shows and hides with the
# inventory and follows it wherever it goes - dragged, or put back by
# uiwindowpos. With no room on the left (the inventory at the screen's left
# edge) it stands on the right instead.
#
# It folds (the owner, 30 September: "mozliwosc zwiniecia i rozwiniecia"): a
# tab on the board's outer edge, as the belt window has one, folds the board
# away and leaves only the tab against the inventory; the tab unfolds it
# again. Folded or not is kept for the character with the window positions
# (uiwindowpos, key FOLDED_KEY).
#
# The icons are mt2009_ui/sidebar/<name>_01/02/03.tga (normal/over/down),
# one frame for all eight (the Kolo Fortuny's wheel added later), and the
# tab's arrows mt2009_ui/sidebar/tab_left|tab_right_01/02/03.tga.
SIDEBAR_IMAGE = "mt2009_ui/sidebar/%s_%02d.tga"

class SidebarWindow(ui.Window):
	BUTTON_WIDTH = 32
	BUTTON_HEIGHT = 32
	BUTTON_GAP_X = 16
	BUTTON_GAP_Y = 10

	# The fold tab: beside the first icon, on the board's outer edge.
	TAB_WIDTH = 14
	TAB_HEIGHT = 44
	TAB_Y = 4

	# CP1250, as the tooltips below.
	TOOLTIP_FOLD = "Zwi\xf1 pasek"
	TOOLTIP_UNFOLD = "Rozwi\xf1 pasek"
	FOLDED_KEY = "pasek_boczny_zwiniety"

	# (image, tooltip, handler, keybind action) - the tooltips are CP1250.
	# MT2009_PLUS_VEKIRION_V1 (Autor: Vekirion): the key in brackets is the
	# action's current one (keybind.py, rebound in the Esc menu's "Skroty
	# klawiszowe"), refreshed when it changes; no action, no key shown.
	BUTTONS = (
		("companion", "Towarzysz", "OnClickCompanion", "companion"),
		("autohunt", "Auto\xb3owy", "OnClickAutoHunt", "autohunt"),
		("pickup", "Sortowanie autopickup", "OnClickPickupFilter", "pickup_filter"),
		("trash", "Kosz", "OnClickGarbageBin", "garbage_bin"),
		("shopsearch", "Wyszukiwarka sklep\xf3w", "OnClickShopSearch", "shop_search"),
		("battlepass", "Battle Pass", "OnClickBattlePass", None),
		("calendar", "Kalendarz event\xf3w", "OnClickEventCalendar", "event_calendar"),
		("wheel", "Ko\xb3o Fortuny", "OnClickWheel", "wheel"),
		# MT2009_PLUS_DUNGEON_PANEL_V1: the dungeon panel (uidungeoninfo.py).
		("dungeon", "Wyprawy", "OnClickDungeonInfo", "dungeon_info"),
		# MT2009_PLUS_TP_BOOKMARKS_V1: the saved teleport positions (uitpbookmarks.py).
		("teleport", "Zapisane pozycje", "OnClickTpBookmarks", "tp_bookmarks"),
		# MT2009_PLUS_CLEAR_MISSIONS_V1: the /usunmisje window (uiusunmisje.py).
		("missions", "Usuñ misje", "OnClickClearMissions", None),
		# MT2009_PLUS_WEEKLY_RANKING_V1: the weekly ranking (uiweeklyrank.py).
		("ranking", "Ranking tygodniowy", "OnClickWeeklyRank", "weekly_rank"),
		# MT2009_PLUS_DROP_WIKI_V1: the drop wiki (uidropwiki.py).
		("dropwiki", "Drop wiki", "OnClickDropWiki", "drop_wiki"),
	)

	def __init__(self, wndInventory):
		ui.Window.__init__(self)
		self.AddFlag("float")
		# A frame round the board and the tab: only they take the mouse, and
		# a click on the rest of it (the strip over a folded tab) goes through.
		self.AddFlag("not_pick")
		# Held weakly: the inventory holds this bar, and ui.Window has a
		# __del__, so a cycle through the two would never be collected.
		self.wndInventory = proxy(wndInventory)
		self.lastLayout = None
		self.folded = False
		self.foldedLoaded = False
		self.buttons = []
		self.keybindVersion = -1
		self.board = None
		self.tab = None
		self.__CreateBoard()
		self.__CreateTab()
		self.fullWidth = self.TAB_WIDTH + self.board.GetWidth()

	def __del__(self):
		ui.Window.__del__(self)

	def Destroy(self):
		for button in self.buttons:
			button.Hide()
		self.buttons = []
		if self.tab:
			self.tab.Hide()
			self.tab = None
		if self.board:
			self.board.Hide()
			self.board = None
		self.wndInventory = None
		self.lastLayout = None

	def __CreateBoard(self):
		board = ui.Board()
		board.SetParent(self)
		self.board = board

		y = self.BUTTON_GAP_Y
		for name, text, handler, action in self.BUTTONS:
			button = ui.Button()
			button.SetParent(board)
			button.SetUpVisual(SIDEBAR_IMAGE % (name, 1))
			button.SetOverVisual(SIDEBAR_IMAGE % (name, 2))
			button.SetDownVisual(SIDEBAR_IMAGE % (name, 3))
			button.SetToolTipText(text)
			# The windows behind these are built on their first click; the
			# costume button has MSS32 crash on its click sample in that
			# case, so these open without it too.
			button.disableClickSound = True
			button.SAFE_SetEvent(getattr(self, handler))
			button.SetPosition(self.BUTTON_GAP_X, y)
			button.Show()
			self.buttons.append(button)
			y += self.BUTTON_HEIGHT + self.BUTTON_GAP_Y

		board.SetSize(self.BUTTON_GAP_X + self.BUTTON_WIDTH + self.BUTTON_GAP_X, y)
		board.Show()
		self.__RefreshToolTips()

	# "Kosz (J)": the key each window has now (keybind.py).
	def __RefreshToolTips(self):
		try:
			import keybind
			version = keybind.GetVersion()
		except Exception:
			return
		if version == self.keybindVersion:
			return
		self.keybindVersion = version
		for button, (name, text, handler, action) in zip(self.buttons, self.BUTTONS):
			if action:
				button.SetToolTipText(keybind.DecorateLabel(text, action))

	def __CreateTab(self):
		tab = ui.Button()
		tab.SetParent(self)
		tab.disableClickSound = True
		tab.SAFE_SetEvent(self.OnClickFold)
		self.tab = tab
		self.__SetTabArrow("left")
		tab.SetToolTipText(self.TOOLTIP_FOLD)
		tab.Show()

	def __SetTabArrow(self, direction):
		name = "tab_" + direction
		self.tab.SetUpVisual(SIDEBAR_IMAGE % (name, 1))
		self.tab.SetOverVisual(SIDEBAR_IMAGE % (name, 2))
		self.tab.SetDownVisual(SIDEBAR_IMAGE % (name, 3))

	# Folded or not, as the character left it. Read once there is a
	# character to read it for (uiwindowpos keeps one file per character).
	def __LoadFolded(self):
		if self.foldedLoaded:
			return
		try:
			import uiwindowpos
			value = uiwindowpos.GetValue(self.FOLDED_KEY, 0)
		except Exception:
			value = None
		if value is None:
			return
		self.folded = bool(value)
		self.foldedLoaded = True

	def IsFolded(self):
		self.__LoadFolded()
		return self.folded

	def Show(self):
		self.__LoadFolded()
		ui.Window.Show(self)
		self.AdjustPosition()

	def Close(self):
		self.Hide()

	def __GetInventoryRect(self):
		try:
			x, y = self.wndInventory.GetGlobalPosition()
			return x, y, self.wndInventory.GetWidth()
		except (ReferenceError, AttributeError):
			return None

	# The side is chosen by the unfolded width, so folding and unfolding
	# never move the bar across the inventory.
	def __IsOnLeft(self, x):
		return x - self.fullWidth >= 0

	# How far the bar reaches out on the inventory's left: the board and its
	# tab, the tab alone when folded, or nothing when it stands on the right.
	# The belt window hangs past it.
	def GetLeftOffset(self):
		self.__LoadFolded()
		rect = self.__GetInventoryRect()
		if rect is None or not self.__IsOnLeft(rect[0]):
			return 0
		if self.folded:
			return self.TAB_WIDTH
		return self.fullWidth

	def __GetLayout(self):
		rect = self.__GetInventoryRect()
		if rect is None:
			return None
		return rect + (self.__IsOnLeft(rect[0]), self.folded)

	def AdjustPosition(self):
		layout = self.__GetLayout()
		if layout is None or not self.board or not self.tab:
			return
		x, y, width, onLeft, folded = layout
		boardWidth = self.board.GetWidth()

		if folded:
			self.board.Hide()
			self.SetSize(self.TAB_WIDTH, self.TAB_Y + self.TAB_HEIGHT)
			self.tab.SetPosition(0, self.TAB_Y)
			frameWidth = self.TAB_WIDTH
		else:
			self.SetSize(self.fullWidth, max(self.board.GetHeight(), self.TAB_Y + self.TAB_HEIGHT))
			if onLeft:
				self.tab.SetPosition(0, self.TAB_Y)
				self.board.SetPosition(self.TAB_WIDTH, 0)
			else:
				self.board.SetPosition(0, 0)
				self.tab.SetPosition(boardWidth, self.TAB_Y)
			self.board.Show()
			frameWidth = self.fullWidth

		if onLeft:
			self.SetPosition(x - frameWidth, y)
		else:
			self.SetPosition(x + width, y)

		# The arrow points where the board goes: towards the inventory to
		# fold it, away from it to unfold it.
		if onLeft == folded:
			self.__SetTabArrow("left")
		else:
			self.__SetTabArrow("right")
		if folded:
			self.tab.SetToolTipText(self.TOOLTIP_UNFOLD)
		else:
			self.tab.SetToolTipText(self.TOOLTIP_FOLD)

		self.lastLayout = layout

	def OnClickFold(self):
		self.__LoadFolded()
		self.folded = not self.folded
		try:
			import uiwindowpos
			uiwindowpos.SetValue(self.FOLDED_KEY, 1 if self.folded else 0)
		except Exception:
			pass
		self.AdjustPosition()
		# The belt hangs past the bar and moves with it.
		try:
			wndBelt = getattr(self.wndInventory, "wndBelt", None)
			if wndBelt and wndBelt.IsShow():
				wndBelt.AdjustPositionAndSize()
		except ReferenceError:
			pass

	def OnUpdate(self):
		# The inventory is moved by more than a drag (uiwindowpos restores it
		# after its Show); the bar keeps up with it every frame.
		try:
			if not self.wndInventory.IsShow():
				self.Hide()
				return
		except (ReferenceError, AttributeError):
			return
		if self.__GetLayout() != self.lastLayout:
			self.AdjustPosition()
		self.__RefreshToolTips()

	def __GetInterface(self):
		try:
			return getattr(self.wndInventory, "interface", None)
		except ReferenceError:
			return None

	# Towarzysz - as the taskbar's button and the P key open it.
	def OnClickCompanion(self):
		import uisidekick
		uisidekick.ToggleWindow()

	# Autolowy - as the K key and the taskbar's button.
	def OnClickAutoHunt(self):
		import uiautohunt
		uiautohunt.ToggleWindow()

	# Filtr podnoszenia (Ctrl+Z).
	def OnClickPickupFilter(self):
		import uipickupfilter
		uipickupfilter.ToggleWindow()

	# Kosz (J).
	def OnClickGarbageBin(self):
		interface = self.__GetInterface()
		if interface is not None:
			interface.ToggleGarbageBinWindow()

	# Wyszukiwarka sklepow - opened and closed as F5 does (game.py).
	def OnClickShopSearch(self):
		interface = self.__GetInterface()
		search = getattr(interface, "offlineShopSearch", None) if interface is not None else None
		if search is None:
			return
		if search.IsShow():
			search.Close()
		else:
			search.Open()

	# Battle Pass - as the taskbar's button.
	def OnClickBattlePass(self):
		import uibattlepass
		uibattlepass.ToggleWindow()

	# Kalendarz eventow - as F11 and the taskbar's button.
	def OnClickEventCalendar(self):
		import uieventcalendar
		uieventcalendar.ToggleWindow()

	# Kolo Fortuny - as the F12 key (uiwheel.py, "/kolo").
	def OnClickWheel(self):
		import uiwheel
		uiwheel.ToggleWindow()

	def OnClickDungeonInfo(self):
		import uidungeoninfo
		uidungeoninfo.ToggleWindow()

	def OnClickClearMissions(self):
		# MT2009_PLUS_CLEAR_MISSIONS_V1: the same as typing /usunmisje.
		import warpsafe, net
		if warpsafe.InGame():
			net.SendChatPacket("/usunmisje")

	def OnClickTpBookmarks(self):
		import uitpbookmarks
		uitpbookmarks.ToggleWindow()

	def OnClickWeeklyRank(self):
		import uiweeklyrank
		uiweeklyrank.ToggleWindow()

	def OnClickDropWiki(self):
		import uidropwiki
		uidropwiki.ToggleWindow()

class GridSlotStateManager():
	SLOT_STATE_NONE = 0
	SLOT_STATE_IN_USE = 1
	SLOT_STATE_UNUSABLE = 2
	SLOT_STATE_TO_BE_SOLD = 3
	SLOT_STATE_NEW_ITEM = 4

	SLOT_MASK_COLOR_TO_BE_SOLD = (1, 1, 0, 0.3)

	def __init__(self, slotWindow, x, y):
		self.slotStates = [self.SLOT_STATE_NONE for i in range(x*y)]
		self.slotWindow = slotWindow
		self.secondSlotWindow = slotWindow

	def __del__(self):
		self.slotStates = []
		self.slotWindow = None

	def GetSlotWindow(self, globalSlot):
		if self.secondSlotWindow and globalSlot >= player.INVENTORY_DEFAULT_MAX_NUM:
			return self.secondSlotWindow
		return self.slotWindow

	def HideNonTradableItemSlots(self):
		pass

	def SetSlotState(self, slotIndex, state):
		if self.slotStates[slotIndex] != self.SLOT_STATE_NONE:
			self.MarkItemClean(slotIndex)

		self.slotStates[slotIndex] = state

	def MarkItemUnusable(self, slotIndex):
		self.SetSlotState(slotIndex, self.SLOT_STATE_UNUSABLE)
		self.GetSlotWindow(slotIndex).SetCantMouseEventSlot(slotIndex)

	def MarkItemInUse(self, slotIndex):
		self.SetSlotState(slotIndex, self.SLOT_STATE_IN_USE)
		self.GetSlotWindow(slotIndex).SetUnusableSlot(slotIndex)

	def MarkItemClean(self, globalSlotIndex):
		self.slotStates[globalSlotIndex] = self.SLOT_STATE_NONE

		(localSlot, page) = GetLocalSlotAndInventoryPageFromGlobalSlot(globalSlotIndex)
		self.GetSlotWindow(globalSlotIndex).SetCanMouseEventSlot(localSlot)
		self.GetSlotWindow(globalSlotIndex).SetUsableSlot(localSlot)
		self.GetSlotWindow(globalSlotIndex).DeactivateSlot(localSlot)

	def ClearSlotStates(self, *checkState):
		slotCount = len(self.slotStates)

		if len(checkState) > 0:
			for i in range(slotCount):
				if self.slotStates[i] in checkState:
					self.slotStates[i] = self.SLOT_STATE_NONE
		else:
			self.slotStates = [self.SLOT_STATE_NONE for i in range(slotCount)]
		self.RefreshAllSlots()

	def RefreshAllSlots(self):
		for i in range(len(self.slotStates)):
			self.RefreshSlotState(i, i, False)
		self.RefreshSlotWindows()

	# MT2009_PLUS_SLOT_REFRESH_ONCE_V1: a slot window's RefreshSlot also
	# replays the mouse-over of the slot under the cursor (CSlotWindow::
	# RefreshSlot: OnOverOutItem + OnOverInItem), i.e. the whole item tooltip
	# is built anew. Called for every cell it rebuilt the tooltip ~90 times
	# per inventory refresh while the cursor was over an item - the short
	# client lag after every enchant (the cursor is always on the item then).
	# The states are set cell by cell and each window is refreshed once.
	def RefreshSlotWindows(self):
		refreshed = []
		for wnd in (self.slotWindow, self.secondSlotWindow):
			if wnd and wnd not in refreshed:
				refreshed.append(wnd)
				wnd.RefreshSlot()

	def RefreshSlotState(self, realSlotIndex, localSlotIndex=0, refresh=True):
		state = self.slotStates[realSlotIndex]

		if realSlotIndex >= player.INVENTORY_DEFAULT_MAX_NUM:
			localSlotIndex = realSlotIndex

		if state == self.SLOT_STATE_NONE:
			self.GetSlotWindow(realSlotIndex).SetCanMouseEventSlot(localSlotIndex)
			self.GetSlotWindow(realSlotIndex).SetUsableSlot(localSlotIndex)
		elif state == self.SLOT_STATE_UNUSABLE:
			self.GetSlotWindow(realSlotIndex).SetCantMouseEventSlot(localSlotIndex)
		elif state == self.SLOT_STATE_IN_USE:
			self.GetSlotWindow(realSlotIndex).SetUnusableSlot(localSlotIndex)
		elif state == self.SLOT_STATE_TO_BE_SOLD:
			self.GetSlotWindow(realSlotIndex).SetSlotMaskColor(localSlotIndex, self.SLOT_MASK_COLOR_TO_BE_SOLD)
		elif state == self.SLOT_STATE_NEW_ITEM:
			self.GetSlotWindow(realSlotIndex).ActivateSlot(localSlotIndex)

		if refresh:
			self.GetSlotWindow(realSlotIndex).RefreshSlot()

	def OnSlotMouseOverIn(self, slot):
		pass

	def OnSlotMouseOverOut(self, slot):
		pass

class InventorySlotManager(GridSlotStateManager):
	def __init__(self, slotWindow):
		GridSlotStateManager.__init__(self, slotWindow, player.INVENTORY_PAGE_SIZE, player.INVENTORY_PAGE_COUNT)
		self.secondSlotWindow = None
		self.tab = 0

		self.isMyShopManageOpen = False

		# What the open windows refuse (SetItemSlotLimit) and the cells
		# marked for it, so that the marks follow the items.
		self.slotLimits = []
		self.limitedSlots = {}

		eventManager.EventManager().add_observer(uiShop.EVENT_ADD_MASS_SELL, self.OnMassSellItemAdd)
		eventManager.EventManager().add_observer(uiShop.EVENT_STOP_MASS_SELL, self.OnMassSellStop)

		eventManager.EventManager().add_observer(uiAttachMetin.EVENT_ATTACH_METIN_OPEN, self.OnAttachMetin)
		eventManager.EventManager().add_observer(uiAttachMetin.EVENT_ATTACH_METIN_CLOSE, self.OnRefineClose)

		eventManager.EventManager().add_observer(uiRefine.EVENT_OPEN_REFINE, self.OnRefineItem)
		eventManager.EventManager().add_observer(uiRefine.EVENT_CLOSE_REFINE, self.OnRefineClose)

		eventManager.EventManager().add_observer(uiExchange.EVENT_OPEN_EXCHANGE, self.SetItemSlotLimit_Exchange)
		eventManager.EventManager().add_observer(uiExchange.EVENT_ADD_ITEM_TO_EXCHANGE, self.OnAddItemToExchange)
		eventManager.EventManager().add_observer(uiExchange.EVENT_CLOSE_EXCHANGE, self.OnExchangeClose)

		eventManager.EventManager().add_observer(uiItemExchange.EVENT_ADD_ITEM_TO_ITEM_EXCHANGE, self.OnAddItemToItemExchange)
		eventManager.EventManager().add_observer(uiItemExchange.EVENT_REMOVE_ITEM_EXCHANGE, self.OnRemoveItemFromItemExchange)
		eventManager.EventManager().add_observer(uiItemExchange.EVENT_ITEM_EXCHANGE_CLOSE, self.OnExchangeItemClose)

		eventManager.EventManager().add_observer(uiPotionRecharge.EVENT_RECHARGE_ADD_AUTOPOTION_SLOT,self.OnAddItemToItemExchange)
		eventManager.EventManager().add_observer(uiPotionRecharge.EVENT_RECHARGE_ADD_ITEM_SLOT,self.OnAddItemToItemExchange)
		eventManager.EventManager().add_observer(uiPotionRecharge.EVENT_RECHARGE_REMOVE_ITEM,self.OnRemoveItemFromItemExchange)
		eventManager.EventManager().add_observer(uiPotionRecharge.EVENT_RECHARGE_POTION_CLOSE, self.OnExchangeItemClose)

		eventManager.EventManager().add_observer(uiSafeBox.EVENT_OPEN_SAFEBOX, self.SetItemSlotLimit_Safebox)
		eventManager.EventManager().add_observer(uiSafeBox.EVENT_CLOSE_SAFEBOX, self.OnSafeboxClose)

		eventManager.EventManager().add_observer(offlineShopManage.EVENT_OPEN_MYSHOP_SHOP_MANAGE, self.SetItemSlotLimit_PlayerShop)
		eventManager.EventManager().add_observer(offlineShopManage.EVENT_CLOSE_MYSHOP_SHOP_MANAGE, self.__ClearNonTradableSlots)

		eventManager.EventManager().add_observer(uiShop.EVENT_OPEN_SHOP_DIALOG, self.SetItemSlotLimit_NPCShop)
		eventManager.EventManager().add_observer(uiShop.EVENT_CLOSE_SHOP_DIALOG, self.__ClearSlotsOnShopDialogClose)

		eventManager.EventManager().add_observer(eventManager.OPEN_PRIVATE_SHOP_BUILDER, self.SetItemSlotLimit_PlayerShop)
		eventManager.EventManager().add_observer(eventManager.CLOSE_PRIVATE_SHOP_BUILDER, self.OnExchangeClose)

		eventManager.EventManager().add_observer(eventManager.ADD_ITEM_TO_SHOP_BUILDER, self.OnAddItemToShopBuilder)
		eventManager.EventManager().add_observer(eventManager.REMOVE_ITEM_FROM_SHOP_BUILDER, self.OnRemoveItemFromShopBuilder)

	def __del__(self):
		GridSlotStateManager.__del__(self)

	def OnMassSellItemAdd(self, sourceSlotPos, sourceWindowType):
		(localSlot, page) = GetLocalSlotAndInventoryPageFromGlobalSlot(sourceSlotPos)
		if self.slotStates[sourceSlotPos] == self.SLOT_STATE_TO_BE_SOLD:
			self.slotStates[sourceSlotPos] = self.SLOT_STATE_NONE
			self.GetSlotWindow(sourceSlotPos).SetSlotMaskNormalColor(localSlot)
			return
		else:
			self.slotStates[sourceSlotPos] = self.SLOT_STATE_TO_BE_SOLD
			self.GetSlotWindow(sourceSlotPos).SetSlotMaskColor(localSlot, self.SLOT_MASK_COLOR_TO_BE_SOLD)

	def OnMassSellItemRemove(self, sourceSlotPos, sourceWindowType):
		(localSlot, page) = GetLocalSlotAndInventoryPageFromGlobalSlot(sourceSlotPos)

	def ClearSlotStates(self, *checkState):
		# Every window that sets a limit clears all the marks as it
		# closes, so the limits end there too.
		if not checkState or self.SLOT_STATE_UNUSABLE in checkState:
			self.slotLimits = []
			self.limitedSlots = {}
		GridSlotStateManager.ClearSlotStates(self, *checkState)

	def OnRefineClose(self):
		self.ClearSlotStates(self.SLOT_STATE_UNUSABLE)

	def OnSafeboxClose(self):
		self.ClearSlotStates(self.SLOT_STATE_UNUSABLE)

	def OnExchangeItemClose(self): # nie handel
		self.ClearSlotStates(self.SLOT_STATE_IN_USE)

	def OnExchangeClose(self): # handel
		self.ClearSlotStates(self.SLOT_STATE_UNUSABLE, self.SLOT_STATE_IN_USE)

	def OnMassSellStop(self):
		self.ClearSlotStates(self.SLOT_STATE_TO_BE_SOLD)

	def __ClearNonTradableSlots(self):
		self.ClearSlotStates(self.SLOT_STATE_UNUSABLE)

	def __ClearSlotsOnShopDialogClose(self):
		self.ClearSlotStates(self.SLOT_STATE_UNUSABLE, self.SLOT_STATE_TO_BE_SOLD)

	def OnAttachMetin(self, itemSlotPos, metinSlotPos):
		(localSlot1, page) = GetLocalSlotAndInventoryPageFromGlobalSlot(itemSlotPos)
		(localSlot2, page2) = GetLocalSlotAndInventoryPageFromGlobalSlot(metinSlotPos)

		self.slotStates[itemSlotPos] = self.SLOT_STATE_UNUSABLE
		self.slotStates[metinSlotPos] = self.SLOT_STATE_UNUSABLE

		if self.tab == page:
			self.GetSlotWindow(itemSlotPos).SetCantMouseEventSlot(localSlot1)

		if self.tab == page2:
			self.GetSlotWindow(metinSlotPos).SetCantMouseEventSlot(localSlot2)

	def OnRefineItem(self, itemSlotPos):
		self.MarkItemUnusable(itemSlotPos)

	def OnAddItemToItemExchange(self, sourceSlotPos): # nie handel
		self.MarkItemInUse(sourceSlotPos)

	def OnRemoveItemFromItemExchange(self, sourceSlotPos): # nie handel
		self.MarkItemClean(sourceSlotPos)

	def OnAddItemToExchange(self, sourceWindowType, sourceSlotPos): # handel
		self.MarkItemInUse(sourceSlotPos)

	def OnAddItemToShopBuilder(self, sourceSlotPos, sourceWindowType):
		self.MarkItemInUse(sourceSlotPos)

	def OnRemoveItemFromShopBuilder(self, sourceSlotPos, sourceWindowType):
		self.MarkItemClean(sourceSlotPos)

	def SetItemSlotLimit_NPCShop(self):
		self.SetItemSlotLimit(item.ITEM_ANTIFLAG_SELL)

	def SetItemSlotLimit_PlayerShop(self):
		self.SetItemSlotLimit(item.ITEM_ANTIFLAG_MYSHOP)

	def SetItemSlotLimit_Safebox(self):
		self.SetItemSlotLimit(item.ITEM_ANTIFLAG_SAFEBOX)

	def SetItemSlotLimit_Exchange(self):
		self.SetItemSlotLimit(item.ITEM_ANTIFLAG_GIVE)

	def SetItemSlotLimit(self, antiflag):
		if antiflag not in self.slotLimits:
			self.slotLimits.append(antiflag)
		for slot in range(player.INVENTORY_PAGE_SIZE * player.INVENTORY_PAGE_COUNT):
			isAntiflag = player.IsAntiFlagBySlot(slot, antiflag)
			if isAntiflag:
				self.SetSlotState(slot, self.SLOT_STATE_UNUSABLE)
				self.limitedSlots[slot] = True

		self.RefreshAllSlots()

	def SetTab(self, tab):
		self.tab = tab

	def MarkItemUnusable(self, globalSlotIndex):
		self.SetSlotState(globalSlotIndex, self.SLOT_STATE_UNUSABLE)

		localSlot, page = GetLocalSlotAndInventoryPageFromGlobalSlot(globalSlotIndex)
		self.GetSlotWindow(globalSlotIndex).SetCantMouseEventSlot(localSlot)

	def MarkItemInUse(self, globalSlotIndex):
		self.SetSlotState(globalSlotIndex, self.SLOT_STATE_IN_USE)

		localSlot, page = GetLocalSlotAndInventoryPageFromGlobalSlot(globalSlotIndex)
		self.GetSlotWindow(globalSlotIndex).SetUnusableSlot(localSlot)

	def MarkItemClean(self, slotIndex):
		self.slotStates[slotIndex] = self.SLOT_STATE_NONE
		self.GetSlotWindow(slotIndex).SetCanMouseEventSlot(slotIndex)
		self.GetSlotWindow(slotIndex).SetUsableSlot(slotIndex)
		self.GetSlotWindow(slotIndex).DeactivateSlot(slotIndex)

	def FollowSlotLimits(self, slot):
		# A cell is marked while it holds an item an open window refuses,
		# and a cell this marked is freed once it no longer does.
		if not self.slotLimits or slot >= len(self.slotStates):
			return
		limited = False
		for antiflag in self.slotLimits:
			if player.IsAntiFlagBySlot(slot, antiflag):
				limited = True
				break
		state = self.slotStates[slot]
		if limited:
			if state in (self.SLOT_STATE_NONE, self.SLOT_STATE_NEW_ITEM):
				self.slotStates[slot] = self.SLOT_STATE_UNUSABLE
				self.limitedSlots[slot] = True
		elif slot in self.limitedSlots:
			del self.limitedSlots[slot]
			if state == self.SLOT_STATE_UNUSABLE:
				self.slotStates[slot] = self.SLOT_STATE_NONE

	def RefreshAllSlots(self):
		for i in range(player.INVENTORY_PAGE_SIZE):
			realSlot = player.INVENTORY_PAGE_SIZE * self.tab + i
			self.FollowSlotLimits(realSlot)
			self.RefreshSlotState(realSlot, i, False)

		for i in range(player.INVENTORY_PAGE_SIZE):
			realSlot = player.INVENTORY_DEFAULT_MAX_NUM + i
			self.FollowSlotLimits(realSlot)
			self.RefreshSlotState(realSlot, i, False)

		self.RefreshSlotWindows() # MT2009_PLUS_SLOT_REFRESH_ONCE_V1

	def HighlightSlot(self, inventorySlot):
		if inventorySlot < len(self.slotStates):
			self.slotStates[inventorySlot] = self.SLOT_STATE_NEW_ITEM

	def OnSlotMouseOverIn(self, slot):
		if slot >= len(self.slotStates):
			return

		globalSlot = GetGlobalSlotFromLocalSlotAndTab(slot, self.tab)
		slotState = self.slotStates[globalSlot]
		if slotState == self.SLOT_STATE_NEW_ITEM:
			self.slotStates[globalSlot] = self.SLOT_STATE_NONE
			self.GetSlotWindow(globalSlot).DeactivateSlot(slot)

# MT2009_PLUS_VEKIRION_V1 (Autor: Vekirion) - quick box opening, his
# MT2009_PLUS_OPEN_ALL_V3: Ctrl + right click on a stack of boxes that open
# by a plain use (item type GIFTBOX: the Cor Draconis and the other chests)
# opens up to OPEN_LIMIT of them, from that stack and then the other stacks
# of the same item. The server takes at most 60 box uses per 500 ms
# (server-patches/vekirion, CHARACTER::UseItem; every other item keeps the
# engine's 5) and silently drops the rest: so uses go out at most RATE_COUNT
# per RATE_WINDOW seconds (under the server's limit even with some lag), up to
# IN_FLIGHT unanswered at a time, and uses not answered in LOST_AFTER are
# taken as dropped and sent again. It stops when the limit is reached or
# none is left, when MAX_RETRIES rounds in a row get no answer (the server
# says why in the chat: no 3 free slots, the Alchemy quest not done...),
# when an item is
# held on the cursor or a question window is open, on a Ctrl + right click
# again, and - for a Cor Draconis - before a stone would have no room in the
# Alchemy inventory (the server would drop it on the ground). Boxes that ask
# before use are not opened.
OPEN_ALL_START = "Otwieranie: %d szt. (Ctrl + PPM ponownie - stop)"
OPEN_ALL_DONE = "Otwarto: %d szt."
OPEN_ALL_STOPPED = "Otwieranie przerwane, otwarto: %d szt."
OPEN_ALL_DS_FULL = "Brak miejsca w Alchemii (zak\xb3adka %d, strona %d), otwarto: %d szt."
# the Cor Draconis whose stones are of one grade (special_item_group.cors.txt);
# any other Cor is checked against all the grade pages it could fill
COR_DRACONIS_GRADES = {
	50255 : (0,), 51501 : (0,), 51502 : (0,),
	50256 : (1,), 51507 : (1,),
	50257 : (2,), 51508 : (2,),
	50258 : (3,), 51509 : (3,),
	50259 : (4,), 51510 : (4,),
}

def IsCorDraconisVnum(vnum):
	return 50252 == vnum or (50255 <= vnum and vnum <= 50260) or (51501 <= vnum and vnum <= 51699)

def InventoryUsableSize():
	return getattr(player, "INVENTORY_DEFAULT_MAX_NUM", player.INVENTORY_MAX_NUM)

def IsInventoryBusy():
	return mouseModule.mouseController.isAttached() or constInfo.GET_ITEM_QUESTION_DIALOG_STATUS() \
		or uiPrivateShopBuilder.IsBuildingPrivateShop()

class ItemOpenAllRunner(ui.Window):
	OPEN_LIMIT = 50
	IN_FLIGHT = 50
	RATE_COUNT = 50
	RATE_WINDOW = 0.5
	LOST_AFTER = 0.8
	MAX_RETRIES = 2
	DS_KIND_COUNT = 7

	def __init__(self):
		ui.Window.__init__(self)
		self.__Reset()

	def __del__(self):
		ui.Window.__del__(self)

	def __Reset(self):
		self.vnum = 0
		self.slot = -1
		self.slotCount = 0
		self.inFlight = 0
		self.opened = 0
		self.target = 0
		self.lastProgress = 0.0
		self.sendTimes = []
		self.retries = 0

	def IsRunning(self):
		return 0 != self.vnum

	def __CountAll(self, vnum):
		total = 0
		for i in xrange(InventoryUsableSize()):
			if player.GetItemIndex(i) == vnum:
				total += player.GetItemCount(i)
		return total

	def Start(self, slot):
		vnum = player.GetItemIndex(slot)
		if not vnum:
			return False
		item.SelectItem(vnum)
		if item.GetItemType() != getattr(item, "ITEM_TYPE_GIFTBOX", 23):
			return False
		if item.IsFlag(item.ITEM_FLAG_CONFIRM_WHEN_USE):
			return False
		total = max(self.__CountAll(vnum), player.GetItemCount(slot))
		if total <= 0:
			return False

		self.__Reset()
		self.vnum = vnum
		self.slot = slot
		self.slotCount = player.GetItemCount(slot)
		self.target = min(total, self.OPEN_LIMIT)
		chat.AppendChat(chat.CHAT_TYPE_INFO, OPEN_ALL_START % self.target)
		self.Show()
		return True

	def Stop(self, message = None):
		if not self.IsRunning():
			return
		if None == message:
			message = OPEN_ALL_STOPPED % self.opened
		self.__Reset()
		self.Hide()
		chat.AppendChat(chat.CHAT_TYPE_INFO, message)

	def __FindSlot(self):
		if player.GetItemIndex(self.slot) == self.vnum and player.GetItemCount(self.slot) > 0:
			return self.slot
		for i in xrange(InventoryUsableSize()):
			if player.GetItemIndex(i) == self.vnum and player.GetItemCount(i) > 0:
				return i
		return -1

	# (free slots, tab, page) of the fullest Alchemy page a stone of this
	# Cor could land on (tab and page counted from 1), or None for other boxes
	def __TightestDragonSoulPage(self):
		if not IsCorDraconisVnum(self.vnum):
			return None
		try:
			if app.ENABLE_DS_GRADE_MYTH:
				pageCount = player.DRAGON_SOUL_PAGE_COUNT
			else:
				pageCount = 5
			pageSize = player.DRAGON_SOUL_PAGE_SIZE
		except Exception:
			return None
		tightest = None
		for kind in xrange(self.DS_KIND_COUNT):
			for grade in COR_DRACONIS_GRADES.get(self.vnum, (0, 1, 2, 3, 4)):
				base = (kind * pageCount + grade) * pageSize
				free = 0
				for i in xrange(pageSize):
					if 0 == player.GetItemIndex(player.DRAGON_SOUL_INVENTORY, base + i):
						free += 1
						if free > self.IN_FLIGHT:
							break
				if None == tightest or free < tightest[0]:
					tightest = (free, kind + 1, grade + 1)
		return tightest

	def OnUpdate(self):
		if not self.IsRunning():
			return
		# MT2009_PLUS_VEKIRION_V1: no use goes out on the way to another core
		# (warpsafe.py); a warp ends the run.
		if not __import__("warpsafe").InGame():
			self.Stop()
			return
		now = app.GetTime()

		# what the server took since the last frame
		if player.GetItemIndex(self.slot) == self.vnum:
			count = player.GetItemCount(self.slot)
		else:
			count = 0
		if count < self.slotCount:
			# counted even when late, after its use was taken as dropped
			taken = self.slotCount - count
			self.opened += taken
			self.inFlight = max(0, self.inFlight - taken)
			self.lastProgress = now
			self.retries = 0
		if 0 == count:
			self.inFlight = 0	# uses past the end of the stack fall through on the server
		self.slotCount = count

		# uses the server dropped (over its rate limit) are sent again
		if self.inFlight > 0 and now - self.lastProgress > self.LOST_AFTER:
			self.retries += 1
			if self.retries > self.MAX_RETRIES:
				self.Stop()
				return
			self.inFlight = 0
			self.lastProgress = now
		if self.opened >= self.target:
			self.Stop(OPEN_ALL_DONE % self.opened)
			return
		if IsInventoryBusy():
			self.Stop()
			return

		tightest = self.__TightestDragonSoulPage()
		self.sendTimes = [t for t in self.sendTimes if now - t < self.RATE_WINDOW]
		while self.opened + self.inFlight < self.target and self.inFlight < self.IN_FLIGHT \
				and len(self.sendTimes) < self.RATE_COUNT:
			if 0 == self.inFlight and 0 == self.slotCount:
				slot = self.__FindSlot()
				if slot < 0:
					self.Stop(OPEN_ALL_DONE % self.opened)
					return
				self.slot = slot
				self.slotCount = player.GetItemCount(slot)
			if self.inFlight >= self.slotCount:
				break
			if tightest and tightest[0] <= self.inFlight:
				if 0 == self.inFlight:
					self.Stop(OPEN_ALL_DS_FULL % (tightest[1], tightest[2], self.opened))
					return
				break
			if 0 == self.inFlight:
				self.lastProgress = now
			self.inFlight += 1
			self.sendTimes.append(now)
			net.SendItemUsePacket(self.slot)

# MT2009_PLUS_VEKIRION_V1 (Autor: Vekirion), his
# MT2009_PLUS_SPLIT_PACKS_V1: the Shift + left click window gets a second
# field, "Paczki po:" (pack size). Left empty, the window picks up items as
# before. Filled with Y, it splits the stack into packs of Y moved to free
# slots (the current inventory page first, then the others), the rest
# staying in the original slot: 100 by 15 gives 6 packs of 15 and 10 left.
# The upper field then is the number of packs wanted; 1 there (its default)
# means as many as the stack and the free slots allow. Up to IN_FLIGHT moves
# are on their way at a time, each to its own free slot; it stops when done,
# when no free slot is left, or when the server takes no move in
# ANSWER_TIMEOUT.
SPLIT_LABEL = "Paczki po:"
SPLIT_START = "Dzielenie: %d paczek po %d szt."
SPLIT_DONE = "Podzielono: %d paczek po %d szt."
SPLIT_NO_SPACE = "Brak wolnego miejsca w ekwipunku, podzielono: %d paczek po %d szt."
SPLIT_STOPPED = "Dzielenie przerwane, podzielono: %d paczek po %d szt."
SPLIT_TOO_BIG = "Paczka musi by\xe6 mniejsza ni\xbf ca\xb3y stos (%d szt.)."
INVENTORY_COLUMNS = 5

def AddSplitPackRow(dlg):
	try:
		rowY = dlg.acceptButton.GetLocalPosition()[1]
		dy = 26
		dlg.SetSize(dlg.GetWidth(), dlg.GetHeight() + dy)
		dlg.board.SetSize(dlg.GetWidth(), dlg.GetHeight())
		for button in (dlg.acceptButton, dlg.cancelButton):
			(x, y) = button.GetLocalPosition()
			button.SetPosition(x, y + dy)

		slot = ui.ImageBox()
		slot.SetParent(dlg.board)
		slot.LoadImage("d:/ymir work/ui/public/Parameter_Slot_02.sub")
		slotX = max(85, dlg.GetWidth() - 20 - slot.GetWidth())
		slot.SetPosition(slotX, rowY)
		slot.Show()

		label = ui.TextLine()
		label.SetParent(dlg.board)
		label.SetPosition(20, rowY + 3)
		label.SetText(SPLIT_LABEL)
		label.Show()

		edit = ui.EditLine()
		edit.SetParent(slot)
		edit.SetSize(max(30, slot.GetWidth() - 6), 18)
		edit.SetPosition(3, 2)
		edit.SetMax(6)
		edit.SetNumberMode()
		edit.SetText("")
		edit.Show()

		# the second field must lose the keyboard however the window closes
		main = dlg.pickValueEditLine
		def CloseBoth():
			edit.KillFocus()
			dlg.Close()
		def AcceptBoth():
			edit.KillFocus()
			dlg.OnAccept()
		edit.SetReturnEvent(AcceptBoth)
		edit.SetEscapeEvent(CloseBoth)
		edit.SetTabEvent(lambda: main.SetFocus())
		main.SetReturnEvent(AcceptBoth)
		main.SetEscapeEvent(CloseBoth)
		main.SetTabEvent(lambda: edit.SetFocus())
		dlg.acceptButton.SetEvent(AcceptBoth)
		dlg.cancelButton.SetEvent(CloseBoth)
		dlg.board.SetCloseEvent(CloseBoth)

		dlg.splitPackSlot = slot
		dlg.splitPackLabel = label
		dlg.splitPackEditLine = edit
	except Exception, e:
		dbg.TraceError("Exception : AddSplitPackRow, %s" % e)

def GetSplitPackSize(dlg):
	edit = getattr(dlg, "splitPackEditLine", None)
	if not edit:
		return 0
	text = edit.GetText()
	if text and text.isdigit():
		return int(text)
	return 0

def ClearSplitPackSize(dlg):
	edit = getattr(dlg, "splitPackEditLine", None)
	if edit:
		edit.KillFocus()
		edit.SetText("")

class ItemSplitRunner(ui.Window):
	IN_FLIGHT = 4
	ANSWER_TIMEOUT = 2.0

	def __init__(self):
		ui.Window.__init__(self)
		self.__Reset()

	def __del__(self):
		ui.Window.__del__(self)

	def __Reset(self):
		self.vnum = 0
		self.src = -1
		self.packSize = 0
		self.height = 1
		self.target = 0
		self.done = 0
		self.pending = {}
		self.lastProgress = 0.0

	def IsRunning(self):
		return 0 != self.vnum

	def Start(self, src, packSize, packCount):
		vnum = player.GetItemIndex(src)
		count = player.GetItemCount(src)
		if not vnum or count <= 1 or packSize <= 0:
			return False
		if packSize >= count:
			chat.AppendChat(chat.CHAT_TYPE_INFO, SPLIT_TOO_BIG % count)
			return False

		moves = count / packSize
		if 0 == count % packSize:
			moves -= 1	# the original slot keeps the last pack
		if packCount > 1:
			if packCount * packSize < count:
				moves = min(moves, packCount)
			else:
				moves = min(moves, packCount - 1)
		if moves <= 0:
			return False

		item.SelectItem(vnum)
		(width, height) = item.GetItemSize()

		self.__Reset()
		self.vnum = vnum
		self.src = src
		self.packSize = packSize
		self.height = max(1, height)
		self.target = moves
		chat.AppendChat(chat.CHAT_TYPE_INFO, SPLIT_START % (moves, packSize))
		self.Show()
		return True

	def Stop(self, message = None):
		if not self.IsRunning():
			return
		if None == message:
			message = SPLIT_STOPPED % (self.done, self.packSize)
		self.__Reset()
		self.Hide()
		chat.AppendChat(chat.CHAT_TYPE_INFO, message)

	def __FindFreeSlot(self):
		pageSize = player.INVENTORY_PAGE_SIZE
		pageCount = max(1, InventoryUsableSize() / pageSize)
		srcPage = min(self.src / pageSize, pageCount - 1)
		for p in xrange(pageCount):
			page = (srcPage + p) % pageCount
			base = page * pageSize
			taken = set()
			for i in xrange(base, base + pageSize):
				vnum = player.GetItemIndex(i)
				if vnum:
					item.SelectItem(vnum)
					h = max(1, item.GetItemSize()[1])
					for k in xrange(h):
						taken.add(i + k * INVENTORY_COLUMNS)
			for i in self.pending.keys():
				for k in xrange(self.height):
					taken.add(i + k * INVENTORY_COLUMNS)
			for i in xrange(base, base + pageSize):
				last = i + (self.height - 1) * INVENTORY_COLUMNS
				if last >= base + pageSize:
					continue
				free = True
				for k in xrange(self.height):
					if (i + k * INVENTORY_COLUMNS) in taken:
						free = False
						break
				if free:
					return i
		return -1

	def OnUpdate(self):
		if not self.IsRunning():
			return
		# MT2009_PLUS_VEKIRION_V1: no move goes out on the way to another core.
		if not __import__("warpsafe").InGame():
			self.Stop()
			return
		now = app.GetTime()

		for dst in self.pending.keys():
			if player.GetItemIndex(dst) == self.vnum and player.GetItemCount(dst) > 0:
				del self.pending[dst]
				self.done += 1
				self.lastProgress = now

		if self.pending and now - self.lastProgress > self.ANSWER_TIMEOUT:
			self.Stop()
			return
		if self.done >= self.target and not self.pending:
			self.Stop(SPLIT_DONE % (self.done, self.packSize))
			return
		if IsInventoryBusy():
			self.Stop()
			return
		if player.GetItemIndex(self.src) != self.vnum:
			if not self.pending:
				self.Stop()
			return

		srcCount = player.GetItemCount(self.src)
		while self.done + len(self.pending) < self.target and len(self.pending) < self.IN_FLIGHT:
			if srcCount - self.packSize * (len(self.pending) + 1) < 1:
				break
			dst = self.__FindFreeSlot()
			if dst < 0:
				if not self.pending:
					self.Stop(SPLIT_NO_SPACE % (self.done, self.packSize))
				return
			if not self.pending:
				self.lastProgress = now
			self.pending[dst] = now
			net.SendItemMovePacket(self.src, dst, self.packSize)

class InventoryWindow(ui.ScriptWindow):

	USE_TYPE_TUPLE = ("USE_CLEAN_SOCKET", "USE_CHANGE_ATTRIBUTE", "USE_ADD_ATTRIBUTE", "USE_ADD_ATTRIBUTE2", "USE_ADD_ACCESSORY_SOCKET", "USE_PUT_INTO_ACCESSORY_SOCKET", "USE_PUT_INTO_BELT_SOCKET", "USE_PUT_INTO_RING_SOCKET")
	if app.ENABLE_USE_COSTUME_ATTR:
		USE_TYPE_TUPLE = tuple(list(USE_TYPE_TUPLE) + ["USE_CHANGE_COSTUME_ATTR", "USE_RESET_COSTUME_ATTR"])

	questionDialog = None
	tooltipItem = None
	wndCostume = None
	wndBelt = None
	wndSideBar = None
	dlgPickMoney = None
	if app.ENABLE_CHEQUE_SYSTEM:
		dlgPickETC = None

	sellingSlotNumber = -1
	isLoaded = 0
	isOpenedCostumeWindowWhenClosingInventory = 0
	isOpenedBeltWindowWhenClosingInventory = 0

	def __init__(self):
		ui.ScriptWindow.__init__(self)

		self.isOpenedBeltWindowWhenClosingInventory = 0

		self.inventoryPageIndex = 0

		if app.ENABLE_ACCE_COSTUME_SYSTEM:
			self.wndAcceCombine = None
			self.wndAcceAbsorption = None

		self.isExchangeDialogOpen = False
		self.isOfflineShopBuilderOpen = False
		self.isOfflineShopManageOpen = False
		self.isSafeboxOpen = False
		self.isExchangeItemOpen = False
		self.isRechargePotion = False
		self.wndHorseInventory = None
		self.wndChestPreview = None
		self.chestPreviewButton = None
		eventManager.EventManager().add_observer(uiExchange.EVENT_OPEN_EXCHANGE, self.OnExchangeDialogOpen) # handel
		eventManager.EventManager().add_observer(uiExchange.EVENT_CLOSE_EXCHANGE, self.OnExchangeDialogClose) # handel

		eventManager.EventManager().add_observer(eventManager.OPEN_PRIVATE_SHOP_BUILDER, self.OnOfflineShopBuilderOpen)
		eventManager.EventManager().add_observer(eventManager.CLOSE_PRIVATE_SHOP_BUILDER, self.OnOfflineShopBuilderClose)

		eventManager.EventManager().add_observer(offlineShopManage.EVENT_OPEN_MYSHOP_SHOP_MANAGE, self.OnOfflineShopManageOpen)
		eventManager.EventManager().add_observer(offlineShopManage.EVENT_CLOSE_MYSHOP_SHOP_MANAGE, self.OnOfflineShopManageClose)

		eventManager.EventManager().add_observer(uiSafeBox.EVENT_OPEN_SAFEBOX, self.OnSafeboxOpen)
		eventManager.EventManager().add_observer(uiSafeBox.EVENT_CLOSE_SAFEBOX, self.OnSafeboxClose)

		eventManager.EventManager().add_observer(uiItemExchange.EVENT_ITEM_EXCHANGE_OPEN, self.OnExchangeItemOpen) # nie handel
		eventManager.EventManager().add_observer(uiItemExchange.EVENT_ITEM_EXCHANGE_CLOSE, self.OnExchangeItemClose) # nie handel

		eventManager.EventManager().add_observer(uiPotionRecharge.EVENT_RECHARGE_POTION_OPEN, self.OnRechargePotionOpen)
		eventManager.EventManager().add_observer(uiPotionRecharge.EVENT_RECHARGE_POTION_CLOSE, self.OnRechargePotionClose)

		eventManager.EventManager().add_observer(uiSafeBox.EVENT_QUICK_REMOVE_SAFEBOX_ITEM, self.OnQuickRemoveSafeboxItem)

		self.__LoadWindow()

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def Show(self):
		self.__LoadWindow()

		ui.ScriptWindow.Show(self)

		if self.isOpenedCostumeWindowWhenClosingInventory and self.wndCostume:
			self.wndCostume.Show()

		if self.wndBelt:
			self.wndBelt.Show(self.isOpenedBeltWindowWhenClosingInventory)

		if self.wndSideBar:
			self.wndSideBar.Show()

	def BindInterfaceClass(self, interface):
		self.interface = interface
		if app.ENABLE_WON_EXCHANGE_WINDOW and self.interface and self.wndChequeSlot:
			self.wndChequeSlot.SetEvent(ui.__mem_func__(self.interface.ToggleWonExchangeWindow))

	def __LoadWindow(self):
		if self.isLoaded == 1:
			return

		self.isLoaded = 1

		try:
			pyScrLoader = ui.PythonScriptLoader()
			pyScrLoader.LoadScriptFile(self, "UIScript/InventoryWindow.py")
		except:
			import exception
			exception.Abort("InventoryWindow.LoadWindow.LoadObject")

		try:
			wndItem = self.GetChild("ItemSlot")
			wndEquip = self.GetChild("EquipmentSlot")
			self.GetChild("TitleBar").SetCloseEvent(ui.__mem_func__(self.Close))
			self.wndMoney = self.GetChild("Money")
			self.wndMoneySlot = self.GetChild("Money_Slot")
			self.DSSButton = self.GetChild2("DSSButton")
			self.costumeButton = self.GetChild2("CostumeButton")

			# additional buttons
			self.horseInventoryButton = self.GetChild("HorseInventoryWindow")
			self.depositButton = self.GetChild("DepositButton")
			self.myshopButton = self.GetChild("MyShopButton")
			self.chestPreviewButton = self.GetChild2("ChestPreviewButton")

			if app.ENABLE_CHEQUE_SYSTEM:
				self.wndCheque = self.GetChild("Cheque")
				self.wndChequeSlot = self.GetChild("Cheque_Slot")

				self.wndMoneyIcon = self.GetChild("Money_Icon")
				self.wndChequeIcon = self.GetChild("Cheque_Icon")

				self.wndMoneyIcon.SetEvent(ui.__mem_func__(self.EventProgress), "mouse_over_in", 0)
				self.wndChequeIcon.SetEvent(ui.__mem_func__(self.EventProgress), "mouse_over_in", 1)

				self.wndMoneyIcon.SetEvent(ui.__mem_func__(self.EventProgress), "mouse_over_out", 0)
				self.wndChequeIcon.SetEvent(ui.__mem_func__(self.EventProgress), "mouse_over_out", 1)

				self.toolTip = uiToolTip.ToolTip()
				self.toolTip.ClearToolTip()

			self.autostackButton = self.GetChild("AutoStackButton")
			self.autostackButton.SAFE_SetEvent(self.__OnAutoStackButton)


			self.inventoryTab = []
			for i in xrange(player.INVENTORY_PAGE_COUNT-1):
				self.inventoryTab.append(self.GetChild("Inventory_Tab_%02d" % (i+1)))

			self.equipmentTab = []
			self.equipmentTab.append(self.GetChild("Equipment_Tab_01"))
			self.equipmentTab.append(self.GetChild("Equipment_Tab_02"))

			if self.costumeButton and not app.ENABLE_COSTUME_SYSTEM:
				self.costumeButton.Hide()
				self.costumeButton.Destroy()
				self.costumeButton = 0

			# Belt Inventory Window
			self.wndBelt = None

			if app.ENABLE_NEW_EQUIPMENT_SYSTEM:
				self.wndBelt = BeltInventoryWindow(self)

		except:
			import exception
			exception.Abort("InventoryWindow.LoadWindow.BindObject")

		# Icon sidebar (SidebarWindow); the inventory goes on without it
		# should it fail.
		self.wndSideBar = None
		try:
			self.wndSideBar = SidebarWindow(self)
			self.wndSideBar.Hide()
		except Exception, e:
			dbg.TraceError("InventoryWindow.LoadWindow.SideBar: %s" % str(e))
			self.wndSideBar = None

		## Item
		wndItem.SetSelectEmptySlotEvent(ui.__mem_func__(self.SelectEmptySlot))
		wndItem.SetSelectItemSlotEvent(ui.__mem_func__(self.SelectItemSlot))
		wndItem.SetUnselectItemSlotEvent(ui.__mem_func__(self.OnRightClickBagItem))
		wndItem.SetUseSlotEvent(ui.__mem_func__(self.UseItemSlot))
		wndItem.SetOverInItemEvent(ui.__mem_func__(self.OverInItem))
		wndItem.SetOverOutItemEvent(ui.__mem_func__(self.OverOutItem))

		## Equipment
		wndEquip.SetSelectEmptySlotEvent(ui.__mem_func__(self.SelectEmptySlot))
		wndEquip.SetSelectItemSlotEvent(ui.__mem_func__(self.SelectItemSlot))
		wndEquip.SetUnselectItemSlotEvent(ui.__mem_func__(self.UseItemSlot))
		wndEquip.SetUseSlotEvent(ui.__mem_func__(self.UseItemSlot))
		wndEquip.SetOverInItemEvent(ui.__mem_func__(self.OverInItem))
		wndEquip.SetOverOutItemEvent(ui.__mem_func__(self.OverOutItem))

		## PickMoneyDialog
		dlgPickMoney = uiPickMoney.PickMoneyDialog()
		dlgPickMoney.LoadDialog()
		dlgPickMoney.Hide()
		if not app.ENABLE_CHEQUE_SYSTEM:
			AddSplitPackRow(dlgPickMoney)	# MT2009_PLUS_SPLIT_PACKS_V1

		## PickETCDialog
		if app.ENABLE_CHEQUE_SYSTEM:
			dlgPickETC = uiPickETC.PickETCDialog()
			dlgPickETC.LoadDialog()
			dlgPickETC.Hide()
			AddSplitPackRow(dlgPickETC)	# MT2009_PLUS_SPLIT_PACKS_V1

		## RefineDialog
		self.refineDialog = uiRefine.RefineDialog()
		self.refineDialog.Hide()

		## AttachMetinDialog
		self.attachMetinDialog = uiAttachMetin.AttachMetinDialog()
		self.attachMetinDialog.Hide()

		## MoneySlot
		if app.ENABLE_CHEQUE_SYSTEM:
			self.wndChequeSlot.SetEvent(ui.__mem_func__(self.OpenPickMoneyDialog), 1)
			self.wndMoneySlot.SetEvent(ui.__mem_func__(self.OpenPickMoneyDialog), 0)
		else:
			self.wndMoneySlot.SetEvent(ui.__mem_func__(self.OpenPickMoneyDialog))

		for i in xrange(player.INVENTORY_PAGE_COUNT-1):
			self.inventoryTab[i].SAFE_SetEvent(self.SetInventoryPage, i)
		self.inventoryTab[0].Down()

		self.equipmentTab[0].SAFE_SetEvent(self.SetEquipmentPage, 0)
		self.equipmentTab[1].SAFE_SetEvent(self.SetEquipmentPage, 1)
		self.equipmentTab[0].Down()
		self.equipmentTab[0].Hide()
		self.equipmentTab[1].Hide()

		self.wndItem = wndItem
		self.wndEquip = wndEquip
		self.dlgPickMoney = dlgPickMoney

		# Additional buttons
		if self.horseInventoryButton:
			self.horseInventoryButton.SAFE_SetEvent(self.ClickHorseInventoryButton)

		if self.depositButton:
			self.depositButton.SetEvent(ui.__mem_func__(self.ClickDepositButton))

		if self.myshopButton:
			self.myshopButton.SetEvent(ui.__mem_func__(self.ClickMyShopButton))

		if self.chestPreviewButton:
			self.chestPreviewButton.SetEvent(ui.__mem_func__(self.ClickChestPreviewButton))

		self.inventorySlotStateMgr = InventorySlotManager(self.wndItem)

		if app.ENABLE_CHEQUE_SYSTEM:
			self.dlgPickETC = dlgPickETC

		if self.DSSButton:
			self.DSSButton.SetEvent(ui.__mem_func__(self.ClickDSSButton))

		# Costume Button.  MSS32 crashes on this client's click sample for this
		# lazily-created window, so invoke only this button without that sample.
		if self.costumeButton:
			self.costumeButton.disableClickSound = True
			self.costumeButton.SetEvent(ui.__mem_func__(self.ClickCostumeButton))

		self.wndCostume = None

 		#####
		if app.ENABLE_ACCE_COSTUME_SYSTEM:
			self.listAttachedAcces = []

		## Refresh
		self.SetInventoryPage(0)
		self.SetEquipmentPage(0)
		self.RefreshItemSlot()
		self.RefreshGold()

	@ui.WindowDestroy
	def Destroy(self):
		self.ClearDictionary()

		# MT2009_PLUS_OPEN_ALL_V1
		if getattr(self, "openAllRunner", None):
			self.openAllRunner.Hide()
			self.openAllRunner = None
		# MT2009_PLUS_SPLIT_PACKS_V1
		if getattr(self, "splitRunner", None):
			self.splitRunner.Hide()
			self.splitRunner = None

		if self.dlgPickMoney:
			self.dlgPickMoney.Destroy()
			self.dlgPickMoney = 0

		if self.refineDialog:
			self.refineDialog.Destroy()
			self.refineDialog = 0

		if app.ENABLE_CHEQUE_SYSTEM and self.dlgPickETC:
			self.dlgPickETC.Destroy()
			self.dlgPickETC = 0

		if self.attachMetinDialog:
			self.attachMetinDialog.Destroy()
			self.attachMetinDialog = 0

		self.tooltipItem = None
		for mark in getattr(self, "sortLockMarks", None) or []:
			mark.Hide()
		self.sortLockMarks = None
		self.wndItem = 0
		self.wndEquip = 0
		self.dlgPickMoney = 0
		self.wndMoney = 0
		self.wndMoneySlot = 0
		if app.ENABLE_CHEQUE_SYSTEM:
			self.wndCheque = 0
			self.wndChequeSlot = 0
			self.dlgPickETC = 0
		self.questionDialog = None
		self.mallButton = None
		self.DSSButton = None
		self.costumeButton = None
		self.wndDragonSoulRefine = None
		self.interface = None

		self.wndHorseInventory = None

		if self.wndCostume:
			self.wndCostume.Destroy()
			self.wndCostume = None

		if self.wndBelt:
			self.wndBelt.Destroy()
			self.wndBelt = None

		if self.wndSideBar:
			self.wndSideBar.Hide()
			self.wndSideBar.Destroy()
			self.wndSideBar = None

		if app.ENABLE_ACCE_COSTUME_SYSTEM:
			self.wndAcceCombine = None
			self.wndAcceAbsorption = None

		self.inventoryTab = []
		self.equipmentTab = []

		self.inventorySlotStateMgr = None

		if self.wndChestPreview:
			self.wndChestPreview.Close()
			self.wndChestPreview = None
		self.chestPreviewButton = None

	def Hide(self):
		if constInfo.GET_ITEM_QUESTION_DIALOG_STATUS():
			self.OnCloseQuestionDialog()
			return
		if None != self.tooltipItem:
			self.tooltipItem.HideToolTip()

		if self.wndCostume:
			self.isOpenedCostumeWindowWhenClosingInventory = self.wndCostume.IsShow()
			self.wndCostume.Close()

		if self.wndBelt:
			self.isOpenedBeltWindowWhenClosingInventory = self.wndBelt.IsOpeningInventory()
			print "Is Opening Belt Inven?? ", self.isOpenedBeltWindowWhenClosingInventory
			self.wndBelt.Close()

		if self.wndSideBar:
			self.wndSideBar.Hide()

		if self.dlgPickMoney:
			ClearSplitPackSize(self.dlgPickMoney)	# MT2009_PLUS_SPLIT_PACKS_V1
			self.dlgPickMoney.Close()

		if app.ENABLE_CHEQUE_SYSTEM:
			if self.dlgPickETC:
				ClearSplitPackSize(self.dlgPickETC)	# MT2009_PLUS_SPLIT_PACKS_V1
				self.dlgPickETC.Close()

		if self.wndChestPreview:
			self.wndChestPreview.Close()

		wndMgr.Hide(self.hWnd)


	def Close(self):
		eventManager.EventManager().send_event(EVENT_CLOSE_INVENTORY)
		self.Hide()

	def SetHorseInventory(self, wndHorseInventory):
		self.wndHorseInventory = proxy(wndHorseInventory)

		self.wndHorseInventory.itemSlot.SetSelectEmptySlotEvent(ui.__mem_func__(self.SelectEmptySlot))
		self.wndHorseInventory.itemSlot.SetSelectItemSlotEvent(ui.__mem_func__(self.SelectItemSlot))
		self.wndHorseInventory.itemSlot.SetUnselectItemSlotEvent(ui.__mem_func__(self.UseItemSlot))
		self.wndHorseInventory.itemSlot.SetUseSlotEvent(ui.__mem_func__(self.UseItemSlot))
		self.wndHorseInventory.itemSlot.SetOverInItemEvent(ui.__mem_func__(self.OverInItem))
		self.wndHorseInventory.itemSlot.SetOverOutItemEvent(ui.__mem_func__(self.OverOutItem))

		self.inventorySlotStateMgr.secondSlotWindow = self.wndHorseInventory.itemSlot

	def SetInventoryPage(self, page):
		self.inventoryPageIndex = page
		for i in xrange(player.INVENTORY_PAGE_COUNT-1):
			if i!=page:
				self.inventoryTab[i].SetUp()

		self.inventorySlotStateMgr.SetTab(page)
		self.RefreshBagSlotWindow()

	def ClickHorseInventoryButton(self):
		eventManager.EventManager().send_event(uiHorseInventory.EVENT_OPEN_HORSE_INVENTORY)

	def ClickDepositButton(self):
		print "ClickDepositButton"
		net.SendChatPacket("/click_mall")

	def ClickChestPreviewButton(self):
		if not self.wndChestPreview:
			import uichestpreview
			self.wndChestPreview = uichestpreview.ChestPreviewWindow(self)
		self.wndChestPreview.Toggle()

	def ClickMyShopButton(self):
		print "ClickMyShopButton"
		self.interface.OpenPrivateShopManage()

	def SetEquipmentPage(self, page):
		self.equipmentPageIndex = page
		self.equipmentTab[1-page].SetUp()
		self.RefreshEquipSlotWindow()

	# DSSButton
	def ClickDSSButton(self):
		print "click_dss_button"
		self.interface.ToggleDragonSoulWindow()

	def ClickCostumeButton(self):
		print "Click Costume Button"
		if self.wndCostume:
			if self.wndCostume.IsShow():
				self.wndCostume.Hide()
			else:
				self.wndCostume.Show()
		else:
			self.wndCostume = CostumeWindow(self)
			# First opened (no place kept for it yet): past the icon sidebar,
			# not under it.
			if self.wndSideBar:
				x, y = self.GetGlobalPosition()
				x -= self.wndSideBar.GetLeftOffset() + self.wndCostume.GetWidth()
				self.wndCostume.SetPosition(max(0, x), y)
			import uiwindowpos
			uiwindowpos.Track(self.wndCostume, "kostiumy")
			self.wndCostume.Show()


	def __OnAutoStackButton(self):
		import inventoryarrange
		inventoryarrange.OpenChoice()

	def __OnAutoStackButtonByMoves(self):
		import autostackpump
		moves = []
		TOTAL_SLOTS = player.INVENTORY_DEFAULT_MAX_NUM
		for sourceSlot in range(TOTAL_SLOTS):
			srcItemVnum = player.GetItemIndex(sourceSlot)

			if srcItemVnum == 0:
				continue

			item.SelectItem(srcItemVnum)
			if item.IsFlag(item.ITEM_FLAG_STACKABLE):
				for destSlot in range(sourceSlot + 1, TOTAL_SLOTS):
					destItemVnum = player.GetItemIndex(destSlot)

					if destItemVnum == srcItemVnum:
						moves.append((destSlot, sourceSlot))

		autostackpump.Queue(moves)
		chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.AUTOSTACK_INVENTORY)

	def OpenPickMoneyDialog(self, focus_idx = 0):
		pass
		# if mouseModule.mouseController.isAttached():
		# 	attachedSlotPos = mouseModule.mouseController.GetAttachedSlotNumber()
		# 	if player.SLOT_TYPE_SAFEBOX == mouseModule.mouseController.GetAttachedType():
		# 		if player.ITEM_MONEY == mouseModule.mouseController.GetAttachedItemIndex():
		# 			net.SendSafeboxWithdrawMoneyPacket(mouseModule.mouseController.GetAttachedItemCount())
		# 			snd.PlaySound("sound/ui/money.wav")
		#
		# 	mouseModule.mouseController.DeattachObject()
		#
		# else:
		# 	curMoney = player.GetElk()
		# 	if app.ENABLE_CHEQUE_SYSTEM:
		# 		curCheque = player.GetCheque()
		# 		if curMoney <= 0 and curCheque <= 0:
		# 			return
		# 	else:
		# 		curCheque = 1 # default money value
		# 		if curMoney <= 0:
		# 			return
		#
		# 	self.dlgPickMoney.SetTitleName(localeInfo.PICK_MONEY_TITLE)
		# 	self.dlgPickMoney.SetAcceptEvent(ui.__mem_func__(self.OnPickMoney))
		# 	self.dlgPickMoney.Open(curMoney, curCheque)
		# 	if app.ENABLE_CHEQUE_SYSTEM:
		# 		if focus_idx==0:
		# 			self.dlgPickMoney.SetMaxCheque(3)
		# 			self.dlgPickMoney.SetMax(9)
		# 		else:
		# 			self.dlgPickMoney.SetMax(9)
		# 			self.dlgPickMoney.SetMaxCheque(3)
		# 		self.dlgPickMoney.SetFocus(focus_idx)
		# 	else:
		# 		self.dlgPickMoney.SetMax(9)

	def OnPickMoney(self, money, cheque=0):
		mouseModule.mouseController.AttachMoney(self, player.SLOT_TYPE_INVENTORY, money, cheque)

	def OnPickItem(self, count):
		if app.ENABLE_CHEQUE_SYSTEM:
			itemSlotIndex = self.dlgPickETC.itemGlobalSlotIndex
			dlg = self.dlgPickETC
		else:
			itemSlotIndex = self.dlgPickMoney.itemGlobalSlotIndex
			dlg = self.dlgPickMoney

		# MT2009_PLUS_SPLIT_PACKS_V1: a pack size splits the stack instead
		packSize = GetSplitPackSize(dlg)
		ClearSplitPackSize(dlg)
		if packSize > 0:
			runner = getattr(self, "splitRunner", None)
			if runner and runner.IsRunning():
				return
			if not runner:
				runner = ItemSplitRunner()
				self.splitRunner = runner
			runner.Start(itemSlotIndex, packSize, count)
			return
		selectedItemVNum = player.GetItemIndex(itemSlotIndex)
		mouseModule.mouseController.AttachObject(self, player.SLOT_TYPE_INVENTORY, itemSlotIndex, selectedItemVNum, count)

	def __InventoryLocalSlotPosToGlobalSlotPos(self, local):
		if local >= player.INVENTORY_DEFAULT_MAX_NUM:
			return local

		if player.IsEquipmentSlot(local) or player.IsCostumeSlot(local) or (app.ENABLE_NEW_EQUIPMENT_SYSTEM and player.IsBeltInventorySlot(local)):
			return local

		return self.inventoryPageIndex*player.INVENTORY_PAGE_SIZE + local

	def RefreshBagSlotWindow(self, wndSlot=None):
		getItemVNum=player.GetItemIndex
		getItemCount=player.GetItemCount

		if wndSlot is None:
			wndSlot = self.wndItem

		setItemVNum=wndSlot.SetItemSlot

		for i in xrange(player.INVENTORY_PAGE_SIZE):
			localSlot = i + wndSlot.GetStartIndex()
			slotNumber = self.__InventoryLocalSlotPosToGlobalSlotPos(localSlot)

			itemCount = getItemCount(slotNumber)
			if 0 == itemCount:
				wndSlot.ClearSlot(localSlot)
				continue
			elif 1 == itemCount:
				itemCount = 0

			itemVnum = getItemVNum(slotNumber)

			socket = tuple(player.GetItemMetinSocket(slotNumber, j) for j in range(player.METIN_SOCKET_MAX_NUM))

			setItemVNum(localSlot, itemVnum, itemCount, socket=socket)
			# if app.ENABLE_IKASHOP_RENEWAL:
			# 	if app.EXTEND_IKASHOP_PRO:
			# 		self.wndItem.EnableSlot(i)
			# 		board = ikashop.GetBusinessBoard()
			# 		if board and (board.IsShow() or board.IsCreatingAuction()):
			# 			item.SelectItem(itemVnum)
			# 			if item.IsAntiFlag(item.ANTIFLAG_GIVE) or item.IsAntiFlag(item.ANTIFLAG_MYSHOP):
			# 				self.wndItem.DisableSlot(i)




			if constInfo.IS_AUTO_POTION(itemVnum):
				metinSocket = [player.GetItemMetinSocket(slotNumber, j) for j in xrange(player.METIN_SOCKET_MAX_NUM)]

				isActivated = 0 != metinSocket[0]

				if isActivated:
					wndSlot.ActivateSlot(i)
					potionType = 0
					if constInfo.IS_AUTO_POTION_HP(itemVnum):
						potionType = player.AUTO_POTION_TYPE_HP
					elif constInfo.IS_AUTO_POTION_SP(itemVnum):
						potionType = player.AUTO_POTION_TYPE_SP

					usedAmount = int(metinSocket[1])
					totalAmount = int(metinSocket[2])
					player.SetAutoPotionInfo(potionType, isActivated, (totalAmount - usedAmount), totalAmount, self.__InventoryLocalSlotPosToGlobalSlotPos(i))

				else:
					wndSlot.DeactivateSlot(i)

			if constInfo.ENABLE_ACTIVE_PET_SEAL_EFFECT and constInfo.IS_PET_SEAL(itemVnum):
				metinSocket = [player.GetItemMetinSocket(slotNumber, j) for j in xrange(player.METIN_SOCKET_MAX_NUM)]
				isActivated = 0 != metinSocket[2]
				if isActivated:
					wndSlot.ActivateSlot(i)
				else:
					wndSlot.DeactivateSlot(i)

		# MT2009_PLUS_SLOT_REFRESH_ONCE_V1: the bag's own slot window is
		# refreshed by the state manager below (once, after the states) -
		# each RefreshSlot rebuilds the tooltip of the item under the cursor.
		if wndSlot is not self.wndItem:
			wndSlot.RefreshSlot()

		if self.wndBelt:
			self.wndBelt.RefreshSlot()

		self.inventorySlotStateMgr.RefreshAllSlots()

		if wndSlot is self.wndItem:
			self.__RefreshSortLockMarks()

	# MT2009_PLUS_INVENTORY_SORT_LOCK_V1: the star in the corner of a locked
	# item's slot (inventorysortlock.py). One image per cell of the page,
	# children of the bag's slot window drawn over whatever the slot draws
	# (the cooldown, an active potion's or pet seal's glow, the count) and
	# never taking the mouse, so the slot under it works as before.
	SORT_LOCK_IMAGE = "mt2009_ui/sortlock/star.tga"
	SORT_LOCK_X = 1
	SORT_LOCK_Y = 1
	SORT_LOCK_COLUMNS = 5  # uiscript/inventorywindow.py, ItemSlot: x_count 5, x_step and y_step 32

	def __IsTypingText(self):
		interface = getattr(self, "interface", None)
		if not interface:
			return False
		try:
			if interface.IsOpenChat():
				return True
		except Exception:
			pass
		try:
			for dialog in interface.whisperDialogDict.itervalues():
				if dialog.IsShow() and dialog.chatLine.IsFocus():
					return True
		except Exception:
			pass
		return False

	def __ToggleSortLock(self, globalSlot):
		import inventorysortlock
		if not inventorysortlock.IsBagCell(globalSlot):
			return False
		if not inventorysortlock.Toggle(globalSlot):
			return False
		self.__RefreshSortLockMarks()
		snd.PlaySound("sound/ui/pick.wav")
		return True

	def __RefreshSortLockMarks(self):
		if not self.wndItem:
			return
		try:
			import inventorysortlock
			inventorysortlock.Prune()
		except Exception:
			return
		marks = getattr(self, "sortLockMarks", None)
		if marks is None:
			marks = []
			try:
				for i in xrange(player.INVENTORY_PAGE_SIZE):
					mark = ui.ImageBox()
					mark.SetParent(self.wndItem)
					mark.AddFlag("not_pick")
					mark.LoadImage(self.SORT_LOCK_IMAGE)
					mark.SetPosition((i % self.SORT_LOCK_COLUMNS) * 32 + self.SORT_LOCK_X, (i // self.SORT_LOCK_COLUMNS) * 32 + self.SORT_LOCK_Y)
					mark.Hide()
					marks.append(mark)
			except Exception:
				for mark in marks:
					mark.Hide()
				marks = []  # no image in this client's packs: no stars, and no second try
			self.sortLockMarks = marks
		for i in xrange(len(marks)):
			globalSlot = self.__InventoryLocalSlotPosToGlobalSlotPos(i)
			if inventorysortlock.IsLocked(globalSlot):
				marks[i].Show()
			else:
				marks[i].Hide()

	def HighlightSlot(self, inventorySlot):
		self.inventorySlotStateMgr.HighlightSlot(inventorySlot)

	def OnExchangeDialogOpen(self): # handel
		self.isExchangeDialogOpen = True

	def OnExchangeDialogClose(self): # handel
		self.isExchangeDialogOpen = False

	def OnOfflineShopBuilderOpen(self):
		self.isOfflineShopBuilderOpen = True

	def OnOfflineShopBuilderClose(self):
		self.isOfflineShopBuilderOpen = False

	def OnOfflineShopManageOpen(self):
		self.isOfflineShopManageOpen = True

	def OnOfflineShopManageClose(self):
		self.isOfflineShopManageOpen = False

	def OnSafeboxOpen(self):
		self.isSafeboxOpen = True

	def OnSafeboxClose(self):
		self.isSafeboxOpen = False

	def OnExchangeItemOpen(self): # nie handel
		self.isExchangeItemOpen = True

	def OnExchangeItemClose(self): # nie handel
		self.isExchangeItemOpen = False

	def OnRechargePotionOpen(self):
		self.isRechargePotion = True

	def OnRechargePotionClose(self):
		self.isRechargePotion = False

	def OnQuickRemoveSafeboxItem(self, type, slotNumber):
		itemVnum = 0
		if type == "safebox":
			itemVnum = safebox.GetItemID(slotNumber)
		elif type == "mall":
			itemVnum = safebox.GetMallItemID(slotNumber)

		if itemVnum > 0:
			item.SelectItem(itemVnum)
			(itemWidth, itemHeight) = item.GetItemSize()

			freeSlot = self.wndItem.FindEmptySlot(itemHeight, self.inventoryPageIndex * player.INVENTORY_PAGE_SIZE)
			if freeSlot >= 0:
				if type == "safebox":
					net.SendSafeboxCheckoutPacket(slotNumber, freeSlot)
				elif type == "mall":
					net.SendMallCheckoutPacket(slotNumber, freeSlot)

	def SetItemSlotVnum(self, slotNumber):
		getItemVNum=player.GetItemIndex
		getItemCount=player.GetItemCount
		setItemVNum=self.wndEquip.SetItemSlot
		itemCount = getItemCount(slotNumber)
		if itemCount <= 1:
			itemCount = 0
		setItemVNum(slotNumber, getItemVNum(slotNumber), itemCount)
		return

	def RefreshEquipSlotWindow(self):
		SetItemSlotVnum = self.SetItemSlotVnum
		for i in xrange(player.EQUIPMENT_PAGE_COUNT):
			SetItemSlotVnum(player.EQUIPMENT_SLOT_START + i)

		if app.ENABLE_NEW_EQUIPMENT_SYSTEM:
			SetItemSlotVnum(item.EQUIPMENT_BELT)
		if app.ENABLE_PENDANT_SYSTEM:
			SetItemSlotVnum(item.EQUIPMENT_PENDANT)
		if app.ENABLE_GLOVE_SYSTEM:
			SetItemSlotVnum(item.EQUIPMENT_GLOVE)

		self.wndEquip.RefreshSlot()

		if self.wndCostume and hasattr(self.wndCostume, "wndEquip"):
			self.wndCostume.RefreshCostumeSlot()

	def RefreshItemSlot(self):
		self.RefreshBagSlotWindow()
		self.RefreshEquipSlotWindow()
		if self.wndHorseInventory:
			self.RefreshBagSlotWindow(self.wndHorseInventory.itemSlot)

	def RefreshGold(self, gold=-1):
		if gold < 0:
			gold = player.GetGold()

		__import__("digiqol").AnimateMoney(self.wndMoney, gold)  # MT2009_PLUS_DIGI_CLIENT_QOL_V1 (Autor: Digi Rasta): ~0.4 s count

		# if app.ENABLE_CHEQUE_SYSTEM:
		# 	cheque = player.GetCheque()
		# 	self.wndCheque.SetText(localeInfo.NumberToGoldNotText(cheque))

	def SetItemToolTip(self, tooltipItem):
		self.tooltipItem = tooltipItem

	def SellItem(self):
		if self.sellingSlotitemIndex == player.GetItemIndex(self.sellingSlotNumber):
			if self.sellingSlotitemCount == player.GetItemCount(self.sellingSlotNumber):
				net.SendShopSellPacketNew(self.sellingSlotNumber, self.questionDialog.count, player.INVENTORY)
				snd.PlaySound("sound/ui/money.wav")
		self.OnCloseQuestionDialog()

	def OnDetachMetinFromItem(self):
		if None == self.questionDialog:
			return

		#net.SendItemUseToItemPacket(self.questionDialog.sourcePos, self.questionDialog.targetPos)
		self.__SendUseItemToItemPacket(self.questionDialog.sourcePos, self.questionDialog.targetPos)
		self.OnCloseQuestionDialog()

	def OnCloseQuestionDialog(self):
		if not self.questionDialog:
			return

		self.questionDialog.Close()
		self.questionDialog = None
		constInfo.SET_ITEM_QUESTION_DIALOG_STATUS(0)

	## Slot Event
	def SelectEmptySlot(self, selectedSlotPos):
		if constInfo.GET_ITEM_QUESTION_DIALOG_STATUS() == 1:
			return

		selectedSlotPos = self.__InventoryLocalSlotPosToGlobalSlotPos(selectedSlotPos)

		if mouseModule.mouseController.isAttached():

			attachedSlotType = mouseModule.mouseController.GetAttachedType()
			attachedSlotPos = mouseModule.mouseController.GetAttachedSlotNumber()
			attachedItemCount = mouseModule.mouseController.GetAttachedItemCount()
			attachedItemIndex = mouseModule.mouseController.GetAttachedItemIndex()

			# The companion's item (uisidekickinventory.py): "/towarzysz eq wez".
			import uisidekickinventory
			if uisidekickinventory.DropIntoPlayerBag(attachedSlotType, attachedSlotPos, selectedSlotPos):
				mouseModule.mouseController.DeattachObject()
				return
			# MT2009_PLUS_COLLECTOR_STORAGE_V1: an entry of the collector's storage.
			import uicollector
			if uicollector.DropIntoPlayerBag(attachedSlotType, attachedSlotPos, selectedSlotPos):
				mouseModule.mouseController.DeattachObject()
				return
			if player.SLOT_TYPE_INVENTORY == attachedSlotType:
				#@fixme011 BEGIN (block ds equip)
				attachedInvenType = player.SlotTypeToInvenType(attachedSlotType)
				if player.IsDSEquipmentSlot(attachedInvenType, attachedSlotPos):
					mouseModule.mouseController.DeattachObject()
					return
				#@fixme011 END

				itemCount = player.GetItemCount(attachedSlotPos)
				attachedCount = mouseModule.mouseController.GetAttachedItemCount()
				self.__SendMoveItemPacket(attachedSlotPos, selectedSlotPos, attachedCount)

				if item.IsRefineScroll(attachedItemIndex):
					self.wndItem.SetUseMode(False)

			elif player.SLOT_TYPE_PRIVATE_SHOP == attachedSlotType:
				mouseModule.mouseController.RunCallBack("INVENTORY")

			elif player.SLOT_TYPE_SHOP == attachedSlotType:
				net.SendShopBuyPacket(attachedSlotPos)

			elif player.SLOT_TYPE_SAFEBOX == attachedSlotType:

				if player.ITEM_MONEY == attachedItemIndex:
					net.SendSafeboxWithdrawMoneyPacket(mouseModule.mouseController.GetAttachedItemCount())
					snd.PlaySound("sound/ui/money.wav")

				else:
					net.SendSafeboxCheckoutPacket(attachedSlotPos, selectedSlotPos)

			elif player.SLOT_TYPE_MALL == attachedSlotType:
				net.SendMallCheckoutPacket(attachedSlotPos, selectedSlotPos)

			mouseModule.mouseController.DeattachObject()

	def SelectItemSlot(self, itemSlotIndex):
		if constInfo.GET_ITEM_QUESTION_DIALOG_STATUS() == 1:
			return

		if itemSlotIndex >= player.INVENTORY_DEFAULT_MAX_NUM and itemSlotIndex < player.INVENTORY_MAX_NUM and not CanAccessHorseInventory():
			return

		itemSlotIndex = self.__InventoryLocalSlotPosToGlobalSlotPos(itemSlotIndex)

		if mouseModule.mouseController.isAttached():
			attachedSlotType = mouseModule.mouseController.GetAttachedType()
			attachedSlotPos = mouseModule.mouseController.GetAttachedSlotNumber()
			attachedItemVID = mouseModule.mouseController.GetAttachedItemIndex()

			# The companion's item (uisidekickinventory.py): "/towarzysz eq wez".
			import uisidekickinventory
			if uisidekickinventory.DropIntoPlayerBag(attachedSlotType, attachedSlotPos, itemSlotIndex):
				mouseModule.mouseController.DeattachObject()
				return
			# MT2009_PLUS_COLLECTOR_STORAGE_V1: an entry of the collector's storage.
			import uicollector
			if uicollector.DropIntoPlayerBag(attachedSlotType, attachedSlotPos, itemSlotIndex):
				mouseModule.mouseController.DeattachObject()
				return

			if player.SLOT_TYPE_INVENTORY == attachedSlotType:
				#@fixme011 BEGIN (block ds equip)
				attachedInvenType = player.SlotTypeToInvenType(attachedSlotType)
				if player.IsDSEquipmentSlot(attachedInvenType, attachedSlotPos):
					mouseModule.mouseController.DeattachObject()
					return
				#@fixme011 END
				self.__DropSrcItemToDestItemInInventory(attachedItemVID, attachedSlotPos, itemSlotIndex)

			mouseModule.mouseController.DeattachObject()

		else:

			curCursorNum = app.GetCursor()
			if app.SELL == curCursorNum:
				self.__SellItem(itemSlotIndex)

			elif app.BUY == curCursorNum:
				chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.SHOP_BUY_INFO)

			elif app.IsPressed(app.DIK_LALT):
				# MT2009_PLUS_INVENTORY_SORT_LOCK_V1: Alt + left click on an
				# item of the bag locks it against sorting (inventorysortlock.py);
				# while a chat or whisper line is being typed it pastes the
				# item's link, as it always did.
				if not self.__IsTypingText() and self.__ToggleSortLock(itemSlotIndex):
					pass
				else:
					link = player.GetItemLink(itemSlotIndex)
					ime.PasteString(link)

			elif app.IsPressed(app.DIK_LSHIFT):
				__import__("keybind").NoteOtherInput()	# MT2009_PLUS_VEKIRION_V1: Shift + click is no sprint tap
				itemCount = player.GetItemCount(itemSlotIndex)

				if app.ENABLE_CHEQUE_SYSTEM:
					if itemCount > 1:
						self.dlgPickETC.SetTitleName(localeInfo.PICK_ITEM_TITLE)
						self.dlgPickETC.SetAcceptEvent(ui.__mem_func__(self.OnPickItem))
						ClearSplitPackSize(self.dlgPickETC)	# MT2009_PLUS_SPLIT_PACKS_V1
						self.dlgPickETC.Open(itemCount)
						self.dlgPickETC.itemGlobalSlotIndex = itemSlotIndex
				else:
					if itemCount > 1:
						self.dlgPickMoney.SetTitleName(localeInfo.PICK_ITEM_TITLE)
						self.dlgPickMoney.SetAcceptEvent(ui.__mem_func__(self.OnPickItem))
						ClearSplitPackSize(self.dlgPickMoney)	# MT2009_PLUS_SPLIT_PACKS_V1
						self.dlgPickMoney.Open(itemCount)
						self.dlgPickMoney.itemGlobalSlotIndex = itemSlotIndex
				#else:
					#selectedItemVNum = player.GetItemIndex(itemSlotIndex)
					#mouseModule.mouseController.AttachObject(self, player.SLOT_TYPE_INVENTORY, itemSlotIndex, selectedItemVNum)

			elif app.IsPressed(app.DIK_LCONTROL):
				itemIndex = player.GetItemIndex(itemSlotIndex)

				if True == item.CanAddToQuickSlotItem(itemIndex):
					player.RequestAddToEmptyLocalQuickSlot(player.SLOT_TYPE_INVENTORY, itemSlotIndex)
				else:
					chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.QUICKSLOT_REGISTER_DISABLE_ITEM)

			else:
				selectedItemVNum = player.GetItemIndex(itemSlotIndex)
				itemCount = player.GetItemCount(itemSlotIndex)

				mouseModule.mouseController.AttachObject(self, player.SLOT_TYPE_INVENTORY, itemSlotIndex, selectedItemVNum, itemCount)

				if self.__IsUsableItemToItem(selectedItemVNum, itemSlotIndex):
					self.wndItem.SetUseMode(True)
				else:
					self.wndItem.SetUseMode(False)

				snd.PlaySound("sound/ui/pick.wav")

	if app.ENABLE_CHEQUE_SYSTEM:
		def OverInToolTip(self, arg):
			arglen = len(str(arg))
			pos_x, pos_y = wndMgr.GetMousePosition()

			self.toolTip.ClearToolTip()
			self.toolTip.SetThinBoardSize(11 * arglen)
			self.toolTip.SetToolTipPosition(pos_x + 5, pos_y - 5)
			self.toolTip.AppendTextLine(arg, 0xffffff00)
			self.toolTip.Show()

		def OverOutToolTip(self):
			self.toolTip.Hide()

		def EventProgress(self, event_type, idx):
			if "mouse_over_in" == str(event_type):
				if idx == 0 :
					self.OverInToolTip(localeInfo.CHEQUE_SYSTEM_UNIT_YANG)
				elif idx == 1 :
					self.OverInToolTip(localeInfo.CHEQUE_SYSTEM_UNIT_WON)
				else:
					return
			elif "mouse_over_out" == str(event_type) :
				self.OverOutToolTip()
			else:
				return

	def __DropSrcItemToDestItemInInventory(self, srcItemVID, srcItemSlotPos, dstItemSlotPos):
		if srcItemSlotPos == dstItemSlotPos:
			return

		# Cor Draconis containers are stackable, but some client item-proto
		# variants classify them as use-to-item objects.  Merge identical Cors
		# before the generic use-item dispatch can intercept the drop.
		COR_DRACONIS_VNUMS = (
			50252, 50255, 50256, 50257, 50258, 50259, 50260,
			51501, 51502, 51503, 51504, 51505, 51506, 51507, 51508, 51509, 51510,
			51541, 51548, 51549, 51562, 51569, 51576, 51583, 51590, 51597,
			51604, 51611, 51618, 51625, 51632, 76040,
		)
		if srcItemVID in COR_DRACONIS_VNUMS and player.GetItemIndex(dstItemSlotPos) == srcItemVID:
			self.__SendMoveItemPacket(srcItemSlotPos, dstItemSlotPos, 0)
			return

		# MT2009_PLUS_DIGI_STACK_V1 (Autor: Digi Rasta): soul stones stack now - a stone dropped
		# on the same stone joins its stack instead of asking for a socket.
		if item.IsMetin(srcItemVID) and player.GetItemIndex(dstItemSlotPos) == srcItemVID:
			self.__SendMoveItemPacket(srcItemSlotPos, dstItemSlotPos, 0)
			return

		# cyh itemseal 2013 11 08
		if app.ENABLE_SOULBIND_SYSTEM and item.IsSealScroll(srcItemVID):
			self.__SendUseItemToItemPacket(srcItemSlotPos, dstItemSlotPos)

		elif item.IsRefineScroll(srcItemVID):
			if constInfo.ENABLE_SELF_STACK_SCROLLS and player.GetItemIndex(srcItemSlotPos) == player.GetItemIndex(dstItemSlotPos):
				self.__SendMoveItemPacket(srcItemSlotPos, dstItemSlotPos,0)
			else:
				self.RefineItem(srcItemSlotPos, dstItemSlotPos)
				self.wndItem.SetUseMode(False)

		elif item.IsMetin(srcItemVID):
			self.AttachMetinToItem(srcItemSlotPos, dstItemSlotPos)

		elif item.IsDetachScroll(srcItemVID):
			self.DetachMetinFromItem(srcItemSlotPos, dstItemSlotPos)

		elif item.IsKey(srcItemVID):
			self.__SendUseItemToItemPacket(srcItemSlotPos, dstItemSlotPos)

		elif (player.GetItemFlags(srcItemSlotPos) & ITEM_FLAG_APPLICABLE) == ITEM_FLAG_APPLICABLE:
			self.__SendUseItemToItemPacket(srcItemSlotPos, dstItemSlotPos)

		elif item.GetUseType(srcItemVID) in self.USE_TYPE_TUPLE:
			self.__SendUseItemToItemPacket(srcItemSlotPos, dstItemSlotPos)

		elif constInfo.ENABLE_SELF_STACK_SCROLLS and srcItemVID in (71052,71051,71084,71085):
			self.__SendUseItemToItemPacket(srcItemSlotPos, dstItemSlotPos)

		else:
			#snd.PlaySound("sound/ui/drop.wav")

			if player.IsEquipmentSlot(dstItemSlotPos):

				if item.IsEquipmentVID(srcItemVID):
					self.__UseItem(srcItemSlotPos)

			else:
				self.__SendMoveItemPacket(srcItemSlotPos, dstItemSlotPos, 0)
				#net.SendItemMovePacket(srcItemSlotPos, dstItemSlotPos, 0)

	def __SellItem(self, itemSlotPos):
		if not player.IsEquipmentSlot(itemSlotPos):
			if constInfo.SHOP_MASS_SELL:
				eventManager.EventManager().send_event(uiShop.EVENT_ADD_MASS_SELL, itemSlotPos, player.INVENTORY)
				return

			self.sellingSlotNumber = itemSlotPos
			itemIndex = player.GetItemIndex(itemSlotPos)
			itemCount = player.GetItemCount(itemSlotPos)


			self.sellingSlotitemIndex = itemIndex
			self.sellingSlotitemCount = itemCount

			item.SelectItem(itemIndex)
			## 20140220
			if item.IsAntiFlag(item.ANTIFLAG_SELL):
				popup = uiCommon.PopupDialog()
				popup.SetText(localeInfo.SHOP_CANNOT_SELL_ITEM)
				popup.SetAcceptEvent(self.__OnClosePopupDialog)
				popup.Open()
				self.popup = popup
				return

			itemPrice = item.GetISellItemPrice()

			if item.Is1GoldItem():
				itemPrice = itemCount / itemPrice
			else:
				itemPrice = itemPrice * itemCount

			if not app.ENABLE_NO_SELL_PRICE_DIVIDED_BY_5:
				isGoldBar = itemIndex >= 80003 and itemIndex <= 80008

				if not isGoldBar:
					itemPrice /= 5

			item.GetItemName(itemIndex)
			itemName = item.GetItemName()

			self.questionDialog = uiCommon.QuestionDialog()
			self.questionDialog.SetText(localeInfo.DO_YOU_SELL_ITEM(itemName, itemCount, itemPrice))
			self.questionDialog.SetAcceptEvent(ui.__mem_func__(self.SellItem))
			self.questionDialog.SetCancelEvent(ui.__mem_func__(self.OnCloseQuestionDialog))
			self.questionDialog.Open()
			self.questionDialog.count = itemCount

			constInfo.SET_ITEM_QUESTION_DIALOG_STATUS(1)

	def __OnClosePopupDialog(self):
		self.pop = None

	def RefineItem(self, scrollSlotPos, targetSlotPos):

		scrollIndex = player.GetItemIndex(scrollSlotPos)
		targetIndex = player.GetItemIndex(targetSlotPos)

		if player.REFINE_OK != player.CanRefine(scrollIndex, targetSlotPos):
			return

		###########################################################
		self.__SendUseItemToItemPacket(scrollSlotPos, targetSlotPos)
		#net.SendItemUseToItemPacket(scrollSlotPos, targetSlotPos)
		return

	def DetachMetinFromItem(self, scrollSlotPos, targetSlotPos):
		scrollIndex = player.GetItemIndex(scrollSlotPos)
		targetIndex = player.GetItemIndex(targetSlotPos)

		if not player.CanDetach(scrollIndex, targetSlotPos):
			if app.ENABLE_ACCE_COSTUME_SYSTEM:
				item.SelectItem(scrollIndex)
				if item.GetValue(0) == acce.CLEAN_ATTR_VALUE0:
					chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.ACCE_FAILURE_CLEAN)
				else:
					chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.REFINE_FAILURE_METIN_INSEPARABLE_ITEM)
			else:
				chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.REFINE_FAILURE_METIN_INSEPARABLE_ITEM)
			return

		self.questionDialog = uiCommon.QuestionDialog()
		self.questionDialog.SetText(localeInfo.REFINE_DO_YOU_SEPARATE_METIN)
		if app.ENABLE_ACCE_COSTUME_SYSTEM:
			item.SelectItem(targetIndex)
			if item.GetItemType() == item.ITEM_TYPE_COSTUME and item.GetItemSubType() == item.COSTUME_TYPE_ACCE:
				item.SelectItem(scrollIndex)
				if item.GetValue(0) == acce.CLEAN_ATTR_VALUE0:
					self.questionDialog.SetText(localeInfo.ACCE_DO_YOU_CLEAN)

		self.questionDialog.SetAcceptEvent(ui.__mem_func__(self.OnDetachMetinFromItem))
		self.questionDialog.SetCancelEvent(ui.__mem_func__(self.OnCloseQuestionDialog))
		self.questionDialog.Open()
		self.questionDialog.sourcePos = scrollSlotPos
		self.questionDialog.targetPos = targetSlotPos

	def AttachMetinToItem(self, metinSlotPos, targetSlotPos):
		metinIndex = player.GetItemIndex(metinSlotPos)
		targetIndex = player.GetItemIndex(targetSlotPos)

		item.SelectItem(metinIndex)
		itemName = item.GetItemName()

		result = player.CanAttachMetin(metinIndex, targetSlotPos)

		if player.ATTACH_METIN_NOT_MATCHABLE_ITEM == result:
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.REFINE_FAILURE_CAN_NOT_ATTACH(itemName))

		if player.ATTACH_METIN_NO_MATCHABLE_SOCKET == result:
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.REFINE_FAILURE_NO_SOCKET(itemName))

		elif player.ATTACH_METIN_NOT_EXIST_GOLD_SOCKET == result:
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.REFINE_FAILURE_NO_GOLD_SOCKET(itemName))

		elif player.ATTACH_METIN_CANT_ATTACH_TO_EQUIPMENT == result:
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.REFINE_FAILURE_EQUIP_ITEM)

		if player.ATTACH_METIN_OK != result:
			return

		self.attachMetinDialog.Open(metinSlotPos, targetSlotPos)



	def OverOutItem(self):
		self.wndItem.SetUsableItem(False)
		if None != self.tooltipItem:
			self.tooltipItem.HideToolTip()

	def OverInItem(self, overSlotPos):
		self.inventorySlotStateMgr.OnSlotMouseOverIn(overSlotPos)
		overSlotPosGlobal = self.__InventoryLocalSlotPosToGlobalSlotPos(overSlotPos)
		self.wndItem.SetUsableItem(False)

		if mouseModule.mouseController.isAttached():
			attachedItemType = mouseModule.mouseController.GetAttachedType()
			if player.SLOT_TYPE_INVENTORY == attachedItemType:

				attachedSlotPos = mouseModule.mouseController.GetAttachedSlotNumber()
				attachedItemVNum = mouseModule.mouseController.GetAttachedItemIndex()

				if attachedItemVNum==player.ITEM_MONEY: # @fixme005
					pass
				elif self.__CanUseSrcItemToDstItem(attachedItemVNum, attachedSlotPos, overSlotPosGlobal):
					self.wndItem.SetUsableItem(True)
					self.ShowToolTip(overSlotPosGlobal)
					return

		self.ShowToolTip(overSlotPosGlobal)


	def __IsUsableItemToItem(self, srcItemVNum, srcSlotPos):
		if item.IsRefineScroll(srcItemVNum):
			return True
		elif item.IsMetin(srcItemVNum):
			return True
		elif item.IsDetachScroll(srcItemVNum):
			return True
		elif item.IsKey(srcItemVNum):
			return True
		elif (player.GetItemFlags(srcSlotPos) & ITEM_FLAG_APPLICABLE) == ITEM_FLAG_APPLICABLE:
			return True
		elif constInfo.ENABLE_SELF_STACK_SCROLLS and srcItemVNum in (71052,71051,71084,71085):
			return True
		else:
			if item.GetUseType(srcItemVNum) in self.USE_TYPE_TUPLE:
				return True

		return False

	def __CanUseSrcItemToDstItem(self, srcItemVNum, srcSlotPos, dstSlotPos):
		if srcSlotPos == dstSlotPos:
			return False

		if item.IsRefineScroll(srcItemVNum):
			if player.REFINE_OK == player.CanRefine(srcItemVNum, dstSlotPos):
				return True
			elif constInfo.ENABLE_SELF_STACK_SCROLLS and player.GetItemIndex(dstSlotPos) == srcItemVNum:
				return True
		elif item.IsMetin(srcItemVNum):
			if player.ATTACH_METIN_OK == player.CanAttachMetin(srcItemVNum, dstSlotPos):
				return True
		elif item.IsDetachScroll(srcItemVNum):
			if player.DETACH_METIN_OK == player.CanDetach(srcItemVNum, dstSlotPos):
				return True
		elif item.IsKey(srcItemVNum):
			if player.CanUnlock(srcItemVNum, dstSlotPos):
				return True

		elif (player.GetItemFlags(srcSlotPos) & ITEM_FLAG_APPLICABLE) == ITEM_FLAG_APPLICABLE:
			return True

		elif constInfo.ENABLE_SELF_STACK_SCROLLS and srcItemVNum in (71052,71051,71084,71085):
			return True

		else:
			useType=item.GetUseType(srcItemVNum)

			if "USE_CLEAN_SOCKET" == useType:
				if self.__CanCleanBrokenMetinStone(dstSlotPos):
					return True
			elif "USE_CHANGE_ATTRIBUTE" == useType:
				if self.__CanChangeItemAttrList(dstSlotPos):
					return True
			elif "USE_ADD_ATTRIBUTE" == useType:
				if self.__CanAddItemAttr(dstSlotPos):
					return True
			elif "USE_ADD_ATTRIBUTE2" == useType:
				if self.__CanAddItemAttr(dstSlotPos):
					return True
			elif "USE_ADD_ACCESSORY_SOCKET" == useType:
				if self.__CanAddAccessorySocket(dstSlotPos):
					return True
			elif "USE_PUT_INTO_ACCESSORY_SOCKET" == useType:
				if self.__CanPutAccessorySocket(dstSlotPos, srcItemVNum):
					return True;
			elif "USE_PUT_INTO_BELT_SOCKET" == useType:
				dstItemVNum = player.GetItemIndex(dstSlotPos)
				print "USE_PUT_INTO_BELT_SOCKET", srcItemVNum, dstItemVNum

				item.SelectItem(dstItemVNum)

				if item.ITEM_TYPE_BELT == item.GetItemType():
					return True
			elif app.ENABLE_USE_COSTUME_ATTR and "USE_CHANGE_COSTUME_ATTR" == useType:
				if self.__CanChangeCostumeAttrList(dstSlotPos):
					return True
			elif app.ENABLE_USE_COSTUME_ATTR and "USE_RESET_COSTUME_ATTR" == useType:
				if self.__CanResetCostumeAttr(dstSlotPos):
					return True

		return False

	def __CanCleanBrokenMetinStone(self, dstSlotPos):
		dstItemVNum = player.GetItemIndex(dstSlotPos)
		if dstItemVNum == 0:
			return False

		item.SelectItem(dstItemVNum)

		if item.ITEM_TYPE_WEAPON != item.GetItemType():
			return False

		for i in xrange(player.METIN_SOCKET_MAX_NUM):
			if player.GetItemMetinSocket(dstSlotPos, i) == constInfo.ERROR_METIN_STONE:
				return True

		return False

	def __CanChangeItemAttrList(self, dstSlotPos):
		dstItemVNum = player.GetItemIndex(dstSlotPos)
		if dstItemVNum == 0:
			return False

		item.SelectItem(dstItemVNum)

		if not item.GetItemType() in (item.ITEM_TYPE_WEAPON, item.ITEM_TYPE_ARMOR):
			return False

		for i in xrange(player.METIN_SOCKET_MAX_NUM):
			if player.GetItemAttribute(dstSlotPos, i)[0] != 0:
				return True

		return False

	if app.ENABLE_USE_COSTUME_ATTR:
		def __CanChangeCostumeAttrList(self, dstSlotPos):
			dstItemVNum = player.GetItemIndex(dstSlotPos)
			if dstItemVNum == 0:
				return False

			item.SelectItem(dstItemVNum)

			if item.GetItemType() != item.ITEM_TYPE_COSTUME:
				return False

			for i in xrange(player.METIN_SOCKET_MAX_NUM):
				if player.GetItemAttribute(dstSlotPos, i)[0] != 0:
					return True

			return False

		def __CanResetCostumeAttr(self, dstSlotPos):
			dstItemVNum = player.GetItemIndex(dstSlotPos)
			if dstItemVNum == 0:
				return False

			item.SelectItem(dstItemVNum)

			if item.GetItemType() != item.ITEM_TYPE_COSTUME:
				return False

			for i in xrange(player.METIN_SOCKET_MAX_NUM):
				if player.GetItemAttribute(dstSlotPos, i)[0] != 0:
					return True

			return False

	def __CanPutAccessorySocket(self, dstSlotPos, mtrlVnum):
		dstItemVNum = player.GetItemIndex(dstSlotPos)
		if dstItemVNum == 0:
			return False

		item.SelectItem(dstItemVNum)

		if item.GetItemType() != item.ITEM_TYPE_ARMOR:
			return False

		if not item.GetItemSubType() in (item.ARMOR_WRIST, item.ARMOR_NECK, item.ARMOR_EAR):
			return False

		curCount = player.GetItemMetinSocket(dstSlotPos, 0)
		maxCount = player.GetItemMetinSocket(dstSlotPos, 1)

		if mtrlVnum != constInfo.GET_ACCESSORY_MATERIAL_VNUM(dstItemVNum, item.GetItemSubType()):
			return False

		if curCount>=maxCount:
			return False

		return True

	def __CanAddAccessorySocket(self, dstSlotPos):
		dstItemVNum = player.GetItemIndex(dstSlotPos)
		if dstItemVNum == 0:
			return False

		item.SelectItem(dstItemVNum)

		if item.GetItemType() != item.ITEM_TYPE_ARMOR:
			return False

		if not item.GetItemSubType() in (item.ARMOR_WRIST, item.ARMOR_NECK, item.ARMOR_EAR):
			return False

		curCount = player.GetItemMetinSocket(dstSlotPos, 0)
		maxCount = player.GetItemMetinSocket(dstSlotPos, 1)

		ACCESSORY_SOCKET_MAX_SIZE = 3
		if maxCount >= ACCESSORY_SOCKET_MAX_SIZE:
			return False

		return True

	def __CanAddItemAttr(self, dstSlotPos):
		dstItemVNum = player.GetItemIndex(dstSlotPos)
		if dstItemVNum == 0:
			return False

		item.SelectItem(dstItemVNum)

		if not item.GetItemType() in (item.ITEM_TYPE_WEAPON, item.ITEM_TYPE_ARMOR):
			return False

		attrCount = 0
		for i in xrange(player.METIN_SOCKET_MAX_NUM):
			if player.GetItemAttribute(dstSlotPos, i)[0] != 0:
				attrCount += 1

		if attrCount<4:
			return True

		return False

	def ShowToolTip(self, slotIndex):
		if None != self.tooltipItem:
			self.tooltipItem.SetInventoryItem(slotIndex)

			if self.isOfflineShopManageOpen or self.isOfflineShopBuilderOpen:
				self.tooltipItem.AppendSpace(5)
				self.tooltipItem.AppendTextLine(localeInfo.QUICK_ADD_TO_MYSHOP)

			# MT2009_PLUS_INVENTORY_SORT_LOCK_V1
			try:
				import inventorysortlock
				inventorysortlock.AppendToolTip(self.tooltipItem, slotIndex)
			except Exception:
				pass

	def OnTop(self):
		# The sidebar first: the item tooltip stays over it.
		if self.wndSideBar:
			self.wndSideBar.SetTop()
		if None != self.tooltipItem:
			self.tooltipItem.SetTop()

	def OnPressEscapeKey(self):
		self.Close()
		return True

	def OnRightClickBagItem(self, slotIndex):
		# MT2009_PLUS_COLLECTOR_STORAGE_V1: with the collector's storage open, a
		# right click stores the item at once (Ctrl: every stack of the kind,
		# Shift: how many) - uicollector.py, before every other window.
		if self.__QuickPutToCollector(slotIndex):
			return
		garbageBin = getattr(self.interface, "wndGarbageBin", None)
		if garbageBin and garbageBin.IsShow():
			# An open bin consumes this click even when adding is rejected.
			if mouseModule.mouseController.isAttached() or constInfo.GET_ITEM_QUESTION_DIALOG_STATUS():
				return
			if app.GetCursor() == app.SELL:
				return
			if any(getattr(self, flag, False) for flag in ("isExchangeDialogOpen", "isOfflineShopBuilderOpen", "isOfflineShopManageOpen", "isSafeboxOpen", "isExchangeItemOpen", "isRechargePotion")):
				return
			if app.ENABLE_DRAGON_SOUL_SYSTEM and self.wndDragonSoulRefine.IsShow():
				return
			if app.ENABLE_ACCE_COSTUME_SYSTEM and self.isShowAcceWindow():
				return
			globalSlot = self.__InventoryLocalSlotPosToGlobalSlotPos(slotIndex)
			garbageBin.AddItemToGarbageBin(player.INVENTORY, globalSlot)
			self.OverOutItem()
			return
		# MT2009_PLUS_SIDEKICK_QUICK_TRANSFER_V1: with the companion's bag window
		# open, a right click gives the item to the companion, as the safebox's
		# does - after every window of the player's own that takes the click.
		if self.__QuickGiveToSidekick(slotIndex):
			return
		self.UseItemSlot(slotIndex)

	def __QuickPutToCollector(self, slotIndex):
		if constInfo.GET_ITEM_QUESTION_DIALOG_STATUS() or app.GetCursor() == app.SELL:
			return False
		try:
			import uicollector
		except ImportError:
			return False
		if not uicollector.QuickPut(self.__InventoryLocalSlotPosToGlobalSlotPos(slotIndex)):
			return False
		self.OverOutItem()
		return True

	def __QuickGiveToSidekick(self, slotIndex):
		if mouseModule.mouseController.isAttached() or constInfo.GET_ITEM_QUESTION_DIALOG_STATUS():
			return False
		if app.GetCursor() == app.SELL:
			return False
		if any(getattr(self, flag, False) for flag in ("isExchangeDialogOpen", "isOfflineShopBuilderOpen", "isOfflineShopManageOpen", "isSafeboxOpen", "isExchangeItemOpen", "isRechargePotion")):
			return False
		if app.ENABLE_DRAGON_SOUL_SYSTEM and self.wndDragonSoulRefine.IsShow():
			return False
		if app.ENABLE_ACCE_COSTUME_SYSTEM and self.isShowAcceWindow():
			return False
		try:
			import uisidekickinventory
		except ImportError:
			return False
		if not uisidekickinventory.QuickGive(self.__InventoryLocalSlotPosToGlobalSlotPos(slotIndex)):
			return False
		self.OverOutItem()
		return True

	def UseItemSlot(self, slotIndex):
		curCursorNum = app.GetCursor()
		if app.SELL == curCursorNum:
			return

		if constInfo.GET_ITEM_QUESTION_DIALOG_STATUS():
			return

		slotIndex = self.__InventoryLocalSlotPosToGlobalSlotPos(slotIndex)

		if self.isExchangeDialogOpen:
			eventManager.EventManager().send_event(EVENT_QUICK_ADD_INVENTORY_ITEM, "exchange", slotIndex)
			return
		elif self.isOfflineShopBuilderOpen:
			eventManager.EventManager().send_event(EVENT_QUICK_ADD_INVENTORY_ITEM, "myshop", slotIndex)
			return
		elif self.isOfflineShopManageOpen:
			eventManager.EventManager().send_event(EVENT_QUICK_ADD_INVENTORY_ITEM, "myshop_manage", slotIndex)
			return
		elif self.isSafeboxOpen:
			eventManager.EventManager().send_event(EVENT_QUICK_ADD_INVENTORY_ITEM, "safebox", slotIndex)
			return
		elif self.isExchangeItemOpen:
			eventManager.EventManager().send_event(EVENT_QUICK_ADD_INVENTORY_ITEM, "item_exchange", slotIndex)
			return
		elif self.isRechargePotion:
			eventManager.EventManager().send_event(EVENT_QUICK_ADD_INVENTORY_ITEM, "recharge_potion", slotIndex)
			return

		if app.ENABLE_DRAGON_SOUL_SYSTEM:
			if self.wndDragonSoulRefine.IsShow():
				self.wndDragonSoulRefine.AutoSetItem((player.INVENTORY, slotIndex), 1)
				return
		if app.ENABLE_ACCE_COSTUME_SYSTEM:
			if self.isShowAcceWindow():
				acce.Add(player.INVENTORY, slotIndex, 255)
				return

		if self.__OpenAllByCtrl(slotIndex):	# MT2009_PLUS_OPEN_ALL_V1
			return

		self.__UseItem(slotIndex)
		mouseModule.mouseController.DeattachObject()
		self.OverOutItem()

	# MT2009_PLUS_OPEN_ALL_V1: Ctrl + right click opens the whole stack
	# (ItemOpenAllRunner above); again while it runs, stops it.
	def __OpenAllByCtrl(self, slotIndex):
		ctrl = app.IsPressed(app.DIK_LCONTROL) or app.IsPressed(getattr(app, "DIK_RCONTROL", app.DIK_LCONTROL))
		runner = getattr(self, "openAllRunner", None)
		if runner and runner.IsRunning():
			if ctrl:
				runner.Stop()
				self.OverOutItem()
				return True
			return False
		if not ctrl:
			return False
		if not runner:
			runner = ItemOpenAllRunner()
			self.openAllRunner = runner
		if runner.Start(slotIndex):
			mouseModule.mouseController.DeattachObject()
			self.OverOutItem()
			return True
		return False

	def __UseItem(self, slotIndex):
		ItemVNum = player.GetItemIndex(slotIndex)
		# MT2009_PLUS_FLOWER_V1: a flower that would replace another flower's
		# buff asks first (uiflowerevent.py, Owsap's uiinventory question).
		if __import__("uiflowerevent").ConfirmFlowerUse(slotIndex, ItemVNum):
			return
		item.SelectItem(ItemVNum)
		if item.IsFlag(item.ITEM_FLAG_CONFIRM_WHEN_USE):
			self.questionDialog = uiCommon.QuestionDialog()
			self.questionDialog.SetText(localeInfo.INVENTORY_REALLY_USE_ITEM)
			self.questionDialog.SetAcceptEvent(ui.__mem_func__(self.__UseItemQuestionDialog_OnAccept))
			self.questionDialog.SetCancelEvent(ui.__mem_func__(self.__UseItemQuestionDialog_OnCancel))
			self.questionDialog.Open()
			self.questionDialog.slotIndex = slotIndex

			constInfo.SET_ITEM_QUESTION_DIALOG_STATUS(1)

		else:
			self.__SendUseItemPacket(slotIndex)
			#net.SendItemUsePacket(slotIndex)

	def __UseItemQuestionDialog_OnCancel(self):
		self.OnCloseQuestionDialog()

	def __UseItemQuestionDialog_OnAccept(self):
		self.__SendUseItemPacket(self.questionDialog.slotIndex)
		self.OnCloseQuestionDialog()

	def __SendUseItemToItemPacket(self, srcSlotPos, dstSlotPos):
		if uiPrivateShopBuilder.IsBuildingPrivateShop():
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.USE_ITEM_FAILURE_PRIVATE_SHOP)
			return

		dstItemVNum = player.GetItemIndex(dstSlotPos)
		srcItemVNum = player.GetItemIndex(srcSlotPos)
		if constInfo.ENABLE_SELF_STACK_SCROLLS and dstItemVNum == srcItemVNum:
			self.__SendMoveItemPacket(srcSlotPos, dstSlotPos, 0)
		else:
			net.SendItemUseToItemPacket(srcSlotPos, dstSlotPos)

	def __SendUseItemPacket(self, slotPos):
		if uiPrivateShopBuilder.IsBuildingPrivateShop():
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.USE_ITEM_FAILURE_PRIVATE_SHOP)
			return

		net.SendItemUsePacket(slotPos)

	def __SendMoveItemPacket(self, srcSlotPos, dstSlotPos, srcItemCount):
		if uiPrivateShopBuilder.IsBuildingPrivateShop():
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.MOVE_ITEM_FAILURE_PRIVATE_SHOP)
			return

		# MT2009_PLUS_INVENTORY_SORT_LOCK_V1: a locked item moved whole by
		# hand takes its lock along.
		try:
			import inventorysortlock
			inventorysortlock.OnMove(srcSlotPos, dstSlotPos, srcItemCount)
		except Exception:
			pass
		net.SendItemMovePacket(srcSlotPos, dstSlotPos, srcItemCount)

	def SetDragonSoulRefineWindow(self, wndDragonSoulRefine):
		if app.ENABLE_DRAGON_SOUL_SYSTEM:
			self.wndDragonSoulRefine = wndDragonSoulRefine

	def IsDlgQuestionShow(self):
		return bool(self.questionDialog and self.questionDialog.IsShow())

	def CancelDlgQuestion(self):
		self.OnCloseQuestionDialog()

	def SetUseItemMode(self, bUse):
		if self.wndItem:
			self.wndItem.SetUseMode(bUse)

	def OnMoveWindow(self, x, y):
		# print "Inventory Global Pos : ", self.GetGlobalPosition()
		if self.wndBelt:
			# print "Belt Global Pos : ", self.wndBelt.GetGlobalPosition()
			self.wndBelt.AdjustPositionAndSize()
		if self.wndSideBar:
			self.wndSideBar.AdjustPosition()

	if app.ENABLE_ACCE_COSTUME_SYSTEM:
		def SetAcceWindow(self, wndAcceCombine, wndAcceAbsorption):
			self.wndAcceCombine = wndAcceCombine
			self.wndAcceAbsorption = wndAcceAbsorption

		def isShowAcceWindow(self):
			if self.wndAcceCombine:
				if self.wndAcceCombine.IsShow():
					return 1
			if self.wndAcceAbsorption:
				if self.wndAcceAbsorption.IsShow():
					return 1
			return 0
