# MT2009_PLUS_VEKIRION_V1 - Skroty klawiszowe (Autor: Vekirion): every hotkey
# of the game window, rebindable. From Vekirion's package (the owner, 3 October:
# "keybind menu ... help text i panel UI jest responsywne na zmiany"), fitted
# to MT2009 PLUS: the defaults are the keys this client always had (X opens
# Wyprawy, Ctrl+G rides, Ctrl+J takes the seal off, ~ picks up the nearest
# item, Z what lies around) and the sprint is a tap of Left Shift again, as
# players know it.
#
# This module owns the bindings; uikeybind.py is the window that edits them
# (the Esc menu's "Skroty klawiszowe" button) and game.py runs the actions
# (GameWindow.__BuildKeyDict builds {action: (press, release)}). The help
# window (uihelp.py) and the inventory's side bar (uiinventory.py) show the
# keys in use and follow a change (GetVersion).
#
# A binding is (mods, key): key a DirectInput code (app.DIK_*), mods a mask of
# CTRL / SHIFT / ALT. Ctrl, Shift and Alt are modifiers: held with a key they
# make a combination (Shift+M). Every action has two slots (W and the up
# arrow both walk forward).
#
# Shift alone: a Shift key pressed and let go with no other key, Enter or
# inventory click between is a "tap" and runs the action bound to (0, Shift)
# - the sprint by default. Shift+M (minimap) or Shift+click (split a stack)
# is no tap, so the sprint stays as it was; the sprint switches on the
# release, a moment later than the old press. Only the Shift keys can be
# bound alone: Alt and Ctrl held show the names and the cursor, and the
# right Alt is AltGr on Polish keyboards.
#
# A key press is looked up with the modifiers held: the exact combination
# first, then the key alone - so W still walks with Alt held to see names,
# as before. An action whose key is held runs its release when that same key
# comes up (walking, attack, camera), whatever happened to the modifiers.
#
# Saved for the PC (the owner: "Bindings per PC"), not the character:
# autohunt/klawisze.cfg next to the client's other settings, one line per
# action changed from its default: "action=mods:key,mods:key" ("-" = empty).
# An action missing from the file keeps its default, unless that combination
# was given to another action in the meantime. A missing or broken file, or
# a broken line, gives the defaults.
#
# Mouse side buttons (Mouse 4/5) never reach the client's Python - the exe
# passes only the left, right and middle button and the wheel. A mouse's own
# software can send a key for them instead; F13-F15, which no keyboard has
# and nothing here uses, are named for that.
#
# Python 2.7 as the client has it; the texts are CP1250 escapes.

import os
import app

CTRL = 1
SHIFT = 2
ALT = 4

CONFIG_DIR = 'autohunt'
CONFIG_PATH = os.path.join(CONFIG_DIR, 'klawisze.cfg')

# Keys that are never a binding: Esc and Enter are the exe's (system menu,
# chat), Ctrl and Alt are modifiers, the Windows keys leave the game, and
# Backspace clears a slot in the window. The Shift keys are modifiers too,
# but may be bound alone (TAP_NAMES).
RESERVED_NAMES = ('DIK_ESCAPE', 'DIK_ESC', 'DIK_RETURN', 'DIK_LCONTROL', 'DIK_RCONTROL',
	'DIK_LMENU', 'DIK_RMENU', 'DIK_LALT', 'DIK_RALT',
	'DIK_LWIN', 'DIK_RWIN', 'DIK_BACK')

# Modifiers that may be bound alone, run on a tap (see above).
TAP_NAMES = ('DIK_LSHIFT', 'DIK_RSHIFT')

# Codes the exe does not name (app has no DIK_F13): what a mouse's software
# can send for a side button.
EXTRA_KEYS = {0x64: 'F13', 0x65: 'F14', 0x66: 'F15'}

