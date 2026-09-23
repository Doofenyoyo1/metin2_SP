# "Scal i uporzadkuj" on the client (client-root/inventoryarrange.py) against
# stub client modules: one command per click, nothing while a request is out,
# nothing while an item hangs on the cursor or a shop is being built, and the
# chat line only once the server has answered.
#
#     python tests/inventoryarrange_test.py
#
# Runs on Python 3 and on the client's Python 2.7
# (docker run --rm -v "$PWD":/w -w /w python:2.7 python tests/inventoryarrange_test.py).
import os
import sys
import types
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
CLIENT_ROOT = os.path.normpath(os.path.join(HERE, '..', 'linux-port-mt2009', 'client-root'))

STATE = {}


def reset_state():
	STATE.clear()
	STATE.update({'now': 100.0, 'chat': [], 'commands': [], 'attached': False, 'building': False})


def module(name, **attrs):
	mod = types.ModuleType(name)
	for key, value in attrs.items():
		setattr(mod, key, value)
	return mod


class StubMouse(object):
	def isAttached(self):
		return STATE['attached']


def install_stubs():
	sys.modules['app'] = module('app', GetTime=lambda: STATE['now'])
	sys.modules['chat'] = module('chat', CHAT_TYPE_INFO=1, AppendChat=lambda kind, text: STATE['chat'].append(text))
	sys.modules['net'] = module('net', SendChatPacket=lambda text: STATE['commands'].append(text))
	sys.modules['mouseModule'] = module('mouseModule', mouseController=StubMouse())
	sys.modules['uiPrivateShopBuilder'] = module('uiPrivateShopBuilder', IsBuildingPrivateShop=lambda: STATE['building'])


reset_state()
install_stubs()
sys.path.insert(0, CLIENT_ROOT)
import inventoryarrange  # noqa: E402


class InventoryArrangeTest(unittest.TestCase):
	def setUp(self):
		reset_state()
		inventoryarrange._state['pendingUntil'] = 0.0

	def test_one_click_is_one_command(self):
		self.assertTrue(inventoryarrange.Request())
		self.assertEqual(STATE['commands'], ['/inventory_arrange'])
		self.assertEqual(STATE['chat'], [], 'nothing is said before the server answers')

	def test_a_second_click_waits_for_the_answer(self):
		inventoryarrange.Request()
		STATE['now'] += 1.0
		self.assertFalse(inventoryarrange.Request())
		self.assertEqual(len(STATE['commands']), 1)
		inventoryarrange.OnResult('1', '0', '0', '0')
		self.assertTrue(inventoryarrange.Request())
		self.assertEqual(len(STATE['commands']), 2)

	def test_no_answer_frees_the_button(self):
		inventoryarrange.Request()
		STATE['now'] += inventoryarrange.PENDING_TIMEOUT + 0.1
		self.assertTrue(inventoryarrange.Request())
		self.assertEqual(len(STATE['commands']), 2)

	def test_an_item_on_the_cursor_is_put_down_first(self):
		STATE['attached'] = True
		self.assertFalse(inventoryarrange.Request())
		self.assertEqual(STATE['commands'], [])
		self.assertEqual(STATE['chat'], [inventoryarrange.MSG_ATTACHED])

	def test_not_while_a_shop_is_being_built(self):
		STATE['building'] = True
		self.assertFalse(inventoryarrange.Request())
		self.assertEqual(STATE['commands'], [])
		self.assertEqual(STATE['chat'], [inventoryarrange.MSG_SHOP])

	def test_the_answer_is_what_the_chat_says(self):
		inventoryarrange.OnResult('0', '12', '3', '150')
		self.assertEqual(STATE['chat'][-1], inventoryarrange.MSG_DONE % (12, 3))
		expected = {
			'1': inventoryarrange.MSG_NOTHING,
			'2': inventoryarrange.MSG_BUSY,
			'3': inventoryarrange.MSG_COOLDOWN,
			'4': inventoryarrange.MSG_NO_LAYOUT,
			'5': inventoryarrange.MSG_DEAD,
			'6': inventoryarrange.MSG_INCONSISTENT,
			'7': inventoryarrange.MSG_UNSUPPORTED,
			'8': inventoryarrange.MSG_UNSUPPORTED,
			'garbage': inventoryarrange.MSG_UNSUPPORTED,
		}
		for code, text in sorted(expected.items()):
			inventoryarrange.OnResult(code)
			self.assertEqual(STATE['chat'][-1], text, code)

	def test_texts_are_single_byte_for_the_client(self):
		# CP1250 in escapes: every character fits in one byte, so the client's
		# Python 2.7 hands the chat the bytes it draws Polish letters from.
		for name in dir(inventoryarrange):
			if name.startswith('MSG_'):
				text = getattr(inventoryarrange, name)
				self.assertTrue(all(ord(c) < 256 for c in text), name)


if __name__ == '__main__':
	unittest.main()
