# The kills that win a guild war, on the war's board in the lower left (the
# operator, 1 October: "licznik nasz i wroga taki liczony od stu w dol aby
# wiedziec kto prowadzi").
#
# The stock board (uiguild.GuildWarScoreBoard) shows each side's score, and a
# field war's score is its kills on this server. A war the server ends by
# kills - one with a bot guild on a side, while the panel's WAR_KILLS is set -
# is won by the first side to that many, and the server says so to every member
# of both guilds: "guild_war_kills <guild> <enemy> <kills>" (game.py), once the
# character is in the game and again whenever the number changes; 0 takes it
# back. The board then says the target on a line of its own over the two
# sides, each side's kills and how many it still needs, counting down, and
# which side leads - green when it is the player's own guild, red when it is
# the enemy. A war the server does not end by kills - between two people's
# guilds, on an older server, with WAR_KILLS at 0 - keeps the stock board,
# "Name score".
#
# MT2009_PLUS_GUILD_WAR_KILLS_V1. Text only: uiguild.py's board makes, places
# and sizes the lines, and interfacemodule.py keeps the number by the war for
# a board made after it came. Python 2.7 as the client has it; the Polish
# letters are CP1250 escapes. This client speaks Polish only, so T() keeps the
# English twin of every text for a client that one day has a switch.


def T(polish, english):
	return polish


# The client's text lines draw an inline colour - EterLib's text tags: "|c"
# and eight hex digits, ARGB, and "|r" back to the line's own colour - and
# count in their width only what they draw.
OWN_LEAD_COLOR = '|cFF7CE07C'
ENEMY_LEAD_COLOR = '|cFFFF7A6E'
COLOR_END = '|r'

# What a letter of the default font is wide, near enough, for a text line that
# cannot say: the stock board sized itself at six a letter.
CHAR_WIDTH = 6

LEAD_NONE = 0
LEAD_OWN = 1
LEAD_ENEMY = 2


def ParseKills(value):
	"""The kills a command or a call names; anything but a number over 0 is
	no target."""
	try:
		kills = int(value)
	except (TypeError, ValueError):
		return 0
	if kills > 0:
		return kills
	return 0


def Remaining(score, kills):
	"""What a side still needs: the target less its kills, never under 0 - a
	reading can be a kill past it in the moment before the war ends."""
	return max(0, kills - max(0, score))


def Leader(ownScore, enemyScore):
	if ownScore > enemyScore:
		return LEAD_OWN
	if enemyScore > ownScore:
		return LEAD_ENEMY
	return LEAD_NONE


def SideName(name, memberCount):
	"""The stock board's name: with the side's head count in brackets once the
	server has sent one (an arena war's "WarUC"), else the name alone."""
	if memberCount is not None and memberCount >= 0:
		return '%s(%d)' % (name, memberCount)
	return name


def TitleText(kills, ownScore, enemyScore):
	"""The target's line; a level score once anybody has fallen says so."""
	if ownScore == enemyScore and ownScore > 0:
		return T('Do %d zab\xf3jstw - remis', 'First to %d kills - tied') % kills
	return T('Do %d zab\xf3jstw', 'First to %d kills') % kills


def SideText(name, memberCount, score, kills, leads, own):
	"""A side's line: its kills, what it still needs, and - for the side ahead -
	that it leads, in green for the player's own guild and red for the enemy."""
	text = T('%s %d - zosta\xb3o %d', '%s %d - %d to go') % (SideName(name, memberCount), score, Remaining(score, kills))
	if leads:
		color = OWN_LEAD_COLOR if own else ENEMY_LEAD_COLOR
		text += ' - ' + color + T('prowadzi', 'leads') + COLOR_END
	return text


def BoardTexts(kills, ownName, ownMembers, ownScore, enemyName, enemyMembers, enemyScore):
	"""The target's line and the two sides' lines, the player's own first."""
	lead = Leader(ownScore, enemyScore)
	return (TitleText(kills, ownScore, enemyScore),
		SideText(ownName, ownMembers, ownScore, kills, lead == LEAD_OWN, True),
		SideText(enemyName, enemyMembers, enemyScore, kills, lead == LEAD_ENEMY, False))


def VisibleText(text):
	"""The text as the client draws it: no colour tags, "||" one bar."""
	out = []
	i = 0
	while i < len(text):
		if text[i] == '|' and i + 1 < len(text):
			tag = text[i + 1]
			if tag == 'c' and i + 10 <= len(text):
				i += 10
				continue
			if tag == 'r':
				i += 2
				continue
			if tag == '|':
				out.append('|')
				i += 2
				continue
		out.append(text[i])
		i += 1
	return ''.join(out)


def TextWidth(line, text):
	"""The width the client gives a text line it has just been handed, or six a
	letter of what it draws where the line cannot say."""
	try:
		width = int(line.GetTextSize()[0])
	except Exception:
		width = 0
	if width > 0:
		return width
	return CHAR_WIDTH * len(VisibleText(text))
