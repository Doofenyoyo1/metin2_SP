import ui
import localeInfo
import player
import eventManager
import colorInfo

EVENT_OPEN_ATTRIBUTE_LIST = "EVENT_OPEN_ATTRIBUTE_LIST" # | args: None

CATEGORY_SORT_ORDER = [
	"OFFENSIVE",
	"OFFENSIVE_MONSTER",
	"OFFENSIVE_PLAYER",
	"DEFENSIVE",
	"DEFENSIVE_PLAYER",
	"OTHER",
]

ATTRIBUTE_LIST = {
	"OFFENSIVE": {
		"locale": localeInfo.ATTRIBUTE_LIST_OFFENSIVE,
		"attr_list": [
			player.POINT_CRITICAL_PCT,
			player.POINT_PENETRATE_PCT,
			player.POINT_POISON_PCT,
			player.POINT_STUN_PCT,
			player.POINT_SLOW_PCT,
			player.POINT_SKILL_DAMAGE_BONUS,
			player.POINT_NORMAL_HIT_DAMAGE_BONUS,
			player.POINT_REFLECT_MELEE,
			player.POINT_REFLECT_ARROW,
		],
	},

	"OFFENSIVE_MONSTER": {
		"locale": localeInfo.ATTRIBUTE_LIST_OFFENSIVE_MONSTER,
		"attr_list": [
			player.POINT_ATTBONUS_MONSTER, # MT2009_PLUS_HORSE30_V1: the horse's bonus from level 21
			player.POINT_ATTBONUS_ANIMAL,
			player.POINT_ATTBONUS_ORC,
			player.POINT_ATTBONUS_MILGYO,
			player.POINT_ATTBONUS_UNDEAD,
			player.POINT_ATTBONUS_DEVIL,
			player.POINT_ATTBONUS_STONE,
			player.POINT_ATTBONUS_BOSS,
		],
	},

	"OFFENSIVE_PLAYER": {
		"locale": localeInfo.ATTRIBUTE_LIST_OFFENSIVE_PLAYER,
		"attr_list": [
			player.POINT_ATTBONUS_HUMAN,
			player.POINT_ATTBONUS_WARRIOR,
			player.POINT_ATTBONUS_ASSASSIN,
			player.POINT_ATTBONUS_SURA,
			player.POINT_ATTBONUS_SHAMAN,
		],
	},

	"DEFENSIVE": {
		"locale": localeInfo.ATTRIBUTE_LIST_DEFENSIVE,
		"attr_list": [
			player.POINT_SKILL_DEFEND_BONUS,
			player.POINT_NORMAL_HIT_DEFEND_BONUS,
			player.POINT_RESIST_CRITICAL,
			player.POINT_RESIST_PENETRATE,
			player.POINT_BLOCK,
			player.POINT_DODGE,
			player.POINT_POISON_REDUCE,
		],
	},

	"DEFENSIVE_PLAYER": {
		"locale": localeInfo.ATTRIBUTE_LIST_DEFENSIVE_PLAYER,
		"attr_list": [
			player.POINT_RESIST_HUMAN,
			player.POINT_RESIST_WARRIOR,
			player.POINT_RESIST_ASSASSIN,
			player.POINT_RESIST_SURA,
			player.POINT_RESIST_SHAMAN,
			player.POINT_RESIST_SWORD,
			player.POINT_RESIST_TWOHAND,
			player.POINT_RESIST_DAGGER,
			player.POINT_RESIST_BELL,
			player.POINT_RESIST_FAN,
			player.POINT_RESIST_BOW,
			player.POINT_RESIST_MAGIC,
		],
	},

	"OTHER": {
		"locale": localeInfo.ATTRIBUTE_LIST_OTHER,
		"attr_list": [
			player.POINT_STEAL_HP,
			player.POINT_STEAL_SP,
			player.POINT_MANA_BURN_PCT,
			player.POINT_HP_REGEN,
			player.POINT_SP_REGEN,
			player.POINT_ST_REGEN,
			player.POINT_SKILL_DURATION,
		],
	},
}

def GetAttributeName(attr_index, value):
	return localeInfo.GetApplyString(attr_index, value)

