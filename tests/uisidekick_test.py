# The companion's window without a client: what it reads from the server and
# what it sends back, against stub client modules.
#
#     python tests/uisidekick_test.py
#
# Runs on Python 3 and on the client's Python 2.7
# (docker run --rm -v "$PWD":/w -w /w python:2.7 python tests/uisidekick_test.py).
#
# Since client 2.0.51 (upstream's 2.0.50-2.0.52) the window is the player's own
# character window (UIScript/CharacterWindow.py) with the companion in it, four
# pages under the script's tab strip. The stubs below build it from any widget
# the script names, so what is tested is the protocol, the order queue shared
# with the bag window, and what the window sends for a page's buttons - not its
# pixels.
import os
import sys
import types
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
CLIENT_ROOT = os.path.normpath(os.path.join(HERE, '..', 'linux-port-mt2009', 'client-root'))

STATE = {}


def reset_state():
	STATE.clear()
	STATE.update({'now': 100.0, 'commands': [], 'questions': []})


def module(name, **attrs):
	mod = types.ModuleType(name)
	for key, value in attrs.items():
		setattr(mod, key, value)
	return mod


class LooseModule(types.ModuleType):
	"""A client module whose constants the window only passes on: any name
	it does not set is 0."""

	def __getattr__(self, name):
		if name.startswith('__'):
			raise AttributeError(name)
		return 0


class WidgetModule(types.ModuleType):
	"""The ui module: any widget class it does not set is Anything."""

	def __getattr__(self, name):
		if name.startswith('__'):
			raise AttributeError(name)
		return Anything


def loose_module(name, **attrs):
	mod = LooseModule(name)
	for key, value in attrs.items():
		setattr(mod, key, value)
	return mod


class Anything(object):
	"""Any client widget: what the window asks of it is kept, the rest is
	accepted and does nothing. A script window hands out one of these for
	every child the script would have made."""

	def __init__(self, *args, **kwargs):
		self.text = ''
		self.shown = False
		self.pressed = False
		self.event = None
		self.eventArgs = ()
		self.children = {}
		self.Children = []

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

	def Down(self):
		self.pressed = True

	def SetUp(self):
		self.pressed = False

	def SAFE_SetEvent(self, event, *args):
		self.event = event
		self.eventArgs = args

	def GetChild(self, name):
		if name not in self.children:
			self.children[name] = Anything()
		return self.children[name]

	def GetWidth(self):
		return 250

	def GetHeight(self):
		return 360

	def GetTextSize(self):
		return (len(self.text) * 6, 12)

	def GetLocalPosition(self):
		return (0, 0)

	def GetGlobalPosition(self):
		return (0, 0)

	def __getattr__(self, name):
		if name.startswith('__'):
			raise AttributeError(name)
		return lambda *args, **kwargs: None


class StubQuestion(Anything):
	def __init__(self, *args, **kwargs):
		Anything.__init__(self)
		self.accept = None
		self.cancel = None
		self.opened = False
		STATE['questions'].append(self)

	def SetAcceptEvent(self, event):
		self.accept = event

	def SetCancelEvent(self, event):
		self.cancel = event

	def Open(self):
		self.opened = True

	def Close(self):
		self.opened = False


class ScriptLoader(object):
	def LoadScriptFile(self, window, name):
		STATE['script'] = name


