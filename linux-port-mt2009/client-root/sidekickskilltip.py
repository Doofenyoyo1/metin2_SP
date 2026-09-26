# The tooltip over a companion's skill in its skills window: the one the
# player's own skill window shows (uitooltip.SkillToolTip), with the
# companion's numbers in it - "po najechaniu na skill, wyswietlala sie
# informacja dotyczaca danej umiejetnosci, dokladnie tak jak w okienku
# umiejetnosci wlasnej postaci" (Piciu713, 25 September).
#
# The stock tooltip reads the player: the skill's grade, level and power from
# the player's own skill slot, and every formula's variables from
# CPythonPlayer::GetStatus (CPythonSkill::SSkillData::ProcessFormula) - the
# attack and hit rate among them computed from the player's own weapon
# (__UpdateBattleStatus), which nothing in Python can set. So for the moment it
# takes to draw one, the tooltip's module sees two stand-ins for its `player`
# and `skill` modules: the skill slot and the statuses are the companion's, and
# the lines whose numbers come from a formula are worked out here, from the
# client's own formulas - locale/<lang>/skilldesc.txt's affect lines,
# gamedata/skilltable.txt's mana, duration and cooldown - with the companion's
# statuses, the way ProcessFormula works them out with the player's. The title,
# the grade's name, the description, the colours and the layout are the stock
# tooltip's own. The stand-ins go the moment the tooltip is drawn, whatever
# happened in it.
#
# The statuses come at the end of SidekickSkillBegin (playerbot_sidekick.h,
# SendPlayerBotSidekickSkills). The skill power by level and the battle points
# are worked out here as CPythonPlayer does it for the player
# (LocaleService_GetSkillPower, __GetHitRate, __GetTotalAtk), from the level,
# the stats and the weapon's vnum.
#
# Python 2.7 as the client has it.

import __future__
import math
import re
import struct

# SidekickSkillBegin after its first five: the companion's statuses, in order.
STAT_FIELDS = ('lv', 'st', 'dx', 'ht', 'iq', 'maxhp', 'maxsp', 'def', 'mwep', 'atkspd', 'magicatt',
	'skillduration', 'partybuffer', 'castingspeed', 'weapon')

# LocaleService_GetSkillPower (UserInterface/Locale.cpp): the power of a skill
# level in percent, 0 past the table.
SKILL_POWERS = (0,
	5, 6, 8, 10, 12, 14, 16, 18, 20, 22,
	24, 26, 28, 30, 32, 34, 36, 38, 40, 50,
	52, 54, 56, 58, 60, 63, 66, 69, 72, 82,
	85, 88, 91, 94, 98, 102, 106, 110, 115, 125,
	125)

# CPythonSkill::SSkillData::CheckIfSkillIsMagic: the magic attacks, whose
# "%.0f" lines take the bonus of mana and magic attack. Flame Spirit (78)
# takes half of it.
MAGIC_SKILLS = (35, 76, 77, 78, 80, 81, 91, 92, 93, 106, 107, 108)
FLAME_SPIRIT = 78

# ms_StatusNameMap (PythonSkill.cpp): a formula's variable and the status it
# reads. 'ar' is the hit rate divided by a hundred (ProcessFormula).
STATUS_VARIABLES = {
	'chain': 'none', 'HR': 'hit', 'ar': 'hitrate', 'LV': 'lv', 'Level': 'lv', 'lv': 'lv',
	'MaxHP': 'maxhp', 'maxhp': 'maxhp', 'MaxSP': 'maxsp', 'maxsp': 'maxsp',
	'MinMWEP': 'minwep', 'MaxMWEP': 'maxwep', 'MinWEP': 'minwep', 'MaxWEP': 'maxwep',
	'minmtk': 'minwep', 'maxmtk': 'maxwep', 'minwep': 'minwep', 'maxwep': 'maxwep',
	'MinATK': 'minatk', 'MaxATK': 'maxatk', 'AttackPower': 'minatk', 'AtkMin': 'minatk', 'AtkMax': 'maxatk',
	'minatk': 'minatk', 'maxatk': 'maxatk', 'ATKSPD': 'atkspd',
	'DefencePower': 'def', 'DEF': 'def', 'odef': 'def', 'MWEP': 'mwep', 'MagicAttackPower': 'mwep',
	'INT': 'iq', 'iq': 'iq', 'STR': 'st', 'str': 'st', 'DEX': 'dx', 'dex': 'dx', 'CON': 'ht', 'con': 'ht',
	'minmwep': 'minmwep', 'maxmwep': 'maxmwep',
}

