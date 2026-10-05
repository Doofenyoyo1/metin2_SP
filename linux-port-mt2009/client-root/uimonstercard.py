import ui
import uiScriptLocale
import app
import net
import dbg
import snd
import player
import mouseModule
import wndMgr
import localeInfo
import localeinfo_point
import uiToolTip
import chr
import item
import chat
import nonplayer
import constInfo

# MT2009_PLUS_MONSTER_CARDS_V1 - Karty Potworow (Autor: Digi Rasta, a port of the "Official-Monster-Card-System"
# window, Best Studio): the tables and the player.* / net.* functions the window asks for (monstercard.py),
# the sets' bonus names from our localeinfo_point.
import monstercard as mckarty


class ExPopupDialog(ui.ScriptWindow):
	# Z uicommon.py paczki (mod ma PopupDialog z innym ukladem tekstu).

	def __init__(self, layer = "UI"):
		ui.ScriptWindow.__init__(self, layer)
		self.__LoadDialog()
		self.acceptEvent = lambda *arg: None

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def __LoadDialog(self):
		try:
			PythonScriptLoader = ui.PythonScriptLoader()
			PythonScriptLoader.LoadScriptFile(self, "UIScript/PopupDialog.py")

			self.board = self.GetChild("board")
			self.message = self.GetChild("message")
			self.message.Show()
			self.accceptButton = self.GetChild("accept")
			self.accceptButton.SetEvent(ui.__mem_func__(self.Close))
		except:
			import exception
			exception.Abort("MonsterCard.PopupDialog.LoadDialog")

	def Open(self):
		self.SetCenterPosition()
		self.SetTop()
		self.Show()

	def Close(self):
		self.Hide()
		self.acceptEvent()

	def Destroy(self):
		self.Close()
		self.ClearDictionary()

	def SetWidth(self, width):
		height = self.GetHeight()
		self.SetSize(width, height)
		self.board.SetSize(width, height)
		self.SetCenterPosition()
		self.UpdateRect()

	def SetText(self, text):
		self.message.SetText(text)

	def SetAcceptEvent(self, event):
		self.acceptEvent = event

	def SetButtonName(self, name):
		self.accceptButton.SetText(name)

	def OnPressEscapeKey(self):
		self.Close()
		return True

	def OnIMEReturn(self):
		self.Close()
		return True

	def GetTextSize(self):
		if self.message:
			return self.message.GetTextSize()
		return (0, 0)


class ExQuestionDialog(ui.ScriptWindow):
	# Z uicommon.py paczki (mod ma QuestionDialog bez GetTextSize).

	def __init__(self, layer = "UI"):
		ui.ScriptWindow.__init__(self, layer)
		self.__CreateDialog()

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def __CreateDialog(self):
		pyScrLoader = ui.PythonScriptLoader()
		pyScrLoader.LoadScriptFile(self, "uiscript/questiondialog.py")

		self.board = self.GetChild("board")
		self.textLine = self.GetChild("message")
		self.acceptButton = self.GetChild("accept")
		self.cancelButton = self.GetChild("cancel")

	def Open(self):
		self.SetCenterPosition()
		self.SetTop()
		self.Show()

	def Close(self):
		self.Hide()

	def SetWidth(self, width):
		height = self.GetHeight()
		self.SetSize(width, height)
		self.board.SetSize(width, height)
		self.SetCenterPosition()
		self.UpdateRect()

	def SAFE_SetAcceptEvent(self, event):
		self.acceptButton.SAFE_SetEvent(event)

	def SAFE_SetCancelEvent(self, event):
		self.cancelButton.SAFE_SetEvent(event)

	def SetAcceptEvent(self, event):
		self.acceptButton.SetEvent(event)

	def SetCancelEvent(self, event):
		self.cancelButton.SetEvent(event)

	def SetText(self, text):
		self.textLine.SetText(text)

	def SetAcceptText(self, text):
		self.acceptButton.SetText(text)

	def SetCancelText(self, text):
		self.cancelButton.SetText(text)

	def OnPressEscapeKey(self):
		self.Close()
		return True

	def GetTextSize(self):
		if self.textLine:
			return self.textLine.GetTextSize()
		return (0, 0)


def _AffectDict():
	# Teksty bonusow z tabeli moda (localeinfo_point.AFFECT_DICT, klucz = numer POINT); wartosci to teksty z %d -
	# okno paczki oczekuje funkcji od wartosci.
	global _AFFECT_CACHE
	if _AFFECT_CACHE is None:
		wynik = {}
		for key, text in localeinfo_point.AFFECT_DICT.items():
			if isinstance(text, str) and "%" in text:
				wynik[key] = (lambda t: (lambda val: t % val))(text)
			else:
				wynik[key] = text
		_AFFECT_CACHE = wynik
	return _AFFECT_CACHE

_AFFECT_CACHE = None

from collections import deque

def unsigned32(n):
	return n & 0xFFFFFFFFL
	
ROOT_PATH = "d:/ymir work/ui/game/monster_card/"
CARD_PATH = "d:/ymir work/ui/game/monster_card/card/"

## ??? ???
WAIT_ARRAY_WIDTH		= 8		## ??? ?????? WAIT ??? ???? 8?
WAIT_ARRAY_HEIGHT		= 2		## ??? ?????? WAIT ??? ???? 2?
SELECTED_ARRAY_WIDTH	= 3		## ??? ?????? ???? ??? ???? 3?, ???? 1?

MISSION_STATE_NONE		 = 0
MISSION_STATE_WAIT		 = 1	## ??? ????( ??? ??? ??)
MISSION_STATE_PROCEEDING = 2	## ??????
MISSION_STATE_REWARD	 = 3	## ??????

MISSION_INDEX_STAGE			= 0
MISSION_INDEX_MOB_VNUM		= 1
MISSION_INDEX_MOB_CLEAR		= 2
MISSION_INDEX_RESET_TIME	= 3
MISSION_INDEX_RESET_COUNT	= 4
MISSION_INDEX_SHUFFLE_COUNT = 5

CARD_MOVE_SPEED		= 10.0		## ??? ??? ???

SHUFFLE_MAX = 1					## ???? ??? ???

FAILED_MISSION_SHUFFLE_NO_ITEM			= 0
FAILED_MISSION_INIT_ITEM_FALL_SHORT		= 1
FAILED_MISSION_REWARD_INVEN_FULL		= 2
FAILED_MISSION_REWARD_NO_CLEAR			= 3
FAILED_MSSION_COMMON_MSG				= 4
FAILED_MISSION_MSG_MAX					= 5
	
CARD_IMG_DICT_BASE =  \
{
	0	: ROOT_PATH + "empty_card.sub",

	# Alan / solo (ve ortak temel kartlar)
	151 : CARD_PATH + "151.sub",
	152 : CARD_PATH + "152.sub",
	153 : CARD_PATH + "153.sub",
	154 : CARD_PATH + "154.sub",
	155 : CARD_PATH + "155.sub",
	191 : CARD_PATH + "191.sub",
	192 : CARD_PATH + "192.sub",
	193 : CARD_PATH + "193.sub",
	194 : CARD_PATH + "194.sub",
	391 : CARD_PATH + "391.sub",
	393 : CARD_PATH + "393.sub",
	394 : CARD_PATH + "394.sub",
	431 : CARD_PATH + "431.sub",
	432 : CARD_PATH + "432.sub",
	403 : CARD_PATH + "433.sub",
	433 : CARD_PATH + "433.sub",
	434 : CARD_PATH + "434.sub",
	435 : CARD_PATH + "435.sub",
	436 : CARD_PATH + "436.sub",
	491 : CARD_PATH + "491.sub",
	492 : CARD_PATH + "492.sub",
	493 : CARD_PATH + "493.sub",
	494 : CARD_PATH + "494.sub",
	533 : CARD_PATH + "533.sub",
	534 : CARD_PATH + "534.sub",
	591 : CARD_PATH + "591.sub",
	595 : CARD_PATH + "595.sub",
	691 : CARD_PATH + "691.sub",
	791 : CARD_PATH + "791.sub",
	1901 : CARD_PATH + "1901.sub",
	1304 : CARD_PATH + "1304.sub",
	2191 : CARD_PATH + "2191.sub",
	2206 : CARD_PATH + "2206.sub",
	2306 : CARD_PATH + "2306.sub",
	3091 : CARD_PATH + "3091.sub",
	3191 : CARD_PATH + "3191.sub",
	3291 : CARD_PATH + "3291.sub",
	3491 : CARD_PATH + "3491.sub",
	3591 : CARD_PATH + "3591.sub",
	3596 : CARD_PATH + "3596.sub",
	3791 : CARD_PATH + "3791.sub",
	3891 : CARD_PATH + "3891.sub",
	3910 : CARD_PATH + "3910.sub",
	6392 : CARD_PATH + "6392.sub",
	6116 : CARD_PATH + "6116.sub",
	6407 : CARD_PATH + "6407.sub",
	6699 : CARD_PATH + "6699.sub",
	6705 : CARD_PATH + "6705.sub",
	6706 : CARD_PATH + "6706.sub",
	6723 : CARD_PATH + "6723.sub",
	6724 : CARD_PATH + "6724.sub",
	6749 : CARD_PATH + "6749.sub",
	6764 : CARD_PATH + "6764.sub",
	6776 : CARD_PATH + "6776.sub",
	6783 : CARD_PATH + "6783.sub",
	6920 : CARD_PATH + "6920.sub",
	6922 : CARD_PATH + "6922.sub",
}

CARD_IMG_DICT_DUNGEON =  \
{
	# Zindan / party (resmi sunucu icin gerekli olanlar)
	5161 : CARD_PATH + "5161.sub",
	5162 : CARD_PATH + "5162.sub",
	5163 : CARD_PATH + "5163.sub",
	1091 : CARD_PATH + "1091.sub",
	1093 : CARD_PATH + "1093.sub",
	2092 : CARD_PATH + "2092.sub",
	2402 : CARD_PATH + "2402.sub",
	1192 : CARD_PATH + "1192.sub",
	2597 : CARD_PATH + "2597.sub",
	2492 : CARD_PATH + "2492.sub",
	6405 : CARD_PATH + "6405.sub",
	2493 : CARD_PATH + "2493.sub",
	6109 : CARD_PATH + "6109.sub",
	6191 : CARD_PATH + "6191.sub",
	6207 : CARD_PATH + "6116.sub",
	6009 : CARD_PATH + "6009.sub",
	6091 : CARD_PATH + "6091.sub",
	6408 : CARD_PATH + "6408.sub",
	6192 : CARD_PATH + "6192.sub",
	2752 : CARD_PATH + "2752.sub",
	2762 : CARD_PATH + "2762.sub",
	2772 : CARD_PATH + "2772.sub",
	2782 : CARD_PATH + "2782.sub",
	2792 : CARD_PATH + "2792.sub",
	2802 : CARD_PATH + "2802.sub",
	2812 : CARD_PATH + "2812.sub",
	2822 : CARD_PATH + "2822.sub",
	2832 : CARD_PATH + "2832.sub",
	2842 : CARD_PATH + "2842.sub",
	2852 : CARD_PATH + "2852.sub",
	2862 : CARD_PATH + "2862.sub",
	6805 : CARD_PATH + "6805.sub",
	6815 : CARD_PATH + "6815.sub",
	6820 : CARD_PATH + "6820.sub",
	6856 : CARD_PATH + "6756.sub",
	7609 : CARD_PATH + "7609.sub",
	7610 : CARD_PATH + "7610.sub",
	7611 : CARD_PATH + "7611.sub",
	7612 : CARD_PATH + "7612.sub",
	7613 : CARD_PATH + "7613.sub",
	7614 : CARD_PATH + "7614.sub",
	6789 : CARD_PATH + "6789.sub",
	6797 : CARD_PATH + "6797.sub",
	6791 : CARD_PATH + "6791.sub",
	6937 : CARD_PATH + "6937.sub",
	6938 : CARD_PATH + "6938.sub",
	6939 : CARD_PATH + "6939.sub",
}






CARD_IMG_DICT = {}
CARD_IMG_DICT.update(CARD_IMG_DICT_BASE)
CARD_IMG_DICT.update(CARD_IMG_DICT_DUNGEON)

#if app.ENABLE_12ZI:
CARD_IMG_DICT[2752] = CARD_PATH + "2752.sub"	## 12zi ??
CARD_IMG_DICT[2762] = CARD_PATH + "2762.sub"	## 12zi ??
CARD_IMG_DICT[2772] = CARD_PATH + "2772.sub"	## 12zi ??
CARD_IMG_DICT[2782] = CARD_PATH + "2782.sub"	## 12zi ??
CARD_IMG_DICT[2792] = CARD_PATH + "2792.sub"	## 12zi ??
CARD_IMG_DICT[2802] = CARD_PATH + "2802.sub"	## 12zi ??
CARD_IMG_DICT[2812] = CARD_PATH + "2812.sub"	## 12zi ??
CARD_IMG_DICT[2822] = CARD_PATH + "2822.sub"	## 12zi ??
CARD_IMG_DICT[2832] = CARD_PATH + "2832.sub"	## 12zi ??
CARD_IMG_DICT[2842] = CARD_PATH + "2842.sub"	## 12zi ??
CARD_IMG_DICT[2852] = CARD_PATH + "2852.sub"	## 12zi ??
CARD_IMG_DICT[2862] = CARD_PATH + "2862.sub"	## 12zi ??
	

## ????
ILLUSTRATED_ARRAY_WIDTH		= 4
ILLUSTRATED_ARRAY_HEIGHT	= 2
STAR_COUNT = 5

ILLUSTRATION_PAGE_MAX			= 5

CLASS_COUNT_MAX = \
{
	0 : 9,		# ??0 ~ ??1
	1 : 21,		# ??1 ~ ??2
	2 : 30,		# ??2 ~ ??3
	3 : 60,		# ??3 ~ ??4
	4 : 90,		# ??4 ~ ??5
	5 : 120,	# ??5 ~ MAX
}

ILLUSTRATION_MODEL_RENDER	= 1
ILLUSTRATION_MOTION_CALSS	= 2
ILLUSTRATION_POLY_CLASS		= 3
ILLUSTRATION_WARP_CLASS		= 4
ILLUSTRATION_SUMMON_CLASS	= 5
	
	
FAILED_COUNT_MAX = 0
FAILED_POLY_COOLTIME = 1
FAILED_WARP_LIMIT_LEVEL = 2
FAILED_WARP_TRADE = 3
FAILED_ILLUSTRATION_MSG_MAX = 4

TRADE_COUNT = 10	# ????? ????? ??? ????

class MissionPage:
	pass
 
class IllustrationPage:
	pass

class AchievPage:
	pass

