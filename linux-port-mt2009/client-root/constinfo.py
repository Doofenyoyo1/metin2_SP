import app
import chrmgr
import item
import net
import player

GAME_VERSION = {
	"expansion" : 1,
	"major" : 1,
	"minor" : 0,
	"is_beta" : False,
}

# EXTRA BEGIN
ENABLE_NEW_LEVELSKILL_SYSTEM = False # loads 5 (B,M,G,P,F) skills .mse
ENABLE_RANDOM_CHANNEL_SEL = False # don't set a random channel when you open the client
ENABLE_CLEAN_DATA_IF_FAIL_LOGIN = False # don't remove id and password if the login attempt fails
ENABLE_PASTE_FEATURE = True # ctrl+v will now work
ENABLE_FULLSTONE_DETAILS = True # display all the bonuses added by a stone instead of the first one
ENABLE_REFINE_PCT = False # enable successfulness % in the refine dialog
EXTRA_UI_FEATURE = True # enable extra ui features
NEW_678TH_SKILL_ENABLE = True # load 6th warrior / ninja skills (instead of app.NEW_678TH_SKILL_ENABLE)
SELECT_CHAR_NO_DELAY = 0.5 # change the 3sec delay while choosing a character in the select phase
ENABLE_ACTIVE_PET_SEAL_EFFECT = True # enable active effect on pet seals
ENABLE_RECURSIVE_UI_DESTROY = False # force clears everything inside the UI components
ENABLE_CMDCHAT_VARIADIC_ARGS = True # enable variadic arguments in cmdchat functions
ENABLE_SELF_STACK_SCROLLS = True # enable self stack of scrolls, etc
# EXTRA END

GATEKEEPER_CHECK = True

# enable save account
ENABLE_SAVE_ACCOUNT = False
if ENABLE_SAVE_ACCOUNT:
	class SAB:
		ST_CACHE, ST_FILE, ST_REGISTRY = xrange(3)
		slotCount = 5
		storeType = ST_REGISTRY # 0 cache, 1 file, 2 registry
		btnName = {
			"Save": "SaveAccountButton_Save_%02d",
			"Access": "SaveAccountButton_Access_%02d",
			"Remove": "SaveAccountButton_Remove_%02d",
		}
		accData = {}
		regPath = r"SOFTWARE\Metin2"
		regName = "slot%02d_%s"
		regValueId = "id"
		regValuePwd = "pwd"
		fileExt = ".do.not.share.it.txt"
def CreateSABDataFolder(filePath):
	import os
	folderPath = os.path.split(filePath)[0]
	if not os.path.exists(folderPath):
		os.makedirs(folderPath)
def IsExistSABDataFile(filePath):
	import os
	return os.path.exists(filePath)
def GetSABDataFile(idx):
	import os
	filePath = "%s\\Metin2\\" % os.getenv('appdata')
	filePath += SAB.regName % (idx, SAB.regValueId)
	filePath += SAB.fileExt
	return filePath
def DelJsonSABData(idx):
	import os
	filePath = GetSABDataFile(idx)
	if IsExistSABDataFile(filePath):
		os.remove(filePath)
def GetJsonSABData(idx):
	(id, pwd) = ("", "")
	filePath = GetSABDataFile(idx)
	if not IsExistSABDataFile(filePath):
		return (id, pwd)
	with old_open(filePath) as data_file:
		try:
			import json
			(id, pwd) = json.load(data_file)
			id = str(id) # unicode to ascii
			pwd = str(pwd) # unicode to ascii
		except ValueError:
			pass
	return (id, pwd)
def SetJsonSABData(idx, slotData):
	filePath = GetSABDataFile(idx)
	CreateSABDataFolder(filePath)
	with old_open(filePath, "w") as data_file:
		import json
		json.dump(slotData, data_file)
def DelWinRegKeyValue(keyPath, keyName):
	try:
		import _winreg
		_winreg.CreateKey(_winreg.HKEY_CURRENT_USER, keyPath)
		_tmpKey = _winreg.OpenKey(_winreg.HKEY_CURRENT_USER, keyPath, 0, _winreg.KEY_WRITE)
		_winreg.DeleteValue(_tmpKey, keyName)
		_winreg.CloseKey(_tmpKey)
		return True
	except WindowsError:
		return False
