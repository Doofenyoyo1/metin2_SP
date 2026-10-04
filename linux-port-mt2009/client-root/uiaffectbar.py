import ui
import chr
import player
import localeInfo
import constInfo
import uiToolTip
import grp
import app
import item
import net
import skill
import eventManager
import uiMiniMap
import flamewindPath
import itemshop_subscriptionwindow
import updateable
import mathUtils
from _weakref import proxy

EVENT_ARRANGE_AFFECT_LIST = "EVENT_ARRANGE_AFFECT_LIST" # args | width: number, height: number

# MT2009_PLUS_AUTOHUNT_TIME_ICON_V1: the server's affect of Auto Lowy's time.
AFFECT_AUTOHUNT_TIME = 560

AFFECT_SHOW_DATA = {
	chr.NEW_AFFECT_DS_SET : {
		"description" : "Bonus zestawu Smoczych Kamieni",
		"icon" : "icon/item/110000.tga",
	},
	# PREMIUM VIP
	chr.NEW_AFFECT_EXP_BONUS : {
		"description" : localeInfo.TOOLTIP_MALL_EXPBONUS_STATIC,
		"icon" : "d:/ymir work/ui/skill/common/affect/exp_bonus.sub",
	},
	chr.NEW_AFFECT_ITEM_BONUS : {
		"description" : localeInfo.TOOLTIP_MALL_ITEMBONUS_STATIC,
		"icon" : "d:/ymir work/ui/skill/common/affect/item_bonus.sub",
	},
	chr.NEW_AFFECT_GOLD_BONUS : {
		"description" : localeInfo.TOOLTIP_MALL_GOLDBONUS_STATIC,
		"icon" : "d:/ymir work/ui/skill/common/affect/gold_bonus.sub",
	},
	chr.NEW_AFFECT_SAFEBOX : {
		"description" : localeInfo.TOOLTIP_MALL_SAFEBOX,
		"icon" : "d:/ymir work/ui/skill/common/affect/safebox.sub",
	},
	chr.NEW_AFFECT_SHOP_PREMIUM : {
		"description" : localeInfo.TOOLTIP_MALL_SHOP_PREMIUM,
		"icon" : flamewindPath.GetAffect("shop_premium_affect"),
	},
	chr.NEW_AFFECT_FISH_MIND : {
		"description" : localeInfo.TOOLTIP_MALL_FISH_MIND,
		"icon" : "d:/ymir work/ui/skill/common/affect/fishmind.sub",
	},
	chr.NEW_AFFECT_MARRIAGE_FAST : {
		"description" : localeInfo.TOOLTIP_MALL_MARRIAGE_FAST,
		"icon" : "d:/ymir work/ui/skill/common/affect/marriage_fast.sub",
	},

	chr.NEW_AFFECT_AUTOLOOT : {
		"description" : localeInfo.TOOLTIP_MALL_AUTOLOOT,
		"icon" : "d:/ymir work/ui/skill/common/affect/autoloot.sub",
	},

	chr.NEW_AFFECT_SUBSCRIPTION : {
		"description" : "",
		"icon" : "icon/item/premium.tga",
	},

	# MT2009_PLUS_AUTOHUNT_TIME_ICON_V1: the time of "Auto Lowy (8h)" from the
	# ItemShop (autohunt_time.quest: affect 560, counted down only while the
	# character is in the game). With no entry here the affect had no icon and
	# the time left was nowhere; the tooltip now says it, as for any timed affect.
	AFFECT_AUTOHUNT_TIME : {
		"description" : "Auto £owy;Czas leci tylko w grze.",
		"icon" : "icon/item/31070.tga",
	},

	chr.AFFECT_FAST_PICKUP : {
		"description" : localeInfo.TOOLTIP_FAST_PICKUP,
		"icon" : flamewindPath.GetAffect("fast_pickup"),
	},

	# DEBUFFS
	chr.AFFECT_POISON : {
		"description" : localeInfo.SKILL_TOXICDIE,
		"icon" : "d:/ymir work/ui/skill/common/affect/poison.sub",
	},
	chr.AFFECT_FIRE : {
		"description" : localeInfo.SKILL_FIRE,
		"icon" : "d:/ymir work/ui/skill/sura/hwayeom_03.sub",
	},
	chr.AFFECT_SLOW : {
		"description" : localeInfo.SKILL_SLOW,
		"icon" : "d:/ymir work/ui/skill/common/affect/slow.sub",
	},
	chr.AFFECT_STUN : {
		"description" : localeInfo.SKILL_STUN,
		"icon" : "d:/ymir work/ui/skill/common/affect/stun.sub",
	},
	chr.AFFECT_ROOT : {
		"description" : localeInfo.SKILL_ROOT,
		"icon" : flamewindPath.GetAffect("root"),
	},
	chr.AFFECT_HEAL_ON_DAMAGE : {
		"description" : localeInfo.SKILL_HEAL_ON_ATTACK,
		"icon" : flamewindPath.GetAffect("debuff_heal_attack"),
	},
	chr.AFFECT_REMOVE_ALL_RESIST : {
		"description" : localeInfo.AFFECT_REMOVE_ALL_RESIST,
		"icon" : flamewindPath.GetAffect("break_resist"),
	},

	# BUFFS
	chr.AFFECT_NO_ITEM_DEATH_PENALTY : {
		"description" : localeInfo.AFFECT_DEATH_PENALTY,
		"icon" : flamewindPath.GetAffect("death_penalty"),
	},

	chr.NEW_AFFECT_MALL : {
		player.POINT_MALL_ATTBONUS : {
			"description" : localeInfo.MALL_ATT_BONUS_NAME,
			"icon" : "d:/ymir work/ui/skill/common/affect/att_bonus.sub",
		},

		player.POINT_MAX_HP_PCT : {
			"description" : localeInfo.MALL_HP_BONUS_NAME,
			"icon" : "d:/ymir work/ui/skill/common/affect/gold_premium.sub"
		},

		player.POINT_MAX_SP_PCT : {
			"description" : localeInfo.MALL_IQ_BONUS_NAME,
			"icon" : "d:/ymir work/ui/skill/common/affect/gold_premium.sub"
		},

		player.POINT_MALL_DEFBONUS : {
			"description" : localeInfo.MALL_DEF_BONUS_NAME,
			"icon" : "d:/ymir work/ui/skill/common/affect/gold_premium.sub"
		},

		player.POINT_MALL_EXPBONUS : {
			"description" : localeInfo.MALL_EXP_BONUS_NAME,
			"icon" : "d:/ymir work/ui/skill/common/affect/exp_bonus.sub",
		},
		player.POINT_MALL_ITEMBONUS : {
			"description" : localeInfo.TOOLTIP_MALL_ITEMBONUS,
			"icon" : "d:/ymir work/ui/skill/common/affect/item_bonus.sub",
		},
		player.POINT_MALL_GOLDBONUS : {
			"description" : localeInfo.TOOLTIP_MALL_GOLDBONUS,
			"icon" : "d:/ymir work/ui/skill/common/affect/gold_bonus.sub",
		},

		player.POINT_CRITICAL_PCT : {
			"description" : localeInfo.MALL_CRIT_BONUS_NAME,
			"icon" : "d:/ymir work/ui/flamewind/affect/affect_crit.sub"
		},

		player.POINT_PENETRATE_PCT : {
			"description" : localeInfo.MALL_PENET_BONUS_NAME,
			"icon" : "d:/ymir work/ui/flamewind/affect/affect_pene.sub"
		},

		player.POINT_ATT_SPEED : {
			"description" : localeInfo.TOOLTIP_ATT_SPEED,
			"icon" : "d:/ymir work/ui/skill/common/affect/Increase_Attack_Speed.sub",
		},

		player.POINT_MOV_SPEED : {
			"description" : localeInfo.TOOLTIP_MOV_SPEED,
			"icon" : "d:/ymir work/ui/skill/common/affect/Increase_Move_Speed.sub",
		},
	},

	chr.NEW_AFFECT_POLYMORPH : {
		"description" : localeInfo.TOOLTIP_POLYMORPH,
		"icon" : "icon/item/70104.tga"
	},

	chr.NEW_AFFECT_EXP_BONUS : {
		"description" : localeInfo.TOOLTIP_MALL_EXPBONUS_STATIC,
		"icon" : "d:/ymir work/ui/skill/common/affect/exp_bonus.sub",
	},

	chr.NEW_AFFECT_SKILL_BOOK_BONUS : {
		"description" : localeInfo.TOOLTIP_APPLY_SKILL_BOOK_BONUS,
		"icon" : flamewindPath.GetAffect("rada"),
		# "icon" : "d:/ymir work/ui/skill/common/affect/gold_premium.sub"
	},
	chr.NEW_AFFECT_SKILL_BOOK_NO_DELAY : {
		"description" : localeInfo.TOOLTIP_APPLY_SKILL_BOOK_NO_DELAY,
		"icon" : flamewindPath.GetAffect("egzo"),
		# "icon" : "d:/ymir work/ui/skill/common/affect/gold_premium.sub"
	},

	skill.SKILL_BERSERK : {
		"description" : localeInfo.SKILL_BERSERK,
		"icon" : "d:/ymir work/ui/skill/warrior/jeongwi_03.sub",
	},
	skill.SKILL_SWORD_AURA : {
		"description" : localeInfo.SKILL_SWORD_AURA,
		"icon" : "d:/ymir work/ui/skill/warrior/geomgyeong_03.sub",
	},
	skill.SKILL_STRONG_BODY : {
		"description" : localeInfo.SKILL_STRONG_BODY,
		"icon" : "d:/ymir work/ui/skill/warrior/cheongeun_03.sub",
	},
	skill.SKILL_FEATHER_WALK : {
		"description" : localeInfo.SKILL_FEATHER_WALK,
		"icon" : "d:/ymir work/ui/skill/assassin/gyeonggong_03.sub",
	},
	skill.SKILL_STEALTH : {
		"description" : localeInfo.SKILL_STEALTH,
		"icon" : "d:/ymir work/ui/skill/assassin/eunhyeong_03.sub",
	},
	skill.SKILL_ENCHANTED_BLADE : {
		"description" : localeInfo.SKILL_ENCHANTED_BLADE,
		"icon" : "d:/ymir work/ui/skill/sura/gwigeom_03.sub",
	},
	skill.SKILL_TERROR : {
		"description" : localeInfo.SKILL_TERROR,
		"icon" : "d:/ymir work/ui/skill/sura/gongpo_03.sub",
	},
	skill.SKILL_ENCHANTED_ARMOR : {
		"description" : localeInfo.SKILL_ENCHANTED_ARMOR,
		"icon" : "d:/ymir work/ui/skill/sura/jumagap_03.sub"
	},
	skill.SKILL_BLESSING : {
		"description" : localeInfo.SKILL_BLESSING,
		"icon" : "d:/ymir work/ui/skill/shaman/hosin_03.sub",
	},
	skill.SKILL_REFLECT : {
		"description" : localeInfo.SKILL_BOHO,
		"icon" : "d:/ymir work/ui/skill/shaman/boho_03.sub",
	},
	skill.SKILL_SWIFTNESS : {
		"description" : localeInfo.SKILL_SWIFTNESS,
		"icon" : "d:/ymir work/ui/skill/shaman/kwaesok_03.sub",
	},
	skill.SKILL_MANASHIELD : {
		"description" : localeInfo.SKILL_MANASHIELD,
		"icon" : "d:/ymir work/ui/skill/sura/heuksin_03.sub",
	},
	skill.SKILL_FLAME_SPIRIT : {
		"description" : localeInfo.SKILL_FLAME_SPIRIT,
		"icon" : "d:/ymir work/ui/skill/sura/muyeong_03.sub",
	},
	skill.SKILL_DRAGON_HELP : {
		"description" : localeInfo.SKILL_DRAGON_HELP,
		"icon" : "d:/ymir work/ui/skill/shaman/gicheon_03.sub",
	},
	skill.SKILL_ATTACK_UP : {
		"description" : localeInfo.SKILL_ATTACK_UP,
		"icon" : "d:/ymir work/ui/skill/shaman/jeungryeok_03.sub",
	},
	skill.SKILL_DISPEL : {
		"description" : localeInfo.SKILL_DISPEL,
		"icon" : "d:/ymir work/ui/skill/sura/pabeop_03.sub",
	},

	skill.SKILL_DRAGON_ROAR : {
		"description" : localeInfo.AFFECT_FIRE_RAGE,
		"icon" : flamewindPath.GetAffect("fire_rage"),
	},

	skill.SKILL_FIRE_ARROW : {
		"description" : localeInfo.SKILL_FIRE_ARROW,
		"icon" : "d:/ymir work/ui/skill/assassin/hwajo_03.sub",
	},

	chr.AFFECT_HEAVEN_PROTECTION: {
		"description": localeInfo.AFFECT_HEAVEN_PROTECTION,
		"icon": flamewindPath.GetAffect("heaven_protection"),
	},

	chr.AFF_SKILL_STRONG_BODY_WITH_FALL : {
		"description" : localeInfo.SKILL_STRONG_BODY,
		"icon" : "d:/ymir work/ui/skill/warrior/cheongeun_03.sub",
	},
	chr.AFFECT_FIRE : {
		"description" : localeInfo.SKILL_FIRE,
		"icon" : "d:/ymir work/ui/skill/sura/hwayeom_03.sub",
	},
	chr.AFFECT_CHINA_FIREWORK : {
		"description" : localeInfo.SKILL_POWERFUL_STRIKE,
		"icon" : "d:/ymir work/ui/skill/common/affect/powerfulstrike.sub",
	},

	chr.NEW_AFFECT_AUTO_HP_RECOVERY : {
		"description" : localeInfo.TOOLTIP_AUTO_POTION_REST,
		"icon" : "d:/ymir work/ui/pattern/auto_hpgauge/05.dds",
	},

	chr.NEW_AFFECT_AUTO_SP_RECOVERY : {
		"description" : localeInfo.TOOLTIP_AUTO_POTION_REST,
		"icon" : "d:/ymir work/ui/pattern/auto_spgauge/05.dds",
	},

	chr.AFFECT_ATT_SPEED_POTION : {
		"description" : localeInfo.SKILL_INC_ATKSPD,
		"icon" : "d:/ymir work/ui/skill/common/affect/Increase_Attack_Speed.sub",
	},

	chr.AFFECT_MOV_SPEED_POTION : {
		"description" : localeInfo.SKILL_INC_MOVSPD,
		"icon" : "d:/ymir work/ui/skill/common/affect/Increase_Move_Speed.sub",
	},

	chr.AFFECT_FISH_MIND : {
		"description" : localeInfo.SKILL_FISHMIND,
		"icon" : "d:/ymir work/ui/skill/common/affect/fishmind.sub",
	},

	chr.AFFECT_SPRINT : {
		"description" : localeInfo.SKILL_SPRINT_AFFECT,
		"icon" : "d:/ymir work/ui/skill/common/support/sprint.sub",
	},

	chr.AFFECT_BLOODTHIRST : {
		"description" : localeInfo.AFFECT_BLOODTHIRST,
		"icon" : flamewindPath.GetAffect("bloodthirst"),
	},

	chr.AFFECT_DEBUFF_HEAVEN_PROTECTION : {
		"description" : localeInfo.AFFECT_DEBUFF_HEAVEN_PROTECTION,
		"icon" : flamewindPath.GetAffect("heaven_protection_debuff"),
	},

	chr.AFFECT_TEMPLE_CURSE : {
		"description" : localeInfo.TOOLTIP_AFFECT_TEMPLE,
		"icon" : flamewindPath.GetAffect("temple"),
	},

	chr.AFFECT_DEVILTOWER_DAMAGE : {
		"description" : localeInfo.TOOLTIP_AFFECT_DEVILTOWER_DAMAGE,
		"icon" : flamewindPath.GetAffect("demonic_spirit"),
	},

	chr.AFFECT_HERBALIST_BONUS : {
		"description" : localeInfo.TOOLTIP_AFFECT_HERBALIST_BONUS,
		"icon" : flamewindPath.GetAffect("herb_bonus"),
	},

	## potions

	chr.AFFECT_COLLECT_NO_FAIL_TIME : {"description" : localeInfo.TOOLTIP_POTION_COLLECT_NO_FAIL_TIME,"icon" : flamewindPath.GetAffect("potion_special1"),},
	chr.AFFECT_LEARN_NO_FAIL_TIME : {"description" : localeInfo.TOOLTIP_POTION_NO_FAIL_TIME,"icon" : flamewindPath.GetAffect("fail_no_time"),},
	chr.AFFECT_EXP_BLOCK : {"description" : localeInfo.TOOLTIP_POTION_NO_EXP,"icon" : flamewindPath.GetAffect("potion_special6"),},
	chr.AFFECT_POTION_GENERAL_USE : {
		player.POINT_LEARN_CHANCE : {
			"description" : localeInfo.TOOLTIP_POTION_LEARN,
			"icon" : flamewindPath.GetAffect("learn"),
		},
	},

	chr.AFFECT_POTION_FOOD : {
		player.POINT_ATT_SPEED : {"icon" : flamewindPath.GetAffect("affect_fish"),},
		player.POINT_MOV_SPEED : {"icon" : flamewindPath.GetAffect("affect_fish"),},

		player.POINT_ST : {"icon" : flamewindPath.GetAffect("affect_fish"),},
		player.POINT_HT : {"icon" : flamewindPath.GetAffect("affect_fish"),},
		player.POINT_DX : {"icon" : flamewindPath.GetAffect("affect_fish"),},
		player.POINT_IQ : {"icon" : flamewindPath.GetAffect("affect_fish"),},
		player.POINT_SKILL_DURATION: {"icon": flamewindPath.GetAffect("affect_fish"), },
		player.POINT_MAX_HP: {"icon": flamewindPath.GetAffect("affect_fish"), },
		player.POINT_SKILL_DAMAGE_BONUS: {"icon": flamewindPath.GetAffect("affect_fish"), },
		player.POINT_SKILL_DEFEND_BONUS: {"icon": flamewindPath.GetAffect("affect_fish"), },
		player.POINT_RESIST_HUMAN: {"icon": flamewindPath.GetAffect("affect_fish"), },
		player.POINT_ATTBONUS_HUMAN: {"icon": flamewindPath.GetAffect("affect_fish"), },
		player.POINT_MALL_EXPBONUS : {"icon" : flamewindPath.GetAffect("affect_fish"),},
	},

	chr.AFFECT_POTION_BOOST : {
		player.POINT_ATT_SPEED : {"icon" : "d:/ymir work/ui/skill/common/affect/Increase_Attack_Speed.sub",},
		player.POINT_MOV_SPEED : {"icon" : "d:/ymir work/ui/skill/common/affect/Increase_Move_Speed.sub",},
		player.POINT_ST : {"icon" : flamewindPath.GetAffect("rosa_orange"),},
		player.POINT_HT : {"icon" : flamewindPath.GetAffect("rosa_yellow"),},
		player.POINT_DX : {"icon" : flamewindPath.GetAffect("rosa_cyan"),},
		player.POINT_IQ : {"icon" : flamewindPath.GetAffect("rosa_blue"),},
		player.POINT_MAX_HP : {"icon" : flamewindPath.GetAffect("rosa_red"), },
		player.POINT_CASTING_SPEED : {"icon" : flamewindPath.GetAffect("mix_pink"), },
		player.POINT_SKILL_DURATION : {"icon" : flamewindPath.GetAffect("mix_violet"), },
		player.POINT_HP_REGEN : {"icon" : flamewindPath.GetAffect("mix_red"), },
		player.POINT_SP_REGEN : {"icon" : flamewindPath.GetAffect("mix_blue"), },
		player.POINT_ST_REGEN : {"icon" : flamewindPath.GetAffect("mix_yellow"), },
	},

	chr.AFFECT_POTION_OFFENSIVE : {
		player.POINT_CRITICAL_PCT : {"icon" : flamewindPath.GetAffect("rosa_green"), },
		player.POINT_PENETRATE_PCT : {"icon" : flamewindPath.GetAffect("rosa_plat"), },
		player.POINT_MAGIC_ATT: {"icon": flamewindPath.GetAffect("rosa_violet"), },
		player.POINT_ATT_GRADE_BONUS : {"icon" : flamewindPath.GetAffect("rosa_crimson"), },
		player.POINT_ATTBONUS_HUMAN : {"icon" : flamewindPath.GetAffect("potion_deepviolet"), },
		player.POINT_SKILL_DAMAGE_BONUS : {"icon" : flamewindPath.GetAffect("potion_deepgreen"), },
		player.POINT_ATTBONUS_ORC_VALLEY : {"icon" : flamewindPath.GetAffect("potion_orc_valley"), },
	},

	chr.AFFECT_POTION_DEFENSIVE : {
		player.POINT_RESIST_MAGIC: {"icon": flamewindPath.GetAffect("rosa_pink"), },
		player.POINT_DEF_GRADE_BONUS : {"icon" : flamewindPath.GetAffect("rosa_grey"), },
		player.POINT_RESIST_HUMAN : {"icon" : flamewindPath.GetAffect("potion_clear"), },
		player.POINT_SKILL_DEFEND_BONUS : {"icon" : flamewindPath.GetAffect("rosa_beige"), },
		player.POINT_NORMAL_HIT_DEFEND_BONUS : {"icon" : flamewindPath.GetAffect("potion_brown"), },
	},

	chr.AFFECT_MONKEY_CURSE_EASY: {"description": localeInfo.TOOLTIP_AFFECT_MONKEY_CURSE_EASY,
									  "icon": flamewindPath.GetAffect("monkey"), },
	chr.AFFECT_MONKEY_CURSE_NORMAL: {"description": localeInfo.TOOLTIP_AFFECT_MONKEY_CURSE_NORMAL,
										  "icon": flamewindPath.GetAffect("monkey"), },
	chr.AFFECT_MONKEY_CURSE_HARD: {"description": localeInfo.TOOLTIP_AFFECT_MONKEY_CURSE_HARD,
										  "icon": flamewindPath.GetAffect("monkey"), },
}

