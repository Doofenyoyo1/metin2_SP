# MT2009_PLUS_MONSTER_CARDS_V1 - Karty Potworow, the Monster Card System (Autor: Digi Rasta,
# nowy-system v0.25.2: his port of the "Official-Monster-Card-System" package, Best Studio).
# The package's window (uimonstercard.py) asks the exe for player.GetMonsterCardMissionInfo,
# player.GetIllustrationData, net.REQUEST_MISSION and more, which the official exe has in C++;
# here they are Python, fed by the server's "MONSTERCARDSYSTEM <text>" lines (game.py,
# /cardmonster in playerbot_monster_card.h). The monster's picture: a 3D model where the exe
# has a model viewer (player.Mt2009Model*, not in our exe yet), the card picture twice its
# size otherwise. The window opens from the Esc menu ("Karty Potworow", uisystem.py) or a key
# bound in the keybind window (keybind.py, "monster_card").
# Python 2.7; texts as CP1250 escapes.
import app
import chat
import dbg
import item
import localeInfo
import net
import player
import ui
import uiScriptLocale
import uiToolTip
import wndMgr

import monstercard_data as D
import monstercard_text as T

CZAS_TELEPORT = 30 * 60
CZAS_PRZEMIANA = 3 * 60 * 60

_stan = {"stage": 0, "main": [0, 0, 0], "clear": [0, 0, 0], "deck": [0] * 16, "mission": False, "illustration": False}
_mob = {}			# vnum -> [zebrane, zabicia, potrzeba, gwiazdki, teleport, przemiana, przywolanie]
_zalozone = {}		# vnum osiagniecia -> 1
_ranga = {}			# vnum osiagniecia -> ranga
_okno = []
_drop = []


# ------------------------------------------------------------------ funkcje player.* / net.* / app.*

def _GetMonsterCardMissionInfo():
	return (_stan["stage"], tuple(_stan["main"]), tuple(_stan["clear"]), 0, 0, 0)


def _IsMissionDataLoad():
	return _stan["mission"]


def _GetMissionVec(group):
	return [(w[0], 0, w[1], w[3], w[4], w[5]) for w in D.POZIOMY.get(group, [])][:16]


def _GetMobEmergenceAreaIndex(vnum):
	opis = D.OPIS.get(vnum)
	if not opis:
		return 0
	return (opis[2], opis[3], opis[4])


def _GetIllustrationFileLoad():
	return 1


def _IsIllustrationDataLoad():
	return _stan["illustration"]


def _PageMax(lista):
	return (len(lista) + 7) // 8


def _PageData(lista, page):
	wynik = []
	if page <= 0:
		return wynik
	for vnum in lista[(page - 1) * 8:(page - 1) * 8 + 8]:
		opis = D.OPIS.get(vnum, (0, 0, 0, 0, 0))
		wynik.append((vnum, 0, opis[0], opis[2], opis[3], opis[4]))
	return wynik


def _GetIllustrationSoloPageMax():
	return _PageMax(D.KOLEKCJA_POLE)


def _GetIllustrationPartyPageMax():
	return _PageMax(D.KOLEKCJA_LOCH)


def _GetIllustrationSoloPageData(page):
	return _PageData(D.KOLEKCJA_POLE, page)


def _GetIllustrationPartyPageData(page):
	return _PageData(D.KOLEKCJA_LOCH, page)


def _GetIllustrationData(vnum):
	m = _mob.get(vnum)
	if not m:
		return (0, 0, 0, 0, 0)
	cool0 = (m[5] + CZAS_PRZEMIANA) if m[5] > 0 else 0
	cool1 = (m[4] + CZAS_TELEPORT) if m[4] > 0 else 0
	return (m[0], m[0], m[3], cool0, cool1)


def _IsMonsterCardAchievApplied(vnum):
	return vnum in _zalozone


def _GetMonsterCardAchievRegistRank(vnum):
	return _ranga.get(vnum, 0)


def _Nic(*args):
	return 0


def _SendIllustrationMessage(kind):
	net.SendChatPacket("/cardmonster 8 %d 0" % kind)


