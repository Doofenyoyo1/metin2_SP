# The bonus switcher without a client: its decisions against stub modules.
#
#     python tests/uibonusswitch_test.py
#
# Runs on Python 3 and on the client's Python 2.7
# (docker run --rm -v "$PWD":/w -w /w python:2.7 python tests/uibonusswitch_test.py).
import os
import shutil
import sys
import tempfile
import types
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
CLIENT_ROOT = os.path.normpath(os.path.join(HERE, '..', 'linux-port-mt2009', 'client-root'))

STATE = {}

WEAPON, ARMOR, USE = 1, 2, 3
CHANGE, GREEN, ADD = 71084, 71151, 71085
SWORD30, SWORD75, RING = 290, 5160, 14000


def reset_state():
	STATE.clear()
	STATE.update({
		'now': 100.0, 'chat': [], 'sent': [], 'bag': {}, 'attrs': {},
		'selected': 0, 'equipment': set(),
	})


def module(name, **attrs):
	mod = types.ModuleType(name)
	for key, value in attrs.items():
		setattr(mod, key, value)
	return mod


class StubWidget(object):
	def __init__(self, *args, **kwargs):
		self.text = ''
		self.shown = False

	def SetText(self, text):
		self.text = text

	def GetText(self):
		return self.text

	def Show(self):
		self.shown = True

	def Hide(self):
		self.shown = False

	def IsShow(self):
		return self.shown

	def __getattr__(self, name):
		if name.startswith('__'):
			raise AttributeError(name)
		return lambda *args, **kwargs: None


# (type, subtype, level limit, use type)
PROTOS = {
	SWORD30: (WEAPON, 0, 30, ''),
	SWORD75: (WEAPON, 0, 75, ''),
	RING: (ARMOR, 5, 0, ''),
	CHANGE: (USE, 0, 0, 'USE_CHANGE_ATTRIBUTE'),
	GREEN: (USE, 0, 0, 'USE_CHANGE_ATTRIBUTE'),
	ADD: (USE, 0, 0, 'USE_ADD_ATTRIBUTE'),
}


def install_stubs():
	def get_limit(i):
		proto = PROTOS.get(STATE['selected'])
		if i == 0 and proto and proto[2]:
			return (1, proto[2])
		return (0, 0)

	def use_type(vnum):
		return PROTOS.get(vnum, (0, 0, 0, ''))[3]

	def select(vnum):
		STATE['selected'] = vnum

	sys.modules['item'] = module(
		'item', ITEM_TYPE_WEAPON=WEAPON, ITEM_TYPE_ARMOR=ARMOR, ARMOR_BODY=0, LIMIT_LEVEL=1,
		LIMIT_MAX_NUM=2, SelectItem=select, GetUseType=use_type, GetLimit=get_limit,
		GetItemType=lambda: PROTOS.get(STATE['selected'], (0,))[0],
		GetItemSubType=lambda: PROTOS.get(STATE['selected'], (0, 0))[1],
		GetItemName=lambda: 'Item %d' % STATE['selected'])

	def item_index(cell):
		return STATE['bag'].get(cell, (0, 0))[0]

	def item_count(cell):
		return STATE['bag'].get(cell, (0, 0))[1]

	def attribute(cell, i):
		return STATE['attrs'].get(cell, [(0, 0)] * 7)[i]

	sys.modules['player'] = module(
		'player', INVENTORY_PAGE_SIZE=45, INVENTORY_PAGE_COUNT=4, SLOT_TYPE_INVENTORY=1,
		GetItemIndex=item_index, GetItemCount=item_count, GetItemAttribute=attribute,
		IsEquipmentSlot=lambda cell: cell in STATE['equipment'])

	sys.modules['net'] = module('net', SendItemUseToItemPacket=lambda src, dst: STATE['sent'].append((src, dst)))
	sys.modules['chat'] = module('chat', CHAT_TYPE_INFO=1, AppendChat=lambda kind, text: STATE['chat'].append(text))
	sys.modules['clientclock'] = module('clientclock', Now=lambda: STATE['now'])

	class Controller(object):
		def isAttached(self):
			return 'attached' in STATE

		def GetAttachedType(self):
			return STATE['attached'][0]

		def GetAttachedSlotNumber(self):
			return STATE['attached'][1]

		def DeattachObject(self):
			STATE.pop('attached', None)

	sys.modules['mouseModule'] = module('mouseModule', mouseController=Controller())
	sys.modules['ui'] = module(
		'ui', BoardWithTitleBar=StubWidget, ThinBoard=StubWidget, TextLine=StubWidget,
		Button=StubWidget, SlotWindow=StubWidget, SlotBar=StubWidget, EditLine=StubWidget,
		__mem_func__=lambda f: f)