def install_stubs():
	sys.modules['app'] = module('app', GetTime=lambda: STATE['now'])
	sys.modules['net'] = module('net', SendChatPacket=lambda text: STATE['commands'].append(text))
	sys.modules['systemSetting'] = module('systemSetting', GetLanguage=lambda: 'pl')
	sys.modules['chat'] = module('chat', AppendChat=lambda *args: None, CHAT_TYPE_INFO=1)
	sys.modules['wndMgr'] = loose_module('wndMgr', GetScreenWidth=lambda: 1024, GetScreenHeight=lambda: 768,
		GetMousePosition=lambda: (0, 0))
	ui = WidgetModule('ui')
	for key, value in dict(ScriptWindow=Anything, Window=Anything, TextLine=Anything, Button=Anything,
		ToggleButton=Anything, RadioButton=Anything, ImageBox=Anything, ExpandedImageBox=Anything,
		Bar=Anything, Line=Anything, Box=Anything, ThinBoard=Anything, BoardWithTitleBar=Anything,
		SlotWindow=Anything, GridSlotWindow=Anything, Gauge=Anything, AniImageBox=Anything,
		PythonScriptLoader=ScriptLoader).items():
		setattr(ui, key, value)
	setattr(ui, '__mem_func__', lambda func: func)
	sys.modules['ui'] = ui
	sys.modules['uiCommon'] = module('uiCommon', QuestionDialog=StubQuestion, InputDialog=StubQuestion,
		MoneyInputDialog=StubQuestion)
	sys.modules['uiToolTip'] = module('uiToolTip', ToolTip=Anything, SkillToolTip=Anything,
		ItemToolTip=Anything)
	# The bag window is tested on its own (uisidekickinventory_test.py); here
	# the real module is loaded on loose stubs, because the window's skill and
	# character pages read its models and send through its orders.
	for name in ('item', 'player', 'skill', 'snd', 'mouseModule'):
		sys.modules[name] = loose_module(name)
	sys.modules['mouseModule'].mouseController = Anything()


reset_state()
install_stubs()
sys.path.insert(0, CLIENT_ROOT)
import clientclock  # noqa: E402
import uisidekick  # noqa: E402
import uisidekickinventory as inv  # noqa: E402


def hexed(text):
	return ''.join('%02x' % ord(c) for c in text)


def info_line(**over):
	values = dict(race=5, group=2, level=42, exp=37, hp=900, maxhp=1800, sp=100, maxsp=400, where=1, dist=350,
		mode=0, stance=1, loot=2, protect=1, buffs=0, gold=1234567, red=120, blue=40, dead=0)
	values.update(over)
	order = ('race', 'group', 'level', 'exp', 'hp', 'maxhp', 'sp', 'maxsp', 'where', 'dist',
		'mode', 'stance', 'loot', 'protect', 'buffs', 'gold', 'red', 'blue', 'dead')
	return ['1', '1'] + [str(values[k]) for k in order]


def tick(seconds):
	STATE['now'] += seconds


class ProtocolTest(unittest.TestCase):
	def test_the_three_answers_of_the_first_word(self):
		self.assertEqual(uisidekick.ParseInfo(['1', '0']), {'has': False})
		self.assertEqual(uisidekick.ParseInfo(['1', '2']), {'has': False, 'off': True})
		info = uisidekick.ParseInfo(info_line())
		self.assertTrue(info['has'])
		self.assertEqual((info['race'], info['level'], info['stance'], info['loot']), (5, 42, 1, 2))
		self.assertEqual(info['gold'], 1234567)
		self.assertNotIn('lure', info)

	def test_the_words_a_newer_server_adds(self):
		# MT2009 PLUS (server 2.12.0) puts the party leader, its role and the
		# companion's Leadership before "Grupa".
		info = uisidekick.ParseInfo(info_line() + ['1', '0', '1', '0', '1', '2', '17', '1'])
		self.assertEqual((info['lure'], info['luring'], info['solo'], info['chests'], info['party']),
			(1, 0, 1, 0, 1))
		self.assertEqual((info['lead'], info['role'], info['leadership']), (1, 2, 17))
		info = uisidekick.ParseInfo(info_line() + ['0', '1'])
		self.assertEqual((info['lure'], info['luring']), (0, 1))
		self.assertNotIn('solo', info)

	def test_another_protocol_or_a_short_line_is_ignored(self):
		self.assertIsNone(uisidekick.ParseInfo(['2', '1'] + info_line()[2:]))
		self.assertIsNone(uisidekick.ParseInfo(['1', '1', '5', '2']))
		self.assertIsNone(uisidekick.ParseInfo([]))

	def test_texts_come_as_hex_and_nothing_else_gets_through(self):
		self.assertEqual(uisidekick.DecodeText(hexed('Dolina Ork\xf3w')), 'Dolina Ork\xf3w')
		self.assertEqual(uisidekick.DecodeText('-'), '')
		self.assertEqual(uisidekick.DecodeText('4a6f61'), 'Joa')
		self.assertEqual(uisidekick.DecodeText('4a6f6'), '')  # odd length
		self.assertEqual(uisidekick.DecodeText('zz'), '')
		self.assertEqual(uisidekick.DecodeText('0a41'), '?A')  # a control character is shown as '?'
		self.assertEqual(uisidekick.DecodeText('41' * 65), '')  # longer than the server sends

	def test_numbers_as_the_client_writes_them(self):
		self.assertEqual(uisidekick.FormatGold(0), '0')
		self.assertEqual(uisidekick.FormatGold(999), '999')
		self.assertEqual(uisidekick.FormatGold(1234567), '1.234.567')
		self.assertEqual(uisidekick.ClassText(5, 2), 'Ninja (\xa3ucznik)')
		self.assertEqual(uisidekick.ClassText(3, 0), 'Szaman')
		self.assertEqual(uisidekick.PathText(5, 2), '\xa3ucznik')
		self.assertEqual(uisidekick.PathText(3, 0), 'Szaman')
		self.assertEqual(uisidekick.PlaceText({'where': 1, 'dist': 350}, 'Joan'), 'Joan, obok ciebie')
		self.assertEqual(uisidekick.PlaceText({'where': 1, 'dist': 4200}, 'Joan'), 'Joan, 42 m od ciebie')
		self.assertEqual(uisidekick.PlaceText({'where': 2}, 'Dolina Ork\xf3w'), 'Dolina Ork\xf3w (inna mapa)')
		self.assertEqual(uisidekick.PlaceText({'where': 0}, ''), 'poza gr\xb9')


