# Orders to the bots of the player's guild - the guild window's "Boty gildii"
# button and the small window it opens (Derpsonkowy95, 28 September: "bot
# moglby reagowac na kluczowe slowa ... i przy podaniu lokalizacji bot moglby
# tam przyjsc"; the operator: "warto zrobic to w GUI jako komendy gildyjne").
#
# Three orders, each one command to the server and nothing else:
#
#   /gildia_boty pomoc      - "Pomocy!": the free bots of the guild come and
#                             fight what the player fights
#   /gildia_boty exp        - "Expimy razem": they come and hunt round the
#                             player for a while
#   /gildia_boty wracajcie  - "Wracajcie": the ones called go back
#
# The command carries no place. The server sends the bots to where this
# character stands on it (playerbot_guild_orders.h), so there is nothing here
# a client could get wrong or make up, and it answers in the chat who is
# coming and why the rest are not. Who may order is the server's rule too:
# the guild's master, and the ranks the master allowed to use the guild's
# skills (the grade page's fourth box). The button on the guild window shows
# only to them, the way the stock "declare war" shows only to the master; a
# client whose exe cannot say shows it, and the server answers.
#
# uiguild.py's GuildWindow makes the button on its info page, shows it or
# hides it with the page's refresh, closes this window when the player leaves
# the guild and destroys it with its own (four small insertions of
# clientrootify.py into the stock script).
#
# Python 2.7 as the client has it; the Polish letters are CP1250 escapes, and
# every text is a Polish and English pair (playerbot_lang.T), read when the
# window is built.

import clientclock
import net
import ui
from playerbot_lang import T

COMMAND = '/gildia_boty '
ORDER_HELP = 'pomoc'
ORDER_HUNT = 'exp'
ORDER_RELEASE = 'wracajcie'
# A double click is one order: a second one inside this is not sent. The
# server keeps its own clock between two calls and says so in the chat.
CLICK_SPACING = 1.0

# The button's place on the guild window's info page (uiscript/
# guildwindow_guildinfopage.py): the bottom row holds "Wyplac" at 78 and the
# master's "Wypowiedz wojne" at 265, both 88 wide, and this one fits the gap.
BUTTON_X = 171
BUTTON_Y = 265


def MayOrder():
	"""Whether this player gives the guild's bots orders: the master, or a rank
	with the right to use the guild's skills. The server decides; this only
	keeps a button from those it would refuse. An exe that cannot say is
	answered by the server."""
	try:
		import guild
		import player
		if not guild.IsGuildEnable():
			return False
		if player.GetMainCharacterName() == guild.GetGuildMasterName():
			return True
		return bool(guild.MainPlayerHasAuthority(guild.AUTH_SKILL))
	except Exception:
		return True


def SendOrder(order):
	net.SendChatPacket(COMMAND + order)