install_stubs()
sys.path.insert(0, CLIENT_ROOT)
import uibonusswitch as bs


def put(cell, vnum, count=1, attrs=None):
	STATE['bag'][cell] = (vnum, count)
	if attrs is not None:
		STATE['attrs'][cell] = list(attrs) + [(0, 0)] * (7 - len(attrs))


def consume(cell):
	vnum, count = STATE['bag'][cell]
	if count <= 1:
		del STATE['bag'][cell]
	else:
		STATE['bag'][cell] = (vnum, count - 1)


class Base(unittest.TestCase):
	def setUp(self):
		reset_state()
		self.dir = tempfile.mkdtemp()
		self.cwd = os.getcwd()
		os.chdir(self.dir)
		self.sw = bs.Switcher()

	def tearDown(self):
		os.chdir(self.cwd)
		shutil.rmtree(self.dir, ignore_errors=True)

	def tick(self, seconds=0.05):
		STATE['now'] += seconds
		if self.sw.CanUpdate():
			self.sw.OnUpdate()


class MatchTest(Base):
	def test_every_wanted_line_at_its_minimum(self):
		attrs = [(122, 30), (6, 1500), (40, 10), (0, 0), (0, 0)]
		self.assertTrue(bs.AttrsMatch(attrs, [(122, 25), (6, 0)]))
		self.assertFalse(bs.AttrsMatch(attrs, [(122, 31)]))
		self.assertFalse(bs.AttrsMatch(attrs, [(122, 25), (121, 0)]))

	def test_nothing_wanted_is_no_match(self):
		self.assertFalse(bs.AttrsMatch([(6, 100)] * 5, [(0, 0)] * 5))

	def test_negative_line_never_matches(self):
		self.assertFalse(bs.AttrsMatch([(122, -10)], [(122, 0)]))


class StoneTest(Base):
	def test_green_only_where_it_fits(self):
		put(0, SWORD75, attrs=[(6, 100)])
		put(5, GREEN, 3)
		self.assertEqual(bs.FindStone(0, SWORD75), (-1, 0))
		put(7, CHANGE, 2)
		self.assertEqual(bs.FindStone(0, SWORD75), (7, 2))

	def test_green_first_on_low_gear(self):
		put(0, SWORD30, attrs=[(6, 100)])
		put(5, CHANGE, 2)
		put(9, GREEN, 3)
		self.assertEqual(bs.FindStone(0, SWORD30), (9, 5))

	def test_green_never_on_jewellery(self):
		put(0, RING, attrs=[(6, 100)])
		put(5, GREEN, 3)
		self.assertEqual(bs.FindStone(0, RING)[0], -1)

	def test_add_stone_is_not_a_change_stone(self):
		put(0, SWORD75, attrs=[(6, 100)])
		put(5, ADD, 3)
		self.assertEqual(bs.FindStone(0, SWORD75)[0], -1)


