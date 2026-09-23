# The safebox's "Scal i uporzadkuj" and its stacks moved by count, on the
# client (client-root/safeboxtransfer.py), against stub client modules: which
# drops go by the engine's packet and which by a command with a count, what the
# chat says about each answer, and the count dialog the bag's own split uses.
#
#     python tests/safeboxtransfer_test.py
#
# Runs on Python 3 and on the client's Python 2.7
# (docker run --rm -v "$PWD":/w -w /w python:2.7 python tests/safeboxtransfer_test.py).
import os
import sys
import types
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
CLIENT_ROOT = os.path.normpath(os.path.join(HERE, '..', 'linux-port-mt2009', 'client-root'))

STATE = {}

INVENTORY = 1
DRAGON_SOUL_INVENTORY = 3


def reset_state():
	STATE.clear()
	STATE.update({'now': 100.0, 'chat': [], 'sent': [], 'attached': False, 'building': False,
			'bag': {}, 'box': {}, 'dialogs': []})


def module(name, **attrs):
	mod = types.ModuleType(name)
	for key, value in attrs.items():
		setattr(mod, key, value)
	return mod


class StubMouse(object):
	def isAttached(self):
		return STATE['attached']


class StubDialog(object):
	def __init__(self, kind):
		self.kind = kind
		self.calls = []
		STATE['dialogs'].append(self)

	def LoadDialog(self):
		self.calls.append('load')

	def SetAcceptEvent(self, event):
		self.calls.append('accept')
		self.event = event

	def Hide(self):
		self.calls.append('hide')


def install_stubs():
	sys.modules['app'] = module('app', GetTime=lambda: STATE['now'])
	sys.modules['chat'] = module('chat', CHAT_TYPE_INFO=1, AppendChat=lambda kind, text: STATE['chat'].append(text))
	sys.modules['net'] = module('net',
			SendChatPacket=lambda text: STATE['sent'].append(text),
			SendSafeboxItemMovePacket=lambda a, b: STATE['sent'].append(('move', a, b)),
			SendSafeboxCheckinPacket=lambda w, a, b: STATE['sent'].append(('checkin', w, a, b)),
			SendSafeboxCheckoutPacket=lambda a, b: STATE['sent'].append(('checkout', a, b)))
	sys.modules['mouseModule'] = module('mouseModule', mouseController=StubMouse())
	sys.modules['player'] = module('player', INVENTORY=INVENTORY, INVENTORY_PAGE_SIZE=45, INVENTORY_PAGE_COUNT=4,
			GetItemCount=lambda cell: STATE['bag'].get(cell, 0))
	sys.modules['safebox'] = module('safebox', GetItemCount=lambda pos: STATE['box'].get(pos, 0))
	sys.modules['uiPrivateShopBuilder'] = module('uiPrivateShopBuilder', IsBuildingPrivateShop=lambda: STATE['building'])
	sys.modules['uiPickETC'] = module('uiPickETC', PickETCDialog=lambda: StubDialog('etc'))
	sys.modules['uiPickMoney'] = module('uiPickMoney', PickMoneyDialog=lambda: StubDialog('money'))


reset_state()
install_stubs()
sys.path.insert(0, CLIENT_ROOT)
import safeboxtransfer  # noqa: E402