# Podglad potwora w polu "model_view" (240x306 na x 3, y 26 okna "model_view_window"):
# - nasze exe (player.Mt2009Model*, Mt2009Window.cpp): model 3D w oknie render target, przyciski kamery i "Ruch" dzialaja;
# - exe moda: obrazek karty powiekszony 2x, przyciski kamery ukryte (nic by nie robily).
# Rasa bez danych modelu w kliencie: takze w naszym exe obrazek karty.
MODEL_3D = hasattr(player, "Mt2009ModelSelect") and hasattr(wndMgr, "RegisterRenderTarget")
SKALA_PODGLADU = 2.0
PRZYCISKI_KAMERY = ("mv_up_camera_button", "mv_down_camera_button", "mv_left_rotation_button", "mv_right_rotation_button",
	"mv_reset_button", "mv_zoomin_button", "mv_zoomout_button")
_podglad = {}


class _ModelView(ui.Window):
	def RegisterWindow(self, layer):
		self.hWnd = wndMgr.RegisterRenderTarget(self, layer)


def _Podglad():
	if not _okno:
		return None
	if not _podglad:
		okno = _okno[0]
		try:
			rodzic = okno.GetChild("model_view_window")
		except KeyError:
			return None
		obraz = ui.ExpandedImageBox()
		obraz.SetParent(rodzic)
		obraz.AddFlag("not_pick")
		obraz.Hide()
		_podglad["obraz"] = obraz
		if MODEL_3D:
			model = _ModelView()
			model.SetParent(rodzic)
			model.AddFlag("not_pick")
			model.SetPosition(3, 26)
			model.SetSize(240, 306)
			wndMgr.SetRenderTarget(model.hWnd, getattr(app, "RENDER_TARGET_INDEX_ILLUSTRATED", 1))
			model.Show()
			_podglad["model"] = model
		for nazwa in PRZYCISKI_KAMERY:
			try:
				przycisk = okno.GetChild(nazwa)
			except KeyError:
				continue
			if MODEL_3D:
				przycisk.SetTop()
			else:
				przycisk.Hide()
				przycisk.Show = _Nic
	return _podglad["obraz"]


def _ObrazekKarty(obraz, vnum):
	sciezka = None
	if vnum != 0xFFFFFFFF:
		sciezka = __import__("uimonstercard").CARD_IMG_DICT.get(vnum)
	if not sciezka:
		obraz.Hide()
		return
	obraz.LoadImage(sciezka)
	obraz.SetScale(1.0, 1.0)
	szer = int(obraz.GetWidth() * SKALA_PODGLADU)
	wys = int(obraz.GetHeight() * SKALA_PODGLADU)
	obraz.SetScale(SKALA_PODGLADU, SKALA_PODGLADU)
	obraz.SetPosition(3 + (240 - szer) / 2, 26 + (306 - wys) / 2)
	obraz.Show()


def _IllustrationSelectModel(vnum):
	obraz = _Podglad()
	if not obraz:
		return 0
	if MODEL_3D:
		if player.Mt2009ModelSelect(vnum):
			obraz.Hide()
			player.Mt2009ModelShow(1)
			return 0
		player.Mt2009ModelShow(0)
	_ObrazekKarty(obraz, vnum)
	return 0


def _IllustrationShow(pokaz):
	if MODEL_3D:
		player.Mt2009ModelShow(1 if pokaz else 0)
	if not pokaz and _podglad:
		_podglad["obraz"].Hide()
	return 0


def _Model(nazwa):
	if not MODEL_3D:
		return _Nic
	funkcja = getattr(player, nazwa)
	return lambda *args: funkcja(*args)


_PLAYER = {
	"GetMonsterCardMissionInfo": _GetMonsterCardMissionInfo,
	"IsMissionDataLoad": _IsMissionDataLoad,
	"GetMissionVec": _GetMissionVec,
	"GetMobEmergenceAreaIndex": _GetMobEmergenceAreaIndex,
	"GetIllustrationFileLoad": _GetIllustrationFileLoad,
	"IsIllustrationDataLoad": _IsIllustrationDataLoad,
	"GetIllustrationSoloPageMax": _GetIllustrationSoloPageMax,
	"GetIllustrationPartyPageMax": _GetIllustrationPartyPageMax,
	"GetIllustrationSoloPageData": _GetIllustrationSoloPageData,
	"GetIllustrationPartyPageData": _GetIllustrationPartyPageData,
	"GetIllustrationData": _GetIllustrationData,
	"IsMonsterCardAchievApplied": _IsMonsterCardAchievApplied,
	"GetMonsterCardAchievRegistRank": _GetMonsterCardAchievRegistRank,
	# podglad: model 3D w naszym exe, obrazek karty na exe moda (wyzej)
	"IllustrationSelectModel": _IllustrationSelectModel,
	"IllustrationShow": _IllustrationShow,
	"IllustrationChangeMotion": lambda vnum=0: _Model("Mt2009ModelMotion")(),
	"IllustrationModelRotation": lambda stopnie: _Model("Mt2009ModelRotation")(float(stopnie)),
	"IllustrationModelUpDown": lambda gora: _Model("Mt2009ModelUpDown")(1 if gora else 0),
	"IllustrationModelZoom": lambda blizej: _Model("Mt2009ModelZoom")(1 if blizej else 0),
	"IllustrationModelViewReset": lambda: _Model("Mt2009ModelReset")(),
	"IllustrationModelViewRes": lambda: _Model("Mt2009ModelReset")(),
}