NICE_NAMES = {
	'DIK_SPACE': 'Spacja', 'DIK_TAB': 'Tab', 'DIK_CAPITAL': 'Caps Lock',
	'DIK_UP': 'Strz. g\xf3ra', 'DIK_DOWN': 'Strz. d\xf3\xb3',
	'DIK_LEFT': 'Strz. lewo', 'DIK_RIGHT': 'Strz. prawo',
	'DIK_PGUP': 'Page Up', 'DIK_PGDN': 'Page Down', 'DIK_PRIOR': 'Page Up', 'DIK_NEXT': 'Page Down',
	'DIK_HOME': 'Home', 'DIK_END': 'End', 'DIK_INSERT': 'Insert', 'DIK_DELETE': 'Delete',
	'DIK_GRAVE': '~', 'DIK_MINUS': '-', 'DIK_EQUALS': '=', 'DIK_LBRACKET': '[',
	'DIK_RBRACKET': ']', 'DIK_SEMICOLON': ';', 'DIK_APOSTROPHE': "'", 'DIK_BACKSLASH': '\\',
	'DIK_COMMA': ',', 'DIK_PERIOD': '.', 'DIK_SLASH': '/',
	'DIK_SYSRQ': 'Print Screen', 'DIK_SCROLL': 'Scroll Lock', 'DIK_PAUSE': 'Pause',
	'DIK_NUMLOCK': 'Num Lock', 'DIK_ADD': 'Num +', 'DIK_SUBTRACT': 'Num -',
	'DIK_MULTIPLY': 'Num *', 'DIK_DIVIDE': 'Num /', 'DIK_DECIMAL': 'Num .',
	'DIK_NUMPADENTER': 'Num Enter', 'DIK_NUMPADCOMMA': 'Num ,', 'DIK_APPS': 'Menu',
	'DIK_LSHIFT': 'Lewy Shift', 'DIK_RSHIFT': 'Prawy Shift',
}

