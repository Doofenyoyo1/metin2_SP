# The companion's window ("Towarzysz", playerbot_sidekick.h on the server), on
# the P key and from the Towarzysz letter: the player's own character window
# with the companion in it - "tak jak w oryginale nasza glowna postac" (the operator,
# on Piciu713's and prodnathin's requests of 28 September: "jaki zakres ataku
# ma moj towarzysz i ile ma obrony ... ile ma punktow inteligencji czy zycia z
# uwzglednieniem ekwipunku ... wykorzystac taby 'Emocje' i 'Zadania' do
# przyciskow takich jak 'polecenia'").
#
# The window is the stock uiscript of the player's character window
# (UIScript/CharacterWindow.py, the one uicharacter.py loads) in a window of its
# own - the board, the pages and the tab strip are the player's, pixel for
# pixel - with the companion's numbers where the player's stand:
#
#   Status        the player's status page: the face and the name (the path
#                 where the guild stands), level and experience, the four
#                 stats with the gear counted (the spent points and the gear's
#                 share over the value), a "+" while it has points - Ctrl +
#                 click for several, as the player's -, HP and SP, the attack
#                 range and defence, magic attack and defence, the three speeds
#                 and evasion. The client works the battle numbers out as it
#                 does for the player (sidekickskilltip.CharacterNumbers); what
#                 it cannot know the server sends. The player's "Bonusy" opens
#                 the companion's bag here, its tooltip lists what it wears.
#   Umiejetnosci  the player's skill page: the path's six skills on the skill
#                 board, a column per grade, the points, a "+" where one can go
#                 and the player's own skill tooltip (sidekickskilltip.py).
#                 Under it, where the player's support skills stand: who spends
#                 the points, the chosen skill with "Zeruj" (a right click on a
#                 skill asks at once), and the server's answers.
#   Polecenia     the Emotions tab of the player's window: what the companion
#                 does and where, the orders, the stance, the loot, its bag.
#   Opcje         the Quests tab: guard, buffs, lure, "Gra beze mnie", chests,
#                 party, who spends the stat and skill points, and the one
#                 free reset of its stats.
#
# The tab strip is one picture per pressed tab with the four names painted in
# (locale/<lang>/ui/windows/tab_1..4.sub), and two of them are the player's
# ("Emocje", "Zadania"); the client has no other. Each painted name is covered
# with the stone's own colour and the tab's name written over it, so the strip
# reads Status / Umiejetn. / Polecenia / Opcje, in English in an English
# client, with the stock picture's pressed tab.
#
# The server answers "/towarzysz okno" (and "okno 1" for the whole gear, which
# the window asks for when it opens) with three commands, each far under the
# 512 bytes CHARACTER::ChatPacket formats into:
#
#   SidekickInfo <protocol> 0                       - no companion
#   SidekickInfo <protocol> 2                       - companions switched off
#                                                     in this world (M2_SIDEKICK)
#   SidekickInfo <protocol> 1 <race> <group> <level> <exp%> <hp> <maxhp> <sp>
#                <maxsp> <where> <dist> <mode> <stance> <loot> <protect>
#                <buffs> <gold> <red> <blue> <dead> [<lure> <luring> [<solo> [<chests>
#                [<party>]]]]
#   SidekickNames <name> <place> <doing>            - hex of the CP1250 bytes
#   SidekickGear <slot 0-7> <name>                  - hex, only when changed
#
# where: 0 not in the game, 1 on the owner's map, 2 elsewhere; mode: 0 at the
# owner's side, 1 free, 2 waiting, 3 shopping; stance: 0 attacks everything,
# 1 attacks nobody first, 2 does not fight; loot: 0 nothing, 1 the owner's,
# 2 everything. The orders are the letter's own commands, so the window adds
# nothing the server did not already take from the quest: przywolaj, wolny,
# czekaj, zakupy, stan, walka N, zbieraj N, ochrona N, buffy N, luruj N, sam N,
# skrzynki N, grupa N, odprawa tak. lure (server 2.2.19): the companion wakes packs round
# the owner and brings them over; luring: 0 no course, 1 out to a pack, 2 back
# with them. solo (server 2.2.33, "Gra beze mnie"): with its owner out of the
# game it plays on alone, up to thirty levels over the owner's. chests (server
# 2.2.33, "Skrzynki"): 1 it opens the chests in its bag, 0 it leaves them closed.
# party (server 2.2.38, "Grupa"): 1 it joins its owner's party whoever leads it,
# while a place stays free after it for one more person; 0 a party somebody
# else leads only on that leader's invitation.
#
# The status and skill pages read the answer to "/towarzysz umiejetnosci"
# (SidekickSkillBegin with the stats, the skills, SidekickSkillEnd), which the
# window asks for every SKILL_POLL_INTERVAL; uisidekickinventory.py parses it
# (with the bag's answers, and the answer to every order of the pages,
# SidekickEqResult), and says so to RefreshSkills. The bag is its own window
# (uisidekickinventory.py). Every command of the companion's windows leaves
# through the one queue below, because the server's limit is per character.
#
# Python 2.7 as the client has it; the Polish letters are CP1250 escapes.
# Every text is a Polish and English pair (playerbot_lang.T), English for a
# client set to any language but Polish; the tables below are read once, at
# import. What the server writes into the window - the companion's place,
# what it is doing, its gear - comes in the language the client told it
# (PlayerBotLanguage, playerbot_lang.py); in English it names an item or a
# monster "{i<vnum>}" or "{m<vnum>}", the placeholders of the bots' status
# line, and the window writes the client's own name in (ExpandNames).

import clientclock
import net
import ui
from playerbot_lang import T

PROTOCOL = 1
POLL_INTERVAL = 1.5
# The status and skill pages' numbers come with the skill list.
SKILL_POLL_INTERVAL = 3.0
# The server drops a sixth command in half a second (ENABLE_ANTI_CMD_FLOOD)
# without a word, so a burst of clicks is spaced.
COMMAND_SPACING = 0.3
HEX_DIGITS = '0123456789abcdefABCDEF'
MAX_TEXT_BYTES = 64
STATUS_SECONDS = 8.0

COLOR_NORMAL = 0xffc2c2c2
COLOR_BAD = 0xffe57975
COLOR_HINT = 0xffd8c9a0

JOBS = T(('Wojownik', 'Ninja', 'Sura', 'Szaman'), ('Warrior', 'Ninja', 'Sura', 'Shaman'))
PATHS = T((
	('Cia\xb3o', 'Umys\xb3'),
	('Skrytob\xf3jca', '\xa3ucznik'),
	('Bro\xf1', 'Czarna magia'),
	('Smok', 'Leczenie'),
), (
	('Body', 'Mental'),
	('Assassin', 'Archer'),
	('Weaponry', 'Black magic'),
	('Dragon', 'Healing'),
))
MODES = T(('przy tobie', 'wolna r\xeaka', 'czeka w miejscu', 'robi zakupy'),
	('at your side', 'free hand', 'waiting here', 'shopping'))
STANCES = T(('Atakuj', 'Nie 1. atak', 'Nie walcz'), ('Attack', 'No 1st hit', "Don't fight"))
STANCE_HINTS = T(('bije wszystko w pobli\xbfu', 'nie zaczyna, broni ciebie i siebie', 'nie walczy wcale'),
	('hits everything nearby', 'never first, guards you and itself', 'does not fight at all'))
LOOTS = T(('Nic', 'Tw\xf3j drop', 'Wszystko'), ('Nothing', 'Your drop', 'Everything'))
GEAR_LABELS = T(('Bro\xf1', 'Zbroja', 'He\xb3m', 'Tarcza', 'Buty', 'Bransoleta', 'Naszyjnik', 'Kolczyki'),
	('Weapon', 'Armour', 'Helmet', 'Shield', 'Shoes', 'Bracelet', 'Necklace', 'Earrings'))
YES_NO = T(('nie', 'tak'), ('no', 'yes'))

# ---------------------------------------------------------------- the pages

SCRIPT = 'UIScript/CharacterWindow.py'

PAGE_STATUS = 'STATUS'
PAGE_SKILL = 'SKILL'
PAGE_ORDERS = 'ORDERS'
PAGE_OPTIONS = 'OPTIONS'
PAGES = (PAGE_STATUS, PAGE_SKILL, PAGE_ORDERS, PAGE_OPTIONS)
# The script's tab picture, tab button and title bar of each page, in the
# order of the strip: the Emotions and the Quests are the orders and options.
TAB_IMAGES = ('Tab_01', 'Tab_02', 'Tab_03', 'Tab_04')
TAB_BUTTONS = ('Tab_Button_01', 'Tab_Button_02', 'Tab_Button_03', 'Tab_Button_04')
TITLE_BARS = ('Character_TitleBar', 'Skill_TitleBar', 'Emoticon_TitleBar', 'Quest_TitleBar')
PAGE_TITLES = T(('Towarzysz', 'Umiej\xeatno\x9cci towarzysza', 'Polecenia', 'Opcje towarzysza'),
	('Companion', "Companion's skills", 'Orders', "Companion's options"))