class AttributeListWindow(ui.ScriptWindow):
	class AttrItem(ui.TextValueBar):
		def __init__(self, attr_index, is_perc_val):
			self.attr_index = attr_index
			self.is_perc_val = is_perc_val
			ui.TextValueBar.__init__(self, self.GetAttrName(), self.GetValueText(0), 318, 20)
			self.Show()

		def __del__(self):
			ui.TextValueBar.__del__(self)

		def GetAttrName(self):
			temp_name = GetAttributeName(self.attr_index, 999)
			temp_name = temp_name.replace("999", "")
			temp_name = temp_name.replace("+", "")
			temp_name = temp_name.replace("-", "")
			temp_name = temp_name.replace(":", "")

			if self.attr_index not in (player.POINT_STEAL_HP, player.POINT_STEAL_SP):
				temp_name = temp_name.replace("% ", "")
				temp_name = temp_name.replace(" %", "")
			return temp_name

		def GetValueText(self, attr_value):
			if self.is_perc_val:
				return "%d%%" % attr_value
			else:
				return "%d" % attr_value

		def SetValue(self, value):
			self.SetValueLabel(self.GetValueText(value))

			color = colorInfo.DISABLED_FONT_COLOR
			if value > 0:
				color = colorInfo.POSITIVE_COLOR
			elif value < 0:
				color = colorInfo.NEGATIVE_COLOR

			self.title.SetPackedFontColor(color)
			self.value.SetPackedFontColor(color)

		def RefreshStatus(self):
			val = player.GetStatus(self.attr_index)
			self.SetValue(val)

	def __init__(self):
		ui.ScriptWindow.__init__(self)
		self.__Initialize()
		self.LoadDialog()

		eventManager.EventManager().add_observer(EVENT_OPEN_ATTRIBUTE_LIST, self.Open)

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def __Initialize(self):
		self.categoryList = []
		self.attrItemList = []

	@ui.WindowDestroy
	def Destroy(self):
		self.ClearDictionary()
		self.__Initialize()

	def LoadDialog(self):
		try:
			PythonScriptLoader = ui.PythonScriptLoader()
			PythonScriptLoader.LoadScriptFile(self, "UIScript/attributelistwindow.py")

			self.board = self.GetChild("Board")
			self.content = self.GetChild("ContentWindow")
			self.mask = self.GetChild("ContentMask")
			self.titleBar = self.GetChild("TitleBar")
			self.scrollBar = self.GetChild("ScrollBar")

			self.titleBar.SetCloseEvent(ui.__mem_func__(self.Close))

			self.scrollBar.SetScrollContent(self.mask, self.content)
			self.scrollBar.SAFE_SetOnWheelEvent(self.content)

			for category in CATEGORY_SORT_ORDER:
				data = ATTRIBUTE_LIST[category]
				self.__AddCategory(data["locale"])
				for attr_index in data["attr_list"]:
					self.__AddAttr(attr_index)

		except:
			import exception
			exception.Abort("AttributeListWindow.LoadDialog.BindObject")

	def __AddCategory(self, category_name):
		category = ui.HorizontalBarTitle(318, category_name)
		category.SetParent(self.content)
		category.SetPosition(8, 0)
		category.title.SetWindowHorizontalAlignCenter()
		category.title.SetHorizontalAlignCenter()
		category.title.SetPosition(0, 0)
		category.Show()
		self.__AddContentItem(category)
		self.categoryList.append(category)
		return category

	def __AddAttr(self, attr_index):
		is_perc_value = GetAttributeName(attr_index, 1).find('%')
		attrItem = self.AttrItem(attr_index, is_perc_value)
		attrItem.SetParent(self.content)
		attrItem.SetPosition(8, 0)
		attrItem.SetBarBackground(len(self.attrItemList) % 2 == 0)
		self.__AddContentItem(attrItem)
		self.attrItemList.append(attrItem)
		return attrItem

	def __AddContentItem(self, item):
		x, y = item.GetLocalPosition()
		item.SetPosition(x, self.content.GetHeight())
		self.content.SetHeight(self.content.GetHeight() + item.GetHeight())
		item.SetClippingMaskWindow(self.mask)
		self.scrollBar.ResizeScrollBar()

	def RefreshStatus(self): # binary call
		for i in self.attrItemList:
			i.RefreshStatus()

	def OnPressEscapeKey(self):
		self.Close()
		return True

	def Close(self):
		self.Hide()

	def Open(self):
		if self.IsShow():
			self.Close()
		else:
			self.Show()
			self.SetTop()
