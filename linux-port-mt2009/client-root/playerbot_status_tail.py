# A bot's status over its head, and not a line in the chat history.
#
# The server (SendPlayerBotOverheadChat in playerbot_status.h) sends a bot's
# status as the command "PlayerBotStatus <vid> <hex>" rather than as talking:
# the client puts every talking packet from a character into the chat history
# beside its text tail (RecvChatPacket), so a town full of bots filled the chat
# window. The text travels as hex because the command parser splits its line on
# spaces; the bytes are the status's own CP1250, shown as they came. The native
# tail replaces the bubble of the same VID and ignores a VID nobody can see.
#
# Written for the client's Python 2.7 without a module it might lack, and
# checked on Python 3 as well (tests/playerbot_status_tail_test.py).

MAX_STATUS_BYTES = 159
HEX_DIGITS = "0123456789abcdefABCDEF"


def decode_status(vid_arg, hex_arg):
	try:
		vid = int(vid_arg)
	except (ValueError, TypeError):
		return None
	if vid <= 0 or vid > 0xffffffff:
		return None
	if not hex_arg or len(hex_arg) > MAX_STATUS_BYTES * 2 or len(hex_arg) % 2:
		return None
	chars = []
	for i in range(0, len(hex_arg), 2):
		pair = hex_arg[i:i + 2]
		if pair[0] not in HEX_DIGITS or pair[1] not in HEX_DIGITS:
			return None
		value = int(pair, 16)
		if value < 32 or value == 127:
			return None
		chars.append(chr(value))
	# RegisterChatTail reads the VID as a signed int and keeps it as a DWORD.
	if vid >= 0x80000000:
		vid -= 0x100000000
	return vid, "".join(chars)


# Since server 2.2.6 the command may carry the same status in English as a
# third word: "PlayerBotStatus <vid> <hex> <hex_en>". A client whose language is
# not Polish draws the English one - English being what a Romanian or a German
# reads before Polish - and a monster's or an item's name in it arrives as
# "{m<vnum>}" or "{i<vnum>}", filled in here from the client's own tables, in
# the client's own language. Anything that does not decode falls back to the
# Polish line, so an older server and a broken word both still show a status.
PLACEHOLDER_OPEN = "{"
PLACEHOLDER_CLOSE = "}"
PLACEHOLDER_MAX = 8


def client_language():
	try:
		import systemSetting
		return systemSetting.GetLanguage()
	except Exception:
		return "pl"


def monster_name(vnum):
	try:
		import nonplayer
		name = nonplayer.GetMonsterName(vnum)
		if name:
			return name
	except Exception:
		pass
	return "?"


def item_name(vnum):
	# item.GetItemName names the selected item and ignores an argument, so
	# the item is selected first, the way every tooltip does it (an unknown
	# vnum selects 60001 and says so in syserr.txt; the server sends none).
	try:
		import item
		item.SelectItem(vnum)
		name = item.GetItemName()
		if name:
			return name
	except Exception:
		pass
	return "?"


def expand_names(text):
	"""Every {m<vnum>} and {i<vnum>} replaced by the client's own name for it."""
	out = []
	i = 0
	while i < len(text):
		start = text.find(PLACEHOLDER_OPEN, i)
		if start < 0:
			out.append(text[i:])
			break
		end = text.find(PLACEHOLDER_CLOSE, start)
		out.append(text[i:start])
		if end < 0:
			out.append(text[start:])
			break
		token = text[start + 1:end]
		kind, digits = token[:1], token[1:]
		if kind in ("m", "i") and digits.isdigit() and len(digits) <= PLACEHOLDER_MAX:
			vnum = int(digits)
			out.append(monster_name(vnum) if kind == "m" else item_name(vnum))
		else:
			out.append(text[start:end + 1])
		i = end + 1
	return "".join(out)


def choose_status(vid_arg, hex_arg, hex_en=None, language=None):
	"""(vid, text) to draw, or None: the English line for a non-Polish client."""
	status = decode_status(vid_arg, hex_arg)
	if status is None:
		return None
	if hex_en is None:
		return status
	if language is None:
		language = client_language()
	if language == "pl":
		return status
	english = decode_status(vid_arg, hex_en)
	if english is None:
		return status
	return english[0], expand_names(english[1])