TAB_LABELS = T(('Status', 'Umiej\xeatn.', 'Polecenia', 'Opcje'), ('Status', 'Skills', 'Orders', 'Options'))

# The painted name of a tab, measured on the four pictures: rows 14-26 of the
# strip, inside the tab's frame. A cover of the stone's colour two pixels in
# from the button's left and three from its right, rows 13-27, hides it whole,
# and the tab's own name is written on its middle.
TAB_COVER_LEFT = 2
TAB_COVER_TRIM = 5
TAB_COVER_TOP = 13
TAB_COVER_HEIGHT = 15
TAB_LABEL_Y = 20
TAB_STONE_COLOR = 0xff1c1a18
TAB_TEXT_COLOR = 0xff969696
TAB_TEXT_COLOR_ACTIVE = 0xfff7e9c4

# Where the Emotions and Quests pages stand in the script, which the orders and
# options take: under the title bar, over the tab strip.
PAGE_X = 0
PAGE_Y = 24
PAGE_WIDTH = 250
PAGE_HEIGHT = 304
# The script's section bars on those pages, and their titles' colour on the
# skill page.
SECTION_X = 12
SECTION_WIDTH = 223
SECTION_COLOR = 0xffffe3ad
LINE_X = 18
ROW_COLORS = (0x10ffffff, 0x40000000)

BUTTON_IMAGE = 'd:/ymir work/ui/public/%s_button_%02d.sub'
BUTTON_WIDTHS = {'small': 43, 'middle': 61, 'large': 88, 'xlarge': 180}
TAB_BUTTON_IMAGE = 'd:/ymir work/ui/game/windows/skill_tab_button_%02d.sub'
PLUS_IMAGE = 'd:/ymir work/ui/game/windows/btn_plus_%s.sub'
SLOT_BASE_IMAGE = 'd:/ymir work/ui/public/slot_base.sub'
# Three middle buttons a row, centred on the page.
ORDER_COLUMNS = (23, 94, 165)

# The faces of uicharacter.py, by race (playerSettingModule.RACE_*).
FACE_IMAGES = ('icon/face/warrior_m.tga', 'icon/face/assassin_w.tga', 'icon/face/sura_m.tga',
	'icon/face/shaman_w.tga', 'icon/face/warrior_w.tga', 'icon/face/assassin_m.tga', 'icon/face/sura_w.tga',
	'icon/face/shaman_m.tga')

# The status page: the script's name of a stat, and the server's.
STAT_KEYS = (('HTH', 'ht'), ('INT', 'iq'), ('STR', 'st'), ('DEX', 'dx'))
STATUS_VALUES = ('Level', 'Exp', 'RestExp', 'HP', 'SP', 'STR', 'DEX', 'HTH', 'INT', 'ATT', 'DEF', 'MATT', 'MDEF',
	'ASPD', 'MSPD', 'CSPD', 'ER')
STAT_NAMES = {
	'ht': T('Witalno\x9c\xe6', 'Vitality'),
	'iq': T('Inteligencja', 'Intelligence'),
	'st': T('Si\xb3a', 'Strength'),
	'dx': T('Zr\xeaczno\x9c\xe6', 'Dexterity'),
}
STAT_HINTS = {
	'ht': T('Podnosi P\xaf i obron\xea.', 'Raises HP and defence.'),
	'iq': T('Podnosi PE i magiczny atak.', 'Raises SP and magic attack.'),
	'st': T('Podnosi atak.', 'Raises attack.'),
	'dx': T('Podnosi atak i uniki.', 'Raises attack and evasion.'),
}

# The skill board: the path's skills in slots 1-8, the Master's column twenty
# further on and the Grand Master's forty (SKILL_GRADE_STEP_COUNT).
SKILL_SLOTS = 8
SKILL_GRADE_STEP = 20
SKILL_GRADE_COLUMNS = 3

# The options: the SidekickInfo word, the letter's order, what the switch is
# when the server says nothing, the name and what it does.
SWITCHES = (
	('protect', 'ochrona', 1, T('Ochrona', 'Guard'),
		T('Gdy masz ma\xb3o \xbfycia, bierze na siebie, co ci\xea bije.', 'When you are low on health it takes on what hits you.')),
	('buffs', 'buffy', 1, T('Buffy', 'Buffs'),
		T('Rzuca na ciebie swoje buffy.', 'Casts its buffs on you.')),
	('lure', 'luruj', 0, T('Lurowanie', 'Luring'),
		T('\x8cci\xb9ga do ciebie grupy potwor\xf3w z okolicy.', 'Brings you packs of monsters from around.')),
	('solo', 'sam', 0, T('Gra beze mnie', 'Plays without me'),
		T('Gra dalej, gdy wyjdziesz z gry - do twojego poziomu +30.', 'Plays on when you leave the game - up to your level +30.')),
	('chests', 'skrzynki', 1, T('Skrzynki', 'Chests'),
		T('Sam otwiera skrzynie z plecaka. Wy\xb3\xb9czone: zostawia je tobie.', 'Opens the chests in its bag. Off: it leaves them to you.')),
	('party', 'grupa', 1, T('Grupa', 'Party'),
		T('Do\xb3\xb9cza do twojej grupy, tak\xbfe prowadzonej przez kogo\x9c innego.', 'Joins your party, also one somebody else leads.')),
)
SWITCH_TOP = 28
ROW_STEP = 22
ROW_HEIGHT = 21

TEXT_WAITING = T('Czekam na odpowied\x9f serwera...', 'Waiting for the server...')
TEXT_NO_COMPANION = T(('Nie masz jeszcze towarzysza.', 'Wybierz go w li\x9ccie "Towarzysz".'),
	('You have no companion yet.', 'Choose one in the "Companion" letter.'))
TEXT_SWITCHED_OFF = T(('Towarzysze s\xb9 wy\xb3\xb9czeni na tym serwerze.', 'W\xb3\xb9cza je w\xb3a\x9cciciel serwera w launcherze.'),
	('Companions are switched off on this server.', 'The server owner turns them on in the launcher.'))
TEXT_COMING = T('Za chwil\xea b\xeadzie w grze.', 'In the game in a moment.')
TEXT_PLUS_LABEL = T('|cffbc893adost\xeapne [|r%d|cffbc893a]|r', '|cffbc893aavailable [|r%d|cffbc893a]|r')
TEXT_STAT_PLUS_CTRL = T('Ctrl + klik: kilka punkt\xf3w naraz', 'Ctrl + click: several points at once')
TEXT_STAT_SPENT = T('Rozdane: %d', 'Spent: %d')
TEXT_STAT_GEAR = T('Z ekwipunku: %+d', 'From the gear: %+d')
TEXT_STAT_INPUT = T('%s: ile punkt\xf3w?', '%s: how many points?')
TEXT_DEF_TIP = T('Bazowa obrona: %d (+%d%% wzmocnienia)', 'Base defence: %d (+%d%% boost)')
TEXT_BAG_SHORT = T('Plecak', 'Bag')
TEXT_WEARING = T('Na sobie', 'Wearing')
TEXT_BAG_HINT = T('Klik: plecak towarzysza', "Click: the companion's bag")
TEXT_SKILL_POINTS_BAR = T('Punkty rozdaj\xea sam:', 'I spend the points:')
TEXT_CHOOSE_SKILL = T('Kliknij umiej\xeatno\x9c\xe6 powy\xbfej.', 'Click a skill above.')
TEXT_RIGHT_CLICK_RESET = T('Prawy klik na niej: zeruj.', 'Right click on it: reset.')
TEXT_SKILL_TIP_RESET = T('Prawy klik: zeruj', 'Right click: reset')
TEXT_SKILL_LEVEL = T('Poziom: %s', 'Level: %s')
TEXT_NO_PATH = T('Towarzysz dostanie \x9ccie\xbfk\xea na 5 poziomie.', 'Your companion gets a path at level 5.')
TEXT_AI_SPENDS = T('Punkty rozdaje SI towarzysza.', "Your companion's AI spends the points.")
TEXT_SKILL_RESET = T('Zeruj', 'Reset')
TEXT_SKILL_RESET_ASK = T('Wyzerowa\xe6 %s?', 'Reset %s?')
TEXT_SKILL_RESET_MASTER = T(' Mistrz przepadnie.', ' Its Master grade will be lost.')
TEXT_SKILL_RESET_HOW = T('Towarzysz zu\xbfyje KZ albo Zw\xf3j Powrotu Umiej\xeatno\x9cci.',
	'Your companion will use a Book of Forgetfulness or a Skill Reset Document.')