_NET = {
	"REQUEST_MISSION": 2, "RECIVE_MISSION": 3, "SHUFFLE_MISSION": 6, "REWARD_MISSION": 5, "INIT_MISSION": 7,
	"REQUEST_ILLUSTRATION": 2,
	"MC_PROMOTION": 6, "MC_TRADE": 5, "MC_POLY": 1, "MC_WARP": 0, "MC_SPAWN": 3, "MC_REKRUTE": 4,
}


def _Install():
	app.ENABLE_MONSTER_CARD = 1
	app.ENABLE_MONSTER_CARD_ACHIEV = 1
	app.IllustratedCreate = _Nic
	for name, func in _PLAYER.items():
		setattr(player, name, func)
	for name, value in _NET.items():
		setattr(net, name, value)
	net.SendIllustrationMessage = _SendIllustrationMessage
	for name, text in T.GRA.items():
		setattr(localeInfo, name, text)
	for name, text in T.INTERFEJS.items():
		setattr(uiScriptLocale, name, text)
	# obszary wystepowania potworow: numer mapy -> nazwa strefy z localeInfo moda
	strefy = {}
	for index, mapa in D.MAPY.items():
		strefy[index] = getattr(localeInfo, "MINIMAP_ZONE_NAME_DICT", {}).get(mapa, mapa)
	localeInfo.MINIMAP_ZONE_NAME_DICT_BY_IDX = strefy


_Install()


# ------------------------------------------------------------------ opis przedmiotu (tooltip) kart 50283 / 50284

def CardTooltip(tip, vnum, slots):
	# Every monster's card has the same name and icon - the tooltip names the monster and shows its
	# card (uitooltip.py). The monster: a mission card's socket 1, a tradable card's socket 0.
	import ui
	import nonplayer
	item.SelectItem(vnum)
	mob = 0
	if slots:
		if vnum == 50284:
			mob = slots[0] or slots[1]
		else:
			mob = slots[1] or slots[0]
	tip.SetTitle(item.GetItemName())
	if mob:
		tip.AppendTextLine(nonplayer.GetMonsterName(mob), tip.POSITIVE_COLOR)
	tip.AppendDescription(item.GetItemDescription(), 26)
	if mob and mob in D.OPIS:
		image = ui.ImageBox()
		image.SetParent(tip)
		image.Show()
		image.LoadImage("d:/ymir work/ui/game/monster_card/card/%d.sub" % mob)
		image.SetPosition((tip.toolTipWidth / 2) - image.GetWidth() / 2, tip.toolTipHeight)
		tip.toolTipHeight += image.GetHeight()
		tip.childrenList.append(image)
		tip.ResizeToolTip()


# ------------------------------------------------------------------ okno

def Window():
	if not _okno:
		_okno.append(__import__("uimonstercard").MonsterCardWindow())
	return _okno[0]


def Toggle():
	window = Window()
	if window.IsShow():
		window.Close()
	else:
		window.Show()


# The keybind window's action "monster_card" (game.py, GameWindow.__BuildKeyDict).
ToggleWindow = Toggle


def _Ui(name, *args):
	if not _okno:
		return
	try:
		getattr(_okno[0], name)(*args)
	except:
		import sys
		dbg.TraceError("monstercard: blad w oknie kart (%s): %s" % (name, sys.exc_info()[1]))


def _Info(text):
	chat.AppendChat(chat.CHAT_TYPE_INFO, text)


def _Int(text, default=0):
	try:
		return int(text)
	except ValueError:
		return default


# ------------------------------------------------------------------ komendy serwera