def show(vid_arg, hex_arg, *rest):
	status = choose_status(vid_arg, hex_arg, rest[0] if rest else None)
	if status is None:
		return
	import textTail
	textTail.RegisterChatTail(status[0], status[1])


# A bot's personality, on its own row between its nickname and its guild name
# (2026-09-16: textTail.AttachPersonality, a new CPythonTextTail row added
# specifically for this - see UserInterface/PythonTextTail.cpp/.h. Before that
# this used AttachTitle and sat where the alignment title (ranga) stands,
# fighting the client over that one spot; the two are independent now, so the
# classic alignment title is never touched and always shows.
#
# The server (ManagePlayerBotPersonalityTitle in playerbot_status.h) sends
# "PlayerBotTitle <vid> <personality>" while a player is near, about every ten
# seconds. AttachPersonality is safe to call every second - unlike AttachTitle
# it isn't fighting anything for its slot - so the keeper, one of game.py's
# updateables, refreshes every bot heard from in the last minute anyway, simply
# to expire ones nobody has heard from in a while. The names are the classic
# panel's, in CP1250 like every name the client draws.

PERSONALITY_TITLES = {
	0: "Wytrwa\xb3y poszukiwacz",
	1: "Pogromca Metin\xf3w",
	2: "Towarzysz dru\xbfyny",
	3: "Mistrz ekwipunku",
	4: "Rozwa\xbfny zbieracz",
	5: "Handlarz",
	6: "W\xeadrowiec",
	7: "Dropek Metin\xf3w",
	8: "Dropek z M3",
	9: "Dropek z M2",
	10: "Dropek medali",
	# Iwakura's personalities (playerbot_persona_rules.h, PERSONA_TITLE_BASE +
	# EPersona): under the server's PERSONA switch the title is the one that
	# claims the bot now, and it changes with what the bot is doing.
	100: "Grinder",
	101: "Zdobywca",
	102: "Handlarz",
	103: "Hazardzista",
	104: "Perfekcjonista",
	105: "Pogromca Metin\xf3w",
	106: "G\xf3rnik",
	107: "Rybak",
	108: "Najemnik",
	109: "Towarzysz",
	# Iwakura's Patch 3, point 7: the rare personalities, in red.
	110: "Metinolog",
	111: "Na\xb3ogowiec",
	112: "Szalony Naukowiec",
	113: "Egzekutor",
	114: "Szalony W\xeadkarz",
	# Community Patch 5, point 1: the four gamblers that replaced the
	# Hazardzista, rare as well and in purple.
	115: "M\xb3odszy Hazardzista",
	116: "Starszy Hazardzista",
	117: "Naczelny Hazardzista",
	118: "Szalony Hazardzista",
	# MT2009_PLUS_BOTLIFE_V1: Baek-Go's herbalist, the Gornik's sibling
	# (playerbot_persona_rules.h PERSONA_ZIELARZ).
	119: "Zielarz",
}

# Iwakura's names are Polish, and the English client shows English ones.
import systemSetting
if systemSetting.GetLanguage() == "en":
	PERSONALITY_TITLES.update({0: 'Persistent Explorer', 1: 'Metin Slayer', 2: 'Team Player', 3: 'Equipment Master', 4: 'Careful Gatherer', 5: 'Trader', 6: 'Wanderer', 7: 'Metin Farmer', 8: 'M3 Farmer', 9: 'M2 Farmer', 10: 'Medal Farmer', 100: 'Grinder', 101: 'Conqueror', 102: 'Trader', 103: 'Gambler', 104: 'Perfectionist', 105: 'Metin Slayer', 106: 'Miner', 107: 'Fisher', 108: 'Mercenary', 109: 'Companion', 110: 'Metinologist', 111: 'Addict', 112: 'Mad Scientist', 113: 'Executioner', 114: 'Mad Angler', 115: 'Junior Gambler', 116: 'Senior Gambler', 117: 'Chief Gambler', 118: 'Mad Gambler', 119: 'Herbalist'})