def GetWinRegKeyValue(keyPath, keyName):
	try:
		import _winreg
		_tmpKey = _winreg.OpenKey(_winreg.HKEY_CURRENT_USER, keyPath, 0, _winreg.KEY_READ)
		keyValue, keyType = _winreg.QueryValueEx(_tmpKey, keyName)
		_winreg.CloseKey(_tmpKey)
		return str(keyValue) # unicode to ascii
	except WindowsError:
		return None
def SetWinRegKeyValue(keyPath, keyName, keyValue):
	try:
		import _winreg
		_winreg.CreateKey(_winreg.HKEY_CURRENT_USER, keyPath)
		_tmpKey = _winreg.OpenKey(_winreg.HKEY_CURRENT_USER, keyPath, 0, _winreg.KEY_WRITE)
		_winreg.SetValueEx(_tmpKey, keyName, 0, _winreg.REG_SZ, keyValue)
		_winreg.CloseKey(_tmpKey)
		return True
	except WindowsError:
		return False

# classic minmax def
def minmax(tmin, tmid, tmax):
	if tmid < tmin:
		return tmin
	elif tmid > tmax:
		return tmax
	return tmid
# EXTRA END

# TRADE_GOLD_WITH_THOUSANDS_SEPARATORS BEGIN
def intWithCommas(x, commasign='.'):
	# alternative of
	# return '{0:,}'.format(x).replace(',', commasign)
	if type(x) not in [type(0), type(0L)]:
		raise TypeError("Parameter must be an integer.")
	if x < 0:
		return '-' + intWithCommas(-x, commasign)
	result = ''
	while x >= 1000:
		x, r = divmod(x, 1000)
		result = "%s%03d%s" % (commasign, r, result)
	return "%d%s" % (x, result)
# TRADE_GOLD_WITH_THOUSANDS_SEPARATORS END

def Emoji(path):
	return "|E{}|e".format(path)

def Color(hexString):
	return "|cff{}|h".format(hexString)

def TextColor(text, hexString):
	return "|cff{}|h{}|r".format(hexString, text)

# option
IN_GAME_SHOP_ENABLE = 1
CONSOLE_ENABLE = 0

PVPMODE_ENABLE = 1
PVPMODE_TEST_ENABLE = 0
PVPMODE_ACCELKEY_ENABLE = 1
PVPMODE_ACCELKEY_DELAY = 0.5
PVPMODE_PROTECTED_LEVEL = 15

FOG_LEVEL0 = 4800.0
FOG_LEVEL1 = 9600.0
FOG_LEVEL2 = 12800.0
FOG_LEVEL = FOG_LEVEL0
FOG_LEVEL_LIST=[FOG_LEVEL0, FOG_LEVEL1, FOG_LEVEL2]

CAMERA_MAX_DISTANCE_SHORT = 2500.0
CAMERA_MAX_DISTANCE_LONG = 3500.0
CAMERA_MAX_DISTANCE_LIST=[CAMERA_MAX_DISTANCE_SHORT, CAMERA_MAX_DISTANCE_LONG]
CAMERA_MAX_DISTANCE = CAMERA_MAX_DISTANCE_SHORT

CHRNAME_COLOR_INDEX = 0

ENVIRONMENT_NIGHT="d:/ymir work/environment/moonlight04.msenv"

# constant
HIGH_PRICE = 500000
MIDDLE_PRICE = 50000
ERROR_METIN_STONE = 28960
SUB2_LOADING_ENABLE = 1
EXPANDED_COMBO_ENABLE = 1
CONVERT_EMPIRE_LANGUAGE_ENABLE = 0
USE_ITEM_WEAPON_TABLE_ATTACK_BONUS = 0
ADD_DEF_BONUS_ENABLE = 0
LOGIN_COUNT_LIMIT_ENABLE = 0

USE_SKILL_EFFECT_UPGRADE_ENABLE = 1