AFFECT_CATEGORIES = {
	"vip" : [
		chr.NEW_AFFECT_EXP_BONUS,
		chr.NEW_AFFECT_ITEM_BONUS,
		chr.NEW_AFFECT_GOLD_BONUS,
		chr.NEW_AFFECT_SAFEBOX,
		chr.NEW_AFFECT_SHOP_PREMIUM,
		chr.NEW_AFFECT_FISH_MIND,
		chr.NEW_AFFECT_MARRIAGE_FAST,
		chr.NEW_AFFECT_AUTOLOOT,
		chr.NEW_AFFECT_SUBSCRIPTION,
	],

	"debuff" : [
		chr.AFFECT_POISON,
		chr.AFFECT_SLOW,
		chr.AFFECT_STUN,
		chr.AFFECT_ROOT,
		chr.AFFECT_FIRE,
		skill.SKILL_DISPEL,
		chr.AFFECT_DEBUFF_HEAVEN_PROTECTION,
		chr.AFFECT_TEMPLE_CURSE,
		chr.AFFECT_DEVILTOWER_DAMAGE
	],
}

AFFECT_SHOW_MANY_POINTS = [
	# chr.AFFECT_BOOST_PERM_START,
	# chr.AFFECT_BOOST_SKILL_START,
	chr.NEW_AFFECT_POLYMORPH,
]