class QueueTest(unittest.TestCase):
	# The server drops a sixth command in half a second without a word, so
	# the window and the bag window send through one queue.
	def setUp(self):
		reset_state()
		clientclock.Reset()
		uisidekick.ResetCommands()

	def test_orders_wait_their_turn_and_keep_their_place(self):
		uisidekick.SendCommand('przywolaj')
		uisidekick.SendCommand('wolny')
		uisidekick.SendCommand('czekaj')
		self.assertEqual(STATE['commands'], ['/towarzysz przywolaj'])
		self.assertTrue(uisidekick.HasPendingCommands())
		tick(0.1)
		self.assertFalse(uisidekick.PumpCommands())
		tick(uisidekick.COMMAND_SPACING)
		self.assertTrue(uisidekick.PumpCommands())
		self.assertEqual(STATE['commands'][-1], '/towarzysz wolny')
		tick(uisidekick.COMMAND_SPACING)
		self.assertTrue(uisidekick.PumpCommands())
		self.assertEqual(STATE['commands'][-1], '/towarzysz czekaj')
		self.assertFalse(uisidekick.HasPendingCommands())

	def test_a_poll_never_goes_before_an_order(self):
		uisidekick.SendCommand('stan')
		uisidekick.SendCommand('walka 0')
		tick(1.0)
		self.assertFalse(uisidekick.TryPoll('okno'))
		self.assertTrue(uisidekick.PumpCommands())
		tick(1.0)
		self.assertTrue(uisidekick.TryPoll('okno'))
		self.assertEqual(STATE['commands'], ['/towarzysz stan', '/towarzysz walka 0', '/towarzysz okno'])

	def test_the_keeper_sends_what_a_closed_window_left(self):
		keeper = uisidekick.GetKeeper()
		self.assertFalse(keeper.CanUpdate())
		uisidekick.SendCommand('przywolaj')
		uisidekick.SendCommand('odprawa tak')
		self.assertTrue(keeper.CanUpdate())
		tick(uisidekick.COMMAND_SPACING)
		keeper.OnUpdate()
		self.assertEqual(STATE['commands'][-1], '/towarzysz odprawa tak')
		self.assertFalse(keeper.CanUpdate())


