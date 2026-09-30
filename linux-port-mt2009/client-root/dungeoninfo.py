# MT2009_PLUS_DUNGEON_PANEL_V1: "dungeonInfo" - the C++ module of the Arezzo client's dungeon panel,
# here in Python for exe 2.0.25 (which has no such module and no packets 140/141). The same functions
# the window (uidungeoninfo.py) calls, filled from the server's "DungeonInfo" command lines
# (playerbot_dungeon_panel.h; game.py passes them to OnCommand); the window's requests go back as
# "/lochy open|warp <i>|rank <i> <type>". The panel's texts (Arezzo's, CP1250) are set on localeInfo
# and uiScriptLocale unless the locale already has them.
import app
import net
import localeInfo
import uiScriptLocale

MAX_RANKING_LINES = 10

_LOCALE = (
	('DUNGEON_INFO_COOLDOWN', "Odnowienie: %s"),
	('DUNGEON_INFO_DO_YOU_TELEPORT', "Czy chcesz teleportowa\xe6 si\xea do %s?"),
	('DUNGEON_INFO_DURATION', "Czas trwania: %s"),
	('DUNGEON_INFO_ELEMENT_ATK_BONUS', "Dzia\xb3aj\xb9cy bonus"),
	('DUNGEON_INFO_ELEMENT_DEF_AND_RES_BONUS', "Obrona i odporno\x9cci"),
	('DUNGEON_INFO_ELEMENT_UNKOWN_BONUS', "Brak wymaganego bonusu"),
	('DUNGEON_INFO_ENTRACE', "Wej\x9ccie: %s"),
	('DUNGEON_INFO_FASTEST_TIME', "Najszybszy Czas: %s"),
	('DUNGEON_INFO_HIGHEST_DMG', "Najwy\xbfsze DMG: %s"),
	('DUNGEON_INFO_LEVEL_LIMIT', "Limit Lvl: %d do %d"),
	('DUNGEON_INFO_LOCATION', "Lokacja: %s"),
	('DUNGEON_INFO_NONE', "Brak"),
	('DUNGEON_INFO_NOT_FOUND', "Nie znaleziono \xbfadnych wypraw."),
	('DUNGEON_INFO_PARTY_LIMIT', "Limit cz\xb3onk\xf3w grupy: od %d do %d"),
	('DUNGEON_INFO_STATUS_AVAILABLE', "Dost\xeapne"),
	('DUNGEON_INFO_STATUS_CLOSED', "Niedost\xeapne"),
	('DUNGEON_INFO_STATUS_COOLDOWN', "Odczekaj"),
	('DUNGEON_INFO_STATUS_HIGH_LEVEL', "Za ma\xb3y poziom!"),
	('DUNGEON_INFO_STATUS_LOW_LEVEL', "Zbyt du\xbfy poziom!"),
	('DUNGEON_INFO_TOOL_TIP_01', "W tym oknie s\xb9 wszystkie wyprawy."),
	('DUNGEON_INFO_TOOL_TIP_02', "Ka\xbfda wyprawa z listy po lewej pokazuje"),
	('DUNGEON_INFO_TOOL_TIP_03', "informacje o niej oraz przedmioty,"),
	('DUNGEON_INFO_TOOL_TIP_04', "kt\xf3re wypadaj\xb9 z ostatniego bossa."),
	('DUNGEON_INFO_TOOL_TIP_05', "Mo\xbfesz te\xbf teleportowa\xe6 si\xea pod wej\x9ccie,"),
	('DUNGEON_INFO_TOOL_TIP_06', "je\x9cli wyprawa jest dost\xeapna."),
	('DUNGEON_INFO_TOOL_TIP_07', "Po prawej s\xb9 twoje wyniki"),
	('DUNGEON_INFO_TOOL_TIP_08', "i ranking 10 najlepszych graczy."),
	('DUNGEON_INFO_TOTAL_FINISHED', "Uko\xf1czenia: %d"),
	('DUNGEON_INFO_TYPE', "Typ: %s"),
	('DUNGEON_INFO_TYPE_01', "Globalny"),
	('DUNGEON_INFO_TYPE_02', "Grupa"),
	('DUNGEON_INFO_TYPE_03', "Gildia"),
)
_SCRIPT = (
	('DUNGEON_INFO_TITLE', "Wyprawy"),
	('DUNGEON_INFO_GO_TO_ENTRANCE_TOOL_TIP', "Teleportuj pod wej\x9ccie"),
	('DUNGEON_RANKING', "Ranking"),
	('DUNGEON_RANKING_TYPE_TOOL_TIP_01', "Ranking Uko\xf1cze\xf1"),
	('DUNGEON_RANKING_TYPE_TOOL_TIP_02', "Ranking Czasowy"),
	('DUNGEON_RANKING_TYPE_TOOL_TIP_03', "Ranking DMG"),
)
for _k, _v in _LOCALE:
	if not hasattr(localeInfo, _k):
		setattr(localeInfo, _k, _v)
for _k, _v in _SCRIPT:
	if not hasattr(uiScriptLocale, _k):
		setattr(uiScriptLocale, _k, _v)

_names = {}
_list = []
_rank = []
_rankMe = [0, 0]
_rankType = [0]