REMOVABLE_AFFECTS = [
	chr.AFFECT_POTION_BOOST,
	chr.AFFECT_POTION_OFFENSIVE,
	chr.AFFECT_POTION_DEFENSIVE,
	chr.NEW_AFFECT_POLYMORPH,
	chr.AFFECT_EXP_BLOCK,
]

AFFECT_IMAGE_SIZE = 32
AFFECT_IMAGE_SCALE = 0.8
MAX_ROW_SIZE = 9
STEP_BETWEEN_CATEGORIES = 5

def IsVIPAffect(affect):
	return affect in AFFECT_CATEGORIES["vip"]

def GetAffectData(affect, point=0):
	if not AFFECT_SHOW_DATA.has_key(affect):
		return None
	affectData = AFFECT_SHOW_DATA[affect]
	if affectData.has_key(point):
		affectData = AFFECT_SHOW_DATA[affect][point]
	return affectData

class BarItem(ui.ExpandedImageBox):
	def __init__(self):
		ui.ExpandedImageBox.__init__(self)
		self.tooltip = None
		self.SAFE_SetEvent("MOUSE_RIGHT_BUTTON", self.OnMouseRight)
		self.SAFE_SetEvent("MOUSE_OVER_IN", self.ShowToolTip)
		self.SAFE_SetEvent("MOUSE_OVER_OUT", self.HideToolTip)

	def LoadImage(self, filePath):
		ui.ExpandedImageBox.LoadImage(self, filePath)
		self.SetScale(AFFECT_IMAGE_SCALE, AFFECT_IMAGE_SCALE)
		self.Show()

	def OnMouseRight(self):
		pass

	def ResetToolTip(self):
		if self.tooltip:
			self.tooltip.AlignHorizonalCenter()

	def ShowToolTip(self):
		pass

	def HideToolTip(self):
		self.tooltip.HideToolTip()

	def OnFixedUpdate(self):
		pass