VIEW_OTHER_EMPIRE_PLAYER_TARGET_BOARD = 1
GUILD_MONEY_PER_GSP = 300
GUILD_WAR_TYPE_SELECT_ENABLE = 1
TWO_HANDED_WEAPON_ATT_SPEED_DECREASE_VALUE = 10

HAIR_COLOR_ENABLE = 1
ARMOR_SPECULAR_ENABLE = 1
WEAPON_SPECULAR_ENABLE = 1
SEQUENCE_PACKET_ENABLE = 0
KEEP_ACCOUNT_CONNETION_ENABLE = 1
MINIMAP_POSITIONINFO_ENABLE = 1

isItemQuestionDialog = 0

CURRENT_GAME_FONT_NAME = None
CURRENT_GAME_FONT_SIZE = {
	"DEFAULT" : None,
	"LARGE" : None,
	"SMALL" : None
}

SPECIAL_FLAG = {}
AFFECT_DICT = {}

myshop_data={"name": "", "duration": 0, "items":{}}

SHOPNAMES_RANGE = 5000

CAMERA_WAYPOINTS = {}
CAMERA_BEZIER_POINTS = {}

FIXED_TIME_SINCE_START = long(0)

SHOP_MASS_SELL = False

ITEMSHOP_DATA = {}


CRAFTING_DATA = {}
CRAFTING_LOAD_DATA = {}

CHAT_LAST_SENTENCE_STACK = []
WHISPER_DICT = {}

IS_GAMEMASTER = False
ADMIN_API = None
IS_MARRIAGE_PREMIUM = False

OFFLINESHOP_LAST_SEARCHED_CATEGORY = -1
OFFLINESHOP_LAST_SEARCHED_SUBCATEGORY = -1
OFFLINESHOP_LAST_SEARCH_IS_ATTR = False
OFFLINESHOP_LAST_SEARCHED_ITEMS = []

IS_HORSE_SUMMONED = False

def OFFLINE_SHOP_SEARCH_CLEAR():
	global OFFLINESHOP_LAST_SEARCHED_ITEMS
	OFFLINESHOP_LAST_SEARCHED_ITEMS = []

def GET_ITEM_QUESTION_DIALOG_STATUS():
	global isItemQuestionDialog
	return isItemQuestionDialog

def SET_ITEM_QUESTION_DIALOG_STATUS(flag):
	global isItemQuestionDialog
	isItemQuestionDialog = flag

########################

def SET_DEFAULT_FOG_LEVEL():
	global FOG_LEVEL
	app.SetMinFog(FOG_LEVEL)

def SET_FOG_LEVEL_INDEX(index):
	global FOG_LEVEL
	global FOG_LEVEL_LIST
	try:
		FOG_LEVEL=FOG_LEVEL_LIST[index]
	except IndexError:
		FOG_LEVEL=FOG_LEVEL_LIST[0]
	app.SetMinFog(FOG_LEVEL)

def GET_FOG_LEVEL_INDEX():
	global FOG_LEVEL
	global FOG_LEVEL_LIST
	return FOG_LEVEL_LIST.index(FOG_LEVEL)

########################

def SET_DEFAULT_CAMERA_MAX_DISTANCE():
	global CAMERA_MAX_DISTANCE
	app.SetCameraMaxDistance(CAMERA_MAX_DISTANCE)

def SET_CAMERA_MAX_DISTANCE_INDEX(index):
	global CAMERA_MAX_DISTANCE
	global CAMERA_MAX_DISTANCE_LIST
	try:
		CAMERA_MAX_DISTANCE=CAMERA_MAX_DISTANCE_LIST[index]
	except:
		CAMERA_MAX_DISTANCE=CAMERA_MAX_DISTANCE_LIST[0]

	app.SetCameraMaxDistance(CAMERA_MAX_DISTANCE)

def GET_CAMERA_MAX_DISTANCE_INDEX():
	global CAMERA_MAX_DISTANCE
	global CAMERA_MAX_DISTANCE_LIST
	return CAMERA_MAX_DISTANCE_LIST.index(CAMERA_MAX_DISTANCE)

########################