class MonsterCardAchievDetailWindow(ui.ScriptWindow):
	def __init__(self):
		ui.ScriptWindow.__init__(self, "UI")
		self.isLoaded = False
		self.vnum = 0
		self.data = None
		self.name = ""
		self.onSelectRank = None
		self.onToggleApply = None
		self.onCloseEvent = None

		self.text = None
		self.apply_button = None
		self.regist_button = None
		self.reward_img = None
		self.applying_img = None
		self.disable_img = None
		self.star_buttons = []
		self.applying_star_imgs = []
		self.regist_star_imgs = []
		self.card_imgs = []
		self.card_clear_imgs = []
		self.selectedRank = 0
		self.allUnlocked = False
		self.minClass = 0
		
		# Thinboard tooltip (same style as main Monster Card window tooltips)
		self.buttontooltip = None
		self.ShowButtonToolTip = False

	def LoadWindow(self):
		if self.isLoaded:
			return
		self.isLoaded = True
		self.__LoadScript("uiscript/monstercardachievdetailwindow.py")
		self.__BindObject()
		try:
			self.buttontooltip = uiToolTip.ToolTip()
			self.buttontooltip.ClearToolTip()
		except:
			self.buttontooltip = None

	def __LoadScript(self, fileName):
		pyScrLoader = ui.PythonScriptLoader()
		pyScrLoader.LoadScriptFile(self, fileName)

	def __BindObject(self):
		self.GetChild("board").SetCloseEvent(ui.__mem_func__(self.Close))
		# "-" button (official): closes this panel
		try:
			minus_btn = self.GetChild("achiev_detail_minus")
			minus_btn.SetEvent(ui.__mem_func__(self.Close))
			try:
				minus_btn.SetAlwaysToolTip(True)
				minus_btn.SetShowToolTipEvent(ui.__mem_func__(self.OverInToolTipButton), "Zamknij")
				minus_btn.SetHideToolTipEvent(ui.__mem_func__(self.OverOutToolTipButton))
			except:
				pass
		except:
			pass
		self.text = self.GetChild("achiev_detail_text")
		self.apply_button = self.GetChild("achiev_detail_apply")
		self.regist_button = self.GetChild("achiev_detail_regist")
		self.reward_img = self.GetChild("achiev_detail_reward")
		self.applying_img = self.GetChild("achiev_detail_applying")
		self.disable_img = self.GetChild("achiev_detail_disable")

		self.apply_button.SetEvent(ui.__mem_func__(self.__OnClickApply))
		self.regist_button.SetEvent(ui.__mem_func__(self.__OnClickRegist))
		# Header tooltips should match main window (Uygula / Etkin / Etkin de?il)
		try:
			if self.apply_button and hasattr(uiScriptLocale, "MC_ACHIEV_APPLY"):
				self.apply_button.SetAlwaysToolTip(True)
				self.apply_button.SetShowToolTipEvent(ui.__mem_func__(self.OverInToolTipButton), uiScriptLocale.MC_ACHIEV_APPLY)
				self.apply_button.SetHideToolTipEvent(ui.__mem_func__(self.OverOutToolTipButton))
		except:
			pass
		try:
			if self.applying_img and hasattr(uiScriptLocale, "MC_ACHIEV_APPLYING"):
				self.applying_img.eventDict["MOUSE_OVER_IN"] = lambda: self.OverInToolTipButton(uiScriptLocale.MC_ACHIEV_APPLYING)
				self.applying_img.eventDict["MOUSE_OVER_OUT"] = lambda: self.OverOutToolTipButton()
		except:
			pass
		try:
			if self.disable_img and hasattr(uiScriptLocale, "MC_ACHIEV_DISABLE"):
				self.disable_img.eventDict["MOUSE_OVER_IN"] = lambda: self.OverInToolTipButton(uiScriptLocale.MC_ACHIEV_DISABLE)
				self.disable_img.eventDict["MOUSE_OVER_OUT"] = lambda: self.OverOutToolTipButton()
		except:
			pass
		# Thinboard tooltip for "Ba?ar?? talep et" inside detail window.
		try:
			if hasattr(uiScriptLocale, "MC_ACHIEV_REGIST"):
				self.regist_button.SetAlwaysToolTip(True)
				self.regist_button.SetShowToolTipEvent(ui.__mem_func__(self.OverInToolTipButton), uiScriptLocale.MC_ACHIEV_REGIST)
				self.regist_button.SetHideToolTipEvent(ui.__mem_func__(self.OverOutToolTipButton))
		except:
			pass

		for i in xrange(5):
			self.star_buttons.append(self.GetChild("achiev_star_button_0" + str(i)))
			self.applying_star_imgs.append(self.GetChild("achiev_applying_star_img_0" + str(i)))
			self.regist_star_imgs.append(self.GetChild("achiev_regist_star_img_0" + str(i)))
			self.star_buttons[i].SetEvent(ui.__mem_func__(self.__OnSelectRank), i + 1)

		for i in xrange(8):
			self.card_imgs.append(self.GetChild("achiev_detail_monster_card_0" + str(i)))
			self.card_clear_imgs.append(self.GetChild("achiev_detail_monster_card_clear_0" + str(i)))

		# Middle bonus icon in detail header: show tooltip like main window's middle icon
		try:
			if self.reward_img:
				self.reward_img.Show()
				self.reward_img.eventDict["MOUSE_OVER_IN"] = lambda: self.OverInToolTipButton(self.__FormatDetailBonusTooltip())
				self.reward_img.eventDict["MOUSE_OVER_OUT"] = lambda: self.OverOutToolTipButton()
		except:
			pass

	def __GetAchievRankValue(self, rank):
		try:
			rank = int(rank)
		except:
			return 0
		if rank <= 0:
			return 0
		try:
			bonuses = self.data.get("rank_bonus", [])
		except:
			bonuses = []
		idx = rank - 1
		if idx < 0 or idx >= len(bonuses):
			return 0
		return bonuses[idx]

	def __FormatDetailBonusTooltip(self):
		if not self.data:
			return ""
		try:
			applyTypes = self.data.get("apply_type", [])
		except:
			applyTypes = []
		if not applyTypes:
			return ""

		# Pull AFFECT_DICT from tooltip module (same mapping used in main window)
		aff = _AffectDict()
		applyId = applyTypes[0]

		# Determine start rank
		rank_count = 0
		try:
			rank_count = int(self.data.get("rank_count", 0))
		except:
			rank_count = 0
		start_rank = 1
		if rank_count > 0 and rank_count < 5:
			start_rank = 6 - rank_count

		# Prefer registered rank if exists, else selected rank, else start_rank
		regRank = 0
		try:
			regRank = int(player.GetMonsterCardAchievRegistRank(self.vnum))
		except:
			regRank = 0
		r = regRank if regRank > 0 else (self.selectedRank if self.selectedRank > 0 else start_rank)
		# Official detail window: show configured bonus value per selected star (preview),
		# regardless of whether player has unlocked/requested it yet.
		val = self.__GetAchievRankValue(r)

		v = aff.get(applyId, "APPLY_%d" % applyId)
		try:
			if callable(v):
				try:
					v = v(int(val))
				except:
					v = v()
		except:
			pass
		try:
			name = str(v) if v is not None else ""
		except:
			name = "APPLY_%d" % applyId

		# If localized text already includes the value, don't append again.
		try:
			valStr = str(int(val))
			if valStr in name and ("+" in name or "%" in name or name.lstrip().startswith(valStr)):
				return "%s" % name
		except:
			pass
		return "%s +%d" % (name, int(val))

	def Open(self, vnum, name, data, registRank, isApplying):
		self.LoadWindow()
		self.vnum = vnum
		self.data = data
		self.name = name
		self.selectedRank = 0
		self.allUnlocked = False

		if self.text:
			self.text.SetText(name)

		# Center this detail window on screen (official behavior: opens centered over Monster Card UI).
		try:
			self.SetCenterPosition()
		except:
			pass

		# rank_count=2 => ranks 4,5 ; rank_count=3 => ranks 3,4,5 ; rank_count=5 => 1..5
		rank_count = 0
		try:
			rank_count = int(data.get("rank_count", 0))
		except:
			rank_count = 0
		start_rank = 1
		if rank_count > 0 and rank_count < 5:
			start_rank = 6 - rank_count

		for i in xrange(5):
			self.applying_star_imgs[i].Hide()
			self.regist_star_imgs[i].Hide()

		# Apply visuals: if applied, show apply-star up to registered rank
		if isApplying and registRank > 0:
			for i in xrange(min(registRank, 5)):
				self.applying_star_imgs[i].Show()

		if registRank > 0:
			for i in xrange(registRank):
				self.regist_star_imgs[i].Show()

		# Enable/disable Apply button based on registRank
		try:
			if self.apply_button:
				if registRank > 0:
					self.apply_button.Enable()
				else:
					self.apply_button.Disable()
		except:
			pass
		
		# "Ba?ar?? talep et" button (detail): decide after allUnlocked is computed (below).
		try:
			if self.regist_button:
				self.regist_button.Hide()
		except:
			pass

		# Top-right icon logic should match main window:
		# - no stage yet -> disable image
		# - stage exists (allUnlocked) or applying -> applying image
		# - after request (registRank>0, not applying) -> show apply button icon
		try:
			if self.reward_img:
				self.reward_img.Hide()
			if self.applying_img:
				self.applying_img.Hide()
			if self.disable_img:
				self.disable_img.Hide()
		except:
			pass

		# Star buttons availability:
		# - only ranks in [start_rank..5]
		# - require all required cards to be unlocked (stage>0)
		allUnlocked = True

		# Cards
		monsters = []
		try:
			monsters = data.get("monster", [])
		except:
			monsters = []

		minClass = 99
		for i in xrange(8):
			v = 0
			if i < len(monsters):
				v = monsters[i]
			if v and CARD_IMG_DICT.has_key(v):
				self.card_imgs[i].LoadImage(CARD_IMG_DICT[v])
				# card_alpha is used as a "locked" overlay in this UI; hide it when player has at least stage 1.
				ill_data = player.GetIllustrationData(v)
				if ill_data:
					(accumulation_count, cur_count, cur_class, cooltime0, cooltime1) = ill_data
					minClass = min(minClass, cur_class)
					if cur_class > 0:
						self.card_clear_imgs[i].Hide()
					else:
						self.card_clear_imgs[i].Show()
						allUnlocked = False
				else:
					minClass = 0
					self.card_clear_imgs[i].Show()
					allUnlocked = False
			else:
				self.card_imgs[i].LoadImage(CARD_IMG_DICT[0])
				self.card_clear_imgs[i].Hide()

		if minClass == 99:
			minClass = 0
		self.minClass = minClass
		if minClass < start_rank:
			allUnlocked = False
		self.allUnlocked = allUnlocked

		# "Ba?ar?? talep et" button (detail): only show when player is eligible to request.
		# Official: hidden until stage is unlocked and not already registered/applied.
		try:
			if self.regist_button:
				if registRank <= 0 and (not isApplying) and allUnlocked:
					self.regist_button.Show()
				else:
					self.regist_button.Hide()
		except:
			pass

		try:
			# Middle bonus icon should always stay visible
			if self.reward_img:
				self.reward_img.Show()
				try:
					self.reward_img.eventDict["MOUSE_OVER_IN"] = lambda: self.OverInToolTipButton(self.__FormatDetailBonusTooltip())
				except:
					pass

			if registRank > 0 and not isApplying:
				# Requested/registered but not applied yet: show apply button (icon) and reward marker.
				if self.apply_button:
					self.apply_button.Show()
				if self.applying_img:
					self.applying_img.Hide()
				if self.disable_img:
					self.disable_img.Hide()
			elif isApplying or allUnlocked:
				# Applying or eligible stage exists.
				if self.applying_img:
					self.applying_img.Show()
				if self.apply_button:
					self.apply_button.Hide()
				if self.disable_img:
					self.disable_img.Hide()
			else:
				# No stage yet.
				if self.disable_img:
					self.disable_img.Show()
				if self.apply_button:
					self.apply_button.Hide()
				if self.applying_img:
					self.applying_img.Hide()
		except:
			pass

		# Star buttons enable/disable
		for i in xrange(5):
			rank = i + 1
			try:
				if rank < start_rank:
					# Official: only show available ranks (e.g. rank_count=2 -> show x4,x5 only).
					self.star_buttons[i].Hide()
					self.applying_star_imgs[i].Hide()
					self.regist_star_imgs[i].Hide()
				else:
					# Official: ranks are selectable to preview bonus values even before unlocking.
					self.star_buttons[i].Show()
					self.star_buttons[i].Enable()
					self.star_buttons[i].SetUp()
			except:
				pass

		# Official feel: preselect first available rank so "Ba?ar?? talep et" immediately works.
		try:
			if self.selectedRank <= 0:
				if allUnlocked:
					self.selectedRank = int(start_rank)
					for i in xrange(5):
						if i == (self.selectedRank - 1):
							self.star_buttons[i].Down()
						else:
							self.star_buttons[i].SetUp()
		except:
			pass

		self.Show()
		self.SetTop()

	def __OnSelectRank(self, rank):
		self.selectedRank = rank
		# Update star button visuals immediately.
		try:
			for i in xrange(5):
				if (i + 1) == int(rank):
					self.star_buttons[i].Down()
				else:
					self.star_buttons[i].SetUp()
		except:
			pass
		if self.onSelectRank:
			self.onSelectRank(self.vnum, rank)
		# Do not close on rank select; registration happens via "Ba?ar?? talep et" confirmation.
		try:
			self.SetTop()
		except:
			pass

	def __OnClickApply(self):
		if self.onToggleApply:
			self.onToggleApply(self.vnum)
		self.Close()

	def __OnClickRegist(self):
		# Official: 2-step confirmation then register selected rank.
		if not self.data or not self.vnum:
			return
		if self.selectedRank <= 0:
			return
		if self.selectedRank > self.minClass:
			chat.AppendChat(chat.CHAT_TYPE_INFO, mckarty.T.KOMUNIKATY["RANK_STARS"])
			return
		# Count required monsters/cards for this achievement.
		needCount = 0
		try:
			monsters = self.data.get("monster", [])
			needCount = len(monsters)
		except:
			needCount = 0

		# Step 1: QUESTION_1 only
		try:
			text1 = ""
			if hasattr(localeInfo, "MC_ACHIEV_REGIST_QUESTION_1"):
				text1 = localeInfo.MC_ACHIEV_REGIST_QUESTION_1 % (self.name, needCount)
			else:
				text1 = "%s" % self.name
			# Prevent overflow by inserting a line break after the first question mark if present.
			try:
				qpos = text1.find("?")
				if qpos != -1 and qpos + 1 < len(text1):
					text1 = text1[:qpos + 1] + "\\n" + text1[qpos + 1:].lstrip()
			except:
				pass
			full = text1
		except:
			full = "%s" % self.name

		# Use same question dialog pattern as MonsterCardWindow.
		try:
			if hasattr(self, "question") and self.question:
				self.question.Close()
		except:
			pass
		try:
			q = ExQuestionDialog("TOP_MOST")
			q.SetText(full)
			# Auto-size to prevent text overflow
			try:
				(tw, th) = q.GetTextSize()
				w = max(340, min(620, int(tw) + 80))
				q.SetWidth(w)
			except:
				pass
			q.SetAcceptEvent(ui.__mem_func__(self.__AcceptRegistStep1))
			q.SetCancelEvent(ui.__mem_func__(self.__CloseQuestionDialog))
			q.Open()
			self.question = q
		except:
			pass

	def __AcceptRegistStep1(self):
		self.__CloseQuestionDialog()
		text2 = ""
		try:
			text2 = localeInfo.MC_ACHIEV_REGIST_QUESTION_2 if hasattr(localeInfo, "MC_ACHIEV_REGIST_QUESTION_2") else ""
		except:
			text2 = ""
		if not text2:
			return self.__AcceptRegist()
		try:
			q = ExQuestionDialog("TOP_MOST")
			q.SetText(text2)
			# Auto-size step 2 as well
			try:
				(tw, th) = q.GetTextSize()
				w = max(340, min(620, int(tw) + 80))
				q.SetWidth(w)
			except:
				pass
			q.SetAcceptEvent(ui.__mem_func__(self.__AcceptRegist))
			q.SetCancelEvent(ui.__mem_func__(self.__CloseQuestionDialog))
			q.Open()
			self.question = q
		except:
			pass

	def __CloseQuestionDialog(self):
		try:
			if hasattr(self, "question") and self.question:
				self.question.Close()
		except:
			pass
		self.question = None

	def __AcceptRegist(self):
		self.__CloseQuestionDialog()
		if self.vnum and self.selectedRank > 0:
			# Confirmed: register selected rank.
			net.SendChatPacket("/cardmonster 11 %d %d" % (self.vnum, self.selectedRank))

	def Close(self):
		self.Hide()
		if self.onCloseEvent:
			self.onCloseEvent()

	def OverInToolTipButton(self, btnText):
		if self.buttontooltip:
			texts = btnText.split('\\n')
			if texts and texts[-1] == "":
				del texts[-1]
			if not texts:
				return
			lens = [len(text) for text in texts]
			text_max_len = max(lens) + 2

			pos_x, pos_y = wndMgr.GetMousePosition()

			self.buttontooltip.ClearToolTip()
			self.buttontooltip.SetThinBoardSize(11 * text_max_len)
			for text in texts:
				self.buttontooltip.AppendTextLine(text, 0xffffffff)
			self.buttontooltip.SetToolTipPosition(pos_x, pos_y - 20)
			self.buttontooltip.Show()
			self.buttontooltip.SetTop()
			self.ShowButtonToolTip = True

	def OverOutToolTipButton(self):
		if self.buttontooltip:
			self.buttontooltip.Hide()
			self.ShowButtonToolTip = False

	def OnUpdate(self):
		# keep tooltip following mouse
		if self.buttontooltip and self.ShowButtonToolTip:
			pos_x, pos_y = wndMgr.GetMousePosition()
			self.buttontooltip.SetToolTipPosition(pos_x, pos_y - 20)
 

