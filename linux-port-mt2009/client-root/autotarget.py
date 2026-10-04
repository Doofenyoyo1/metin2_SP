# MT2009_PLUS_AUTO_TARGET_V1 - the next target after a kill: once the target
# in hand dies, the client targets the nearest hostile monster this character
# has already been fighting (never a random monster, a player or an NPC).
#
# A bow shoots only at the target in the client's hand, and with none alive
# at whatever stands under the cursor (CPythonPlayer::NEW_Attack, which says
# NEED_TARGET when nothing does). So when an archer's target falls, the space
# bar shoots nothing until the cursor finds the next monster - in a crowd that
# stands all round the archer, every one of them struck by the passive's extra
# arrows already. A swing needs no target: it hits what is in front of it, and
# the client makes each victim the target by itself (OnHit's SetTarget,
# PythonPlayerEventHandler.cpp). So the game options' "Kolejny cel" row is on
# for a bow in the hand by default, and "Zawsze" takes it to every weapon - a
# black-magic Sura's or a Shaman's spells want a target too.
#
# "Already attacked" is what this client can see of a fight, which is little:
# Python has no list of the characters round it and hears of no hit. It does
# see the target in the player's own hand - set by a click on a monster, by a
# melee hit, by a skill, by the bow's pick under the cursor - and the character
# under the cursor when the player clicks (game.py's OnMouseLeftButtonDown,
# NoteClick). A monster or a Metin stone that was either is remembered as being
# in the fight while it lives and keeps near (NEAR_DISTANCE: the reach of the
# world's archer and mage monsters is 1000 at most); one that has kept away for
# FORGET_SECONDS is forgotten. When the target in hand dies, the nearest one
# remembered within PICK_DISTANCE becomes the target - a stone only when no
# monster is left, which is the Metin whose pack it was - and nothing else ever
# does: no player, no NPC, no monster this character did not fight - but one:
# MT2009_PLUS_AUTO_TARGET_V2, a monster attacking this character that it has
# not struck yet. Only the server knows those. A server that answers the probe
# "/autotarget_aggro 0" with AutoTargetAggroReady is asked once a kill
# ("/autotarget_aggro <tag>") and names the monsters near whose victim is this
# character (AutoTargetAggro <tag> <vid,...>); the answer counts only for the
# kill it was asked for and never overrides the player's hand. An older server
# is never asked, and the module works as before. The
# archer's extra arrows fly at the monsters already fighting the archer
# (CHARACTER::Shoot), but only the exe hears which (HEADER_GC_ADD_FLY_SHOOT_
# TARGETING), so a monster struck by nothing but an extra arrow is known here
# only if clicked, shot at, or named by the server as attacking this character.
#
# The player's hand comes first. Nothing here attacks or walks: the target is
# chosen, and a held space bar, a click or a skill does the rest. A click on a
# character after the kill, or within CLICK_RESPECT before it on any character
# but the target that fell, is the player's choice, and the client takes it
# when its own rules let it - nothing is chosen in its place. A target the
# client picks meanwhile (a melee hit, the bow's pick under the cursor) ends
# the wait the same way. A click on the ground is a walk, which choosing a
# target does not stop, so it changes nothing here. While Auto Lowy hunts, its
# targeting is the only one (uiautohunt.py).
#
# player.SetTarget is the client's own, with its rules. A change is refused
# without a word for a second after the last one (m_dwTargetEndTime, which
# every auto-attack step on the same target renews), and while the hand's
# motion has a fly event - a bow's whole shot (CanChangeTarget,
# __IsNeedFlyTargetMotion) - so the choice is tried every RETRY_INTERVAL for
# RETARGET_WINDOW. And a character off the screen cannot be targeted at all
# (CanPickInstance asks the view frustum): SetTarget then clears the target and
# tells the server so, so a candidate whose middle is not on the screen waits.
# A change that takes is the one target packet a click would send; a refused
# one sends nothing.
#
# Every time is clientclock.Now(): app.GetTime() starts again at every warp.
#
# Python 2.7 as the client has it (the module also imports under Python 3).

import chr
import clientclock
import net
import player
import sys
import wndMgr

# The game options' three choices, kept in autotarget.cfg beside the client:
# with a bow in the hand (the default), with any weapon, never.
MODE_OFF = 0
MODE_BOW = 1
MODE_ALWAYS = 2
MODES = (MODE_OFF, MODE_BOW, MODE_ALWAYS)
DEFAULT_MODE = MODE_BOW
CONFIG_FILE = 'autotarget.cfg'
CONFIG_KEY = 'mode'