PERSONALITY_COLOURS = {
	0: (0.75, 0.85, 1.0),
	1: (1.0, 0.65, 0.3),
	2: (0.45, 0.9, 0.95),
	3: (1.0, 0.85, 0.35),
	4: (0.7, 0.95, 0.6),
	5: (1.0, 0.95, 0.5),
	6: (0.85, 0.75, 1.0),
	7: (1.0, 0.6, 0.6),
	8: (0.95, 0.7, 0.95),
	9: (0.8, 0.85, 0.65),
	10: (0.95, 0.8, 0.55),
	100: (0.8, 0.8, 0.8),
	101: (0.55, 0.95, 0.55),
	102: (1.0, 0.95, 0.5),
	103: (1.0, 0.55, 0.85),
	104: (1.0, 0.85, 0.35),
	105: (1.0, 0.65, 0.3),
	106: (0.75, 0.65, 0.5),
	107: (0.45, 0.8, 1.0),
	108: (1.0, 0.45, 0.45),
	109: (0.45, 0.9, 0.95),
	110: (1.0, 0.15, 0.15),
	111: (1.0, 0.15, 0.15),
	112: (1.0, 0.15, 0.15),
	113: (1.0, 0.15, 0.15),
	114: (1.0, 0.15, 0.15),
	115: (0.72, 0.35, 1.0),
	116: (0.72, 0.35, 1.0),
	117: (0.72, 0.35, 1.0),
	118: (0.72, 0.35, 1.0),
	119: (0.45, 0.85, 0.4),
}

# MT2009_PLUS_LEGENDS_V1: the System Legend. Since the server's legends the
# command carries two words more, "PlayerBotTitle <vid> <personality> <tier>
# <empire>": a bot of a tier shows its tier's title in the tier's colour in
# the same row, in place of its personality - "Wyrozniajacy sie", "Specjalny",
# "Chodzaca Legenda", "Czempion <krolestwa>" (ManagePlayerBotPersonalityTitle,
# playerbot_legends.h). The tiers are meant to be seen: the player's switch
# for the personalities below leaves them showing. Tier 0 (or an older
# server's two words) is the personality as before.
LEGEND_TITLES = {
	1: "Wyr\xf3\xbfniaj\xb9cy si\xea",
	2: "Specjalny",
	3: "Chodz\xb9ca Legenda",
}
LEGEND_CHAMPION_TITLE = "Czempion %s"
LEGEND_KINGDOMS = {1: "Shinsoo", 2: "Chunjo", 3: "Jinno"}
if systemSetting.GetLanguage() == "en":
	LEGEND_TITLES.update({1: "Distinguished", 2: "Special", 3: "Walking Legend"})
	LEGEND_CHAMPION_TITLE = "Champion of %s"
LEGEND_COLOURS = {
	1: (0.55, 0.85, 1.0),
	2: (0.8, 0.5, 1.0),
	3: (1.0, 0.6, 0.15),
	4: (1.0, 0.85, 0.1),
}


def legend_title(tier, empire):
	"""(text, colour) of a tier of the System Legend, or None."""
	if tier == 4:
		kingdom = LEGEND_KINGDOMS.get(empire)
		if not kingdom:
			return None
		return LEGEND_CHAMPION_TITLE % kingdom, LEGEND_COLOURS[4]
	if tier in LEGEND_TITLES:
		return LEGEND_TITLES[tier], LEGEND_COLOURS[tier]
	return None


def decode_legend(tier_arg, empire_arg):
	try:
		tier = int(tier_arg)
		empire = int(empire_arg)
	except (ValueError, TypeError):
		return None
	return legend_title(tier, empire)


TITLE_REFRESH_SECONDS = 1.0
TITLE_FORGET_SECONDS = 60.0

# The player's own switch, "Tytuly botow" in the game options: whether a bot's
# personality shows in its own row at all (NerrVoVy, 15 September; moved off
# AttachTitle onto its own row 2026-09-16). One line in playerbot_titles.cfg
# beside the client, because systemSetting has no key of its own for it; a
# missing or unreadable file means on. Off, nothing is attached and the keeper
# forgets what it held; textTail.DetachPersonality clears anything already
# showing. Either way the classic alignment title (ranga) is untouched - this
# switch no longer has anything to do with it.
TITLES_CONFIG_FILE = "playerbot_titles.cfg"
TITLES_CONFIG_KEY = "personality_titles"
_titlesEnabled = None


def TitlesConfigText(enabled):
	return "%s=%d\n" % (TITLES_CONFIG_KEY, 1 if enabled else 0)


