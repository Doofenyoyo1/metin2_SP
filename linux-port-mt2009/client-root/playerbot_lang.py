# Which of our texts' two languages the player reads.
#
# Everything this project wrote into the client - Auto Lowy, the companion's
# windows, the chest and drop previews, the bag's and the depot's arranging,
# the Dom Towarowy - says it in Polish or in English: Polish for a client set
# to Polish, English for every other language, English being what a Romanian
# or a German reads before Polish. That is the rule the bots' status lines
# have followed since server 2.2.6 (playerbot_status_tail.py), and it is the
# client's own switch that decides: the LANGUAGE line of game1.cfg, written by
# the login screen and by the launcher, read here through
# systemSetting.GetLanguage(). A client that cannot say reads Polish, and so
# does every test that stubs nothing.
#
# The stock scripts' texts are not ours and stay the locale pack's
# (localeinfo.py, uiscriptlocale.py and english_gui.py over them, "en" only).
#
# The server cannot see the switch, and what it says to one player - the
# companion's lines, a war's or a duel's refusal, our quests' dialogs, the
# bots' notices - it says in one language. So it asks: at every entry into the
# game a login quest (playerbot_lang.quest) sends "PlayerBotLanguage", and
# AnswerServer() says "/playerbot_lang en" or "pl". The client never offers it
# unasked, so a server from before the question never hears a command it does
# not know ("This command does not exist" after every teleport is what an
# unasked command costs).
#
# Python 2.7 as the client has it, and 3 for the tests.

POLISH = 'pl'
ENGLISH = 'en'


def Language():
	"""The client's language code; Polish when the client cannot say."""
	try:
		import systemSetting
		language = systemSetting.GetLanguage()
	except Exception:
		return POLISH
	return language or POLISH


def IsEnglish():
	return Language() != POLISH


def T(pl, en):
	"""The Polish text, or its English twin for a client that is not Polish.
	The two of a pair take the same %-arguments."""
	if IsEnglish():
		return en
	return pl


def ServerCode():
	return ENGLISH if IsEnglish() else POLISH


def AnswerServer(*rest):
	"""The answer to the server's "PlayerBotLanguage"."""
	import net
	net.SendChatPacket('/playerbot_lang ' + ServerCode())