# CPoly's functions (EterBase/Poly/Poly.cpp) the tables can use. A random
# number is the middle of its range: a tooltip has one number to show.
FUNCTIONS = {
	'min': min, 'max': max, 'floor': math.floor, 'abs': abs, 'sqrt': math.sqrt, 'rt': math.sqrt,
	'number': lambda low, high: (low + high) / 2.0, 'irandom': lambda low, high: (low + high) / 2.0,
	'irand': lambda low, high: (low + high) / 2.0, 'frandom': lambda low, high: (low + high) / 2.0,
	'frand': lambda low, high: (low + high) / 2.0, 'cos': math.cos, 'sin': math.sin, 'tan': math.tan,
	'log': math.log10, 'ln': math.log, 'mod': lambda a, b: math.fmod(a, b), 'pi': math.pi, 'e': math.e,
}

# skilldesc.txt: the columns of the four affect lines (description, min, max),
# PythonSkill.h's DESC_TOKEN_TYPE_AFFECT_* with ENABLE_4TH_AFF_SKILL_DESC.
DESC_AFFECT_FIRST = 17
DESC_AFFECT_COUNT = 4
# skilltable.txt: PythonSkill.h's TABLE_TOKEN_TYPE_* columns.
TABLE_SP_COST = 8
TABLE_DURATION = 9
TABLE_DURATION_SP_COST = 10
TABLE_COOLDOWN = 11

# The stock tooltip's line buffer: szDescription[64+1].
AFFECT_TEXT_MAX = 64

_NAME = re.compile(r'[A-Za-z_][A-Za-z_0-9]*')
# A dot before a name is an attribute, which no formula of CPoly has.
_ATTRIBUTE = re.compile(r'\.\s*[A-Za-z_]')
_SPEC = re.compile(r'%([-+ #0]*)(\d*)(?:\.(\d+))?([a-zA-Z%])')
_DIVISION = __future__.division.compiler_flag


def _Float32(value):
	"""A float as the client keeps one (C float), for the numbers that cross
	ProcessFormula in single precision: the skill power and the result."""
	try:
		return struct.unpack('f', struct.pack('f', value))[0]
	except (OverflowError, struct.error):
		return value


def _Text(data):
	if data is None:
		return ''
	if not isinstance(data, str):
		try:
			return data.decode('latin-1')
		except Exception:
			return ''
	return data


def SkillPower(level):
	"""The power of an engine skill level (0-40), as the client computes it:
	LocaleService_GetSkillPower(level) / 100.0f."""
	if 0 <= level < len(SKILL_POWERS):
		return _Float32(SKILL_POWERS[level] / 100.0)
	return 0.0


def ParseStats(args):
	"""SidekickSkillBegin's arguments after the first five, or None from a
	server that sends only those."""
	if len(args) < len(STAT_FIELDS):
		return None
	stats = {}
	for name, value in zip(STAT_FIELDS, args):
		try:
			stats[name] = int(value)
		except (TypeError, ValueError):
			stats[name] = 0
	return stats


def HitRate(level, dx):
	"""CPythonPlayer::__GetHitRate."""
	src = (dx * 4 + level * 2) // 6
	return 100 * (min(90, src) + 210) // 300 - ((level * 2 + 5) // (level + 95) * 30)


def StatAttack(job, st, dx, iq):
	"""CPythonPlayer::__GetStatAtk by class (job 0-3)."""
	if job == 0:
		return st * 2
	if job == 1:
		return dx + st
	if job == 2:
		return st + iq
	if job == 3:
		return (5 * iq + st) // 3
	return st


def WeaponValues(vnum, itemModule=None):
	"""(min, max, magic min, magic max, refine bonus) of a weapon, as
	__SetWeaponPower reads them: values 3, 4, 1, 2 and 5 of item_proto."""
	zero = (0, 0, 0, 0, 0)
	if not vnum:
		return zero
	try:
		if itemModule is None:
			import item as itemModule
		itemModule.SelectItem(vnum)
		if itemModule.GetItemType() != getattr(itemModule, 'ITEM_TYPE_WEAPON', 1):
			return zero
		return (itemModule.GetValue(3), itemModule.GetValue(4), itemModule.GetValue(1), itemModule.GetValue(2),
			itemModule.GetValue(5))
	except Exception:
		return zero