class LoveImage(BarItem):
	FILE_PATH = "d:/ymir work/ui/pattern/LovePoint/"
	FILE_DICT = {
		0: FILE_PATH + "01.dds",
		1: FILE_PATH + "02.dds",
		2: FILE_PATH + "02.dds",
		3: FILE_PATH + "03.dds",
		4: FILE_PATH + "04.dds",
		5: FILE_PATH + "05.dds",
	}

	def __init__(self):
		BarItem.__init__(self)
		self.loverName = None
		self.lovePoint = 0
		self.Hide()

	def SetLoverInfo(self, name, lovePoint):
		self.loverName = name
		self.lovePoint = lovePoint
		self.__Refresh()
		self.Hide()

	def OnUpdateLovePoint(self, lovePoint):
		self.lovePoint = lovePoint
		self.__Refresh()

	def __Refresh(self):
		self.lovePoint = max(0, self.lovePoint)
		self.lovePoint = min(100, self.lovePoint)

		if 0 == self.lovePoint:
			loveGrade = 0
		else:
			loveGrade = self.lovePoint / 25 + 1
		fileName = self.FILE_DICT.get(loveGrade, self.FILE_PATH+"00.dds")
		self.LoadImage(fileName)

	def ShowToolTip(self):
		self.tooltip.ClearToolTip()
		# self.ResetToolTip()
		self.tooltip.SetTitle(self.loverName)
		self.tooltip.AppendTextLine(localeInfo.AFF_LOVE_POINT % (self.lovePoint))
		self.tooltip.ShowToolTip()

