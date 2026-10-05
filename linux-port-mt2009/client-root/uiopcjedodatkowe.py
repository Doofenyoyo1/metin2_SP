# MT2009_PLUS_DIGI_CLIENT_QOL_V1 - "Opcje dodatkowe" (Autor: Digi Rasta; nowy-system 0.19.0,
# paczki Biore BL_HIDE_EFFECT, Hide-Objects, Graphic-Mask-Control, Chat-Log-Viewer).
# The window is the "Dodatkowe" tab of the ESC menu's "Opcje Gry" (MT2009_PLUS_OPTIONS_TABS_V1,
# uioptionstabs.py; it had its own button before).
#   - buff effects / skill auras    exe: app.SetHideEffects (MT2009_PLUS_DIGI_CLIENT_QOL_V1)
#   - player shops                  exe: chrmgr.SetShopsVisible (ikashop entities + shop characters)
#   - trees / objects / clouds / water   background.SetVisiblePart (every exe)
#   - chat and whispers to logs/czat_YYYY-MM-DD.txt   exe: app.SetChatLog
# A row whose exe call is missing is not shown (and its setting is never applied).
# Settings for the whole client (every character): opcje_dodatkowe.cfg, key=0/1.
# Apply() is called by game.py every frame and acts once a second, so a new map, a warp
# or shops that came into view get the settings by themselves.
# Python 2.7 as the client has it; texts CP1250 as escapes.
import app
import background
import chrmgr
import ui
from _weakref import proxy

CONFIG_FILE = "opcje_dodatkowe.cfg"
# The package's check box pictures (its ui/game/refine/checkbox.tga / checked.tga), in our root pack.
# The client's d:/ymir work/ui/game/quest/quest_check*.tga are not in the players' packs (2.0.46
# syserr: "Failed to load image"); a picture that cannot be loaded falls back to "[x]" / "[ ]".
CHECKBOX_IMAGE = "mt2009_ui/checkbox/checkbox.tga"
CHECKED_IMAGE = "mt2009_ui/checkbox/checked.tga"


def _HasEffects():
	return hasattr(app, "SetHideEffects")


def _HasShops():
	return hasattr(chrmgr, "SetShopsVisible")


def _HasChatLog():
	return hasattr(app, "SetChatLog")


def _HasParts():
	return hasattr(background, "SetVisiblePart") and hasattr(background, "PART_TREE")


# (key, text, does the exe have it)
OPTIONS = (
	("buff", "Ukryj efekty wzmocnie\xf1 (szaman)", _HasEffects),
	("skill", "Ukryj aury umiej\xeatno\x9cci", _HasEffects),
	("shops", "Ukryj sklepy graczy", _HasShops),
	("trees", "Ukryj drzewa", _HasParts),
	("objects", "Ukryj budynki i obiekty", _HasParts),
	("clouds", "Ukryj chmury", _HasParts),
	("water", "Ukryj wod\xea", _HasParts),
	("chatlog", "Zapisuj czat do pliku (folder logs)", _HasChatLog),
	# MT2009_PLUS_KILL_SOUND_SWITCH_V1: the kill streak's sounds after a boss, a
	# metin or a player (digiserverqol.py, KillSound) - off unless ticked (the
	# owner, 3 October).
	("killsound", "D\x9fwi\xeaki zab\xf3jstw (boss, metin, gracz)", lambda: True),
	# MT2009_PLUS_SHOP_CHEAP_MARK_V1: the cheap offers' mark in the offline shops (offlineshopguest.py).
	("shopcheap_off", "Nie pod\x9cwietlaj tanich ofert w sklepikach", lambda: True),
)
PARTS = (
	("trees", "PART_TREE"),
	("objects", "PART_OBJECT"),
	("clouds", "PART_CLOUD"),
	("water", "PART_WATER"),
)

_settings = None
_state = {"next": 0.0, "partsHidden": {}, "shopsHidden": False, "effects": None, "chatlog": None}


def _Open(name, mode):
	try:
		return old_open(name, mode)  # the real file open (system.py); open() reads packs
	except NameError:
		return open(name, mode)


def Settings():
	global _settings
	if _settings is None:
		_settings = dict((key, False) for key, text, available in OPTIONS)
		try:
			f = _Open(CONFIG_FILE, "r")
			try:
				for line in f.read().split("\n"):
					key, sep, value = line.strip().partition("=")
					if sep and key in _settings:
						_settings[key] = value.strip() == "1"
			finally:
				f.close()
		except IOError:
			pass
	return _settings


def Save():
	try:
		f = _Open(CONFIG_FILE, "w")
		try:
			for key, value in sorted(Settings().items()):
				f.write("%s=%d\n" % (key, 1 if value else 0))
		finally:
			f.close()
	except IOError:
		pass


def Set(key, value):
	Settings()[key] = value
	Save()
	Apply(True)