# A monster this near is in the fight: at the character's side, or shooting
# or casting at it from its reach (mob_proto's ATTACK_RANGE, 1000 at most).
NEAR_DISTANCE = 1200
# How far the next target may stand. A monster that was struck and keeps
# further than this is not coming at the character; one that is comes within
# it inside the window below.
PICK_DISTANCE = 1500
# A remembered monster that has not been near for this long has left the
# fight - it went home, or it fights somebody else.
FORGET_SECONDS = 20.0
MAX_REMEMBERED = 24
# How long after the kill the choice is tried: the client's own refusals last
# a second after a change of target and a bow's shot about a second and a half.
RETARGET_WINDOW = 3.0
RETRY_INTERVAL = 0.15
# A click on another character this soon before the kill was the player
# choosing the next target, which the client's lock may still be holding back.
CLICK_RESPECT = 1.5
# A candidate the client would not take (off the screen at the frustum's word,
# though its middle was on it) is left for this long: past the second the
# client's own lock runs after the refusal, so the next candidate is tried
# first when it ends.
UNPICKABLE_SECONDS = 2.0
PRUNE_INTERVAL = 0.5
# The point of a candidate that must be on the screen: its middle, not its feet.
SCREEN_TEST_HEIGHT = 100
# MT2009_PLUS_AUTO_TARGET_V2: the server's list of the monsters attacking this
# character - asked at most this often, waited for this long before a Metin
# stone is taken in their place, and read up to this many.
QUERY_INTERVAL = 0.75
QUERY_WAIT = 0.75
MAX_AGGRO_TARGETS = 24

MONSTER = 'monster'
STONE = 'stone'
# CActorInstance::TYPE_STONE, which the chr module does not export.
STONE_INSTANCE_TYPE = 2

_state = {'mode': None, 'keeper': None}


def ModeFromText(text):
	"""The mode a settings file says; the default for anything else."""
	for line in (text or '').splitlines():
		key, sep, value = line.strip().partition('=')
		if not sep or key.strip() != CONFIG_KEY:
			continue
		try:
			mode = int(value.strip())
		except ValueError:
			return DEFAULT_MODE
		if mode in MODES:
			return mode
		return DEFAULT_MODE
	return DEFAULT_MODE


def Mode():
	if _state['mode'] is None:
		try:
			f = open(CONFIG_FILE, 'r')
			try:
				_state['mode'] = ModeFromText(f.read())
			finally:
				f.close()
		except (IOError, OSError):
			_state['mode'] = DEFAULT_MODE
	return _state['mode']


def SetMode(mode):
	"""The game options' choice, kept at once."""
	if mode not in MODES:
		mode = DEFAULT_MODE
	_state['mode'] = mode
	try:
		f = open(CONFIG_FILE, 'w')
		try:
			f.write('%s=%d\n' % (CONFIG_KEY, mode))
		finally:
			f.close()
	except (IOError, OSError):
		pass
	GetKeeper().Forget()
	return mode


def HoldsBow():
	if hasattr(player, 'IsBowEquipped'):
		return bool(player.IsBowEquipped())
	# An exe older than client 2.0.25 cannot say what is in the hand: a Ninja
	# of the archery school is taken to hold its bow (uiautohunt.py's Reach).
	import net
	return net.GetMainActorRace() % 4 == 1 and net.GetMainActorSkillGroup() == 2


def AutoHuntRunning():
	"""Whether Auto Lowy hunts. Its module is loaded with the game window; one
	that is not loaded hunts nothing."""
	hunt = sys.modules.get('uiautohunt')
	if hunt is None:
		return False
	return bool(getattr(hunt.GetHunter(), 'running', False))


def IsAlive(vid):
	if vid <= 0 or not chr.HasInstance(vid):
		return False
	# player.IsTargetDead (client 2.0.25's exe) says a monster is dead the
	# moment the server says so. An older exe keeps the corpse until it is
	# gone, which is two or three seconds later.
	if hasattr(player, 'IsTargetDead'):
		return not player.IsTargetDead(vid)
	return True


def Kind(vid):
	"""MONSTER, STONE or None - a player, an NPC, anything else, or nothing.
	chr.GetInstanceType names a missing instance a player, so it is asked only
	of one that is there."""
	if vid <= 0 or not chr.HasInstance(vid):
		return None
	kind = chr.GetInstanceType(vid)
	if kind == chr.INSTANCE_TYPE_ENEMY:
		return MONSTER
	if kind == getattr(chr, 'INSTANCE_TYPE_STONE', STONE_INSTANCE_TYPE):
		return STONE
	return None


def OnScreen(vid):
	# chr.GetProjectPosition says (-100, -100) for a point behind the camera.
	(x, y) = chr.GetProjectPosition(vid, SCREEN_TEST_HEIGHT)
	return 0 <= x < wndMgr.GetScreenWidth() and 0 <= y < wndMgr.GetScreenHeight()