class HorseImage(BarItem):
	FILE_PATH = "d:/ymir work/ui/pattern/HorseState/"

	FILE_DICT = {
		00: FILE_PATH + "00.dds",
		01: FILE_PATH + "00.dds",
		02: FILE_PATH + "00.dds",
		03: FILE_PATH + "00.dds",
		10: FILE_PATH + "10.dds",
		11: FILE_PATH + "11.dds",
		12: FILE_PATH + "12.dds",
		13: FILE_PATH + "13.dds",
		20: FILE_PATH + "20.dds",
		21: FILE_PATH + "21.dds",
		22: FILE_PATH + "22.dds",
		23: FILE_PATH + "23.dds",
		30: FILE_PATH + "30.dds",
		31: FILE_PATH + "31.dds",
		32: FILE_PATH + "32.dds",
		33: FILE_PATH + "33.dds",
	}

	def __init__(self):
		BarItem.__init__(self)
		self.level = 0
		self.health = 0
		self.battery = 0
		self.Hide()

	def __GetHorseGrade(self, level):
		if 0 == level:
			return 0

		return (level-1)/10 + 1

	def SetState(self, level, health, battery):
		if level == 0:
			self.Hide()
			return

		self.level = level
		self.health = health
		self.battery = battery

		fileName = self.FILE_DICT[health * 10 + battery]
		print fileName
		self.LoadImage(fileName)

	def ShowToolTip(self):
		self.tooltip.ClearToolTip()
		# self.ResetToolTip()
		grade = self.__GetHorseGrade(self.level)
		self.tooltip.AppendTextLine(localeInfo.LEVEL_LIST[grade])

		healthName = localeInfo.HEALTH_LIST[self.health]
		if len(healthName) > 0:
			self.tooltip.AppendTextLine(healthName)

		if self.health > 0:
			if self.battery == 0:
				self.tooltip.AppendTextLine(localeInfo.NEEFD_REST)

		self.tooltip.ShowToolTip()