# (category, [(action, label, (default, default), hold)]). A default is
# (mods, 'DIK_NAME'); hold - the action has a release (game.py). Only the
# actions game.py runs are shown (Available): the cinema camera is in
# exes built with it only. No combination is a default twice.
CATEGORIES = (
	('Ruch i walka', (
		('move_up', 'Ruch do przodu', ((0, 'DIK_W'), (0, 'DIK_UP')), True),
		('move_down', 'Ruch do ty\xb3u', ((0, 'DIK_S'), (0, 'DIK_DOWN')), True),
		('move_left', 'Ruch w lewo', ((0, 'DIK_A'), (0, 'DIK_LEFT')), True),
		('move_right', 'Ruch w prawo', ((0, 'DIK_D'), (0, 'DIK_RIGHT')), True),
		('attack', 'Atak', ((0, 'DIK_SPACE'),), True),
		('sprint', 'Sprint (w\xb3\xb9cz/wy\xb3\xb9cz)', ((0, 'DIK_LSHIFT'),), False),
		('pickup', 'Podnie\x9c przedmioty wok\xf3\xb3', ((0, 'DIK_Z'),), False),
		('pickup_near', 'Podnie\x9c najbli\xbfszy przedmiot', ((0, 'DIK_GRAVE'),), False),
		('pickup_filter', 'Filtr podnoszenia', ((CTRL, 'DIK_Z'),), False),
	)),
	('Pasek szybkiego dost\xeapu', (
		('quick_1', 'Slot 1', ((0, 'DIK_1'),), False),
		('quick_2', 'Slot 2', ((0, 'DIK_2'),), False),
		('quick_3', 'Slot 3', ((0, 'DIK_3'),), False),
		('quick_4', 'Slot 4', ((0, 'DIK_4'),), False),
		('quick_5', 'Slot 5', ((0, 'DIK_5'),), False),
		('quick_6', 'Slot 6', ((0, 'DIK_TAB'),), False),
		('quick_7', 'Slot 7', ((0, 'DIK_F1'),), False),
		('quick_8', 'Slot 8', ((0, 'DIK_F2'),), False),
		('quick_9', 'Slot 9', ((0, 'DIK_F3'),), False),
		('quick_10', 'Slot 10', ((0, 'DIK_F4'),), False),
	)),
	('Emotki', tuple(
		('emote_%d' % n, 'Emotka %d' % n, ((CTRL, 'DIK_%d' % n),), False) for n in range(1, 10)
	)),
	('Okna', (
		('character', 'Posta\xe6', ((0, 'DIK_C'),), False),
		('skills', 'Umiej\xeatno\x9cci', ((0, 'DIK_V'),), False),
		('emote_window', 'Okno emotek', ((0, 'DIK_B'),), False),
		('quests', 'Zadania', ((0, 'DIK_N'),), False),
		('inventory', 'Ekwipunek', ((0, 'DIK_I'),), False),
		('dragon_soul', 'Alchemia', ((0, 'DIK_O'),), False),
		('atlas', 'Mapa \x9cwiata', ((0, 'DIK_M'),), False),
		('minimap', 'Minimapa (poka\xbf/ukryj)', ((SHIFT, 'DIK_M'),), False),
		('minimap_in', 'Minimapa - przybli\xbf', ((0, 'DIK_ADD'),), False),
		('minimap_out', 'Minimapa - oddal', ((0, 'DIK_SUBTRACT'),), False),
		('messenger', 'Komunikator', ((ALT, 'DIK_M'),), False),
		('guild', 'Gildia', ((ALT, 'DIK_G'),), False),
		('chat_log', 'Historia czatu', ((0, 'DIK_L'),), False),
		('help', 'Pomoc', ((0, 'DIK_H'),), False),
		('player_stat', 'Statystyki gracza', ((0, 'DIK_Y'),), False),
		('companion', 'Towarzysz', ((0, 'DIK_P'),), False),
		('autohunt', 'Auto\xb3owy', ((0, 'DIK_K'),), False),
		# The bonus switcher (uibonusswitch.py, Metin2 SinglePlayer): 0 by default.
		('bonus_switch', 'Prze\xb3\xb9cznik bonus\xf3w', ((0, 'DIK_0'),), False),
		# MT2009_PLUS_AUTOHUNT_QUICK_V1: start/stop with the saved settings, no window.
		('autohunt_quick', 'Auto\xb3owy - szybki start/stop', ((SHIFT, 'DIK_K'),), False),
		('garbage_bin', 'Kosz', ((0, 'DIK_J'),), False),
		('shop_search', 'Wyszukiwarka sklep\xf3w', ((0, 'DIK_F5'),), False),
		('event_calendar', 'Kalendarz event\xf3w', ((0, 'DIK_F11'),), False),
		('wheel', 'Ko\xb3o Fortuny', ((0, 'DIK_F12'),), False),
		('new_pet', 'Zwierzak', ((0, 'DIK_U'),), False),
		('dungeon_info', 'Wyprawy', ((0, 'DIK_X'),), False),
		('battle_pass', 'Battle Pass', (), False),
		('tp_bookmarks', 'Zapisane pozycje', (), False),
		# MT2009_PLUS_WEEKLY_RANKING_V1: the weekly ranking (uiweeklyrank.py), no key by default.
		('weekly_rank', 'Ranking tygodniowy', (), False),
		# MT2009_PLUS_MONSTER_CARDS_V1: Karty Potworow (monstercard.py) - J, the package's key, is
		# the Kosz here, so none by default (the Esc menu's button opens it too).
		('monster_card', 'Karty Potwor\xf3w', (), False),
		('drop_wiki', 'Drop wiki', ((0, 'DIK_SLASH'),), False),	# MT2009_PLUS_DROP_WIKI_V1
		('hide_ui', 'Ukryj interfejs', ((CTRL, 'DIK_TAB'),), False),
		('quest_buttons', 'Ukryj/poka\xbf ikony zada\xf1', ((CTRL, 'DIK_Q'),), False),
		('screenshot', 'Zrzut ekranu', ((0, 'DIK_SYSRQ'),), False),
		('gm_panel', 'Panel GM', ((0, 'DIK_F9'),), False),
		('bot_admin', 'Panel bot\xf3w (GM)', ((0, 'DIK_F10'),), False),
		('console', 'Konsola (tryb debug)', ((0, 'DIK_COMMA'),), False),
	)),
	('Ko\xf1 i wierzchowiec', (
		('ride', 'Wsi\xb9d\x9f/zsi\xb9d\x9f (ko\xf1, wierzchowiec)', ((CTRL, 'DIK_G'),), False),
		('unmount', 'Zdejmij piecz\xea\xe6 wierzchowca', ((CTRL, 'DIK_J'),), False),
		('horse_call', 'Ko\xf1 - wsi\xb9d\x9f/zsi\xb9d\x9f', ((CTRL, 'DIK_H'),), False),
		('horse_back', 'Ode\x9clij konia', ((CTRL, 'DIK_B'),), False),
		('horse_feed', 'Nakarm konia', ((CTRL, 'DIK_F'),), False),
	)),
	('Kana\xb3', (
		('channel_1', 'Zmie\xf1 na CH1', ((ALT, 'DIK_1'),), False),
		('channel_2', 'Zmie\xf1 na CH2', ((ALT, 'DIK_2'),), False),
	)),
	('Kamera', (
		('cam_rot_left', 'Obr\xf3t w lewo', ((0, 'DIK_Q'),), True),
		('cam_rot_right', 'Obr\xf3t w prawo', ((0, 'DIK_E'),), True),
		('cam_zoom_in', 'Przybli\xbf', ((0, 'DIK_R'),), True),
		('cam_zoom_out', 'Oddal', ((0, 'DIK_F'),), True),
		('cam_pitch_up', 'Pochyl w g\xf3r\xea', ((0, 'DIK_T'),), True),
		('cam_pitch_down', 'Pochyl w d\xf3\xb3', ((0, 'DIK_G'),), True),
		('movie_reset', 'Kamera filmowa - reset', ((0, 'DIK_NUMPAD9'),), False),
		('movie_rot_left', 'Kamera filmowa - obr\xf3t w lewo', ((0, 'DIK_NUMPAD4'),), True),
		('movie_rot_right', 'Kamera filmowa - obr\xf3t w prawo', ((0, 'DIK_NUMPAD6'),), True),
		('movie_zoom_in', 'Kamera filmowa - przybli\xbf', ((0, 'DIK_PGUP'),), True),
		('movie_zoom_out', 'Kamera filmowa - oddal', ((0, 'DIK_PGDN'),), True),
		('movie_pitch_up', 'Kamera filmowa - w g\xf3r\xea', ((0, 'DIK_NUMPAD8'),), True),
		('movie_pitch_down', 'Kamera filmowa - w d\xf3\xb3', ((0, 'DIK_NUMPAD2'),), True),
		# F9 is the GM panel's; the cinema camera's FOV- had lost it to that
		# long before this window, so it starts without a key.
		('cine_speed_up', 'Wolna kamera - szybciej', ((0, 'DIK_F6'),), False),
		('cine_speed_down', 'Wolna kamera - wolniej', ((0, 'DIK_F7'),), False),
		('cine_fov_up', 'Wolna kamera - FOV +', ((0, 'DIK_F8'),), False),
		('cine_fov_down', 'Wolna kamera - FOV -', (), False),
	)),
)