def TitlesEnabledFromText(text):
	for line in (text or "").splitlines():
		key, sep, value = line.strip().partition("=")
		if sep and key.strip() == TITLES_CONFIG_KEY:
			return value.strip() != "0"
	return True


def TitlesEnabled():
	global _titlesEnabled
	if _titlesEnabled is None:
		try:
			f = open(TITLES_CONFIG_FILE, "r")
			try:
				_titlesEnabled = TitlesEnabledFromText(f.read())
			finally:
				f.close()
		except (IOError, OSError):
			_titlesEnabled = True
	return _titlesEnabled


def SetTitlesEnabled(enabled):
	global _titlesEnabled
	_titlesEnabled = bool(enabled)
	try:
		f = open(TITLES_CONFIG_FILE, "w")
		try:
			f.write(TitlesConfigText(_titlesEnabled))
		finally:
			f.close()
	except (IOError, OSError):
		pass
	if not _titlesEnabled:
		# The personalities go; a tier of the System Legend stays.
		keeper = GetTitleKeeper()
		import textTail
		for vid in keeper.ForgetPersonalities():
			if hasattr(textTail, "DetachPersonality"):
				textTail.DetachPersonality(vid)
	return _titlesEnabled


def decode_title(vid_arg, personality_arg):
	try:
		vid = int(vid_arg)
		personality = int(personality_arg)
	except (ValueError, TypeError):
		return None
	if vid <= 0 or vid > 0xffffffff or personality not in PERSONALITY_TITLES:
		return None
	# AttachTitle reads the VID as a signed int, like RegisterChatTail.
	if vid >= 0x80000000:
		vid -= 0x100000000
	return vid, personality


def attach_text(vid, text, colour):
	import textTail
	if not hasattr(textTail, "AttachPersonality"):
		return False
	(r, g, b) = colour
	textTail.AttachPersonality(vid, text, r, g, b)
	return True


def attach_title(vid, personality):
	return attach_text(vid, PERSONALITY_TITLES[personality],
			PERSONALITY_COLOURS.get(personality, (1.0, 1.0, 1.0)))


# MT2009_PLUS_WEEKLY_RANKING_V1: a title holder of the weekly ranking
# (uiweeklyrank.py, playerbot_weekly_rank.h on the server) - "WRANK tail <vid>
# <cat> <place>" around the holder, a player or a bot, about every ten
# seconds: its best title in the same row, in the place's colour. A bot's tier
# of the System Legend stays in front of it ("Chodzaca Legenda | Lowca I"); a
# bot's personality gives way to it. The personality switch leaves it showing.
RANK_TITLES = {
	1: "\xa3owca",
	2: "Niszczyciel",
	3: "Pogromca Boss\xf3w",
	4: "Zab\xf3jca",
	5: "Podr\xf3\xbfnik",
	6: "Kowal",
	7: "Alchemik",
	8: "Mistrz Poziom\xf3w",
}
RANK_ROMAN = {1: "I", 2: "II", 3: "III"}
RANK_COLOURS = {
	1: (1.0, 0.82, 0.25),
	2: (0.85, 0.88, 0.95),
	3: (0.9, 0.6, 0.35),
}


def decode_rank(cat_arg, place_arg):
	try:
		cat = int(cat_arg)
		place = int(place_arg)
	except (ValueError, TypeError):
		return None
	if cat not in RANK_TITLES or place not in RANK_ROMAN:
		return None
	return "%s %s" % (RANK_TITLES[cat], RANK_ROMAN[place]), RANK_COLOURS[place]