def Apply(force=False):
	now = app.GetTime()
	# MT2009_PLUS_EXTRA_OPTIONS_CLOCK_V1: the game's clock starts again after a
	# channel change or a warp - the old "next" lay far ahead, so nothing was
	# applied and the hidden trees came back until it caught up (the owner,
	# 3 October). A clock that went back resets the throttle.
	if now + 2.0 < _state["next"] - 1.0:
		_state["next"] = 0.0
	if not force and now < _state["next"]:
		return
	_state["next"] = now + 1.0
	s = Settings()

	if _HasEffects():
		effects = (s["buff"], s["skill"])
		if effects != _state["effects"]:
			_state["effects"] = effects
			app.SetHideEffects(int(effects[0]), int(effects[1]))
	if _HasChatLog() and s["chatlog"] != _state["chatlog"]:
		_state["chatlog"] = s["chatlog"]
		app.SetChatLog(int(s["chatlog"]))
	if _HasShops():
		# Every second while hidden: shop characters that came into view are hidden too.
		if s["shops"]:
			chrmgr.SetShopsVisible(0)
			_state["shopsHidden"] = True
		elif _state["shopsHidden"]:
			chrmgr.SetShopsVisible(1)
			_state["shopsHidden"] = False

	# Map parts: a new map (a warp, a channel change) starts with every part shown, so a hidden
	# part is hidden again every second; a part switched back on is shown once.
	if _HasParts():
		for key, partName in PARTS:
			part = getattr(background, partName)
			if s[key]:
				background.SetVisiblePart(part, 0)
				_state["partsHidden"][key] = True
			elif _state["partsHidden"].get(key):
				background.SetVisiblePart(part, 1)
				_state["partsHidden"][key] = False


class OptionRow(ui.Window):
	def __init__(self, owner, key, text):
		ui.Window.__init__(self)
		self.owner = proxy(owner)
		self.key = key
		self.textMark = None
		box = mark = None
		try:
			box = ui.ImageBox()
			box.SetParent(self)
			box.AddFlag("not_pick")
			box.LoadImage(CHECKBOX_IMAGE)
			box.SetPosition(0, 3)
			box.Show()
			mark = ui.ImageBox()
			mark.SetParent(self)
			mark.AddFlag("not_pick")
			mark.LoadImage(CHECKED_IMAGE)
			mark.SetPosition(0, -1)
		except Exception:
			# No picture: a text mark, so a missing file never stops the window.
			for image in (box, mark):
				if image:
					image.Hide()
			box = mark = None
			textMark = ui.TextLine()
			textMark.SetParent(self)
			textMark.AddFlag("not_pick")
			textMark.SetPosition(0, 1)
			textMark.SetText("[ ]")
			textMark.SetOutline()
			textMark.Show()
			self.textMark = textMark
		label = ui.TextLine()
		label.SetParent(self)
		label.AddFlag("not_pick")
		label.SetPosition(18, 1)
		label.SetText(text)
		label.SetOutline()
		label.Show()
		self.box = box
		self.mark = mark
		self.label = label
		self.SetSize(18 + label.GetTextSize()[0], 18)

	def __del__(self):
		ui.Window.__del__(self)

	def SetChecked(self, checked):
		if self.textMark:
			self.textMark.SetText("[x]" if checked else "[ ]")
		elif checked:
			self.mark.Show()
		else:
			self.mark.Hide()

	def OnMouseLeftButtonUp(self):
		self.owner.Toggle(self.key)
		return True


class ExtraOptionsWindow(ui.BoardWithTitleBar):
	WIDTH = 305  # MT2009_PLUS_OPTIONS_TABS_V1: was 270; as wide as Opcje Systemowe, under the tab row
	TOP = 36
	ROW = 22

	def __init__(self):
		ui.BoardWithTitleBar.__init__(self)
		self.rows = {}
		self.AddFlag("movable")
		self.AddFlag("float")
		self.SetTitleName("Opcje dodatkowe")
		self.SetCloseEvent(ui.__mem_func__(self.Close))
		y = self.TOP
		for key, text, available in OPTIONS:
			if not available():
				continue
			row = OptionRow(self, key, text)
			row.SetParent(self)
			row.SetPosition(18, y)
			row.Show()
			self.rows[key] = row
			y += self.ROW
		self.SetSize(self.WIDTH, y + 14)
		self.Refresh()

	def __del__(self):
		ui.BoardWithTitleBar.__del__(self)

	def Destroy(self):
		self.rows = {}
		self.Hide()

	def Refresh(self):
		s = Settings()
		for key, row in self.rows.items():
			row.SetChecked(s[key])

	def Toggle(self, key):
		Set(key, not Settings()[key])
		self.Refresh()

	def Open(self):
		self.Refresh()
		self.SetCenterPosition()
		self.SetTop()
		self.Show()

	def Close(self):
		self.Hide()

	def OnPressEscapeKey(self):
		self.Close()
		return True
