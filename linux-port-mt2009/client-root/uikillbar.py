# MT2009_PLUS_DIGI_SERVER_QOL_V1 - the kill bar. Autor: Digi Rasta
# (nowy-system v0.23.0, uikillbar.py; his "Metin2-Kill-Bar" of the Biore list).
#
# The core sends "KillBar <killer race> <weapon subtype> <victim race> <killer>
# <victim>" to every real player of the map when a character kills a character
# with a real player on one side - bot against bot never comes
# (playerbot_digi_qol.h; the kingdoms' two thousand bots would bury it). The
# last rows stand in the upper right corner under the minimap, each for
# SHOW_SECONDS. The window takes no mouse. Icons: mt2009_ui/killbar/*.png
# (his PNGs, 20 x 20).
#
# Python 2.7 as the client has it.
import app
import ui
import wndMgr

ICON = "mt2009_ui/killbar/%s.png"
# The characters' races (MAIN_RACE_*) and the weapons' subtypes (WEAPON_*).
RACE_ICON = {
	0: "warrior_m", 1: "assassin_w", 2: "sura_m", 3: "shaman_w",
	4: "warrior_w", 5: "assassin_m", 6: "sura_w", 7: "shaman_m", 8: "wolfman",
}
WEAPON_ICON = {0: "sword", 1: "dagger", 2: "bow", 3: "twohand", 4: "bell", 5: "fan", 8: "fist"}
MAX_ROWS = 5
SHOW_SECONDS = 6.0
ROW_HEIGHT = 22
WIDTH = 320
TOP = 190
KILLER_COLOR = 0xffeeeeee
VICTIM_COLOR = 0xffff8080


class KillRow(ui.Window):
	def __init__(self, killerRace, weapon, victimRace, killer, victim):
		ui.Window.__init__(self)
		self.AddFlag("not_pick")
		self.widgets = []
		self.time = app.GetTime()
		x = self.__Icon(0, RACE_ICON.get(killerRace, "warrior_m"))
		x = self.__Text(x, killer, KILLER_COLOR)
		x = self.__Icon(x, WEAPON_ICON.get(weapon, "fist"))
		x = self.__Icon(x, RACE_ICON.get(victimRace, "warrior_m"))
		x = self.__Text(x, victim, VICTIM_COLOR)
		self.SetSize(x, ROW_HEIGHT)

	def __del__(self):
		ui.Window.__del__(self)

	def Destroy(self):
		self.Hide()
		for widget in self.widgets:
			widget.Hide()
		self.widgets = []

	def __Icon(self, x, name):
		icon = ui.ImageBox()
		icon.SetParent(self)
		icon.AddFlag("not_pick")
		try:
			icon.LoadImage(ICON % name)
		except Exception:
			pass
		icon.SetPosition(x, 1)
		icon.Show()
		self.widgets.append(icon)
		return x + 22

	def __Text(self, x, text, color):
		line = ui.TextLine()
		line.SetParent(self)
		line.AddFlag("not_pick")
		line.SetPosition(x, 4)
		line.SetText(text)
		line.SetPackedFontColor(color)
		line.SetOutline()
		line.Show()
		self.widgets.append(line)
		return x + line.GetTextSize()[0] + 6


class KillBar(ui.Window):
	def __init__(self):
		ui.Window.__init__(self)
		self.AddFlag("not_pick")
		self.rows = []
		self.SetSize(WIDTH, MAX_ROWS * ROW_HEIGHT)
		self.SetPosition(wndMgr.GetScreenWidth() - WIDTH - 10, TOP)
		self.Show()

	def __del__(self):
		ui.Window.__del__(self)

	def Destroy(self):
		for row in self.rows:
			row.Destroy()
		self.rows = []
		self.Hide()

	def Add(self, killerRace, weapon, victimRace, killer, victim):
		row = KillRow(killerRace, weapon, victimRace, killer, victim)
		row.SetParent(self)
		row.Show()
		self.rows.insert(0, row)
		for old in self.rows[MAX_ROWS:]:
			old.Destroy()
		self.rows = self.rows[:MAX_ROWS]
		self.__Arrange()
		self.Show()
		self.SetTop()

	def __Arrange(self):
		for i, row in enumerate(self.rows):
			row.SetPosition(WIDTH - row.GetWidth(), i * ROW_HEIGHT)

	def OnUpdate(self):
		if not self.rows:
			return
		now = app.GetTime()
		alive = [row for row in self.rows if now - row.time < SHOW_SECONDS]
		if len(alive) != len(self.rows):
			for row in self.rows:
				if row not in alive:
					row.Destroy()
			self.rows = alive
			self.__Arrange()


_bar = []


def Add(killerRace, weapon, victimRace, killer, victim):
	if not _bar:
		_bar.append(KillBar())
	_bar[0].Add(killerRace, weapon, victimRace, killer, victim)


def DestroyWindow():
	"""The game window closes (a warp, a channel change, the logout)."""
	while _bar:
		_bar.pop().Destroy()