class AffectImage(BarItem):
	def __init__(self, affect, view_affect, point, value, duration):
		BarItem.__init__(self)
		self.__Initialize()
		self.affect = affect
		self.view_affect = view_affect
		self.createTime = int(app.GetGlobalTimeStamp())
		self.AddPoint(point, value)
		if not self.Load(point, value):
			print "no data for affect ", affect
		self.SetDuration(duration)

	def __Initialize(self):
		self.affect = 0
		self.view_affect = 0
		self.pointsDict = {}
		self.duration = 0
		self.description = ""
		self.boundWnd = None
		self.canUpdate = False
		self.durationLabel = None
		self.flash = None

		self.flash_delay = 1 * updateable.ONE_SECOND
		self.flash_timer = 0

	def Destroy(self):
		self.__Initialize()
		self.Hide()

	def AddPoint(self, point, value):
		if point == 0 or value == 0:
			return

		if point == player.POINT_MANASHIELD:
			return

		self.pointsDict[point] = value,

	def RemovePoint(self, point):
		self.pointsDict.pop(point, None)

	def SetDuration(self, duration):
		self.duration = duration
		if IsVIPAffect(self.affect):
			self.SetAlpha(0.5 if duration < 1 else 1)

	def Load(self, point, value):
		affectData = GetAffectData(self.view_affect, point)
		if affectData == None:
			return False

		self.LoadImage(affectData["icon"])
		self.SetScale(AFFECT_IMAGE_SCALE, AFFECT_IMAGE_SCALE)

		if affectData.has_key("description"):
			desc = affectData["description"]
			if type(desc) is int:
				item.SelectItem(desc)
				desc = item.GetItemName()
			self.SetAffectDescription(desc, value)
		self.Show()
		return True

	def OnMouseRight(self):
		if self.affect in REMOVABLE_AFFECTS:
			if len(self.pointsDict) == 1:
				first_key, first_value = next(iter(self.pointsDict.iteritems()))
				net.SendChatPacket("/remove_affect %d %d" % (self.affect, first_key))
			else:
				net.SendChatPacket("/remove_affect %d 0" % (self.affect))

	def SetAffectDescription(self, _description, value):
		try:
			description = _description(float(value))
		except:
			try:
				try:
					description = _description % float(value)
				except TypeError:
					description = _description
				except KeyError:
					description = "UNKNOWN"
			except:
				try:
					description = _description.format(float(value))
				except:
					description = _description

		self.description = description

	def GetDurationLabelValue(self, duration):
		return localeInfo.SecondToSmartDHMS(duration)
		# if duration >= 10:
		# 	return localeInfo.SecondToSmartDHMS(duration)
		# else:
		# 	return localeInfo.AFFECT_NEAR_END

	def ShowToolTip(self):
		self.canUpdate = True

		if not self.tooltip:
			return

		self.tooltip.ClearToolTip()

		if self.affect == chr.NEW_AFFECT_SUBSCRIPTION:
			benefit_list = itemshop_subscriptionwindow.BENEFITS
			benefit_count = len(benefit_list)
			for i in range(1, benefit_count+1):
				benefit = benefit_list[i]
				iconPath = benefit["icon"]
				iconPath = iconPath.replace("icon/", "").replace(".tga", "")
				desc = benefit["desc"]
				descSplit = desc.split("[ENTER]")
				descCount = len(descSplit)
				for j in range(descCount):
					d = descSplit[j]
					if j == 0:
						self.tooltip.AppendSpace(10)
						self.tooltip.AppendTextLine("|E%s|e %s" % (iconPath, d))
						self.tooltip.AppendSpace(6)
					else:
						self.tooltip.AppendTextLine(d)
						self.tooltip.AppendSpace(5)
		else:
			if len(self.description) > 0:
				for text in self.description.split(";"):
					if text == " ":
						self.tooltip.AppendSpace(5)
					else:
						self.tooltip.AppendTextLine(text)
		if len(self.pointsDict) > 0:
			self.tooltip.AppendSpace(5)
			for point in self.pointsDict.keys():
				try:
					pointValue = self.pointsDict[point][0]
				except:
					pointValue = self.pointsDict[point]
				color = None
				if pointValue >= 0 or point == player.POINT_MANASHIELD:
					color = grp.GenerateColor(0.5411, 0.7254, 0.5568, 1.0)
				else:
					color = grp.GenerateColor(0.9, 0.4745, 0.4627, 1.0)

				applyString = localeInfo.GetApplyString(point, pointValue)
				self.tooltip.AppendTextLine(applyString, color)

		currentTime = int(app.GetGlobalTimeStamp())
		deltaTime = currentTime - self.createTime
		duration = self.duration - deltaTime
		if duration > 0:
			self.tooltip.AppendSpace(5)
			if self.duration >= 365*24*3600:
				self.tooltip.AppendTextLine(localeInfo.AFFECT_INFINITE)
			else:
				self.durationLabel = proxy(self.tooltip.AppendTextLine(self.GetDurationLabelValue(duration)))
		elif IsVIPAffect(self.affect):
			self.tooltip.AppendSpace(5)
			self.tooltip.AppendTextLine("Nieaktywny", grp.GenerateColor(1.0, 0.3745, 0.3627, 1.0))

		if self.affect in REMOVABLE_AFFECTS:
			self.tooltip.AppendSpace(5)
			self.tooltip.AppendTextLine(localeInfo.TOOLTIP_DELETE_AFFECT)
		self.tooltip.ShowToolTip()

	def HideToolTip(self):
		if self.tooltip:
			self.tooltip.HideToolTip()
		self.canUpdate = False

	def UpdateFlash(self):
		if not self.flash:
			flash = ui.ExpandedImageBox()
			flash.SetParent(self)
			flash.LoadImage("d:/ymir work/ui/public/slotfinishcooltimeeffect/01.sub")
			flash.AddFlag("not_pick")
			flash.SetScale(AFFECT_IMAGE_SCALE, AFFECT_IMAGE_SCALE)
			flash.SetPosition(0, 0)
			flash.SetAlpha(0)
			flash.Show()
			self.flash = flash

		self.flash_timer += updateable.FIXED_TIMESTEP_UPDATE
		t = mathUtils.Clamp01(float(self.flash_timer) / self.flash_delay)
		progress = mathUtils.GetEaseOutExpo(t)
		self.flash.SetAlpha(progress * 0.9)

		if t >= 1:
			self.flash_timer = 0

	def GetLeftTime(self):
		currentTime = int(app.GetGlobalTimeStamp())
		deltaTime = currentTime - self.createTime
		return self.duration - deltaTime

	def OnFixedUpdate(self):
		duration = self.GetLeftTime()
		if self.canUpdate and self.durationLabel:
			self.durationLabel.SetText(self.GetDurationLabelValue(duration))

		if duration < 10:
			self.UpdateFlash()