class ArrangeTest(unittest.TestCase):
	def setUp(self):
		reset_state()
		safeboxtransfer._state['arrangeUntil'] = 0.0

	def test_one_click_is_one_command(self):
		self.assertTrue(safeboxtransfer.RequestArrange())
		self.assertEqual(STATE['sent'], ['/safebox_arrange'])
		self.assertEqual(STATE['chat'], [], 'nothing is said before the server answers')

	def test_a_second_click_waits_for_the_answer(self):
		safeboxtransfer.RequestArrange()
		STATE['now'] += 1.0
		self.assertFalse(safeboxtransfer.RequestArrange())
		safeboxtransfer.OnArrangeResult('1', '0', '0', '0')
		self.assertTrue(safeboxtransfer.RequestArrange())
		self.assertEqual(len(STATE['sent']), 2)

	def test_no_answer_frees_the_button(self):
		safeboxtransfer.RequestArrange()
		STATE['now'] += safeboxtransfer.PENDING_TIMEOUT + 0.1
		self.assertTrue(safeboxtransfer.RequestArrange())

	def test_not_with_an_item_on_the_cursor_or_a_shop_being_built(self):
		STATE['attached'] = True
		self.assertFalse(safeboxtransfer.RequestArrange())
		STATE['attached'] = False
		STATE['building'] = True
		self.assertFalse(safeboxtransfer.RequestArrange())
		self.assertEqual(STATE['sent'], [])
		self.assertEqual(STATE['chat'], [safeboxtransfer.MSG_ATTACHED, safeboxtransfer.MSG_SHOP])

	def test_the_answer_is_what_the_chat_says(self):
		safeboxtransfer.OnArrangeResult('0', '20', '4', '310')
		self.assertEqual(STATE['chat'][-1], safeboxtransfer.MSG_ARRANGE_DONE % (20, 4))
		expected = {
			'1': safeboxtransfer.MSG_ARRANGE_NOTHING,
			'2': safeboxtransfer.MSG_ARRANGE_BUSY,
			'3': safeboxtransfer.MSG_ARRANGE_COOLDOWN,
			'4': safeboxtransfer.MSG_ARRANGE_NO_LAYOUT,
			'5': safeboxtransfer.MSG_ARRANGE_DEAD,
			'6': safeboxtransfer.MSG_ARRANGE_INCONSISTENT,
			'7': safeboxtransfer.MSG_UNSUPPORTED,
			'9': safeboxtransfer.MSG_NO_SAFEBOX,
			'garbage': safeboxtransfer.MSG_UNSUPPORTED,
		}
		for code, text in sorted(expected.items()):
			safeboxtransfer.OnArrangeResult(code)
			self.assertEqual(STATE['chat'][-1], text, code)


class DropTest(unittest.TestCase):
	def setUp(self):
		reset_state()

	# Inside the safebox.
	def test_a_whole_stack_onto_a_free_place_keeps_the_packet(self):
		STATE['box'] = {3: 50}
		safeboxtransfer.DropInSafebox(3, 10, 0, False)
		safeboxtransfer.DropInSafebox(3, 10, 50, False)
		safeboxtransfer.DropInSafebox(3, 10, 80, False)
		self.assertEqual(STATE['sent'], [('move', 3, 10)] * 3)

	def test_a_part_inside_the_safebox_is_a_command(self):
		STATE['box'] = {3: 50}
		safeboxtransfer.DropInSafebox(3, 10, 20, False)
		self.assertEqual(STATE['sent'], ['/safebox_move 3 10 20'])

	def test_onto_a_taken_place_inside_the_safebox_is_a_command(self):
		STATE['box'] = {3: 50, 10: 5}
		safeboxtransfer.DropInSafebox(3, 10, 0, True)
		safeboxtransfer.DropInSafebox(3, 10, 7, True)
		self.assertEqual(STATE['sent'], ['/safebox_move 3 10 0', '/safebox_move 3 10 7'])

	def test_onto_itself_is_nothing(self):
		STATE['box'] = {3: 50}
		safeboxtransfer.DropInSafebox(3, 3, 0, True)
		self.assertEqual(STATE['sent'], [])

	# The bag into the safebox.
	def test_a_whole_stack_of_the_bag_onto_a_free_place_keeps_the_packet(self):
		STATE['bag'] = {12: 200}
		safeboxtransfer.DropIntoSafebox(INVENTORY, 12, 4, 200, False)
		self.assertEqual(STATE['sent'], [('checkin', INVENTORY, 12, 4)])

	def test_a_part_of_the_bag_into_the_safebox_is_a_command(self):
		STATE['bag'] = {12: 200}
		safeboxtransfer.DropIntoSafebox(INVENTORY, 12, 4, 60, False)
		self.assertEqual(STATE['sent'], ['/safebox_put 12 4 60'])

	def test_the_bag_onto_a_stack_in_the_safebox_pours(self):
		STATE['bag'] = {12: 200}
		safeboxtransfer.DropIntoSafebox(INVENTORY, 12, 4, 0, True)
		safeboxtransfer.DropIntoSafebox(INVENTORY, 12, 4, 30, True)
		self.assertEqual(STATE['sent'], ['/safebox_put 12 4 0', '/safebox_put 12 4 30'])

	def test_the_last_bag_page_takes_part_and_the_horse_page_does_not(self):
		STATE['bag'] = {179: 10, 180: 10}
		safeboxtransfer.DropIntoSafebox(INVENTORY, 179, 4, 3, False)
		safeboxtransfer.DropIntoSafebox(INVENTORY, 180, 4, 3, False)
		safeboxtransfer.DropIntoSafebox(INVENTORY, 180, 4, 3, True)
		self.assertEqual(STATE['sent'], ['/safebox_put 179 4 3', ('checkin', INVENTORY, 180, 4)])

	def test_another_window_keeps_the_packet_and_never_pours(self):
		safeboxtransfer.DropIntoSafebox(DRAGON_SOUL_INVENTORY, 7, 4, 0, False)
		safeboxtransfer.DropIntoSafebox(DRAGON_SOUL_INVENTORY, 7, 4, 0, True)
		self.assertEqual(STATE['sent'], [('checkin', DRAGON_SOUL_INVENTORY, 7, 4)])

	# The safebox into the bag.
	def test_a_whole_stack_into_a_free_cell_of_the_bag_keeps_the_packet(self):
		STATE['box'] = {9: 40}
		safeboxtransfer.DropIntoBag(9, 33, 0, False)
		safeboxtransfer.DropIntoBag(9, 33, 40, False)
		self.assertEqual(STATE['sent'], [('checkout', 9, 33)] * 2)

	def test_a_part_into_the_bag_is_a_command(self):
		STATE['box'] = {9: 40}
		safeboxtransfer.DropIntoBag(9, 33, 15, False)
		self.assertEqual(STATE['sent'], ['/safebox_take 9 33 15'])

	def test_onto_a_stack_in_the_bag_pours(self):
		STATE['box'] = {9: 40}
		safeboxtransfer.DropIntoBag(9, 33, 0, True)
		safeboxtransfer.DropIntoBag(9, 33, 5, True)
		self.assertEqual(STATE['sent'], ['/safebox_take 9 33 0', '/safebox_take 9 33 5'])

	def test_the_belt_keeps_the_packet_and_never_pours(self):
		STATE['box'] = {9: 40}
		safeboxtransfer.DropIntoBag(9, 290, 5, False)
		safeboxtransfer.DropIntoBag(9, 290, 5, True)
		self.assertEqual(STATE['sent'], [('checkout', 9, 290)])