class GuildBotsWindow(ui.BoardWithTitleBar):
	WIDTH = 316
	HEIGHT = 132

	def __init__(self):
		ui.BoardWithTitleBar.__init__(self)
		self.widgets = []
		self.lastSent = None
		self.AddFlag('movable')
		self.AddFlag('float')
		self.SetSize(self.WIDTH, self.HEIGHT)
		self.SetTitleName(T('Boty gildii', 'Guild bots'))
		self.SetCloseEvent(ui.__mem_func__(self.Close))
		self.Build()
		self.Refresh()

	def Build(self):
		self.introLine = self._Label(self.WIDTH // 2, 36, T('Wezwij wolne boty gildii z tej cz\xea\x9cci \x9cwiata.',
			'Call the free bots of your guild near you.'))
		# Three large buttons (88 wide) a hundred apart, as in the companion's
		# window. No tooltips: a button's is drawn nineteen pixels over it,
		# on the line above, and the two lines under the buttons say what
		# each order does.
		self.helpButton = self._Btn(14, 58, T('Pomocy!', 'Help me!'), ORDER_HELP)
		self.huntButton = self._Btn(114, 58, T('Expimy razem', 'Hunt with me'), ORDER_HUNT)
		self.releaseButton = self._Btn(214, 58, T('Wracajcie', 'Go back'), ORDER_RELEASE)
		self.statusLine = self._Label(self.WIDTH // 2, 90, '')
		self.detailLine = self._Label(self.WIDTH // 2, 106, '')

	def _Label(self, x, y, text):
		line = ui.TextLine()
		line.SetParent(self)
		line.SetPosition(x, y)
		line.SetHorizontalAlignCenter()
		line.SetText(text)
		line.Show()
		self.widgets.append(line)
		return line

	def _Btn(self, x, y, text, order):
		button = ui.Button()
		button.SetParent(self)
		button.SetPosition(x, y)
		button.SetUpVisual('d:/ymir work/ui/public/large_button_01.sub')
		button.SetOverVisual('d:/ymir work/ui/public/large_button_02.sub')
		button.SetDownVisual('d:/ymir work/ui/public/large_button_03.sub')
		button.SetText(text)
		button.SAFE_SetEvent(self.OnOrder, order)
		button.Show()
		self.widgets.append(button)
		return button

	def Refresh(self):
		if MayOrder():
			self.SetStatus(T('Pomocy: walcz\xb9 z tym, co ci\xea atakuje.', 'Help: they fight what attacks you.'),
				T('Expimy: poluj\xb9 wok\xf3\xb3 ciebie.', 'Hunt: they hunt around you.'))
		else:
			self.SetStatus(T('Rozkazy wydaje mistrz gildii', 'Orders come from the guild master'),
				T('i rangi z prawem do umiej\xeatno\x9cci gildii.', 'and the ranks allowed to use guild skills.'))

	def SetStatus(self, status, detail):
		self.statusLine.SetText(status)
		self.detailLine.SetText(detail)

	def OnOrder(self, order):
		now = clientclock.Now()
		if self.lastSent is not None and now - self.lastSent < CLICK_SPACING:
			self.SetStatus(T('Chwil\xea...', 'A moment...'), '')
			return
		self.lastSent = now
		SendOrder(order)
		self.SetStatus(T('Rozkaz wys\xb3any.', 'Order sent.'), T('Kto idzie, powie ci czat.', 'The chat says who is coming.'))

	def Open(self):
		self.Refresh()
		self.Show()
		self.SetTop()

	def Close(self):
		self.Hide()

	def OnPressEscapeKey(self):
		self.Close()
		return True

	def Destroy(self):
		self.Hide()
		self.widgets = []


_window = {'window': None}


def GetWindow():
	if _window['window'] is None:
		window = GuildBotsWindow()
		window.SetCenterPosition()
		_window['window'] = window
	return _window['window']


def ToggleWindow():
	window = GetWindow()
	if window.IsShow():
		window.Close()
	else:
		window.Open()


def CloseWindow():
	window = _window['window']
	if window is not None and window.IsShow():
		window.Close()


def Destroy():
	window = _window['window']
	if window is not None:
		window.Destroy()
	_window['window'] = None


def RefreshGuildWindowButton(button):
	if MayOrder():
		button.Show()
	else:
		button.Hide()


def MakeGuildWindowButton(page):
	"""The info page's "Boty gildii": opens the orders window. A module
	function for its event, so the button holds no reference to the page's
	window and the page's ClearDictionary has nothing of ours to break."""
	button = ui.Button()
	button.SetParent(page)
	button.SetPosition(BUTTON_X, BUTTON_Y)
	button.SetUpVisual('d:/ymir work/ui/public/large_button_01.sub')
	button.SetOverVisual('d:/ymir work/ui/public/large_button_02.sub')
	button.SetDownVisual('d:/ymir work/ui/public/large_button_03.sub')
	button.SetText(T('Boty gildii', 'Guild bots'))
	button.SetToolTipText(T('Rozkazy dla bot\xf3w twojej gildii', "Orders to your guild's bots"))
	button.SetEvent(ToggleWindow)
	RefreshGuildWindowButton(button)
	return button