def CompanionStatus(stats, job, weapon=None):
	"""Every status a formula can read, the battle points worked out the way
	__UpdateBattleStatus works them out for the player."""
	status = dict(stats)
	level = stats.get('lv', 0)
	hit = HitRate(level, stats.get('dx', 0))
	low, high, magicLow, magicHigh, bonus = weapon if weapon is not None else (0, 0, 0, 0, 0)
	statAtk = StatAttack(job, stats.get('st', 0), stats.get('dx', 0), stats.get('iq', 0))
	status.update({
		'none': 0, 'hit': hit, 'hitrate': hit / 100.0,
		'minwep': low + bonus, 'maxwep': high + bonus, 'minmwep': magicLow + bonus, 'maxmwep': magicHigh + bonus,
		'minatk': 2 * level + (statAtk + 2 * (low + bonus)) * hit // 100,
		'maxatk': 2 * level + (statAtk + 2 * (high + bonus)) * hit // 100,
	})
	return status


def FormulaVariables(status, power, realLevel=0):
	"""ProcessFormula's variables: k and SkillPoint the power, sl the real
	skill level (1-40) and the grade flags from it, the rest statuses."""
	values = {}
	for name, key in STATUS_VARIABLES.items():
		values[name] = status.get(key, 0)
	values['k'] = values['SkillPoint'] = power
	values['sl'] = realLevel
	values['isgraden'] = 1 if realLevel < 20 else 0
	values['isgradem'] = 1 if 20 <= realLevel < 30 else 0
	values['isgradeg'] = 1 if 30 <= realLevel < 40 else 0
	values['isgradep'] = 1 if realLevel >= 40 else 0
	values['gr'] = 0 if realLevel < 20 else (1 if realLevel < 30 else (2 if realLevel < 40 else 3))
	return values


def EvalFormula(formula, values):
	"""A formula of the client's skill tables, or 0.0 for one it cannot read -
	CPoly's answer too. A variable nobody knows is 0; nothing but the names
	above can be reached from one."""
	text = _Text(formula).strip()
	if not text or _ATTRIBUTE.search(text):
		return 0.0
	text = text.replace('^', '**')
	try:
		code = compile(text, '<skill>', 'eval', _DIVISION, True)
	except Exception:
		return 0.0
	names = {}
	for name in code.co_names:
		if name in FUNCTIONS:
			names[name] = FUNCTIONS[name]
		elif _NAME.match(name) and not name.startswith('_'):
			names[name] = float(values.get(name, 0.0))
		else:
			return 0.0
	try:
		return float(eval(code, {'__builtins__': {}}, names))
	except Exception:
		return 0.0


def CFormat(fmt, numbers):
	"""_snprintf(desc, value, value) as the stock line is made: the
	conversions take the numbers in order and the rest are left out."""
	out = []
	pos = 0
	index = 0
	for match in _SPEC.finditer(fmt):
		out.append(fmt[pos:match.start()])
		pos = match.end()
		flags, width, precision, conv = match.groups()
		if conv == '%':
			out.append('%')
			continue
		value = numbers[index] if index < len(numbers) else 0.0
		index += 1
		if conv in 'fFeEgG':
			spec = '%' + flags + width + ('.' + precision if precision is not None else '') + conv
			out.append(spec % value)
		elif conv in 'diu':
			out.append('%d' % int(value))
	out.append(fmt[pos:])
	return ''.join(out)[:AFFECT_TEXT_MAX]


def MagicBonus(vnum, status):
	"""The mana and magic attack bonus of a magic skill's line
	(CPythonPlayer::GetMagicDamageBonusFromSP + POINT_MAGIC_ATT), in the
	client's single precision."""
	bonusSP = max(status.get('maxsp', 0) - 180 - 20 * (status.get('iq', 0) + status.get('lv', 0)), 0)
	bonusPow = math.pow(bonusSP, 1.22) if bonusSP > 0 else 0.0
	fromSP = _Float32(55.0 * bonusPow / (bonusPow + 7000))
	return _Float32(1 + _Float32(fromSP + status.get('magicatt', 0)) / (200.0 if vnum == FLAME_SPIRIT else 100.0))


def AffectText(vnum, affect, power, realLevel, status):
	"""CPythonSkill::SSkillData::GetAffectDescription with the companion. Like
	CPoly the formula is worked out in double and kept as a float, which is
	the difference between 112 and 111 for a mana cost of 100+200*k."""
	desc, minFormula, maxFormula = affect
	values = FormulaVariables(status, power, realLevel)
	low = abs(_Float32(EvalFormula(minFormula, values)))
	high = abs(_Float32(EvalFormula(maxFormula, values)))
	if '%.0f' in desc:
		if vnum in MAGIC_SKILLS:
			bonus = MagicBonus(vnum, status)
			low = _Float32(low * bonus)
			high = _Float32(high * bonus)
		low = math.floor(low)
		high = math.floor(high)
	return CFormat(desc, (low, high))