class AnswerTest(unittest.TestCase):
	def setUp(self):
		reset_state()

	def test_a_transfer_that_worked_says_nothing(self):
		safeboxtransfer.OnTransferResult('1', '0', '30')
		self.assertEqual(STATE['chat'], [])

	def test_every_refusal_says_why(self):
		expected = {
			'1': safeboxtransfer.MSG_TRANSFER_BUSY,
			'2': safeboxtransfer.MSG_NO_SAFEBOX,
			'3': safeboxtransfer.MSG_TRANSFER_NO_ITEM,
			'4': safeboxtransfer.MSG_TRANSFER_OCCUPIED,
			'5': safeboxtransfer.MSG_TRANSFER_FULL,
			'6': safeboxtransfer.MSG_TRANSFER_REFUSED,
			'7': safeboxtransfer.MSG_TRANSFER_BAD_REQUEST,
			'8': safeboxtransfer.MSG_TRANSFER_COOLDOWN,
			'9': safeboxtransfer.MSG_UNSUPPORTED,
			'10': safeboxtransfer.MSG_TRANSFER_DEAD,
			'garbage': safeboxtransfer.MSG_UNSUPPORTED,
		}
		for code, text in sorted(expected.items()):
			safeboxtransfer.OnTransferResult('3', code, '0')
			self.assertEqual(STATE['chat'][-1], text, code)

	def test_texts_are_single_byte_for_the_client(self):
		for name in dir(safeboxtransfer):
			if name.startswith('MSG_'):
				text = getattr(safeboxtransfer, name)
				self.assertTrue(all(ord(c) < 256 for c in text), name)


class PickDialogTest(unittest.TestCase):
	def setUp(self):
		reset_state()

	def tearDown(self):
		app = sys.modules['app']
		if hasattr(app, 'ENABLE_CHEQUE_SYSTEM'):
			delattr(app, 'ENABLE_CHEQUE_SYSTEM')

	def test_the_bag_dialog_of_this_build(self):
		event = lambda count: None
		dialog = safeboxtransfer.MakePickDialog(event)
		self.assertEqual(dialog.kind, 'money')
		self.assertEqual(dialog.calls, ['load', 'accept', 'hide'])
		self.assertTrue(dialog.event is event)
		sys.modules['app'].ENABLE_CHEQUE_SYSTEM = 1
		self.assertEqual(safeboxtransfer.MakePickDialog(event).kind, 'etc')


if __name__ == '__main__':
	unittest.main()