class TitleKeeper(object):
	"""One of game.py's updateables: every title heard from, attached again.

	A title is kept as (text, colour, legend, heard): legend is True for a
	tier of the System Legend, which the personality switch leaves showing.
	A weekly ranking title is kept apart, in ranks, as (text, colour, heard),
	and drawn with (or in place of) the other (Compose)."""

	def __init__(self):
		self.titles = {}
		self.ranks = {}
		self.nextRefresh = 0.0

	def Remember(self, vid, text, colour, legend, now):
		self.titles[vid] = (text, colour, legend, now)

	def RememberRank(self, vid, text, colour, now):
		self.ranks[vid] = (text, colour, now)

	def Compose(self, vid):
		"""(text, colour) the row shows for vid now, or None."""
		title = self.titles.get(vid)
		rank = self.ranks.get(vid)
		if rank is None:
			if title is None:
				return None
			return title[0], title[1]
		if title is not None and title[2]:
			return "%s | %s" % (title[0], rank[0]), rank[1]
		return rank[0], rank[1]

	def Apply(self, vid):
		shown = self.Compose(vid)
		if shown is not None:
			attach_text(vid, shown[0], shown[1])

	def ForgetPersonalities(self):
		gone = [vid for vid, title in self.titles.items() if not title[2]]
		for vid in gone:
			del self.titles[vid]
		return gone

	def CanUpdate(self):
		return bool(self.titles) or bool(self.ranks)

	def OnUpdate(self):
		if not TitlesEnabled():
			self.ForgetPersonalities()
		import clientclock
		now = clientclock.Now()
		if now < self.nextRefresh:
			return
		self.nextRefresh = now + TITLE_REFRESH_SECONDS
		for vid, (text, colour, legend, heard) in list(self.titles.items()):
			if now - heard > TITLE_FORGET_SECONDS:
				del self.titles[vid]
				continue
			if vid not in self.ranks:
				attach_text(vid, text, colour)
		for vid, (text, colour, heard) in list(self.ranks.items()):
			if now - heard > TITLE_FORGET_SECONDS:
				del self.ranks[vid]
				if vid in self.titles:
					self.Apply(vid)
				else:
					import textTail
					if hasattr(textTail, "DetachPersonality"):
						textTail.DetachPersonality(vid)
				continue
			self.Apply(vid)

	def Destroy(self):
		self.titles = {}
		self.ranks = {}


_keeper = None


def GetTitleKeeper():
	global _keeper
	if _keeper is None:
		_keeper = TitleKeeper()
	return _keeper


def decode_vid(vid_arg):
	try:
		vid = int(vid_arg)
	except (ValueError, TypeError):
		return None
	if vid <= 0 or vid > 0xffffffff:
		return None
	if vid >= 0x80000000:
		vid -= 0x100000000
	return vid


def show_title(vid_arg, personality_arg, tier_arg=None, empire_arg=None):
	# MT2009_PLUS_LEGENDS_V1: a tier of the System Legend first, whatever the
	# personality switch says.
	legend = decode_legend(tier_arg, empire_arg) if tier_arg is not None else None
	if legend is not None:
		vid = decode_vid(vid_arg)
		if vid is None or not attach_text(vid, legend[0], legend[1]):
			return False
		import clientclock
		keeper = GetTitleKeeper()
		keeper.Remember(vid, legend[0], legend[1], True, clientclock.Now())
		if vid in keeper.ranks:
			keeper.Apply(vid)
		return True
	if not TitlesEnabled():
		# A bot that has lost its tier (or the server's switch went off) while
		# its tier still shows: the row goes.
		vid = decode_vid(vid_arg)
		keeper = GetTitleKeeper()
		if vid is not None and vid in keeper.titles and keeper.titles[vid][2]:
			del keeper.titles[vid]
			import textTail
			if hasattr(textTail, "DetachPersonality"):
				textTail.DetachPersonality(vid)
		return False
	decoded = decode_title(vid_arg, personality_arg)
	if decoded is None or not attach_title(decoded[0], decoded[1]):
		return False
	import clientclock
	keeper = GetTitleKeeper()
	keeper.Remember(decoded[0], PERSONALITY_TITLES[decoded[1]],
			PERSONALITY_COLOURS.get(decoded[1], (1.0, 1.0, 1.0)), False, clientclock.Now())
	if decoded[0] in keeper.ranks:
		keeper.Apply(decoded[0])
	return True


# MT2009_PLUS_WEEKLY_RANKING_V1: "WRANK tail <vid> <cat> <place>" (game.py).
def show_rank_title(vid_arg=None, cat_arg=None, place_arg=None, *rest):
	vid = decode_vid(vid_arg)
	rank = decode_rank(cat_arg, place_arg)
	if vid is None or rank is None:
		return False
	import textTail
	if not hasattr(textTail, "AttachPersonality"):
		return False
	import clientclock
	keeper = GetTitleKeeper()
	keeper.RememberRank(vid, rank[0], rank[1], clientclock.Now())
	keeper.Apply(vid)
	return True