class WindowTest(unittest.TestCase):
	def setUp(self):
		reset_state()
		clientclock.Reset()
		uisidekick.Destroy()
		self.window = uisidekick.GetWindow()

	def test_it_is_the_players_character_window(self):
		self.assertEqual(STATE['script'], 'UIScript/CharacterWindow.py')
		self.assertEqual(self.window.page, uisidekick.PAGE_STATUS)

	def test_opening_asks_for_the_whole_gear_and_the_skills_then_polls(self):
		uisidekick.ToggleWindow()
		self.assertTrue(self.window.IsShow())
		self.assertEqual(STATE['commands'], ['/towarzysz okno 1'])
		tick(uisidekick.COMMAND_SPACING)
		self.window.OnUpdate()
		self.assertEqual(STATE['commands'][-1], '/towarzysz umiejetnosci')
		tick(0.5)
		self.window.OnUpdate()
		self.assertEqual(len(STATE['commands']), 2)
		tick(uisidekick.POLL_INTERVAL)
		self.window.OnUpdate()
		self.assertEqual(STATE['commands'][-1], '/towarzysz okno')
		uisidekick.ToggleWindow()
		self.assertFalse(self.window.IsShow())

	def test_the_snapshot_is_kept_while_the_window_is_shut(self):
		self.assertFalse(self.window.IsShow())
		uisidekick.OnServerInfo(*info_line())
		uisidekick.OnServerNames(hexed('Wojtek'), hexed('Joan'), hexed('walczy'))
		uisidekick.OnServerGear('0', hexed('Miecz+7'))
		self.assertTrue(self.window.HasCompanion())
		self.assertEqual(self.window.info['level'], 42)
		self.assertEqual(self.window.names, ('Wojtek', 'Joan', 'walczy'))
		self.assertEqual(self.window.gear[0], 'Miecz+7')
		self.assertEqual(self.window.gear[1], '')

	def test_no_companion_and_a_world_without_them(self):
		uisidekick.OnServerInfo('1', '0')
		self.assertFalse(self.window.HasCompanion())
		uisidekick.OnServerInfo('1', '2')
		self.assertFalse(self.window.HasCompanion())
		self.assertTrue(self.window.info.get('off'))
		# A line of another protocol changes nothing.
		uisidekick.OnServerInfo(*info_line())
		uisidekick.OnServerInfo('9', '1')
		self.assertTrue(self.window.HasCompanion())

	def test_the_pages(self):
		for page in uisidekick.PAGES:
			self.window.SetPage(page)
			self.assertEqual(self.window.page, page)
			self.assertTrue(self.window.tabButtons[page].pressed)
		self.window.SetPage('NOWHERE')
		self.assertEqual(self.window.page, uisidekick.PAGES[-1])

	def test_orders_are_the_letters_commands(self):
		uisidekick.OnServerInfo(*info_line())
		self.window.OnStance(2)
		self.assertEqual(STATE['commands'], ['/towarzysz walka 2'])
		tick(uisidekick.COMMAND_SPACING)
		self.window.OnLoot(1)
		self.assertEqual(STATE['commands'][-1], '/towarzysz zbieraj 1')
		tick(uisidekick.COMMAND_SPACING)
		self.window.OnSwitch('protect')  # on in the snapshot
		self.assertEqual(STATE['commands'][-1], '/towarzysz ochrona 0')
		tick(uisidekick.COMMAND_SPACING)
		self.window.OnSwitch('buffs')  # off in the snapshot
		self.assertEqual(STATE['commands'][-1], '/towarzysz buffy 1')
		tick(uisidekick.COMMAND_SPACING)
		self.window.OnOrder('czekaj')
		self.assertEqual(STATE['commands'][-1], '/towarzysz czekaj')
		# An order asks for the answer at the next look.
		self.assertEqual(self.window.nextPoll, 0.0)

	def test_every_order_button_is_a_command_the_server_knows(self):
		self.assertEqual([order for text, order in uisidekick.ORDERS],
			['przywolaj', 'czekaj', 'wolny', 'zakupy', 'ryby', 'stan'])
		self.assertEqual([order for name, order, default, text, hint in uisidekick.SWITCHES],
			['ochrona', 'buffy', 'luruj', 'sam', 'skrzynki', 'grupa', 'monety'])

	def test_dismissing_asks_first(self):
		self.window.OnDismiss()
		self.assertEqual(STATE['commands'], [])
		question = STATE['questions'][-1]
		self.assertTrue(question.opened)
		question.cancel()
		self.assertFalse(question.opened)
		self.assertEqual(STATE['commands'], [])
		self.window.OnDismiss()
		STATE['questions'][-1].accept()
		self.assertEqual(STATE['commands'], ['/towarzysz odprawa tak'])


