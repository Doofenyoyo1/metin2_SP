# MT2009_PLUS_EVENT_MANAGER_V1 - the in-game event list, the python side.
#
# The server (playerbot_ingame_events.h) keeps a list of events - every kind
# the panels' scheduler runs (chest, exp, drop, yang, tanaka, zuo, bossloot,
# metinloot, goblin, catchking, rumi, yutnori, flower, easter, ...) and the
# flag events (rumi_xmas) - each a key with on/off, start, end, the end of its
# reward window and a figure. A client gets it after it says hello:
#
#   /ingame_event hello <caps>     1 the packet HEADER_GC_INGAME_EVENT (the exe
#                                    has the ingameEventSystem module),
#                                  2 the same as chat lines "IGE ..." (an older
#                                    exe; see OnCommand),
#                                  4 Owsap's "<flag> <value>" commands (game.py
#                                    hands them to OnOwsapFlag).
#
# This module is what the rest of the python asks: with the new exe it reads
# the exe's module ingameEventSystem, without it a list of its own fed by the
# "IGE" lines - the same functions either way. What an event is called and
# what its button opens is uiingameevent.py's (EVENTS): a new event is a
# server kind and a line there, never a new exe.
#
# Owsap's mini game python asks ingameEventSystem.GetInGameEventEnable(TYPE)
# and friends; "import ingameevent as ingameEventSystem" gives it the same
# names (the INGAME_EVENT_TYPE_* below are our keys).
#
# Never name a file of the pack ingameeventsystem.py: the pack's import comes
# before the exe's modules and would hide the exe's list.
#
# Python 2.7 as the client has it.

import app
import net

try:
	import ingameEventSystem as _native
except ImportError:
	_native = None

CAP_PACKET = 1
CAP_TEXT = 2
CAP_OWSAP_FLAGS = 4

# The Owsap flag commands the server sends (game.py registers one handler
# each; a command nobody registered is an "Unknown Server Command" line).
OWSAP_FLAGS = ('mini_game_okey', 'mini_game_okey_normal', 'mini_game_yutnori',
		'mini_game_catchking', 'e_flower_drop', 'easter_drop')

# Owsap's type names, as our keys.
INGAME_EVENT_TYPE_OKEY = 'rumi_xmas'
INGAME_EVENT_TYPE_OKEY_NORMAL = 'rumi'
INGAME_EVENT_TYPE_YUTNORI = 'yutnori'
INGAME_EVENT_TYPE_CATCHKING = 'catchking'
INGAME_EVENT_TYPE_FLOWER_EVENT = 'flower'
INGAME_EVENT_TYPE_EASTER_EVENT = 'easter'

_data = {
	'events': [],		# the list without the exe: [key, enable, start, end, rewardEnd, value]
	'pending': None,	# "IGE begin" .. "IGE end"
	'flags': {},		# Owsap's flags as heard
	'listeners': [],
	'handler': None,
	'owsap': None,		# Owsap's MiniGameWindow, when its python is ported
}


class _Handler:
	"""What the exe calls when its list changed (Owsap's name)."""

	def RefreshInGameEvent(self, isRefresh=1):
		Notify()


def HasNative():
	return _native is not None


def Hello():
	"""The game window, a little after entering the world: forget the last
	core's list and ask this one for its own."""
	_data['flags'].clear()
	if _native:
		_native.Clear()
		if not _data['handler']:
			_data['handler'] = _Handler()
		_native.SetInGameEventHandler(_data['handler'])
		caps = CAP_PACKET
	else:
		del _data['events'][:]
		caps = CAP_TEXT
	net.SendChatPacket('/ingame_event hello %d' % (caps | CAP_OWSAP_FLAGS))


def Destroy():
	if _native:
		_native.DestroyInGameEventHandler()
	_data['handler'] = None
	_data['owsap'] = None
	del _data['listeners'][:]


def AddListener(func):
	if func not in _data['listeners']:
		_data['listeners'].append(func)


def RemoveListener(func):
	if func in _data['listeners']:
		_data['listeners'].remove(func)