class AutoPotionImage(AffectImage):
	FILE_PATH_HP = "d:/ymir work/ui/pattern/auto_hpgauge/"
	FILE_PATH_SP = "d:/ymir work/ui/pattern/auto_spgauge/"

	def __init__(self, affect, point, value, duration):
		AffectImage.__init__(self, affect, affect, point, value, duration)
		self.potionType = player.AUTO_POTION_TYPE_HP
		self.filePath = ""
		self.amountPercent = 0
		self.grade = 0
		self.itemVnum = 0

	def SetPotionType(self, type):
		self.potionType = type

		if player.AUTO_POTION_TYPE_HP == type:
			self.filePath = self.FILE_PATH_HP
		elif player.AUTO_POTION_TYPE_SP == type:
			self.filePath = self.FILE_PATH_SP

		self.grade = 0

		self.Refresh()

	def __GetPotionTypeAffect(self):
		if self.potionType == player.AUTO_POTION_TYPE_HP:
			return chr.NEW_AFFECT_AUTO_HP_RECOVERY
		else:
			return chr.NEW_AFFECT_AUTO_SP_RECOVERY

	def Refresh(self):
		if not constInfo.AFFECT_DICT.has_key(self.__GetPotionTypeAffect()):
			return

		isActivated, currentAmount, totalAmount, slotIndex = player.GetAutoPotionInfo(self.potionType)
		if not isActivated:
			return

		amountPercent = (float(currentAmount) / totalAmount) * 100.0
		self.amountPercent = amountPercent

		grade = 1
		if amountPercent > 80.0:
			grade = 5
		elif amountPercent > 60.0:
			grade = 4
		elif amountPercent > 30.0:
			grade = 3
		elif amountPercent > 5.0:
			grade = 2

		if self.grade != grade:
			self.grade = grade
			fmt = self.filePath + "%.2d.dds"
			fileName = fmt % grade

			print fileName, grade
			self.LoadImage(fileName)

	def OnFixedUpdate(self):
		self.Refresh()

	def ShowToolTip(self):
		if self.tooltip:
			self.tooltip.ClearToolTip()
			# self.ResetToolTip()
			if player.AUTO_POTION_TYPE_HP == self.potionType:
				self.tooltip.SetTitle(localeInfo.TOOLTIP_AUTO_POTION_HP)
			else:
				self.tooltip.SetTitle(localeInfo.TOOLTIP_AUTO_POTION_SP)

			self.tooltip.AppendTextLine(localeInfo.TOOLTIP_AUTO_POTION_REST % self.amountPercent)
			self.tooltip.ShowToolTip()