# Shown in the window, not rebindable: the exe's keys, the modifiers' own
# doings and the clicks of the windows (they read the modifiers themselves).
FIXED = (
	('Esc', 'Menu systemowe / zamknij okno'),
	('Enter', 'Czat (w Alchemii: Uszlachetnij)'),
	('Shift+Enter', 'Szept'),
	('Alt (przytrzymaj)', 'Nazwy graczy i przedmiot\xf3w'),
	('Ctrl (przytrzymaj)', 'Kursor nad postaciami'),
	('Ctrl+V', 'Wklej (w polu tekstowym)'),
	('Alt + LPM', 'Ekwipunek: blokada sortowania / link w czacie'),
	('Shift + LPM', 'Ekwipunek: podziel stos (pole "Paczki po")'),
	('Ctrl + LPM', 'Ekwipunek: na pasek szybkiego dost\xeapu'),
	('Ctrl + PPM', 'Ekwipunek: otw\xf3rz ca\xb3y stos skrzynek'),
	('PPM', 'Torba towarzysza: we\x9f do siebie'),
	('Ctrl + PPM', 'Sklep offline: zmie\xf1 cen\xea wszystkich'),
)

SLOT_COUNT = 2

# Shown in red in the window when they have no key at all (the owner,
# 3 October: core functions must stand out when unbound).
CORE = ('move_up', 'move_down', 'move_left', 'move_right', 'attack', 'pickup',
	'quick_1', 'quick_2', 'quick_3', 'quick_4', 'quick_5',
	'character', 'skills', 'quests', 'inventory', 'atlas', 'ride')