def OnServer(*args):
	if not args:
		return
	parts = args[0].split("/")
	cmd = parts[0]
	n = len(parts)

	if cmd == "ADD_DATA" and n >= 3:
		if parts[1] == "Level":
			_stan["stage"] = _Int(parts[2])
			_stan["mission"] = True
		elif parts[1] == "Cards" and n >= 24:
			_stan["main"] = [_Int(x) for x in parts[2:5]]
			_stan["deck"] = [_Int(x) for x in parts[5:21]]
			_stan["clear"] = [_Int(x) for x in parts[21:24]]
			_stan["mission"] = True
	elif cmd == "NEW_MISSION" and n >= 17:
		_stan["deck"] = [_Int(x) for x in parts[1:17]]
		_stan["mission"] = True
	elif cmd == "REC_MAINCARDS" and n >= 4:
		_stan["main"] = [_Int(x) for x in parts[1:4]]
		_stan["mission"] = True
		_Ui("RefreshMissionPage")
	elif cmd == "SUCCES_KILL":
		_stan["mission"] = True
	elif cmd == "SUCCES_MISSION":
		_Ui("RefreshMissionPage")
	elif cmd == "OPEN":
		_Ui("RefreshMissionPage")
	elif cmd == "ADD_MOB_INFO" and n >= 6:
		vnum = _Int(parts[1])
		czasy = [0, 0, 0]
		for i in (0, 1, 2):
			if n >= 7 + i:
				czasy[i] = _Int(parts[6 + i])
		_mob[vnum] = [_Int(parts[2]), _Int(parts[3]), _Int(parts[4]), _Int(parts[5]), czasy[0], czasy[1], czasy[2]]
		_stan["illustration"] = True
		_Ui("MonsterCardIllustrationRefresh")
	elif cmd == "ILLUSTRATION_READY":
		_stan["illustration"] = True
		_Ui("MonsterCardIllustrationRefresh")
	elif cmd == "NO_NEW_ORDER" and n >= 2:
		_Ui("MonsterCardShowCooldown", _Int(parts[1]))
		if not _okno:
			_Info(T.KOMUNIKATY["COOLDOWN"] % localeInfo.SecondToHM(_Int(parts[1])))
	elif cmd == "NO_NEED_STAGE" and n >= 2:
		_Ui("MonsterCardShowNeedStage", _Int(parts[1]))
		if not _okno:
			_Info(T.KOMUNIKATY["NEED_STAGE"] % _Int(parts[1]))
	elif cmd == "ACHIEV_APPLY" and n >= 3:
		if _Int(parts[2]):
			_zalozone[_Int(parts[1])] = 1
		else:
			_zalozone.pop(_Int(parts[1]), None)
		_Ui("MonsterCardAchievRefresh")
	elif cmd == "ACHIEV_REGIST" and n >= 3:
		vnum = _Int(parts[1])
		if _Int(parts[2]) > 0:
			_ranga[vnum] = _Int(parts[2])
		else:
			_ranga.pop(vnum, None)
		_Ui("MonsterCardAchievRefresh")
	elif cmd == "MISSION_FAIL" and n >= 3:
		_Ui("MonsterCardMissionFail", _Int(parts[1]), _Int(parts[2]))
	elif cmd == "DISABLED":		# M2_MONSTER_CARDS=0 on the server (MT2009 Classic)
		_Ui("Close")
		_Info(T.KOMUNIKATY["DISABLED"])
	elif cmd in T.KOMUNIKATY:
		_Info(T.KOMUNIKATY[cmd])
	elif cmd == "NEW_DROPP_GUI" and n >= 2:
		del _drop[:]
		_drop.append(_Int(parts[1]))
	elif cmd == "ADD_DROPP" and n >= 4:
		ile = _Int(parts[2])
		_drop.append((_Int(parts[1]), ile, _Int(parts[4], ile) if n >= 5 else ile, float(parts[3])))
	elif cmd == "OPEN_DROPP_GUI" and _drop:
		import nonplayer
		_Info(T.KOMUNIKATY["DROP_HEADER"] % nonplayer.GetMonsterName(_drop[0]))
		for vnum, ile_min, ile_max, chance in _drop[1:]:
			item.SelectItem(vnum)
			ile = str(ile_min) if ile_min == ile_max else "%d-%d" % (ile_min, ile_max)
			_Info(T.KOMUNIKATY["DROP_ROW"] % (item.GetItemName(), ile, ("%.2f" if chance >= 0.1 else "%.4f") % chance))
		reszta = _Int(parts[1]) if n >= 2 else 0
		if reszta > 0:
			_Info(T.KOMUNIKATY["DROP_MORE"] % reszta)
		del _drop[:]
