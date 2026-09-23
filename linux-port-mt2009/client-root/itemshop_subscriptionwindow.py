import utils
import gamefont
import colorinfo
import ui
import itemshop_contentwindow
import localeInfo
import math
from _weakref import proxy

BENEFITS = {
	1 : {
		"icon": "icon/item/70005.tga",
		"desc": localeInfo.SUBSCRIPTION_BENEFIT_EXP,
		"is_only_sub" : False,
	},

	2 : {
		"icon": "icon/item/70043.tga",
		"desc": localeInfo.SUBSCRIPTION_BENEFIT_DROP,
		"is_only_sub" : False,
	},

	3 : {
		"icon": "icon/item/premium.tga",
		"desc": localeInfo.SUBSCRIPTION_BENEFIT_CHANNEL,
		"is_only_sub" : True,
	},

	4 : {
		"icon": "icon/item/70002.tga",
		"desc": localeInfo.SUBSCRIPTION_BENEFIT_PICKUP,
		"is_only_sub" : False,
	},

	5 : {
		"icon": "icon/item/71113.tga",
		"desc": localeInfo.SUBSCRIPTION_BENEFIT_ITEM_VIEW,
		"is_only_sub" : False,
	},

	6 : {
		"icon": "icon/item/71068.tga",
		"desc": localeInfo.SUBSCRIPTION_BENEFIT_LOVE,
		"is_only_sub" : True,
	},

	7 : {
		"icon": "icon/item/71008.tga",
		"desc": localeInfo.SUBSCRIPTION_BENEFIT_FISH,
		"is_only_sub" : False,
	},

	8 : {
		"icon": "icon/item/71009.tga",
		"desc": localeInfo.SUBSCRIPTION_BENEFIT_DEPOSIT,
		"is_only_sub" : True,
	},

	9 : {
		"icon": "icon/item/71011.tga",
		"desc": localeInfo.SUBSCRIPTION_BENEFIT_EMOTION,
		"is_only_sub" : True,
	},

	10 : {
		"icon": "icon/item/71095.tga",
		"desc": localeInfo.SUBSCRIPTION_BENEFIT_PASSAGE,
		"is_only_sub" : False,
	},

	11 : {
		"icon": "icon/item/71017.tga",
		"desc": localeInfo.SUBSCRIPTION_BENEFIT_GOLD,
		"is_only_sub" : False,
	},

	12 : {
		"icon": "icon/item/50200.tga",
		"desc": localeInfo.SUBSCRIPTION_BENEFIT_SHOP,
		"is_only_sub" : False,
	},
}