def GetZoneName(mapIndex):
	return _names.get(mapIndex, "")

# The window names a dungeon by its map index (Arezzo's localeinfo has MINIMAP_ZONE_NAME_DICT_BY_IDX).
if not hasattr(localeInfo, "GetMiniMapZoneNameByIdx"):
	localeInfo.GetMiniMapZoneNameByIdx = GetZoneName


def _get(key):
	if 0 <= key < len(_list):
		return _list[key]
	return None


def GetCount():
	return len(_list)


def GetType(key):
	d = _get(key)
	return d["type"] if d else -1


def GetMapIndex(key):
	d = _get(key)
	return d["map"] if d else 0


def GetEntryMapIndex(key):
	d = _get(key)
	return d["entry"] if d else 0


def GetLevelLimit(key, i):
	d = _get(key)
	return d["level"][i] if d and i in (0, 1) else 0


def GetMemberLimit(key, i):
	d = _get(key)
	return d["party"][i] if d and i in (0, 1) else 0


def GetDuration(key):
	return 0


def GetCooldown(key):
	d = _get(key)
	if not d or not d["cooldownEnd"]:
		return 0
	return max(0, d["cooldownEnd"] - app.GetGlobalTimeStamp())


def GetElement(key):
	return 0


def GetAttBonusCount(key):
	return 0


def GetDefBonusCount(key):
	return 0


def GetAttBonus(key, i):
	return 0


def GetDefBonus(key, i):
	return 0


def GetFinish(key):
	d = _get(key)
	return d["finish"] if d else 0


def GetFinishTime(key):
	d = _get(key)
	return d["time"] if d else 0


def GetFinishDamage(key):
	d = _get(key)
	return d["damage"] if d else 0


def GetRequiredItemVnum(key, slot):
	d = _get(key)
	return d["req"][0] if d and slot == 0 else 0


def GetRequiredItemCount(key, slot):
	d = _get(key)
	return d["req"][1] if d and slot == 0 else 0


def GetBossDropCount(key):
	d = _get(key)
	return len(d["drops"]) if d else 0


def _drop(key, i):
	d = _get(key)
	if d and 0 <= i < len(d["drops"]):
		return d["drops"][i]
	return (0, 0, 0)


def GetBossDropItemVnum(key, i):
	return _drop(key, i)[0]


def GetBossDropItemCount(key, i):
	return _drop(key, i)[1]


def GetBossDropItemPercentage(key, i):
	return _drop(key, i)[2]


def GetRankingCount():
	return len(_rank)


def GetRankingByLine(line):
	if 0 <= line < len(_rank):
		return _rank[line]
	return ("", 0, 0)


def GetMyRankingLine():
	return _rankMe[0]


def ClearRanking():
	del _rank[:]
	_rankMe[0] = 0
	_rankMe[1] = 0


def Open():
	net.SendChatPacket("/lochy open")


def Close():
	pass


def Warp(key):
	if _get(key):
		net.SendChatPacket("/lochy warp %d" % key)


def Ranking(key, rankType):
	if _get(key):
		ClearRanking()
		_rankType[0] = rankType
		net.SendChatPacket("/lochy rank %d %d" % (key, rankType))


def _int(v):
	try:
		return int(v)
	except (TypeError, ValueError):
		return 0


# "DungeonInfo <sub> ..." from the server (game.py).
def OnCommand(game, *args):
	if not args:
		return
	sub, a = args[0], args[1:]
	try:
		if sub == "clear":
			_names.clear()
			del _list[:]
		elif sub == "name" and len(a) >= 2:
			_names[_int(a[0])] = " ".join(a[1:]).replace("_", " ")
		elif sub == "add" and len(a) >= 15:
			v = [_int(x) for x in a[:15]]
			cooldown = v[9]
			_list.append({
				"type": v[1], "map": v[2], "entry": v[3],
				"level": (v[4], v[5]), "party": (v[6], v[7]), "boss": v[8],
				"cooldownEnd": (app.GetGlobalTimeStamp() + cooldown) if cooldown > 0 else 0,
				"finish": v[10], "time": v[11], "damage": v[12], "req": (v[13], v[14]),
				"drops": [],
			})
		elif sub == "drop" and len(a) >= 4:
			d = _get(_int(a[0]))
			if d:
				d["drops"].append((_int(a[1]), _int(a[2]), _int(a[3])))
		elif sub == "open":
			import uidungeoninfo
			wnd = uidungeoninfo.GetWindow()
			if wnd.IsShow():
				wnd.Initialize()
		elif sub == "rank" and len(a) >= 5:
			_rank.append((a[2], _int(a[3]), _int(a[4])))
		elif sub == "rankme" and len(a) >= 2:
			_rankMe[0] = _int(a[0])
			_rankMe[1] = _int(a[1])
		elif sub == "rankend":
			import uidungeoninfo
			wnd = uidungeoninfo.GetWindow()
			if wnd.IsShow():
				wnd.OnRefreshRanking()
	except Exception:
		import dbg
		import sys
		dbg.TraceError("dungeoninfo.OnCommand %s: %s" % (str(args), str(sys.exc_info()[1])))
