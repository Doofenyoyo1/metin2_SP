# Walking through one's own companion (server 2.2.24; the operator, 26 September:
# "towarzysz jest dla naszej postaci nieblokujacym, zeby dalo sie przez niego
# przenikac, bo jak expi sie z nim to on strasznie przeszkadza").
#
# The collision between two characters is the client's alone:
# CInstanceBase::CheckAdvancing tests the main instance against every other
# one, and CActorInstance::TestActorCollision skips a victim whose actor type
# is NPC (ENABLE_NPC_WITHOUT_COLLISIONS in this client's GameLib). The server
# tells the owner's client which character its companion is
# (playerbot_sidekick.h, SendPlayerBotSidekickBody):
#
#   SidekickVid <vid>     - 0 once it has gone
#
# and the keeper below types that instance as an NPC - chr.SetInstanceType on
# the selected instance - whenever the client has made it anew, which it does
# every time the companion comes back into view. Nothing else of the instance
# changes. What an NPC's type also means in this client: a click on the
# companion opens no player menu (its own window gives and takes items), a new
# hair or sash shows when it next comes into view, and the minimap draws it
# as an NPC.
#
# Python 2.7 as the client has it; any exe of this line has the three chr
# calls, and one without them only loses the collision, never the game.

import chr
import clientclock
import player

CHECK_EVERY = 0.25

_state = {'vid': 0, 'next': 0.0}


def ParseVid(value):
	try:
		vid = int(value)
	except (TypeError, ValueError):
		return 0
	return vid if vid > 0 else 0


def SetVid(value):
	_state['vid'] = ParseVid(value)
	_state['next'] = 0.0


def GetVid():
	return _state['vid']


def Apply():
	"""True when the companion's instance was typed as an NPC just now."""
	vid = _state['vid']
	if not vid:
		return False
	main = player.GetMainCharacterIndex()
	if vid == main or not chr.HasInstance(vid):
		return False
	if chr.GetInstanceType(vid) == chr.INSTANCE_TYPE_NPC:
		return False
	chr.SelectInstance(vid)
	chr.SetInstanceType(chr.INSTANCE_TYPE_NPC)
	# The selection is what every other chr.Set* call acts on; the main
	# character is where the stock scripts expect to find it.
	chr.SelectInstance(main)
	return True


class Keeper(object):
	"""Registered among game.py's updateables; ends with the game window, whose
	next one hears the VID again (a warp is a new login on the server)."""

	def CanUpdate(self):
		return _state['vid'] != 0

	def OnUpdate(self):
		now = clientclock.Now()
		if now < _state['next']:
			return
		_state['next'] = now + CHECK_EVERY
		try:
			Apply()
		except Exception:
			_state['vid'] = 0

	def Destroy(self):
		_state['vid'] = 0
		_state['next'] = 0.0


_keeper = []


def GetKeeper():
	if not _keeper:
		_keeper.append(Keeper())
	return _keeper[0]
