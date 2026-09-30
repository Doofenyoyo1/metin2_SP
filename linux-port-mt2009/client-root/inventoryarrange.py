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
# The texts are CP1250, the client's own, written as escapes so the file stays
# ASCII, each a Polish and English pair (playerbot_lang.T): English for a
# client set to any language but Polish. Python 2.7 as the client has it.

import app
import clientclock
import chat
import mouseModule
import net
import uiPrivateShopBuilder
from playerbot_lang import T

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

MSG_DONE = T('Uporz\xb9dkowano ekwipunek: przestawiono %d, scalono stos\xf3w: %d.',
	'Inventory sorted: %d moved, %d stacks merged.')
MSG_NOTHING = T('Ekwipunek jest ju\xbf uporz\xb9dkowany.', 'The inventory is already sorted.')
MSG_BUSY = T('Nie mo\xbfna teraz uporz\xb9dkowa\xe6 ekwipunku - zamknij handel, sklep lub inne okno.',
	'The inventory cannot be sorted now - close the trade, the shop or the other window.')
MSG_COOLDOWN = T('Odczekaj chwil\xea przed kolejnym porz\xb9dkowaniem.', 'Wait a moment before sorting again.')
MSG_NO_LAYOUT = T('Nie uda\xb3o si\xea u\xb3o\xbfy\xe6 ekwipunku - nic nie zmieniono.',
	'The inventory could not be laid out - nothing was changed.')
MSG_DEAD = T('Nie mo\xbfesz porz\xb9dkowa\xe6 ekwipunku po \x9cmierci.', 'You cannot sort the inventory while dead.')
MSG_INCONSISTENT = T('Ekwipunek jest w nieoczekiwanym stanie - nic nie zmieniono. Zg\xb3o\x9c to na GitHubie.',
	'The inventory is in an unexpected state - nothing was changed. Please report it on GitHub.')
MSG_UNSUPPORTED = T('Serwer nie obs\xb3uguje porz\xb9dkowania ekwipunku.', 'The server cannot sort the inventory.')
MSG_ATTACHED = T('Od\xb3\xf3\xbf najpierw przedmiot trzymany kursorem.', 'Put down the item on your cursor first.')
MSG_SHOP = T('Nie mo\xbfna porz\xb9dkowa\xe6 ekwipunku podczas otwierania sklepu.',
	'The inventory cannot be sorted while a shop is being opened.')

_state = {'pendingUntil': 0.0}


def _int(value):
	try:
		return int(value)
	except (TypeError, ValueError):
		return -1


def IsPending():
	return clientclock.Now() < _state['pendingUntil']


def Request():
	if IsPending():
		return False
	if mouseModule.mouseController.isAttached():
		chat.AppendChat(chat.CHAT_TYPE_INFO, MSG_ATTACHED)
		return False
	if uiPrivateShopBuilder.IsBuildingPrivateShop():
		chat.AppendChat(chat.CHAT_TYPE_INFO, MSG_SHOP)
		return False
	_state['pendingUntil'] = clientclock.Now() + PENDING_TIMEOUT
	net.SendChatPacket('/inventory_arrange')
	return True


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
	chat.AppendChat(chat.CHAT_TYPE_INFO, Message(_int(code), max(0, _int(moved)), max(0, _int(merged))))