_state = {
	'capture': None,	# the window's handler while it waits for a key
	'escapeAt': -1.0,	# when an Esc cancelled the wait (the window then stays)
	'tap': None,		# the Shift key down with nothing else since
	'loaded': False,
	'bindings': {},		# action -> [binding or None] * SLOT_COUNT
	'lookup': {},		# (mods, key) -> action
	'available': None,	# set of actions game.py runs, None = all
	'version': 0,
}

_codeNames = {}


def _Rows():
	for category, actions in CATEGORIES:
		for row in actions:
			if row[1] is not None:
				yield category, row


def _BuildNames():
	if _codeNames:
		return
	for name in dir(app):
		if not name.startswith('DIK_'):
			continue
		code = getattr(app, name)
		if not isinstance(code, int):
			continue
		nice = NICE_NAMES.get(name)
		if nice is None:
			short = name[4:]
			if short.startswith('NUMPAD') and len(short) == 7:
				nice = 'Num ' + short[6:]
			else:
				nice = short.capitalize() if len(short) > 1 and not short[1:].isdigit() else short
		# A nice name wins over a plain one for a code with two names.
		if code not in _codeNames or name in NICE_NAMES:
			_codeNames[code] = nice
	for code, nice in EXTRA_KEYS.items():
		_codeNames.setdefault(code, nice)


def KeyName(code):
	_BuildNames()
	return _codeNames.get(code, 'Klawisz %d' % code)


def BindingText(binding):
	if not binding:
		return ''
	mods, key = binding
	parts = []
	if mods & CTRL:
		parts.append('Ctrl')
	if mods & SHIFT:
		parts.append('Shift')
	if mods & ALT:
		parts.append('Alt')
	parts.append(KeyName(key))
	return '+'.join(parts)


def _Code(name):
	return getattr(app, name, None)


_reserved = []


def IsReserved(key):
	if not _reserved:
		for name in RESERVED_NAMES:
			code = _Code(name)
			if code is not None:
				_reserved.append(code)
		_reserved.append(-1)
	return key in _reserved


def IsModifierKey(key):
	return key in (_Code('DIK_LCONTROL'), _Code('DIK_RCONTROL'), _Code('DIK_LSHIFT'),
		_Code('DIK_RSHIFT'), _Code('DIK_LALT'), _Code('DIK_RALT'), _Code('DIK_LMENU'), _Code('DIK_RMENU'))


def IsTapKey(key):
	for name in TAP_NAMES:
		code = _Code(name)
		if code is not None and code == key:
			return True
	return False


def _Pressed(*names):
	for name in names:
		code = _Code(name)
		if code is not None and app.IsPressed(code):
			return True
	return False


def CurrentMods():
	mods = 0
	if _Pressed('DIK_LCONTROL', 'DIK_RCONTROL'):
		mods |= CTRL
	if _Pressed('DIK_LSHIFT', 'DIK_RSHIFT'):
		mods |= SHIFT
	if _Pressed('DIK_LALT', 'DIK_RALT'):
		mods |= ALT
	return mods


def Actions():
	"""(category, [(action, label)]) of the actions this client runs."""
	result = []
	for category, actions in CATEGORIES:
		rows = [(action, label) for action, label, defaults, hold in actions
			if label is not None and IsAvailable(action)]
		if rows:
			result.append((category, rows))
	return result


def ActionLabel(action):
	for category, (name, label, defaults, hold) in _Rows():
		if name == action:
			return label
	return action


def _Defaults():
	result = {}
	for category, (action, label, defaults, hold) in _Rows():
		slots = [None] * SLOT_COUNT
		for idx, (mods, name) in enumerate(defaults[:SLOT_COUNT]):
			code = _Code(name)
			if code is not None:
				slots[idx] = (mods, code)
		result[action] = slots
	return result


def IsValidBinding(binding):
	"""A key with modifiers, or a Shift key alone; never Esc, Enter, Ctrl or
	Alt alone, nor a Shift with modifiers."""
	if not binding:
		return False
	mods, key = binding
	if IsTapKey(key):
		return 0 == mods
	return not IsReserved(key) and not IsModifierKey(key)


_BROKEN = 'broken'


def _ParseBinding(text):
	"""(mods, key), None for an empty slot ("-"), _BROKEN for anything else."""
	text = text.strip()
	if not text or text == '-':
		return None
	try:
		mods, key = text.split(':', 1)
		mods, key = int(mods), int(key)
	except ValueError:
		return _BROKEN
	if mods & ~(CTRL | SHIFT | ALT) or not IsValidBinding((mods, key)):
		return _BROKEN
	return (mods, key)


