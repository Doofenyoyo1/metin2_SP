# MT2009_PLUS_OPTIONS_TABS_V1 - "Opcje Gry": one button of the Esc menu
# (uisystem.py, uiscript/systemdialog.py) in place of the four option buttons,
# with a row of tabs on top of the window:
#   Gra        - Opcje Gry (uigameoption.py)
#   System     - Opcje Systemowe (uisystemoption.py)
#   Dodatkowe  - Opcje dodatkowe (uiopcjedodatkowe.py, Autor: Digi Rasta)
#   Skroty     - Skroty klawiszowe (uikeybind.py, Autor: Vekirion)
#
# Only the look changes (the owner, 5 October): every tab is the window it was,
# with its settings, saving and behaviour. The tab row is a small window of its
# own standing on the shown page's top edge; a tab click closes that page and
# opens the chosen one at the same place (same centre, same top), so the tabs
# stay under the mouse. The row follows the page when it is dragged and goes
# away when the page is closed (X, Esc, the keys' "Zapisz").
#
# The keys page keeps its draft (unsaved keys) while another tab is shown in
# the same opening of the options; closing the options drops it, as before.
#
# Python 2.7 as the client has it; the texts are CP1250 escapes.

import ui
import wndMgr
from _weakref import proxy

# 78x19, three states; the client's etc pack (the safe box uses the 52 px ones).
TAB_IMAGE = "d:/ymir work/ui/game/windows/tab_button_large_%02d.sub"
TAB_WIDTH = 78
TAB_HEIGHT = 19
TAB_GAP = 1

# (page key for SystemDialog.GetOptionPage, tab text)
PAGES = (
	("game", "Gra"),
	("system", "System"),
	("extra", "Dodatkowe"),
	("keybind", "Skr\xf3ty"),
)

# The last tab and place for the client's whole run (a new character too).
_last = {"index": 0, "anchor": None}


class OptionsTabs(ui.Window):

	def __init__(self, owner):
		ui.Window.__init__(self)
		# The Esc menu's handlers run on a weak proxy of SystemDialog already
		# (SAFE_SetEvent), and a proxy of a proxy is a TypeError.
		try:
			self.owner = proxy(owner)
		except TypeError:
			self.owner = owner
		self.tabs = []
		self.current = -1
		self.page = None		# the page shown
		self.pagePos = None		# where it was last seen
		self.opened = {}		# pages opened in this opening of the options
		self.AddFlag("float")
		self.SetSize(len(PAGES) * TAB_WIDTH + (len(PAGES) - 1) * TAB_GAP, TAB_HEIGHT)
		for index, (key, text) in enumerate(PAGES):
			tab = ui.RadioButton()
			tab.SetParent(self)
			tab.SetUpVisual(TAB_IMAGE % 1)
			tab.SetOverVisual(TAB_IMAGE % 2)
			tab.SetDownVisual(TAB_IMAGE % 3)
			tab.SetPosition(index * (TAB_WIDTH + TAB_GAP), 0)
			tab.SetText(text)
			tab.SAFE_SetEvent(self.SelectPage, index)
			tab.Show()
			self.tabs.append(tab)
		self.Hide()

	def __del__(self):
		ui.Window.__del__(self)

	# Not ui.WindowDestroy: the pages belong to SystemDialog, which destroys them.
	def Destroy(self):
		self.page = None
		self.opened = {}
		for tab in self.tabs:
			tab.Hide()
		self.tabs = []
		self.owner = None
		self.Hide()

	def Open(self):
		if self.__PageShown():
			self.page.SetTop()
			self.SetTop()
			return
		self.page = None
		self.opened = {}
		self.__ShowPage(_last["index"])

	def Close(self):
		self.__HidePage()
		self.Hide()

	def SelectPage(self, index):
		if index == self.current and self.__PageShown():
			self.tabs[index].Down()
			return
		self.__ShowPage(index)

	def __PageShown(self):
		if self.page is None:
			return False
		try:
			return self.page.IsShow()
		except ReferenceError:
			return False

	def __ShowPage(self, index):
		if self.owner is None:
			return
		anchor = self.__Anchor()
		self.__HidePage()
		key = PAGES[index][0]
		page = self.owner.GetOptionPage(key)
		if not page:
			return
		self.current = index
		_last["index"] = index
		self.page = page
		# Each page opened as its old Esc menu button did.
		if key == "keybind" and key in self.opened and hasattr(page, "Resume"):
			page.Resume()
		elif key in ("keybind", "extra"):
			page.Open()
		else:
			page.Show()
		self.opened[key] = True
		self.__Place(anchor)
		page.SetTop()
		for i, tab in enumerate(self.tabs):
			if i == index:
				tab.Down()
			else:
				tab.SetUp()
		self.Show()
		self.SetTop()

	# The page shown is closed as its X would; the keys' page only hidden
	# (its draft kept for this opening).
	def __HidePage(self):
		page = self.page
		self.page = None
		if page is None:
			return
		try:
			if PAGES[self.current][0] == "keybind" and hasattr(page, "Suspend"):
				page.Suspend()
			else:
				page.Close()
		except ReferenceError:
			pass

	# (centre x, top) of the page shown, else of the last one.
	def __Anchor(self):
		if self.__PageShown():
			x, y = self.page.GetGlobalPosition()
			return (x + self.page.GetWidth() / 2, y)
		return _last["anchor"]

	def __Place(self, anchor):
		screenWidth, screenHeight = wndMgr.GetScreenWidth(), wndMgr.GetScreenHeight()
		width, height = self.page.GetWidth(), self.page.GetHeight()
		if anchor:
			centre, top = anchor
		else:
			centre = screenWidth / 2
			top = (screenHeight - height - TAB_HEIGHT) / 2 + TAB_HEIGHT
		x = max(0, min(screenWidth - width, centre - width / 2))
		y = max(TAB_HEIGHT, min(screenHeight - height, top))
		self.page.SetPosition(x, y)
		self.__Follow()

	def __Follow(self):
		x, y = self.page.GetGlobalPosition()
		width = self.page.GetWidth()
		self.pagePos = (x, y)
		_last["anchor"] = (x + width / 2, y)
		self.SetPosition(x + (width - self.GetWidth()) / 2, max(0, y - TAB_HEIGHT))

	def OnUpdate(self):
		if not self.__PageShown():
			# The page closed itself (X, Esc, "Zapisz"): the options are closed.
			self.page = None
			self.opened = {}
			self.Hide()
			return
		if tuple(self.page.GetGlobalPosition()) != self.pagePos:
			self.__Follow()

	def OnPressEscapeKey(self):
		if self.__PageShown():
			self.page.OnPressEscapeKey()
		else:
			self.Hide()
		return True