class ItemShopSubscriptionContent(itemshop_contentwindow.ItemShopContentWindow):
	class BenefitItem(ui.ImageBox):
		def __init__(self, parent, index, x, y, isMain=False):
			ui.ImageBox.__init__(self)
			self.tooltip = None
			self.index = index

			self.SetParent(parent)
			self.LoadImage("d:/ymir work/ui/public/slot_base.sub")
			self.SetPosition(x, y)
			self.Show()

			icon = ui.ImageBox()
			icon.SetParent(self)
			icon.LoadImage(BENEFITS[index]["icon"])
			icon.SetPosition(0, 0)
			icon.Show()
			icon.SAFE_SetStringEvent("MOUSE_OVER_IN", self.ShowToolTip)
			icon.SAFE_SetStringEvent("MOUSE_OVER_OUT", self.HideToolTip)
			self.icon = icon

			if isMain:
				self.mainText = self.AddMainText()

		def __del__(self):
			ui.ImageBox.__del__(self)

		def Destroy(self):
			self.tooltip = None

		def SetToolTip(self, tooltip):
			self.tooltip = proxy(tooltip)

		def AddMainText(self):
			text = ui.TextLine()
			text.SetParent(self)
			text.SetBold()
			text.SetPackedFontColor(colorinfo.GetColorFromColorTuple(colorinfo.COLOR_GOOD))
			txt = BENEFITS[self.index]["desc"].split("[ENTER]")[0]
			text.SetText("- %s" % txt)
			text.SetVerticalAlignCenter()
			text.SetWindowVerticalAlignCenter()
			text.SetPosition(36, 0)
			text.Show()
			return text

		def ShowToolTip(self):
			self.tooltip.ClearToolTip()
			data = BENEFITS[self.index]
			descSplit = data["desc"].split("[ENTER]")
			for desc in descSplit:
				self.tooltip.AppendTextLine(desc)
			if data["is_only_sub"]:
				self.tooltip.AppendSpace(5)
				self.tooltip.AppendTextLine(localeInfo.SUBSCRIPTION_BENEFIT_ONLY_SUB, isBold=True)
			self.tooltip.ShowToolTip()

		def HideToolTip(self):
			self.tooltip.HideToolTip()

	def __init__(self, itemshopWnd, tooltip):
		itemshop_contentwindow.ItemShopContentWindow.__init__(self, itemshopWnd.content)
		self.tooltip = tooltip
		self.elements = []
		self.benefits = []
		self.__CreateContent()

	def __del__(self):
		itemshop_contentwindow.ItemShopContentWindow.__del__(self)

	def Destroy(self):
		for i in self.benefits:
			i.Destroy()

		self.benefits = None
		self.elements = None
		self.tooltip = None

	def __CreateContent(self):
		self.__CreateTitle(localeInfo.SUBSCRIPTION_PRES_TITLE1, 15, 20)
		self.__CreateDescription(localeInfo.SUBSCRIPTION_PRES_DESC1, 15, 42)
		self.__CreateTitle(localeInfo.SUBSCRIPTION_PRES_TITLE2, 15, 90)
		self.__CreateDescription(localeInfo.SUBSCRIPTION_PRES_DESC2, 15, 102)

		mainBenefits = [1,2,3]
		for i in mainBenefits:
			benefit = self.BenefitItem(self, i, 15, 125 + (i-1) * 32, True)
			benefit.SetToolTip(self.tooltip)
			self.benefits.append(benefit)

		elementCount = len(BENEFITS) - len(mainBenefits)

		self.__CreateDescription(localeInfo.SUBSCRIPTION_PRES_DESC3 % elementCount, 0, 230, alignCenter=True)

		columns = 5
		rows = math.ceil(float(elementCount)/columns)
		benefitsWidth = columns*32
		contentWidth = self.GetWidth()

		for i in range(elementCount):
			benefitIndex = len(mainBenefits) + 1 + i

			row = i / columns
			col = i - columns * row
			x = contentWidth/2 - benefitsWidth/2 + 32*col
			y = 265 + row * 32
			benefit = self.BenefitItem(self, benefitIndex, x, y, False)
			benefit.SetToolTip(self.tooltip)
			self.benefits.append(benefit)

		self.__CreateDescription(localeInfo.SUBSCRIPTION_PRES_DESC4, 0, 340, alignCenter=True, isBold=True)

		btn = ui.Button()
		btn.SetParent(self)
		btn.SetUpVisual("d:/ymir work/ui/public/large_button_01.sub")
		btn.SetOverVisual("d:/ymir work/ui/public/large_button_02.sub")
		btn.SetDownVisual("d:/ymir work/ui/public/large_button_03.sub")
		btn.SetWindowHorizontalAlignCenter()
		btn.SetPosition(0, 375)
		btn.SetText(localeInfo.SUBSCRIPTION_BUTTON, isBold=True)
		btn.SetTextColor(colorinfo.GetColorFromColorTuple(colorinfo.DRAGON_COIN_COLOR))
		btn.SAFE_SetBetterToolTip(localeInfo.OPEN_URL_INFO, self.tooltip)
		btn.SAFE_SetEvent(self.__OnButtonClick)
		btn.Show()
		self.button = btn

	def __CreateTitle(self, title, x, y):
		text = ui.TextLine()
		text.SetParent(self)
		text.SetFontName(gamefont.GetCurrentFontLarge())
		text.SetBold()
		text.SetText(title)
		text.SetPosition(x, y)
		text.Show()
		self.elements.append(text)

	def __CreateDescription(self, description, x, y, alignCenter=False, isBold=False):
		descSplited = description.split("[ENTER]")
		idx=0
		for desc in descSplited:
			text = ui.TextLine()
			text.SetParent(self)

			if isBold:
				text.SetBold()

			text.SetText(desc)

			if alignCenter:
				text.SetWindowHorizontalAlignCenter()
				text.SetHorizontalAlignCenter()

			width, height = text.GetTextSize()
			text.SetPosition(x, y + idx*height+2)
			text.Show()

			idx+=1
			self.elements.append(text)

	def __OnButtonClick(self):
		utils.open_url("https://buycoffee.to/metin2-playerbots")
