# The companion's gear ("Towarzysz", playerbot_sidekick.h on the server): its
# bag and what it wears, shown and handled the way the player's own inventory
# is, in a window of its own that the companion's window (uisidekick.py) opens
# with its "Plecak" and "Ekwipunek" buttons. And the answers about its skills
# and stats, which the status and skill pages of that window show: this module
# keeps what the server said (SkillModel) and routes the answer to every order
# of either window to the one that gave it (OnEqResult).
#
# The server's answers (protocol v1, CHAT_TYPE_COMMAND through game.py):
#
#   SidekickEqNone <protocol> <reason>          0 no companion, 1 not in the game
#                                               now, 2 companions off in this world
#                                               (the answer to either window)
#   SidekickEqBegin <protocol> <gen> <bagCells> <pageCells>
#                                               a whole picture follows
#   SidekickEqItem <gen> <pos> <vnum> <count> <flags> <s0> <s1> <s2> <attrs>
#   SidekickEqEmpty <gen> <pos>                 a place that is empty now
#   SidekickEqEnd <gen> <gold>                  end of a package, whole or changes
#   SidekickEqResult <code> <text>              0 done, 1 not in the game, 2
#                                               refused, 3 nothing there, 9 a bad
#                                               order; the text is hex CP1250
#   SidekickSkillBegin <protocol> <points> <job> <group> <manual> [<stats>
#                      [<statInfo> [<status>]]] stats since server 2.2.18: the
#                                               fifteen statuses the skill
#                                               tooltip's numbers come from
#                                               (sidekickskilltip.STAT_FIELDS);
#                                               statInfo since 2.2.19: the stat
#                                               points left, who spends them,
#                                               whether the free reset is left,
#                                               and vitality, intelligence,
#                                               strength, dexterity as spent;
#                                               status from the server after
#                                               2.2.35: what the status page
#                                               needs and the client cannot
#                                               work out - the experience and
#                                               what the level needs, the attack
#                                               the gear and the party add over
#                                               the weapon's, the defence boost
#                                               in percent and the moving speed
#                                               (STATUS_FIELDS)
#   SidekickSkill <vnum> <level> <grade>        level 0-40 as the engine keeps it
#   SidekickSkillEnd
#
# A position is a bag cell (page * 45 + row * 5 + column) or 1000 + WEAR_*, and
# -1 in an order means "wherever it fits". flags are bits: 1 the owner put it on
# (the AI will not take it off), 2 the owner's gift (the AI will not sell it), 4
# the owner took it off (the AI will not put it on by itself); "eq odepnij"
# clears 1 and 4. attrs is "-" or seven "type:value" pairs. A poll with nothing
# changed is not answered at all, and a package is applied in the order it comes;
# a whole picture comes with "eq 1" and by itself when the companion is back in
# the world.
#
# The orders are the server's own words: eq, eq 1, eq ruch <from> <to>,
# eq daj <myCell> <to>, eq wez <from> <myCell>, eq odepnij <pos>, umiejetnosci,
# umiejetnosci dodaj <vnum>, umiejetnosci reczne <0|1>, umiejetnosci zeruj <vnum>
# (since server 2.2.35), statystyki, statystyki dodaj <ht|iq|st|dx> <n>,
# statystyki reczne <0|1>, statystyki odnow.
# They leave through
# uisidekick.py's queue, one every 0.3 s for all the companion's windows
# together, because the server drops a sixth command in half a second.
#
# A companion's item on the cursor is mouseModule's attached object of a slot
# type the stock root never uses, SLOT_TYPE_SIDEKICK. The stock AttachObject
# finds the icon and the size only for the types it lists and drops anything
# else at once, so the icon is handed to it first and wndMgr told the real size
# after; its DeattachObject deletes the icon of those types only, so this module
# deletes its own once the cursor has let go of it. Every stock window that is
# dropped on reads the type and does nothing with one it does not know (the
# safebox asks SlotTypeToInvenType, which is RESERVED_WINDOW for anything past
# the engine's twelve types; the quick slots send it and the server refuses a
# quickslot type over three; a click in the world only lets go) - only the
# player's inventory takes it, through the lines clientrootify.py puts into
# uiinventory.py (DropIntoPlayerBag).
#
# MT2009_PLUS_SIDEKICK_QUICK_TRANSFER_V1: the safebox's right click. With this
# window open, a right click on an item of the player's bag gives it to the
# companion ("eq daj <cell> -1", uiinventory.py's OnRightClickBagItem through
# QuickGive), and a right click on an item of the companion's bag takes it to
# the player ("eq wez <pos> -1"); a double click there still puts it on (or
# uses the Bleach or a dye), and a right click on a worn piece still takes it
# off. The server's refusals are the window's drag's own (a locked or traded
# item, a dragon stone, no room). Flag 8 marks the owner's drop the companion
# picked up because the owner's bag was full ("Pelne EQ" in the Options page
# of uisidekick.py) - it holds it for the owner and the AI never touches it.
#
# Python 2.7 as the client has it; the Polish letters are CP1250 escapes.

import app
import chat
import clientclock
import item
import mouseModule
import player
import sidekickskilltip
import skill
import snd
import ui
import uiToolTip
import uisidekick
import wndMgr

EQ_PROTOCOL = 1
SKILL_PROTOCOL = 1

# A slot type none of the stock scripts attaches or reads (player.SLOT_TYPE_*
# runs 0-11, SLOT_TYPE_MAX is 12). As a BYTE it stays 101, past both tables the
# engine indexes by it.
SLOT_TYPE_SIDEKICK = 101

WEAR_BASE = 1000
WEAR_MAX = 32
DEFAULT_BAG_CELLS = 180
PAGE_CELLS = 45
GRID_COLUMNS = 5
GRID_ROWS = 9
MAX_PAGES = 4
CELL = 32
SOCKET_COUNT = 3
ATTR_COUNT = 7

FLAG_PINNED = 1
FLAG_GIFT = 2
FLAG_UNWANTED = 4
FLAG_HELD = 8	# MT2009_PLUS_SIDEKICK_QUICK_TRANSFER_V1
FLAGS_UNPIN = FLAG_PINNED | FLAG_UNWANTED

RESULT_DONE = 0
RESULT_NOT_IN_GAME = 1
RESULT_REFUSED = 2
RESULT_NOTHING_THERE = 3
RESULT_BAD_ORDER = 9

EQ_POLL_INTERVAL = 1.5
STATUS_SECONDS = 8.0
STATUS_LINES = 2
STATUS_LINE_HEIGHT = 14
# A use within this of a drop is the drop's second click.
DOUBLE_CLICK_SECONDS = 0.5
# The server cuts a text at 150 bytes; a little room over that.
RESULT_TEXT_BYTES = 200

# A normal skill takes points to seventeen; the Master grade comes from books.
MAX_POINT_LEVEL = 17

WEAR_BODY = 0
WEAR_HEAD = 1
WEAR_FOOTS = 2
WEAR_WRIST = 3
WEAR_WEAPON = 4
WEAR_NECK = 5
WEAR_EAR = 6
WEAR_UNIQUE1 = 7
WEAR_UNIQUE2 = 8
WEAR_ARROW = 9
WEAR_SHIELD = 10