class Keeper(object):
	"""One of the game window's updateables (game.py's CreateUpdateables)."""

	def __init__(self):
		self.broken = False
		self.lastClick = (None, 0)
		# MT2009_PLUS_AUTO_TARGET_V2: whether this game window's server answers
		# the query (None: not probed yet), and the tag of the last kill asked.
		self.serverReady = None
		self.requestSerial = 0
		self.nextQuery = 0.0
		self.Forget()

	def Forget(self):
		# The monsters in the fight, each with the last time it was in hand,
		# clicked or near.
		self.fought = {}
		# The target in hand at the last look, whether it lived, and what it was.
		self.target = 0
		self.targetAlive = False
		self.targetKind = None
		# A kill whose next target is being chosen: {'fallen', 'since', 'next',
		# 'request', 'queried', 'queryAt', 'replied', 'aggro'}.
		self.pending = None
		self.unpickable = {}
		self.nextPrune = 0.0

	def Destroy(self):
		# The game window closes: a warp or a logout. The next one starts afresh.
		self.Forget()
		self.lastClick = (None, 0)
		self.serverReady = None

	def CanUpdate(self):
		return not self.broken and Mode() != MODE_OFF

	def Active(self):
		mode = Mode()
		if mode == MODE_OFF or self.broken:
			return False
		if AutoHuntRunning():
			return False
		if player.GetStatus(player.HP) <= 0:
			return False
		if mode == MODE_BOW and not HoldsBow():
			return False
		return True

	def OnUpdate(self):
		# MT2009_PLUS_SIDEKICK_WARP_SAFE_V1: nothing on the way to another core.
		import warpsafe
		if not warpsafe.InGame():
			return
		try:
			self.Update(clientclock.Now())
		except Exception:
			self.Break()

	def Break(self):
		# Whatever went wrong here, the game window's frame must not: one line
		# in syserr.txt, and the module stands down until the client restarts.
		self.broken = True
		self.Forget()
		try:
			import dbg
			import traceback
			dbg.TraceError('autotarget: %s' % traceback.format_exc())
		except Exception:
			pass

	def Update(self, now):
		if not self.Active():
			if self.fought or self.pending or self.target:
				self.Forget()
			return
		if self.serverReady is None:
			# Once a game window: does this server name the attackers?
			self.serverReady = False
			net.SendChatPacket('/autotarget_aggro 0')
		current = player.GetTargetVID()
		alive = IsAlive(current)
		kind = Kind(current) if alive else None
		if kind:
			self.fought[current] = now
		# The target in hand fell: dead where it stood, or gone - the client
		# lets go of a target whose corpse it removes (NotifyCharacterDead),
		# which with an older exe is the first it knows of the death.
		if self.target and self.targetAlive and self.targetKind and (
				(current == self.target and not alive) or (not current and not IsAlive(self.target))):
			self.OnTargetFell(now, self.target)
		self.target = current
		self.targetAlive = alive
		self.targetKind = kind
		if self.pending:
			self.Retarget(now, current)
		if now >= self.nextPrune:
			self.Prune(now)

	def OnTargetFell(self, now, vid):
		self.fought.pop(vid, None)
		(clickTime, clickVid) = self.lastClick
		if clickTime is not None and now - clickTime <= CLICK_RESPECT and clickVid != vid:
			return
		self.requestSerial = self.requestSerial % 2147483647 + 1
		self.pending = {'fallen': vid, 'since': now, 'next': now,
			'request': self.requestSerial, 'queried': False, 'queryAt': None,
			'replied': False, 'aggro': []}

	def Retarget(self, now, current):
		pending = self.pending
		if now - pending['since'] > RETARGET_WINDOW:
			self.pending = None
			return
		if current and current != pending['fallen']:
			# The player's choice, the client's, or ours that took: the wait is over.
			self.pending = None
			return
		if self.serverReady and not pending['queried'] and now >= self.nextQuery:
			pending['queried'] = True
			pending['queryAt'] = now
			self.nextQuery = now + QUERY_INTERVAL
			net.SendChatPacket('/autotarget_aggro %d' % pending['request'])
		if now < pending['next']:
			return
		pending['next'] = now + RETRY_INTERVAL
		vid = self.Choose(now, pending['fallen'])
		if not vid:
			return
		# A remembered Metin stone does not win before the server has named
		# the pack attacking this character - for QUERY_WAIT at most.
		if Kind(vid) == STONE and self.serverReady and not pending['replied'] and (
				not pending['queried'] or now - pending['queryAt'] < QUERY_WAIT):
			return
		player.SetTarget(vid)
		after = player.GetTargetVID()
		if after == vid:
			self.pending = None
			self.fought[vid] = now
			self.target = vid
			self.targetAlive = True
			self.targetKind = Kind(vid)
		elif current and not after:
			# CanPickInstance said no: the client let go of the fallen target
			# instead. A refusal leaves the target as it was.
			self.unpickable[vid] = now + UNPICKABLE_SECONDS

	def Choose(self, now, fallen):
		"""The nearest monster of the fight within PICK_DISTANCE and on the
		screen; a stone only with no monster; 0 for none."""
		best = None
		for vid in set(list(self.fought.keys()) + self.pending['aggro']):
			if vid == fallen or self.unpickable.get(vid, 0.0) > now:
				continue
			if not IsAlive(vid):
				continue
			kind = Kind(vid)
			if not kind or not player.CanAttackInstance(vid):
				continue
			distance = player.GetCharacterDistance(vid)
			if distance < 0 or distance > PICK_DISTANCE:
				continue
			if not OnScreen(vid):
				continue
			key = (kind == STONE, distance, vid)
			if best is None or key < best:
				best = key
		if best is None:
			return 0
		return best[2]

	def Prune(self, now):
		self.nextPrune = now + PRUNE_INTERVAL
		for vid in list(self.fought.keys()):
			if not IsAlive(vid):
				del self.fought[vid]
				continue
			distance = player.GetCharacterDistance(vid)
			if 0 <= distance <= NEAR_DISTANCE:
				self.fought[vid] = now
			elif now - self.fought[vid] > FORGET_SECONDS:
				del self.fought[vid]
		if len(self.fought) > MAX_REMEMBERED:
			oldest = sorted(self.fought, key=lambda vid: self.fought[vid])
			for vid in oldest[:len(self.fought) - MAX_REMEMBERED]:
				del self.fought[vid]
		for vid in list(self.unpickable.keys()):
			if self.unpickable[vid] <= now:
				del self.unpickable[vid]

	def OnServerAggro(self, request, vids):
		"""MT2009_PLUS_AUTO_TARGET_V2: the server's answer. Only this kill's
		answer counts; a click, a warp, another kill, the option or Auto Lowy
		end it."""
		self.serverReady = True
		pending = self.pending
		if not pending or not pending['queried'] or not self.Active():
			return
		try:
			if int(request) != pending['request']:
				return
		except (TypeError, ValueError):
			return
		if clientclock.Now() - pending['since'] > RETARGET_WINDOW:
			return
		current = player.GetTargetVID()
		if current and current != pending['fallen']:
			return
		candidates = []
		for text in str(vids).split(',')[:MAX_AGGRO_TARGETS]:
			try:
				vid = int(text)
			except (TypeError, ValueError):
				continue
			if 0 < vid <= 4294967295 and vid not in candidates and Kind(vid) == MONSTER and IsAlive(vid):
				candidates.append(vid)
		pending['aggro'] = candidates
		pending['replied'] = True
		# The next try at once: the answer is what it was waiting for.
		pending['next'] = clientclock.Now()

	def NoteClick(self, vid):
		"""A click into the world, on the character under the cursor (chr.Pick:
		-1 for none). A click on the ground is a walk and changes nothing."""
		try:
			vid = int(vid)
		except (TypeError, ValueError):
			return
		if vid <= 0:
			return
		now = clientclock.Now()
		self.lastClick = (now, vid)
		if self.pending and vid != self.pending['fallen']:
			self.pending = None
		# A monster clicked on is attacked - the click is the order - even when
		# the client's lock keeps it out of the hand for a moment.
		if self.Active() and IsAlive(vid) and Kind(vid) and player.CanAttackInstance(vid):
			self.fought[vid] = now


def GetKeeper():
	if _state['keeper'] is None:
		_state['keeper'] = Keeper()
	return _state['keeper']


def NoteClick(vid):
	"""game.py's OnMouseLeftButtonDown, after the client has the click."""
	keeper = GetKeeper()
	if keeper.broken:
		return
	try:
		keeper.NoteClick(vid)
	except Exception:
		keeper.Break()


def OnServerAggro(request, vids='0', *rest):
	"""game.py's AutoTargetAggro server command (MT2009_PLUS_AUTO_TARGET_V2)."""
	keeper = GetKeeper()
	if keeper.broken:
		return
	try:
		keeper.OnServerAggro(request, vids)
	except Exception:
		keeper.Break()


def OnServerReady(*rest):
	"""game.py's AutoTargetAggroReady server command: the probe's answer."""
	GetKeeper().serverReady = True