def _RebuildLookup():
	lookup = {}
	for action, slots in _state['bindings'].items():
		if not IsAvailable(action):
			continue
		for binding in slots:
			if binding and binding not in lookup:
				lookup[binding] = action
	_state['lookup'] = lookup
	_state['version'] += 1


def Load():
	if _state['loaded']:
		return
	_state['loaded'] = True
	defaults = _Defaults()
	saved = {}
	try:
		handle = open(CONFIG_PATH, 'r')
		try:
			for line in handle:
				if '=' not in line:
					continue
				action, value = line.strip().split('=', 1)
				if action not in defaults:
					continue
				slots = [_ParseBinding(part) for part in value.split(',')[:SLOT_COUNT]]
				if _BROKEN in slots:
					continue	# a broken line: that action keeps its default
				slots += [None] * (SLOT_COUNT - len(slots))
				saved[action] = slots
		finally:
			handle.close()
	except Exception:
		# No file yet, or one that cannot be read: the defaults.
		saved = {}

	bindings = {}
	used = set()
	for action, slots in saved.items():
		# A combination the file gives twice stays with the first action.
		kept = []
		for binding in slots:
			if binding and binding in used:
				binding = None
			if binding:
				used.add(binding)
			kept.append(binding)
		bindings[action] = kept
	for action, slots in defaults.items():
		if action in bindings:
			continue
		# A default the player gave to something else stays with that.
		bindings[action] = [binding if binding not in used else None for binding in slots]
	_state['bindings'] = bindings
	_RebuildLookup()


def Save():
	defaults = _Defaults()
	lines = []
	for action in sorted(_state['bindings'].keys()):
		slots = _state['bindings'][action]
		if slots == defaults.get(action):
			continue
		parts = ['%d:%d' % binding if binding else '-' for binding in slots]
		lines.append('%s=%s\n' % (action, ','.join(parts)))
	try:
		if not os.path.exists(CONFIG_DIR):
			os.makedirs(CONFIG_DIR)
		handle = open(CONFIG_PATH, 'w')
		try:
			handle.writelines(lines)
		finally:
			handle.close()
	except (IOError, OSError):
		pass


def SetAvailable(actions):
	"""The actions game.py runs (its __BuildKeyDict); the rest are not shown
	and take no keys."""
	_state['available'] = set(actions)
	Load()
	_RebuildLookup()


def IsAvailable(action):
	available = _state['available']
	return available is None or action in available


def GetVersion():
	Load()
	return _state['version']


def GetBinding(action, slot):
	Load()
	slots = _state['bindings'].get(action)
	if not slots:
		return None
	return slots[slot]


def GetText(action, separator=' / '):
	"""The action's keys, as a tooltip shows them: "W / Strz. g\xf3ra"."""
	Load()
	texts = [BindingText(binding) for binding in _state['bindings'].get(action, ()) if binding]
	return separator.join(texts)


def IsDefault(action):
	"""Whether the action has its default keys (uihelp.py keeps the help's
	own texts then)."""
	Load()
	return _state['bindings'].get(action) == _Defaults().get(action)


def DecorateLabel(label, action):
	"""A tooltip's text with the action's current key: "Kosz (J)"."""
	text = GetText(action)
	if not text:
		return label
	return '%s (%s)' % (label, text)


def FindOwner(binding):
	Load()
	return _state['lookup'].get(binding)


def AssignInto(bindings, action, slot, binding):
	"""Puts a binding in a slot of a bindings dict (the window's draft); the
	action that had it loses it. Returns that action, or None."""
	previous = None
	if binding:
		for other, slots in bindings.items():
			for idx, current in enumerate(slots):
				if current == binding and not (other == action and idx == slot):
					slots[idx] = None
					if other != action:
						previous = other
	bindings.setdefault(action, [None] * SLOT_COUNT)[slot] = binding
	return previous


def SetBinding(action, slot, binding):
	Load()
	previous = AssignInto(_state['bindings'], action, slot, binding)
	_RebuildLookup()
	Save()
	return previous


def CopyBindings():
	"""The bindings now in use, as a draft the window may change."""
	Load()
	return dict((action, list(slots)) for action, slots in _state['bindings'].items())