TEXT_SECTION_DOING = T('Co robi', 'What it does')
TEXT_SECTION_ORDERS = T('Polecenia', 'Orders')
TEXT_SECTION_COMBAT = T('Walka', 'Combat')
TEXT_SECTION_LOOT = T('Drop', 'Drop')
TEXT_SECTION_BEHAVIOUR = T('Zachowanie', 'Behaviour')
TEXT_SECTION_POINTS = T('Punkty', 'Points')
TEXT_DOING = T('Teraz: %s', 'Now: %s')
TEXT_DOWN = T('Teraz: le\xbfy, zaraz wstanie', 'Now: down, getting up soon')
TEXT_WHERE = T('Gdzie: %s', 'Where: %s')
TEXT_POTIONS = T('Mikstury: %d czerw. / %d nieb.', 'Potions: %d red / %d blue')
TEXT_GOLD = 'Yang: %s'
TEXT_MODE = T('Tryb: %s', 'Mode: %s')
ORDERS = (
	(T('Przywo\xb3aj', 'Call'), 'przywolaj'),
	(T('Czekaj tu', 'Wait here'), 'czekaj'),
	(T('Wolna r\xeaka', 'Free hand'), 'wolny'),
	(T('Na zakupy', 'Shopping'), 'zakupy'),
	(T('Raport', 'Report'), 'stan'),
)
TEXT_DISMISS = T('Odpraw', 'Dismiss')
TEXT_DISMISS_ASK = T('Odprawi\xe6 towarzysza na dobre? Tego nie da si\xea cofn\xb9\xe6.',
	'Dismiss your companion for good? This cannot be undone.')
TEXT_INVENTORY = T('Ekwipunek', 'Inventory')
TEXT_STAT_MANUAL = T('Statystyki rozdaj\xea sam', 'I spend the stat points')
TEXT_SKILL_MANUAL = T('Umiej\xeatno\x9cci rozdaj\xea sam', 'I spend the skill points')
TEXT_STAT_MANUAL_HINT = T('Pierwszy "+" na zak\xb3adce Status te\xbf ci je daje.',
	'The first "+" on the Status tab gives them to you too.')
TEXT_SKILL_MANUAL_HINT = T('Pierwszy "+" na zak\xb3adce Umiej\xeatno\x9cci te\xbf ci je daje.',
	'The first "+" on the Skills tab gives them to you too.')
TEXT_STAT_RESET = T('Rozdaj od nowa (raz za darmo)', 'Spend again (once for free)')
TEXT_STAT_RESET_HINT = T('Cofa wszystkie punkty statystyk - raz, za darmo.', 'Takes every stat point back - once, for free.')
TEXT_STAT_RESET_ASK = T('Cofn\xb9\xe6 wszystkie punkty statystyk towarzysza, \xbfeby rozda\xe6 je od nowa? Darmowy reset jest tylko jeden.',
	"Take back all your companion's stat points to spend them again? There is only one free reset.")
TEXT_STAT_OLD_SERVER = T('Zaktualizuj serwer, \xbfeby rozdawa\xe6 statystyki.', 'Update the server to spend stats.')
TEXT_STAT_AI_SPENDS = T('Punkty statystyk rozdaje SI towarzysza.', "Your companion's AI spends the stat points.")


def DecodeText(value, limit=MAX_TEXT_BYTES):
	"""The server's hex of CP1250 bytes; '' for '-', anything malformed or
	longer than limit bytes."""
	if not value or value == '-' or len(value) % 2 or len(value) > limit * 2:
		return ''
	chars = []
	for i in range(0, len(value), 2):
		pair = value[i:i + 2]
		if pair[0] not in HEX_DIGITS or pair[1] not in HEX_DIGITS:
			return ''
		code = int(pair, 16)
		chars.append(chr(code) if code >= 32 and code != 127 else '?')
	return ''.join(chars)


def ExpandNames(text):
	"""Every {i<vnum>} and {m<vnum>} the server wrote replaced by the client's
	own name for it (playerbot_status_tail.expand_names); the text as it came
	when that cannot be done."""
	try:
		import playerbot_status_tail
		return playerbot_status_tail.expand_names(text)
	except Exception:
		return text


def DecodeNamedText(value, limit=MAX_TEXT_BYTES):
	"""DecodeText, with the names the server left to the client written in."""
	return ExpandNames(DecodeText(value, limit))


def ParseInt(value, default=0):
	try:
		return int(value)
	except (TypeError, ValueError):
		return default


# ---------------------------------------------------------------- the queue
#
# One queue for this window and the bag window: an order waits its turn and
# keeps its place, a poll goes only when nothing waits, and nothing leaves
# sooner than COMMAND_SPACING after the last. The windows pump it while shown
# and the keeper while anything waits, so an order given just before a window
# closed still goes.

_queue = {'pending': [], 'next': 0.0}


def _Send(text, now):
	_queue['next'] = now + COMMAND_SPACING
	net.SendChatPacket('/towarzysz ' + text)


def SendCommand(text):
	"""An order: at once when the line is free, else after those before it."""
	now = clientclock.Now()
	if not _queue['pending'] and now >= _queue['next']:
		_Send(text, now)
	else:
		_queue['pending'].append(text)


def PumpCommands():
	"""Sends the next waiting order when its time has come; True when it did."""
	if not _queue['pending']:
		return False
	now = clientclock.Now()
	if now < _queue['next']:
		return False
	_Send(_queue['pending'].pop(0), now)
	return True


def TryPoll(text):
	"""A poll, only when no order waits and the line is free; True when sent."""
	if _queue['pending']:
		return False
	now = clientclock.Now()
	if now < _queue['next']:
		return False
	_Send(text, now)
	return True


def HasPendingCommands():
	return bool(_queue['pending'])


def ResetCommands():
	_queue['pending'] = []
	_queue['next'] = 0.0


def ParseInfo(args):
	"""SidekickInfo's words after the command, as a dict; None when the
	protocol is another one or the line is short. 'has' is False with no
	companion."""
	if len(args) < 2 or ParseInt(args[0], -1) != PROTOCOL:
		return None
	state = ParseInt(args[1])
	if state == 0:
		return {'has': False}
	if state == 2:
		return {'has': False, 'off': True}
	names = ('race', 'group', 'level', 'exp', 'hp', 'maxhp', 'sp', 'maxsp', 'where', 'dist',
			'mode', 'stance', 'loot', 'protect', 'buffs', 'gold', 'red', 'blue', 'dead')
	values = args[2:]
	if len(values) < len(names):
		return None
	info = {'has': True}
	for i, name in enumerate(names):
		info[name] = ParseInt(values[i])
	# The lure came later: an older server sends no such words, and the window
	# then shows no switch for it. The same for "Gra beze mnie" after it.
	if len(values) >= len(names) + 2:
		info['lure'] = ParseInt(values[len(names)])
		info['luring'] = ParseInt(values[len(names) + 1])
	if len(values) >= len(names) + 3:
		info['solo'] = ParseInt(values[len(names) + 2])
	if len(values) >= len(names) + 4:
		info['chests'] = ParseInt(values[len(names) + 3])
	if len(values) >= len(names) + 5:
		info['party'] = ParseInt(values[len(names) + 4])
	return info


def FormatGold(value):
	"""1234567 -> '1.234.567', as the client writes yang."""
	text = str(max(0, value))
	parts = []
	while len(text) > 3:
		parts.insert(0, text[-3:])
		text = text[:-3]
	parts.insert(0, text)
	return '.'.join(parts)


def ClassText(race, group):
	if race < 0:
		return ''
	job = race % 4
	text = JOBS[job]
	if group in (1, 2):
		text += ' (%s)' % PATHS[job][group - 1]
	return text


def PathText(race, group):
	"""What stands where the player's guild does: the path, or the class
	before level five gives one."""
	if race < 0:
		return ''
	job = race % 4
	if group in (1, 2):
		return PATHS[job][group - 1]
	return JOBS[job]


def FaceImage(race):
	if 0 <= race < len(FACE_IMAGES):
		return FACE_IMAGES[race]
	return ''