# The player's own equipment slots (uiscript/inventorywindow.py, "EquipmentSlot"
# on equipment_base.sub): (wear, x, y, width, height). The costumes and the belt
# may come from the server as well; this window does not draw them.
EQUIPMENT_LAYOUT = (
	(WEAR_BODY, 39, 37, 32, 64),
	(WEAR_HEAD, 39, 2, 32, 32),
	(WEAR_FOOTS, 39, 145, 32, 32),
	(WEAR_WRIST, 75, 67, 32, 32),
	(WEAR_WEAPON, 3, 3, 32, 96),
	(WEAR_NECK, 114, 84, 32, 32),
	(WEAR_EAR, 114, 52, 32, 32),
	(WEAR_UNIQUE1, 2, 113, 32, 32),
	(WEAR_UNIQUE2, 75, 113, 32, 32),
	(WEAR_ARROW, 114, 1, 32, 32),
	(WEAR_SHIELD, 75, 35, 32, 32),
)
EQUIPMENT_BASE_IMAGE = 'd:/ymir work/ui/game/windows/equipment_base.sub'
EQUIPMENT_BASE_WIDTH = 156
EQUIPMENT_BASE_HEIGHT = 188
SLOT_BASE_IMAGE = 'd:/ymir work/ui/public/slot_base.sub'
MONEY_ICON_IMAGE = 'd:/ymir work/ui/game/windows/money_icon.sub'
TAB_IMAGE = 'd:/ymir work/ui/game/windows/tab_button_small_%02d.sub'
TAB_WIDTH = 32
TAB_HEIGHT = 19
# The player's window puts its four tabs 40 pixels apart, 4 in from the grid.
TAB_STEP = 40
BUTTON_IMAGE = 'd:/ymir work/ui/public/%s_button_%02d.sub'
PAGE_NAMES = ('I', 'II', 'III', 'IV')

# The owner's marks as a tint over the icon - no new picture.
MASK_PINNED = (0.25, 0.55, 1.0, 0.25)
MASK_UNWANTED = (1.0, 0.45, 0.2, 0.25)
MASK_GIFT = (0.3, 1.0, 0.35, 0.22)
MASK_PINNED_GIFT = (0.3, 0.9, 1.0, 0.25)
MASK_UNWANTED_GIFT = (1.0, 0.8, 0.25, 0.25)
MASK_HELD = (1.0, 0.85, 0.2, 0.22)	# MT2009_PLUS_SIDEKICK_QUICK_TRANSFER_V1

COLOR_NORMAL = 0xffc2c2c2
COLOR_GOOD = 0xff8ab98e
COLOR_BAD = 0xffe57975

STATE_WAITING = 0
STATE_OK = 1
STATE_NONE = 2
STATE_OTHER_PROTOCOL = 3

# Who gave the order an answer is for: the bag window, the skill page, the
# status page's "+" (which has no line for an answer: it goes to the chat, and
# the number it changed says the rest), and the options page.
ORIGIN_EQ = 'eq'
ORIGIN_SKILL = 'skill'
ORIGIN_STAT = 'stat'
ORIGIN_OPTIONS = 'options'

STAT_MAX = 90
STAT_INFO_FIELDS = ('points', 'manual', 'reset', 'ht', 'iq', 'st', 'dx')
# SidekickSkillBegin's words before the stat points': the protocol, the four
# of the skills and the tooltip's fifteen.
STAT_INFO_OFFSET = 20
# And after the stat points', what the status page needs from the server (the
# words of SendPlayerBotSidekickSkills): the experience and what the level
# needs, the attack over the weapon's, the defence boost in percent and the
# moving speed.
STATUS_FIELDS = ('exp', 'nextexp', 'attbonus', 'defbonus', 'movspeed')
STATUS_OFFSET = STAT_INFO_OFFSET + len(STAT_INFO_FIELDS)

# Every status text fits the line it is written on (uisidekick.FitText cuts the
# server's own if it must, and then the chat carries it whole).
TEXT_EQ_TITLE = 'Ekwipunek towarzysza'
TEXT_WAITING = 'Czekam na odpowied\x9f serwera...'
TEXT_NONE = ((
	'Nie masz jeszcze towarzysza.',
	'Towarzysz nie jest teraz w grze.',
	'Towarzysze s\xb9 tu wy\xb3\xb9czeni.',
))
TEXT_OTHER_PROTOCOL = 'Zaktualizuj klienta i serwer.'
TEXT_GOLD = 'Yang: %s'
TEXT_UNPIN = 'Odepnij'
TEXT_UNPIN_HOW = 'Podnie\x9c go i kliknij Odepnij.'
TEXT_NOT_PINNED = 'Tego nie trzeba odpina\xe6.'
TEXT_WEAR_TO_WEAR = 'Zdejmij go najpierw do torby.'
TEXT_ONLY_FROM_BAG = 'Daj mu przedmiot z torby.'
TEXT_WHOLE_STACK = 'Towarzysz bierze tylko ca\xb3y stos.'
TEXT_GIVE_YANG = 'Daj'
TEXT_TAKE_YANG = 'We\x9f'
TEXT_GIVE_YANG_TITLE = 'Daj yang towarzyszowi'
TEXT_TAKE_YANG_TITLE = 'We\x9f yang od towarzysza'
TEXT_YANG_AMOUNT = 'Kwota: '
TEXT_ITEM_MOVED = 'Towarzysz ju\xbf go przestawi\xb3.'
TEXT_RESULTS = {
	RESULT_DONE: 'Gotowe.',
	RESULT_NOT_IN_GAME: 'Towarzysz nie jest teraz w grze.',
	RESULT_REFUSED: 'Towarzysz odm\xf3wi\xb3.',
	RESULT_NOTHING_THERE: 'Tam nic nie ma.',
	RESULT_BAD_ORDER: 'Z\xb3e polecenie.',
}
TEXT_REFUSED = 'Nie uda\xb3o si\xea.'
TEXT_TIP_PINNED = 'Za\xb3o\xbfone przez ciebie - towarzysz tego nie zdejmie'
TEXT_TIP_GIFT = 'Prezent od ciebie'
TEXT_TIP_UNWANTED = 'Zdj\xeate przez ciebie - towarzysz sam tego nie za\xb3o\xbfy'
TEXT_TIP_UNPIN = 'Ctrl + klik: odepnij'
TEXT_TIP_EQUIP = 'Dwuklik: za\xb3\xf3\xbf'
# MT2009_PLUS_SIDEKICK_QUICK_TRANSFER_V1
TEXT_TIP_TAKE = 'Prawy klik: we\x9f do siebie'
TEXT_TIP_HELD = 'Tw\xf3j drop - trzyma go dla ciebie'
TEXT_TIP_UNEQUIP = 'Prawy klik: zdejmij'
TEXT_SKILL_NAME = 'Umiej\xeatno\x9c\xe6 %d'