def Notify():
	owsap = _data['owsap']
	if owsap:
		try:
			owsap.RefreshInGameEvent(True)
		except Exception, msg:
			import dbg
			dbg.TraceError('ingameevent handler: %s' % msg)
	for func in list(_data['listeners']):
		try:
			func()
		except Exception, msg:
			import dbg
			dbg.TraceError('ingameevent listener: %s' % msg)


# ------------------------------------------------------------------ the list

def _Find(key):
	for event in _data['events']:
		if event[0] == key:
			return event
	return None


def GetEventCount():
	if _native:
		return _native.GetEventCount()
	return len(_data['events'])


def GetEventKey(index):
	if _native:
		return _native.GetEventKey(index)
	if 0 <= index < len(_data['events']):
		return _data['events'][index][0]
	return ''


def GetEventKeys():
	return [GetEventKey(i) for i in xrange(GetEventCount())]


def GetEventInfo(key):
	"""(enable, start, end, reward end, value); zeros for an unknown key."""
	if _native:
		return _native.GetEventInfo(key)
	event = _Find(key)
	if not event:
		return (0, 0, 0, 0, 0)
	return tuple(event[1:6])


def IsEvent(key):
	if _native:
		return _native.IsEvent(key)
	return _Find(key) is not None


def IsActive(key):
	return GetEventInfo(key)[0] != 0


IsEventActive = IsActive


def GetEventStart(key):
	return GetEventInfo(key)[1]


def GetEventEnd(key):
	return GetEventInfo(key)[2]


GetEventEndTime = GetEventEnd


def GetEventRewardEnd(key):
	return GetEventInfo(key)[3]


def GetEventValue(key):
	return GetEventInfo(key)[4]


def GetActiveEvents():
	if _native:
		return _native.GetActiveEvents()
	return tuple([event[0] for event in _data['events'] if event[1]])


def GetActiveEventCount():
	return len(GetActiveEvents())


def GetServerTime():
	return app.GetGlobalTimeStamp()


def GetLeftTime(key):
	"""Seconds until the event ends; 0 for one without an end or over."""
	end = GetEventEnd(key)
	if not end:
		return 0
	return max(0, end - GetServerTime())


# Owsap's names, for its mini game python ("import ingameevent as
# ingameEventSystem").
def GetInGameEventEnable(key):
	return IsActive(key)


def GetInGameEventData(key):
	info = GetEventInfo(key)
	return (info[0], info[1], info[2])


def GetInGameEventEndTime(key):
	return GetEventEnd(key)


def GetInGameEventCount():
	return GetActiveEventCount()


def SetIngameEventHandler(handler):
	"""Owsap's MiniGameWindow: its RefreshInGameEvent(True) on every change."""
	_data['owsap'] = handler


SetInGameEventHandler = SetIngameEventHandler


def DestroyInGameEventHandler():
	_data['owsap'] = None


# ------------------------------------------------------- what the server says

def OnCommand(sub='', *args):
	"""The "IGE" lines, for a client whose exe has no list of its own:
	IGE begin <count> / IGE ev <key> <enable> <start> <end> <reward end> <value> / IGE end"""
	if sub == 'begin':
		_data['pending'] = []
	elif sub == 'ev':
		if _data['pending'] is None or len(args) < 6:
			return
		try:
			values = [int(a) for a in args[1:6]]
		except ValueError:
			return
		_data['pending'].append([args[0]] + values)
	elif sub == 'end':
		if _data['pending'] is None:
			return
		if _native:
			_native.Clear()
			for event in _data['pending']:
				_native.SetEvent(*event)
		else:
			_data['events'] = _data['pending']
		_data['pending'] = None
		Notify()


def OnOwsapFlag(name, value='0'):
	"""Owsap's "<flag> <value>" (mini_game_okey 1759000000 ...): kept for its
	mini game python; the list itself comes whole from the server."""
	try:
		_data['flags'][name] = int(value)
	except ValueError:
		return


def GetFlag(name):
	return _data['flags'].get(name, 0)