def _Lines(data):
	for line in _Text(data).split('\n'):
		fields = line.rstrip('\r').split('\t')
		if fields and fields[0].strip().isdigit():
			yield int(fields[0].strip()), fields


def ParseSkillDesc(data):
	"""skilldesc.txt: {vnum: [(description, min, max), ...]}, the affect lines
	RegisterSkillDesc keeps - those with a description."""
	table = {}
	for vnum, fields in _Lines(data):
		affects = []
		for j in range(DESC_AFFECT_COUNT):
			column = DESC_AFFECT_FIRST + j * 3
			if column >= len(fields) or not fields[column]:
				continue
			minFormula = fields[column + 1] if column + 1 < len(fields) else ''
			maxFormula = fields[column + 2] if column + 2 < len(fields) else ''
			affects.append((fields[column], minFormula, maxFormula))
		table[vnum] = affects
	return table


def ParseSkillTable(data):
	"""skilltable.txt: {vnum: {'sp', 'duration', 'contsp', 'cooldown'}}, each
	a formula or '' where RegisterSkillTable keeps none."""
	table = {}
	for vnum, fields in _Lines(data):
		def column(index):
			return fields[index] if index < len(fields) else ''
		table[vnum] = {'sp': column(TABLE_SP_COST), 'duration': column(TABLE_DURATION),
			'contsp': column(TABLE_DURATION_SP_COST), 'cooldown': column(TABLE_COOLDOWN)}
	return table


_tables = {'desc': None, 'table': None}


def SkillDescPaths():
	"""Where the client's skill descriptions are, the stock's own first:
	PythonApplication.cpp loads locale/<the language chosen>/SkillDesc.txt,
	which on a client of several languages is not app.GetLocalePath()."""
	paths = []
	try:
		import systemSetting
		language = systemSetting.GetLanguage()
		if language:
			paths.append('locale/%s/skilldesc.txt' % language)
	except Exception:
		pass
	try:
		import app
		paths.append('%s/skilldesc.txt' % app.GetLocalePath())
	except Exception:
		pass
	paths.append('locale/pl/skilldesc.txt')
	return paths


def _LoadTables():
	"""The client's own tables, read once from its packs: the description in
	the language the client runs in, where the stock loads it from."""
	if _tables['desc'] is None:
		_tables['desc'] = {}
		_tables['table'] = {}
		try:
			import pack
		except Exception:
			return _tables['desc'], _tables['table']
		for path in SkillDescPaths():
			try:
				desc = ParseSkillDesc(pack.Get(path))
			except Exception:
				continue
			if desc:
				_tables['desc'] = desc
				break
		try:
			_tables['table'] = ParseSkillTable(pack.Get('gamedata/skilltable.txt'))
		except Exception:
			_tables['table'] = {}
	return _tables['desc'], _tables['table']


class _Stand(object):
	"""A module seen through the companion: what is defined here is the
	companion's, everything else the module's own."""

	def __init__(self, real):
		self._real = real

	def __getattr__(self, name):
		return getattr(self._real, name)


class _PlayerStand(_Stand):
	def __init__(self, real, view):
		_Stand.__init__(self, real)
		self._view = view
		keys = (('LEVEL', 'lv'), ('ST', 'st'), ('DX', 'dx'), ('HT', 'ht'), ('IQ', 'iq'),
			('MAX_HP', 'maxhp'), ('MAX_SP', 'maxsp'), ('POINT_SKILL_DURATION', 'skillduration'),
			('POINT_PARTY_BUFFER_BONUS', 'partybuffer'))
		self._statusKeys = {}
		for name, key in keys:
			if hasattr(real, name):
				self._statusKeys[getattr(real, name)] = key

	def GetStatus(self, pointType):
		key = self._statusKeys.get(pointType)
		if key is None or key not in self._view.status:
			return self._real.GetStatus(pointType)
		return self._view.status[key]

	def GetSkillSlotIndex(self, skillIndex):
		return self._view.slot

	def GetSkillGrade(self, slotIndex):
		return self._view.grade

	def GetSkillLevel(self, slotIndex):
		return self._view.step

	def GetSkillCurrentEfficientPercentage(self, slotIndex):
		return SkillPower(self._view.level)

	def GetSkillNextEfficientPercentage(self, slotIndex):
		return SkillPower(self._view.level + 1)

	def GetCoolTimeReduction(self):
		# playerGetCoolTimeReduction, with the companion's casting speed.
		speed = 100 - self._view.status.get('castingspeed', 100)
		if speed > 0:
			speed = 100 + speed
		elif speed < 0:
			speed = 10000.0 / (100 - speed)
		else:
			speed = 100
		return speed / 100.0