# ---------------------------------------------------------------- parsing

def ParseInt(value, default=0):
	return uisidekick.ParseInt(value, default)


def ParseAttrs(value):
	"""'-' or up to seven 'type:value' pairs, as seven (type, value) tuples; a
	pair that is not two numbers is an empty line."""
	attrs = []
	if value and value != '-':
		for part in value.split(',')[:ATTR_COUNT]:
			pieces = part.split(':')
			if len(pieces) != 2:
				attrs.append((0, 0))
				continue
			kind = ParseInt(pieces[0], None)
			amount = ParseInt(pieces[1], None)
			if kind is None or amount is None or kind < 0:
				attrs.append((0, 0))
			else:
				attrs.append((kind, amount))
	while len(attrs) < ATTR_COUNT:
		attrs.append((0, 0))
	return attrs


def IsWearPos(pos):
	return WEAR_BASE <= pos < WEAR_BASE + WEAR_MAX


def WearPos(wear):
	return WEAR_BASE + wear


def IsPlayerBagCell(cell):
	return 0 <= cell < getattr(player, 'INVENTORY_DEFAULT_MAX_NUM', DEFAULT_BAG_CELLS)


def SkillGradeStep(level, grade):
	"""The engine's level (1-19, 20-29 M1-M10, 30-39 G1-G10, 40 P) as (grade,
	step). A server that sent the step within its grade is read as well."""
	level = max(0, level)
	if level >= 40:
		return 3, 1
	if level >= 30:
		return 2, level - 29
	if level >= 20:
		return 1, level - 19
	if grade >= 3:
		return 3, 1
	if grade in (1, 2) and level >= 1:
		return grade, min(level, 10)
	return 0, level


def SkillLevelText(level, grade=0):
	grade, step = SkillGradeStep(level, grade)
	if grade == 3:
		return 'P'
	if grade == 2:
		return 'G%d' % step
	if grade == 1:
		return 'M%d' % step
	return '%d' % step


def ParseStatInfo(args):
	"""The stat points' words after the tooltip's, as a dict; None from a
	server that does not send them."""
	if len(args) < len(STAT_INFO_FIELDS):
		return None
	info = {}
	for name, value in zip(STAT_INFO_FIELDS, args):
		info[name] = ParseInt(value, 0)
	info['points'] = max(0, info['points'])
	info['manual'] = 1 if info['manual'] else 0
	info['reset'] = 1 if info['reset'] else 0
	return info


def ParseStatus(args):
	"""The status page's words after the stat points', as a dict; None from a
	server that does not send them."""
	if len(args) < len(STATUS_FIELDS):
		return None
	status = {}
	for name, value in zip(STATUS_FIELDS, args):
		status[name] = ParseInt(value, 0)
	for name in ('exp', 'nextexp', 'movspeed'):
		status[name] = max(0, status[name])
	return status


def CanAddStatPoint(info, key):
	return bool(info) and info.get('points', 0) > 0 and info.get(key, STAT_MAX) < STAT_MAX


def CanAddSkillPoint(points, level, grade):
	grade, step = SkillGradeStep(level, grade)
	return points > 0 and grade == 0 and step < MAX_POINT_LEVEL


def MaskFor(flags):
	if flags & FLAG_HELD:
		return MASK_HELD
	gift = flags & FLAG_GIFT
	if flags & FLAG_PINNED:
		return MASK_PINNED_GIFT if gift else MASK_PINNED
	if flags & FLAG_UNWANTED:
		return MASK_UNWANTED_GIFT if gift else MASK_UNWANTED
	if gift:
		return MASK_GIFT
	return None


WEARABLE_TYPE_NAMES = ('ITEM_TYPE_WEAPON', 'ITEM_TYPE_ARMOR', 'ITEM_TYPE_UNIQUE', 'ITEM_TYPE_COSTUME',
	'ITEM_TYPE_BELT', 'ITEM_TYPE_ROD', 'ITEM_TYPE_PICK')


def IsWearable(vnum):
	"""A kind of item that goes on a body - the right click's "zaloz" is offered
	for these. Only a hint: the order goes for anything, the server decides."""
	kinds = [getattr(item, name) for name in WEARABLE_TYPE_NAMES if hasattr(item, name)]
	if not kinds:
		return True
	try:
		item.SelectItem(vnum)
		return item.GetItemType() in kinds
	except Exception:
		return True


def ToolTipLines(pos, flags, wearable=True):
	"""What the tooltip says under the item: the owner's marks and the clicks."""
	lines = []
	if flags & FLAG_PINNED:
		lines.append(TEXT_TIP_PINNED)
	if flags & FLAG_UNWANTED:
		lines.append(TEXT_TIP_UNWANTED)
	if flags & FLAG_HELD:
		lines.append(TEXT_TIP_HELD)
	elif flags & FLAG_GIFT:
		lines.append(TEXT_TIP_GIFT)
	if flags & FLAGS_UNPIN:
		lines.append(TEXT_TIP_UNPIN)
	if IsWearPos(pos):
		lines.append(TEXT_TIP_UNEQUIP)
	else:
		lines.append(TEXT_TIP_TAKE)
		if wearable:
			lines.append(TEXT_TIP_EQUIP)
	return lines


def ResultText(code, text):
	if text:
		return text
	return TEXT_RESULTS.get(code, TEXT_REFUSED)


# ---------------------------------------------------------------- the model

