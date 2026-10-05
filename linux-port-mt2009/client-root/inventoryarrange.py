# "Scal i uporzadkuj" - the inventory's AutoStackButton (uiinventory.py).
#
# One request to the server, which pours the stacks together and lays the four
# bag pages out again (playerbot_arrange.cpp), and one answer back:
# "InventoryArrangeResult <code> <moved> <merged> <units>" (game.py). The old
# button sent a move for every pair of stacks - three hundred in a frame, which
# the server's flood limit closed the connection on - and the pump that spread
# them out (autostackpump.py) could only pour stacks together, never order a
# page.
#
# The chat says what the server did once it has done it, not when the button
# is clicked. A second click while the first request is out does nothing, and
# an answer that never comes (a server without the command) frees the button
# after PENDING_TIMEOUT seconds. Nothing is sent while an item hangs on the
# cursor or a private shop is being built: both point at cells the server is
# about to change.
#
# MT2009_PLUS_INVENTORY_SORT_LOCK_V1: the cells locked with Alt + left click
# (inventorysortlock.py) go with both requests as "keep=<hex>", and the
# server leaves those items where they stand. A server without the lock
# answers RESULT_BAD_REQUEST to the extra word; that is said as such.
#
# The texts are CP1250, the client's own, written as escapes so the file stays
# ASCII. Python 2.7 as the client has it.

import app
import clientclock
import chat
import mouseModule
import net
import uiPrivateShopBuilder
import ui
import wndMgr

PENDING_TIMEOUT = 5.0

# playerbot_arrange.h, EResult.
RESULT_DONE = 0
RESULT_NOTHING = 1
RESULT_BUSY = 2
RESULT_COOLDOWN = 3
RESULT_NO_LAYOUT = 4
RESULT_DEAD = 5
RESULT_INCONSISTENT = 6
RESULT_UNSUPPORTED = 7
RESULT_BAD_REQUEST = 8

MSG_DONE = 'Uporz\xb9dkowano ekwipunek: przestawiono %d, scalono stos\xf3w: %d.'
MSG_NOTHING = 'Ekwipunek jest ju\xbf uporz\xb9dkowany.'
MSG_BUSY = 'Nie mo\xbfna teraz uporz\xb9dkowa\xe6 ekwipunku - zamknij handel, sklep lub inne okno.'
MSG_COOLDOWN = 'Odczekaj chwil\xea przed kolejnym porz\xb9dkowaniem.'
MSG_NO_LAYOUT = 'Nie uda\xb3o si\xea u\xb3o\xbfy\xe6 ekwipunku - nic nie zmieniono.'
MSG_DEAD = 'Nie mo\xbfesz porz\xb9dkowa\xe6 ekwipunku po \x9cmierci.'
MSG_INCONSISTENT = 'Ekwipunek jest w nieoczekiwanym stanie - nic nie zmieniono. Zg\xb3o\x9c to na Discordzie.'
MSG_UNSUPPORTED = 'Serwer nie obs\xb3uguje porz\xb9dkowania ekwipunku.'
MSG_ATTACHED = 'Od\xb3\xf3\xbf najpierw przedmiot trzymany kursorem.'
MSG_SHOP = 'Nie mo\xbfna porz\xb9dkowa\xe6 ekwipunku podczas otwierania sklepu.'

MSG_MERGED = 'Po\xb3\xb9czono stosy: %d. Reszta ekwipunku zosta\xb3a na miejscu.'
MSG_NOTHING_MERGE = 'Nie ma stos\xf3w do po\xb3\xb9czenia.'
MSG_NO_LOCK = 'Serwer nie obs\xb3uguje blokady sortowania (Alt+LPM) - zaktualizuj serwer albo zdejmij blokady.'

# "Uporzadkuj" or "tylko scal stosy" (the operator, 28 September: "mozna
# wybrac albo samo ukladanie z laczeniem w stacki albo samo laczenie w
# stacki bez sortowania"): the button opens ChoiceWindow, and the merge
# asks "/inventory_arrange merge" (server-patches/playerqol).
MODE_ARRANGE = 0
MODE_MERGE = 1

_state = {'pendingUntil': 0.0, 'mode': MODE_ARRANGE, 'choice': None, 'keep': False}


def _int(value):
	try:
		return int(value)
	except (TypeError, ValueError):
		return -1


def IsPending():
	return clientclock.Now() < _state['pendingUntil']