class LoopTest(Base):
	def ready(self, attrs=((6, 100), (40, 5))):
		put(10, SWORD75, attrs=list(attrs))
		put(20, CHANGE, 5)
		self.assertTrue(self.sw.SetItem(10))
		self.sw.wanted = [(122, 20), (0, 0), (0, 0), (0, 0), (0, 0)]

	def answer(self, attrs):
		src, dst = STATE['sent'][-1]
		consume(src)
		STATE['attrs'][dst] = list(attrs) + [(0, 0)] * (7 - len(attrs))

	def test_changes_one_at_a_time_until_the_bonus_comes(self):
		self.ready()
		self.sw.Start()
		self.assertTrue(self.sw.running)
		self.tick()
		self.assertEqual(STATE['sent'], [(20, 10)])
		self.tick()
		self.assertEqual(len(STATE['sent']), 1)  # waits for the answer
		self.answer([(6, 200), (17, 5)])
		self.tick(0.5)
		self.tick(0.5)
		self.assertEqual(len(STATE['sent']), 2)
		self.answer([(122, 25), (17, 5)])
		self.tick(0.5)
		self.tick(0.5)
		self.assertFalse(self.sw.running)
		self.assertEqual(len(STATE['sent']), 2)
		self.assertIn('gotowe', STATE['chat'][-1])

	def test_keeps_the_pace(self):
		self.ready()
		self.sw.delayMs = 1000
		self.sw.Start()
		self.tick()
		self.answer([(6, 200)])
		self.tick(0.2)
		self.tick(0.2)
		self.assertEqual(len(STATE['sent']), 1)
		self.tick(0.7)
		self.assertEqual(len(STATE['sent']), 2)

	def test_same_roll_counts_by_the_stone(self):
		self.ready(attrs=[(6, 100)])
		self.sw.Start()
		self.tick()
		src, dst = STATE['sent'][-1]
		consume(src)  # the server rolled the same line again
		self.tick(0.5)
		self.tick(0.5)
		self.assertEqual(len(STATE['sent']), 2)

	def test_stops_when_the_stones_run_out(self):
		self.ready()
		STATE['bag'][20] = (CHANGE, 1)
		self.sw.Start()
		self.tick()
		self.answer([(6, 300)])
		self.tick(0.5)
		self.tick(0.5)
		self.assertFalse(self.sw.running)
		self.assertIn('kamienie', STATE['chat'][-1])

	def test_stops_when_the_item_moves(self):
		self.ready()
		self.sw.Start()
		self.tick()
		del STATE['bag'][10]
		self.tick()
		self.assertFalse(self.sw.running)

	def test_gives_up_on_a_silent_server(self):
		self.ready()
		self.sw.Start()
		for _ in range(20):
			self.tick(1.0)
		self.assertFalse(self.sw.running)
		self.assertEqual(len(STATE['sent']), bs.MAX_SILENT_TRIES)

	def test_refuses_what_it_cannot_switch(self):
		put(10, SWORD75, attrs=[])
		put(20, CHANGE, 5)
		self.assertTrue(self.sw.SetItem(10))
		self.sw.wanted = [(122, 20)] + [(0, 0)] * 4
		self.sw.Start()
		self.assertFalse(self.sw.running)  # no bonus to change
		put(11, SWORD75, attrs=[(122, 30)])
		self.sw.SetItem(11)
		self.sw.Start()
		self.assertFalse(self.sw.running)  # already there
		STATE['equipment'].add(12)
		put(12, SWORD75, attrs=[(6, 100)])
		self.assertFalse(self.sw.SetItem(12))
		put(13, CHANGE, 5)
		self.assertFalse(self.sw.SetItem(13))

	def test_settings_survive(self):
		self.sw.wanted = [(122, 25), (6, 1000), (0, 0), (0, 0), (0, 0)]
		self.sw.delayMs = 400
		self.sw.SaveConfig()
		again = bs.Switcher()
		self.assertEqual(again.wanted[:2], [(122, 25), (6, 1000)])
		self.assertEqual(again.delayMs, 400)


class WindowTest(Base):
	def test_builds_takes_an_item_and_cycles_a_bonus(self):
		put(10, SWORD75, attrs=[(6, 100)])
		win = bs.BonusSwitchWindow(self.sw)
		win.Refresh()
		STATE['attached'] = (1, 10)
		win.OnItemSlot(0)
		self.assertEqual(self.sw.cell, 10)
		win.OnNextBonus(0)
		self.assertEqual(self.sw.wanted[0][0], bs.BONUS_TYPES[1])
		win.OnPrevBonus(0)
		win.OnPrevBonus(0)
		self.assertEqual(self.sw.wanted[0][0], bs.BONUS_TYPES[-1])


if __name__ == '__main__':
	unittest.main()