## ???? ???
class MonsterCardWindow(ui.ScriptWindow):
			
	def __init__(self):
		ui.ScriptWindow.__init__(self, "UI")
		self.isLoaded = 0
		self.SetWindowName("MonsterCardWindow")
		
		self.tabDict		= None
		self.tabButtonDict	= None
		self.pageDict		= None
		self.curKey			= None
		self.popup			= None
		self.question		= None
		
		## ??? ??????
		self.mission_page	= MissionPage()
		self.mission_page.waitArray				= [[0 for col in range(0,WAIT_ARRAY_WIDTH)] for row in range(0,WAIT_ARRAY_HEIGHT)]
		self.mission_page.waitVnumDict			= {}
		self.mission_page.selectedArray			= [0 for col in range(0,SELECTED_ARRAY_WIDTH)]	# ????
		self.mission_page.MissionClearImgArray	= [0 for col in range(0,SELECTED_ARRAY_WIDTH)]	# ??? ?????
		self.mission_page.selectedFrameArray	= [0 for col in range(0,SELECTED_ARRAY_WIDTH)]	# ???? ??????
		self.mission_page.seletedMobNameArray	= [0 for col in range(0,SELECTED_ARRAY_WIDTH)]	# ???(text) ???
		self.mission_page.seletedAreaImageArray	= [0 for col in range(0,SELECTED_ARRAY_WIDTH)]	# ???? ???? ????? ???
		self.mission_page.setectedAreaTextArray	= [0 for col in range(0,SELECTED_ARRAY_WIDTH)]	# ???? ???? ???(text) ???
		self.mission_page.recive_mission_button = None	# ????? ???
		self.mission_page.shuffle_card_button	= None	# ????? ???		
		self.mission_page.reward_card_button	= None	# ????? ???
		self.mission_page.mission_init_button	= None	# ???? ???
		self.mission_page.init_question_button	= None	# ????? ???
		self.mission_page.mission_state			= MISSION_STATE_NONE
		self.mission_page.mission_data			= None
		self.mission_page.mission_tuple			= []
		
		self.mission_page.card_move_queue		= deque()
		self.mission_page.move_img				= None
		self.mission_page.lock					= False
		
		
		
		## ?????? ??????
		self.illustration_page = IllustrationPage()
		self.illustration_page.CardImageArray		= [[0 for col in range(0,ILLUSTRATED_ARRAY_WIDTH)] for row in range(0,ILLUSTRATED_ARRAY_HEIGHT)]
		self.illustration_page.CardImageAlpha		= [[0 for col in range(0,ILLUSTRATED_ARRAY_WIDTH)] for row in range(0,ILLUSTRATED_ARRAY_HEIGHT)]
		self.illustration_page.CardSelectImage		= [[0 for col in range(0,ILLUSTRATED_ARRAY_WIDTH)] for row in range(0,ILLUSTRATED_ARRAY_HEIGHT)]
		self.illustration_page.CardData				= [[0 for col in range(0,ILLUSTRATED_ARRAY_WIDTH)] for row in range(0,ILLUSTRATED_ARRAY_HEIGHT)]
		self.illustration_page.CardEnergyBGArray	= [[0 for col in range(0,ILLUSTRATED_ARRAY_WIDTH)] for row in range(0,ILLUSTRATED_ARRAY_HEIGHT)]		
		self.illustration_page.CardEnergyImageArray = [[0 for col in range(0,ILLUSTRATED_ARRAY_WIDTH)] for row in range(0,ILLUSTRATED_ARRAY_HEIGHT)]		
		self.illustration_page.CardAreaImageArray	= [[0 for col in range(0,ILLUSTRATED_ARRAY_WIDTH)] for row in range(0,ILLUSTRATED_ARRAY_HEIGHT)]
		self.illustration_page.CardAreaTextArray	= [[0 for col in range(0,ILLUSTRATED_ARRAY_WIDTH)] for row in range(0,ILLUSTRATED_ARRAY_HEIGHT)]
		self.illustration_page.CardMobNameArray		= [[0 for col in range(0,ILLUSTRATED_ARRAY_WIDTH)] for row in range(0,ILLUSTRATED_ARRAY_HEIGHT)]
		self.illustration_page.CardStarOnArray		= [[[0 for col in range(0,STAR_COUNT)] for col in range(0,ILLUSTRATED_ARRAY_WIDTH)] for row in range(0,ILLUSTRATED_ARRAY_HEIGHT)]
		self.illustration_page.CardStarOffArray		= [[[0 for col in range(0,STAR_COUNT)] for col in range(0,ILLUSTRATED_ARRAY_WIDTH)] for row in range(0,ILLUSTRATED_ARRAY_HEIGHT)]
		self.illustration_page.solo_cur_page		= 1
		self.illustration_page.solo_page_max		= 0
		self.illustration_page.party_cur_page		= 1
		self.illustration_page.party_page_max		= 0
		
		# ??????
		self.illustration_page.flushArray			= [[0 for col in range(0,ILLUSTRATED_ARRAY_WIDTH)] for row in range(0,ILLUSTRATED_ARRAY_HEIGHT)]
		
		## page ???
		self.illustration_page.page_button_list		= [0 for col in range(0,ILLUSTRATION_PAGE_MAX)]
		self.illustration_page.first_prev_button	= None	# <<
		self.illustration_page.prev_button			= None	# <
		self.illustration_page.next_button			= None	# >
		self.illustration_page.last_next_button		= None	# >>

		## ???? ?????? (2026)
		self.achiev_page = AchievPage()
		self.achiev_page.isLoaded = False
		self.achiev_page.type = 0 # 0 field, 1 dungeon
		self.achiev_page.data = []
		self.achiev_page.nameDict = {}
		self.achiev_page.listItems = []
		self.achiev_page.cur_index = 0
		self.achiev_page.affectDict = None

		self.achiev_page.field_button = None
		self.achiev_page.dungeon_button = None
		self.achiev_page.list_window = None
		self.achiev_page.list_scroll = None
		self.achiev_page.bonus_bar_text = None
		self.achiev_page.field_bonus_value = None
		self.achiev_page.change_view_button = None
		self.achiev_page.bonus_list_window = None
		self.achiev_page.model_window = None
		self.achiev_page.bonus_scroll = None
		self.achiev_page.bonus_lines = []
		self.achiev_page.bonus_line_data = []
		self.achiev_page.bonus_line_visible = 0
		self.achiev_page.bonus_scroll_pos = 0.0
		self.achiev_page.show_model = False
		self.achiev_page.detailWnd = None
		self.achiev_page.sort_mode = 0 # 0 all, 1 applying, 2 regist, 3 able_regist, 4 disable
		self.achiev_page.sort_list_button = None
		self.achiev_page.sort_list_window = None
		self.achiev_page.sort_arrow_button = None
		self.achiev_page.sort_all_button = None
		self.achiev_page.sort_applying_button = None
		self.achiev_page.sort_regist_button = None
		self.achiev_page.sort_able_regist_button = None
		self.achiev_page.sort_disable_button = None
		self.achiev_page.sort_mouse_over_img = None
		
		## ?????? ?????? ????
		self.illustration_page.motion_button_tooltip	= None
		self.illustration_page.motion_button_tooltip2	= None
		self.illustration_page.poly_button_tooltip		= None
		self.illustration_page.poly_button_tooltip2		= None
		self.illustration_page.warp_button_tooltip		= None
		self.illustration_page.warp_button_tooltip2		= None
		
		## ??
		self.illustration_page.cur_model_vnum		= 0
		self.illustration_page.cur_data				= None
		self.illustration_page.cur_model_rotation	= 0.0
		
		## ????
		self.buttontooltip		= None
		self.ShowButtonToolTip	= False
		
		self.__LoadWindow()
		
	def __del__(self):
		ui.ScriptWindow.__del__(self)
		
		if self.mission_page.card_move_queue:
			self.mission_page.card_move_queue.clear()
		
	def Show(self):
		self.__LoadWindow()
		ui.ScriptWindow.Show(self)
		# Official behavior: Monster Card opens centered.
		try:
			self.SetCenterPosition()
		except:
			pass
		self.ShowPage()
		self.SetTop()
			
	def Hide(self):
		wndMgr.Hide(self.hWnd)
		
	def Close(self):
		
		player.IllustrationShow( False )
		# Clear preview: engine treats race 0 as valid on some shards.
		player.IllustrationSelectModel( 0xFFFFFFFF )
		self.__ClearIllustrationButton()
		self.illustration_page.cur_model_vnum		= 0
		self.illustration_page.cur_data				= None
		self.illustration_page.cur_model_rotation	= 0.0
		
		if self.illustration_page.mv_reset_button:
			self.illustration_page.mv_reset_button.Hide()
		if self.illustration_page.left_rotation_button:
			self.illustration_page.left_rotation_button.Hide()
		if self.illustration_page.right_rotation_button:
			self.illustration_page.right_rotation_button.Hide()
		if self.illustration_page.zoomin_button:
			self.illustration_page.zoomin_button.Hide()
		if self.illustration_page.zoomout_button:
			self.illustration_page.zoomout_button.Hide()
		if self.illustration_page.mv_up_button:
			self.illustration_page.mv_up_button.Hide()
		if self.illustration_page.mv_down_button:
			self.illustration_page.mv_down_button.Hide()
		
		if self.illustration_page.mv_count_text:
			self.illustration_page.mv_count_text.SetText("")
		if self.illustration_page.mv_name_text:
			self.illustration_page.mv_name_text.SetText("")
			
		self.Hide()
		
		if self.buttontooltip:
			self.buttontooltip.Hide()
			self.ShowButtonToolTip	= False
		
	def OnPressEscapeKey(self):
		self.Close()
		return True
		
	def BindInterfaceClass(self, interface):
		from _weakref import proxy
		self.interface = proxy(interface)
		
	def __LoadScript(self, fileName):
		pyScrLoader = ui.PythonScriptLoader()
		pyScrLoader.LoadScriptFile(self, fileName)
		
	def __LoadWindow(self):
	
		player.IllustrationShow( False )
		player.IllustrationSelectModel( 0xFFFFFFFF )
		
		if self.isLoaded == 1:
			return
		self.isLoaded = 1
		
		## script
		try:
			self.__LoadScript("uiscript/monstercardwindow.py")
				
		except:
			import exception
			exception.Abort("MonsterCardWindow.LoadWindow.__LoadScript")
		
		## object	
		try:
			self.__BindObject()
		except:
			import exception
			exception.Abort("MonsterCardWindow.LoadWindow.__BindObject")
			
		## event
		try:	
			self.__BindEvent()
		except:
			import exception
			exception.Abort("MonsterCardWindow.LoadWindow.__BindEvent")
		
		self.SetPage("MISSION")
		
			
	def Destroy(self):
		self.isLoaded = 0
		
	def OnRender(self):
		pass
		
	def __BindObject(self):
		self.GetChild("board").SetCloseEvent(ui.__mem_func__(self.Close))
		
		## ?????, ???????, ??????? (+ ????: 2026 UI)
		self.tabDict = {
			"MISSION"	: self.GetChild("tab_menu_1"),
			"SOLO"		: self.GetChild("tab_menu_2"),
			"PARTY"		: self.GetChild("tab_menu_3"),
		}

		self.tabButtonDict = {
			"MISSION"	: self.GetChild("tab_menu_button_1"),
			"SOLO"		: self.GetChild("tab_menu_button_2"),
			"PARTY"		: self.GetChild("tab_menu_button_3"),
		}
		
		
		self.pageDict = {
			"MISSION"	: self.GetChild("mission_page"),
			"SOLO"		: self.GetChild("illustration_page"),
			"PARTY"		: self.GetChild("illustration_page"),
		}

		# Achiev page is present in UI script; enable by default when monster card is on.
		# (Some clients don't expose app.ENABLE_MONSTER_CARD_ACHIEV even though the UI exists.)
		if not hasattr(app, "ENABLE_MONSTER_CARD_ACHIEV") or app.ENABLE_MONSTER_CARD_ACHIEV:
			self.tabDict["ACHIEV"] = self.GetChild("tab_menu_4")
			self.tabButtonDict["ACHIEV"] = self.GetChild("tab_menu_button_4")
			self.pageDict["ACHIEV"] = self.GetChild("achiev_page")

			self.achiev_page.field_button = self.GetChild("field_achiev_button")
			self.achiev_page.dungeon_button = self.GetChild("dungeon_achiev_button")
			self.achiev_page.list_window = self.GetChild("achiev_card_list_window")
			self.achiev_page.list_scroll = self.GetChild("achiev_list_scrollbar")
			self.achiev_page.bonus_bar_text = self.GetChild("achiev_bonus_bar_text")
			self.achiev_page.change_view_button = self.GetChild("achiev_list_change_view_button")
			self.achiev_page.bonus_list_window = self.GetChild("achiev_card_bonus_list_window")
			self.achiev_page.model_window = self.GetChild("achiev_card_model_window")
			self.achiev_page.bonus_scroll = self.GetChild("achiev_bonus_list_scrollbar")
			self.achiev_page.field_bonus_value = self.GetChild("achiev_field_bonus_text_value")
			self.achiev_page.sort_list_button = self.GetChild("achiev_sort_list_button")
			self.achiev_page.sort_list_window = self.GetChild("achiev_sort_list_window")
			self.achiev_page.sort_arrow_button = self.GetChild("achiev_sort_list_arrow_button")
			self.achiev_page.sort_all_button = self.GetChild("achiev_sort_all_button")
			self.achiev_page.sort_applying_button = self.GetChild("achiev_sort_applying_button")
			self.achiev_page.sort_regist_button = self.GetChild("achiev_sort_regist_button")
			self.achiev_page.sort_able_regist_button = self.GetChild("achiev_sort_able_regist_button")
			self.achiev_page.sort_disable_button = self.GetChild("achiev_sort_disable_button")
			self.achiev_page.sort_mouse_over_img = self.GetChild("achiev_sort_list_mouse_over_image")
		
		##??? ??????########################################################################
		## ??? ??? ??????
		## wait window ?? ???? ??????? ????
		## ???? 8 * ???? 2
		wait_window = self.GetChild("wait_card_window")
		for row in xrange(0, WAIT_ARRAY_HEIGHT):
			for col in xrange(0, WAIT_ARRAY_WIDTH):
				ex_image = ui.ExpandedImageBox()
				ex_image.SetParent( wait_window )
				ex_image.LoadImage( CARD_IMG_DICT[0] )
				ex_image.SetPosition( 69 * col + 13 * col , 84 * row + 6 *row )
				ex_image.SetScale( 0.75, 0.75 )
				ex_image.Show()
				# ImageBox/ExpandedImageBox doesn't implement SetShowToolTipEvent on this client build.
				# eventDict callbacks are called without args; keep as plain callables (no __mem_func__ for lambdas).
				# (Custom hint removed; keep official behavior)
				ex_image.eventDict["MOUSE_OVER_IN"] = lambda: None
				ex_image.eventDict["MOUSE_OVER_OUT"] = lambda: self.OverOutToolTipButton()
				self.mission_page.waitArray[row][col] = ex_image
				
		## ???? ????? ????
		for col in xrange(0,SELECTED_ARRAY_WIDTH):
			# ????
			self.mission_page.selectedArray[col] = self.GetChild( "selected_img" + str(col) )
			# ??? ?????
			self.mission_page.MissionClearImgArray[col] = self.GetChild( "selected_clear_img" + str(col) )
			self.mission_page.MissionClearImgArray[col].Hide()
			# ??????
			self.mission_page.selectedFrameArray[col] = self.GetChild( "selected_frame" + str(col) )
			# ????????
			self.mission_page.seletedAreaImageArray[col] = self.GetChild( "selected_area" + str(col) )
			
			
		## ??? ??? ???? ?????
		# ????? ???
		self.mission_page.recive_mission_button = self.GetChild("recive_mission_button")
		# ????? ???
		self.mission_page.shuffle_card_button = self.GetChild("shuffle_card_button")
		# ????? ???
		self.mission_page.reward_card_button = self.GetChild("reward_card_button")
		# ???? ???
		self.mission_page.mission_init_button = self.GetChild("mission_init_button")
		# ????? ???
		self.mission_page.init_question_button = self.GetChild("init_question_button")
		
		## wait ??? alpha bg
		self.mission_page.wait_card_alpha = self.GetChild("wait_card_alpha_bg_window")
		
		## Alter Text
		self.mission_page.alter_text = self.GetChild("MissionAlterText")
		
		##### ??? ???????? ???? move image ???? ????.	
		self.mission_page.move_img = ui.MoveImageBox()
		self.mission_page.move_img.SetParent( self.GetChild("mission_page") )
		self.mission_page.move_img.SetEndMoveEvent( ui.__mem_func__(self.CardMoveEndEvnet) )
		self.mission_page.move_img.SetMoveSpeed(CARD_MOVE_SPEED)
		self.mission_page.move_img.Hide()
		#####
		
		##?????? ??????########################################################################
		## illustration card ???? ??????? ????
		## ???? 4 * ???? 2
		#ILLUSTRATED_ARRAY_WIDTH	= 4
		#ILLUSTRATED_ARRAY_HEIGHT	= 2
		for row in xrange(0, ILLUSTRATED_ARRAY_HEIGHT):
			for col in xrange(0, ILLUSTRATED_ARRAY_WIDTH):
				## ????
				ex_image = ui.ExpandedImageBox()
				ex_image.SetParent( self.GetChild("illustrated_window") )
				ex_image.LoadImage( CARD_IMG_DICT[0] )
				# x,y = (42,113) - (21,75)
				# img width, height = (92,112)
				# gap 29, 89
				ex_image.SetPosition( 21 + (col* 92) + (col*29), 38 + (row*112) + (row*89) )
				ex_image.Show()
				self.illustration_page.CardImageArray[row][col] = ex_image
				## ???? ????
				alpha_image = ui.ExpandedImageBox()
				alpha_image.SetParent( self.GetChild("illustrated_window") )
				alpha_image.LoadImage( ROOT_PATH + "card_alpha.sub" )
				alpha_image.SetPosition( 21 + (col* 92) + (col*29), 38 + (row*112) + (row*89) )
				alpha_image.Show()
				self.illustration_page.CardImageAlpha[row][col] = alpha_image
				
				## ???? ????
				alpha_image = ui.ExpandedImageBox()
				alpha_image.SetParent( self.GetChild("illustrated_window") )
				alpha_image.LoadImage( ROOT_PATH + "card_view_line.sub" )
				alpha_image.SetPosition( 21 + (col* 92) + (col*29), 38 + (row*112) + (row*89) )
				alpha_image.Hide()
				self.illustration_page.CardSelectImage[row][col] = alpha_image
				
				## ??????
				ani_image = ui.AniImageBox()
				ani_image.SetParent( self.GetChild("illustrated_window") )
				ani_image.SetDelay( 6 )
				ani_image.AppendImage( "D:/Ymir Work/UI/game/monster_card/card_effect/card_effect2.sub" )
				ani_image.AppendImage( "D:/Ymir Work/UI/game/monster_card/card_effect/card_effect3.sub" )
				ani_image.AppendImage( "D:/Ymir Work/UI/game/monster_card/card_effect/card_effect4.sub" )
				ani_image.AppendImage( "D:/Ymir Work/UI/game/monster_card/card_effect/card_effect5.sub" )
				ani_image.AppendImage( "D:/Ymir Work/UI/game/monster_card/card_effect/card_effect4.sub" )
				ani_image.AppendImage( "D:/Ymir Work/UI/game/monster_card/card_effect/card_effect3.sub" )
				ani_image.AppendImage( "D:/Ymir Work/UI/game/monster_card/card_effect/card_effect2.sub" )
				ani_image.AppendImage( "D:/Ymir Work/UI/game/monster_card/card_effect/card_effect1.sub" )
				ani_image.SetPosition( 21 + (col* 92) + (col*29), 38 + (row*112) + (row*89) )
				ani_image.Hide()
				self.illustration_page.flushArray[row][col] = ani_image
				
				## ???????? bg ????
				# x,y = (44,234) - (21,75)
				# img width, height = (88,10)
				# gap 33, 191
				energy_bg_image = ui.ExpandedImageBox()
				energy_bg_image.SetParent( self.GetChild("illustrated_window") )
				energy_bg_image.LoadImage( ROOT_PATH + "energy_bar_bg.sub" )
				energy_bg_image.SetPosition( 23 + (col* 88) + (col*33), 159 + (row*10) + (row*191) )
				energy_bg_image.Show()
				self.illustration_page.CardEnergyBGArray[row][col] = energy_bg_image
				## ???????? img ????
				energu_image = ui.ExpandedImageBox()
				energu_image.SetParent( energy_bg_image )
				energu_image.LoadImage( ROOT_PATH + "energy_bar.sub" )
				energu_image.SetPosition(1,1)
				energu_image.Show()
				self.illustration_page.CardEnergyImageArray[row][col] = energu_image
				
				## ???(star) ????
				for cnt in xrange(0, STAR_COUNT):
					star_off_image = ui.ExpandedImageBox()
					star_off_image.SetParent( self.GetChild("star_window" + str(row) + str(col) ) )
					star_off_image.LoadImage( ROOT_PATH + "star_bg.sub" )
					star_off_image.SetPosition( 16 * cnt , 0)
					star_off_image.Show()
					self.illustration_page.CardStarOffArray[row][col][cnt] = star_off_image
					
					star_on_image = ui.ExpandedImageBox()
					star_on_image.SetParent( self.GetChild("star_window" + str(row) + str(col) ) )
					star_on_image.LoadImage( ROOT_PATH + "star_img.sub" )
					star_on_image.SetPosition( 16 * cnt , 0)
					star_on_image.Show()
					self.illustration_page.CardStarOnArray[row][col][cnt] = star_on_image
				#????????
				self.illustration_page.CardAreaImageArray[row][col] = self.GetChild( "illustrated_area" + str(row) + str(col) )
			
		## ???? ???
		self.illustration_page.promotion_button		= self.GetChild("promotion_button")
		## ??? ???
		self.illustration_page.exchange_button		= self.GetChild("exchange_button")
		## ??? ???
		self.illustration_page.motion_button		= self.GetChild("motion_button")
		## ???? ???
		self.illustration_page.poly_button			= self.GetChild("poly_button")
		## ??? ???
		self.illustration_page.warp_button			= self.GetChild("warp_button")
		## ??? ???
		self.illustration_page.summon_button		= self.GetChild("summon_button")
		
		## ??? ???
		self.illustration_page.mv_name_text			= self.GetChild("MV_name_text")
		## ??? ???? ???
		self.illustration_page.mv_reset_button		= self.GetChild( "mv_reset_button" )
		self.illustration_page.mv_reset_button.Hide()
		## ??? ??? ???
		self.illustration_page.left_rotation_button	= self.GetChild( "mv_left_rotation_button" )
		self.illustration_page.left_rotation_button.Hide()
		self.illustration_page.right_rotation_button= self.GetChild( "mv_right_rotation_button" )
		self.illustration_page.right_rotation_button.Hide()
		## ??? ???,??? ???
		self.illustration_page.zoomin_button		= self.GetChild( "mv_zoomin_button" )
		self.illustration_page.zoomin_button.Hide()
		self.illustration_page.zoomout_button		= self.GetChild( "mv_zoomout_button" )
		self.illustration_page.zoomout_button.Hide()
		## ??? ??,?? ???
		self.illustration_page.mv_up_button			= self.GetChild( "mv_up_camera_button" )
		self.illustration_page.mv_up_button.Hide()
		self.illustration_page.mv_down_button		= self.GetChild( "mv_down_camera_button" )
		self.illustration_page.mv_down_button.Hide()
		
		## ???? ??? ???
		self.illustration_page.mv_count_text		= self.GetChild("MV_countText")
		## page ???
		for button_index in range(ILLUSTRATION_PAGE_MAX):
			self.illustration_page.page_button_list[button_index] = self.GetChild( "page_button" + str(button_index) )
		
		self.illustration_page.first_prev_button	= self.GetChild( "first_prev_button" )	# <<
		self.illustration_page.prev_button			= self.GetChild( "prev_button" )		# <
		self.illustration_page.next_button			= self.GetChild( "next_button" )		# >
		self.illustration_page.last_next_button		= self.GetChild( "last_next_button" )	# >>
		
		## ????
		self.buttontooltip = uiToolTip.ToolTip()
		self.buttontooltip.ClearToolTip()
		
		
		
	def __BindEvent(self):
		##??? ??????########################################################################
		## ???? ??? ??? ?? ui. ?????,???????,???????
		if localeInfo.IsARABIC():
			for (tabKey, tabValue) in self.tabDict.items():
				tabValue.LeftRightReverse()
			
		for (tabKey, tabButton) in self.tabButtonDict.items():
			tabButton.SetEvent(ui.__mem_func__(self.__OnClickTabButton), tabKey)
			
			self.tabButtonDict["MISSION"].SetShowToolTipEvent(ui.__mem_func__(self.OverInToolTipButton), uiScriptLocale.MC_TAB_BUTTON_CARD_MISSION)
		self.tabButtonDict["MISSION"].SetHideToolTipEvent(ui.__mem_func__(self.OverOutToolTipButton))
		self.tabButtonDict["SOLO"].SetShowToolTipEvent(ui.__mem_func__(self.OverInToolTipButton), uiScriptLocale.MC_TAB_BUTTON_SOLO)
		self.tabButtonDict["SOLO"].SetHideToolTipEvent(ui.__mem_func__(self.OverOutToolTipButton))
		self.tabButtonDict["PARTY"].SetShowToolTipEvent(ui.__mem_func__(self.OverInToolTipButton), uiScriptLocale.MC_TAB_BUTTON_PARTY)
		self.tabButtonDict["PARTY"].SetHideToolTipEvent(ui.__mem_func__(self.OverOutToolTipButton))

		if self.tabButtonDict.has_key("ACHIEV"):
			if hasattr(uiScriptLocale, "MC_TAB_BUTTON_ACHIEV"):
				self.tabButtonDict["ACHIEV"].SetShowToolTipEvent(ui.__mem_func__(self.OverInToolTipButton), uiScriptLocale.MC_TAB_BUTTON_ACHIEV)
				self.tabButtonDict["ACHIEV"].SetHideToolTipEvent(ui.__mem_func__(self.OverOutToolTipButton))
			if self.achiev_page.field_button:
				self.achiev_page.field_button.SetEvent(ui.__mem_func__(self.__OnClickAchievType), 0)
			if self.achiev_page.dungeon_button:
				self.achiev_page.dungeon_button.SetEvent(ui.__mem_func__(self.__OnClickAchievType), 1)
			if self.achiev_page.change_view_button:
				self.achiev_page.change_view_button.SetEvent(ui.__mem_func__(self.__ToggleAchievView))
		if self.achiev_page.sort_list_window:
			self.achiev_page.sort_list_window.Hide()
		if self.achiev_page.sort_mouse_over_img:
			self.achiev_page.sort_mouse_over_img.Hide()
		if self.achiev_page.sort_list_button:
			self.achiev_page.sort_list_button.SetEvent(ui.__mem_func__(self.__ToggleAchievSortList))
			self.achiev_page.sort_list_button.SetShowToolTipEvent(ui.__mem_func__(self.OverInToolTipButton), uiScriptLocale.MC_ACHIEV_SORT_ALL if hasattr(uiScriptLocale, "MC_ACHIEV_SORT_ALL") else "")
			self.achiev_page.sort_list_button.SetHideToolTipEvent(ui.__mem_func__(self.OverOutToolTipButton))
		if self.achiev_page.sort_arrow_button:
			self.achiev_page.sort_arrow_button.SetEvent(ui.__mem_func__(self.__ToggleAchievSortList))
		if self.achiev_page.sort_all_button:
			self.achiev_page.sort_all_button.SetEvent(ui.__mem_func__(self.__SetAchievSortMode), 0)
		if self.achiev_page.sort_applying_button:
			self.achiev_page.sort_applying_button.SetEvent(ui.__mem_func__(self.__SetAchievSortMode), 1)
		if self.achiev_page.sort_regist_button:
			self.achiev_page.sort_regist_button.SetEvent(ui.__mem_func__(self.__SetAchievSortMode), 2)
		if self.achiev_page.sort_able_regist_button:
			self.achiev_page.sort_able_regist_button.SetEvent(ui.__mem_func__(self.__SetAchievSortMode), 3)
		if self.achiev_page.sort_disable_button:
			self.achiev_page.sort_disable_button.SetEvent(ui.__mem_func__(self.__SetAchievSortMode), 4)
		
		## ???? ????? ????
		for col in xrange(0,SELECTED_ARRAY_WIDTH):
			# ??????
			self.mission_page.selectedFrameArray[col].SetEvent(ui.__mem_func__(self.__SelectedImgOverIn), "mouse_over_in", col)
			self.mission_page.selectedFrameArray[col].SetEvent(ui.__mem_func__(self.__SelectedImgOverOut), "mouse_over_out", col)
			# ????????
			self.mission_page.seletedAreaImageArray[col].SetEvent(ui.__mem_func__(self.__EmergenceAreaOverIn), "mouse_over_in", col)
			self.mission_page.seletedAreaImageArray[col].SetEvent(ui.__mem_func__(self.__EmergenceAreaOverOut), "mouse_over_out", col)
			
		# ????? ???
		self.mission_page.recive_mission_button.SetEvent(ui.__mem_func__(self.__OnClickReciveMissionButton) )
		self.mission_page.recive_mission_button.SetShowToolTipEvent(ui.__mem_func__(self.OverInToolTipButton), uiScriptLocale.MC_REQUEST_MISSION_BUTTON)
		self.mission_page.recive_mission_button.SetHideToolTipEvent(ui.__mem_func__(self.OverOutToolTipButton))
		# ????? ???
		self.mission_page.shuffle_card_button.SetEvent(ui.__mem_func__(self.__OnClickShuffleCardButton) )
		self.mission_page.shuffle_card_button.SetShowToolTipEvent(ui.__mem_func__(self.OverInToolTipButton), uiScriptLocale.MC_SHUFFLE_BUTTON)
		self.mission_page.shuffle_card_button.SetHideToolTipEvent(ui.__mem_func__(self.OverOutToolTipButton))
		# ????? ???
		self.mission_page.reward_card_button.SetEvent(ui.__mem_func__(self.__OnClickRewardCardButton) )
		self.mission_page.reward_card_button.SetShowToolTipEvent(ui.__mem_func__(self.OverInToolTipButton), uiScriptLocale.MC_REWARD_CARD_BUTTON)
		self.mission_page.reward_card_button.SetHideToolTipEvent(ui.__mem_func__(self.OverOutToolTipButton))
		# ???? ???
		self.mission_page.mission_init_button.SetEvent(ui.__mem_func__(self.__OnClickMissionInitButton) )
		self.mission_page.mission_init_button.SetShowToolTipEvent(ui.__mem_func__(self.OverInToolTipButton), uiScriptLocale.MC_MISSION_INIT_BUTTON)
		self.mission_page.mission_init_button.SetHideToolTipEvent(ui.__mem_func__(self.OverOutToolTipButton))
		# ????? ???
		self.mission_page.init_question_button = self.GetChild("init_question_button")
		init_question_desclist = [localeInfo.MC_QUESTION_BUTTON_DESC1, localeInfo.MC_QUESTION_BUTTON_DESC2, localeInfo.MC_QUESTION_BUTTON_DESC3]
		self.mission_page.init_question_button.SetToolTipWindow( self.__CreateGameTypeToolTip("", init_question_desclist) )
		self.mission_page.init_question_button.SetEvent(ui.__mem_func__(self.__OnClickQuestionButton) )
		
		
		## ?????? ?????? ########################################################	
		## ???? ???
		self.illustration_page.promotion_button.SetEvent(ui.__mem_func__(self.__OnClickPromotionButton) )
		self.illustration_page.promotion_button.SetShowToolTipEvent(ui.__mem_func__(self.OverInToolTipButton), uiScriptLocale.MC_PROMOTION_BUTTON)
		self.illustration_page.promotion_button.SetHideToolTipEvent(ui.__mem_func__(self.OverOutToolTipButton))
		## ??? ???
		self.illustration_page.exchange_button.SetEvent(ui.__mem_func__(self.__OnClickExchangeButton) )
		self.illustration_page.exchange_button.SetShowToolTipEvent(ui.__mem_func__(self.OverInToolTipButton), uiScriptLocale.MC_EXCHANGE_BUTTON)
		self.illustration_page.exchange_button.SetHideToolTipEvent(ui.__mem_func__(self.OverOutToolTipButton))
		## ??? ???
		self.illustration_page.motion_button.SetEvent(ui.__mem_func__(self.__OnClickMotionButton) )
		self.illustration_page.motion_button.SetAlwaysToolTip(True)
		## ??? ????
		self.illustration_page.motion_button_tooltip = uiToolTip.ToolTip()
		self.illustration_page.motion_button_tooltip.ClearToolTip()
		self.illustration_page.motion_button_tooltip.SetThinBoardSize(11 * len(str(uiScriptLocale.MC_MOTION_BUTTON)))
		self.illustration_page.motion_button_tooltip.AppendTextLine(uiScriptLocale.MC_MOTION_BUTTON, 0xffffffff)
		motion_tooltip_list = [uiScriptLocale.MC_MOTION_BUTTON, localeInfo.MC_MOTION_BUTTON_OVER_MSG]
		self.illustration_page.motion_button_tooltip2 = self.__CreateGameTypeToolTip("", motion_tooltip_list)
		## ???? ???
		self.illustration_page.poly_button.SetEvent(ui.__mem_func__(self.__OnClickPolyButton) )
		self.illustration_page.poly_button.SetAlwaysToolTip(True)
		## ???? ????
		self.illustration_page.poly_button_tooltip = uiToolTip.ToolTip()
		self.illustration_page.poly_button_tooltip.ClearToolTip()
		self.illustration_page.poly_button_tooltip.SetThinBoardSize(11 * len(str(uiScriptLocale.MC_POLY_BUTTON)))
		self.illustration_page.poly_button_tooltip.AppendTextLine(uiScriptLocale.MC_POLY_BUTTON, 0xffffffff)
		poly_tooltip_list = [uiScriptLocale.MC_POLY_BUTTON, localeInfo.MC_POLY_BUTTON_OVER_MSG]
		self.illustration_page.poly_button_tooltip2 = self.__CreateGameTypeToolTip("", poly_tooltip_list)		
		## ??? ???
		self.illustration_page.warp_button.SetEvent(ui.__mem_func__(self.__OnClickWarpButton) )
		self.illustration_page.warp_button.SetAlwaysToolTip(True)
		## ??? ????
		self.illustration_page.warp_button_tooltip = uiToolTip.ToolTip()
		self.illustration_page.warp_button_tooltip.ClearToolTip()
		self.illustration_page.warp_button_tooltip.SetThinBoardSize(11 * len(str(uiScriptLocale.MC_WARP_BUTTON)))
		self.illustration_page.warp_button_tooltip.AppendTextLine(uiScriptLocale.MC_WARP_BUTTON, 0xffffffff)
		warp_tooltip_list = [uiScriptLocale.MC_WARP_BUTTON, localeInfo.MC_WARP_BUTTON_OVER_MSG]
		self.illustration_page.warp_button_tooltip2 = self.__CreateGameTypeToolTip("", warp_tooltip_list)
		## ??? ???
		self.illustration_page.summon_button.SetEvent(ui.__mem_func__(self.__OnClickSummonButton) )
		self.illustration_page.summon_button.SetShowToolTipEvent(ui.__mem_func__(self.OverInToolTipButton), uiScriptLocale.MC_SUMMON_BUTTON)
		self.illustration_page.summon_button.SetHideToolTipEvent(ui.__mem_func__(self.OverOutToolTipButton))		
		self.illustration_page.summon_button.SetAlwaysToolTip(True)
		
		## ??? ???? ???
		self.illustration_page.mv_reset_button.SetEvent(ui.__mem_func__(self.__ModelViewReset) )
		
		## page ???
		for button_index in range(ILLUSTRATION_PAGE_MAX):
			self.illustration_page.page_button_list[button_index].SetEvent(ui.__mem_func__(self.__OnClickPageButton), button_index)
		
		self.illustration_page.first_prev_button.SetEvent(ui.__mem_func__(self.__OnClickFirstPrevPageButton))
		self.illustration_page.prev_button.SetEvent(ui.__mem_func__(self.__OnClickPrevPageButton))
		self.illustration_page.next_button.SetEvent(ui.__mem_func__(self.__OnClickNextPageButton))
		self.illustration_page.last_next_button.SetEvent(ui.__mem_func__(self.__OnClickLastNextPageButton))
		
		if localeInfo.IsARABIC():
			temp_pos_list = [0 for col in range(0,ILLUSTRATION_PAGE_MAX)]
			for button_index in range(ILLUSTRATION_PAGE_MAX):
				temp_pos_list[button_index] = self.illustration_page.page_button_list[button_index].GetLocalPosition()
				
			for button_index in range(ILLUSTRATION_PAGE_MAX):
				x = temp_pos_list[ILLUSTRATION_PAGE_MAX -1 -button_index][0]
				y = temp_pos_list[ILLUSTRATION_PAGE_MAX -1 -button_index][1]
				self.illustration_page.page_button_list[button_index].SetPosition(x, y)
				
			temp_pos	= self.illustration_page.first_prev_button.GetLocalPosition()
			temp_pos2	= self.illustration_page.last_next_button.GetLocalPosition()
			self.illustration_page.first_prev_button.SetPosition( temp_pos2[0], temp_pos2[1])
			self.illustration_page.last_next_button.SetPosition( temp_pos[0], temp_pos[1])
			
			temp_pos	= self.illustration_page.prev_button.GetLocalPosition()
			temp_pos2	= self.illustration_page.next_button.GetLocalPosition()
			self.illustration_page.prev_button.SetPosition( temp_pos2[0], temp_pos2[1])
			self.illustration_page.next_button.SetPosition( temp_pos[0], temp_pos[1])
			
			self.illustration_page.first_prev_button.LeftRightReverse()
			self.illustration_page.prev_button.LeftRightReverse()
			self.illustration_page.next_button.LeftRightReverse()
			self.illustration_page.last_next_button.LeftRightReverse()
				
		for row in xrange(0, ILLUSTRATED_ARRAY_HEIGHT):
			for col in xrange(0, ILLUSTRATED_ARRAY_WIDTH):
				## ????
				self.illustration_page.CardImageArray[row][col].SetEvent(ui.__mem_func__(self.__IllustrationImgOverIn), "mouse_over_in", row, col)
				self.illustration_page.CardImageArray[row][col].SetEvent(ui.__mem_func__(self.__IllustrationImgOverOut), "mouse_over_out", row, col)				
				self.illustration_page.CardImageArray[row][col].SetEvent( ui.__mem_func__(self.__CardImgClick), "mouse_click", row, col)
				## ???? ????
				self.illustration_page.CardSelectImage[row][col].SetEvent(ui.__mem_func__(self.__IllustrationImgOverIn), "mouse_over_in", row, col)
				self.illustration_page.CardSelectImage[row][col].SetEvent(ui.__mem_func__(self.__IllustrationImgOverOut), "mouse_over_out", row, col)	
				self.illustration_page.CardSelectImage[row][col].SetEvent( ui.__mem_func__(self.__CardImgClick), "mouse_click", row, col)
				## ???? ????
				self.illustration_page.CardImageAlpha[row][col].SetEvent(ui.__mem_func__(self.__IllustrationImgOverIn), "mouse_over_in", row, col)
				self.illustration_page.CardImageAlpha[row][col].SetEvent(ui.__mem_func__(self.__IllustrationImgOverOut), "mouse_over_out", row, col)	
				self.illustration_page.CardImageAlpha[row][col].SetEvent( ui.__mem_func__(self.__CardImgClick), "mouse_click", row, col)
				## ????????
				self.illustration_page.CardAreaImageArray[row][col].SetEvent(ui.__mem_func__(self.__IllustrationEmergenceAreaOverIn), "mouse_over_in", row, col)
				self.illustration_page.CardAreaImageArray[row][col].SetEvent(ui.__mem_func__(self.__IllustrationEmergenceAreaOverOut), "mouse_over_out", row, col)	
		
				
	def __CreateGameTypeToolTip(self, title, descList):
		
		toolTip = uiToolTip.ToolTip()
		
		if title:
			toolTip.SetTitle(title)
			toolTip.AppendSpace(5)

		for desc in descList:
			toolTip.AutoAppendTextLine(desc)
			
		toolTip.AlignHorizonalCenter()
		toolTip.SetTop()
		return toolTip
	
	def __OnClickTabButton(self, tabKey):
		if self.mission_page.lock:
			return
			
		self.SetPage( tabKey )
		
	def SetPage(self, key):
	
		self.curKey = key
	
		for (tabKey, tabButton) in self.tabButtonDict.items():
			if tabKey != key:
				tabButton.SetUp()
				
		for tabMenuImg in self.tabDict.itervalues():
			tabMenuImg.Hide()
			
		for pageWindow in self.pageDict.itervalues():
			pageWindow.Hide()
			
		
		self.tabDict[key].Show()
		self.pageDict[key].Show()
		self.ShowPage()
		
	def ShowPage(self):
	
		if not self.IsShow():
			return
			
		if "MISSION" == self.curKey:
			self.__ShowMissionPage()
		if "SOLO" == self.curKey:
			self.ShowSoloPage()
		if "PARTY" == self.curKey:
			self.ShowPartyPage()
		if "ACHIEV" == self.curKey:
			self.__ShowAchievPage()

	def __ShowAchievPage(self):
		if hasattr(app, "ENABLE_MONSTER_CARD_ACHIEV") and not app.ENABLE_MONSTER_CARD_ACHIEV:
			return

		if not self.achiev_page.isLoaded:
			self.__LoadAchievData()
			self.__BuildAchievListUI()
			self.__RefreshAchievList()
			# default view: bonus list
			if self.achiev_page.model_window:
				self.achiev_page.model_window.Hide()
			if self.achiev_page.bonus_list_window:
				self.achiev_page.bonus_list_window.Show()
			self.achiev_page.show_model = False
			if self.achiev_page.bonus_scroll:
				self.achiev_page.bonus_scroll.SetScrollEvent(ui.__mem_func__(self.__OnAchievBonusScroll))
			self.achiev_page.isLoaded = True

	def __LoadAchievNameDict(self):
		for vnum, e in mckarty.D.OSIAGNIECIA.items():
			self.achiev_page.nameDict[vnum] = e["nazwa"]

	def __LoadAchievData(self):
		self.achiev_page.data = []
		self.__LoadAchievNameDict()
		for vnum in sorted(mckarty.D.OSIAGNIECIA.keys()):
			e = mckarty.D.OSIAGNIECIA[vnum]
			self.achiev_page.data.append({"vnum": vnum, "type": e["typ"], "rank_count": e["rangi"],
				"monster": list(e["potwory"]), "rank_bonus": list(e["bonusy"]), "apply_type": list(e["apply"])})

	def __GetAffectDict(self):
		if self.achiev_page.affectDict is not None:
			return self.achiev_page.affectDict
		self.achiev_page.affectDict = _AffectDict()
		return self.achiev_page.affectDict

	def __BuildAchievListUI(self):
		# Create 10 visible rows (like official) inside achiev_card_list_window
		if not self.achiev_page.list_window:
			return

		for it in self.achiev_page.listItems:
			try:
				it["wnd"].Hide()
			except:
				pass
		self.achiev_page.listItems = []

		ROW_H = 36
		for idx in xrange(10):
			wnd = ui.Window()
			wnd.SetParent(self.achiev_page.list_window)
			wnd.SetSize(476, ROW_H)
			wnd.SetPosition(0, idx * (ROW_H + 4))
			wnd.Show()

			bg = ui.ImageBox()
			bg.SetParent(wnd)
			bg.SetPosition(0, 0)
			bg.LoadImage("d:/ymir work/ui/game/monster_card/achiev/achiev_list_bar_img.sub")
			bg.Show()

			# status dot (green/red) will be swapped later
			status = ui.ImageBox()
			status.SetParent(wnd)
			status.SetPosition(35, 10)
			status.LoadImage("d:/ymir work/ui/game/monster_card/achiev/achiev_list_bar_new_img.sub")
			status.Hide()

			text = ui.TextLine()
			text.SetParent(wnd)
			text.SetPosition(50, 9)
			text.SetText("")
			text.Show()

			countText = ui.TextLine()
			countText.SetParent(wnd)
			countText.SetPosition(222, 10)
			countText.SetText("")
			countText.Show()

			btn = ui.Button()
			btn.SetParent(wnd)
			btn.SetPosition(6, 7)
			# Some packs only ship the default (+) button visual.
			btn.SetUpVisual("d:/ymir work/ui/game/monster_card/achiev/button/achiev_plus_default_button.sub")
			btn.SetOverVisual("d:/ymir work/ui/game/monster_card/achiev/button/achiev_plus_default_button.sub")
			btn.SetDownVisual("d:/ymir work/ui/game/monster_card/achiev/button/achiev_plus_default_button.sub")
			# Official: clicking "+" (Ayr??nt??lar) opens achievement detail panel.
			btn.SetEvent(ui.__mem_func__(self.__OnClickAchievDetail), idx)
			try:
				if hasattr(uiScriptLocale, "MC_ACHIEV_SHOW"):
					btn.SetAlwaysToolTip(True)
					btn.SetShowToolTipEvent(ui.__mem_func__(self.OverInToolTipButton), uiScriptLocale.MC_ACHIEV_SHOW)
					btn.SetHideToolTipEvent(ui.__mem_func__(self.OverOutToolTipButton))
			except:
				pass
			btn.Show()

			# action buttons (right side icons)
			btn_reg = ui.Button()
			btn_reg.SetParent(wnd)
			btn_reg.SetPosition(330, 4)
			btn_reg.SetUpVisual("d:/ymir work/ui/game/monster_card/achiev/button/achiev_regist_default_button.sub")
			btn_reg.SetOverVisual("d:/ymir work/ui/game/monster_card/achiev/button/achiev_regist_over_button.sub")
			btn_reg.SetDownVisual("d:/ymir work/ui/game/monster_card/achiev/button/achiev_regist_down_button.sub")
			btn_reg.SetEvent(ui.__mem_func__(self.__OnClickAchievRegist), idx)
			try:
				# List button: "Ba?ar?? talep et"
				if hasattr(uiScriptLocale, "MC_ACHIEV_REGIST"):
					btn_reg.SetAlwaysToolTip(True)
					btn_reg.SetShowToolTipEvent(ui.__mem_func__(self.OverInToolTipButton), uiScriptLocale.MC_ACHIEV_REGIST)
					btn_reg.SetHideToolTipEvent(ui.__mem_func__(self.OverOutToolTipButton))
			except:
				pass
			btn_reg.Show()

			btn_apply = ui.Button()
			btn_apply.SetParent(wnd)
			btn_apply.SetPosition(367, 4)
			# Middle icon: shows achievement bonus as tooltip (no click action).
			btn_apply.SetUpVisual("d:/ymir work/ui/game/monster_card/achiev/achiev_list_bar_reward_img.sub")
			btn_apply.SetOverVisual("d:/ymir work/ui/game/monster_card/achiev/achiev_list_bar_reward_img.sub")
			btn_apply.SetDownVisual("d:/ymir work/ui/game/monster_card/achiev/achiev_list_bar_reward_img.sub")
			# Tooltip-only (avoid sending apply packets on click).
			btn_apply.SetEvent(ui.__mem_func__(self.__OnClickAchievBonusNoop), idx)
			try:
				btn_apply.SetAlwaysToolTip(True)
				btn_apply.SetHideToolTipEvent(ui.__mem_func__(self.OverOutToolTipButton))
			except:
				pass
			btn_apply.Show()

			btn_star = ui.Button()
			btn_star.SetParent(wnd)
			btn_star.SetPosition(409, 4)
			# Rightmost icon: applying vs not applying (tooltip only).
			btn_star.SetUpVisual("d:/ymir work/ui/game/monster_card/achiev/achiev_list_bar_disable_img.sub")
			btn_star.SetOverVisual("d:/ymir work/ui/game/monster_card/achiev/achiev_list_bar_disable_img.sub")
			btn_star.SetDownVisual("d:/ymir work/ui/game/monster_card/achiev/achiev_list_bar_disable_img.sub")
			# State indicator only (avoid row selection side effects).
			btn_star.SetEvent(ui.__mem_func__(self.__OnClickAchievBonusNoop), idx)
			try:
				btn_star.SetAlwaysToolTip(True)
				btn_star.SetHideToolTipEvent(ui.__mem_func__(self.OverOutToolTipButton))
			except:
				pass
			btn_star.Show()

			self.achiev_page.listItems.append({
				"wnd": wnd,
				"bg": bg,
				"status": status,
				"text": text,
				"count": countText,
				"btn": btn,
				"btn_apply": btn_apply,
				"btn_reg": btn_reg,
				"btn_star": btn_star,
				"data_index": -1
			})

		if self.achiev_page.list_scroll:
			self.achiev_page.list_scroll.SetScrollEvent(ui.__mem_func__(self.__OnAchievScroll))

	def __OnClickAchievRow(self, localIndex):
		if localIndex < 0 or localIndex >= len(self.achiev_page.listItems):
			return
		data_index = self.achiev_page.listItems[localIndex]["data_index"]
		if data_index < 0:
			return
		self.achiev_page.cur_index = data_index
		self.__RefreshAchievRightPanel()

	def __OnClickAchievApply(self, localIndex):
		self.__OnClickAchievRow(localIndex)
		data_index = self.achiev_page.listItems[localIndex]["data_index"]
		data = self.__GetAchievFiltered()
		if data_index < 0 or data_index >= len(data):
			return
		vnum = data[data_index].get("vnum", 0)
		if not vnum:
			return
		net.SendChatPacket("/cardmonster 10 %d" % vnum)
		self.__RefreshAchievList()

	def __OnClickAchievRegist(self, localIndex):
		# List "Ba?ar?? talep et": official 2-step confirmation.
		self.__OnClickAchievRow(localIndex)
		data_index = self.achiev_page.listItems[localIndex]["data_index"]
		data = self.__GetAchievFiltered()
		if data_index < 0 or data_index >= len(data):
			return
		e = data[data_index]
		vnum = e.get("vnum", 0)
		if not vnum:
			return

		name = self.achiev_page.nameDict.get(vnum, "(%d)" % vnum)

		needCount = 0
		try:
			needCount = len(e.get("monster", []))
		except:
			needCount = 0

		# Default rank is determined by rank_count (same logic as detail window)
		rank_count = 0
		try:
			rank_count = int(e.get("rank_count", 0))
		except:
			rank_count = 0
		start_rank = 1
		if rank_count > 0 and rank_count < 5:
			start_rank = 6 - rank_count

		# Step 1: Ask confirmation using locale_game.txt (QUESTION_1 only)
		try:
			text1 = ""
			if hasattr(localeInfo, "MC_ACHIEV_REGIST_QUESTION_1"):
				text1 = localeInfo.MC_ACHIEV_REGIST_QUESTION_1 % (name, needCount)
			else:
				text1 = "%s" % name
			# Prevent text overflow by inserting a line break after the first question mark if present.
			try:
				qpos = text1.find("?")
				if qpos != -1 and qpos + 1 < len(text1):
					text1 = text1[:qpos + 1] + "\\n" + text1[qpos + 1:].lstrip()
			except:
				pass
			full = text1
		except:
			full = "%s" % name

		if self.question:
			try:
				self.question.Close()
			except:
				pass

		question = ExQuestionDialog("TOP_MOST")
		question.SetText(full)
		# Auto-size question dialog to prevent text overflow (official feel)
		try:
			(tw, th) = question.GetTextSize()
			w = max(340, min(620, int(tw) + 80))
			question.SetWidth(w)
		except:
			pass
		question.SetAcceptEvent(lambda v=vnum, r=start_rank: self.__OnAcceptAchievRegistStep1(v, r))
		question.SetCancelEvent(ui.__mem_func__(self.__CloseQuestionDialog))
		question.Open()
		self.question = question

	def __OnClickAchievDetail(self, localIndex):
		# "+" (Ayr??nt??lar): open achievement detail panel.
		self.__OnClickAchievRow(localIndex)
		data_index = self.achiev_page.listItems[localIndex]["data_index"]
		data = self.__GetAchievFiltered()
		if data_index < 0 or data_index >= len(data):
			return
		e = data[data_index]
		vnum = e.get("vnum", 0)
		if not vnum:
			return
		name = self.achiev_page.nameDict.get(vnum, "(%d)" % vnum)

		if self.achiev_page.detailWnd is None:
			self.achiev_page.detailWnd = MonsterCardAchievDetailWindow()
			self.achiev_page.detailWnd.onSelectRank = ui.__mem_func__(self.__OnSelectAchievRank)
			self.achiev_page.detailWnd.onToggleApply = ui.__mem_func__(self.__OnToggleAchievApplyFromDetail)
		regRank = player.GetMonsterCardAchievRegistRank(vnum)
		isApplying = player.IsMonsterCardAchievApplied(vnum)
		self.achiev_page.detailWnd.Open(vnum, name, e, regRank, isApplying)
		# Position detail window centered on Monster Card, slightly left/down.
		try:
			(x, y) = self.GetGlobalPosition()
			w = self.GetWidth()
			h = self.GetHeight()
			dw = self.achiev_page.detailWnd.GetWidth()
			dh = self.achiev_page.detailWnd.GetHeight()
			offx = -130
			offy = 25
			self.achiev_page.detailWnd.SetPosition(int(x + (w - dw) / 2 + offx), int(y + (h - dh) / 2 + offy))
		except:
			pass

	def __OnAcceptAchievRegistStep1(self, vnum, rank):
		# Step 2: QUESTION_2 only; if accepted, send packet.
		self.__CloseQuestionDialog()
		text2 = ""
		try:
			text2 = localeInfo.MC_ACHIEV_REGIST_QUESTION_2 if hasattr(localeInfo, "MC_ACHIEV_REGIST_QUESTION_2") else ""
		except:
			text2 = ""
		if not text2:
			# No second question provided, proceed directly.
			return self.__AcceptAchievRegist(vnum, rank)

		question = ExQuestionDialog("TOP_MOST")
		question.SetText(text2)
		# Auto-size dialog width for step 2 as well
		try:
			(tw, th) = question.GetTextSize()
			w = max(340, min(620, int(tw) + 80))
			question.SetWidth(w)
		except:
			pass
		question.SetAcceptEvent(lambda v=vnum, r=rank: self.__AcceptAchievRegist(v, r))
		question.SetCancelEvent(ui.__mem_func__(self.__CloseQuestionDialog))
		question.Open()
		self.question = question

	def __AcceptAchievRegist(self, vnum, rank):
		self.__CloseQuestionDialog()
		try:
			net.SendChatPacket("/cardmonster 11 %d %d" % (int(vnum), int(rank)))
		except:
			pass
		self.__RefreshAchievList()

	def __OnSelectAchievRank(self, vnum, rank):
		# Selection only; actual registration is confirmed via detail window "regist" button.
		try:
			if self.achiev_page.detailWnd:
				self.achiev_page.detailWnd.selectedRank = int(rank)
		except:
			pass
		self.__RefreshAchievList()

	def __OnToggleAchievApplyFromDetail(self, vnum):
		net.SendChatPacket("/cardmonster 10 %d" % vnum)
		self.__RefreshAchievList()

	def __OnClickAchievType(self, typ):
		if typ not in [0, 1]:
			return
		self.achiev_page.type = typ
		self.achiev_page.cur_index = 0
		self.__RefreshAchievList()

	def __ToggleAchievView(self):
		if not self.achiev_page.isLoaded:
			return
		self.achiev_page.show_model = not self.achiev_page.show_model
		if self.achiev_page.bonus_list_window and self.achiev_page.model_window:
			if self.achiev_page.show_model:
				self.achiev_page.bonus_list_window.Hide()
				self.achiev_page.model_window.Show()
			else:
				self.achiev_page.model_window.Hide()
				self.achiev_page.bonus_list_window.Show()

	def __ClearAchievBonusLines(self):
		for l in self.achiev_page.bonus_lines:
			try:
				l.Hide()
			except:
				pass
		self.achiev_page.bonus_lines = []

	def __ApplyAchievDungeonBonusLineLayout(self, t, rowIndex):
		if not self.achiev_page.bonus_list_window or not t:
			return
		listW = 240
		try:
			listW = self.achiev_page.bonus_list_window.GetWidth()
		except:
			pass
		if listW <= 0:
			listW = 240
		y = 95 + 16 * rowIndex
		try:
			# In this UI, center-align anchors around the element's X position.
			# So place the text at the window's midpoint and center-align it there.
			t.SetPosition(int(listW / 2), y)
			t.SetHorizontalAlignCenter()
			t.SetPackedFontColor(0xFFFFE3AD)
		except:
			pass

	def __EnsureAchievBonusLines(self, lineCount):
		if not self.achiev_page.bonus_list_window:
			return
		if lineCount < 0:
			lineCount = 0
		while len(self.achiev_page.bonus_lines) < lineCount:
			idx = len(self.achiev_page.bonus_lines)
			t = ui.TextLine()
			t.SetParent(self.achiev_page.bonus_list_window)
			t.SetText("")
			self.__ApplyAchievDungeonBonusLineLayout(t, idx)
			t.Show()
			self.achiev_page.bonus_lines.append(t)

	def __OnAchievBonusScroll(self):
		if not self.achiev_page.bonus_scroll:
			return
		self.achiev_page.bonus_scroll_pos = self.achiev_page.bonus_scroll.GetPos()
		self.__RefreshAchievBonusLines()

	def __RefreshAchievBonusLines(self):
		lines = self.achiev_page.bonus_line_data
		total = len(lines)
		visible = self.achiev_page.bonus_line_visible
		if visible <= 0:
			visible = len(self.achiev_page.bonus_lines)

		start = 0
		if total > visible:
			start = int(self.achiev_page.bonus_scroll_pos * (total - visible))

		for i in xrange(len(self.achiev_page.bonus_lines)):
			t = self.achiev_page.bonus_lines[i]
			li = start + i
			if li < total:
				t.SetText(lines[li])
				self.__ApplyAchievDungeonBonusLineLayout(t, i)
				t.Show()
			else:
				t.SetText("")
				t.Hide()

	def __OnAchievScroll(self):
		self.__RefreshAchievList()

	def __ToggleAchievSortList(self):
		if not self.achiev_page.sort_list_window:
			return
		if self.achiev_page.sort_list_window.IsShow():
			self.achiev_page.sort_list_window.Hide()
			if self.achiev_page.sort_mouse_over_img:
				self.achiev_page.sort_mouse_over_img.Hide()
		else:
			self.achiev_page.sort_list_window.Show()
			if self.achiev_page.sort_mouse_over_img:
				self.achiev_page.sort_mouse_over_img.Show()

	def __SetAchievSortMode(self, mode):
		try:
			mode = int(mode)
		except:
			return
		if mode < 0 or mode > 4:
			return
		self.achiev_page.sort_mode = mode
		self.achiev_page.cur_index = 0
		if self.achiev_page.sort_list_window:
			self.achiev_page.sort_list_window.Hide()
		if self.achiev_page.sort_mouse_over_img:
			self.achiev_page.sort_mouse_over_img.Hide()
		# Update button label (uses UI script button text field)
		if self.achiev_page.sort_list_button:
			try:
				label = ""
				if mode == 0 and hasattr(uiScriptLocale, "MC_ACHIEV_SORT_ALL"):
					label = uiScriptLocale.MC_ACHIEV_SORT_ALL
				elif mode == 1 and hasattr(uiScriptLocale, "MC_ACHIEV_SORT_APPLYING"):
					label = uiScriptLocale.MC_ACHIEV_SORT_APPLYING
				elif mode == 2 and hasattr(uiScriptLocale, "MC_ACHIEV_SORT_REGIST"):
					label = uiScriptLocale.MC_ACHIEV_SORT_REGIST
				elif mode == 3 and hasattr(uiScriptLocale, "MC_ACHIEV_SORT_ABLE_REGIST"):
					label = uiScriptLocale.MC_ACHIEV_SORT_ABLE_REGIST
				elif mode == 4 and hasattr(uiScriptLocale, "MC_ACHIEV_SORT_DISABLE"):
					label = uiScriptLocale.MC_ACHIEV_SORT_DISABLE
				if label:
					self.achiev_page.sort_list_button.SetText(label)
			except:
				pass
		self.__RefreshAchievList()

	def __GetAchievFiltered(self):
		out = []
		for e in self.achiev_page.data:
			if e.get("type", 0) == self.achiev_page.type:
				# Sort filter
				mode = self.achiev_page.sort_mode if hasattr(self.achiev_page, "sort_mode") else 0
				vnum = e.get("vnum", 0)
				isApplying = False
				regRank = 0
				try:
					isApplying = player.IsMonsterCardAchievApplied(vnum)
					regRank = player.GetMonsterCardAchievRegistRank(vnum)
				except:
					pass

				monsters = e.get("monster", [])
				done = 0
				totalNeed = len(monsters)
				for mv in monsters:
					ill = player.GetIllustrationData(mv)
					if ill:
						(accumulation_count, cur_count, cur_class, cooltime0, cooltime1) = ill
						if cur_class > 0:
							done += 1
				isAbleRegist = (totalNeed > 0 and done == totalNeed and regRank <= 0 and not isApplying)

				if mode == 0:
					out.append(e)
				elif mode == 1 and isApplying:
					out.append(e)
				elif mode == 2 and regRank > 0 and not isApplying:
					out.append(e)
				elif mode == 3 and isAbleRegist:
					out.append(e)
				elif mode == 4 and (not isApplying and regRank <= 0 and not isAbleRegist):
					out.append(e)
		# Official-like: in "All" view, show currently applied bonuses first.
		# Keep a stable order by vnum for ties.
		if mode == 0:
			try:
				def sort_key(e):
					v = e.get("vnum", 0)
					try:
						app = player.IsMonsterCardAchievApplied(v)
					except:
						app = False
					# applied first, then vnum asc
					return (0 if app else 1, int(v))
				out.sort(key=sort_key)
			except:
				pass
		return out

	def __GetAchievRankValue(self, e, rank):
		try:
			rank = int(rank)
		except:
			return 0
		if rank <= 0:
			return 0
		bonuses = e.get("rank_bonus", [])
		idx = rank - 1
		if idx < 0 or idx >= len(bonuses):
			return 0
		return bonuses[idx]

	def __FormatAchievBonusTooltip(self, e):
		if not e:
			return ""
		aff = self.__GetAffectDict()
		applyTypes = e.get("apply_type", [])
		if not applyTypes:
			return ""

		vnum = e.get("vnum", 0)
		regRank = 0
		try:
			regRank = int(player.GetMonsterCardAchievRegistRank(vnum))
		except:
			regRank = 0

		def aff_name(applyId, value):
			v = aff.get(applyId, "APPLY_%d" % applyId)
			# Some locale packs store tooltip texts as callables; prefer formatting with value.
			try:
				if callable(v):
					try:
						v = v(int(value))
					except:
						try:
							v = v()
						except:
							v = "APPLY_%d" % applyId
			except:
				pass
			# Never stringify a callable (would show "<function ...>").
			try:
				if callable(v):
					v = "APPLY_%d" % applyId
			except:
				v = "APPLY_%d" % applyId
			try:
				if v is None:
					return ""
				# Keep official "+" formatting if provided by locale.
				return str(v)
			except:
				return ""

		# Official list tooltip: show a single line.
		# - if registered: show registered rank bonus
		# - else: show 0 until conditions are met
		rank_count = 0
		try:
			rank_count = int(e.get("rank_count", 0))
		except:
			rank_count = 0
		start_rank = 1
		if rank_count > 0:
			start_rank = 6 - rank_count
			if start_rank < 1:
				start_rank = 1
			if start_rank > 5:
				start_rank = 5

		eligible = True
		if regRank <= 0:
			eligible = False
			try:
				monsters = e.get("monster", [])
				totalNeed = len(monsters)
				done = 0
				for mv in monsters:
					ill = player.GetIllustrationData(mv)
					if ill:
						(accumulation_count, cur_count, cur_class, cooltime0, cooltime1) = ill
						if cur_class > 0:
							done += 1
				eligible = (totalNeed > 0 and done == totalNeed)
			except:
				eligible = False

		r = regRank if regRank > 0 else start_rank
		val = self.__GetAchievRankValue(e, r) if (regRank > 0 or eligible) else 0
		name = aff_name(applyTypes[0], val)
		# Official: show "+<value>" (even 0) for flat bonuses.
		# If localized text already includes the value (common for % bonuses or "0 SungMa ..." style), don't append again.
		try:
			valStr = str(int(val))
			if valStr in name and ("+" in name or "%" in name or name.lstrip().startswith(valStr)):
				return "%s" % name
		except:
			pass
		return "%s +%d" % (name, int(val))

	def __OnClickAchievBonusNoop(self, *args):
		return

	def __RefreshAchievList(self):
		if not self.achiev_page.list_scroll:
			scroll = 0.0
		else:
			scroll = self.achiev_page.list_scroll.GetPos()

		data = self.__GetAchievFiltered()
		total = len(data)
		per = 10
		start = int(scroll * max(0, total - per))

		for i in xrange(len(self.achiev_page.listItems)):
			row = self.achiev_page.listItems[i]
			di = start + i
			if di < total:
				e = data[di]
				name = self.achiev_page.nameDict.get(e.get("vnum", 0), "(%d)" % e.get("vnum", 0))
				row["text"].SetText(name)
				# Progress: how many required monsters are at least stage 1 (illustration data comes from server ADD_MOB_INFO).
				monsters = e.get("monster", [])
				done = 0
				totalNeed = len(monsters)
				for mv in monsters:
					ill = player.GetIllustrationData(mv)
					if ill:
						(accumulation_count, cur_count, cur_class, cooltime0, cooltime1) = ill
						if cur_class > 0:
							done += 1
				if totalNeed > 0:
					# Official: show only current progress number (no "/total").
					row["count"].SetText("(%d)" % done)
				else:
					row["count"].SetText("(0)")

				# Place count text right after the name (official alignment).
				try:
					(nameX, nameY) = row["text"].GetLocalPosition()
					(tw, th) = row["text"].GetTextSize()
					row["count"].SetPosition(nameX + tw + 6, nameY + 1)
				except:
					pass
				vnum = e.get("vnum", 0)
				# Determine states once per row
				regRank = 0
				try:
					regRank = int(player.GetMonsterCardAchievRegistRank(vnum))
				except:
					regRank = 0
				isApplying = False
				try:
					isApplying = bool(player.IsMonsterCardAchievApplied(vnum))
				except:
					isApplying = False
				isAbleRegist = (totalNeed > 0 and done == totalNeed and regRank <= 0 and not isApplying)

				# status dot: only the small "new" marker (~11px). reward/applying subs are ~26px wide and
				# overlap the name at x=50 if shown here; those states are already shown on btn_apply / btn_star.
				try:
					if regRank > 0 or isApplying:
						row["status"].Hide()
					elif isAbleRegist:
						row["status"].LoadImage("d:/ymir work/ui/game/monster_card/achiev/achiev_list_bar_new_img.sub")
						row["status"].Show()
					else:
						row["status"].Hide()
				except:
					pass

				# "Ba?ar?? talep et" button should only be visible when achievable.
				try:
					if isAbleRegist:
						row["btn_reg"].Show()
					else:
						row["btn_reg"].Hide()
				except:
					pass

				# Dynamic layout: long achievement names must not overlap right-side icons.
				# Base positions come from __BuildAchievListUI(); we shift right controls based on text width.
				try:
					(nameX, nameY) = row["text"].GetLocalPosition()
					(tw, th) = row["text"].GetTextSize()
					(cw, ch) = row["count"].GetTextSize()
					# Reserve space for right icons (3 buttons + small gap)
					rightReserve = 150
					wndW = 476
					try:
						wndW = row["wnd"].GetWidth()
					except:
						pass
					contentEnd = nameX + tw + 6 + cw
					minLeft = max(240, wndW - rightReserve)
					shift = 0
					if contentEnd > minLeft:
						shift = int(contentEnd - minLeft)
					# Clamp to avoid pushing icons out of window
					maxShift = max(0, wndW - (minLeft + rightReserve))
					if shift > maxShift:
						shift = maxShift
					row["btn_reg"].SetPosition(330 + shift, 4)
					row["btn_apply"].SetPosition(367 + shift, 4)
					row["btn_star"].SetPosition(409 + shift, 4)
				except:
					pass

				# Middle bonus icon is tooltip-only; keep it visible in all states (official list shows the reward icon).
				try:
					row["btn_apply"].Show()
				except:
					pass

				row["data_index"] = di
				# Middle: bonus tooltip
				try:
					tip = self.__FormatAchievBonusTooltip(e)
					if tip:
						row["btn_apply"].SetShowToolTipEvent(ui.__mem_func__(self.OverInToolTipButton), tip)
					else:
						row["btn_apply"].SetShowToolTipEvent(ui.__mem_func__(self.OverInToolTipButton), "")
				except:
					pass
				# Right: state icon (3 purposes with different icons)
				try:
					# 1) No stage yet -> disable img
					# 2) Stage exists (eligible) or already applying -> applying img
					# 3) After request (regRank>0, not applying) -> turns into "apply" button icon
					if regRank > 0 and not isApplying:
						row["btn_star"].SetUpVisual("d:/ymir work/ui/game/monster_card/achiev/button/achiev_apply_default_button.sub")
						row["btn_star"].SetOverVisual("d:/ymir work/ui/game/monster_card/achiev/button/achiev_apply_over_button.sub")
						row["btn_star"].SetDownVisual("d:/ymir work/ui/game/monster_card/achiev/button/achiev_apply_down_button.sub")
						row["btn_star"].SetEvent(ui.__mem_func__(self.__OnClickAchievApply), i)
						if hasattr(uiScriptLocale, "MC_ACHIEV_APPLY"):
							row["btn_star"].SetShowToolTipEvent(ui.__mem_func__(self.OverInToolTipButton), uiScriptLocale.MC_ACHIEV_APPLY)
					elif isApplying or isAbleRegist:
						row["btn_star"].SetUpVisual("d:/ymir work/ui/game/monster_card/achiev/achiev_list_bar_applying_img.sub")
						row["btn_star"].SetOverVisual("d:/ymir work/ui/game/monster_card/achiev/achiev_list_bar_applying_img.sub")
						row["btn_star"].SetDownVisual("d:/ymir work/ui/game/monster_card/achiev/achiev_list_bar_applying_img.sub")
						row["btn_star"].SetEvent(ui.__mem_func__(self.__OnClickAchievBonusNoop), i)
						if hasattr(uiScriptLocale, "MC_ACHIEV_APPLYING"):
							row["btn_star"].SetShowToolTipEvent(ui.__mem_func__(self.OverInToolTipButton), uiScriptLocale.MC_ACHIEV_APPLYING)
					else:
						row["btn_star"].SetUpVisual("d:/ymir work/ui/game/monster_card/achiev/achiev_list_bar_disable_img.sub")
						row["btn_star"].SetOverVisual("d:/ymir work/ui/game/monster_card/achiev/achiev_list_bar_disable_img.sub")
						row["btn_star"].SetDownVisual("d:/ymir work/ui/game/monster_card/achiev/achiev_list_bar_disable_img.sub")
						row["btn_star"].SetEvent(ui.__mem_func__(self.__OnClickAchievBonusNoop), i)
						if hasattr(uiScriptLocale, "MC_ACHIEV_DISABLE"):
							row["btn_star"].SetShowToolTipEvent(ui.__mem_func__(self.OverInToolTipButton), uiScriptLocale.MC_ACHIEV_DISABLE)
				except:
					pass
				row["wnd"].Show()
			else:
				row["text"].SetText("")
				row["count"].SetText("")
				row["data_index"] = -1
				try:
					row["btn_apply"].SetShowToolTipEvent(ui.__mem_func__(self.OverInToolTipButton), "")
					row["btn_star"].SetShowToolTipEvent(ui.__mem_func__(self.OverInToolTipButton), "")
					row["btn_star"].SetUpVisual("d:/ymir work/ui/game/monster_card/achiev/achiev_list_bar_disable_img.sub")
					row["btn_star"].SetOverVisual("d:/ymir work/ui/game/monster_card/achiev/achiev_list_bar_disable_img.sub")
					row["btn_star"].SetDownVisual("d:/ymir work/ui/game/monster_card/achiev/achiev_list_bar_disable_img.sub")
				except:
					pass
				row["wnd"].Hide()

		self.__RefreshAchievRightPanel()

	def __RefreshAchievRightPanel(self):
		if not self.achiev_page.field_bonus_value:
			return

		# Right panel shows currently applied bonuses:
		# - Field: only one can be active at a time.
		# - Dungeon: all applied stack.
		aff = self.__GetAffectDict()

		# Header
		if self.achiev_page.bonus_bar_text:
			try:
				self.achiev_page.bonus_bar_text.SetText(uiScriptLocale.MONSTER_CARD_ACHIEV_BONUS if hasattr(uiScriptLocale, "MONSTER_CARD_ACHIEV_BONUS") else (uiScriptLocale.MC_TAB_BUTTON_ACHIEV if hasattr(uiScriptLocale, "MC_TAB_BUTTON_ACHIEV") else ""))
			except:
				pass

		noneText = uiScriptLocale.MC_ACHIEV_BONUS_NONE if hasattr(uiScriptLocale, "MC_ACHIEV_BONUS_NONE") else ""

		# Index by vnum for quick lookup
		byVnum = {}
		for e in self.achiev_page.data:
			byVnum[e.get("vnum", 0)] = e

		def buildAppliedLine(vnum):
			e = byVnum.get(vnum, None)
			if not e:
				return ""
			applyTypes = e.get("apply_type", [])
			if not applyTypes:
				return ""
			try:
				rank = player.GetMonsterCardAchievRegistRank(vnum)
			except:
				rank = 0
			val = self.__GetAchievRankValue(e, rank)
			# AFFECT_DICT can contain callables; resolve to localized text.
			rawAff = aff.get(applyTypes[0], "APPLY_%d" % applyTypes[0])
			affText = rawAff
			fromCallable = False
			try:
				if callable(rawAff):
					fromCallable = True
					try:
						affText = rawAff(int(val))
					except:
						try:
							affText = rawAff()
						except:
							affText = "APPLY_%d" % applyTypes[0]
							fromCallable = False
			except:
				pass
			try:
				if callable(affText):
					affText = "APPLY_%d" % applyTypes[0]
					fromCallable = False
			except:
				affText = "APPLY_%d" % applyTypes[0]
				fromCallable = False
			# Callables already embed the value (e.g. "Sald??r?? De?eri +100"); do not append " 100" again.
			if fromCallable:
				return affText if affText else ""
			if val:
				# Plain string template: append numeric value.
				try:
					valStr = str(int(val))
					if valStr in affText and ("+" in affText or "%" in affText or affText.lstrip().startswith(valStr)):
						return affText
				except:
					pass
				return "%s %d" % (affText, int(val))
			return affText

		# Field applied (type==0): pick first applied one
		fieldSummary = ""
		for vnum in byVnum.keys():
			e = byVnum.get(vnum)
			if not e or e.get("type", 0) != 0:
				continue
			try:
				if player.IsMonsterCardAchievApplied(vnum):
					fieldSummary = buildAppliedLine(vnum)
					break
			except:
				continue
		if not fieldSummary:
			fieldSummary = ""
		self.achiev_page.field_bonus_value.SetText(fieldSummary)

		# Dungeon applied list (type==1)
		lines = []
		for vnum in byVnum.keys():
			e = byVnum.get(vnum)
			if not e or e.get("type", 0) != 1:
				continue
			try:
				if not player.IsMonsterCardAchievApplied(vnum):
					continue
			except:
				continue
			s = buildAppliedLine(vnum)
			if s:
				lines.append(s)

		if not lines:
			lines = []

		visible = 18
		self.achiev_page.bonus_line_visible = visible
		self.achiev_page.bonus_line_data = lines
		self.__EnsureAchievBonusLines(visible)

		if self.achiev_page.bonus_scroll:
			try:
				# Official: keep scrollbar visible in Achievements panel.
				self.achiev_page.bonus_scroll.Show()
				if len(lines) <= visible:
					self.achiev_page.bonus_scroll_pos = 0.0
					try:
						self.achiev_page.bonus_scroll.SetPos(0.0)
					except:
						pass
			except:
				pass

		self.__RefreshAchievBonusLines()
	
	def OverInToolTipButton(self, btnText):
	
		if self.buttontooltip:
			texts = btnText.split('\\n')
			if texts[-1] == "":
				del texts[-1]
			lens = [len(text) for text in texts]
			text_max_len = max(lens) + 2
			
			pos_x, pos_y = wndMgr.GetMousePosition()
			
			self.buttontooltip.ClearToolTip()
			self.buttontooltip.SetThinBoardSize(11 * text_max_len)
			for text in texts:
				self.buttontooltip.AppendTextLine(text, 0xffffffff)
			self.buttontooltip.SetToolTipPosition(pos_x, pos_y - 20)
			self.buttontooltip.Show()
			self.buttontooltip.SetTop()
			self.ShowButtonToolTip = True

	def OverOutToolTipButton(self):
	
		if self.buttontooltip:
			self.buttontooltip.Hide()
			self.ShowButtonToolTip = False
			
	def ButtonToolTipProgress(self):
		if self.buttontooltip and self.ShowButtonToolTip:
			pos_x, pos_y = wndMgr.GetMousePosition()
			self.buttontooltip.SetToolTipPosition(pos_x, pos_y - 20)
			
	## Update
	def OnUpdate(self):
	
		self.ButtonToolTipProgress()
		
		self.__ModelUpDownCameraProgress()
		self.__ModelRotationProgress()
		self.__ModelZoomProgress()

		# Mission UI auto-refresh:
		# After completing one target, the server updates mission info; refresh without relog/reopen.
		try:
			now = app.GetGlobalTimeStamp()
			if not hasattr(self.mission_page, "last_mission_poll"):
				self.mission_page.last_mission_poll = 0
			# Pull updated mission info from server periodically while Mission tab is open.
			try:
				if not hasattr(self.mission_page, "last_mission_request"):
					self.mission_page.last_mission_request = 0
				if self.pageDict and self.pageDict.has_key("MISSION") and self.pageDict["MISSION"].IsShow():
					if now - self.mission_page.last_mission_request >= 1:
						self.mission_page.last_mission_request = now
						if not getattr(self.mission_page, "lock", False) and len(getattr(self.mission_page, "card_move_queue", [])) == 0:
							net.SendChatPacket("/cardmonster %d" % net.REQUEST_MISSION)
			except:
				pass
			
			if now - self.mission_page.last_mission_poll >= 1:
				self.mission_page.last_mission_poll = now
				cur = player.GetMonsterCardMissionInfo()
				if cur:
					stage = cur[MISSION_INDEX_STAGE]
					vnums = tuple(cur[MISSION_INDEX_MOB_VNUM])
					clears = tuple(cur[MISSION_INDEX_MOB_CLEAR])
					snap = (stage, vnums, clears)
					if getattr(self.mission_page, "last_mission_snap", None) != snap:
						self.mission_page.last_mission_snap = snap
						# Avoid fighting with the receive animation lock/move queue.
						if not getattr(self.mission_page, "lock", False) and len(getattr(self.mission_page, "card_move_queue", [])) == 0:
							self.RefreshMissionPage()
		except:
			pass
			
		## ??? ??????? ????
		if len(self.mission_page.card_move_queue) > 0:
		
			if False == self.mission_page.move_img.GetMove():
				self.CardMoveStartEvent()
				
			else:
				(dst_index, dst_vnum) = self.mission_page.card_move_queue[0][1]
				(dstX, dstY) = self.mission_page.selectedArray[dst_index].GetGlobalPosition()
				self.mission_page.move_img.SetMovePosition(dstX, dstY)
			
	## ??? ?????? ########################################################
	## ???? ????
	def __RefreshMissionState(self):
			
		## mob_vnum ?? ????? 0 ??? ??? ??? ?? ????
		if False == any( self.mission_page.mission_data[MISSION_INDEX_MOB_VNUM] ):
			self.mission_page.mission_state		= MISSION_STATE_WAIT
			return
		
		## mission clear ?? ??? ????? ??????, ????? ??????
		if all( self.mission_page.mission_data[MISSION_INDEX_MOB_CLEAR] ):
			self.mission_page.mission_state	= MISSION_STATE_REWARD
		else:
			self.mission_page.mission_state	= MISSION_STATE_PROCEEDING
		
	def RefreshMissionPage(self):
		try:
			prev_state = self.mission_page.mission_state
		except:
			prev_state = None
		
		self.mission_page.mission_data = player.GetMonsterCardMissionInfo()
		
		if not self.mission_page.mission_data:
			print "if not self.mission_page.mission_data"
			return
			
		## ???? ????
		self.__RefreshMissionState()
		
		# If user just pressed "G?rev al??nd??", play card move animation (official behavior).
		try:
			if getattr(self.mission_page, "want_receive_anim", False):
				if prev_state == MISSION_STATE_WAIT and self.mission_page.mission_state in [MISSION_STATE_PROCEEDING, MISSION_STATE_REWARD]:
					self.mission_page.want_receive_anim = False
					self.ReciveMission()
					return
		except:
			pass
		
		## ?????? ???? ????
		if MISSION_STATE_WAIT == self.mission_page.mission_state:
			self.__SetMissionWait()
		elif MISSION_STATE_NONE == self.mission_page.mission_state:
			print "MISSION_STATE_NONE ????~~~~~~~~~"
		else: 
			# MISSION_STATE_PROCEEDING, MISSION_STATE_REWARD ???????
			self.__SetMissionProceeding()		
			
			
	## ??????( ??? ??? ?? )
	def __SetMissionWait(self):
		if MISSION_STATE_WAIT != self.mission_page.mission_state:
			return
			
		## ??? ????
		self.__MissionPageClear()
			
		if not self.mission_page.mission_data:
			print "if not self.mission_page.mission_data"
			return
			
		cur_stage = self.mission_page.mission_data[MISSION_INDEX_STAGE]
		if 0 == cur_stage:
			return

		if self.mission_page.alter_text:
			self.mission_page.alter_text.SetText(localeInfo.MC_ALTER_TEXT % (cur_stage))
		
		
		self.mission_page.mission_tuple = player.GetMissionVec( cur_stage )
		if not self.mission_page.mission_tuple:
			return
		data_count = len( self.mission_page.mission_tuple )
		
		if 0 >= data_count or data_count > WAIT_ARRAY_HEIGHT*WAIT_ARRAY_WIDTH:
			return
		
		# type 0 : solo, type 1 : party
		#	(mob_vnum, mob_level, type, mapindex0, mapindex1,mapindex2) = data		
		for row in xrange(0, WAIT_ARRAY_HEIGHT):
			for col in xrange(0, WAIT_ARRAY_WIDTH):
				index = row * WAIT_ARRAY_WIDTH + col
				if index < data_count:
					(mob_vnum, mob_level, type, mapindex0, mapindex1,mapindex2) = self.mission_page.mission_tuple[index]
					
					if CARD_IMG_DICT.has_key(mob_vnum):
						self.mission_page.waitArray[row][col].LoadImage( CARD_IMG_DICT[mob_vnum] )
						self.mission_page.waitArray[row][col].SetScale( 0.75, 0.75 )
						self.mission_page.waitVnumDict[mob_vnum] = (row,col)
				else:
					self.mission_page.waitArray[row][col].LoadImage( CARD_IMG_DICT[0] )
					self.mission_page.waitArray[row][col].SetScale( 0.75, 0.75 )
					
		
	## ??????, ??????
	def __SetMissionProceeding(self):
		if not self.mission_page.mission_state in [MISSION_STATE_PROCEEDING, MISSION_STATE_REWARD]:
			return
				
		## ??? ????
		self.__MissionPageClear()
		
		if not self.mission_page.mission_data:
			print "if not self.mission_page.mission_data"
			return
		
		cur_stage = self.mission_page.mission_data[MISSION_INDEX_STAGE]
		if 0 == cur_stage:
			return
			
		## ??? ??? ???
		if self.mission_page.recive_mission_button:
			self.mission_page.recive_mission_button.DisableFlash()
			
		## ??? TEXT
		if self.mission_page.alter_text:
			self.mission_page.alter_text.SetText(localeInfo.MC_ALTER_TEXT % (cur_stage))
		
		#####  wait ???
		self.mission_page.mission_tuple = player.GetMissionVec( cur_stage )
		data_count = len( self.mission_page.mission_tuple )
		
		if 0 == data_count or data_count > WAIT_ARRAY_HEIGHT*WAIT_ARRAY_WIDTH:
			return
		
		## (mob_vnum, mob_level, type, mapindex0, mapindex1,mapindex2) = data
		for row in xrange(0, WAIT_ARRAY_HEIGHT):
			for col in xrange(0, WAIT_ARRAY_WIDTH):
				index = row * WAIT_ARRAY_WIDTH + col
				if index < data_count:
					(mob_vnum, mob_level, type, mapindex0, mapindex1,mapindex2) = self.mission_page.mission_tuple[index]
					
					if CARD_IMG_DICT.has_key(mob_vnum):
						if mob_vnum in self.mission_page.mission_data[MISSION_INDEX_MOB_VNUM]:
							self.mission_page.waitArray[row][col].LoadImage( CARD_IMG_DICT[0] )
							self.mission_page.waitArray[row][col].SetScale( 0.75, 0.75 )
						else:
							self.mission_page.waitArray[row][col].LoadImage( CARD_IMG_DICT[mob_vnum] )
							self.mission_page.waitArray[row][col].SetScale( 0.75, 0.75 )
							self.mission_page.waitVnumDict[mob_vnum] = (row,col)
				else:
					self.mission_page.waitArray[row][col].LoadImage( CARD_IMG_DICT[0] )
					self.mission_page.waitArray[row][col].SetScale( 0.75, 0.75 )
					
		if self.mission_page.wait_card_alpha:
			self.mission_page.wait_card_alpha.Show()
			
		
		#####  ????? ???
		for index in xrange(SELECTED_ARRAY_WIDTH):
			mob_vnum = self.mission_page.mission_data[MISSION_INDEX_MOB_VNUM][index]
			## ????
			self.mission_page.selectedArray[index].LoadImage( CARD_IMG_DICT[mob_vnum] )
			## ??? ?????
			if self.mission_page.mission_data[MISSION_INDEX_MOB_CLEAR][index]:
				self.mission_page.MissionClearImgArray[index].Show()
			else:
				self.mission_page.MissionClearImgArray[index].Hide()
			## ???
			mob_name = nonplayer.GetMonsterName(mob_vnum)
			self.mission_page.seletedMobNameArray[index] = mob_name
			## ????????
			area_text=""
			area_indexs = player.GetMobEmergenceAreaIndex(mob_vnum)
			if area_indexs:
				for map_index in area_indexs:
					if 0 == map_index:
						continue
					if localeInfo.MINIMAP_ZONE_NAME_DICT_BY_IDX.has_key(map_index):
						area_text += localeInfo.MINIMAP_ZONE_NAME_DICT_BY_IDX[map_index]
						area_text += "\\n"
			self.mission_page.setectedAreaTextArray[index] = area_text
			
		
		## ?????? ??? ??????
		if all( self.mission_page.mission_data[MISSION_INDEX_MOB_CLEAR] ):
			self.mission_page.reward_card_button.EnableFlash()
			

	def __ShowMissionPage(self):
		
		IsLoad = player.IsMissionDataLoad()
		
		if not IsLoad:
			self.__MissionPageClear()
			net.SendChatPacket("/cardmonster %d" % net.REQUEST_MISSION)
			return
		
		# Data is already loaded; clear any UI-script default texts and refresh.
		self.RefreshMissionPage()
	
	def __MissionPageClear(self):
		## ??? ??? ???
		if self.mission_page.recive_mission_button:
			self.mission_page.recive_mission_button.EnableFlash()
			
		## ???? ??? ??????
		if self.mission_page.reward_card_button:
			self.mission_page.reward_card_button.DisableFlash()
		
		## ???? ??? vnum dict clear
		self.mission_page.waitVnumDict.clear()
		
		## ???? ??? ??? clear
		self.__SelectCellClear()
		## ??? ??? ??? clear
		self.__WaitCellClear()
		
		## Alter Text
		if self.mission_page.alter_text:
			self.mission_page.alter_text.SetText("")
		
	def __SelectCellClear(self):
		
		for col in xrange(0,SELECTED_ARRAY_WIDTH):
			# ????
			self.mission_page.selectedArray[col].LoadImage( CARD_IMG_DICT[0] )
			## ??? ?????
			self.mission_page.MissionClearImgArray[col].Hide()
			# ???
			self.mission_page.seletedMobNameArray[col] = None
			# ???????? Text
			self.mission_page.setectedAreaTextArray[col] = None
			
	def __WaitCellClear(self):
	
		if self.mission_page.wait_card_alpha:
			self.mission_page.wait_card_alpha.Hide()
		
		for row in xrange(0, WAIT_ARRAY_HEIGHT):
			for col in xrange(0, WAIT_ARRAY_WIDTH):
				self.mission_page.waitArray[row][col].LoadImage( CARD_IMG_DICT[0] )
				self.mission_page.waitArray[row][col].SetScale( 0.75, 0.75 )
				
	# ???????? over in
	def __EmergenceAreaOverIn(self, type, index):

		if index >= len(self.mission_page.setectedAreaTextArray):
			return
			
		area_text = self.mission_page.setectedAreaTextArray[index]
		if not area_text:
			return
		
		self.OverInToolTipButton( area_text )
	
	# ???????? over out
	def __EmergenceAreaOverOut(self, type, index):
		self.OverOutToolTipButton()
		
	## ???? over in
	def __SelectedImgOverIn(self, type, index):
		
		if index >= len(self.mission_page.seletedMobNameArray):
			return
			
		name = self.mission_page.seletedMobNameArray[index]
		if name:
			self.OverInToolTipButton(name)
		else:
			# (Custom empty-slot hint removed; keep official behavior)
			self.OverOutToolTipButton()
		
	## ???? over out
	def __SelectedImgOverOut(self, type, index):
		self.OverOutToolTipButton()
		
		
	#????? ??? On Click Event
	def __OnClickReciveMissionButton(self):
		if self.mission_page.lock:
			return
			
		if not self.mission_page.mission_data:
			return
			
		if MISSION_STATE_WAIT != self.mission_page.mission_state:
			return
		
		try:
			self.mission_page.want_receive_anim = True
		except:
			pass
		
		net.SendChatPacket("/cardmonster %d" % net.RECIVE_MISSION)
		
	#????? ??? On Click Event(????)
	def __OnClickShuffleCardButton(self):
		if self.mission_page.lock:
			return
			
		if self.question:
			self.question.Close()
			
		question = ExQuestionDialog("TOP_MOST")
		question.SetText(localeInfo.MC_CARD_SHUFFLE)
		question.SetAcceptEvent( ui.__mem_func__(self.__ShuffleAccept) )
		question.SetCancelEvent( ui.__mem_func__(self.__CloseQuestionDialog) )
		question.Open()
		self.question = question
		
				
	def __ShuffleAccept(self):
	
		self.__CloseQuestionDialog()
		
		if not self.mission_page.mission_data:
			return

		# ????????? ???????			
		if not self.mission_page.mission_state in [MISSION_STATE_PROCEEDING, MISSION_STATE_REWARD]:
			self.MonsterCardMissionFail(FAILED_MSSION_COMMON_MSG, 0)
			return
		
		# ?? ???? 1? ???? ????
		# ??????? ??????? ???? ???? ?????.
		#if self.mission_page.mission_data[MISSION_INDEX_SHUFFLE_COUNT] >= SHUFFLE_MAX:
		#	self.MonsterCardMissionFail(FAILED_MSSION_COMMON_MSG, 0)
		#	return
			
		net.SendChatPacket("/cardmonster %d" % net.SHUFFLE_MISSION)
			
	#????? ??? On Click Event(????)
	def __OnClickRewardCardButton(self):
		if self.mission_page.lock:
			return
			
		if self.question:
			self.question.Close()
			
		question = ExQuestionDialog("TOP_MOST")
		question.SetText(localeInfo.MC_REWARD_MISSION)
		question.SetAcceptEvent( ui.__mem_func__(self.__RewardAccept) )
		question.SetCancelEvent( ui.__mem_func__(self.__CloseQuestionDialog) )
		question.Open()
		self.question = question
			
			
	def __RewardAccept(self):
		
		self.__CloseQuestionDialog()
		
		if not self.mission_page.mission_data:
			return
			
		if not self.mission_page.mission_state in [MISSION_STATE_REWARD]:
			self.MonsterCardMissionFail(FAILED_MISSION_REWARD_NO_CLEAR, 0)
			return
		
		# ????? ??? clear ?? ????
		if not all( self.mission_page.mission_data[MISSION_INDEX_MOB_CLEAR] ):
			self.MonsterCardMissionFail(FAILED_MISSION_REWARD_NO_CLEAR, 0)
			return
			
		net.SendChatPacket("/cardmonster %d" % net.REWARD_MISSION)
		
	#???? ??? On Click Event	
	def __OnClickMissionInitButton(self):
		if self.mission_page.lock:
			return
			
		if self.question:
			self.question.Close()
			
		question = ExQuestionDialog("TOP_MOST")
		question.SetText(localeInfo.MC_MISSION_INIT)
		question.SetAcceptEvent( ui.__mem_func__(self.__InitAccept) )
		question.SetCancelEvent( ui.__mem_func__(self.__CloseQuestionDialog) )
		question.Open()
		self.question = question
			
	def __InitAccept(self):
	
		self.__CloseQuestionDialog()
		
		if not self.mission_page.mission_data:
			return
			
		cur_stage = self.mission_page.mission_data[MISSION_INDEX_STAGE]
		if cur_stage <= 1 and self.mission_page.mission_state in [MISSION_STATE_NONE, MISSION_STATE_WAIT]:
			self.MonsterCardMissionFail(FAILED_MSSION_COMMON_MSG, 0)
			return
		
		# ??? ???? ????????? 6???? ?????? 7????????? ??? ?????
		# ??? ????????.
		#self.MonsterCardMissionFail(FAILED_MISSION_INIT_ITEM_FALL_SHORT, 3)
		#return
		
		net.SendChatPacket("/cardmonster %d" % net.INIT_MISSION)
		
	def __OnClickQuestionButton(self):
	
		if not self.mission_page.mission_data:
			return
			
		reset_time = self.mission_page.mission_data[MISSION_INDEX_RESET_TIME]
		
		if 0 == reset_time: 
			return
		
		curTime = app.GetGlobalTimeStamp()
		
		reset_time = max(0, reset_time + 86400 - curTime)
		reset_time_str = localeInfo.SecondToHM(reset_time)
		self.__OpenPopupDialog(localeInfo.MC_TIME % reset_time_str)
		
	## ????? ?????(mob_vnum 3?? ????????)
	def ReciveMission(self):
		## ?????? ????
		self.mission_page.mission_data = player.GetMonsterCardMissionInfo() 
		if not self.mission_page.mission_data:
			print "if not self.mission_page.mission_data"
			return
		
		## ???? ?????? ????
		self.mission_page.mission_state = MISSION_STATE_WAIT
		
		## Wait ??? ??????? ????
		self.__SetMissionWait()
		
		## ??? ??? ???
		if self.mission_page.recive_mission_button:
			self.mission_page.recive_mission_button.DisableFlash()

		for index in xrange(SELECTED_ARRAY_WIDTH):
			mob_vnum = self.mission_page.mission_data[MISSION_INDEX_MOB_VNUM][index]
			if not self.mission_page.waitVnumDict.has_key(mob_vnum):
				return
			src_index = self.mission_page.waitVnumDict[mob_vnum]
			self.__InsertMoveCard( (src_index, mob_vnum),(index, mob_vnum) )
		
		self.mission_page.lock = True
		
		## ???? ????
		self.__RefreshMissionState()

	def CardMoveStartEvent(self):
				
		if len(self.mission_page.card_move_queue) > 0:
			(src_index, src_vnum) = self.mission_page.card_move_queue[0][0]
			(dst_index, dst_vnum) = self.mission_page.card_move_queue[0][1]
			
			## Wait Image Clear
			wait_image = self.mission_page.waitArray[src_index[0]][src_index[1]]
			wait_image.LoadImage( CARD_IMG_DICT[0] )
			origin_width	= wait_image.GetWidth()
			origin_height	= wait_image.GetHeight()
			wait_image.SetScale( 0.75, 0.75 )
			
			## ????? ????
			self.mission_page.move_img.LoadImage( CARD_IMG_DICT[dst_vnum] )
			
			## ?????? ????
			(parent_x, parent_y) = self.pageDict["MISSION"].GetGlobalPosition()
			(left,top,width,height) = wait_image.GetRect()
			center_pos_x = left + width/2
			center_pos_y = top + height/2
			result_x = center_pos_x - origin_width/2 - parent_x
			result_y = center_pos_y - origin_height/2 - parent_y
			self.mission_page.move_img.SetPosition( result_x, result_y )
		
			## ?????? ????
			(dstX, dstY) = self.mission_page.selectedArray[dst_index].GetGlobalPosition()
			self.mission_page.move_img.SetMovePosition(dstX, dstY)
				
			## ????
			self.mission_page.move_img.Show()
			self.mission_page.move_img.MoveStart()
				
	def CardMoveEndEvnet(self):
		
		if len(self.mission_page.card_move_queue) > 0:
			[srcCard, dstCard] = self.mission_page.card_move_queue.popleft()
			(dst_index, dst_vnum) = dstCard
			
			## ????
			self.mission_page.selectedArray[dst_index].LoadImage( CARD_IMG_DICT[dst_vnum] )
			## ???
			mob_name = nonplayer.GetMonsterName(dst_vnum)
			self.mission_page.seletedMobNameArray[dst_index] = mob_name
			## ????????
			area_text=""
			area_indexs = player.GetMobEmergenceAreaIndex(dst_vnum)
			if area_indexs:
				for map_index in area_indexs:
					if 0 == map_index:
						continue
					if localeInfo.MINIMAP_ZONE_NAME_DICT_BY_IDX.has_key(map_index):
						area_text += localeInfo.MINIMAP_ZONE_NAME_DICT_BY_IDX[map_index]
						area_text += "\\n"
			self.mission_page.setectedAreaTextArray[dst_index] = area_text
				
			# move card hide
			self.mission_page.move_img.Hide()
			
			if len(self.mission_page.card_move_queue) == 0:
				self.mission_page.lock = False
				self.mission_page.wait_card_alpha.Show()
		
	def __InsertMoveCard(self, srcCard, dstCard):
		(src_index, src_vnum) = srcCard
		(dst_index, dst_vnum) = dstCard
		
		src_real_index = src_index[0] * WAIT_ARRAY_WIDTH + src_index[1]
		
		if src_real_index >= WAIT_ARRAY_WIDTH * WAIT_ARRAY_HEIGHT:
			return
			
		if not CARD_IMG_DICT.has_key(src_vnum):
			return
			
		if dst_index >= SELECTED_ARRAY_WIDTH:
			return
			
		if not CARD_IMG_DICT.has_key(dst_vnum):
			return
			
		self.mission_page.card_move_queue.append([srcCard,dstCard])
		
	def MonsterCardMissionFail(self, type, data):
		
		if FAILED_MISSION_SHUFFLE_NO_ITEM == type:
			self.__OpenPopupDialog(localeInfo.MC_SHUFFLE_NO_ITEM)
		elif FAILED_MISSION_INIT_ITEM_FALL_SHORT == type:
			self.__OpenPopupDialog( localeInfo.MC_INIT_ITEM_FALL_SHORT % (data) , True)
		elif FAILED_MISSION_REWARD_INVEN_FULL == type:
			self.__OpenPopupDialog(localeInfo.MC_REWARD_FAIL)
		elif FAILED_MISSION_REWARD_NO_CLEAR == type:
			self.__OpenPopupDialog(localeInfo.MC_MISSION_NO_CLEAR)
		elif FAILED_MSSION_COMMON_MSG == type:
			self.__OpenPopupDialog(localeInfo.MC_MISSION_FAIL_MSG)
		elif FAILED_MISSION_MSG_MAX == type:
			return
					
	def MonsterCardIllustrationFail(self, type, data):
	
		if FAILED_COUNT_MAX == type:
			self.__OpenPopupDialog( localeInfo.MC_USE_ITEM_FAIL )
		elif FAILED_POLY_COOLTIME == type:
			self.__OpenPopupDialog(localeInfo.MC_POLY_FAIL)
		elif FAILED_WARP_LIMIT_LEVEL == type:
			self.__OpenPopupDialog(localeInfo.MC_WARP_LIMIT_LEVEL)
		elif FAILED_WARP_TRADE == type:
			self.__OpenPopupDialog(localeInfo.MC_WARP_FAIL)

	# Called from client command parser: show cooldown seconds.
	def MonsterCardShowCooldown(self, seconds):
		try:
			sec = int(seconds)
		except:
			sec = 0
		if sec <= 0:
			return
		try:
			self.__OpenPopupDialog(localeInfo.MC_TIME % localeInfo.SecondToHM(sec))
		except:
			pass

	# Called from client command parser: insufficient star stage.
	def MonsterCardShowNeedStage(self, needStage):
		# No dedicated locale key in our pack; keep behavior simple.
		try:
			self.__OpenPopupDialog(localeInfo.MC_WARP_FAIL)
		except:
			pass
			
	## ??? ?
	def __OpenPopupDialog(self, msg, resize_width = False):
	
		if not self.popup:
			self.popup = ExPopupDialog("TOP_MOST")

		self.popup.SetText(msg)
		
		if resize_width:
			w,h = self.popup.GetTextSize()
			self.popup.SetWidth( w + 60 )
			
		self.popup.Open()
		
	## ???? ?
	def __CloseQuestionDialog(self):
		if self.question:
			self.question.Close()
			self.question = None
			
	## ?????? ?????? ########################################################
	
	def MonsterCardIllustrationRefresh(self):
		self.__ClearIllustrationButton()
		self.ShowPage()

	# Called from GameWindow via interfaceModule to refresh Achievements UI after server state changes.
	def MonsterCardAchievRefresh(self):
		try:
			if not hasattr(app, "ENABLE_MONSTER_CARD_ACHIEV") or app.ENABLE_MONSTER_CARD_ACHIEV:
				if self.achiev_page and getattr(self.achiev_page, "isLoaded", False):
					self.__RefreshAchievList()
		except:
			pass
			
	def ShowSoloPage(self):
		player.IllustrationShow( True )
		
		IsFileLoad = player.GetIllustrationFileLoad()
		if not IsFileLoad:
			return
		
		# Some clients don't expose IsIllustrationDataLoad; treat as not-loaded and request.
		if hasattr(player, "IsIllustrationDataLoad"):
			dataLoad = player.IsIllustrationDataLoad()
		else:
			dataLoad = 0
		if not dataLoad:
			net.SendChatPacket("/cardmonster 8 %d %d" % (net.REQUEST_ILLUSTRATION, 0))
			return
		
		if 0 == self.illustration_page.solo_page_max:
			self.illustration_page.solo_page_max = player.GetIllustrationSoloPageMax()
			## ????
			self.__ClearIllustrationPage()
		
		self.__ShowPageButton( self.illustration_page.solo_page_max , self.illustration_page.solo_cur_page )
		
	def ShowPartyPage(self):
		player.IllustrationShow( True )
		
		IsFileLoad = player.GetIllustrationFileLoad()
		if not IsFileLoad:
			return
			
		if hasattr(player, "IsIllustrationDataLoad"):
			dataLoad = player.IsIllustrationDataLoad()
		else:
			dataLoad = 0
		if not dataLoad:
			net.SendChatPacket("/cardmonster 8 %d %d" % (net.REQUEST_ILLUSTRATION, 0))
			return
			
		if 0 == self.illustration_page.party_page_max:
			self.illustration_page.party_page_max = player.GetIllustrationPartyPageMax()
			## ????
			self.__ClearIllustrationPage()
			
		self.__ShowPageButton( self.illustration_page.party_page_max , self.illustration_page.party_cur_page )
			
	## ?????? ??? ???,??? ???
	## ILLUSTRATION_PAGE_MAX : ?? ???????? ???????? ??? ???? ??? MAX
	## max_page : 4 -> 1,2,3,4
	## cur_page : 1,2,3,...
	def __ShowPageButton(self, max_page, cur_page):
		if not self.curKey in ["SOLO", "PARTY"]:
			return
			
		if 0 == max_page:
			return
		if cur_page > max_page:
			return
			
		if "SOLO" == self.curKey:
			if max_page > self.illustration_page.solo_page_max:
				return
				
			self.illustration_page.solo_cur_page = cur_page
		elif "PARTY" == self.curKey:
			if max_page > self.illustration_page.party_page_max:
				return
				
			self.illustration_page.party_cur_page = cur_page
		
		total_page_count	= max_page / ILLUSTRATION_PAGE_MAX		# 2 : 0,1,2
		last_page_btn_max	= max_page % ILLUSTRATION_PAGE_MAX		# 4 : 1,2,3,4
		
		cur_page_count	 = (cur_page-1) /  ILLUSTRATION_PAGE_MAX	# 2 : 0,1,2
		down_pos		 = (cur_page % ILLUSTRATION_PAGE_MAX) - 1
		
		btn_count_max = ILLUSTRATION_PAGE_MAX
		if cur_page_count == total_page_count:
			btn_count_max = last_page_btn_max
		
		for button_index in range(ILLUSTRATION_PAGE_MAX):
			self.illustration_page.page_button_list[button_index].Enable()
			self.illustration_page.page_button_list[button_index].SetUp()
			text_number = cur_page_count * ILLUSTRATION_PAGE_MAX + (button_index+1)
			self.illustration_page.page_button_list[button_index].SetText(str(text_number))
			if button_index < btn_count_max:
				self.illustration_page.page_button_list[button_index].Show()
			else:
				self.illustration_page.page_button_list[button_index].Hide()
				
		self.illustration_page.page_button_list[down_pos].Disable()
		self.illustration_page.page_button_list[down_pos].Down()
		
		## ??? ???????? ???? ?????? ????
		self.__ShowIllustrationPage( cur_page )
	
	def __ShowIllustrationPage(self, page):
	
		if not self.curKey in ["SOLO", "PARTY"]:
			return
		
		if "SOLO" == self.curKey:
			page_tuple = player.GetIllustrationSoloPageData( page )
			if not page_tuple:
				return
		elif "PARTY" == self.curKey:
			page_tuple = player.GetIllustrationPartyPageData( page )
			if not page_tuple:
				return
			
		data_count = len( page_tuple )
		if 0 >= data_count or data_count > (ILLUSTRATED_ARRAY_WIDTH * ILLUSTRATED_ARRAY_HEIGHT):
			return
			
		## ???
		if self.illustration_page.cur_model_vnum:
			ill_data = player.GetIllustrationData( self.illustration_page.cur_model_vnum )
			if ill_data:
				(accumulation_count, cur_count, cur_class, cooltime0, cooltime1) = ill_data
				
				# ???
				if self.illustration_page.motion_button:
					if ILLUSTRATION_MOTION_CALSS > cur_class:
						self.illustration_page.motion_button.Disable()
						self.illustration_page.motion_button.Down()
						self.illustration_page.motion_button.SetToolTipWindow( self.illustration_page.motion_button_tooltip2 )
					else:
						self.illustration_page.motion_button.Enable()
						self.illustration_page.motion_button.SetUp()
						self.illustration_page.motion_button.SetToolTipWindow( self.illustration_page.motion_button_tooltip )
				# ????
				if self.illustration_page.poly_button:
					if ILLUSTRATION_POLY_CLASS > cur_class:
						self.illustration_page.poly_button.Disable()
						self.illustration_page.poly_button.Down()
						self.illustration_page.poly_button.SetToolTipWindow( self.illustration_page.poly_button_tooltip2 )
					else:
						self.illustration_page.poly_button.Enable()
						self.illustration_page.poly_button.SetUp()
						self.illustration_page.poly_button.SetToolTipWindow( self.illustration_page.poly_button_tooltip )
				# ???
				if self.illustration_page.warp_button:
					if ILLUSTRATION_WARP_CLASS > cur_class:
						self.illustration_page.warp_button.Disable()
						self.illustration_page.warp_button.Down()
						self.illustration_page.warp_button.SetToolTipWindow( self.illustration_page.warp_button_tooltip2 )
					else:
						self.illustration_page.warp_button.Enable()
						self.illustration_page.warp_button.SetUp()
						self.illustration_page.warp_button.SetToolTipWindow( self.illustration_page.warp_button_tooltip )
		
		## ????
		for row in xrange(0, ILLUSTRATED_ARRAY_HEIGHT):
			for col in xrange(0, ILLUSTRATED_ARRAY_WIDTH):
				index = row * ILLUSTRATED_ARRAY_WIDTH + col
				
				if index < data_count:
					(mob_vnum, mob_level, type, mapindex0, mapindex1,mapindex2) = page_tuple[index]
					
					## Data
					self.illustration_page.CardData[row][col] = page_tuple[index]
					
					if CARD_IMG_DICT.has_key(mob_vnum):
						ill_data = player.GetIllustrationData(mob_vnum)
						
						accumulation_count = 0
						cur_count = 0
						cur_class = 0
						cooltime0 = 0
						cooltime1 = 0
						
						if ill_data:
							(accumulation_count, cur_count, cur_class, cooltime0, cooltime1) = ill_data

						## ????
						self.illustration_page.CardImageArray[row][col].LoadImage( CARD_IMG_DICT[mob_vnum] )
						self.illustration_page.CardImageArray[row][col].Show()
					
						## ???? ????
						if cur_class > 0:
							self.illustration_page.CardImageAlpha[row][col].Hide()
						else:
							self.illustration_page.CardImageAlpha[row][col].Show()
						
						## ???? ????
						if self.illustration_page.cur_model_vnum == mob_vnum:
							self.illustration_page.CardSelectImage[row][col].Show()
						else:
							self.illustration_page.CardSelectImage[row][col].Hide()
						
						## ???????? bg ????
						self.illustration_page.CardEnergyBGArray[row][col].Show()
						
						## ???????? img ????
						count_max = CLASS_COUNT_MAX[cur_class]
						self.illustration_page.CardEnergyImageArray[row][col].SetPercentage( cur_count, count_max )
						self.illustration_page.CardEnergyImageArray[row][col].Show()
						
						## ???? ??????
						if count_max <= cur_count:
							self.illustration_page.flushArray[row][col].ResetFrame()
							self.illustration_page.flushArray[row][col].Show()
						else:
							self.illustration_page.flushArray[row][col].Hide()
						
						## ???(star) ????
						for cnt in xrange(0, STAR_COUNT):
							if cnt < cur_class:
								self.illustration_page.CardStarOnArray[row][col][cnt].Show()
								self.illustration_page.CardStarOffArray[row][col][cnt].Hide()
							else:
								self.illustration_page.CardStarOnArray[row][col][cnt].Hide()
								self.illustration_page.CardStarOffArray[row][col][cnt].Show()
						
						# ???
						mob_name = nonplayer.GetMonsterName(mob_vnum)
						self.illustration_page.CardMobNameArray[row][col] = mob_name
						# ????????
						self.illustration_page.CardAreaImageArray[row][col].Show()
						area_text=""
						area_indexs = player.GetMobEmergenceAreaIndex(mob_vnum)
						if area_indexs:
							for map_index in area_indexs:
								if 0 == map_index:
									continue
								if localeInfo.MINIMAP_ZONE_NAME_DICT_BY_IDX.has_key(map_index):
									area_text += localeInfo.MINIMAP_ZONE_NAME_DICT_BY_IDX[map_index]
									area_text += "\\n"
						self.illustration_page.CardAreaTextArray[row][col] = area_text
						
					## ????? ?????? dict ?? ????.
					else:
						print "????? ???~~~~~ : ", mob_vnum
				## ????? ????.
				else:
					## Data
					self.illustration_page.CardData[row][col] = None
					
					## ????
					self.illustration_page.CardImageArray[row][col].LoadImage( CARD_IMG_DICT[0] )
					self.illustration_page.CardImageArray[row][col].Show()
				
					## ???? ????
					self.illustration_page.CardImageAlpha[row][col].Show()
					
					## ???? ????
					self.illustration_page.CardSelectImage[row][col].Hide()
							
					## ???? ??????
					self.illustration_page.flushArray[row][col].Hide()
			
					## ???????? bg ????
					self.illustration_page.CardEnergyBGArray[row][col].Hide()
					
					## ???????? img ????
					self.illustration_page.CardEnergyImageArray[row][col].Hide()
				
					## ???(star) ????
					for cnt in xrange(0, STAR_COUNT):
						self.illustration_page.CardStarOnArray[row][col][cnt].Hide()
						self.illustration_page.CardStarOffArray[row][col][cnt].Hide()
						
					# ???
					self.illustration_page.CardMobNameArray[row][col] = ""
						
					#????????
					self.illustration_page.CardAreaImageArray[row][col].Hide()
					self.illustration_page.CardAreaTextArray[row][col] = ""
					
					
	def __ClearIllustrationButton(self):
		# ???
		if self.illustration_page.motion_button:
			self.illustration_page.motion_button.Disable()
			self.illustration_page.motion_button.Down()
			self.illustration_page.motion_button_tooltip.Hide()
			self.illustration_page.motion_button_tooltip2.Hide()
			self.illustration_page.motion_button.SetToolTipWindow( self.illustration_page.motion_button_tooltip2 )
		# ????
		if self.illustration_page.poly_button:
			self.illustration_page.poly_button.Disable()
			self.illustration_page.poly_button.Down()
			self.illustration_page.poly_button_tooltip.Hide()
			self.illustration_page.poly_button_tooltip2.Hide()
			self.illustration_page.poly_button.SetToolTipWindow( self.illustration_page.poly_button_tooltip2 )
		# ???
		if self.illustration_page.warp_button:
			self.illustration_page.warp_button.Disable()
			self.illustration_page.warp_button.Down()
			self.illustration_page.warp_button_tooltip.Hide()
			self.illustration_page.warp_button_tooltip2.Hide()
			self.illustration_page.warp_button.SetToolTipWindow( self.illustration_page.warp_button_tooltip2 )
		# ???
		if self.illustration_page.summon_button:
			self.illustration_page.summon_button.Disable()
			self.illustration_page.summon_button.Down()
			
	## ??????? ????
	def __ClearIllustrationPage(self):
	
		## ??? ???
		if self.illustration_page.mv_name_text:
			self.illustration_page.mv_name_text.SetText("")
		## ???? ??? ???
		if self.illustration_page.mv_count_text:
			self.illustration_page.mv_count_text.SetText("")
		
		## ???
		self.__ClearIllustrationButton()

		## ????
		for row in xrange(0, ILLUSTRATED_ARRAY_HEIGHT):
			for col in xrange(0, ILLUSTRATED_ARRAY_WIDTH):
				## Data
				self.illustration_page.CardData[row][col] = None
				
				## ????
				self.illustration_page.CardImageArray[row][col].LoadImage( CARD_IMG_DICT[0] )
				self.illustration_page.CardImageArray[row][col].Show()
				## ???? ????
				self.illustration_page.CardSelectImage[row][col].Hide()
				## ???? ????
				self.illustration_page.CardImageAlpha[row][col].Show()
				## ??????
				self.illustration_page.flushArray[row][col].Hide()
				## ???????? bg ????
				self.illustration_page.CardEnergyBGArray[row][col].Hide()
				
				## ???????? img ????
				self.illustration_page.CardEnergyImageArray[row][col].Hide()
				
				## ???(star) ????
				for cnt in xrange(0, STAR_COUNT):
					self.illustration_page.CardStarOnArray[row][col][cnt].Hide()
					self.illustration_page.CardStarOffArray[row][col][cnt].Hide()
				
				# ???	
				self.illustration_page.CardMobNameArray[row][col] = ""
				
				#????????
				self.illustration_page.CardAreaImageArray[row][col].Hide()
				self.illustration_page.CardAreaTextArray[row][col] = ""
	
	
	## ????, ???? ???? ???
	def __CardImgClick(self, type, row, col):
			
		data = self.illustration_page.CardData[row][col]
		if not data:
			return
			
		(mob_vnum, mob_level, type, mapindex0, mapindex1,mapindex2) = data
		if mob_vnum == self.illustration_page.cur_model_vnum:
			return
			
		accumulation_count = 0
		cur_count = 0
		cur_class = 0
		cooltime0 = 0
		cooltime1 = 0
		
		ill_data = player.GetIllustrationData(mob_vnum)
		if ill_data:
			(accumulation_count, cur_count, cur_class, cooltime0, cooltime1) = ill_data
			
		if self.illustration_page.mv_count_text:
			self.illustration_page.mv_count_text.SetText( localeInfo.MC_ACCUMULATION_COUNT % (accumulation_count) )
			
		if self.illustration_page.mv_name_text:
			mob_name = nonplayer.GetMonsterName(mob_vnum)
			self.illustration_page.mv_name_text.SetText( mob_name )
			
		## ?????? mob vnum ????
		self.illustration_page.cur_model_vnum		= mob_vnum
		self.illustration_page.cur_data				= data
		self.illustration_page.cur_model_rotation	= 0.0
		
		if ILLUSTRATION_MODEL_RENDER <= cur_class:
			player.IllustrationSelectModel( mob_vnum )
		else:
			player.IllustrationSelectModel( 0xFFFFFFFF )
		
		## ???? ???? ?????
		for _row in xrange(0, ILLUSTRATED_ARRAY_HEIGHT):
			for _col in xrange(0, ILLUSTRATED_ARRAY_WIDTH):
				self.illustration_page.CardSelectImage[_row][_col].Hide()
		self.illustration_page.CardSelectImage[row][col].Show()
		
		## ???
		if self.illustration_page.mv_reset_button:
			self.illustration_page.mv_reset_button.Show()
		if self.illustration_page.left_rotation_button:
			self.illustration_page.left_rotation_button.Show()
		if self.illustration_page.right_rotation_button:
			self.illustration_page.right_rotation_button.Show()
		if self.illustration_page.zoomin_button:
			self.illustration_page.zoomin_button.Show()
		if self.illustration_page.zoomout_button:
			self.illustration_page.zoomout_button.Show()
		if self.illustration_page.mv_up_button:
			self.illustration_page.mv_up_button.Show()
		if self.illustration_page.mv_down_button:
			self.illustration_page.mv_down_button.Show()	
			
		# ???
		if self.illustration_page.motion_button:	
			
			if ILLUSTRATION_MOTION_CALSS > cur_class:
				self.illustration_page.motion_button.Disable()
				self.illustration_page.motion_button.Down()
				self.illustration_page.motion_button.SetToolTipWindow( self.illustration_page.motion_button_tooltip2 )
			else:
				self.illustration_page.motion_button.Enable()
				self.illustration_page.motion_button.SetUp()
				self.illustration_page.motion_button.SetToolTipWindow( self.illustration_page.motion_button_tooltip )
				
		# ????
		if self.illustration_page.poly_button:	
			
			if ILLUSTRATION_POLY_CLASS > cur_class:
				self.illustration_page.poly_button.Disable()
				self.illustration_page.poly_button.Down()
				self.illustration_page.poly_button.SetToolTipWindow( self.illustration_page.poly_button_tooltip2 )
			else:
				self.illustration_page.poly_button.Enable()
				self.illustration_page.poly_button.SetUp()
				self.illustration_page.poly_button.SetToolTipWindow( self.illustration_page.poly_button_tooltip )
		# ???
		if self.illustration_page.warp_button:
			
			if ILLUSTRATION_WARP_CLASS > cur_class:
				self.illustration_page.warp_button.Disable()
				self.illustration_page.warp_button.Down()
				self.illustration_page.warp_button.SetToolTipWindow( self.illustration_page.warp_button_tooltip2 )
			else:
				self.illustration_page.warp_button.Enable()
				self.illustration_page.warp_button.SetUp()
				self.illustration_page.warp_button.SetToolTipWindow( self.illustration_page.warp_button_tooltip )
		# ???
		if self.illustration_page.summon_button:
			if ILLUSTRATION_SUMMON_CLASS > cur_class:
				self.illustration_page.summon_button.Disable()
				self.illustration_page.summon_button.Down()
			else:
				self.illustration_page.summon_button.Enable()
				self.illustration_page.summon_button.SetUp()
		
	# ???????? over in
	def __IllustrationEmergenceAreaOverIn(self, type, row, col):
			
		area_text = self.illustration_page.CardAreaTextArray[row][col]
		if not area_text:
			return
		
		self.OverInToolTipButton( area_text )
	
	# ???????? over out
	def __IllustrationEmergenceAreaOverOut(self, type, row, col):
		self.OverOutToolTipButton()
		
		
	def __OnClickPageButton(self, index):
		
		if "SOLO" == self.curKey:
			page_max		= self.illustration_page.solo_page_max
			temp_page		= self.illustration_page.solo_cur_page
		elif "PARTY" == self.curKey:
			page_max		= self.illustration_page.party_page_max
			temp_page		= self.illustration_page.party_cur_page
		else:
			return
			
		cur_page_count	= (temp_page-1) /  ILLUSTRATION_PAGE_MAX
		cur_page		= cur_page_count * ILLUSTRATION_PAGE_MAX + (index+1)
		self.__ShowPageButton( page_max, cur_page )
		
		
	def __OnClickFirstPrevPageButton(self):
		if "SOLO" == self.curKey:
			page_max		= self.illustration_page.solo_page_max
			temp_page		= self.illustration_page.solo_cur_page
		elif "PARTY" == self.curKey:
			page_max		= self.illustration_page.party_page_max
			temp_page		= self.illustration_page.party_cur_page
		else:
			return
			
		temp_page_count = temp_page - ILLUSTRATION_PAGE_MAX
		temp_page_count = max( [1, temp_page_count] )
		cur_page_count	= (temp_page_count-1) /  ILLUSTRATION_PAGE_MAX
		cur_page		= cur_page_count * ILLUSTRATION_PAGE_MAX + 1
		self.__ShowPageButton( page_max, cur_page )
		
	def __OnClickPrevPageButton(self):
		if "SOLO" == self.curKey:
			page_max		= self.illustration_page.solo_page_max
			temp_page		= self.illustration_page.solo_cur_page
		elif "PARTY" == self.curKey:
			page_max		= self.illustration_page.party_page_max
			temp_page		= self.illustration_page.party_cur_page
		else:
			return
			
		cur_page = max( [1, temp_page - 1] )
		self.__ShowPageButton( page_max, cur_page )
		
	def __OnClickNextPageButton(self):
		if "SOLO" == self.curKey:
			page_max		= self.illustration_page.solo_page_max
			temp_page		= self.illustration_page.solo_cur_page
		elif "PARTY" == self.curKey:
			page_max		= self.illustration_page.party_page_max
			temp_page		= self.illustration_page.party_cur_page
		else:
			return
			
		cur_page = min( [page_max, temp_page + 1] )
		self.__ShowPageButton( page_max, cur_page )
		
	def __OnClickLastNextPageButton(self):
		if "SOLO" == self.curKey:
			page_max		= self.illustration_page.solo_page_max
			temp_page		= self.illustration_page.solo_cur_page
		elif "PARTY" == self.curKey:
			page_max		= self.illustration_page.party_page_max
			temp_page		= self.illustration_page.party_cur_page
		else:
			return
			
		temp_page_count = temp_page + ILLUSTRATION_PAGE_MAX
		temp_page_count = min( [page_max, temp_page_count] )
		cur_page_count	= (temp_page_count-1) /  ILLUSTRATION_PAGE_MAX
		cur_page		= cur_page_count * ILLUSTRATION_PAGE_MAX + 1
		
		if cur_page > temp_page:
			self.__ShowPageButton( page_max, cur_page )
		
	## ???? over in
	def __IllustrationImgOverIn(self, type, row, col):
		name = self.illustration_page.CardMobNameArray[row][col]
		if not name:
			return
			
		self.OverInToolTipButton( name )
		
	## ???? over out
	def __IllustrationImgOverOut(self, type, row, col):
		self.OverOutToolTipButton()
			
	def __OnClickPromotionButton(self):
		
		if self.question:
			self.question.Close()
			
		if not self.illustration_page.cur_data:
			return
			
		(mob_vnum, mob_level, type, mapindex0, mapindex1,mapindex2) = self.illustration_page.cur_data
		accumulation_count = 0
		cur_count = 0
		cur_class = 0
		cooltime0 = 0
		cooltime1 = 0
		
		ill_data = player.GetIllustrationData(mob_vnum)
		if not ill_data:
			self.__OpenPopupDialog(localeInfo.MC_CARD_FALL_SHORT)
			return
		
		(accumulation_count, cur_count, cur_class, cooltime0, cooltime1) = ill_data
		count_max = CLASS_COUNT_MAX[cur_class]
		
		if cur_count != count_max:
			self.__OpenPopupDialog(localeInfo.MC_CARD_FALL_SHORT)
			return
		if cur_class == STAR_COUNT and cur_count == count_max:
			self.__OpenPopupDialog(localeInfo.MC_PROMOTION_MAX)
			return
			
		question = ExQuestionDialog("TOP_MOST")
		question.SetText(localeInfo.MC_PROMOTION_QUESTION)
		question.SetAcceptEvent( lambda arg = mob_vnum : self.__PromotionAccept(arg) )
		question.SetCancelEvent( ui.__mem_func__(self.__CloseQuestionDialog) )
		question.Open()
		self.question = question
	
	def __PromotionAccept(self, mob_vnum):
		net.SendChatPacket("/cardmonster 8 %d %d" % (net.MC_PROMOTION, mob_vnum))
		self.__CloseQuestionDialog()
		
	def __OnClickExchangeButton(self):
		if self.question:
			self.question.Close()
			
		if not self.illustration_page.cur_data:
			return
			
		(mob_vnum, mob_level, type, mapindex0, mapindex1,mapindex2) = self.illustration_page.cur_data
		accumulation_count = 0
		cur_count = 0
		cur_class = 0
		cooltime0 = 0
		cooltime1 = 0
		
		ill_data = player.GetIllustrationData(mob_vnum)
		if not ill_data:
			self.__OpenPopupDialog(localeInfo.MC_CARD_FALL_SHORT)
			return
		
		(accumulation_count, cur_count, cur_class, cooltime0, cooltime1) = ill_data
		count_max = CLASS_COUNT_MAX[cur_class]
		
		if cur_count < TRADE_COUNT:
			self.__OpenPopupDialog(localeInfo.MC_CARD_FALL_SHORT)
			return
			
		question = ExQuestionDialog("TOP_MOST")
		question.SetText(localeInfo.MC_TRADE_QUESTION % TRADE_COUNT )
		question.SetAcceptEvent( lambda arg = mob_vnum : self.__TradeAccept(arg) )
		question.SetCancelEvent( ui.__mem_func__(self.__CloseQuestionDialog) )
		(w,h) = question.GetTextSize()
		question.SetWidth(w+20)
		question.Open()
		self.question = question
	
	def __TradeAccept(self, mob_vnum):
		net.SendChatPacket("/cardmonster 8 %d %d" % (net.MC_TRADE, mob_vnum))
		self.__CloseQuestionDialog()
		
		
	def __OnClickMotionButton(self):		
		ill_data = player.GetIllustrationData( self.illustration_page.cur_model_vnum  )
		if not ill_data:
			return
			
		(accumulation_count, cur_count, cur_class, cooltime0, cooltime1) = ill_data
		if cur_class < ILLUSTRATION_MOTION_CALSS:
			return
						
		player.IllustrationChangeMotion( self.illustration_page.cur_model_vnum )
		
		
	def __OnClickPolyButton(self):
		if self.question:
			self.question.Close()
			
		if not self.illustration_page.cur_data:
			return
			
		(mob_vnum, mob_level, type, mapindex0, mapindex1,mapindex2) = self.illustration_page.cur_data
		accumulation_count = 0
		cur_count = 0
		cur_class = 0
		cooltime0 = 0
		cooltime1 = 0
		
		ill_data = player.GetIllustrationData(mob_vnum)
		if not ill_data:
			return
		
		(accumulation_count, cur_count, cur_class, cooltime0, cooltime1) = ill_data
		
		if cur_class < ILLUSTRATION_POLY_CLASS:
			self.__OpenPopupDialog(localeInfo.MC_POLY_FAIL)
			return
			
		curTime = app.GetGlobalTimeStamp()
		cooltime = max(0, cooltime0 - curTime)
		cooltime_str = localeInfo.SecondToHM(cooltime)
		if cooltime:
			self.__OpenPopupDialog( localeInfo.MC_TIME % cooltime_str )
			return
			
		question = ExQuestionDialog("TOP_MOST")
		question.SetText(localeInfo.MC_POLY_QUESTION )
		question.SetAcceptEvent( lambda arg = mob_vnum : self.__PolyAccept(arg) )
		question.SetCancelEvent( ui.__mem_func__(self.__CloseQuestionDialog) )
		question.Open()
		self.question = question
	
	def __PolyAccept(self, mob_vnum):
		net.SendChatPacket("/cardmonster 8 %d %d" % (net.MC_POLY, mob_vnum))
		self.__CloseQuestionDialog()
		
		
	def __OnClickWarpButton(self):
		if self.question:
			self.question.Close()
			
		if not self.illustration_page.cur_data:
			return
			
		(mob_vnum, mob_level, type, mapindex0, mapindex1,mapindex2) = self.illustration_page.cur_data
		accumulation_count = 0
		cur_count = 0
		cur_class = 0
		cooltime0 = 0
		cooltime1 = 0
		
		ill_data = player.GetIllustrationData(mob_vnum)
		if not ill_data:
			return
		
		(accumulation_count, cur_count, cur_class, cooltime0, cooltime1) = ill_data
		
		if cur_class < ILLUSTRATION_WARP_CLASS:
			self.__OpenPopupDialog(localeInfo.MC_WARP_FAIL)
			return
			
		curTime = app.GetGlobalTimeStamp()
		cooltime = max(0, cooltime1 - curTime)
		cooltime_str = localeInfo.SecondToHM(cooltime)
		if cooltime:
			self.__OpenPopupDialog( localeInfo.MC_TIME % cooltime_str )
			return
			
		question = ExQuestionDialog("TOP_MOST")
		question.SetText(localeInfo.MC_WARP_QUESTION )
		question.SetAcceptEvent( lambda arg = mob_vnum : self.__WarpAccept(arg) )
		question.SetCancelEvent( ui.__mem_func__(self.__CloseQuestionDialog) )
		question.Open()
		self.question = question
	
	def __WarpAccept(self, mob_vnum):
		net.SendChatPacket("/cardmonster 8 %d %d" % (net.MC_WARP, mob_vnum))
		self.__CloseQuestionDialog()
		
		
	def __OnClickSummonButton(self):
		if self.question:
			self.question.Close()
			
		if not self.illustration_page.cur_data:
			return
			
		(mob_vnum, mob_level, type, mapindex0, mapindex1,mapindex2) = self.illustration_page.cur_data
		ill_data = player.GetIllustrationData(mob_vnum)
		if not ill_data:
			return
			
		(accumulation_count, cur_count, cur_class, cooltime0, cooltime1) = ill_data
		if cur_class < ILLUSTRATION_SUMMON_CLASS:
			# No dedicated locale key in our pack for summon-fail; reuse warp fail style.
			self.__OpenPopupDialog(localeInfo.MC_WARP_FAIL)
			return
		
		question = ExQuestionDialog("TOP_MOST")
		question.SetText(localeInfo.MC_WARP_QUESTION)
		question.SetAcceptEvent( lambda arg = mob_vnum : self.__SummonAccept(arg) )
		question.SetCancelEvent( ui.__mem_func__(self.__CloseQuestionDialog) )
		question.Open()
		self.question = question
		
	def __SummonAccept(self, mob_vnum):
		try:
			net.SendChatPacket("/cardmonster 8 %d %d" % (net.MC_SPAWN, mob_vnum))
		except:
			pass
		self.__CloseQuestionDialog()
		
	## ?? ??,?? ????
	def __ModelUpDownCameraProgress(self):
	
		if self.illustration_page.mv_up_button:
			if self.illustration_page.mv_up_button.IsDown():
				player.IllustrationModelUpDown( True )
				
		if self.illustration_page.mv_down_button:
			if self.illustration_page.mv_down_button.IsDown():
				player.IllustrationModelUpDown( False )
			
	## ?? ???
	def __ModelRotationProgress(self):
	
		if self.illustration_page.left_rotation_button:
			if self.illustration_page.left_rotation_button.IsDown():
				self.illustration_page.cur_model_rotation -= 2
				player.IllustrationModelRotation( self.illustration_page.cur_model_rotation )
				
		if self.illustration_page.right_rotation_button:
			if self.illustration_page.right_rotation_button.IsDown():
				self.illustration_page.cur_model_rotation += 2
				player.IllustrationModelRotation( self.illustration_page.cur_model_rotation )
				
	## ?? ?? in/out			
	def __ModelZoomProgress(self):
	
		if self.illustration_page.zoomin_button:
			if self.illustration_page.zoomin_button.IsDown():
				player.IllustrationModelZoom( True )
				
		if self.illustration_page.zoomout_button:
			if self.illustration_page.zoomout_button.IsDown():
				player.IllustrationModelZoom( False )
				
	
	## ??? ????
	def __ModelViewReset(self):
		self.illustration_page.cur_model_rotation = 0.0
		player.IllustrationModelViewRes