def Request(mode=MODE_ARRANGE):
	if IsPending():
		return False
	if mouseModule.mouseController.isAttached():
		chat.AppendChat(chat.CHAT_TYPE_INFO, MSG_ATTACHED)
		return False
	if uiPrivateShopBuilder.IsBuildingPrivateShop():
		chat.AppendChat(chat.CHAT_TYPE_INFO, MSG_SHOP)
		return False
	words = ['/inventory_arrange']
	if mode == MODE_MERGE:
		words.append('merge')
	keep = ''
	try:
		import inventorysortlock
		keep = inventorysortlock.KeepArgument()
	except Exception:
		keep = ''
	if keep:
		words.append(keep)
	_state['pendingUntil'] = clientclock.Now() + PENDING_TIMEOUT
	_state['mode'] = mode
	_state['keep'] = bool(keep)
	net.SendChatPacket(' '.join(words))
	return True


class ChoiceWindow(ui.ThinBoard):
	WIDTH = 196
	HEIGHT = 70

	def __init__(self):
		ui.ThinBoard.__init__(self)
		self.AddFlag('float')
		self.SetSize(self.WIDTH, self.HEIGHT)
		self.widgets = []
		self._Btn(8, 8, 'U\xb3\xf3\xbf i scal', MODE_ARRANGE)
		self._Btn(8, 36, 'Tylko scal stosy', MODE_MERGE)

	def _Btn(self, x, y, text, mode):
		button = ui.Button()
		button.SetParent(self)
		button.SetPosition(x, y)
		button.SetUpVisual('d:/ymir work/ui/public/xlarge_button_01.sub')
		button.SetOverVisual('d:/ymir work/ui/public/xlarge_button_02.sub')
		button.SetDownVisual('d:/ymir work/ui/public/xlarge_button_03.sub')
		button.SetText(text)
		button.SAFE_SetEvent(self.OnChoose, mode)
		button.Show()
		self.widgets.append(button)

	def OnChoose(self, mode):
		self.Hide()
		Request(mode)

	def OpenAt(self, x, y):
		(sw, sh) = (wndMgr.GetScreenWidth(), wndMgr.GetScreenHeight())
		self.SetPosition(max(0, min(x - self.WIDTH // 2, sw - self.WIDTH)), max(0, min(y + 12, sh - self.HEIGHT)))
		self.Show()
		self.SetTop()

	def OnPressEscapeKey(self):
		self.Hide()
		return True

	def Destroy(self):
		self.Hide()
		self.widgets = []


def OpenChoice():
	"""The inventory's button: where the cursor is, the two ways to tidy."""
	window = _state['choice']
	if window is None:
		window = ChoiceWindow()
		_state['choice'] = window
	if window.IsShow():
		window.Hide()
		return
	(x, y) = wndMgr.GetMousePosition()
	window.OpenAt(x, y)


def DestroyChoice():
	window = _state['choice']
	_state['choice'] = None
	if window is not None:
		window.Destroy()


def Message(code, moved, merged):
	if code == RESULT_DONE:
		return MSG_DONE % (moved, merged)
	if code == RESULT_NOTHING:
		return MSG_NOTHING
	if code == RESULT_BUSY:
		return MSG_BUSY
	if code == RESULT_COOLDOWN:
		return MSG_COOLDOWN
	if code == RESULT_NO_LAYOUT:
		return MSG_NO_LAYOUT
	if code == RESULT_DEAD:
		return MSG_DEAD
	if code == RESULT_INCONSISTENT:
		return MSG_INCONSISTENT
	return MSG_UNSUPPORTED


def OnResult(code='0', moved='0', merged='0', units='0'):
	_state['pendingUntil'] = 0.0
	code = _int(code)
	if code == RESULT_BAD_REQUEST and _state['keep']:
		chat.AppendChat(chat.CHAT_TYPE_INFO, MSG_NO_LOCK)
		return
	if _state['mode'] == MODE_MERGE and code == RESULT_DONE:
		chat.AppendChat(chat.CHAT_TYPE_INFO, MSG_MERGED % max(0, _int(merged)))
		return
	if _state['mode'] == MODE_MERGE and code == RESULT_NOTHING:
		chat.AppendChat(chat.CHAT_TYPE_INFO, MSG_NOTHING_MERGE)
		return
	chat.AppendChat(chat.CHAT_TYPE_INFO, Message(code, max(0, _int(moved)), max(0, _int(merged))))