class EquipmentModel(object):
	"""What the server said the companion carries: the bag, what it wears and
	its yang. Kept whether a window is open or not."""

	def __init__(self):
		self.Reset()

	def Reset(self):
		self.state = STATE_WAITING
		self.reason = 0
		self.gen = 0
		self.bagCells = DEFAULT_BAG_CELLS
		self.pageCells = PAGE_CELLS
		self.items = {}
		self.gold = 0
		# A whole picture is gathered apart and put in place at its End, so a
		# window never shows half of one.
		self.incoming = None

	def Pages(self):
		pages = (self.bagCells + self.pageCells - 1) // self.pageCells
		return max(1, min(MAX_PAGES, pages))

	def IsValidPos(self, pos):
		return 0 <= pos < self.bagCells or IsWearPos(pos)

	def Get(self, pos):
		return self.items.get(pos)

	def OnNone(self, args):
		if not args:
			return False
		protocol = ParseInt(args[0], -1)
		self.Reset()
		if protocol != EQ_PROTOCOL:
			self.state = STATE_OTHER_PROTOCOL
			return True
		self.state = STATE_NONE
		self.reason = ParseInt(args[1], 0) if len(args) > 1 else 0
		return True

	def OnBegin(self, args):
		if not args:
			return False
		if ParseInt(args[0], -1) != EQ_PROTOCOL:
			self.Reset()
			self.state = STATE_OTHER_PROTOCOL
			return True
		pageCells = ParseInt(args[3], PAGE_CELLS) if len(args) > 3 else PAGE_CELLS
		if not 1 <= pageCells <= PAGE_CELLS:
			pageCells = PAGE_CELLS
		bagCells = ParseInt(args[2], DEFAULT_BAG_CELLS) if len(args) > 2 else DEFAULT_BAG_CELLS
		if not 1 <= bagCells <= pageCells * MAX_PAGES:
			bagCells = min(DEFAULT_BAG_CELLS, pageCells * MAX_PAGES)
		if len(args) > 1:
			self.gen = ParseInt(args[1], self.gen)
		self.bagCells = bagCells
		self.pageCells = pageCells
		if self.state in (STATE_NONE, STATE_OTHER_PROTOCOL):
			self.state = STATE_WAITING
		self.incoming = {}
		return True

	def _Target(self):
		return self.incoming if self.incoming is not None else self.items

	def OnItem(self, args):
		if self.state in (STATE_NONE, STATE_OTHER_PROTOCOL) or len(args) < 4:
			return False
		pos = ParseInt(args[1], None)
		if pos is None or not self.IsValidPos(pos):
			return False
		self.gen = ParseInt(args[0], self.gen)
		vnum = ParseInt(args[2], 0)
		if vnum <= 0:
			self._Target().pop(pos, None)
			return True
		self._Target()[pos] = {
			'pos': pos,
			'vnum': vnum,
			'count': max(1, ParseInt(args[3], 1)),
			'flags': max(0, ParseInt(args[4], 0)) if len(args) > 4 else 0,
			'sockets': [ParseInt(args[5 + i], 0) if len(args) > 5 + i else 0 for i in range(SOCKET_COUNT)],
			'attrs': ParseAttrs(args[8] if len(args) > 8 else '-'),
		}
		return True

	def OnEmpty(self, args):
		if self.state in (STATE_NONE, STATE_OTHER_PROTOCOL) or len(args) < 2:
			return False
		pos = ParseInt(args[1], None)
		if pos is None:
			return False
		self.gen = ParseInt(args[0], self.gen)
		self._Target().pop(pos, None)
		return True

	def OnEnd(self, args):
		if self.incoming is None and self.state in (STATE_NONE, STATE_OTHER_PROTOCOL):
			return False
		if self.incoming is not None:
			self.items = self.incoming
			self.incoming = None
		if args:
			self.gen = ParseInt(args[0], self.gen)
		if len(args) > 1:
			self.gold = max(0, ParseInt(args[1], self.gold))
		self.state = STATE_OK
		return True


class SkillModel(object):
	"""What the server said of the companion's skills and stats: the list and
	its points (the skill page), the fifteen statuses (the skill tooltip and
	the status page), the stat points (the status page's "+" and the options)
	and the status page's own numbers. Kept whether a window is open or not."""

	def __init__(self):
		self.Reset()

	def Reset(self):
		self.state = STATE_WAITING
		self.reason = 0
		self.points = 0
		self.job = 0
		self.group = 0
		self.manual = 0
		self.skills = []
		self.stats = None
		self.statInfo = None
		self.status = None
		self.incoming = None

	def OnBegin(self, args):
		if not args:
			return False
		if ParseInt(args[0], -1) != SKILL_PROTOCOL:
			self.Reset()
			self.state = STATE_OTHER_PROTOCOL
			return True
		values = [ParseInt(args[i], 0) if len(args) > i else 0 for i in range(1, 5)]
		self.incoming = {
			'points': max(0, values[0]),
			'job': max(0, values[1]),
			'group': max(0, values[2]),
			'manual': 1 if values[3] else 0,
			'skills': [],
			'stats': sidekickskilltip.ParseStats(args[5:]),
			'statInfo': ParseStatInfo(args[STAT_INFO_OFFSET:]),
			'status': ParseStatus(args[STATUS_OFFSET:]),
		}
		return True

	def OnSkill(self, args):
		if self.incoming is None or len(args) < 2:
			return False
		vnum = ParseInt(args[0], 0)
		if vnum <= 0:
			return False
		level = max(0, min(40, ParseInt(args[1], 0)))
		grade = max(0, min(3, ParseInt(args[2], 0))) if len(args) > 2 else 0
		skills = self.incoming['skills']
		for i, (known, _, _) in enumerate(skills):
			if known == vnum:
				skills[i] = (vnum, level, grade)
				return True
		skills.append((vnum, level, grade))
		return True

	def OnEnd(self, args):
		if self.incoming is None:
			return False
		incoming = self.incoming
		self.incoming = None
		self.points = incoming['points']
		self.job = incoming['job']
		self.group = incoming['group']
		self.manual = incoming['manual']
		self.skills = incoming['skills']
		self.stats = incoming['stats']
		self.statInfo = incoming['statInfo']
		self.status = incoming['status']
		self.state = STATE_OK
		return True

	def OnNone(self, state, reason):
		self.Reset()
		self.state = state
		self.reason = reason


_equipment = EquipmentModel()
_skills = SkillModel()
_state = {'eqWindow': None, 'icon': 0, 'lastOrigin': None}


def GetSkillModel():
	"""The skills and stats as the server last said them (the companion's
	window, uisidekick.py, shows them)."""
	return _skills


# ---------------------------------------------------------------- the cursor

def _Controller():
	return getattr(mouseModule, 'mouseController', None)


def IsCursorBusy():
	"""Whether the cursor holds anything - no tooltip over a slot then."""
	controller = _Controller()
	return bool(controller) and controller.isAttached()


def IsSidekickAttached():
	controller = _Controller()
	return bool(controller) and controller.isAttached() and controller.GetAttachedType() == SLOT_TYPE_SIDEKICK


def AttachItem(owner, pos, vnum, count):
	"""Puts the companion's item at pos on the cursor. False when the cursor
	already holds something or the item has no icon."""
	controller = _Controller()
	if not controller or controller.isAttached():
		return False
	ReleaseIcon()
	item.SelectItem(vnum)
	handle = item.GetIconInstance()
	if not handle:
		return False
	(width, height) = item.GetItemSize()
	# AttachObject keeps a type it does not list only when the icon is there
	# already, and tells wndMgr one cell - the grid under the cursor picks its
	# cells by that size, so the real one follows.
	controller.AttachedIconHandle = handle
	controller.AttachObject(owner, SLOT_TYPE_SIDEKICK, pos, vnum, count)
	if controller.isAttached() and controller.GetAttachedType() == SLOT_TYPE_SIDEKICK and \
			controller.AttachedIconHandle == handle:
		_state['icon'] = handle
		wndMgr.AttachIcon(SLOT_TYPE_SIDEKICK, vnum, pos, width, height)
		return True
	# AttachObject caught something and left the flag up with no icon.
	if controller.isAttached() and controller.GetAttachedType() == SLOT_TYPE_SIDEKICK:
		controller.DeattachObject()
	if controller.AttachedIconHandle == handle:
		controller.AttachedIconHandle = 0
	item.DeleteIconInstance(handle)
	return False


def ReleaseIcon(force=False):
	"""Deletes the icon this module handed the cursor once the cursor has let go
	of it; force lets go first."""
	handle = _state['icon']
	if not handle:
		return
	controller = _Controller()
	held = bool(controller) and controller.isAttached() and controller.AttachedIconHandle == handle
	if held:
		if not force:
			return
		controller.DeattachObject()
	item.DeleteIconInstance(handle)
	_state['icon'] = 0


