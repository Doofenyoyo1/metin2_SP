import app
import pack
import os
import flamewindPath
import systemSetting

AUTOBAN_QUIZ_ANSWER = "ANSWER"
AUTOBAN_QUIZ_REFRESH = "REFRESH"
AUTOBAN_QUIZ_REST_TIME = "REST_TIME"

OPTION_SHADOW = "SHADOW"

CODEPAGE = str(app.GetDefaultCodePage())

def LoadLocaleFile(srcFileName, localeDict):
	localeDict["CUBE_INFO_TITLE"] = "Recipe"
	localeDict["CUBE_REQUIRE_MATERIAL"] = "Requirements"
	localeDict["CUBE_REQUIRE_MATERIAL_OR"] = "or"

	try:
		lines = open(srcFileName, "r").readlines()
	except IOError:
		import dbg
		dbg.LogBox("LoadUIScriptLocaleError(%(srcFileName)s)" % locals())
		app.Abort()

	for line in lines:
		tokens = line[:-1].split("\t")

		if len(tokens) >= 2:
			localeDict[tokens[0]] = tokens[1]

		else:
			print(len(tokens), lines.index(line), line)

name = app.GetLocalePath()

__IS_ARABIC		= "locale/ae" == app.GetLocalePath()
if app.ENABLE_LOCALE_COMMON and not __IS_ARABIC:
	LOCALE_UISCRIPT_PATH = "UIScript/"
	LOCALE_COMMON_UISCRIPT_PATH = "locale/common/ui/"
else:
	LOCALE_UISCRIPT_PATH = "%s/ui/" % (name)
	LOCALE_COMMON_UISCRIPT_PATH = "locale/common/ui/"
LOGIN_PATH = "%s/ui/login/" % (name)
EMPIRE_PATH = "%s/ui/empire/" % (name)
if app.ENABLE_LOCALE_COMMON and not __IS_ARABIC:
	GUILD_PATH = "locale/common/ui/guild/"
else:
	GUILD_PATH = "%s/ui/guild/" % (name)
SELECT_PATH = "%s/ui/select/" % (name)
WINDOWS_PATH = "%s/ui/windows/" % (name)
MAPNAME_PATH = "%s/ui/mapname/" % (name)

JOBDESC_WARRIOR_PATH = "%s/jobdesc_warrior.txt" % (name)
JOBDESC_ASSASSIN_PATH = "%s/jobdesc_assassin.txt" % (name)
JOBDESC_SURA_PATH = "%s/jobdesc_sura.txt" % (name)
JOBDESC_SHAMAN_PATH = "%s/jobdesc_shaman.txt" % (name)
if app.ENABLE_WOLFMAN_CHARACTER:
	JOBDESC_WOLFMAN_PATH = "%s/jobdesc_wolfman.txt" % (name)

EMPIREDESC_A = "%s/empiredesc_a.txt" % (name)
EMPIREDESC_B = "%s/empiredesc_b.txt" % (name)
EMPIREDESC_C = "%s/empiredesc_c.txt" % (name)

RULES = "%s/rules.txt" % (name)

DEFAULT_LANGUAGE = "pl"
LOCALE_INTERFACE_FILE_NAME = "locale/%s/locale_interface.txt" % (DEFAULT_LANGUAGE)
LoadLocaleFile(LOCALE_INTERFACE_FILE_NAME, locals())

if systemSetting.GetLanguage() != DEFAULT_LANGUAGE:
	LOCALE_INTERFACE_FILE_NAME = "locale/%s/locale_interface.txt" % (systemSetting.GetLanguage())
	LoadLocaleFile(LOCALE_INTERFACE_FILE_NAME, locals())

if app.ENABLE_LOCALE_COMMON:
	def TryLoadLocaleFile(filename):
		if pack.Exist(filename) or os.path.exists(filename):
			LoadLocaleFile(filename, globals())
	TryLoadLocaleFile("locale/common/locale_interface_ex.txt")
	TryLoadLocaleFile("%s/locale_interface_ex.txt" % app.GetLocalePath())