def SET_DEFAULT_CHRNAME_COLOR():
	global CHRNAME_COLOR_INDEX
	chrmgr.SetEmpireNameMode(CHRNAME_COLOR_INDEX)

def SET_CHRNAME_COLOR_INDEX(index):
	global CHRNAME_COLOR_INDEX
	CHRNAME_COLOR_INDEX=index
	chrmgr.SetEmpireNameMode(index)

def GET_CHRNAME_COLOR_INDEX():
	global CHRNAME_COLOR_INDEX
	return CHRNAME_COLOR_INDEX

def SET_VIEW_OTHER_EMPIRE_PLAYER_TARGET_BOARD(index):
	global VIEW_OTHER_EMPIRE_PLAYER_TARGET_BOARD
	VIEW_OTHER_EMPIRE_PLAYER_TARGET_BOARD = index

def GET_VIEW_OTHER_EMPIRE_PLAYER_TARGET_BOARD():
	global VIEW_OTHER_EMPIRE_PLAYER_TARGET_BOARD
	return VIEW_OTHER_EMPIRE_PLAYER_TARGET_BOARD

def SET_DEFAULT_CONVERT_EMPIRE_LANGUAGE_ENABLE():
	global CONVERT_EMPIRE_LANGUAGE_ENABLE
	net.SetEmpireLanguageMode(CONVERT_EMPIRE_LANGUAGE_ENABLE)

def SET_DEFAULT_USE_ITEM_WEAPON_TABLE_ATTACK_BONUS():
	global USE_ITEM_WEAPON_TABLE_ATTACK_BONUS
	player.SetWeaponAttackBonusFlag(USE_ITEM_WEAPON_TABLE_ATTACK_BONUS)

def SET_DEFAULT_USE_SKILL_EFFECT_ENABLE():
	global USE_SKILL_EFFECT_UPGRADE_ENABLE
	app.SetSkillEffectUpgradeEnable(USE_SKILL_EFFECT_UPGRADE_ENABLE)

def SET_TWO_HANDED_WEAPON_ATT_SPEED_DECREASE_VALUE():
	global TWO_HANDED_WEAPON_ATT_SPEED_DECREASE_VALUE
	app.SetTwoHandedWeaponAttSpeedDecreaseValue(TWO_HANDED_WEAPON_ATT_SPEED_DECREASE_VALUE)

########################

ACCESSORY_MATERIAL_LIST = [50623, 50624, 50625, 50626, 50627, 50628, 50629, 50630, 50631, 50632, 50633, 50634, 50635, 50636, 50637, 50638, 50639]
JewelAccessoryInfos = [
		# jewel		wrist	neck	ear
		[ 50634,	14420,	16220,	17220 ],
		[ 50635,	14500,	16500,	17500 ],
		[ 50636,	14520,	16520,	17520 ],
		[ 50637,	14540,	16540,	17540 ],
		[ 50638,	14560,	16560,	17560 ],
		[ 50639,	14570,	16570,	17570 ],
	]
def GET_ACCESSORY_MATERIAL_VNUM(vnum, subType):
	ret = vnum
	item_base = (vnum / 10) * 10
	for info in JewelAccessoryInfos:
		if item.ARMOR_WRIST == subType:
			if info[1] == item_base:
				return info[0]
		elif item.ARMOR_NECK == subType:
			if info[2] == item_base:
				return info[0]
		elif item.ARMOR_EAR == subType:
			if info[3] == item_base:
				return info[0]

	if vnum >= 16210 and vnum <= 16219:
		return 50625

	if item.ARMOR_WRIST == subType:
		WRIST_ITEM_VNUM_BASE = 14000
		ret -= WRIST_ITEM_VNUM_BASE
	elif item.ARMOR_NECK == subType:
		NECK_ITEM_VNUM_BASE = 16000
		ret -= NECK_ITEM_VNUM_BASE
	elif item.ARMOR_EAR == subType:
		EAR_ITEM_VNUM_BASE = 17000
		ret -= EAR_ITEM_VNUM_BASE

	type = ret/20

	if type<0 or type>=len(ACCESSORY_MATERIAL_LIST):
		type = (ret-170) / 20
		if type<0 or type>=len(ACCESSORY_MATERIAL_LIST):
			return 0

	return ACCESSORY_MATERIAL_LIST[type]