def Deattach():
	controller = _Controller()
	if controller and controller.isAttached():
		controller.DeattachObject()
	ReleaseIcon()


def IsCtrlPressed():
	return app.IsPressed(app.DIK_LCONTROL) or app.IsPressed(app.DIK_RCONTROL)


# ---------------------------------------------------------------- orders

def SendOrder(origin, text):
	_state['lastOrigin'] = origin
	uisidekick.SendCommand(text)


def DropIntoPlayerBag(attachedType, attachedPos, cell):
	"""The player's inventory window calls this for whatever it is dropped on
	(uiinventory.py, clientrootify.py): True when it was the companion's item and
	the order went. A cell outside the four bag pages - the player's own
	equipment, the horse's page, the belt - means the first free cell."""
	if attachedType != SLOT_TYPE_SIDEKICK:
		return False
	if not IsPlayerBagCell(cell):
		cell = -1
	SendOrder(ORIGIN_EQ, 'eq wez %d %d' % (attachedPos, cell))
	return True


def QuickGive(cell):
	"""MT2009_PLUS_SIDEKICK_QUICK_TRANSFER_V1: a right click on the player's
	bag with this window open gives the item to the companion, as the
	safebox's does (uiinventory.py, OnRightClickBagItem). True when the order
	went; False with the window shut, a cell outside the bag pages or the
	cursor holding something."""
	if not AnyShown() or IsCursorBusy() or not IsPlayerBagCell(cell):
		return False
	if not player.GetItemIndex(cell):
		return False
	SendOrder(ORIGIN_EQ, 'eq daj %d -1' % cell)
	return True


def YangOrderText(text):
	"""What the amount dialog holds, as the server reads it ("eq yang daj
	<kwota>", ParsePlayerBotSidekickYang): digits with one decimal point and
	k's ("1.5kk", "500k"); a thousands separator is dropped. '' when it
	names no amount."""
	text = (text or '').strip().lower().replace(' ', '')
	body = text.rstrip('k')
	ks = len(text) - len(body)
	if body.count('.') + body.count(',') > 1:
		body = body.replace('.', '').replace(',', '')
	body = body.replace(',', '.')
	if not body or body[0] == '.' or ks > 3:
		return ''
	for c in body:
		if not (c.isdigit() or c == '.'):
			return ''
	return body + 'k' * ks


# ---------------------------------------------------------------- the window