class AffectShower(ui.Window):
	def __init__(self):
		ui.Window.__init__(self)
		self.__Initialize()
		self.SetPosition(10, 10)

		self.isMinimapHidden = False

		eventManager.EventManager().add_observer(uiMiniMap.EVENT_SET_MINIMAP_HIDE_STATE, self.OnMinimapHideState)

		self.vipShower = ui.Window()
		self.vipShower.SetWindowHorizontalAlignRight()
		self.vipShower.SetPosition(160, 8)
		self.vipShower.SetSize(0, 32)
		# self.vipShower.SetSize((AFFECT_IMAGE_SIZE * AFFECT_IMAGE_SCALE + 2) * len(AFFECT_CATEGORIES["vip"]), 32)
		self.vipShower.Show()

		self.tooltip = uiToolTip.ToolTip(130)
		# self.CreateVIPAffects()
		self.AddCustomItem("horse").Hide()
		self.AddCustomItem("love").Hide()
		self.Show()

	def __Initialize(self):
		self.affectImageDict = {"vip" : {}, "debuff" : {}, "other" : {}}
		self.tooltip = None

	def Destroy(self):
		self.__Initialize()
		self.Hide()

	def GetAffectImageAndCategory(self, search_affect, search_point):
		for category, affectDict in self.affectImageDict.items():
			for affect_type, pointDict in affectDict.items():
				if search_affect == affect_type:
					for point_key, item in pointDict.items():
						if search_point == point_key:
							return (item, point_key, category,)
		return (None, None, None,)

	def GetAffectCategory(self, affect):
		for category in AFFECT_CATEGORIES.keys():
			if affect in AFFECT_CATEGORIES[category]:
				return category

		return "other"

	def CreateVIPAffects(self):
		for affect in AFFECT_CATEGORIES["vip"]:
			self.AddAffect(affect, 0, 0, 0)

	def GetItem(self, type, point=0):
		return self.affectImageDict[self.GetAffectCategory(type)][type][point]

	def OnFixedUpdate(self):
		for category in self.affectImageDict.keys():
			for type in self.affectImageDict[category]:
				for pointIdx in self.affectImageDict[category][type].keys():
					affectImage = self.affectImageDict[category][type][pointIdx]
					if not affectImage.IsShow():
						continue
					if affectImage:
						affectImage.OnFixedUpdate()

	def AddCustomItem(self, type):
		affectDict = self.affectImageDict[self.GetAffectCategory(type)]

		item = None
		if type == "horse":
			item = HorseImage()
		elif type == "love":
			item = LoveImage()

		if item != None:
			item.SetParent(self)
			item.tooltip = self.tooltip
			if not affectDict.has_key(type):
				affectDict[type] = {}

			affectDict[type][0] = item

		return item

	def __ConvertSkillAffectBasedOnFlag(self, affect_type, flag):
		if affect_type > 111:
			return affect_type

		if flag == chr.AFF_SLOW:
			return chr.AFFECT_SLOW
		elif flag == chr.AFF_STUN:
			return chr.AFFECT_STUN
		elif flag == chr.AFF_ROOT:
			return chr.AFFECT_ROOT
		elif flag == chr.AFF_FIRE:
			return chr.AFFECT_FIRE
		elif flag == chr.AFF_POISON:
			return chr.AFFECT_POISON
		elif flag == chr.AFF_SKILL_DISPEL:
			return skill.SKILL_DISPEL
		else:
			return affect_type

	def AddAffect(self, type, pointIdx, value, duration, itemVnum, flag):
		# type = self.GetAffectType(type)
		view_type = self.__ConvertSkillAffectBasedOnFlag(type, flag)

		print "AddAffect: %d %d %d %d" % (type, pointIdx, value, duration)
		if not AFFECT_SHOW_DATA.has_key(view_type):
			return

		affectCategory = self.GetAffectCategory(view_type)
		affectDict = self.affectImageDict[affectCategory]

		# jezeli affect nadawany jest 2 razy (bo np. 2 rozne pointy), a chcemy go wyswietlic tylko raz
		if AFFECT_SHOW_DATA[view_type].has_key("description") and affectDict.has_key(view_type):

			for point_key in affectDict[type].keys():
				if IsVIPAffect(type):
					affectDict[type][point_key].SetDuration(duration)

					# if type == chr.NEW_AFFECT_AUTOLOOT and duration > 0 and self.interface:
					# 	affectDict[type][key].boundWnd = proxy(self.interface.autolootDialog)
				else:
					affectDict[type][point_key].AddPoint(pointIdx, value)
			return

		if IsVIPAffect(type) and affectDict.has_key(chr.NEW_AFFECT_SUBSCRIPTION):
			return

		# MT2009_PLUS_VEKIRION_V1 (Autor: Vekirion): an affect whose point has no
		# entry in AFFECT_SHOW_DATA has no "icon" and used to crash with
		# KeyError: 'icon' (AffectImage). Skip it and log which one.
		if type != chr.NEW_AFFECT_AUTO_HP_RECOVERY and type != chr.NEW_AFFECT_AUTO_SP_RECOVERY:
			_data = GetAffectData(view_type, pointIdx)
			if _data == None or not _data.has_key("icon"):
				import dbg
				dbg.TraceError("AffectBar: no icon for affect %d point %d value %d" % (view_type, pointIdx, value))
				return

		if not affectDict.has_key(type):
			affectDict[type] = {}

		affectImage = None
		if type == chr.NEW_AFFECT_AUTO_HP_RECOVERY or type == chr.NEW_AFFECT_AUTO_SP_RECOVERY:
			affectImage = AutoPotionImage(type, pointIdx, value, duration)
			affectImage.SetPotionType(player.AUTO_POTION_TYPE_HP if type == chr.NEW_AFFECT_AUTO_HP_RECOVERY else player.AUTO_POTION_TYPE_SP)
		else:
			affectImage = AffectImage(type, view_type, pointIdx, value, duration)

		if affectCategory == "vip":
			affectImage.SetParent(self.vipShower)
		else:
			affectImage.SetParent(self)
		affectImage.tooltip = self.tooltip
		affectImage.itemVnum = itemVnum

		affectDict[type][pointIdx] = affectImage

		self.__ArrangeAffectImage()

	def RemoveAffect(self, type, pointIdx):
		affectImage, pointKey, category = self.GetAffectImageAndCategory(type, pointIdx)
		if affectImage is None:
			print "AffectImage (type: %d, point: %d) does not exist" % (type, pointIdx)
			return []

		if len(affectImage.pointsDict) > 1:
			affectImage.RemovePoint(pointIdx)

		view_type = affectImage.view_affect
		if affectImage.GetLeftTime() <= 1:
			itemVnum = affectImage.itemVnum
			effect_name = ""
			is_negative_effect = view_type in AFFECT_CATEGORIES["debuff"]
			if itemVnum > 0:
				item.SelectItem(itemVnum)
				effect_name = item.GetItemName()
			elif view_type <= 111 or is_negative_effect or view_type in (chr.AFFECT_BLOODTHIRST,):
				effect_name = AFFECT_SHOW_DATA[view_type]["description"]

			if len(effect_name) > 0:
				eventManager.EventManager().send_event(eventManager.ADD_FLOATING_TEXT, localeInfo.FLOATING_TEXT_END_AFFECT % effect_name, is_negative_effect)

		affectImage.Destroy()
		self.affectImageDict[category][type].pop(pointKey, None)
		if len(self.affectImageDict[category][type]) < 1:
			self.affectImageDict[category].pop(type, None)
		self.__ArrangeAffectImage()

	def ArrangeAffectImage(self):
		self.__ArrangeAffectImage()

	def __SetVipShowerPosition(self, width):
		self.vipShower.SetPosition((42 if self.isMinimapHidden else 136) + width, 8)

	def OnMinimapHideState(self, isHidden):
		self.isMinimapHidden = isHidden
		self.__SetVipShowerPosition(self.vipShower.GetWidth())

	def __ArrangeAffectImage(self):
		self.SetSize(0, 0)
		windowSize = [0, 0]
		for key in self.affectImageDict.keys():
			(width, height) = self.__RefreshAffectCategory(key, windowSize[1])

			if key == "vip":
				self.__SetVipShowerPosition(width)
				self.vipShower.SetSize(width, height)
				continue

			if width > windowSize[0]:
				windowSize[0] = width

			windowSize[1] += height

		if self.tooltip:
			self.tooltip.HideToolTip()

		eventManager.EventManager().send_event(EVENT_ARRANGE_AFFECT_LIST, windowSize[0], windowSize[1])
		self.SetSize(windowSize[0], windowSize[1])

	def __RefreshAffectCategory(self, category, windowHeight):
		if windowHeight > 0:
			windowHeight += STEP_BETWEEN_CATEGORIES

		imageSize = AFFECT_IMAGE_SIZE * AFFECT_IMAGE_SCALE
		columnCount = 0

		idx = 0
		for type in self.affectImageDict[category]:
			for pointIdx in self.affectImageDict[category][type].keys():
				affectImage = self.affectImageDict[category][type][pointIdx]
				if not affectImage.IsShow():
					continue

				column = idx / MAX_ROW_SIZE
				xPos = idx % MAX_ROW_SIZE
				affectImage.SetPosition((imageSize+2) * xPos, windowHeight + (imageSize+2) * column)
				idx += 1
				columnCount = column + 1
		sizeX = (imageSize+2) * (MAX_ROW_SIZE if idx > MAX_ROW_SIZE else idx)
		sizeY = (imageSize + 2) * columnCount + (STEP_BETWEEN_CATEGORIES if windowHeight > 0 else 0)
		return (sizeX, sizeY)