def DefaultBindings():
	return _Defaults()


def CommitBindings(bindings):
	"""The window's "Zapisz": the draft becomes the bindings, saved."""
	Load()
	_state['bindings'] = dict((action, list(slots)) for action, slots in bindings.items())
	_RebuildLookup()
	Save()


def TextOf(bindings, action, separator=' / '):
	return separator.join([BindingText(binding) for binding in bindings.get(action, ()) if binding])


def ResetToDefaults():
	Load()
	_state['bindings'] = _Defaults()
	_RebuildLookup()
	Save()


def Lookup(mods, key):
	"""The action of a key pressed with these modifiers: the combination,
	else the key alone."""
	Load()
	lookup = _state['lookup']
	action = lookup.get((mods, key))
	if action is None and mods:
		action = lookup.get((0, key))
	return action


def Resolve(key):
	return Lookup(CurrentMods(), key)


def IsHeld(action):
	"""Whether a key of the action is held now, its modifiers with it (the
	pick-up repeats while its key is held - updateable.py)."""
	Load()
	mods = CurrentMods()
	for binding in _state['bindings'].get(action, ()):
		if binding and not IsTapKey(binding[1]) and app.IsPressed(binding[1]) \
				and Lookup(mods, binding[1]) == action:
			return True
	return False


def MatchesKey(action, key):
	"""Whether a key just pressed is one of the action's (a window that
	closes on its own key - uihelp.py)."""
	return Resolve(key) == action


# A tap of Shift (game.py's OnKeyDown / OnKeyUp, OnIMEReturn; uiinventory's
# Shift + click): pressed alone, let go with nothing between.
def NoteKeyDown(key):
	if IsTapKey(key):
		_state['tap'] = key
	else:
		_state['tap'] = None


def NoteOtherInput():
	"""Enter, a click with Shift held: the Shift held is a modifier."""
	_state['tap'] = None


def TakeTap(key):
	"""The key let go: the action of its tap, or None."""
	if _state['tap'] is None or _state['tap'] != key:
		if IsTapKey(key):
			_state['tap'] = None
		return None
	_state['tap'] = None
	Load()
	return _state['lookup'].get((0, key))


# Waiting for a key (uikeybind.py). The key is taken in the game window's
# OnKeyDown (game.py), which gets DirectInput codes - the same as app.DIK_* and
# the bindings. A window's own OnKeyDown, locked or focused, gets Windows'
# virtual-key codes instead (L came as 76, DirectInput's Num 5), so the window
# never reads keys itself. While waiting, no action runs: the key goes only
# here. The handler gets (key, mods).
def StartCapture(handler):
	_state['capture'] = handler
	_state['tap'] = None


def StopCapture():
	_state['capture'] = None


def IsCapturing():
	return _state['capture'] is not None


def CaptureKey(key):
	"""game.py's OnKeyDown, first thing: True when the key was taken."""
	handler = _state['capture']
	if handler is None:
		return False
	if IsModifierKey(key):
		# A Shift may be the key itself: known on its release (CaptureKeyUp).
		NoteKeyDown(key)
		return True
	_state['tap'] = None
	if key == _Code('DIK_ESCAPE') or key == _Code('DIK_ESC'):
		_state['escapeAt'] = app.GetTime()
	handler(key, CurrentMods())
	return True


def CaptureKeyUp(key):
	"""game.py's OnKeyUp, first thing: a Shift tapped alone while waiting is
	the key. True when it was taken."""
	handler = _state['capture']
	if handler is None:
		return False
	if not IsTapKey(key) or _state['tap'] != key:
		return False
	_state['tap'] = None
	handler(key, 0)
	return True


def NoteEscape():
	_state['escapeAt'] = app.GetTime()


def TookEscape():
	"""Whether an Esc just ended a wait - the window's OnPressEscapeKey then
	does not close it as well."""
	took = 0 <= app.GetTime() - _state['escapeAt'] < 0.5
	_state['escapeAt'] = -1.0
	return took


def EscapeJustUsed():
	"""TookEscape without using it up (the game window's OnPressEscapeKey:
	no system menu for the Esc that cancelled a wait)."""
	return 0 <= app.GetTime() - _state['escapeAt'] < 0.5
