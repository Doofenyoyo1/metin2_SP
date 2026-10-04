# MT2009_PLUS_SHOP_AUTO_PRICE_V1: which of a text's two languages the player
# reads - Polish for a client set to Polish, English for every other one (the
# LANGUAGE line of game1.cfg, read through systemSetting.GetLanguage(), as
# playerbot_status_tail.py does). A client that cannot say reads Polish.
#
# Came with the "Auto-cena" extension (shopautoprice.py and the shop windows
# use T()). Only the client-side helper: the extension's AnswerServer() sent
# "/playerbot_lang", a command our server does not have, and is left out.
#
# Python 2.7 as the client has it.

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
