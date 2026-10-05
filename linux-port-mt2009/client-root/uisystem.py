import net
import app
import ui
import uiOption

import uiSystemOption
import uiGameOption
import uiScriptLocale
import networkModule
import constInfo
import localeInfo
import uiMoveChannel
import uiItemShop
import eventManager
import utils
from _weakref import proxy

SYSTEM_MENU_FOR_PORTAL = False

###################################################################################################
## System
class SystemDialog(ui.ScriptWindow):

	def __init__(self):
		ui.ScriptWindow.__init__(self)
		self.__Initialize()

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def __Initialize(self):
		self.eventOpenHelpWindow = None
		self.systemOptionDlg = None
		self.gameOptionDlg = None
		self.extraOptionDlg = None  # MT2009_PLUS_DIGI_CLIENT_QOL_V1
		self.optionsTabs = None  # MT2009_PLUS_OPTIONS_TABS_V1
		self.interface = None
		self.toolTip = None

	def SetToolTip(self, tooltip):
		self.toolTip = proxy(tooltip)

	def BindInterface(self, interface):
		self.interface = interface

	def LoadDialog(self):
		if SYSTEM_MENU_FOR_PORTAL:
			self.__LoadSystemMenu_ForPortal()
		else:
			self.__LoadSystemMenu_Default()

	def __LoadSystemMenu_Default(self):
		pyScrLoader = ui.PythonScriptLoader()
		pyScrLoader.LoadScriptFile(self, "uiscript/systemdialog.py")

		# MT2009_PLUS_OPTIONS_TABS_V1: one "Opcje Gry" button with tabs (uioptionstabs.py); the four
		# option buttons below are no longer in uiscript/systemdialog.py.
		if self.GetChild2("options_button"):
			self.GetChild2("options_button").SAFE_SetEvent(self.__ClickOptionsButton)
		if self.GetChild2("system_option_button"):
			self.GetChild2("system_option_button").SAFE_SetEvent(self.__ClickSystemOptionButton)
		if self.GetChild2("game_option_button"):
			self.GetChild2("game_option_button").SAFE_SetEvent(self.__ClickGameOptionButton)
		# MT2009_PLUS_VEKIRION_V1 (Autor: Vekirion): Skroty klawiszowe (uikeybind.py).
		if self.GetChild2("keybind_button"):
			self.GetChild2("keybind_button").SAFE_SetEvent(self.__ClickKeybindButton)
		if self.GetChild2("monster_card_button"):  # MT2009_PLUS_MONSTER_CARDS_V1 (Autor: Digi Rasta)
			self.GetChild2("monster_card_button").SAFE_SetEvent(self.__ClickMonsterCardButton)
		if self.GetChild2("extra_option_button"):  # MT2009_PLUS_DIGI_CLIENT_QOL_V1 (Autor: Digi Rasta)
			self.GetChild2("extra_option_button").SAFE_SetEvent(self.__ClickExtraOptionButton)
		self.GetChild("change_button").SAFE_SetEvent(self.__ClickChangeCharacterButton)
		self.GetChild("logout_button").SAFE_SetEvent(self.__ClickLogOutButton)
		self.GetChild("exit_button").SAFE_SetEvent(self.__ClickExitButton)
		self.GetChild("help_button").SAFE_SetEvent(self.__ClickHelpButton)

		self.GetChild("support_button").SAFE_SetEvent(self.__ClickSupportButton)
		self.GetChild("support_button").SAFE_SetStringEvent("MOUSE_OVER_IN", self.__OnMouseOverSupportButton)
		self.GetChild("support_button").SAFE_SetStringEvent("MOUSE_OVER_OUT", self.__OnMouseOverOut)

		self.GetChild("escape_button").SAFE_SetEvent(self.__ClickEscapeButton)
		self.GetChild("escape_button").SAFE_SetStringEvent("MOUSE_OVER_IN", self.__OnMouseOverEscapeButton)
		self.GetChild("escape_button").SAFE_SetStringEvent("MOUSE_OVER_OUT", self.__OnMouseOverOut)

		self.GetChild("cancel_button").SAFE_SetEvent(self.Close)

		data = constInfo.GAME_VERSION
		version_string = "Wersja: %d.%d.%d%s" % (
			data["expansion"], data["major"], data["minor"], "b" if data["is_beta"] else "")
		self.GetChild("GameVersion").SetText(version_string)

		if constInfo.IN_GAME_SHOP_ENABLE:
			self.GetChild("mall_button").SAFE_SetEvent(self.__ClickInGameShopButton)

		if app.ENABLE_MOVE_CHANNEL:
			self.GetChild("movechannel_button").SAFE_SetEvent(self.__ClickMoveChannelButton)


	def __LoadSystemMenu_ForPortal(self):
		pyScrLoader = ui.PythonScriptLoader()
		pyScrLoader.LoadScriptFile(self, "uiscript/systemdialog_forportal.py")

		self.GetChild("system_option_button").SAFE_SetEvent(self.__ClickSystemOptionButton)
		self.GetChild("game_option_button").SAFE_SetEvent(self.__ClickGameOptionButton)
		self.GetChild("change_button").SAFE_SetEvent(self.__ClickChangeCharacterButton)
		self.GetChild("exit_button").SAFE_SetEvent(self.__ClickExitButton)
		self.GetChild("help_button").SAFE_SetEvent(self.__ClickHelpButton)
		self.GetChild("cancel_button").SAFE_SetEvent(self.Close)

	@ui.WindowDestroy
	def Destroy(self):
		self.ClearDictionary()

		if self.optionsTabs:  # MT2009_PLUS_OPTIONS_TABS_V1
			self.optionsTabs.Destroy()

		if self.gameOptionDlg:
			self.gameOptionDlg.Destroy()

		if self.extraOptionDlg:  # MT2009_PLUS_DIGI_CLIENT_QOL_V1
			self.extraOptionDlg.Destroy()

		__import__("uikeybind").DestroyWindow()  # MT2009_PLUS_VEKIRION_V1

		if self.systemOptionDlg:
			self.systemOptionDlg.Destroy()

		self.__Initialize()

	def SetOpenHelpWindowEvent(self, event):
		self.eventOpenHelpWindow = event

	def OpenDialog(self):
		self.Show()

	def __ClickChangeCharacterButton(self):
		import autologin
		autologin.NoteManualExit()
		self.Close()

		net.ExitGame()

	def __OnClosePopupDialog(self):
		self.popup = None

	def __ClickLogOutButton(self):
		import autologin
		autologin.NoteManualExit()
		if SYSTEM_MENU_FOR_PORTAL:
			if app.loggined:
				self.Close()
				net.ExitApplication()
			else:
				self.Close()
				net.LogOutGame()
		else:
			self.Close()
			net.LogOutGame()


	def __ClickExitButton(self):
		self.Close()
		net.ExitApplication()

	def __ClickSystemOptionButton(self):
		self.Close()

		if not self.systemOptionDlg:
			self.systemOptionDlg = uiSystemOption.OptionDialog()

		self.systemOptionDlg.Show()

	def __ClickKeybindButton(self):  # MT2009_PLUS_VEKIRION_V1 (Autor: Vekirion): Skroty klawiszowe
		self.Close()
		__import__("uikeybind").OpenWindow()

	def __ClickMonsterCardButton(self):  # MT2009_PLUS_MONSTER_CARDS_V1 (Autor: Digi Rasta): Karty Potworow
		self.Close()
		__import__("monstercard").Toggle()

	def __ClickExtraOptionButton(self):  # MT2009_PLUS_DIGI_CLIENT_QOL_V1 (Autor: Digi Rasta): Opcje dodatkowe
		self.Close()
		if not self.extraOptionDlg:
			self.extraOptionDlg = __import__("uiopcjedodatkowe").ExtraOptionsWindow()
		self.extraOptionDlg.Open()

	def __ClickGameOptionButton(self):
		self.Close()

		if not self.gameOptionDlg:
			self.gameOptionDlg = uiGameOption.OptionDialog()
			self.gameOptionDlg.SetToolTip(self.toolTip)

		self.gameOptionDlg.Show()

	# MT2009_PLUS_OPTIONS_TABS_V1: "Opcje Gry" - Gra, System, Dodatkowe and Skroty as tabs of one
	# window (uioptionstabs.py); the pages are the dialogs above, made on first use as before.
	def __ClickOptionsButton(self):
		self.Close()
		if self.optionsTabs is None:
			self.optionsTabs = __import__("uioptionstabs").OptionsTabs(self)
		self.optionsTabs.Open()

	def GetOptionPage(self, key):
		if key == "game":
			if not self.gameOptionDlg:
				self.gameOptionDlg = uiGameOption.OptionDialog()
				self.gameOptionDlg.SetToolTip(self.toolTip)
			return self.gameOptionDlg
		if key == "system":
			if not self.systemOptionDlg:
				self.systemOptionDlg = uiSystemOption.OptionDialog()
			return self.systemOptionDlg
		if key == "extra":
			if not self.extraOptionDlg:
				self.extraOptionDlg = __import__("uiopcjedodatkowe").ExtraOptionsWindow()
			return self.extraOptionDlg
		if key == "keybind":
			return __import__("uikeybind").GetWindow()
		return None

	if app.ENABLE_MOVE_CHANNEL:
		def __ClickMoveChannelButton(self):
			self.Close()
			if self.interface:
				self.interface.ToggleMoveChannelWindow()

	def __ClickHelpButton(self):
		self.Close()

		if None != self.eventOpenHelpWindow:
			self.eventOpenHelpWindow()

	def __ClickSupportButton(self):
		utils.open_url("https://metin2sp.pl/discord")

	def __OnMouseOverSupportButton(self):
		if self.toolTip:
			self.toolTip.ClearToolTip()
			self.toolTip.AppendTextLine(localeInfo.OPEN_URL_INFO)
			self.toolTip.ShowToolTip()

	def __ClickEscapeButton(self):
		net.SendChatPacket("/escape")

	def __OnMouseOverEscapeButton(self):
		if self.toolTip:
			self.toolTip.ClearToolTip()
			self.toolTip.AppendTextLine(localeInfo.TOOLTIP_ESCAPE_OPTION)
			self.toolTip.ShowToolTip()

	def __ClickInGameShopButton(self):
		self.Close()
		eventManager.EventManager().send_event(uiItemShop.EVENT_OPEN_ITEMSHOP)

	def Close(self):
		self.Hide()
		self.__OnMouseOverOut()

		return True

	def OnBlockMode(self, mode):
		uiGameOption.blockMode = mode
		if self.gameOptionDlg:
			self.gameOptionDlg.OnBlockMode(mode)

	def OnChangePKMode(self):
		if self.gameOptionDlg:
			self.gameOptionDlg.OnChangePKMode()

	def __OnMouseOverOut(self):
		if self.toolTip:
			self.toolTip.HideToolTip()

	def OnPressExitKey(self):
		self.Close()
		return True

	def OnPressEscapeKey(self):
		self.Close()
		return True
