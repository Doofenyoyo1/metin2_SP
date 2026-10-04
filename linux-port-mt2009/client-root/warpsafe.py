# MT2009_PLUS_SIDEKICK_WARP_SAFE_V1 (shared): is the connection in the game
# phase? Every window or updateable of ours that sends to the server from its
# OnUpdate - a poll, a timed request, an order it queued - asks this first.
#
# A teleport or a channel change keeps the game window and the windows over
# it updating while the client opens the connection to the next core: the
# exe's warp handler only sleeps and connects, and the game window closes
# when that core's loading phase begins. On the way the connection says
# hello in plain bytes (handshake), then both sides switch to the TEA
# encryption - the server when the hello is done, the client when it reads
# the login phase and again after its login packet. A packet written between
# those switches reaches the server under the other key: it reads garbage
# ("login phase does not handle this packet", "UNKNOWN HEADER ... REMAIN
# BYTES: 24"), closes the connection and the client lands on the login
# screen (the Companion window's polls, 3 October). The server's
# MT2009_PLUS_EARLY_PACKET_V1 passes over a chat command only while it can
# still read it.
#
# The client leaves the game phase (__LeaveGamePhase) as soon as the next
# core says hello and destroys every character there, its own too; until the
# next map's loading made it again the player's character is not in the
# character manager. Whatever is written before that hello goes out plain
# while the server is still in its handshake, where it is passed over whole.
#
# Python 2.7 as the client has it.


def InGame():
	"""True while the connection is in the game phase: the player's own
	character is in the character manager."""
	try:
		import chr
		import player
		vid = player.GetMainCharacterIndex()
		return bool(vid) and bool(chr.HasInstance(vid))
	except (ImportError, AttributeError):
		# A client without these calls: as before.
		return True
	except Exception:
		return False