##################################################################

def GET_BELT_MATERIAL_VNUM(vnum, subType = 0):
	return 18900

##################################################################

def IS_AUTO_POTION(itemVnum):
	return IS_AUTO_POTION_HP(itemVnum) or IS_AUTO_POTION_SP(itemVnum)

def IS_AUTO_POTION_HP(itemVnum):
	if 72723 <= itemVnum and 72726 >= itemVnum:
		return 1
	elif itemVnum >= 76021 and itemVnum <= 76022:
		return 1
	elif itemVnum == 79012:
		return 1

	return 0

def IS_AUTO_POTION_SP(itemVnum):
	if 72727 <= itemVnum and 72730 >= itemVnum:
		return 1
	elif itemVnum >= 76004 and itemVnum <= 76005:
		return 1
	elif itemVnum == 79013:
		return 1

	return 0

def IS_PET_SEAL(itemVnum):
	return False
	# if not app.ENABLE_PET_SYSTEM_EX:
	# 	return False
	# item.SelectItem(itemVnum)
	# itemType = item.GetItemType()
	# # itemSubType = item.GetItemSubType()
	# return item.ITEM_TYPE_PET == itemType

def IS_UNIQUE_70LEVEL_WEAPON(vnum):
	return vnum >= 170 and vnum <= 179 or \
		vnum >= 250 and vnum <= 259 or \
		vnum >= 1120 and vnum <= 1129 or \
		vnum >= 7150 and vnum <= 7159 or \
		vnum >= 2180 and vnum <= 2189 or \
		vnum >= 3150 and vnum <= 3159 or \
		vnum >= 5130 and vnum <= 5139 or \
		vnum >= 7150 and vnum <= 7159

def GET_UNIQUE_70LEVEL_WEAPON_APPLY_VALUE(point, refineLevel):
	if point == 0:
		return 0

	dict = {
		player.POINT_MAX_HP: (50, 100, 150, 300, 450, 600, 800, 1000, 1200, 1500),
		player.POINT_CASTING_SPEED: (5, 6, 7, 8, 10, 12, 14, 17, 20, 25),
		player.POINT_FIRE_PCT: (2, 3, 4, 5, 6, 7, 8, 10, 12, 15),
		player.POINT_CRITICAL_PCT: (2, 3, 4, 5, 6, 7, 8, 10, 12, 15),
		player.POINT_PENETRATE_PCT: (2, 3, 4, 5, 6, 7, 8, 10, 12, 15),
		player.POINT_STUN_PCT: (2, 3, 4, 5, 6, 7, 8, 10, 12, 15),
		player.POINT_SLOW_PCT: (2, 3, 4, 5, 6, 7, 8, 10, 12, 15),
		player.POINT_ATTBONUS_HUMAN: (2, 3, 4, 5, 6, 7, 8, 10, 12, 15),
		player.POINT_ATTBONUS_MONSTER: (2, 3, 4, 5, 6, 7, 8, 10, 12, 15),
		player.POINT_ATTBONUS_BOSS: (4, 5, 6, 7, 9, 11, 13, 15, 17, 20),
		player.POINT_HT: (2, 3, 4, 5, 6, 7, 8, 10, 12, 16),
		player.POINT_IQ: (2, 3, 4, 5, 6, 7, 8, 10, 12, 16),
		player.POINT_ST: (2, 3, 4, 5, 6, 7, 8, 10, 12, 16),
		player.POINT_DX: (2, 3, 4, 5, 6, 7, 8, 10, 12, 16),
	}
	return dict[point][refineLevel]

# Czy zalogowana postac jest GM-em. Ustawiana wylacznie przez serwer
# ("SetGMFlag"), zerowana przy kazdym wejsciu do gry w game.py.
# Klient nigdy nie decyduje o tym sam - chowa tylko to, czego zwykly
# gracz i tak nie moglby uzyc; kazda komenda /gmpanel_* jest po stronie
# serwera sprawdzana wzgledem prawdziwego gm_level (cmd_info w cmd.cpp).
IsGM = False