class _Window(ui.BoardWithTitleBar):
	"""The bag window's frame: the board, the widgets, the status lines, the
	placement beside the companion's window."""

	WIDTH = 220
	HEIGHT = 300
	TITLE = ''

	def __init__(self):
		ui.BoardWithTitleBar.__init__(self)
		self.widgets = []
		self.statusLines = []
		self.placed = False
		self.statusUntil = 0.0
		self.nextPoll = 0.0
		self.AddFlag('movable')
		self.AddFlag('float')
		self.SetSize(self.WIDTH, self.HEIGHT)
		self.SetTitleName(self.TITLE)
		self.SetCloseEvent(ui.__mem_func__(self.Close))

	def _Label(self, parent, x, y, text):
		line = ui.TextLine()
		line.SetParent(parent)
		line.SetPosition(x, y)
		line.SetText(text)
		line.Show()
		self.widgets.append(line)
		return line

	def _Image(self, parent, x, y, name):
		image = ui.ImageBox()
		image.SetParent(parent)
		image.SetPosition(x, y)
		image.LoadImage(name)
		image.Show()
		self.widgets.append(image)
		return image

	def _Btn(self, parent, size, x, y, text, event, *args):
		button = ui.Button()
		button.SetParent(parent)
		button.SetPosition(x, y)
		button.SetUpVisual(BUTTON_IMAGE % (size, 1))
		button.SetOverVisual(BUTTON_IMAGE % (size, 2))
		button.SetDownVisual(BUTTON_IMAGE % (size, 3))
		button.SetText(text)
		button.SAFE_SetEvent(event, *args)
		button.Show()
		self.widgets.append(button)
		return button

	def _StatusLines(self, y):
		for i in range(STATUS_LINES):
			line = self._Label(self, self.WIDTH // 2, y + i * STATUS_LINE_HEIGHT, '')
			line.SetHorizontalAlignCenter()
			self.statusLines.append(line)

	def SetStatus(self, text, color=COLOR_NORMAL, seconds=STATUS_SECONDS, toChat=True):
		"""Two lines under the window. What does not fit is cut, and a server's
		text cut short goes to the chat whole, so nothing it said is lost."""
		if not self.statusLines:
			return
		for line in self.statusLines:
			line.SetPackedFontColor(color)
		if not uisidekick.WrapText(self.statusLines, text, self.WIDTH - 20) and toChat:
			chat.AppendChat(chat.CHAT_TYPE_INFO, text)
		self.statusUntil = clientclock.Now() + seconds if seconds else 0.0

	def StatusText(self):
		return ' '.join(line.GetText() for line in self.statusLines if line.GetText())

	def SetIdleStatus(self, text, color=COLOR_NORMAL):
		self.SetStatus(text, color, 0, False)

	def UpdateStatusClock(self):
		if self.statusUntil and clientclock.Now() >= self.statusUntil:
			self.statusUntil = 0.0
			self.RefreshIdleStatus()

	def RefreshIdleStatus(self):
		pass

	def RefreshStatusFor(self, model):
		# An answer stands its seconds; that there is no companion (or no
		# answer yet) is said at once, over whatever stood.
		if model.state != STATE_OK:
			self.statusUntil = 0.0
		if not self.statusUntil:
			self.RefreshIdleStatus()

	def PlaceBeside(self, anchor, preferRight):
		if self.placed:
			return
		self.placed = True
		try:
			screenWidth = wndMgr.GetScreenWidth()
			screenHeight = wndMgr.GetScreenHeight()
			(anchorX, anchorY) = anchor.GetGlobalPosition()
			anchorWidth = anchor.GetWidth()
		except Exception:
			self.SetCenterPosition()
			return
		right = anchorX + anchorWidth + 4
		left = anchorX - self.WIDTH - 4
		for x in ((right, left) if preferRight else (left, right)):
			if 0 <= x and x + self.WIDTH <= screenWidth:
				self.SetPosition(x, max(0, min(anchorY, screenHeight - self.HEIGHT)))
				return
		self.SetCenterPosition()

	def Place(self, anchor, preferRight):
		if anchor is not None:
			self.PlaceBeside(anchor, preferRight)
		elif not self.placed:
			self.placed = True
			self.SetCenterPosition()

	def OnPressEscapeKey(self):
		self.Close()
		return True


def IdleText(model):
	"""The status of a window with nothing to say but where its answer stands."""
	if model.state == STATE_WAITING:
		return TEXT_WAITING, COLOR_NORMAL
	if model.state == STATE_NONE:
		reason = model.reason if 0 <= model.reason < len(TEXT_NONE) else 0
		return TEXT_NONE[reason], COLOR_NORMAL
	if model.state == STATE_OTHER_PROTOCOL:
		return TEXT_OTHER_PROTOCOL, COLOR_BAD
	return '', COLOR_NORMAL


class EquipmentWindow(_Window):
	"""The companion's bag and what it wears, on the player's own equipment
	picture and bag grid."""

	WIDTH = 240
	HEIGHT = 590
	TITLE = TEXT_EQ_TITLE

	def __init__(self):
		_Window.__init__(self)
		self.page = 0
		self.tooltip = None
		self.droppedAt = -DOUBLE_CLICK_SECONDS
		self.yangDialog = None
		self.yangGive = True
		self.Build()
		self.Refresh()

	# ---------------------------------------------------------- building

	def Build(self):
		# The player's inventory, top to bottom: the equipment picture, the page
		# tabs, the 5x9 grid - and under it the yang and two status lines, in 590
		# pixels so the window fits an 800x600 screen. The 240 of its width are
		# for those lines: the server's answers run to some eighty characters.
		y = 32
		self.equipmentBase = self._Image(self, (self.WIDTH - EQUIPMENT_BASE_WIDTH) // 2, y, EQUIPMENT_BASE_IMAGE)
		equip = ui.SlotWindow()
		equip.SetParent(self.equipmentBase)
		equip.SetPosition(3, 3)
		equip.SetSize(150, 182)
		for wear, x, slotY, width, height in EQUIPMENT_LAYOUT:
			equip.AppendSlot(WearPos(wear), x, slotY, width, height)
		equip.SetSelectEmptySlotEvent(ui.__mem_func__(self.OnSelectEmptyEquip))
		equip.SetSelectItemSlotEvent(ui.__mem_func__(self.OnSelectItemEquip))
		equip.SetUnselectItemSlotEvent(ui.__mem_func__(self.OnRightClickEquip))
		equip.SetUseSlotEvent(ui.__mem_func__(self.OnUseEquip))
		equip.SetOverInItemEvent(ui.__mem_func__(self.OnOverInEquip))
		equip.SetOverOutItemEvent(ui.__mem_func__(self.OnOverOut))
		equip.Show()
		self.widgets.append(equip)
		self.equipSlots = equip
		# Where the player's window keeps its three little buttons, bottom right
		# of the picture, clear of every slot.
		self.unpinButton = self._Btn(self.equipmentBase, 'small', EQUIPMENT_BASE_WIDTH - 47,
			EQUIPMENT_BASE_HEIGHT - 23, TEXT_UNPIN, self.OnUnpinButton)
		y += EQUIPMENT_BASE_HEIGHT + 3

		gridX = (self.WIDTH - GRID_COLUMNS * CELL) // 2
		self.tabs = []
		for i in range(MAX_PAGES):
			tab = ui.RadioButton()
			tab.SetParent(self)
			tab.SetPosition(gridX + 4 + i * TAB_STEP, y)
			tab.SetUpVisual(TAB_IMAGE % 1)
			tab.SetOverVisual(TAB_IMAGE % 2)
			tab.SetDownVisual(TAB_IMAGE % 3)
			tab.SetText(PAGE_NAMES[i])
			tab.SAFE_SetEvent(self.SetPage, i)
			tab.Show()
			self.widgets.append(tab)
			self.tabs.append(tab)
		y += TAB_HEIGHT + 3

		grid = ui.GridSlotWindow()
		grid.SetParent(self)
		grid.SetPosition(gridX, y)
		grid.ArrangeSlot(0, GRID_COLUMNS, GRID_ROWS, CELL, CELL, 0, 0)
		grid.SetSlotBaseImage(SLOT_BASE_IMAGE, 1.0, 1.0, 1.0, 1.0)
		grid.SetSelectEmptySlotEvent(ui.__mem_func__(self.OnSelectEmptyBag))
		grid.SetSelectItemSlotEvent(ui.__mem_func__(self.OnSelectItemBag))
		grid.SetUnselectItemSlotEvent(ui.__mem_func__(self.OnRightClickBag))
		grid.SetUseSlotEvent(ui.__mem_func__(self.OnUseBag))
		grid.SetOverInItemEvent(ui.__mem_func__(self.OnOverInBag))
		grid.SetOverOutItemEvent(ui.__mem_func__(self.OnOverOut))
		grid.Show()
		self.widgets.append(grid)
		self.bagSlots = grid
		y += GRID_ROWS * CELL + 4

		# The companion's yang, and the two ways it moves (server 2.2.27): the
		# amount to the left edge and the buttons to the right, on the one line
		# the 590 pixels leave.
		self._Image(self, 14, y + 1, MONEY_ICON_IMAGE)
		self.goldLine = self._Label(self, 34, y, '')
		self.giveYangButton = self._Btn(self, 'small', self.WIDTH - 98, y - 2, TEXT_GIVE_YANG, self.OnGiveYang)
		self.takeYangButton = self._Btn(self, 'small', self.WIDTH - 52, y - 2, TEXT_TAKE_YANG, self.OnTakeYang)
		y += 20
		self._StatusLines(y)

		tooltip = uiToolTip.ItemToolTip()
		# The tooltip judges by the player's own class and level; the piece is
		# the companion's.
		if hasattr(tooltip, 'SetCannotUseItemForceSetDisableColor'):
			tooltip.SetCannotUseItemForceSetDisableColor(False)
		tooltip.HideToolTip()
		self.tooltip = tooltip

	# ---------------------------------------------------------- positions

	def BagPos(self, localSlot):
		return self.page * _equipment.pageCells + localSlot

	def SetPage(self, page):
		self.page = max(0, min(page, _equipment.Pages() - 1))
		self.Refresh()

	# ---------------------------------------------------------- showing

	def Refresh(self):
		model = _equipment
		pages = model.Pages()
		if self.page >= pages:
			self.page = pages - 1
		for i, tab in enumerate(self.tabs):
			if i < pages:
				tab.Show()
			else:
				tab.Hide()
			if i == self.page:
				tab.Down()
			else:
				tab.SetUp()
		first = self.page * model.pageCells
		for local in range(GRID_COLUMNS * GRID_ROWS):
			entry = model.Get(first + local) if local < model.pageCells else None
			self.ShowEntry(self.bagSlots, local, entry)
		self.bagSlots.RefreshSlot()
		for wear, _, _, _, _ in EQUIPMENT_LAYOUT:
			self.ShowEntry(self.equipSlots, WearPos(wear), model.Get(WearPos(wear)))
		self.equipSlots.RefreshSlot()
		if model.state == STATE_OK:
			self.goldLine.SetText(TEXT_GOLD % uisidekick.FormatGold(model.gold))
		else:
			self.goldLine.SetText('')
		self.CheckAttached()
		self.RefreshStatusFor(model)

	def ShowEntry(self, slots, slot, entry):
		if not entry:
			slots.ClearSlot(slot)
			return
		count = entry['count'] if entry['count'] > 1 else 0
		slots.SetItemSlot(slot, entry['vnum'], count, socket=tuple(entry['sockets']))
		mask = MaskFor(entry['flags'])
		if mask:
			slots.SetSlotMaskColorRaw(slot, mask[0], mask[1], mask[2], mask[3])
		else:
			slots.SetSlotMaskColorRaw(slot, 0.0, 0.0, 0.0, 0.0)

	def RefreshIdleStatus(self):
		text, color = IdleText(_equipment)
		self.SetIdleStatus(text, color)

	def CheckAttached(self):
		# An item the AI moved or sold while the player held it is not the one
		# an order would name any more.
		if not IsSidekickAttached():
			return
		controller = _Controller()
		entry = _equipment.Get(controller.GetAttachedSlotNumber())
		if entry and entry['vnum'] == controller.GetAttachedItemIndex():
			return
		Deattach()
		# With the companion gone the window says that instead.
		if _equipment.state == STATE_OK:
			self.SetStatus(TEXT_ITEM_MOVED, COLOR_BAD, toChat=False)

	# ---------------------------------------------------------- the mouse

	def OnSelectEmptyBag(self, slot):
		self.OnSelect(self.BagPos(slot), False)

	def OnSelectItemBag(self, slot):
		self.OnSelect(self.BagPos(slot), True)

	def OnSelectEmptyEquip(self, slot):
		self.OnSelect(slot, False)

	def OnSelectItemEquip(self, slot):
		self.OnSelect(slot, True)

	def OnRightClickBag(self, slot):
		self.OnRightClick(self.BagPos(slot))

	def OnRightClickEquip(self, slot):
		self.OnRightClick(slot)

	def OnUseBag(self, slot):
		self.OnUse(self.BagPos(slot))

	def OnUseEquip(self, slot):
		self.OnUse(slot)

	def OnSelect(self, pos, occupied):
		controller = _Controller()
		if controller and controller.isAttached():
			self.DropOn(pos)
			return
		entry = _equipment.Get(pos)
		if not occupied or not entry:
			return
		if IsCtrlPressed() and entry['flags'] & FLAGS_UNPIN:
			self.Unpin(pos)
			return
		if AttachItem(self, pos, entry['vnum'], entry['count']):
			self.OnOverOut()
			snd.PlaySound('sound/ui/pick.wav')

	def DropOn(self, pos):
		controller = _Controller()
		kind = controller.GetAttachedType()
		source = controller.GetAttachedSlotNumber()
		self.droppedAt = clientclock.Now()
		if kind == SLOT_TYPE_SIDEKICK:
			if source == pos:
				pass
			elif IsWearPos(source) and IsWearPos(pos):
				self.SetStatus(TEXT_WEAR_TO_WEAR, COLOR_BAD, toChat=False)
			else:
				SendOrder(ORIGIN_EQ, 'eq ruch %d %d' % (source, pos))
		elif kind == player.SLOT_TYPE_INVENTORY:
			count = controller.GetAttachedItemCount()
			if controller.GetAttachedItemIndex() == player.ITEM_MONEY:
				# Yang dropped on the bag is given, as the amount picked.
				if count > 0:
					SendOrder(ORIGIN_EQ, 'eq yang daj %d' % count)
			elif not IsPlayerBagCell(source):
				self.SetStatus(TEXT_ONLY_FROM_BAG, COLOR_BAD, toChat=False)
			elif 0 < count < player.GetItemCount(source):
				# The order carries no count: a split stack would go whole.
				self.SetStatus(TEXT_WHOLE_STACK, COLOR_BAD, toChat=False)
			else:
				SendOrder(ORIGIN_EQ, 'eq daj %d %d' % (source, pos))
		Deattach()

	def OnRightClick(self, pos):
		controller = _Controller()
		if controller and controller.isAttached():
			Deattach()
			return
		entry = _equipment.Get(pos)
		if not entry:
			return
		if IsCtrlPressed() and entry['flags'] & FLAGS_UNPIN:
			self.Unpin(pos)
			return
		# MT2009_PLUS_SIDEKICK_QUICK_TRANSFER_V1: a bag item goes to the player
		# (the safebox's right click); a worn piece comes off into its bag.
		if IsWearPos(pos):
			SendOrder(ORIGIN_EQ, 'eq ruch %d -1' % pos)
		else:
			SendOrder(ORIGIN_EQ, 'eq wez %d -1' % pos)

	def OnUse(self, pos):
		# The first click of a double click has put the item on the cursor - or,
		# with something already there, dropped that: then it is not a use.
		if clientclock.Now() - self.droppedAt < DOUBLE_CLICK_SECONDS:
			return
		if IsSidekickAttached():
			Deattach()
		if _equipment.Get(pos):
			SendOrder(ORIGIN_EQ, 'eq ruch %d -1' % pos)

	def Unpin(self, pos):
		entry = _equipment.Get(pos)
		if not entry:
			return
		if not entry['flags'] & FLAGS_UNPIN:
			self.SetStatus(TEXT_NOT_PINNED, toChat=False)
			return
		SendOrder(ORIGIN_EQ, 'eq odepnij %d' % pos)

	def OnUnpinButton(self):
		if IsSidekickAttached():
			pos = _Controller().GetAttachedSlotNumber()
			Deattach()
			self.Unpin(pos)
			return
		self.SetStatus(TEXT_UNPIN_HOW, toChat=False)

	# ---------------------------------------------------------- the tooltip

	def OnOverInBag(self, slot):
		self.ShowToolTipFor(self.BagPos(slot))

	def OnOverInEquip(self, slot):
		self.ShowToolTipFor(slot)

	def ShowToolTipFor(self, pos):
		tooltip = self.tooltip
		if not tooltip:
			return
		controller = _Controller()
		if controller and controller.isAttached():
			return
		entry = _equipment.Get(pos)
		if not entry:
			tooltip.HideToolTip()
			return
		sockets = list(entry['sockets'])
		while len(sockets) < getattr(player, 'METIN_SOCKET_MAX_NUM', SOCKET_COUNT):
			sockets.append(0)
		attrs = list(entry['attrs'])
		while len(attrs) < getattr(player, 'ATTRIBUTE_SLOT_MAX_NUM', ATTR_COUNT):
			attrs.append((0, 0))
		lines = ToolTipLines(pos, entry['flags'], IsWearable(entry['vnum']))
		tooltip.ClearToolTip()
		tooltip.AddItemData(entry['vnum'], sockets, attrs)
		marks = getattr(tooltip, 'POSITIVE_COLOR', COLOR_GOOD)
		tooltip.AppendSpace(5)
		for text in lines:
			tooltip.AppendTextLine(text, COLOR_NORMAL if text in (TEXT_TIP_EQUIP, TEXT_TIP_UNEQUIP, TEXT_TIP_UNPIN) else marks)
		tooltip.ShowToolTip()

	def OnOverOut(self):
		if self.tooltip:
			self.tooltip.HideToolTip()

	# ---------------------------------------------------------- yang

	def OnGiveYang(self):
		self.OpenYangDialog(True)

	def OnTakeYang(self):
		self.OpenYangDialog(False)

	def OpenYangDialog(self, give):
		"""The client's own amount dialog. What is typed goes to the server as
		YangOrderText makes it; the server says what came of it."""
		self.CloseYangDialog()
		import uiCommon
		dialog = uiCommon.MoneyInputDialog()
		dialog.SetTitle(TEXT_GIVE_YANG_TITLE if give else TEXT_TAKE_YANG_TITLE)
		if hasattr(dialog, 'SetMoneyHeaderText'):
			dialog.SetMoneyHeaderText(TEXT_YANG_AMOUNT)
		if hasattr(dialog, 'HideCheque'):
			try:
				dialog.HideCheque()
			except Exception:
				pass
		dialog.SetAcceptEvent(ui.__mem_func__(self.OnAcceptYang))
		dialog.SetCancelEvent(ui.__mem_func__(self.CloseYangDialog))
		dialog.Open()
		self.yangDialog = dialog
		self.yangGive = give

	def OnAcceptYang(self):
		dialog = self.yangDialog
		if not dialog:
			return
		text = YangOrderText(dialog.GetText())
		give = self.yangGive
		self.CloseYangDialog()
		if text:
			SendOrder(ORIGIN_EQ, 'eq yang %s %s' % ('daj' if give else 'wez', text))

	def CloseYangDialog(self):
		dialog = self.yangDialog
		self.yangDialog = None
		if dialog:
			dialog.Close()

	# ---------------------------------------------------------- the clock

	def OnUpdate(self):
		ReleaseIcon()
		self.UpdateStatusClock()
		# MT2009_PLUS_SIDEKICK_WARP_SAFE_V1: no polls on the way to another
		# core (uisidekick.InGame) - the bag's 'eq' every 1.5 s was the poll
		# a teleport with this window open met most often.
		if not uisidekick.InGame():
			return
		if uisidekick.PumpCommands():
			return
		now = clientclock.Now()
		# Nothing changed, nothing comes back: the poll does not wait for an answer.
		if now >= self.nextPoll and uisidekick.TryPoll('eq'):
			self.nextPoll = now + EQ_POLL_INTERVAL

	def Open(self, anchor=None):
		self.Place(anchor, True)
		self.Show()
		self.SetTop()
		self.nextPoll = clientclock.Now() + EQ_POLL_INTERVAL
		self.Refresh()
		SendOrder(ORIGIN_EQ, 'eq 1')

	def Close(self):
		if IsSidekickAttached():
			Deattach()
		ReleaseIcon(True)
		self.OnOverOut()
		self.CloseYangDialog()
		self.Hide()

	def Destroy(self):
		self.Close()
		self.tooltip = None
		self.widgets = []


def SkillName(vnum, grade=0):
	# skill.GetSkillName raises for a skill the client does not know.
	try:
		name = skill.GetSkillName(vnum, grade)
		if not name:
			name = skill.GetSkillName(vnum)
		if name:
			return name
	except Exception:
		pass
	return TEXT_SKILL_NAME % vnum


# ---------------------------------------------------------------- access

def GetEquipmentWindow():
	if _state['eqWindow'] is None:
		_state['eqWindow'] = EquipmentWindow()
	return _state['eqWindow']


def ToggleEquipmentWindow(anchor=None):
	window = GetEquipmentWindow()
	if window.IsShow():
		window.Close()
	else:
		window.Open(anchor)


def _Shown():
	window = _state['eqWindow']
	return window if window is not None and window.IsShow() else None


def AnyShown():
	"""Whether the companion's bag window is open."""
	return _Shown() is not None


def CloseAll():
	"""The bag window closed with the companion's own: P closed that one
	alone, and the bag stayed on the screen to be clicked shut (prodnathin,
	28 September)."""
	window = _Shown()
	if window is not None:
		window.Close()


def _RefreshEquipment():
	if _state['eqWindow'] is not None:
		_state['eqWindow'].Refresh()


def _RefreshSkills():
	# The status, skill and options pages of the companion's window read it.
	uisidekick.RefreshSkills()


# ---------------------------------------------------------------- the server

def OnEqNone(*args):
	# The answer to either window's question: with no companion there is
	# neither a bag nor skills to show.
	if _equipment.OnNone(args):
		_skills.OnNone(_equipment.state, _equipment.reason)
		_RefreshEquipment()
		_RefreshSkills()


def OnEqBegin(*args):
	if _equipment.OnBegin(args) and _equipment.state == STATE_OTHER_PROTOCOL:
		_RefreshEquipment()


def OnEqItem(*args):
	_equipment.OnItem(args)


def OnEqEmpty(*args):
	_equipment.OnEmpty(args)


def OnEqEnd(*args):
	if _equipment.OnEnd(args):
		_RefreshEquipment()


def OnEqResult(*args):
	"""The one answer to every order, where the order was given: the bag
	window's in its lines, the skill and options pages' in theirs
	(uisidekick.ShowResult). The status page's "+" has no line for one: its
	answer is the chat's, and the number it changed says the rest. When the
	window that asked is closed, in whatever of the companion's is open, and in
	the chat with none open. An item the answer names as "{i<vnum>}" is
	written in in the client's words
	(uisidekick.ExpandNames)."""
	code = ParseInt(args[0], -1) if args else -1
	text = ResultText(code, uisidekick.DecodeNamedText(args[1], RESULT_TEXT_BYTES) if len(args) > 1 else '')
	color = COLOR_NORMAL if code == RESULT_DONE else COLOR_BAD
	origin = _state['lastOrigin']
	bag = _Shown()
	if origin == ORIGIN_STAT:
		chat.AppendChat(chat.CHAT_TYPE_INFO, text)
	elif origin == ORIGIN_EQ and bag is not None:
		bag.SetStatus(text, color)
	elif origin != ORIGIN_EQ and uisidekick.ShowResult(origin, text, color):
		pass
	elif bag is not None:
		bag.SetStatus(text, color)
	elif not uisidekick.ShowResult(None, text, color):
		chat.AppendChat(chat.CHAT_TYPE_INFO, text)


def OnSkillBegin(*args):
	if _skills.OnBegin(args) and _skills.state == STATE_OTHER_PROTOCOL:
		_RefreshSkills()


def OnSkill(*args):
	_skills.OnSkill(args)


def OnSkillEnd(*args):
	if _skills.OnEnd(args):
		_RefreshSkills()


# ---------------------------------------------------------------- the game window

def Destroy():
	ReleaseIcon(True)
	window = _state['eqWindow']
	if window is not None:
		window.Destroy()
	_state['eqWindow'] = None
	_state['lastOrigin'] = None
	_equipment.Reset()
	_skills.Reset()


class Keeper(object):
	"""One of the game's updateables, for its Destroy alone: the game window's
	Close destroys every updateable, and the bag window goes with it rather
	than stand over the character select. It updates itself while shown, and
	uisidekick.py's keeper sends what is still queued."""

	def CanUpdate(self):
		return False

	def OnUpdate(self):
		pass

	def Destroy(self):
		Destroy()


def GetKeeper():
	return Keeper()
