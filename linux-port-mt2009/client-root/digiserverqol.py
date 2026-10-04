# MT2009_PLUS_DIGI_SERVER_QOL_V1 - Digi Rasta's server conveniences, the
# client's commands. Autor: Digi Rasta (nowy-system v0.23.0, the "Biore"
# systems of 3 October; his klient.py patched game.py for them).
#
# game.py calls Register(self) once its own commands are in (one line), and
# DestroyWindows() when the game window closes. The commands come from the
# core (playerbot_digi_qol.h, server-patches/digirasta-qol):
#
#   RefineFailedType <0|1|2>   after "RefineFailed": the item lost a level /
#                              was destroyed (or one piece of its stack) /
#                              stayed as it was - the popup says which
#   KillBar <killer race> <weapon> <victim race> <killer> <victim>
#                              the map's kill bar (uikillbar.py)
#   KillSound <1..13>          the kill streak's sound, mt2009_ui/killstreak/N.wav
#   SkillCoolTimeReset         skills are ready again after death: an exe with
#                              player.ResetSkillCoolTimes clears its timers
#                              (exe patch digi-server-qol); the shipped exe has
#                              no way to, so there the server alone resets and the
#                              client keeps its own countdown as before
#   DeadTime <here> <town>     the death window's countdown (uirestart.py)
#   NOWY_KSIEGI open|done <vnum>
#                              Seon-Hae's book exchange (uiskillbookexchange.py)
#
# The commands go straight into the game window's serverCommander: a callable
# with the window as a weak proxy (stringCommander's own callbacks need a bound
# method of the window, which would have been one more method each in game.py).
#
# Python 2.7 as the client has it; the texts are CP1250 escapes.
from _weakref import proxy

import player
import snd

REFINE_FAILED_TEXTS = {
	"0": "Ulepszenie nie powiod\xb3o si\xea - przedmiot straci\xb3 poziom ulepszenia.",
	"1": "Ulepszenie nie powiod\xb3o si\xea - przedmiot zosta\xb3 zniszczony.",
	"2": "Ulepszenie nie powiod\xb3o si\xea - przedmiot pozosta\xb3 nienaruszony.",
}
KILL_SOUND = "mt2009_ui/killstreak/%d.wav"
KILL_SOUND_MAX = 13
# The slots of a character window skill page (CPythonPlayer SKILL_MAX_NUM is 255; a page uses far fewer).
SKILL_PAGE_SLOTS = 64


def _RefineFailedType(game, kind="-1", *rest):
	text = REFINE_FAILED_TEXTS.get(kind)
	if text:
		game.PopupMessage(text)


def _KillBar(game, killerRace="0", weapon="255", victimRace="0", killer="", victim="", *rest):
	try:
		killerRace, weapon, victimRace = int(killerRace), int(weapon), int(victimRace)
	except ValueError:
		return
	import uikillbar
	uikillbar.Add(killerRace, weapon, victimRace, killer, victim)


def _KillSound(game, stage="1", *rest):
	try:
		stage = max(1, min(KILL_SOUND_MAX, int(stage)))
	except ValueError:
		return
	# MT2009_PLUS_KILL_SOUND_SWITCH_V1: only when ticked in "Opcje dodatkowe".
	try:
		import uiopcjedodatkowe
		if not uiopcjedodatkowe.Settings().get("killsound", False):
			return
	except Exception:
		return
	snd.PlaySound(KILL_SOUND % stage)


def _ClearCharacterSkillCoolTimes(interface):
	"""The character window's skill pages keep their slots' cooltime sweeps over
	a refresh (uicharacter.py, ENABLE_SLOT_WINDOW_EX); the reset takes them away."""
	window = getattr(interface, "wndCharacter", None)
	pages = getattr(window, "skillPageDict", None) if window else None
	if not pages:
		return
	for page in pages.values():
		try:
			start = page.GetStartIndex()
			for slot in xrange(start, start + SKILL_PAGE_SLOTS):
				page.SetSlotCoolTime(slot, 0.0, 0.0)
		except Exception:
			continue


def _SkillCoolTimeReset(game, *rest):
	if not hasattr(player, "ResetSkillCoolTimes"):
		return
	player.ResetSkillCoolTimes()
	interface = getattr(game, "interface", None)
	if interface:
		_ClearCharacterSkillCoolTimes(interface)
		interface.RefreshSkill()


def _DeadTime(game, here="0", town="0", *rest):
	try:
		here, town = int(here), int(town)
	except ValueError:
		return
	import uiRestart
	uiRestart.SetDeadTimes(here, town)


def _SkillBooks(game, *args):
	import uiskillbookexchange
	uiskillbookexchange.OnServer(*args)


COMMANDS = (
	("RefineFailedType", _RefineFailedType),
	("KillBar", _KillBar),
	("KillSound", _KillSound),
	("SkillCoolTimeReset", _SkillCoolTimeReset),
	("DeadTime", _DeadTime),
	("NOWY_KSIEGI", _SkillBooks),
)


class _Command:
	def __init__(self, game, func):
		self.game = proxy(game)
		self.func = func

	def __call__(self, *args):
		try:
			return self.func(self.game, *args)
		except ReferenceError:
			return None

	def GetArgumentCount(self):
		# stringCommander.Analyzer asks it only without variadic commands
		# (constInfo.ENABLE_CMDCHAT_VARIADIC_ARGS is on in this client).
		return 1


def Register(game):
	commander = getattr(game, "serverCommander", None)
	if commander is None or not hasattr(commander, "cmdDict"):
		return
	for name, func in COMMANDS:
		if name not in commander.cmdDict:
			commander.cmdDict[name] = _Command(game, func)


def DestroyWindows():
	"""The game window closes (a warp, a channel change, the logout)."""
	import uikillbar
	import uiskillbookexchange
	uikillbar.DestroyWindow()
	uiskillbookexchange.DestroyWindow()