class _SkillStand(_Stand):
	def __init__(self, real, view):
		_Stand.__init__(self, real)
		self._view = view

	def _Affects(self, skillIndex):
		return self._view.desc.get(skillIndex)

	def GetSkillAffectDescriptionCount(self, skillIndex):
		affects = self._Affects(skillIndex)
		if affects is None:
			return self._real.GetSkillAffectDescriptionCount(skillIndex)
		return len(affects)

	def GetSkillAffectDescription(self, skillIndex, index, skillLevel, power):
		affects = self._Affects(skillIndex)
		if affects is None:
			return self._real.GetSkillAffectDescription(skillIndex, index, skillLevel, power)
		if not 0 <= index < len(affects):
			return ''
		return AffectText(skillIndex, affects[index], _Float32(power), skillLevel, self._view.status)

	def _Formula(self, skillIndex, name, power, real, cast):
		row = self._view.table.get(skillIndex)
		if row is None:
			return real(skillIndex, power)
		formula = row.get(name, '')
		if not formula:
			return 0
		values = FormulaVariables(self._view.status, _Float32(power), 0)
		return cast(_Float32(EvalFormula(formula, values)))

	def GetSkillNeedSP(self, skillIndex, power):
		return self._Formula(skillIndex, 'sp', power, self._real.GetSkillNeedSP, int)

	def GetSkillContinuationSP(self, skillIndex, power):
		return self._Formula(skillIndex, 'contsp', power, self._real.GetSkillContinuationSP, _Unsigned)

	def GetDuration(self, skillIndex, power):
		return self._Formula(skillIndex, 'duration', power, self._real.GetDuration, _Unsigned)

	def GetSkillCoolTime(self, skillIndex, power):
		return self._Formula(skillIndex, 'cooldown', power, self._real.GetSkillCoolTime, _Unsigned)


def _Unsigned(value):
	# DWORD(float) in the client: what a negative gives there is no number
	# worth showing.
	return int(value) if value > 0 else 0


class SkillView(object):
	"""One skill of the companion as the stand-ins show it."""

	def __init__(self, vnum, level, grade, step, stats, job, weapon=None, desc=None, table=None):
		self.slot = 0
		self.vnum = vnum
		self.level = level
		self.grade = grade
		self.step = step
		self.status = CompanionStatus(stats, job, weapon)
		self.desc = desc if desc is not None else {}
		self.table = table if table is not None else {}


def _FunctionGlobals(toolTip, name):
	method = getattr(type(toolTip), name)
	function = getattr(method, '__func__', method)
	return function.__globals__


_MISSING = object()


def ShowWithStandIns(toolTip, view, draw):
	"""Draws the tooltip with the module it was written in seeing the
	companion for `player` and `skill`, and puts the modules back after."""
	namespace = _FunctionGlobals(toolTip, 'SetSkillNew')
	realPlayer = namespace.get('player', _MISSING)
	realSkill = namespace.get('skill', _MISSING)
	if realPlayer is _MISSING:
		import player as realPlayerModule
		namespace['player'] = _PlayerStand(realPlayerModule, view)
	else:
		namespace['player'] = _PlayerStand(realPlayer, view)
	if realSkill is _MISSING:
		import skill as realSkillModule
		namespace['skill'] = _SkillStand(realSkillModule, view)
	else:
		namespace['skill'] = _SkillStand(realSkill, view)
	try:
		draw()
	finally:
		for name, real in (('player', realPlayer), ('skill', realSkill)):
			if real is _MISSING:
				namespace.pop(name, None)
			else:
				namespace[name] = real


def Show(toolTip, vnum, level, grade, step, stats, job):
	"""The stock skill tooltip for the companion's skill: with its statuses
	when the server sent them, the name, description and grade alone when it
	did not (a server from before them). Through the stand-ins either way:
	player.GetSkillSlotIndex raises for a skill the player has not got, and a
	warrior's companion may well be a shaman."""
	if not stats:
		view = SkillView(vnum, level, grade, step, {}, job)
		ShowWithStandIns(toolTip, view, lambda: toolTip.SetSkillOnlyName(view.slot, vnum, grade))
		return
	desc, table = _LoadTables()
	view = SkillView(vnum, level, grade, step, stats, job, WeaponValues(stats.get('weapon', 0)), desc, table)
	ShowWithStandIns(toolTip, view, lambda: toolTip.SetSkillNew(view.slot, vnum, grade, step))