def PlaceText(info, place):
	where = info.get('where', 0)
	if where == 0:
		return T('poza gr\xb9', 'out of the game')
	if where == 2:
		return T('%s (inna mapa)', '%s (another map)') % (place or T('inna mapa', 'another map'))
	dist = info.get('dist', 0)
	if dist < 1000:
		near = T('obok ciebie', 'next to you')
	else:
		near = T('%d m od ciebie', '%d m from you') % (dist // 100)
	return '%s, %s' % (place, near) if place else near


def YesNo(value):
	return YES_NO[1 if value else 0]


def RangeText(low, high):
	"""'min-max' as the character window writes a range, one number when the
	two are one."""
	if low == high:
		return '%d' % low
	return '%d-%d' % (low, high)


def DefenceText(defence, boost):
	"""The character window's defence: the boost in green after it."""
	if boost > 0:
		return '%d |cffd5eb4b(+%d)|r' % (defence, boost)
	return '%d' % defence


# ---------------------------------------------------------------- texts on lines

def TextWidth(line, text):
	line.SetText(text)
	try:
		return line.GetTextSize()[0]
	except Exception:
		return 0


def FitText(line, text, maxWidth):
	"""Sets text, cut with '...' until the line is no wider than maxWidth;
	returns what is shown."""
	if TextWidth(line, text) <= maxWidth:
		return text
	cut = text
	while cut and TextWidth(line, cut + '...') > maxWidth:
		cut = cut[:-1].rstrip()
	return cut + '...'


def WrapText(lines, text, maxWidth):
	"""Lays text out over the lines word by word, the last one cut with '...'
	when it must be; True when all of it is shown. The server's texts run to
	some eighty characters, two lines of these windows."""
	words = text.split()
	for i, line in enumerate(lines):
		if i == len(lines) - 1:
			rest = ' '.join(words)
			return FitText(line, rest, maxWidth) == rest
		taken = []
		while words and TextWidth(line, ' '.join(taken + words[:1])) <= maxWidth:
			taken.append(words.pop(0))
		if not taken and words:
			# One word wider than the line: cut there, nothing after it.
			FitText(line, ' '.join(words), maxWidth)
			for other in lines[i + 1:]:
				other.SetText('')
			return False
		line.SetText(' '.join(taken))
	return not words


class StatusLines(object):
	"""A page's lines under its content: a server's answer for its seconds, a
	switch's hint while the mouse is over it, and else the page's own word.
	What does not fit is cut, and a server's text cut short goes to the chat
	whole, so nothing it said is lost."""

	def __init__(self, lines, width):
		self.lines = lines
		self.width = width
		self.until = 0.0
		self.answer = ('', COLOR_NORMAL)
		self.idle = ('', COLOR_NORMAL)
		self.hovering = False

	def _Write(self, text, color):
		for line in self.lines:
			line.SetPackedFontColor(color)
		return WrapText(self.lines, text, self.width)

	def Set(self, text, color=COLOR_NORMAL, seconds=STATUS_SECONDS):
		self.answer = (text, color)
		self.until = clientclock.Now() + seconds
		self.hovering = False
		if not self._Write(text, color):
			import chat
			chat.AppendChat(chat.CHAT_TYPE_INFO, text)

	def SetIdle(self, text, color=COLOR_NORMAL):
		self.idle = (text, color)
		if not self.until and not self.hovering:
			self._Write(text, color)

	def Hover(self, text):
		self.hovering = True
		self._Write(text, COLOR_HINT)

	def EndHover(self):
		self.hovering = False
		self._Show()

	def _Show(self):
		self._Write(*(self.answer if self.until else self.idle))

	def Update(self):
		if self.until and clientclock.Now() >= self.until:
			self.until = 0.0
			if not self.hovering:
				self._Show()

	def Clear(self):
		self.until = 0.0
		self.hovering = False
		self._Show()

	def Text(self):
		return ' '.join(line.GetText() for line in self.lines if line.GetText())


class _Call(object):
	"""A method with its arguments bound, for a stock hook that calls with
	none - a button's ShowToolTip. The method is a ui.__mem_func__, which
	holds the window by a weak proxy, so the button keeps no cycle through
	it."""

	def __init__(self, func, *args):
		self.func = func
		self.args = args

	def __call__(self, *ignored):
		return self.func(*self.args)


def _Inv():
	# uisidekickinventory imports this module at its top; importing it back
	# here would make the two a cycle neither could load first.
	import uisidekickinventory
	return uisidekickinventory


def _TitleText(titleBar):
	"""The text line of a title bar of the script: all four are "TitleName",
	so the dictionary keeps only the last - it is the bar's child."""
	for child in getattr(titleBar, 'Children', ()):
		if isinstance(child, ui.TextLine):
			return child
	return None


# ---------------------------------------------------------------- the window

class SidekickWindow(ui.ScriptWindow):
	"""The player's character window with the companion in it."""

	def __init__(self):
		ui.ScriptWindow.__init__(self)
		self.widgets = []
		self.info = None
		self.names = ('', '', '')
		self.gear = [''] * len(GEAR_LABELS)
		self.page = PAGE_STATUS
		self.nextPoll = 0.0
		self.nextSkillPoll = 0.0
		self.question = None
		self.statDialog = None
		self.statDialogKey = ''
		self.resetVnum = 0
		self.chosenSkill = 0
		self.skillRows = []
		self.toolTip = None
		self.skillToolTip = None
		self.faceShown = ''
		self.Build()
		self.SetPage(PAGE_STATUS)

	# -------------------------------------------------------------- building

	def Build(self):
		ui.PythonScriptLoader().LoadScriptFile(self, SCRIPT)
		child = self.GetChild
		self.board = child('board')
		self.tabImages = dict(zip(PAGES, [child(name) for name in TAB_IMAGES]))
		self.tabButtons = dict(zip(PAGES, [child(name) for name in TAB_BUTTONS]))
		self.titleBars = dict(zip(PAGES, [child(name) for name in TITLE_BARS]))
		for page, title in zip(PAGES, PAGE_TITLES):
			bar = self.titleBars[page]
			bar.SetCloseEvent(ui.__mem_func__(self.Close))
			text = _TitleText(bar)
			if text:
				text.SetText(title)
		for page in PAGES:
			self.tabButtons[page].SAFE_SetEvent(self.SetPage, page)
		# The Emotions and the Quests are the player's: their pages stay shut
		# and the orders and options have pages of their own in their place.
		child('Emoticon_Page').Hide()
		child('Quest_Page').Hide()
		self.pages = {
			PAGE_STATUS: child('Character_Page'),
			PAGE_SKILL: child('Skill_Page'),
			PAGE_ORDERS: self._Page(),
			PAGE_OPTIONS: self._Page(),
		}
		self._BuildTabLabels()
		self._BuildStatusPage()
		self._BuildSkillPage()
		self._BuildOrdersPage()
		self._BuildOptionsPage()
		self.noticeLines = []
		for i in range(2):
			line = self._Label(self.board, self.board.GetWidth() // 2, 150 + i * 16, '')
			line.SetHorizontalAlignCenter()
			self.noticeLines.append(line)

	def _Page(self):
		page = ui.Window()
		page.SetParent(self.board)
		page.SetPosition(PAGE_X, PAGE_Y)
		page.SetSize(PAGE_WIDTH, PAGE_HEIGHT)
		# Dragged by, as the script's pages are.
		page.AddFlag('attach')
		page.Hide()
		self.widgets.append(page)
		return page

	def _Label(self, parent, x, y, text):
		line = ui.TextLine()
		line.SetParent(parent)
		line.SetPosition(x, y)
		line.SetText(text)
		line.Show()
		self.widgets.append(line)
		return line

	def _CenteredLabel(self, parent, y):
		line = self._Label(parent, PAGE_WIDTH // 2, y, '')
		line.SetHorizontalAlignCenter()
		return line

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

	def _Section(self, parent, y, text):
		"""A section of a page as the script's pages have one: its bar and the
		title on it."""
		bar = ui.HorizontalBar()
		bar.SetParent(parent)
		bar.Create(SECTION_WIDTH)
		bar.SetPosition(SECTION_X, y)
		bar.Show()
		self.widgets.append(bar)
		title = self._Label(parent, SECTION_X + 6, y + 2, text)
		title.SetPackedFontColor(SECTION_COLOR)
		return bar

	def _Hover(self, button, text):
		"""The page's hint while the mouse is over a button: the stock tooltip is
		a child line over the button, which on these crowded pages the rows
		after it would draw over."""
		button.ShowToolTip = _Call(ui.__mem_func__(self.OnHover), text)
		button.HideToolTip = ui.__mem_func__(self.OnHoverOut)

	def _BuildTabLabels(self):
		strip = self.GetChild('TabControl')
		self.tabLabels = {}
		for page, text in zip(PAGES, TAB_LABELS):
			button = self.tabButtons[page]
			(x, y) = button.GetLocalPosition()
			width = button.GetWidth()
			cover = ui.Bar()
			cover.SetParent(strip)
			cover.AddFlag('not_pick')
			cover.SetPosition(x + TAB_COVER_LEFT, TAB_COVER_TOP)
			cover.SetSize(width - TAB_COVER_TRIM, TAB_COVER_HEIGHT)
			cover.SetColor(TAB_STONE_COLOR)
			cover.Show()
			self.widgets.append(cover)
			label = self._Label(strip, x + width // 2, TAB_LABEL_Y, text)
			label.SetHorizontalAlignCenter()
			label.SetVerticalAlignCenter()
			self.tabLabels[page] = label

	def _BuildStatusPage(self):
		child = self.GetChild
		page = self.pages[PAGE_STATUS]
		# Everything of the page but its title bar, which stays when there is
		# nothing to show under it.
		self.statusContent = [widget for widget in getattr(page, 'Children', ())
			if hasattr(widget, 'Show') and widget is not self.titleBars[PAGE_STATUS]]
		self.faceImage = child('Face_Image')
		face = child('Face_Slot')
		face.SAFE_SetStringEvent('MOUSE_OVER_IN', self.OnOverFace)
		face.SAFE_SetStringEvent('MOUSE_OVER_OUT', self.HideToolTip)
		self.nameValue = child('Character_Name')
		self.pathValue = child('Guild_Name')
		self.values = dict((name, child(name + '_Value')) for name in STATUS_VALUES)
		self.statusPlusLabel = child('Status_Plus_Label')
		self.statPlusButtons = {}
		for name, key in STAT_KEYS:
			plus = child(name + '_Plus')
			plus.SAFE_SetEvent(self.OnStatPlus, key)
			plus.ShowToolTip = _Call(ui.__mem_func__(self.OnOverStatPlus), key)
			plus.HideToolTip = ui.__mem_func__(self.HideToolTip)
			self.statPlusButtons[key] = plus
			# The stock takes a point back with these for a stat reset item;
			# the companion has its one free reset in the options.
			child(name + '_Minus').Hide()
			slot = child(name + '_Slot')
			slot.SAFE_SetStringEvent('MOUSE_OVER_IN', self.OnOverStatValue, key)
			slot.SAFE_SetStringEvent('MOUSE_OVER_OUT', self.HideToolTip)
		defence = child('DEF_Slot')
		defence.SAFE_SetStringEvent('MOUSE_OVER_IN', self.OnOverDefence)
		defence.SAFE_SetStringEvent('MOUSE_OVER_OUT', self.HideToolTip)
		# The player's "Bonusy" beside "Atrybuty" is the companion's bag.
		self.bagButton = child('AttributeListButton')
		self.bagButton.SetText(TEXT_BAG_SHORT)
		self.bagButton.SAFE_SetEvent(self.OnInventory)
		self.bagButton.ShowToolTip = ui.__mem_func__(self.OnOverBag)
		self.bagButton.HideToolTip = ui.__mem_func__(self.HideToolTip)

	def _BuildSkillPage(self):
		import wndMgr
		child = self.GetChild
		page = self.pages[PAGE_SKILL]
		# The path is the companion's for good, and the horse's skills and the
		# support skills the server does not send.
		for name in ('Skill_Group_Button_1', 'Skill_Group_Button_2', 'Skill_ETC_Slot', 'Support_Skill_Point_Label',
				'SprintOnboardingTint'):
			child(name).Hide()
		self.skillPathName = child('Active_Skill_Group_Name')
		self.skillPathName.Show()
		self.skillPointValue = child('Active_Skill_Point_Value')
		slots = child('Skill_Active_Slot')
		slots.SetSlotStyle(wndMgr.SLOT_STYLE_NONE)
		slots.SetSelectItemSlotEvent(ui.__mem_func__(self.OnSelectSkill))
		slots.SetUseSlotEvent(ui.__mem_func__(self.OnSelectSkill))
		slots.SetUnselectItemSlotEvent(ui.__mem_func__(self.OnRightClickSkill))
		slots.SetOverInItemEvent(ui.__mem_func__(self.OnOverInSkill))
		slots.SetOverOutItemEvent(ui.__mem_func__(self.HideSkillToolTip))
		slots.SetPressedSlotButtonEvent(ui.__mem_func__(self.OnSkillPlus))
		slots.AppendSlotButton(PLUS_IMAGE % 'up', PLUS_IMAGE % 'over', PLUS_IMAGE % 'down')
		self.skillSlots = slots
		# The support skills' bar: who spends the points, with a switch of the
		# same picture as the path buttons on the bar above.
		bar = child('Skill_ETC_Title_Bar')
		child('Support_Skill_Group_Name').SetText(TEXT_SKILL_POINTS_BAR)
		manual = ui.Button()
		manual.SetParent(bar)
		manual.SetPosition(SECTION_WIDTH - 43 - 4, 1)
		manual.SetUpVisual(TAB_BUTTON_IMAGE % 1)
		manual.SetOverVisual(TAB_BUTTON_IMAGE % 2)
		manual.SetDownVisual(TAB_BUTTON_IMAGE % 3)
		manual.SetText('')
		manual.SAFE_SetEvent(self.OnSkillManual)
		manual.Show()
		self.widgets.append(manual)
		self.skillManualButton = manual
		# The chosen skill where the support skills' slots stood.
		chosen = ui.SlotWindow()
		chosen.SetParent(page)
		chosen.SetPosition(18, 221)
		chosen.SetSize(32, 32)
		chosen.AppendSlot(0, 0, 0, 32, 32)
		chosen.SetSlotBaseImage(SLOT_BASE_IMAGE, 1.0, 1.0, 1.0, 1.0)
		chosen.Show()
		self.widgets.append(chosen)
		self.chosenSlot = chosen
		self.chosenName = self._Label(page, 58, 222, '')
		self.chosenLevel = self._Label(page, 58, 237, '')
		self.resetButton = self._Btn(page, 'small', PAGE_WIDTH - 12 - BUTTON_WIDTHS['small'], 227, TEXT_SKILL_RESET,
			self.OnResetChosenSkill)
		self.skillStatus = StatusLines([self._CenteredLabel(page, 262), self._CenteredLabel(page, 276)],
			PAGE_WIDTH - 20)

	def _BuildOrdersPage(self):
		page = self.pages[PAGE_ORDERS]
		self._Section(page, 8, TEXT_SECTION_DOING)
		self.doingLine = self._Label(page, LINE_X, 28, '')
		self.placeLine = self._Label(page, LINE_X, 42, '')
		self.potionLine = self._Label(page, LINE_X, 56, '')
		self.goldLine = self._Label(page, LINE_X, 70, '')
		self._Section(page, 88, TEXT_SECTION_ORDERS)
		self.orderButtons = []
		for i, (text, order) in enumerate(ORDERS):
			x = ORDER_COLUMNS[i % 3]
			y = 108 + (i // 3) * 24
			self.orderButtons.append(self._Btn(page, 'middle', x, y, text, self.OnOrder, order))
		(self.summonButton, self.holdButton, self.freeButton) = self.orderButtons[:3]
		self.dismissButton = self._Btn(page, 'middle', ORDER_COLUMNS[2], 132, TEXT_DISMISS, self.OnDismiss)
		self._Section(page, 158, TEXT_SECTION_COMBAT)
		self.stanceButtons = []
		for i, text in enumerate(STANCES):
			self.stanceButtons.append(self._Btn(page, 'middle', ORDER_COLUMNS[i], 178, text, self.OnStance, i))
		self.stanceHint = self._CenteredLabel(page, 202)
		self._Section(page, 219, TEXT_SECTION_LOOT)
		self.lootButtons = []
		for i, text in enumerate(LOOTS):
			self.lootButtons.append(self._Btn(page, 'middle', ORDER_COLUMNS[i], 239, text, self.OnLoot, i))
		self.inventoryButton = self._Btn(page, 'large', (PAGE_WIDTH - BUTTON_WIDTHS['large']) // 2, 266,
			TEXT_INVENTORY, self.OnInventory)
		self.inventoryButton.ShowToolTip = ui.__mem_func__(self.OnOverBag)
		self.inventoryButton.HideToolTip = ui.__mem_func__(self.HideToolTip)
		self.ordersStatus = StatusLines([self._CenteredLabel(page, 291)], PAGE_WIDTH - 20)

	def _Row(self, page, y, index, text):
		bar = ui.Bar()
		bar.SetParent(page)
		bar.SetPosition(SECTION_X, y)
		bar.SetSize(SECTION_WIDTH, ROW_HEIGHT - 1)
		bar.SetColor(ROW_COLORS[index % 2])
		bar.AddFlag('attach')
		bar.Show()
		self.widgets.append(bar)
		label = self._Label(page, LINE_X + 2, y + 3, text)
		return bar, label

	def _Switch(self, page, y, index, text, hint, event, *args):
		"""A row of the options: the name, and a button that says what the
		switch is set to (created last, so its row's bar is under it)."""
		bar, label = self._Row(page, y, index, text)
		button = self._Btn(page, 'small', SECTION_X + SECTION_WIDTH - BUTTON_WIDTHS['small'] - 2, y - 1, '',
			event, *args)
		self._Hover(button, hint)
		return (bar, label, button)

	def _BuildOptionsPage(self):
		page = self.pages[PAGE_OPTIONS]
		self._Section(page, 8, TEXT_SECTION_BEHAVIOUR)
		self.switchRows = {}
		for i, (key, order, default, text, hint) in enumerate(SWITCHES):
			self.switchRows[key] = self._Switch(page, SWITCH_TOP + i * ROW_STEP, i, text, hint, self.OnSwitch, key)
		points = SWITCH_TOP + len(SWITCHES) * ROW_STEP + 4
		self._Section(page, points, TEXT_SECTION_POINTS)
		self.statManualRow = self._Switch(page, points + 20, 0, TEXT_STAT_MANUAL, TEXT_STAT_MANUAL_HINT,
			self.OnStatManual)
		self.skillManualRow = self._Switch(page, points + 20 + ROW_STEP, 1, TEXT_SKILL_MANUAL,
			TEXT_SKILL_MANUAL_HINT, self.OnSkillManual)
		reset = (PAGE_WIDTH - BUTTON_WIDTHS['xlarge']) // 2
		self.statResetButton = self._Btn(page, 'xlarge', reset, points + 20 + 2 * ROW_STEP + 4, TEXT_STAT_RESET,
			self.OnStatReset)
		self._Hover(self.statResetButton, TEXT_STAT_RESET_HINT)
		top = points + 20 + 2 * ROW_STEP + 36
		self.optionsStatus = StatusLines([self._CenteredLabel(page, top), self._CenteredLabel(page, top + 14)],
			PAGE_WIDTH - 20)

	# -------------------------------------------------------------- the server

	def OnServerInfo(self, args):
		info = ParseInfo(args)
		if info is None:
			return
		self.info = info
		self.RefreshAll()

	def OnServerNames(self, name='-', place='-', doing='-'):
		self.names = (DecodeText(name), DecodeText(place), DecodeNamedText(doing))
		self.RefreshAll()

	def OnServerGear(self, slot='0', name='-'):
		index = ParseInt(slot, -1)
		if 0 <= index < len(self.gear):
			self.gear[index] = DecodeNamedText(name)

	# -------------------------------------------------------------- the pages

	def SetPage(self, page):
		if page not in PAGES:
			return
		self.page = page
		self.HideToolTip()
		self.HideSkillToolTip()
		for key in PAGES:
			shown = key == page
			for widget in (self.tabImages[key], self.titleBars[key]):
				if shown:
					widget.Show()
				else:
					widget.Hide()
			if shown:
				self.tabButtons[key].Down()
			else:
				self.tabButtons[key].SetUp()
			self.tabLabels[key].SetPackedFontColor(TAB_TEXT_COLOR_ACTIVE if shown else TAB_TEXT_COLOR)
		self.RefreshAll()

	def HasCompanion(self):
		return bool(self.info) and bool(self.info.get('has'))

	def ShowContent(self, shown):
		"""The page on show, or with no companion to show only its title bar
		and the notice under it."""
		for key in PAGES:
			page = self.pages[key]
			if key == PAGE_STATUS:
				# The status page holds its own title bar.
				if key == self.page:
					page.Show()
				else:
					page.Hide()
				for widget in self.statusContent:
					if shown:
						widget.Show()
					else:
						widget.Hide()
			elif shown and key == self.page:
				page.Show()
			else:
				page.Hide()

	def ShowNotice(self, lines):
		for i, line in enumerate(self.noticeLines):
			line.SetText(lines[i] if i < len(lines) else '')

	def RefreshAll(self):
		info = self.info
		if not self.HasCompanion():
			if info is None:
				self.ShowNotice((TEXT_WAITING,))
			elif info.get('off'):
				self.ShowNotice(TEXT_SWITCHED_OFF)
			else:
				self.ShowNotice(TEXT_NO_COMPANION)
			self.ShowContent(False)
			self.CloseDialogs()
			return
		self.ShowNotice(())
		self.ShowContent(True)
		self.RefreshStatusPage()
		self.RefreshSkillPage()
		self.RefreshOrdersPage()
		self.RefreshOptionsPage()

	def SkillAnswer(self):
		"""The last skill list and the numbers with it, or None."""
		inv = _Inv()
		model = inv.GetSkillModel()
		return model if model.state == inv.STATE_OK else None

	def RefreshStatusPage(self):
		import sidekickskilltip
		info = self.info
		values = self.values
		name = self.names[0]
		race = info['race']
		self.nameValue.SetText(name or '-')
		self.pathValue.SetText(PathText(race, info['group']))
		face = FaceImage(race)
		if face:
			# Loaded when it changes, not at every snapshot.
			if face != self.faceShown:
				self.faceImage.LoadImage(face)
				self.faceShown = face
			self.faceImage.Show()
		else:
			self.faceImage.Hide()
		inWorld = race >= 0
		values['Level'].SetText('%d' % info['level'] if inWorld else '-')
		model = self.SkillAnswer()
		stats = model.stats if model else None
		statInfo = model.statInfo if model else None
		extra = model.status if model else None
		if extra and inWorld:
			values['Exp'].SetText('%d' % extra['exp'])
			values['RestExp'].SetText('%d' % max(0, extra['nextexp'] - extra['exp']))
		else:
			values['Exp'].SetText('%d%%' % info['exp'] if inWorld else '-')
			values['RestExp'].SetText('')
		for key, maximum in (('HP', 'maxhp'), ('SP', 'maxsp')):
			current = key.lower()
			values[key].SetText('%d/%d' % (max(0, info[current]), info[maximum]) if inWorld else '-')
		for scriptName, key in STAT_KEYS:
			values[scriptName].SetText('%d' % stats[key] if stats else '-')
			if statInfo and _Inv().CanAddStatPoint(statInfo, key):
				self.statPlusButtons[key].Show()
			else:
				self.statPlusButtons[key].Hide()
		if statInfo and statInfo['points'] > 0:
			self.statusPlusLabel.SetText(TEXT_PLUS_LABEL % statInfo['points'])
			self.statusPlusLabel.Show()
		else:
			self.statusPlusLabel.Hide()
		if stats:
			numbers = sidekickskilltip.CharacterNumbers(stats, model.job,
				sidekickskilltip.WeaponValues(stats.get('weapon', 0)),
				extra['attbonus'] if extra else 0, extra['defbonus'] if extra else 0)
			values['ATT'].SetText(RangeText(numbers['minatk'], numbers['maxatk']))
			values['DEF'].SetText(DefenceText(numbers['def'], numbers['defboost']))
			values['MATT'].SetText(RangeText(numbers['minmatk'], numbers['maxmatk']))
			values['MDEF'].SetText('%d' % numbers['mdef'])
			values['ASPD'].SetText('%d' % stats['atkspd'])
			values['MSPD'].SetText('%d' % extra['movspeed'] if extra else '-')
			values['CSPD'].SetText('%d' % stats['castingspeed'])
			values['ER'].SetText('%d' % numbers['evade'])
		else:
			for key in ('ATT', 'DEF', 'MATT', 'MDEF', 'ASPD', 'MSPD', 'CSPD', 'ER'):
				values[key].SetText('-')

	def RefreshSkillPage(self):
		inv = _Inv()
		model = self.SkillAnswer()
		info = self.info
		self.skillRows = model.skills[:SKILL_SLOTS] if model else []
		job = model.job if model else info['race'] % 4 if info['race'] >= 0 else 0
		if not 0 <= job < len(JOBS):
			job = 0
		group = model.group if model else info['group']
		self.skillPathName.SetText(PATHS[job][group - 1] if group in (1, 2) else JOBS[job])
		self.skillPointValue.SetText('%d' % model.points if model else '-')
		slots = self.skillSlots
		slots.HideAllSlotButton()
		for i in range(SKILL_SLOTS):
			for grade in range(SKILL_GRADE_COLUMNS):
				slots.ClearSlot(i + 1 + grade * SKILL_GRADE_STEP)
			if i >= len(self.skillRows):
				continue
			vnum, level, grade = self.skillRows[i]
			shown, step = inv.SkillGradeStep(level, grade)
			# The player's page: the skill in each grade's column, the grade it
			# stands at counted and live, the others dimmed.
			for column in range(SKILL_GRADE_COLUMNS):
				slot = i + 1 + column * SKILL_GRADE_STEP
				slots.SetSkillSlotNew(slot, vnum, column, step)
				slots.SetCoverButton(slot)
				if shown == 3 and column == SKILL_GRADE_COLUMNS - 1:
					slots.SetSlotCountNew(slot, shown, step)
				elif shown != column:
					slots.SetSlotCount(slot, 0)
					slots.DisableCoverButton(slot)
				else:
					slots.SetSlotCountNew(slot, shown, step)
			if model and inv.CanAddSkillPoint(model.points, level, grade):
				slots.ShowSlotButton(i + 1)
		slots.RefreshSlot()
		if model:
			self.skillManualButton.SetText(YesNo(model.manual))
			self.skillManualButton.Show()
		else:
			self.skillManualButton.Hide()
		self.RefreshChosenSkill()
		if not model:
			(text, color) = inv.IdleText(inv.GetSkillModel())
			self.skillStatus.SetIdle(text or TEXT_WAITING, color)
		elif not self.skillRows:
			self.skillStatus.SetIdle(TEXT_NO_PATH)
		else:
			self.skillStatus.SetIdle('' if model.manual else TEXT_AI_SPENDS)

	def ChosenRow(self):
		for row in self.skillRows:
			if row[0] == self.chosenSkill:
				return row
		return None

	def RefreshChosenSkill(self):
		inv = _Inv()
		row = self.ChosenRow()
		if row is None:
			self.chosenSkill = 0
			self.chosenSlot.ClearSlot(0)
			self.chosenName.SetText(TEXT_CHOOSE_SKILL if self.skillRows else '')
			self.chosenLevel.SetText(TEXT_RIGHT_CLICK_RESET if self.skillRows else '')
			self.resetButton.Hide()
		else:
			vnum, level, grade = row
			shown, step = inv.SkillGradeStep(level, grade)
			self.chosenSlot.SetSkillSlotNew(0, vnum, min(shown, SKILL_GRADE_COLUMNS - 1), step)
			self.chosenSlot.SetSlotCountNew(0, shown, step)
			self.chosenName.SetText(inv.SkillName(vnum, shown))
			self.chosenLevel.SetText(TEXT_SKILL_LEVEL % inv.SkillLevelText(level, grade))
			if level > 0:
				self.resetButton.Show()
			else:
				self.resetButton.Hide()
		self.chosenSlot.RefreshSlot()

	def RefreshOrdersPage(self):
		info = self.info
		name, place, doing = self.names
		mode = info['mode'] if 0 <= info['mode'] < len(MODES) else 0
		if info['race'] < 0:
			self.doingLine.SetText(TEXT_DOING % (doing or TEXT_COMING))
		elif info['dead']:
			self.doingLine.SetText(TEXT_DOWN)
		else:
			self.doingLine.SetText(TEXT_DOING % (doing or MODES[mode]))
		self.placeLine.SetText(TEXT_WHERE % PlaceText(info, place))
		self.potionLine.SetText(TEXT_POTIONS % (info['red'], info['blue']))
		self.goldLine.SetText(TEXT_GOLD % FormatGold(info['gold']))
		# The summon, the free hand and the wait are the three states the
		# companion is in outside an errand: the one it is in stays down.
		self.SetPressed((self.summonButton, self.freeButton, self.holdButton), mode if mode < 3 else -1)
		self.SetPressed(self.stanceButtons, info['stance'])
		self.SetPressed(self.lootButtons, info['loot'])
		stance = info['stance'] if 0 <= info['stance'] < len(STANCE_HINTS) else 0
		self.stanceHint.SetText(STANCE_HINTS[stance])
		self.ordersStatus.SetIdle(TEXT_MODE % MODES[mode])

	def RefreshOptionsPage(self):
		info = self.info
		for key, order, default, text, hint in SWITCHES:
			bar, label, button = self.switchRows[key]
			# An older server sends no word for a later switch, and has no
			# such switch to set.
			shown = key in info
			for widget in (bar, label, button):
				if shown:
					widget.Show()
				else:
					widget.Hide()
			button.SetText(YesNo(info.get(key, default)))
		model = self.SkillAnswer()
		statInfo = model.statInfo if model else None
		for row, value in ((self.statManualRow, statInfo['manual'] if statInfo else None),
				(self.skillManualRow, model.manual if model else None)):
			for widget in row:
				if value is None:
					widget.Hide()
				else:
					widget.Show()
			row[2].SetText(YesNo(value))
		if statInfo and statInfo['reset']:
			self.statResetButton.Show()
		else:
			self.statResetButton.Hide()
		if model and statInfo is None:
			self.optionsStatus.SetIdle(TEXT_STAT_OLD_SERVER, COLOR_BAD)
		elif statInfo and not statInfo['manual']:
			self.optionsStatus.SetIdle(TEXT_STAT_AI_SPENDS)
		else:
			self.optionsStatus.SetIdle('')

	def SetPressed(self, buttons, index):
		# A button held down says which one is set, the way the game's own
		# radio groups do it.
		for i, button in enumerate(buttons):
			if i == index:
				button.Down()
			else:
				button.SetUp()

	def ShowResult(self, origin, text, color):
		"""A server's answer to an order of a page, on that page when it is the
		one on show and has lines for it; False when it has not."""
		inv = _Inv()
		lines = {PAGE_SKILL: self.skillStatus, PAGE_OPTIONS: self.optionsStatus}.get(self.page)
		if lines is None or not self.IsShow() or not self.HasCompanion():
			return False
		if origin is not None and {inv.ORIGIN_SKILL: PAGE_SKILL, inv.ORIGIN_OPTIONS: PAGE_OPTIONS}.get(origin) != self.page:
			return False
		lines.Set(text, color)
		return True

	# -------------------------------------------------------------- tooltips

	def ShowToolTipLines(self, lines):
		import uiToolTip
		if self.toolTip is None:
			self.toolTip = uiToolTip.ToolTip()
		toolTip = self.toolTip
		toolTip.ClearToolTip()
		for i, text in enumerate(lines):
			if i == 0:
				toolTip.AppendTextLine(text, getattr(toolTip, 'TITLE_COLOR', COLOR_HINT))
			else:
				toolTip.AppendTextLine(text, getattr(toolTip, 'NORMAL_COLOR', COLOR_NORMAL))
		toolTip.ShowToolTip()

	def HideToolTip(self):
		if self.toolTip:
			self.toolTip.HideToolTip()

	def HideSkillToolTip(self):
		if self.skillToolTip:
			self.skillToolTip.HideToolTip()

	def OnOverFace(self):
		if self.HasCompanion() and self.info['race'] >= 0:
			self.ShowToolTipLines((self.names[0] or PAGE_TITLES[0], ClassText(self.info['race'], self.info['group'])))

	def OnOverStatPlus(self, key):
		self.ShowToolTipLines((STAT_NAMES[key], STAT_HINTS[key], TEXT_STAT_PLUS_CTRL))

	def OnOverStatValue(self, key):
		model = self.SkillAnswer()
		lines = [STAT_NAMES[key]]
		if model and model.stats and model.statInfo:
			spent = model.statInfo[key]
			lines.append(TEXT_STAT_SPENT % spent)
			lines.append(TEXT_STAT_GEAR % (model.stats[key] - spent))
		self.ShowToolTipLines(lines)

	def OnOverDefence(self):
		model = self.SkillAnswer()
		if model and model.stats:
			boost = model.status['defbonus'] if model.status else 0
			self.ShowToolTipLines((TEXT_DEF_TIP % (model.stats.get('def', 0), boost),))

	def OnOverBag(self):
		lines = [TEXT_WEARING]
		for label, name in zip(GEAR_LABELS, self.gear):
			lines.append('%s: %s' % (label, name or '-'))
		lines.append(TEXT_BAG_HINT)
		self.ShowToolTipLines(lines)

	def OnHover(self, text):
		lines = {PAGE_OPTIONS: self.optionsStatus}.get(self.page)
		if lines:
			lines.Hover(text)

	def OnHoverOut(self):
		self.optionsStatus.EndHover()

	def OnOverInSkill(self, slotNumber):
		"""The player's own skill tooltip, with the companion's skill and
		numbers in it (sidekickskilltip.py); over a grade the skill is not at,
		its name at that grade, as the player's page shows it."""
		import sidekickskilltip
		import uiToolTip
		inv = _Inv()
		index = slotNumber % SKILL_GRADE_STEP - 1
		if not 0 <= index < len(self.skillRows) or inv.IsCursorBusy():
			return
		model = self.SkillAnswer()
		if self.skillToolTip is None:
			self.skillToolTip = uiToolTip.SkillToolTip()
		toolTip = self.skillToolTip
		vnum, level, grade = self.skillRows[index]
		shown, step = inv.SkillGradeStep(level, grade)
		column = slotNumber // SKILL_GRADE_STEP
		stats = model.stats if model else None
		job = model.job if model else 0
		try:
			if column == shown or (shown == 3 and column == SKILL_GRADE_COLUMNS - 1):
				sidekickskilltip.Show(toolTip, vnum, level, shown, step, stats, job)
			else:
				sidekickskilltip.ShowName(toolTip, vnum, column, stats, job)
			if level > 0:
				toolTip.AppendSpace(5)
				toolTip.AppendTextLine(TEXT_SKILL_TIP_RESET, COLOR_HINT)
		except Exception:
			# A tooltip that cannot be drawn is no reason to lose the window.
			toolTip.HideToolTip()

	# -------------------------------------------------------------- orders

	def SendCommand(self, text):
		SendCommand(text)

	def OnInventory(self):
		_Inv().ToggleEquipmentWindow(self)

	def OnOrder(self, order):
		self.SendCommand(order)
		# The answer comes back at the next look; asked for at once, the
		# window shows the new mode without waiting for the poll.
		self.nextPoll = 0.0

	def OnStance(self, stance):
		self.SendCommand('walka %d' % stance)
		self.nextPoll = 0.0

	def OnLoot(self, loot):
		self.SendCommand('zbieraj %d' % loot)
		self.nextPoll = 0.0

	def OnSwitch(self, key):
		for name, order, default, text, hint in SWITCHES:
			if name == key:
				value = self.info.get(key, default) if self.info else default
				self.SendCommand('%s %d' % (order, 0 if value else 1))
				self.nextPoll = 0.0
				return

	def OnStatManual(self):
		inv = _Inv()
		model = self.SkillAnswer()
		manual = model.statInfo['manual'] if model and model.statInfo else 0
		inv.SendOrder(inv.ORIGIN_OPTIONS, 'statystyki reczne %d' % (0 if manual else 1))

	def OnSkillManual(self):
		inv = _Inv()
		model = self.SkillAnswer()
		manual = model.manual if model else 0
		origin = inv.ORIGIN_OPTIONS if self.page == PAGE_OPTIONS else inv.ORIGIN_SKILL
		inv.SendOrder(origin, 'umiejetnosci reczne %d' % (0 if manual else 1))

	def OnStatPlus(self, key):
		inv = _Inv()
		# Ctrl + click: several at once, as the player's own "+" does.
		if inv.IsCtrlPressed():
			self.OpenStatDialog(key)
			return
		inv.SendOrder(inv.ORIGIN_STAT, 'statystyki dodaj %s 1' % key)

	def OpenStatDialog(self, key):
		import uiCommon
		self.CloseStatDialog()
		dialog = uiCommon.InputDialog()
		dialog.SetTitle(TEXT_STAT_INPUT % STAT_NAMES[key])
		dialog.SetNumberMode()
		dialog.SetMaxLength(2)
		dialog.SetAcceptEvent(ui.__mem_func__(self.OnStatDialogAccept))
		dialog.SetCancelEvent(ui.__mem_func__(self.CloseStatDialog))
		dialog.Open()
		self.statDialog = dialog
		self.statDialogKey = key

	def OnStatDialogAccept(self):
		inv = _Inv()
		dialog = self.statDialog
		key = self.statDialogKey
		count = ParseInt(dialog.GetText(), 0) if dialog else 0
		self.CloseStatDialog()
		if key and count > 0:
			inv.SendOrder(inv.ORIGIN_STAT, 'statystyki dodaj %s %d' % (key, count))

	def CloseStatDialog(self):
		dialog = self.statDialog
		self.statDialog = None
		self.statDialogKey = ''
		if dialog:
			dialog.Close()

	def OnStatReset(self):
		import uiCommon
		self.CloseQuestion()
		question = uiCommon.QuestionDialog()
		question.SetText(TEXT_STAT_RESET_ASK)
		question.SetAcceptEvent(ui.__mem_func__(self.OnStatResetAccept))
		question.SetCancelEvent(ui.__mem_func__(self.CloseQuestion))
		question.Open()
		self.question = question

	def OnStatResetAccept(self):
		inv = _Inv()
		inv.SendOrder(inv.ORIGIN_OPTIONS, 'statystyki odnow')
		self.CloseQuestion()

	def _SkillRowAt(self, slotNumber):
		index = slotNumber % SKILL_GRADE_STEP - 1
		return self.skillRows[index] if 0 <= index < len(self.skillRows) else None

	def OnSelectSkill(self, slotNumber):
		row = self._SkillRowAt(slotNumber)
		if row:
			self.chosenSkill = row[0]
			self.RefreshChosenSkill()

	def OnRightClickSkill(self, slotNumber):
		row = self._SkillRowAt(slotNumber)
		if not row:
			return
		self.chosenSkill = row[0]
		self.RefreshChosenSkill()
		if row[1] > 0:
			self.AskResetSkill(row[0])

	def OnSkillPlus(self, slotNumber):
		inv = _Inv()
		row = self._SkillRowAt(slotNumber)
		if row:
			inv.SendOrder(inv.ORIGIN_SKILL, 'umiejetnosci dodaj %d' % row[0])

	def OnResetChosenSkill(self):
		if self.ChosenRow():
			self.AskResetSkill(self.chosenSkill)

	def AskResetSkill(self, vnum):
		"""Asked first: the companion takes the skill back to nothing with the
		Forgetting Books or the skill reset scroll in its own bag (blasty, 28
		September)."""
		import uiCommon
		inv = _Inv()
		self.CloseQuestion()
		for known, level, grade in self.skillRows:
			if known != vnum:
				continue
			shown, _ = inv.SkillGradeStep(level, grade)
			question = uiCommon.QuestionDialog2()
			question.SetText1(TEXT_SKILL_RESET_ASK % inv.SkillName(vnum, shown) +
				(TEXT_SKILL_RESET_MASTER if shown > 0 else ''))
			question.SetText2(TEXT_SKILL_RESET_HOW)
			question.SetAcceptEvent(ui.__mem_func__(self.OnResetAccept))
			question.SetCancelEvent(ui.__mem_func__(self.CloseQuestion))
			question.Open()
			self.question = question
			self.resetVnum = vnum
			return

	def OnResetAccept(self):
		inv = _Inv()
		if self.resetVnum:
			inv.SendOrder(inv.ORIGIN_SKILL, 'umiejetnosci zeruj %d' % self.resetVnum)
		self.CloseQuestion()

	def OnDismiss(self):
		import uiCommon
		self.CloseQuestion()
		question = uiCommon.QuestionDialog()
		question.SetText(TEXT_DISMISS_ASK)
		question.SetAcceptEvent(ui.__mem_func__(self.OnDismissAccept))
		question.SetCancelEvent(ui.__mem_func__(self.OnDismissCancel))
		question.Open()
		self.question = question

	def OnDismissAccept(self):
		self.SendCommand('odprawa tak')
		self.OnDismissCancel()
		self.nextPoll = 0.0

	def OnDismissCancel(self):
		self.CloseQuestion()

	def CloseQuestion(self):
		question = self.question
		self.question = None
		self.resetVnum = 0
		if question:
			question.Close()

	def CloseDialogs(self):
		self.CloseQuestion()
		self.CloseStatDialog()

	# -------------------------------------------------------------- the clock

	def OnUpdate(self):
		for lines in (self.skillStatus, self.optionsStatus, self.ordersStatus):
			lines.Update()
		if PumpCommands():
			return
		now = clientclock.Now()
		if now >= self.nextPoll and TryPoll('okno'):
			self.nextPoll = now + POLL_INTERVAL
			return
		if now >= self.nextSkillPoll and TryPoll('umiejetnosci'):
			self.nextSkillPoll = now + SKILL_POLL_INTERVAL

	def Open(self):
		self.Show()
		self.SetTop()
		now = clientclock.Now()
		self.nextPoll = now + POLL_INTERVAL
		self.nextSkillPoll = now + SKILL_POLL_INTERVAL
		SendCommand('okno 1')
		SendCommand('umiejetnosci')

	def Close(self):
		# The orders already given stay in the queue: the keeper sends them.
		self.CloseDialogs()
		self.HideToolTip()
		self.HideSkillToolTip()
		self.Hide()

	def OnPressEscapeKey(self):
		self.Close()
		return True

	def Destroy(self):
		self.Close()
		self.toolTip = None
		self.skillToolTip = None
		self.ClearDictionary()
		self.widgets = []


_window = {'window': None}


def GetWindow():
	if _window['window'] is None:
		window = SidekickWindow()
		# Not over the player's own character window, which the script puts
		# at the screen's left edge.
		window.SetCenterPosition()
		_window['window'] = window
	return _window['window']


def ToggleWindow():
	# P shuts whatever of the companion's is open - its bag stayed on the
	# screen after it (prodnathin, 28 September) - and opens its window only
	# when nothing of it is open.
	inv = _Inv()
	window = GetWindow()
	if window.IsShow() or inv.AnyShown():
		if window.IsShow():
			window.Close()
		inv.CloseAll()
	else:
		window.Open()


def OpenWindow():
	window = GetWindow()
	if not window.IsShow():
		window.Open()


def OnServerInfo(*args):
	# Answers that come while the window is closed are kept all the same, so
	# it opens on what the server last said.
	GetWindow().OnServerInfo(args)


def OnServerNames(name='-', place='-', doing='-', *rest):
	GetWindow().OnServerNames(name, place, doing)


def OnServerGear(slot='0', name='-', *rest):
	GetWindow().OnServerGear(slot, name)


def RefreshSkills():
	"""The skill list and the stats changed (uisidekickinventory.py)."""
	window = _window['window']
	if window is not None:
		window.RefreshAll()


def ShowResult(origin, text, color):
	"""A server's answer to an order of the window's pages, shown on the page
	that gave it (any page with lines for None); False when none can."""
	window = _window['window']
	return window is not None and window.ShowResult(origin, text, color)


def Destroy():
	window = _window['window']
	if window is not None:
		window.Destroy()
	_window['window'] = None
	# The bag window was opened from this one; it goes with it even where its
	# own keeper never got registered.
	import sys
	if 'uisidekickinventory' in sys.modules:
		sys.modules['uisidekickinventory'].Destroy()
	ResetCommands()


class Keeper(object):
	"""One of the game's updateables: the game window's Close destroys every
	updateable, and the companion's window goes with it rather than stand over
	the character select. The window updates itself (its own OnUpdate while
	shown); the keeper sends what is still queued when none is."""

	def CanUpdate(self):
		return HasPendingCommands()

	def OnUpdate(self):
		PumpCommands()

	def Destroy(self):
		Destroy()


def GetKeeper():
	return Keeper()