class SkillPageTest(unittest.TestCase):
	# Since client 2.0.51 the companion's skills are a page of its window, the
	# player's own skill page; the list is the bag module's model.
	def setUp(self):
		reset_state()
		clientclock.Reset()
		uisidekick.Destroy()
		uisidekick.ResetCommands()
		inv.Destroy()
		self.window = uisidekick.GetWindow()
		uisidekick.OnServerInfo(*info_line())

	def listed(self, points=3, manual=0, skills=((106, 17, 0), (107, 25, 1), (108, 40, 3), (109, 5, 0))):
		inv.OnSkillBegin('1', str(points), '0', '1', str(manual))
		for vnum, level, grade in skills:
			inv.OnSkill(str(vnum), str(level), str(grade))
		inv.OnSkillEnd()

	def test_the_list_is_the_bag_modules_model(self):
		self.listed()
		model = inv.GetSkillModel()
		self.assertEqual(model.state, inv.STATE_OK)
		self.assertEqual((model.points, model.job, model.group, model.manual), (3, 0, 1, 0))
		self.assertEqual(self.window.skillRows, [(106, 17, 0), (107, 25, 1), (108, 40, 3), (109, 5, 0)])
		self.assertEqual(inv.SkillLevelText(25, 1), 'M6')
		self.assertEqual(inv.SkillLevelText(40, 3), 'P')
		# "+" where a point can go: under seventeen, normal grade, points left.
		self.assertTrue(inv.CanAddSkillPoint(3, 5, 0))
		self.assertFalse(inv.CanAddSkillPoint(3, 17, 0))
		self.assertFalse(inv.CanAddSkillPoint(0, 5, 0))

	def test_plus_and_who_spends_the_points(self):
		self.listed()
		self.window.OnSkillPlus(4)
		self.assertEqual(STATE['commands'], ['/towarzysz umiejetnosci dodaj 109'])
		# A grade column's slot is the same row.
		tick(uisidekick.COMMAND_SPACING)
		self.window.OnSkillPlus(4 + uisidekick.SKILL_GRADE_STEP)
		self.assertEqual(STATE['commands'][-1], '/towarzysz umiejetnosci dodaj 109')
		tick(uisidekick.COMMAND_SPACING)
		self.window.OnSkillManual()
		self.assertEqual(STATE['commands'][-1], '/towarzysz umiejetnosci reczne 1')
		self.listed(manual=1)
		tick(uisidekick.COMMAND_SPACING)
		self.window.OnSkillManual()
		self.assertEqual(STATE['commands'][-1], '/towarzysz umiejetnosci reczne 0')
		# No row, no order.
		tick(uisidekick.COMMAND_SPACING)
		before = len(STATE['commands'])
		self.window.OnSkillPlus(8)
		self.assertEqual(len(STATE['commands']), before)

	def test_the_answer_goes_to_the_page_that_gave_the_order(self):
		self.listed()
		uisidekick.ToggleWindow()
		self.window.SetPage(uisidekick.PAGE_SKILL)
		self.window.OnSkillPlus(4)
		inv.OnEqResult('2', hexed('Nie ma punktow.'))
		self.assertEqual(self.window.skillStatus.answer[0], 'Nie ma punktow.')
		# On another page the skill page's answer is not written anywhere else.
		self.window.SetPage(uisidekick.PAGE_STATUS)
		self.assertFalse(uisidekick.ShowResult(inv.ORIGIN_SKILL, 'x', 0))

	def test_no_companion_answers_the_page_too(self):
		self.listed()
		inv.OnEqNone('1', '1')
		self.assertNotEqual(inv.GetSkillModel().state, inv.STATE_OK)
		self.assertEqual(self.window.skillRows, [])


if __name__ == '__main__':
	unittest.main()
