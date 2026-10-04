import app
import ui
import localeInfo
import uiScriptLocale

ENABLE_HELP_MULTIPAGE = 0

# MT2009_PLUS_VEKIRION_V1 (Autor: Vekirion): the help's key lines follow
# Skroty klawiszowe (keybind.py). (child, actions, form): a line whose actions
# all have their default keys keeps the help's own text; a rebound one reads
# "<text before the colon>: <keys>", the form putting the keys into the rest.
# CP1250.
HELP_KEY_LINES = (
	("help_01", ("move_up", "move_down", "move_left", "move_right"), "%s"),
	("help_06", ("attack",), "lewy przycisk myszy lub %s"),
	("help_07", ("character",), "%s"),
	("help_08", ("skills",), "%s"),
	("help_09", ("quests",), "%s"),
	("help_10", ("inventory",), "%s"),
	("help_stats", ("player_stat",), "%s"),
	("help_11", ("chat_log",), "%s"),
	("help_12", ("atlas",), "%s"),
	("help_13", ("minimap",), "%s"),
	("help_15", ("pickup", "pickup_near"), "%s lub Lewy Przycisk Myszki"),
	("help_16", ("screenshot",), "%s"),
	("help_17", ("guild",), "%s"),
	("help_18", ("messenger",), "%s"),
	("help_19", ("help",), "%s"),
)
HELP_NO_KEY = "(brak klawisza - Esc, Skr\xf3ty klawiszowe)"

class HelpWindow(ui.ScriptWindow):
	def __init__(self):
		ui.ScriptWindow.__init__(self, "TOP_MOST")
		self.eventClose = 0
	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def LoadDialog(self):
		if ENABLE_HELP_MULTIPAGE:
			self.LoadDialogMultiPage()
		else:
			self.LoadDialogSinglePage()

	def LoadDialogSinglePage(self):
		try:
			pyScrLoader = ui.PythonScriptLoader()

			if localeInfo.IsARABIC():
				pyScrLoader.LoadScriptFile(self, uiScriptLocale.LOCALE_UISCRIPT_PATH + "HelpWindow.py")
			else:
				pyScrLoader.LoadScriptFile(self, "UIScript/HelpWindow.py")
		except:
			import exception
			exception.Abort("HelpWindow.LoadDialogSinglePage.LoadScript")

		try:
			GetObject=self.GetChild
			self.btnClose = GetObject("close_button")
		except:
			import exception
			exception.Abort("DialogWindow.LoadDialogSinglePage.BindObject")


	def LoadDialogMultiPage(self):
		try:
			pyScrLoader = ui.PythonScriptLoader()
			pyScrLoader.LoadScriptFile(self, "UIScript/HelpWindow2.py")
		except:
			import exception
			exception.Abort("HelpWindow.LoadDialogMultiPage.LoadScript")

		try:
			GetObject=self.GetChild
			self.btnClose = GetObject("close_button")
			self.pages = {}
			self.btnPages = {}
			self.pages[0] = GetObject("page_1")
			self.pages[1] = GetObject("page_2")
			self.btnPages[0] = GetObject("page_1_button")
			self.btnPages[1] = GetObject("page_2_button")
			self.btnPages[0].SAFE_SetEvent(self.__OnClickPage1)
			self.btnPages[1].SAFE_SetEvent(self.__OnClickPage2)

			self.__SelectPage(0)

		except:
			import exception
			exception.Abort("DialogWindow.LoadDialogMultiPage.BindObject")

	def __OnClickPage1(self):
		self.__SelectPage(0)

	def __OnClickPage2(self):
		self.__SelectPage(1)

	@ui.WindowDestroy
	def Destroy(self):
		self.eventClose = 0
		self.closeButton = 0
		self.pages = {}
		self.btnPages = {}

	def SetCloseEvent(self, event):
		self.eventClose = event
		self.btnClose.SetEvent(event)

	def Open(self):
		self.__RefreshKeyTexts()
		self.Lock()
		self.Show()

	# MT2009_PLUS_VEKIRION_V1: the keys in use, each time the help opens.
	def __RefreshKeyTexts(self):
		try:
			import keybind
		except Exception:
			return
		if not hasattr(self, "helpKeyTexts"):
			self.helpKeyTexts = {}
		for name, actions, form in HELP_KEY_LINES:
			try:
				line = self.GetChild(name)
			except Exception:
				continue
			original = self.helpKeyTexts.setdefault(name, line.GetText())
			if all([keybind.IsDefault(action) for action in actions]):
				line.SetText(original)
				continue
			keys = [keybind.GetText(action, " / ") for action in actions]
			keys = ", ".join([key for key in keys if key]) or HELP_NO_KEY
			head = original.split(":", 1)[0]
			text = head + ": " + (form % keys)
			if name == "help_16" and "(" in original:
				text += " " + original[original.index("("):]
			line.SetText(text)

	def Close(self):
		self.Unlock()
		self.Hide()

	def OnKeyDown(self, key):
		# MT2009_PLUS_VEKIRION_V1: closed by its own key (keybind.py: H unless rebound).
		import keybind
		if keybind.MatchesKey("help", key) and 0 != self.eventClose:
			self.eventClose()

		return True

	def OnIMEReturn(self):
		return True

	def OnPressEscapeKey(self):
		if 0 != self.eventClose:
			self.eventClose()
		return True

	def OnPressExitKey(self):
		if 0 != self.eventClose:
			self.eventClose()
		return True

	def __SelectPage(self, pageIndex):
		for page in self.pages.values():
			page.Hide()
		for btn in self.btnPages.values():
			btn.SetUp()

		self.pages[pageIndex].Show()
		self.btnPages[pageIndex].Down()
