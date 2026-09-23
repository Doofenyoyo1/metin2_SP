import dbg
import player
import item
import grp
import wndMgr
import skill
import shop
import exchange
import grpText
import safebox
import localeInfo
import app
import background
import nonplayer
import chr

import localeinfo_point
import special_flags

import ui
import mouseModule
import constInfo
from constInfo import Emoji
import colorinfo
import uiItemShop

import gamefont
import flamewindPath
import re

if app.ENABLE_ACCE_COSTUME_SYSTEM:
	import acce

WARP_SCROLLS = [22011, 22000, 22001, 22010]

DESC_DEFAULT_MAX_COLS = 26
DESC_WESTERN_MAX_COLS = 35
DESC_WESTERN_MAX_WIDTH = 220

def chop(n):
	return round(n - 0.5, 1)

def SplitDescription(desc, limit):
	total_tokens = desc.split()
	line_tokens = []
	line_len = 0
	lines = []
	for token in total_tokens:
		if "|" in token:
			sep_pos = token.find("|")
			line_tokens.append(token[:sep_pos])

			lines.append(" ".join(line_tokens))
			line_len = len(token) - (sep_pos + 1)
			line_tokens = [token[sep_pos+1:]]
		else:
			line_len += len(token)
			if len(line_tokens) + line_len > limit:
				lines.append(" ".join(line_tokens))
				line_len = len(token)
				line_tokens = [token]
			else:
				line_tokens.append(token)

	if line_tokens:
		lines.append(" ".join(line_tokens))

	return lines

###################################################################################################
## ToolTip
##
##
class ToolTip(ui.Window):

	TOOL_TIP_WIDTH = 190
	TOOL_TIP_HEIGHT = 10

	TEXT_LINE_HEIGHT = 17

	TITLE_COLOR = grp.GenerateColor(0.9490, 0.9058, 0.7568, 1.0)
	SPECIAL_TITLE_COLOR = grp.GenerateColor(1.0, 0.7843, 0.0, 1.0)
	NORMAL_COLOR = grp.GenerateColor(0.7607, 0.7607, 0.7607, 1.0)
	FONT_COLOR = grp.GenerateColor(0.7607, 0.7607, 0.7607, 1.0)
	PRICE_COLOR = 0xffFFB96D

	HIGH_PRICE_COLOR = SPECIAL_TITLE_COLOR
	MIDDLE_PRICE_COLOR = grp.GenerateColor(0.85, 0.85, 0.85, 1.0)
	LOW_PRICE_COLOR = grp.GenerateColor(0.7, 0.7, 0.7, 1.0)

	ENABLE_COLOR = grp.GenerateColor(0.7607, 0.7607, 0.7607, 1.0)
	DISABLE_COLOR = grp.GenerateColor(0.9, 0.4745, 0.4627, 1.0)

	NEGATIVE_COLOR = grp.GenerateColor(0.9, 0.4745, 0.4627, 1.0)
	SPECIAL_NEGATIVE_COLOR = grp.GenerateColor(1.0, 0.6569, 0.6412, 1.0)
	POSITIVE_COLOR = grp.GenerateColor(0.5411, 0.7254, 0.5568, 1.0)
	SPECIAL_POSITIVE_COLOR = grp.GenerateColor(0.6911, 0.8754, 0.7068, 1.0) #bonus 1-4
	SPECIAL_POSITIVE_COLOR2 = grp.GenerateColor(0.8824, 0.9804, 0.8824, 1.0) #bonus 6-7
	SPECIAL_POSITIVE_COLOR5 = grp.GenerateColor(0.7824, 0.9004, 0.8024, 1.0) #bonus 5

	CONDITION_COLOR = 0xffBEB47D
	CAN_LEVEL_UP_COLOR = 0xff8EC292
	CANNOT_LEVEL_UP_COLOR = DISABLE_COLOR
	NEED_SKILL_POINT_COLOR = 0xff9A9CDB

	def __init__(self, width = TOOL_TIP_WIDTH, isPickable=False, isNoBackground=False):
		ui.Window.__init__(self, "TOP_MOST")

		self.background = ui.ThinBoard()
		self.background.AddFlag("not_pick")
		self.background.SetParent(self)
		if not isNoBackground:
			self.background.Show()
		else:
			self.background.Hide()

		if isPickable:
			pass
		else:
			self.AddFlag("not_pick")

		self.AddFlag("float")

		self.followFlag = True
		self.toolTipWidth = width

		self.xPos = -1
		self.yPos = -1

		self.defFontName = gamefont.GetCurrentFont()
		self.ClearToolTip()
		self.auxiliaryDict = ""

	def __del__(self):
		ui.Window.__del__(self)

	def ClearToolTip(self):
		self.toolTipHeight = 12
		self.childrenList = []
		self.auxiliaryDict = ""

	def SetFollow(self, flag):
		self.followFlag = flag

	def SetDefaultFontName(self, fontName):
		self.defFontName = fontName

	def AppendSpace(self, size):
		self.toolTipHeight += size
		self.ResizeToolTip()

	def AppendHorizontalLine(self):

		for i in xrange(2):
			horizontalLine = ui.Line()
			horizontalLine.SetParent(self)
			horizontalLine.SetPosition(0, self.toolTipHeight + 3 + i)
			horizontalLine.SetWindowHorizontalAlignCenter()
			horizontalLine.SetSize(150, 0)
			horizontalLine.Show()

			if 0 == i:
				horizontalLine.SetColor(0xff555555)
			else:
				horizontalLine.SetColor(0xff000000)

			self.childrenList.append(horizontalLine)

		self.toolTipHeight += 11
		self.ResizeToolTip()

	def AlignHorizonalCenter(self):
		for child in self.childrenList:
			(x, y)=child.GetLocalPosition()
			child.SetPosition(self.toolTipWidth/2, y)

		self.ResizeToolTip()

	def AutoAppendTextLine(self, text, color = FONT_COLOR, centerAlign = True, show = True):
		textLine = ui.TextLine()
		textLine.SetParent(self)
		textLine.SetFontName(self.defFontName)
		textLine.SetPackedFontColor(color)
		textLine.SetText(text)
		textLine.SetOutline()
		textLine.SetFeather(False)
		if show:
			textLine.Show()

		self.childrenList.append(textLine)

		(textWidth, textHeight)=textLine.GetTextSize()

		textWidth += 40
		textHeight += 5

		if self.toolTipWidth < textWidth:
			self.toolTipWidth = textWidth

		# @fixme017 BEGIN (moved after the set of the new width)
		if centerAlign:
			textLine.SetPosition(self.toolTipWidth/2, self.toolTipHeight)
			textLine.SetHorizontalAlignCenter()
		else:
			textLine.SetPosition(10, self.toolTipHeight)
		# @fixme017 END

		self.toolTipHeight += textHeight

		return textLine

	def AppendTextLine(self, text, color = FONT_COLOR, centerAlign = True, isBold=False, isLarge=False):
		textLine = ui.TextLine()
		textLine.SetParent(self)
		if isLarge:
			textLine.SetLargeFont()
		else:
			textLine.SetFontName(self.defFontName)
		textLine.SetPackedFontColor(color)
		if isBold:
			textLine.SetBold()
		textLine.SetText(text)
		textLine.SetOutline()
		textLine.SetFeather(False)
		textLine.Show()

		self.childrenList.append(textLine)

		self.RefreshToolTipWidth()

		if centerAlign:
			textLine.SetHorizontalAlignCenter()
			textLine.SetWindowHorizontalAlignCenter()
			textLine.SetPosition(0, self.toolTipHeight)

		else:
			textLine.SetPosition(10, self.toolTipHeight)

		self.toolTipHeight += self.TEXT_LINE_HEIGHT
		self.ResizeToolTip()

		return textLine

	def AppendDescription(self, desc, limit=0, color=FONT_COLOR, space=5):
		self.__AppendDescription_WesternLanguage(desc, color, space)

	def AppendImage(self, imageDir):
		itemImage = ui.ImageBox()
		itemImage.SetParent(self)
		itemImage.Show()
		itemImage.LoadImage(imageDir)
		itemImage.SetWindowHorizontalAlignCenter()
		itemImage.SetPosition(0, self.toolTipHeight)
		self.childrenList.append(itemImage)
		self.toolTipHeight += itemImage.GetHeight()
		return itemImage

	def AppendColumnBlock(self, columnBlock):
		columnBlock.SetParent(self)
		columnBlock.SetWindowHorizontalAlignCenter()
		columnBlock.SetPosition(0, self.toolTipHeight)
		self.childrenList.append(columnBlock)
		self.RefreshToolTipWidth()
		self.toolTipHeight += columnBlock.GetHeight()
		self.ResizeToolTip()

	if app.ENABLE_CHEQUE_SYSTEM:
		def SetThinBoardSize(self, width, height = 12) :
			self.toolTipWidth = width
			self.toolTipHeight = height

	def __AppendDescription_WesternLanguage(self, desc, color=FONT_COLOR, space=5):
		lines = SplitDescription(desc, DESC_WESTERN_MAX_COLS)
		if not lines:
			return

		self.AppendSpace(space)
		for line in lines:
			self.AppendTextLine(line, color)


	def AppendSavedPosMap(self, x=0, y=0):
		self.AppendSpace(5)

		blockedPreviewMapNames = [
			"metin2_map_nusluck01",
			"metin2_map_trent02",
			"metin2_map_spiderdungeon",
		]
		(mapName, xBase, yBase) = background.GlobalPositionToMapInfo(x, y)
		if mapName in blockedPreviewMapNames:
			self.AppendTextLine(localeInfo.SAVED_POSITION_MAP_PREVIEW_UNAVAILABLE)
			return
		else:
			self.AppendTextLine(localeInfo.SAVED_POSITION_TOOLTIP_INFO)

		(mapGlobalSizeX, mapGlobalSizeY) = background.GetMapSize(x, y)

		itemImage = ui.ExpandedImageBox()
		itemImage.AddFlag("not_pick")
		itemImage.SetWindowHorizontalAlignCenter()
		itemImage.SetPosition(0, self.toolTipHeight)
		itemImage.SetParent(self)
		itemImage.Show()
		itemImage.LoadImage("d:/ymir work/ui/atlas/%s/atlas.sub" % (mapName))
		itemImage.SetScale(0.7, 0.7)

		self.AppendSpace(itemImage.GetHeight()+5)

		scaleFactorX = float(itemImage.GetWidth()) / (mapGlobalSizeX * 256)
		scaleFactorY = float(itemImage.GetHeight()) / (mapGlobalSizeY * 256)

		pointTargetLocalX = (x - xBase) / 100
		pointTargetLocalY = (y - yBase) / 100

		pointTargetImageX = pointTargetLocalX * scaleFactorX
		pointTargetImageY = pointTargetLocalY * scaleFactorY

		targetIcon = ui.Bar()
		targetIcon.AddFlag("not_pick")
		targetIcon.SetParent(itemImage)
		# targetIcon.LoadImage("d:/ymir work/ui/nostalwork/savedPos/savedpos.tga")
		targetIcon.SetSize(5, 5)
		targetIcon.SetColor(0xff9bd0ff)
		targetIcon.SetPosition(pointTargetImageX - targetIcon.GetWidth() / 2,
							   pointTargetImageY - targetIcon.GetHeight() / 2)
		targetIcon.Show()

		targetText = ui.TextLine()
		targetText.SetParent(targetIcon)
		targetText.SetPosition(0, -14)
		targetText.SetHorizontalAlignCenter()
		targetText.SetWindowHorizontalAlignCenter()
		targetText.SetOutline()
		targetText.SetText("(%d, %d)" % (pointTargetLocalX, pointTargetLocalY))
		targetText.Show()

		self.childrenList.append(itemImage)
		self.childrenList.append(targetIcon)
		self.childrenList.append(targetText)
		self.RefreshToolTipWidth()

	def RefreshToolTipWidth(self):
		minWidth = self.TOOL_TIP_WIDTH
		for child in self.childrenList:
			if isinstance(child, ui.TextLine):
				width, height = child.GetTextSize()
				minWidth = max(minWidth, width+10)
			else:
				minWidth = max(minWidth, child.GetWidth() + 20)

		if self.toolTipWidth < minWidth:
			for child in self.childrenList:
				child.RefreshPosition()

		self.toolTipWidth = minWidth

	def ResizeToolTip(self):
		width = self.toolTipWidth
		height =  self.TOOL_TIP_HEIGHT + self.toolTipHeight
		self.SetSize(width, height)
		self.background.SetSize(width, height)

	def SetTitle(self, name):
		self.AppendTextLine(name, self.TITLE_COLOR)

	def GetLimitTextLineColor(self, curValue, limitValue):
		if curValue < limitValue:
			return self.DISABLE_COLOR

		return self.ENABLE_COLOR

	def GetChangeTextLineColor(self, value, isSpecial=False):
		if value > 0:
			if isSpecial:
				return self.SPECIAL_POSITIVE_COLOR
			else:
				return self.POSITIVE_COLOR

		if 0 == value:
			return self.NORMAL_COLOR

		return self.NEGATIVE_COLOR

	def SetToolTipPosition(self, x = -1, y = -1):
		self.xPos = x
		self.yPos = y

	def ShowToolTip(self):
		self.SetTop()
		self.Show()

		self.OnUpdate()

	def HideToolTip(self):
		self.Hide()

	def OnUpdate(self):

		if not self.followFlag:
			return

		x = 0
		y = 0
		width = self.GetWidth()
		height = self.toolTipHeight

		if -1 == self.xPos and -1 == self.yPos:

			(mouseX, mouseY) = wndMgr.GetMousePosition()

			if mouseY < wndMgr.GetScreenHeight() - 300:
				y = mouseY + 40
			else:
				y = mouseY - height - 30

			x = mouseX - width/2

		else:

			x = self.xPos - width/2
			y = self.yPos - height

		x = max(x, 0)
		y = max(y, 0)
		x = min(x + width/2, wndMgr.GetScreenWidth() - width/2) - width/2
		y = min(y + self.GetHeight(), wndMgr.GetScreenHeight()) - self.GetHeight()

		parentWindow = self.GetParentProxy()
		if parentWindow:
			(gx, gy) = parentWindow.GetGlobalPosition()
			x -= gx
			y -= gy

		self.SetPosition(x, y)

class ItemToolTip(ToolTip):

	CHARACTER_NAMES = (
		localeInfo.TOOLTIP_WARRIOR,
		localeInfo.TOOLTIP_ASSASSIN,
		localeInfo.TOOLTIP_SURA,
		localeInfo.TOOLTIP_SHAMAN
	)
	if app.ENABLE_WOLFMAN_CHARACTER:
		CHARACTER_NAMES += (
			localeInfo.TOOLTIP_WOLFMAN,
		)

	CHARACTER_COUNT = len(CHARACTER_NAMES)
	WEAR_NAMES = (
		localeInfo.TOOLTIP_ARMOR,
		localeInfo.TOOLTIP_HELMET,
		localeInfo.TOOLTIP_SHOES,
		localeInfo.TOOLTIP_WRISTLET,
		localeInfo.TOOLTIP_WEAPON,
		localeInfo.TOOLTIP_NECK,
		localeInfo.TOOLTIP_EAR,
		localeInfo.TOOLTIP_UNIQUE,
		localeInfo.TOOLTIP_SHIELD,
		localeInfo.TOOLTIP_ARROW,
	)
	WEAR_COUNT = len(WEAR_NAMES)

	ATTRIBUTE_NEED_WIDTH = {
		23 : 230,
		24 : 230,
		25 : 230,
		26 : 220,
		27 : 210,

		35 : 210,
		36 : 210,
		37 : 210,
		38 : 210,
		39 : 210,
		40 : 210,
		41 : 210,

		42 : 220,
		43 : 230,
		45 : 230,
	}

	ANTI_FLAG_DICT = {
		0 : item.ITEM_ANTIFLAG_WARRIOR,
		1 : item.ITEM_ANTIFLAG_ASSASSIN,
		2 : item.ITEM_ANTIFLAG_SURA,
		3 : item.ITEM_ANTIFLAG_SHAMAN,
	}
	if app.ENABLE_WOLFMAN_CHARACTER:
		ANTI_FLAG_DICT.update({
			4 : item.ITEM_ANTIFLAG_WOLFMAN,
		})

	FONT_COLOR = grp.GenerateColor(0.7607, 0.7607, 0.7607, 1.0)

	def __init__(self, *args, **kwargs):
		ToolTip.__init__(self, *args, **kwargs)
		self.itemVnum = 0
		self.isShopItem = False

		self.bCannotUseItemForceSetDisableColor = True

	def __del__(self):
		ToolTip.__del__(self)

	def SetCannotUseItemForceSetDisableColor(self, enable):
		self.bCannotUseItemForceSetDisableColor = enable

	def CanEquip(self):
		if not item.IsEquipmentVID(self.itemVnum):
			return True

		race = player.GetRace()
		job = chr.RaceToJob(race)
		if not self.ANTI_FLAG_DICT.has_key(job):
			return False

		if item.IsAntiFlag(self.ANTI_FLAG_DICT[job]):
			return False

		sex = chr.RaceToSex(race)

		MALE = 1
		FEMALE = 0

		if item.IsAntiFlag(item.ITEM_ANTIFLAG_MALE) and sex == MALE:
			return False

		if item.IsAntiFlag(item.ITEM_ANTIFLAG_FEMALE) and sex == FEMALE:
			return False

		for i in xrange(item.LIMIT_MAX_NUM):
			(limitType, limitValue) = item.GetLimit(i)

			if item.LIMIT_LEVEL == limitType:
				if player.GetStatus(player.LEVEL) < limitValue:
					return False
			"""
			elif item.LIMIT_STR == limitType:
				if player.GetStatus(player.ST) < limitValue:
					return False
			elif item.LIMIT_DEX == limitType:
				if player.GetStatus(player.DX) < limitValue:
					return False
			elif item.LIMIT_INT == limitType:
				if player.GetStatus(player.IQ) < limitValue:
					return False
			elif item.LIMIT_CON == limitType:
				if player.GetStatus(player.HT) < limitValue:
					return False
			"""

		return True

	def AppendTextLine(self, text, color = FONT_COLOR, centerAlign = True, isBold=False, isLarge=False, disable_color=None):
		if not self.CanEquip() and self.bCannotUseItemForceSetDisableColor:
			if disable_color != None:
				color = disable_color
			else:
				color = self.DISABLE_COLOR

		return ToolTip.AppendTextLine(self, text, color, centerAlign, isBold, isLarge)

	def ClearToolTip(self):
		self.isShopItem = False
		self.toolTipWidth = self.TOOL_TIP_WIDTH
		ToolTip.ClearToolTip(self)

	def SetRawItem(self, itemVnum, metinSlot=None, attrSlot=None, isClearToolTip=True):
		if 0 == itemVnum:
			return

		if isClearToolTip:
			self.ClearToolTip()
		if metinSlot is None:
			metinSlot = [0 for i in xrange(player.METIN_SOCKET_MAX_NUM)]
		if attrSlot is None:
			attrSlot = [(0, 0,) for i in xrange(player.ATTRIBUTE_SLOT_MAX_NUM)]

		self.AddItemData(itemVnum, metinSlot, attrSlot)

	def SetInventoryItem(self, slotIndex, window_type = player.INVENTORY):
		itemVnum = player.GetItemIndex(window_type, slotIndex)
		if 0 == itemVnum:
			return

		self.ClearToolTip()
		if shop.IsOpen():
			if not shop.IsPrivateShop():
				item.SelectItem(itemVnum)
				if app.ENABLE_CHEQUE_SYSTEM:
					self.AppendTextLine(localeInfo.CHEQUE_SYSTEM_SELL_PRICE, grp.GenerateColor(1.0, 0.9686, 0.3098, 1.0))
					self.AppendSellingPrice(player.GetISellItemPrice(window_type, slotIndex))
				else:
					self.AppendSellingPrice(player.GetISellItemPrice(window_type, slotIndex))

		metinSlot = [player.GetItemMetinSocket(window_type, slotIndex, i) for i in xrange(player.METIN_SOCKET_MAX_NUM)]
		attrSlot = [player.GetItemAttribute(window_type, slotIndex, i) for i in xrange(player.ATTRIBUTE_SLOT_MAX_NUM)]

		self.AddItemData(itemVnum, metinSlot, attrSlot, 0, 0, window_type, slotIndex)

	def SetMyShopItem(self, itemVnum, count, metinSlot, attrSlot, price=0):
		self.SetRawItem(itemVnum, metinSlot, attrSlot)
		if price > 0:
			self.AppendPrice(price, count)

	def SetShopItem(self, slotIndex):
		itemVnum = shop.GetItemID(slotIndex)
		if 0 == itemVnum:
			return

		price = shop.GetItemPrice(slotIndex)
		if app.ENABLE_CHEQUE_SYSTEM:
			cheque = shop.GetItemCheque(slotIndex)
		self.ClearToolTip()
		self.isShopItem = True

		metinSlot = []
		for i in xrange(player.METIN_SOCKET_MAX_NUM):
			metinSlot.append(shop.GetItemMetinSocket(slotIndex, i))
		attrSlot = []
		for i in xrange(player.ATTRIBUTE_SLOT_MAX_NUM):
			attrSlot.append(shop.GetItemAttribute(slotIndex, i))

		self.AddItemData(itemVnum, metinSlot, attrSlot)
		if app.ENABLE_CHEQUE_SYSTEM:
			self.AppendSpace(5)
			self.AppendTextLine(localeInfo.CHEQUE_SYSTEM_SELL_PRICE, grp.GenerateColor(1.0, 0.9686, 0.3098, 1.0))
			self.AppendCheque(cheque)
			self.AppendPrice(price)
		else:
			self.AppendPrice(price)

	def SetShopItemBySecondaryCoin(self, slotIndex):
		itemVnum = shop.GetItemID(slotIndex)
		if 0 == itemVnum:
			return

		price = shop.GetItemPrice(slotIndex)
		self.ClearToolTip()
		self.isShopItem = True

		metinSlot = []
		for i in xrange(player.METIN_SOCKET_MAX_NUM):
			metinSlot.append(shop.GetItemMetinSocket(slotIndex, i))
		attrSlot = []
		for i in xrange(player.ATTRIBUTE_SLOT_MAX_NUM):
			attrSlot.append(shop.GetItemAttribute(slotIndex, i))

		self.AddItemData(itemVnum, metinSlot, attrSlot)
		self.AppendPriceBySecondaryCoin(price)

	def SetExchangeOwnerItem(self, slotIndex):
		itemVnum = exchange.GetItemVnumFromSelf(slotIndex)
		if 0 == itemVnum:
			return

		self.ClearToolTip()

		metinSlot = []
		for i in xrange(player.METIN_SOCKET_MAX_NUM):
			metinSlot.append(exchange.GetItemMetinSocketFromSelf(slotIndex, i))
		attrSlot = []
		for i in xrange(player.ATTRIBUTE_SLOT_MAX_NUM):
			attrSlot.append(exchange.GetItemAttributeFromSelf(slotIndex, i))
		self.AddItemData(itemVnum, metinSlot, attrSlot)

	def SetExchangeTargetItem(self, slotIndex):
		itemVnum = exchange.GetItemVnumFromTarget(slotIndex)
		if 0 == itemVnum:
			return

		self.ClearToolTip()

		metinSlot = []
		for i in xrange(player.METIN_SOCKET_MAX_NUM):
			metinSlot.append(exchange.GetItemMetinSocketFromTarget(slotIndex, i))
		attrSlot = []
		for i in xrange(player.ATTRIBUTE_SLOT_MAX_NUM):
			attrSlot.append(exchange.GetItemAttributeFromTarget(slotIndex, i))
		self.AddItemData(itemVnum, metinSlot, attrSlot)

	def SetPrivateShopBuilderItem(self, invenType, invenPos, privateShopSlotIndex):
		itemVnum = player.GetItemIndex(invenType, invenPos)
		if 0 == itemVnum:
			return

		item.SelectItem(itemVnum)
		self.ClearToolTip()
		if app.ENABLE_CHEQUE_SYSTEM:
			self.AppendTextLine(localeInfo.CHEQUE_SYSTEM_SELL_PRICE, grp.GenerateColor(1.0, 0.9686, 0.3098, 1.0))
			self.AppendSellingCheque(shop.GetPrivateShopItemCheque(invenType, invenPos))
			self.AppendSellingPrice(shop.GetPrivateShopItemPrice(invenType, invenPos))
		else:
			self.AppendSellingPrice(shop.GetPrivateShopItemPrice(invenType, invenPos))

		metinSlot = []
		for i in xrange(player.METIN_SOCKET_MAX_NUM):
			metinSlot.append(player.GetItemMetinSocket(invenPos, i))
		attrSlot = []
		for i in xrange(player.ATTRIBUTE_SLOT_MAX_NUM):
			attrSlot.append(player.GetItemAttribute(invenPos, i))

		self.AddItemData(itemVnum, metinSlot, attrSlot)

	def SetSafeBoxItem(self, slotIndex):
		itemVnum = safebox.GetItemID(slotIndex)
		if 0 == itemVnum:
			return

		self.ClearToolTip()
		metinSlot = []
		for i in xrange(player.METIN_SOCKET_MAX_NUM):
			metinSlot.append(safebox.GetItemMetinSocket(slotIndex, i))
		attrSlot = []
		for i in xrange(player.ATTRIBUTE_SLOT_MAX_NUM):
			attrSlot.append(safebox.GetItemAttribute(slotIndex, i))

		self.AddItemData(itemVnum, metinSlot, attrSlot, safebox.GetItemFlags(slotIndex))

	def SetMallItem(self, slotIndex):
		itemVnum = safebox.GetMallItemID(slotIndex)
		if 0 == itemVnum:
			return

		self.ClearToolTip()
		metinSlot = []
		for i in xrange(player.METIN_SOCKET_MAX_NUM):
			metinSlot.append(safebox.GetMallItemMetinSocket(slotIndex, i))
		attrSlot = []
		for i in xrange(player.ATTRIBUTE_SLOT_MAX_NUM):
			attrSlot.append(safebox.GetMallItemAttribute(slotIndex, i))

		self.AddItemData(itemVnum, metinSlot, attrSlot)

	def SetItemToolTip(self, itemVnum):
		self.ClearToolTip()
		metinSlot = []
		for i in xrange(player.METIN_SOCKET_MAX_NUM):
			metinSlot.append(0)
		attrSlot = []
		for i in xrange(player.ATTRIBUTE_SLOT_MAX_NUM):
			attrSlot.append((0, 0))

		self.AddItemData(itemVnum, metinSlot, attrSlot)

	def SetItemShopItemToolTip(self, itemVnum, itemPrice, itemCurrency):
		self.ClearToolTip()
		metinSlot = []
		for i in xrange(player.METIN_SOCKET_MAX_NUM):
			metinSlot.append(0)
		attrSlot = []
		for i in xrange(player.ATTRIBUTE_SLOT_MAX_NUM):
			attrSlot.append((0, 0))

		if itemVnum in (72161,):
			metinSlot[0] = app.GetGlobalTimeStamp() + 3600*24*30
		if itemVnum == 72702:
			metinSlot[0] = app.GetGlobalTimeStamp() + 3600 * 24 * 7

		item.SelectItem(itemVnum)

		if item.GetItemType() == item.ITEM_TYPE_UNIQUE:
			metinSlot[2] = item.GetValue(0)

		self.AddItemData(itemVnum, metinSlot, attrSlot)

		self.AppendSpace(5)
		self.AppendHorizontalLine()
		textColor = colorinfo.DRAGON_COIN_COLOR
		if itemCurrency == uiItemShop.CURRENCY_DRAGON_MARK:
			textColor = colorinfo.DRAGON_MARK_COLOR
		self.AppendTextLine(localeInfo.TOOLTIP_BUYPRICE_CURRENCY % (localeInfo.NumberToString(itemPrice), uiItemShop.GetCurrencyShortName(itemCurrency)), colorinfo.GetColorFromColorTuple(textColor))


	def __AppendAttackSpeedInfo(self, item):
		atkSpd = item.GetValue(0)

		if atkSpd < 80:
			stSpd = localeInfo.TOOLTIP_ITEM_VERY_FAST
		elif atkSpd <= 95:
			stSpd = localeInfo.TOOLTIP_ITEM_FAST
		elif atkSpd <= 105:
			stSpd = localeInfo.TOOLTIP_ITEM_NORMAL
		elif atkSpd <= 120:
			stSpd = localeInfo.TOOLTIP_ITEM_SLOW
		else:
			stSpd = localeInfo.TOOLTIP_ITEM_VERY_SLOW

		self.AppendTextLine(localeInfo.TOOLTIP_ITEM_ATT_SPEED % stSpd, self.NORMAL_COLOR)

	def __AppendAttackGradeInfo(self):
		atkGrade = item.GetValue(1)
		self.AppendTextLine(localeInfo.TOOLTIP_ITEM_ATT_GRADE % atkGrade, self.GetChangeTextLineColor(atkGrade))

	if app.ENABLE_ACCE_COSTUME_SYSTEM:
		def CalcAcceValue(self, value, abs):
			if not value:
				return 0

			valueCalc	= round(value * abs) / 100
			valueCalc	-= 0.5
			valueCalc	= int(valueCalc) +1 if valueCalc > 0 else int(valueCalc)
			value		= 1 if (valueCalc <= 0 and value > 0) else valueCalc
			return value

	def __AppendAttackPowerInfo(self, itemAbsChance = 0):
		minPower = item.GetValue(3)
		maxPower = item.GetValue(4)
		addPower = item.GetValue(5)
		if app.ENABLE_ACCE_COSTUME_SYSTEM:
			if itemAbsChance != 0:
				minPower = self.CalcAcceValue(minPower, itemAbsChance)
				maxPower = self.CalcAcceValue(maxPower, itemAbsChance)
				addPower = self.CalcAcceValue(addPower, itemAbsChance)

		if maxPower > minPower:
			self.AppendTextLine(localeInfo.TOOLTIP_ITEM_ATT_POWER % (minPower+addPower, maxPower+addPower), self.POSITIVE_COLOR)
		else:
			self.AppendTextLine(localeInfo.TOOLTIP_ITEM_ATT_POWER_ONE_ARG % (minPower+addPower), self.POSITIVE_COLOR)

	def __AppendMagicAttackInfo(self, itemAbsChance = 0):
		minMagicAttackPower = item.GetValue(1)
		maxMagicAttackPower = item.GetValue(2)
		addPower			= item.GetValue(5)
		if app.ENABLE_ACCE_COSTUME_SYSTEM:
			if itemAbsChance != 0:
				minMagicAttackPower = self.CalcAcceValue(minMagicAttackPower, itemAbsChance)
				maxMagicAttackPower = self.CalcAcceValue(maxMagicAttackPower, itemAbsChance)
				addPower			= self.CalcAcceValue(addPower, itemAbsChance)

		if minMagicAttackPower > 0 or maxMagicAttackPower > 0:
			if maxMagicAttackPower > minMagicAttackPower:
				self.AppendTextLine(localeInfo.TOOLTIP_ITEM_MAGIC_ATT_POWER % (minMagicAttackPower+addPower, maxMagicAttackPower+addPower), self.POSITIVE_COLOR)
			else:
				self.AppendTextLine(localeInfo.TOOLTIP_ITEM_MAGIC_ATT_POWER_ONE_ARG % (minMagicAttackPower+addPower), self.POSITIVE_COLOR)

	def __AppendMagicDefenceInfo(self, itemAbsChance = 0):
		magicDefencePower = item.GetValue(0)
		if app.ENABLE_ACCE_COSTUME_SYSTEM:
			if itemAbsChance != 0:
				magicDefencePower = self.CalcAcceValue(magicDefencePower, itemAbsChance)

		if magicDefencePower > 0:
			self.AppendTextLine(localeInfo.TOOLTIP_ITEM_MAGIC_DEF_POWER % magicDefencePower, self.GetChangeTextLineColor(magicDefencePower))
	if app.ENABLE_IKASHOP_RENEWAL:
		def AppendAttributes(self, attrs):
			attrs = list(attrs)
			for i in xrange(player.ATTRIBUTE_SLOT_MAX_NUM - len(attrs)):
				attrs.append((0,0))
			self.__AppendAttributeInformation(attrs)

	def format_attributes(self, input_string, color):
		formatted_lines = []

		for line in input_string.splitlines():
			# Find the first instance of "+" or a digit
			match = re.search(r"[+\-]?\d+(\.\d+)?%?", line)
			if match:
				start_index = match.start()
				end_index = match.end()
				# Add |cff{color} at the start and |r right after the numeric part
				formatted_line = (
						line[:start_index]
						+ "|cff{0}".format(color)
						+ line[start_index:end_index]
						+ "|r"
						+ line[end_index:]
				)
			else:
				# If no number or "+" is found, leave the line unchanged
				formatted_line = line

			formatted_lines.append(formatted_line)

		return "\n".join(formatted_lines)

	def __AppendAttributeInformation(self, attrSlot, itemAbsChance = 0):
		if 0 != attrSlot:
			for i in xrange(player.ATTRIBUTE_SLOT_NORM_NUM):
				type = attrSlot[i][0]
				value = attrSlot[i][1]

				if 0 == value:
					continue

				affectString = localeInfo.GetApplyString(type, value)
				if app.ENABLE_ACCE_COSTUME_SYSTEM:
					if item.GetItemType() == item.ITEM_TYPE_COSTUME:
						if item.GetItemSubType() == item.COSTUME_TYPE_ACCE:
							if itemAbsChance != 0:
								value = self.CalcAcceValue(value, itemAbsChance)
								affectString = localeInfo.GetApplyString(type, value)

				if affectString:
					if value < 0:
						affectString = affectString.replace("+", "")
					affectColor = self.__GetAttributeColor(i, value)

					isMaxBonus = localeinfo_point.ATTR_MAX_VALUES.has_key(type) and localeinfo_point.ATTR_MAX_VALUES[type] <= value

					if isMaxBonus:
						value_color = "ffb52b"
						if not self.CanEquip() and self.bCannotUseItemForceSetDisableColor:
							value_color = "ff6e54"
						affectString = self.format_attributes(affectString, value_color)


					text = self.AppendTextLine(affectString, affectColor, disable_color=self.SPECIAL_NEGATIVE_COLOR)
					# if isMaxBonus:
						# affectString += "|cffff9500*|r"
						# text.SetOutline()
						# if not self.CanEquip() and self.bCannotUseItemForceSetDisableColor:
						# 	text.SetPackedOutlineColor(0xff991d1d)
						# else:
						# 	text.SetPackedOutlineColor(0xff106323)


	def __GetAttributeColor(self, index, value):
		if value > 0:
			if index >= player.ATTRIBUTE_SLOT_RARE_START and index < player.ATTRIBUTE_SLOT_RARE_END:
				return self.SPECIAL_POSITIVE_COLOR2
			else:
				if index == player.ATTRIBUTE_SLOT_NORM_END:
					return self.SPECIAL_POSITIVE_COLOR5
				else:
					return self.SPECIAL_POSITIVE_COLOR
		elif value == 0:
			return self.NORMAL_COLOR
		else:
			return self.NEGATIVE_COLOR

	def __IsPolymorphItem(self, itemVnum):
		if itemVnum >= 70103 and itemVnum <= 70107:
			return 1
		return 0

	def __SetPolymorphItemTitle(self, monsterVnum):
		if localeInfo.IsVIETNAM():
			itemName =item.GetItemName()
			itemName+=" "
			itemName+=nonplayer.GetMonsterName(monsterVnum)
		else:
			itemName =nonplayer.GetMonsterName(monsterVnum)
			itemName+=" "
			itemName+=item.GetItemName()
		self.SetTitle(itemName)

	def __SetNormalItemTitle(self):
		self.SetTitle(item.GetItemName())

	def __SetSpecialItemTitle(self):
		self.AppendTextLine(item.GetItemName(), self.SPECIAL_TITLE_COLOR)

	def __SetItemTitle(self, itemVnum, metinSlot, attrSlot):
		if self.__IsPolymorphItem(itemVnum):
			self.__SetPolymorphItemTitle(metinSlot[0])
		else:
			if self.__IsAttr(attrSlot):
				self.__SetSpecialItemTitle()
				return

			self.__SetNormalItemTitle()

	def __IsAttr(self, attrSlot):
		if not attrSlot:
			return False

		for i in xrange(player.ATTRIBUTE_SLOT_NORM_NUM):
			type = attrSlot[i][0]
			if 0 != type:
				return True

		return False

	def AddRefineItemData(self, itemVnum, metinSlot, attrSlot = 0):
		for i in xrange(player.METIN_SOCKET_MAX_NUM):
			metinSlotData=metinSlot[i]
			if self.GetMetinItemIndex(metinSlotData) == constInfo.ERROR_METIN_STONE:
				metinSlot[i]=player.METIN_SOCKET_TYPE_SILVER

		self.AddItemData(itemVnum, metinSlot, attrSlot)

	def AddItemData_Offline(self, itemVnum, itemDesc, itemSummary, metinSlot, attrSlot):
		self.__AdjustMaxWidth(attrSlot, itemDesc)
		self.__SetItemTitle(itemVnum, metinSlot, attrSlot)

		if self.__IsHair(itemVnum):
			self.__AppendHairIcon(itemVnum)

		### Description ###
		self.AppendDescription(itemDesc, 26)
		self.AppendDescription(itemSummary, 26, self.CONDITION_COLOR)

	def AddItemData(self, itemVnum, metinSlot, attrSlot = 0, flags = 0, unbindTime = 0, window_type = player.INVENTORY, slotIndex = -1, **kwargs):
		self.itemVnum = itemVnum
		item.SelectItem(itemVnum)
		itemType = item.GetItemType()
		itemSubType = item.GetItemSubType()

		#self.auxiliaryDict = kwargs.get("auxiliaryDict", player.GetAuxiliaryString(window_type, slotIndex))

		if 50026 == itemVnum:
			if 0 != metinSlot:
				name = item.GetItemName()
				if metinSlot[0] > 0:
					name += " "
					name += localeInfo.NumberToMoneyString(metinSlot[0])
			self.SetTitle(name)
			self.__AppendSealInformation(window_type, slotIndex) ## cyh itemseal 2013 11 11
			self.AdditionalTips(window_type, itemVnum, metinSlot, slotIndex)
			self.ShowToolTip()
			return

		### Skill Book ###
		elif 50300 == itemVnum:
			if 0 != metinSlot:
				self.__SetSkillBookToolTip(metinSlot[0], localeInfo.TOOLTIP_SKILLBOOK_NAME, 1)
			self.__AppendSealInformation(window_type, slotIndex) ## cyh itemseal 2013 11 11
			self.AdditionalTips(window_type, itemVnum, metinSlot, slotIndex)
			self.ShowToolTip()
			return
		elif 70037 == itemVnum:
			if 0 != metinSlot:
				self.__SetSkillBookToolTip(metinSlot[0], localeInfo.TOOLTIP_SKILL_FORGET_BOOK_NAME, 0)
			self.AppendDescription(item.GetItemDescription(), 26)
			self.AppendDescription(item.GetItemSummary(), 26, self.CONDITION_COLOR)
			self.__AppendSealInformation(window_type, slotIndex) ## cyh itemseal 2013 11 11
			self.AdditionalTips(window_type, itemVnum, metinSlot, slotIndex)
			self.ShowToolTip()
			return
		elif 70055 == itemVnum:
			if 0 != metinSlot:
				self.__SetSkillBookToolTip(metinSlot[0], localeInfo.TOOLTIP_SKILL_FORGET_BOOK_NAME, 0)
			self.AppendDescription(item.GetItemDescription(), 26)
			self.AppendDescription(item.GetItemSummary(), 26, self.CONDITION_COLOR)
			self.__AppendSealInformation(window_type, slotIndex) ## cyh itemseal 2013 11 11
			self.AdditionalTips(window_type, itemVnum, metinSlot, slotIndex)
			self.ShowToolTip()
			return
		###########################################################################################


		itemDesc = item.GetItemDescription()
		itemSummary = item.GetItemSummary()

		isCostumeItem = 0
		isCostumeHair = 0
		isCostumeBody = 0
		if app.ENABLE_MOUNT_COSTUME_SYSTEM:
			isCostumeMount = 0
		if app.ENABLE_ACCE_COSTUME_SYSTEM:
			isCostumeAcce = 0
		if app.ENABLE_WEAPON_COSTUME_SYSTEM:
			isCostumeWeapon = 0

		if app.ENABLE_COSTUME_SYSTEM:
			if item.ITEM_TYPE_COSTUME == itemType:
				isCostumeItem = 1
				isCostumeHair = item.COSTUME_TYPE_HAIR == itemSubType
				isCostumeBody = item.COSTUME_TYPE_BODY == itemSubType
				if app.ENABLE_MOUNT_COSTUME_SYSTEM:
					isCostumeMount = item.COSTUME_TYPE_MOUNT == itemSubType
				if app.ENABLE_ACCE_COSTUME_SYSTEM:
					isCostumeAcce = item.COSTUME_TYPE_ACCE == itemSubType
				if app.ENABLE_WEAPON_COSTUME_SYSTEM:
					isCostumeWeapon = item.COSTUME_TYPE_WEAPON == itemSubType

				#dbg.TraceError("IS_COSTUME_ITEM! body(%d) hair(%d)" % (isCostumeBody, isCostumeHair))

		self.__SetItemTitle(itemVnum, metinSlot, attrSlot)

		### Hair Preview Image ###
		if self.__IsHair(itemVnum):
			self.__AppendHairIcon(itemVnum)

		### Description ###
		self.AppendDescription(itemDesc, 26)
		self.AppendDescription(itemSummary, 26, self.CONDITION_COLOR)

		### Weapon ###
		if item.ITEM_TYPE_WEAPON == itemType:

			self.__AppendLimitInformation()

			self.AppendSpace(5)

			if itemSubType == item.WEAPON_ARROW:
				damage_percent = 100 - item.GetValue(2)
				damage_percent_mob = 100 - min(item.GetValue(2) * 2, 100)
				add_damage = item.GetValue(3)
				min_distance = float(item.GetValue(4)) / 100
				max_distance = float(item.GetValue(5)) / 100
				self.AppendTextLine(localeInfo.TOOLTIP_ARROW_DAMAGE_BONUS % add_damage, self.POSITIVE_COLOR)
				self.AppendTextLine(localeInfo.TOOLTIP_ARROW_DAMAGE_REDUCTION % (damage_percent, damage_percent_mob),
									self.NEGATIVE_COLOR)
				self.AppendTextLine(localeInfo.TOOLTIP_ARROW_MIN_RADIUS % min_distance, self.POSITIVE_COLOR)
				self.AppendTextLine(localeInfo.TOOLTIP_ARROW_MAX_RADIUS % max_distance, self.POSITIVE_COLOR)

			elif item.WEAPON_FAN == itemSubType:
				self.__AppendMagicAttackInfo()
				self.__AppendAttackPowerInfo()

			else:
				self.__AppendAttackPowerInfo()
				self.__AppendMagicAttackInfo()

			self.__AppendAffectInformation(attrSlot)
			self.__AppendAttributeInformation(attrSlot)

			self.AppendWearableInformation()

			if app.ENABLE_QUIVER_SYSTEM and item.WEAPON_QUIVER == itemSubType:
				self.AppendLastTimeInformation(metinSlot)
			else:
				self.__AppendMetinSlotInfo(metinSlot)

		### Armor ###
		elif item.ITEM_TYPE_ARMOR == itemType:
			self.__AppendLimitInformation()

			defGrade = item.GetValue(1)
			defBonus = item.GetValue(5)*2
			if defGrade > 0:
				self.AppendSpace(5)
				self.AppendTextLine(localeInfo.TOOLTIP_ITEM_DEF_GRADE % (defGrade+defBonus), self.GetChangeTextLineColor(defGrade))

			self.__AppendMagicDefenceInfo()
			self.__AppendAffectInformation()
			self.__AppendAttributeInformation(attrSlot)

			self.AppendWearableInformation()

			if itemSubType in (item.ARMOR_WRIST, item.ARMOR_NECK, item.ARMOR_EAR):
				self.__AppendAccessoryMetinSlotInfo(metinSlot, constInfo.GET_ACCESSORY_MATERIAL_VNUM(itemVnum, itemSubType))
			else:
				self.__AppendMetinSlotInfo(metinSlot)

		### Ring Slot Item (Not UNIQUE) ###
		elif item.ITEM_TYPE_RING == itemType:
			self.__AppendLimitInformation()
			self.__AppendAffectInformation()
			self.__AppendAttributeInformation(attrSlot)

			#self.__AppendAccessoryMetinSlotInfo(metinSlot, 99001)


		### Belt Item ###
		elif item.ITEM_TYPE_BELT == itemType:
			self.__AppendLimitInformation()
			self.__AppendAffectInformation()
			self.__AppendAttributeInformation(attrSlot)

			self.__AppendAccessoryMetinSlotInfo(metinSlot, constInfo.GET_BELT_MATERIAL_VNUM(itemVnum))

		elif app.ENABLE_PET_SYSTEM_EX and item.ITEM_TYPE_PET == itemType:
			self.__AppendLimitInformation()
			self.__AppendAffectInformation()
			self.__AppendAttributeInformation(attrSlot)
			self.AppendWearableInformation()
			self.AppendLastTimeInformation(metinSlot)

		elif isCostumeItem:
			self.__AppendLimitInformation()
			self.__AppendAffectInformation()
			if app.ENABLE_ACCE_COSTUME_SYSTEM and isCostumeAcce:
				## ABSORPTION RATE
				absChance = int(metinSlot[acce.ABSORPTION_SOCKET])
				self.AppendTextLine(localeInfo.ACCE_ABSORB_CHANCE % (absChance), self.CONDITION_COLOR)
				## END ABSORPTION RATE

				itemAbsorbedVnum = int(metinSlot[acce.ABSORBED_SOCKET])
				if itemAbsorbedVnum:
					## ATTACK / DEFENSE
					item.SelectItem(itemAbsorbedVnum)
					if item.GetItemType() == item.ITEM_TYPE_WEAPON:
						if item.GetItemSubType() == item.WEAPON_FAN:
							self.__AppendMagicAttackInfo(metinSlot[acce.ABSORPTION_SOCKET])
							item.SelectItem(itemAbsorbedVnum)
							self.__AppendAttackPowerInfo(metinSlot[acce.ABSORPTION_SOCKET])
						else:
							self.__AppendAttackPowerInfo(metinSlot[acce.ABSORPTION_SOCKET])
							item.SelectItem(itemAbsorbedVnum)
							self.__AppendMagicAttackInfo(metinSlot[acce.ABSORPTION_SOCKET])
					elif item.GetItemType() == item.ITEM_TYPE_ARMOR:
						defGrade = item.GetValue(1)
						defBonus = item.GetValue(5) * 2
						defGrade = self.CalcAcceValue(defGrade, metinSlot[acce.ABSORPTION_SOCKET])
						defBonus = self.CalcAcceValue(defBonus, metinSlot[acce.ABSORPTION_SOCKET])

						if defGrade > 0:
							self.AppendSpace(5)
							self.AppendTextLine(localeInfo.TOOLTIP_ITEM_DEF_GRADE % (defGrade + defBonus), self.GetChangeTextLineColor(defGrade))

						item.SelectItem(itemAbsorbedVnum)
						self.__AppendMagicDefenceInfo(metinSlot[acce.ABSORPTION_SOCKET])
					## END ATTACK / DEFENSE

					## EFFECT
					item.SelectItem(itemAbsorbedVnum)
					for i in xrange(item.ITEM_APPLY_MAX_NUM):
						(affectType, affectValue) = item.GetAffect(i)
						affectValue = self.CalcAcceValue(affectValue, metinSlot[acce.ABSORPTION_SOCKET])
						affectString = localeInfo.GetApplyString(affectType, affectValue)
						if (not app.USE_ACCE_ABSORB_WITH_NO_NEGATIVE_BONUS and affectString and affectValue != 0)\
								or (app.USE_ACCE_ABSORB_WITH_NO_NEGATIVE_BONUS and affectString and affectValue > 0):
							self.AppendTextLine(affectString, self.GetChangeTextLineColor(affectValue))

						item.SelectItem(itemAbsorbedVnum)
					# END EFFECT

					item.SelectItem(itemVnum)
					## ATTR
					self.__AppendAttributeInformation(attrSlot, metinSlot[acce.ABSORPTION_SOCKET])
					# END ATTR
				else:
					# ATTR
					self.__AppendAttributeInformation(attrSlot)
					# END ATTR
			else:
				self.__AppendAttributeInformation(attrSlot)

			self.AppendWearableInformation()
			self.AppendLastTimeInformation(metinSlot)

		## Rod ##
		elif item.ITEM_TYPE_ROD == itemType:

			if 0 != metinSlot:
				curLevel = item.GetValue(0) / 10
				curEXP = metinSlot[0]
				bait = metinSlot[2]
				maxEXP = item.GetValue(2)
				self.__AppendLimitInformation()
				self.__AppendRodInformation(curLevel, curEXP, maxEXP, bait)

		## Pick ##
		elif item.ITEM_TYPE_PICK == itemType:

			if 0 != metinSlot:
				curLevel = item.GetValue(0) / 10
				curEXP = metinSlot[0]
				maxEXP = item.GetValue(2)
				self.__AppendLimitInformation()
				self.__AppendPickInformation(curLevel, curEXP, maxEXP)

		elif item.ITEM_TYPE_HERB_KNIFE == itemType:

			if 0 != metinSlot:
				curEXP = metinSlot[0]
				maxEXP = item.GetValue(2)
				self.__AppendLimitInformation()
				if maxEXP > 0:
					self.__AppendHerbKnifeInformation(curEXP, maxEXP)

				bonus = item.GetValue(0)
				if bonus > 0:
					self.AppendTextLine(localeInfo.GATHER_SKILL_BONUS % bonus, self.POSITIVE_COLOR)

		## Lottery ##
		elif item.ITEM_TYPE_LOTTERY == itemType:
			if 0 != metinSlot:

				ticketNumber = int(metinSlot[0])
				stepNumber = int(metinSlot[1])

				self.AppendSpace(5)
				self.AppendTextLine(localeInfo.TOOLTIP_LOTTERY_STEP_NUMBER % (stepNumber), self.NORMAL_COLOR)
				self.AppendTextLine(localeInfo.TOOLTIP_LOTTO_NUMBER % (ticketNumber), self.NORMAL_COLOR);

		### Metin ###
		elif item.ITEM_TYPE_METIN == itemType:
			self.AppendMetinInformation()
			self.AppendMetinWearInformation()

		### Fish ###
		elif item.ITEM_TYPE_FISH == itemType:
			if 0 != metinSlot:
				self.__AppendFishInfo(metinSlot[0])

		## item.ITEM_TYPE_BLEND
		elif item.ITEM_TYPE_BLEND == itemType:
			self.__AppendLimitInformation()

			if metinSlot:
				affectType = metinSlot[0]
				affectValue = metinSlot[1]
				time = metinSlot[2]
				self.AppendSpace(5)
				affectText = localeInfo.GetApplyString(affectType, affectValue)

				self.AppendTextLine(affectText, self.NORMAL_COLOR)

				if time > 0:
					minute = (time / 60)
					second = (time % 60)
					timeString = localeInfo.TOOLTIP_POTION_TIME

					if minute > 0:
						timeString += str(minute) + localeInfo.TOOLTIP_POTION_MIN
					if second > 0:
						timeString += " " + str(second) + localeInfo.TOOLTIP_POTION_SEC

					self.AppendTextLine(timeString)
				else:
					self.AppendTextLine(localeInfo.BLEND_POTION_NO_TIME)
			else:
				self.AppendTextLine("BLEND_POTION_NO_INFO")

		elif item.ITEM_TYPE_UNIQUE == itemType:
			if 0 != metinSlot:
				bHasRealtimeFlag = 0

				for i in xrange(item.LIMIT_MAX_NUM):
					(limitType, limitValue) = item.GetLimit(i)

					if item.LIMIT_REAL_TIME == limitType:
						bHasRealtimeFlag = 1


				if 1 == bHasRealtimeFlag:
					self.AppendMallItemLastTime(metinSlot[0])

				if 0 == bHasRealtimeFlag or itemVnum == 27620:
					time = metinSlot[2]

					if 1 == item.GetValue(2):
						self.AppendMallItemLastTime(time)
					else:
						self.AppendUniqueItemLastTime(time)

				if itemSubType == 4: # UNIQUE_USE_COUNT
					self.AppendSpace(5)
					if metinSlot[0] == item.GetValue(0):
						self.AppendTextLine(localeInfo.TOOLTIP_REST_USABLE_COUNT % (metinSlot[0] - metinSlot[1]))


		elif item.ITEM_TYPE_POTION == itemType:
			group_name = {
				0: localeInfo.TOOLTIP_POTION_GROUP_GENERAL_USE,
				1: localeInfo.TOOLTIP_POTION_GROUP_BOOST,
				2: localeInfo.TOOLTIP_POTION_GROUP_OFFENSIVE,
				3: localeInfo.TOOLTIP_POTION_GROUP_DEFENSIVE,
				4: localeInfo.TOOLTIP_POTION_GROUP_FOOD,
			}
			duration = item.GetValue(0)
			group = item.GetValue(1)
			cooldown = item.GetValue(2)
			self.AppendTextLine(localeInfo.TOOLTIP_POTION_GROUP % group_name[group])
			self.__AppendLimitInformation()
			self.__AppendAffectInformation()

			if duration > 0:
				extend = metinSlot[0]
				duration = duration * (100 + extend) / 100
				self.AppendTextLine("%s %s" % (localeInfo.TOOLTIP_POTION_TIME, localeInfo.SecondToSmartDHMS(duration)))
			else:
				self.AppendTextLine(localeInfo.AFFECT_INFINITE)
			if cooldown > 0:
				self.AppendTextLine(localeInfo.TOOLTIP_POTION_COOLDOWN % localeInfo.SecondToSmartDHMS(cooldown))

		### Use ###
		elif item.ITEM_TYPE_USE == itemType:
			self.__AppendLimitInformation()

			if item.USE_POTION == itemSubType or item.USE_POTION_NODELAY == itemSubType:
				self.__AppendPotionInformation()

			elif item.USE_ABILITY_UP == itemSubType:
				self.__AppendAbilityPotionInformation()

			elif item.USE_VIP == itemSubType:
				premiumType = item.GetValue(0)
				duration = item.GetValue(1)

				benefit_by_type = {
					player.PREMIUM_EXP: localeInfo.SUBSCRIPTION_BENEFIT_EXP,
					player.PREMIUM_ITEM: localeInfo.SUBSCRIPTION_BENEFIT_DROP,
					player.PREMIUM_AUTOLOOT: localeInfo.SUBSCRIPTION_BENEFIT_PICKUP,
					player.PREMIUM_FISH_MIND: localeInfo.SUBSCRIPTION_BENEFIT_FISH,
					player.PREMIUM_GOLD: localeInfo.SUBSCRIPTION_BENEFIT_GOLD,
					player.PREMIUM_SHOP: localeInfo.SUBSCRIPTION_BENEFIT_SHOP,
				}
				if benefit_by_type.has_key(premiumType):
					desc = benefit_by_type[premiumType]
					descSplit = desc.split("[ENTER]")
					for d in descSplit:
						self.AppendTextLine(d, self.POSITIVE_COLOR)

				self.AppendTextLine(localeInfo.SecondToDHM(duration * 3600))

			elif item.USE_BAIT == itemSubType:
				poolBonus = item.GetValue(0) / 10
				if poolBonus > 0:
					self.AppendTextLine(localeInfo.FISHING_POOL_BONUS % poolBonus, self.POSITIVE_COLOR)

			elif item.USE_TUNING == itemSubType:
				refineScrollType = item.GetValue(0)
				if refineScrollType:
					bonusFlag = item.GetValue(2)
					item_type = item.GetValue(3)
					use_level = item.GetValue(4)
					refine_level = item.GetValue(5)
					self.AppendSpace(5)
					if bonusFlag & item.TUNING_FLAG_100_CHANCE:
						self.AppendTextLine(localeInfo.REFINE_BONUS_100_CHANCE, self.POSITIVE_COLOR)

					if bonusFlag & item.TUNING_FLAG_NO_GOLD:
						self.AppendTextLine(localeInfo.REFINE_BONUS_NO_GOLD, self.POSITIVE_COLOR)

					if bonusFlag & item.TUNING_FLAG_NO_ITEM:
						self.AppendTextLine(localeInfo.REFINE_BONUS_NO_ITEM, self.POSITIVE_COLOR)

					self.AppendSpace(5)
					itemTypeString = [localeInfo.REFINE_BONUS_ITEM_ALL, localeInfo.REFINE_BONUS_ITEM_ARMOR,
									  localeInfo.REFINE_BONUS_ITEM_WEAPON, localeInfo.REFINE_BONUS_ITEM_JEWELRY]
					self.AppendTextLine(localeInfo.TOOLTIP_REFINE_BONUS_ITEM_TYPE % itemTypeString[item_type])
					self.AppendTextLine(localeInfo.REFINE_BONUS_USE_LEVEL % use_level)
					self.AppendTextLine(localeInfo.REFINE_BONUS_REFINE_LEVEL % (refine_level - 1, refine_level))

			if itemVnum == 72161:  # szklo intuicji+
				self.AppendSpace(5)
				self.AppendMallItemLastTime(metinSlot[0])

			if itemVnum == 50200: # tobol
				self.AppendSpace(5)
				isLevel = player.GetStatus(player.LEVEL) >= 15
				mobCount = special_flags.GetFlag("stat_monster")
				isMob = mobCount >= 800

				self.AppendTextLine(localeInfo.TOOLTIP_OPEN_SHOP_REQ_LEVEL, color=self.POSITIVE_COLOR if isLevel else self.NEGATIVE_COLOR)

				mob_text = localeInfo.TOOLTIP_OPEN_SHOP_REQ_MONSTER
				if not isMob:
					mob_text += " (%d)" % mobCount
				self.AppendTextLine(mob_text, color=self.POSITIVE_COLOR if isMob else self.NEGATIVE_COLOR)

			if 27989 == itemVnum or 76006 == itemVnum:
				if 0 != metinSlot:
					useCount = int(metinSlot[0])

					self.AppendSpace(5)
					self.AppendTextLine(localeInfo.TOOLTIP_REST_USABLE_COUNT % (6 - useCount), self.NORMAL_COLOR)

			elif 50004 == itemVnum:
				if 0 != metinSlot:
					useCount = int(metinSlot[0])

					self.AppendSpace(5)
					self.AppendTextLine(localeInfo.TOOLTIP_REST_USABLE_COUNT % (10 - useCount), self.NORMAL_COLOR)

			elif constInfo.IS_AUTO_POTION(itemVnum):
				if 0 != metinSlot:
					isActivated = int(metinSlot[0])
					usedAmount = float(metinSlot[1])
					totalAmount = float(metinSlot[2])

					if 0 == totalAmount:
						totalAmount = 1

					self.AppendSpace(5)

					if 0 != isActivated:
						self.AppendTextLine("(%s)" % (localeInfo.TOOLTIP_AUTO_POTION_USING), self.SPECIAL_POSITIVE_COLOR)
						self.AppendSpace(5)

					self.AppendTextLine(localeInfo.TOOLTIP_AUTO_POTION_REST % (100.0 - ((usedAmount / totalAmount) * 100.0)), self.POSITIVE_COLOR)

					if usedAmount == totalAmount:
						self.AppendSpace(5)
						self.AppendDescription(localeInfo.POTION_RECHARGE_TOOLTIP)

			elif itemVnum in WARP_SCROLLS:
				if 0 != metinSlot:
					xPos = int(metinSlot[0])
					yPos = int(metinSlot[1])

					if xPos != 0 and yPos != 0:
						self.AppendSavedPosMap(xPos, yPos)
						(mapName, xBase, yBase) = background.GlobalPositionToMapInfo(xPos, yPos)

						localeMapName=localeInfo.MINIMAP_ZONE_NAME_DICT.get(mapName, "")

						self.AppendSpace(5)

						if localeMapName!="":
							self.AppendTextLine(localeInfo.TOOLTIP_MEMORIZED_POSITION % (localeMapName, int(xPos-xBase)/100, int(yPos-yBase)/100), self.NORMAL_COLOR)
						else:
							self.AppendTextLine(localeInfo.TOOLTIP_MEMORIZED_POSITION_ERROR % (int(xPos)/100, int(yPos)/100), self.NORMAL_COLOR)
							dbg.TraceError("NOT_EXIST_IN_MINIMAP_ZONE_NAME_DICT: %s" % mapName)

			#####
			if item.USE_SPECIAL == itemSubType:
				if not self.AppendLastTimeInformation(metinSlot):
					if 0 != metinSlot:
						time = metinSlot[player.METIN_SOCKET_MAX_NUM-1]

						if 1 == item.GetValue(2):
							self.AppendMallItemLastTime(time)

			elif item.USE_TIME_CHARGE_PER == itemSubType:
				if metinSlot[2]:
					self.AppendTextLine(localeInfo.TOOLTIP_TIME_CHARGER_PER(metinSlot[2]))
				else:
					self.AppendTextLine(localeInfo.TOOLTIP_TIME_CHARGER_PER(item.GetValue(0)))

				self.AppendLastTimeInformation(metinSlot)

			elif item.USE_TIME_CHARGE_FIX == itemSubType:
				if metinSlot[2]:
					self.AppendTextLine(localeInfo.TOOLTIP_TIME_CHARGER_FIX(metinSlot[2]))
				else:
					self.AppendTextLine(localeInfo.TOOLTIP_TIME_CHARGER_FIX(item.GetValue(0)))

				self.AppendLastTimeInformation(metinSlot)

		elif item.ITEM_TYPE_QUEST == itemType or item.ITEM_TYPE_SPECIAL == itemType:
			self.__AppendAffectInformation()
			for i in xrange(item.LIMIT_MAX_NUM):
				(limitType, limitValue) = item.GetLimit(i)

				if item.LIMIT_REAL_TIME == limitType:
					self.AppendMallItemLastTime(metinSlot[0])
				elif item.LIMIT_LEVEL == limitType:
					self.__AppendLimitInformation()
		elif item.ITEM_TYPE_DS == itemType:
			self.AppendTextLine(self.__DragonSoulInfoString(itemVnum))
			self.__AppendAttributeInformation(attrSlot)
		else:
			self.__AppendLimitInformation()

		for i in xrange(item.LIMIT_MAX_NUM):
			(limitType, limitValue) = item.GetLimit(i)
			#dbg.TraceError("LimitType : %d, limitValue : %d" % (limitType, limitValue))

			if item.LIMIT_REAL_TIME_START_FIRST_USE == limitType:
				self.AppendRealTimeStartFirstUseLastTime(item, metinSlot, i)
				#dbg.TraceError("2) REAL_TIME_START_FIRST_USE flag On ")

			elif item.LIMIT_TIMER_BASED_ON_WEAR == limitType:
				self.AppendTimerBasedOnWearLastTime(metinSlot)
				#dbg.TraceError("1) REAL_TIME flag On ")

		self.__AppendSealInformation(window_type, slotIndex) ## cyh itemseal 2013 11 11

		self.AdditionalTips(window_type, itemVnum, metinSlot, slotIndex)
		self.__AppendAntiFlagInformation()
		self.ShowToolTip()

	def AdditionalTips(self, window_type, itemVnum, metinSlot, slotIndex):
		item.SelectItem(itemVnum)
		itemType = item.GetItemType()
		itemSubType = item.GetItemSubType()

		if app.ENABLE_EMOJI_SYSTEM and item.IsFlag(item.ITEM_FLAG_STACKABLE) and window_type in (player.INVENTORY,):
			if player.GetItemCount(window_type, slotIndex) > 1:
				self.AppendTextLine(localeInfo.TOOLTIP_TIPS_SPLIT_ITEM.format(Emoji("icon\shortcut\shift.tga"), Emoji("icon\shortcut\mleft.tga")))

		if chr.IsGameMaster(0):
			self.AppendSpace(5)
			self.AppendTextLine("VNUM: {}".format(itemVnum), 0xFFf863ff)
			self.AppendSpace(3)
			self.AppendTextLine("TYPE: {} SUBTYPE {}".format(itemType, itemSubType), 0xFF00cfb6)
			if metinSlot:
				self.AppendSpace(3)
				self.AppendTextLine("SOCKET: {}".format(','.join([str(i) for i in metinSlot])), 0xFF00b6d6)
			if isinstance(self.auxiliaryDict, dict) and self.auxiliaryDict:
				self.AppendTextLine("Auxs: ")
				for _, val in self.auxiliaryDict.items():
					self.AppendTextLine("Key: [{}] Value: [{}]".format(_, val))

	def __DragonSoulInfoString (self, dwVnum):
		step = (dwVnum / 100) % 10
		refine = (dwVnum / 10) % 10
		if 0 == step:
			return localeInfo.DRAGON_SOUL_STEP_LEVEL1 + " " + localeInfo.DRAGON_SOUL_STRENGTH(refine)
		elif 1 == step:
			return localeInfo.DRAGON_SOUL_STEP_LEVEL2 + " " + localeInfo.DRAGON_SOUL_STRENGTH(refine)
		elif 2 == step:
			return localeInfo.DRAGON_SOUL_STEP_LEVEL3 + " " + localeInfo.DRAGON_SOUL_STRENGTH(refine)
		elif 3 == step:
			return localeInfo.DRAGON_SOUL_STEP_LEVEL4 + " " + localeInfo.DRAGON_SOUL_STRENGTH(refine)
		elif 4 == step:
			return localeInfo.DRAGON_SOUL_STEP_LEVEL5 + " " + localeInfo.DRAGON_SOUL_STRENGTH(refine)
		else:
			return ""


	def __IsHair(self, itemVnum):
		return (self.__IsOldHair(itemVnum) or
			self.__IsNewHair(itemVnum) or
			self.__IsNewHair2(itemVnum) or
			self.__IsNewHair3(itemVnum)
		)

	def __IsOldHair(self, itemVnum):
		return itemVnum > 73000 and itemVnum < 74000

	def __IsNewHair(self, itemVnum):
		return itemVnum > 74000 and itemVnum < 75000

	def __IsNewHair2(self, itemVnum):
		return itemVnum > 75000 and itemVnum < 76000

	def __IsNewHair3(self, itemVnum):
		return ((74012 < itemVnum and itemVnum < 74022) or
			(74262 < itemVnum and itemVnum < 74272) or
			(74512 < itemVnum and itemVnum < 74599) or
			(74762 < itemVnum and itemVnum < 74799) or
			(45000 < itemVnum and itemVnum < 47000))

	def __AppendHairIcon(self, itemVnum):
		itemImage = ui.ImageBox()
		itemImage.SetParent(self)
		itemImage.Show()

		if self.__IsOldHair(itemVnum):
			itemImage.LoadImage(flamewindPath.GetHair(str(itemVnum)))
		elif self.__IsNewHair3(itemVnum):
			itemImage.LoadImage("icon/hair/%d.sub" % (itemVnum))
		elif self.__IsNewHair(itemVnum):
			itemImage.LoadImage(flamewindPath.GetHair(str(itemVnum-1000)))
		elif self.__IsNewHair2(itemVnum):
			itemImage.LoadImage("icon/hair/%d.sub" % (itemVnum))

		itemImage.SetWindowHorizontalAlignCenter()
		itemImage.SetPosition(0, self.toolTipHeight)
		self.toolTipHeight += itemImage.GetHeight()
		self.childrenList.append(itemImage)
		self.ResizeToolTip()

	def __AdjustMaxWidth(self, attrSlot, desc):
		newToolTipWidth = self.toolTipWidth
		newToolTipWidth = max(self.__AdjustAttrMaxWidth(attrSlot), newToolTipWidth)
		newToolTipWidth = max(self.__AdjustDescMaxWidth(desc), newToolTipWidth)
		if newToolTipWidth > self.toolTipWidth:
			self.toolTipWidth = newToolTipWidth
			self.ResizeToolTip()

	def __SetSkillBookToolTip(self, skillIndex, bookName, skillGrade):
		skillName = skill.GetSkillName(skillIndex)

		if not skillName:
			return

		if localeInfo.IsVIETNAM():
			itemName = bookName + " " + skillName
		else:
			itemName = skillName + " " + bookName
		self.SetTitle(itemName)

	def __AppendHerbKnifeInformation(self, curEXP, maxEXP):
		self.AppendSpace(5)
		self.AppendTextLine(localeInfo.TOOLTIP_PICK_EXP % (curEXP, maxEXP), self.NORMAL_COLOR)

		if curEXP == maxEXP:
			self.AppendSpace(5)
			self.AppendTextLine(localeInfo.TOOLTIP_HERB_KNIFE_UPGRADE1, self.NORMAL_COLOR)
			self.AppendTextLine(localeInfo.TOOLTIP_HERB_KNIFE_UPGRADE2, self.NORMAL_COLOR)
			self.AppendTextLine(localeInfo.TOOLTIP_HERB_KNIFE_UPGRADE3, self.NORMAL_COLOR)

	def __AppendPickInformation(self, curLevel, curEXP, maxEXP):
		self.AppendSpace(5)
		self.AppendTextLine(localeInfo.TOOLTIP_PICK_LEVEL % (curLevel), self.NORMAL_COLOR)
		self.AppendTextLine(localeInfo.TOOLTIP_PICK_EXP % (curEXP, maxEXP), self.NORMAL_COLOR)

		if curEXP == maxEXP:
			self.AppendSpace(5)
			self.AppendTextLine(localeInfo.TOOLTIP_PICK_UPGRADE1, self.NORMAL_COLOR)
			self.AppendTextLine(localeInfo.TOOLTIP_PICK_UPGRADE2, self.NORMAL_COLOR)
			self.AppendTextLine(localeInfo.TOOLTIP_PICK_UPGRADE3, self.NORMAL_COLOR)


	def __AppendRodInformation(self, curLevel, curEXP, maxEXP, bait):
		self.AppendSpace(5)
		if bait > 0:
			item.SelectItem(bait)
			self.AppendTextLine(localeInfo.ITEM_ROD_BAIT % item.GetItemName(), self.NORMAL_COLOR)

		self.AppendTextLine(localeInfo.TOOLTIP_FISHINGROD_LEVEL % (curLevel), self.NORMAL_COLOR)
		self.AppendTextLine(localeInfo.TOOLTIP_FISHINGROD_EXP % (curEXP, maxEXP), self.NORMAL_COLOR)

		poolBonus = item.GetValue(0) / 10
		chanceBonus = item.GetValue(5)
		if poolBonus > 0 or chanceBonus > 0:
			self.AppendSpace(5)
			self.AppendTextLine(localeInfo.FISHING_POOL_BONUS % poolBonus, self.POSITIVE_COLOR)
			self.AppendTextLine(localeInfo.FISHING_CHANCE_BONUS % chanceBonus, self.POSITIVE_COLOR)

		if curEXP == maxEXP:
			self.AppendHorizontalLine()
			self.AppendTextLine(localeInfo.TOOLTIP_FISHINGROD_UPGRADE1, self.NORMAL_COLOR)
			self.AppendTextLine(localeInfo.TOOLTIP_FISHINGROD_UPGRADE2, self.NORMAL_COLOR)
			self.AppendTextLine(localeInfo.TOOLTIP_FISHINGROD_UPGRADE3, self.NORMAL_COLOR)

	def __AppendLimitInformation(self):

		appendSpace = False

		for i in xrange(item.LIMIT_MAX_NUM):

			(limitType, limitValue) = item.GetLimit(i)

			if limitValue > 0:
				if False == appendSpace:
					self.AppendSpace(5)
					appendSpace = True

			else:
				continue

			if item.LIMIT_LEVEL == limitType:
				color = self.GetLimitTextLineColor(player.GetStatus(player.LEVEL), limitValue)
				self.AppendTextLine(localeInfo.TOOLTIP_ITEM_LIMIT_LEVEL % (limitValue), color)
			"""
			elif item.LIMIT_STR == limitType:
				color = self.GetLimitTextLineColor(player.GetStatus(player.ST), limitValue)
				self.AppendTextLine(localeInfo.TOOLTIP_ITEM_LIMIT_STR % (limitValue), color)
			elif item.LIMIT_DEX == limitType:
				color = self.GetLimitTextLineColor(player.GetStatus(player.DX), limitValue)
				self.AppendTextLine(localeInfo.TOOLTIP_ITEM_LIMIT_DEX % (limitValue), color)
			elif item.LIMIT_INT == limitType:
				color = self.GetLimitTextLineColor(player.GetStatus(player.IQ), limitValue)
				self.AppendTextLine(localeInfo.TOOLTIP_ITEM_LIMIT_INT % (limitValue), color)
			elif item.LIMIT_CON == limitType:
				color = self.GetLimitTextLineColor(player.GetStatus(player.HT), limitValue)
				self.AppendTextLine(localeInfo.TOOLTIP_ITEM_LIMIT_CON % (limitValue), color)
			"""

	## cyh itemseal 2013 11 11
	def __AppendSealInformation(self, window_type, slotIndex):
		if not app.ENABLE_SOULBIND_SYSTEM:
			return

		itemSealDate = player.GetItemSealDate(window_type, slotIndex)
		if itemSealDate == item.GetDefaultSealDate():
			return

		if itemSealDate == item.GetUnlimitedSealDate():
			self.AppendSpace(5)
			self.AppendTextLine(localeInfo.TOOLTIP_SEALED, self.NEGATIVE_COLOR)

		elif itemSealDate > 0:
			self.AppendSpace(5)
			hours, minutes = player.GetItemUnSealLeftTime(window_type, slotIndex)
			self.AppendTextLine(localeInfo.TOOLTIP_UNSEAL_LEFT_TIME % (hours, minutes), self.NEGATIVE_COLOR)

	def __AppendAffectInformation(self, attrList=None):
		affectList = [item.GetAffect(i) for i in xrange(item.ITEM_APPLY_MAX_NUM)]
		if attrList is not None and attrList:
			rareAffectList = [attrList[i] for i in xrange(player.ATTRIBUTE_SLOT_RARE_START, player.ATTRIBUTE_SLOT_RARE_END)]
			affectList += rareAffectList

		for i in affectList:
			affectType = i[0]
			affectValue = i[1]

		# for i in xrange(item.ITEM_APPLY_MAX_NUM):
			 # = item.GetAffect(i)
			if app.ENABLE_ACCE_COSTUME_SYSTEM and affectType==item.APPLY_ACCEDRAIN_RATE:
				continue
			if app.ENABLE_MOUNT_COSTUME_SYSTEM and affectType==item.APPLY_MOUNT:
				continue
			affectString = localeInfo.GetApplyString(affectType, affectValue)
			if affectString:
				if affectValue < 0:
					affectString = affectString.replace("+", "")
				self.AppendTextLine(affectString, self.GetChangeTextLineColor(affectValue))


	def __AppendAntiFlagInformation(self):
		blockGive = item.IsAntiFlag(item.ITEM_ANTIFLAG_GIVE)
		blockMyShop = item.IsAntiFlag(item.ITEM_ANTIFLAG_MYSHOP)

		if blockGive or blockMyShop:
			self.AppendSpace(5)

		if blockGive and blockMyShop:
			self.AppendTextLine(localeInfo.TOOLTIP_ANTIFLAG_NO_TRADE, self.NEGATIVE_COLOR)
		elif blockGive:
			self.AppendTextLine(localeInfo.TOOLTIP_ANTIFLAG_GIVE, self.NEGATIVE_COLOR)
		elif blockMyShop:
			self.AppendTextLine(localeInfo.TOOLTIP_ANTIFLAG_MYSHOP, self.NEGATIVE_COLOR)

	def AppendLastTimeInformation(self, metinSlot):
		bHasRealtimeFlag = False
		for i in xrange(item.LIMIT_MAX_NUM):
			(limitType, limitValue) = item.GetLimit(i)
			if item.LIMIT_REAL_TIME == limitType:
				bHasRealtimeFlag = True

		if bHasRealtimeFlag:
			self.AppendMallItemLastTime(metinSlot[0])
		return bHasRealtimeFlag

	def AppendWearableInformation(self):

		self.AppendSpace(5)
		self.AppendTextLine(localeInfo.TOOLTIP_ITEM_WEARABLE_JOB, self.NORMAL_COLOR)

		flagList = (
			not item.IsAntiFlag(item.ITEM_ANTIFLAG_WARRIOR),
			not item.IsAntiFlag(item.ITEM_ANTIFLAG_ASSASSIN),
			not item.IsAntiFlag(item.ITEM_ANTIFLAG_SURA),
			not item.IsAntiFlag(item.ITEM_ANTIFLAG_SHAMAN))
		if app.ENABLE_WOLFMAN_CHARACTER:
			flagList += (not item.IsAntiFlag(item.ITEM_ANTIFLAG_WOLFMAN),)
		characterNames = ""
		for i in xrange(self.CHARACTER_COUNT):

			name = self.CHARACTER_NAMES[i]
			flag = flagList[i]

			if flag:
				characterNames += " "
				characterNames += name

		textLine = self.AppendTextLine(characterNames, self.NORMAL_COLOR, True)
		textLine.SetFeather()

		if item.IsAntiFlag(item.ITEM_ANTIFLAG_MALE):
			textLine = self.AppendTextLine(localeInfo.FOR_FEMALE, self.NORMAL_COLOR, True)
			textLine.SetFeather()

		if item.IsAntiFlag(item.ITEM_ANTIFLAG_FEMALE):
			textLine = self.AppendTextLine(localeInfo.FOR_MALE, self.NORMAL_COLOR, True)
			textLine.SetFeather()

	def __AppendPotionInformation(self):
		self.AppendSpace(5)

		healHP = item.GetValue(0)
		healSP = item.GetValue(1)
		healStatus = item.GetValue(2)
		healPercentageHP = item.GetValue(3)
		healPercentageSP = item.GetValue(4)

		if healHP > 0:
			self.AppendTextLine(localeInfo.TOOLTIP_POTION_PLUS_HP_POINT % healHP, self.GetChangeTextLineColor(healHP))
		if healSP > 0:
			self.AppendTextLine(localeInfo.TOOLTIP_POTION_PLUS_SP_POINT % healSP, self.GetChangeTextLineColor(healSP))
		if healStatus != 0:
			self.AppendTextLine(localeInfo.TOOLTIP_POTION_CURE)
		if healPercentageHP > 0:
			self.AppendTextLine(localeInfo.TOOLTIP_POTION_PLUS_HP_PERCENT % healPercentageHP, self.GetChangeTextLineColor(healPercentageHP))
		if healPercentageSP > 0:
			self.AppendTextLine(localeInfo.TOOLTIP_POTION_PLUS_SP_PERCENT % healPercentageSP, self.GetChangeTextLineColor(healPercentageSP))

	def __AppendAbilityPotionInformation(self):

		self.AppendSpace(5)

		abilityType = item.GetValue(0)
		time = item.GetValue(1)
		point = item.GetValue(2)

		if abilityType == getattr(item, "APPLY_ATT_SPEED", 17):
			self.AppendTextLine(localeInfo.TOOLTIP_POTION_PLUS_ATTACK_SPEED % point, self.GetChangeTextLineColor(point))
		elif abilityType == getattr(item, "APPLY_MOV_SPEED", 19):
			self.AppendTextLine(localeInfo.TOOLTIP_POTION_PLUS_MOVING_SPEED % point, self.GetChangeTextLineColor(point))

		if time > 0:
			minute = (time / 60)
			second = (time % 60)
			timeString = localeInfo.TOOLTIP_POTION_TIME

			if minute > 0:
				timeString += str(minute) + localeInfo.TOOLTIP_POTION_MIN
			if second > 0:
				timeString += " " + str(second) + localeInfo.TOOLTIP_POTION_SEC

			self.AppendTextLine(timeString)

	def GetPriceColor(self, price):
		if price>=constInfo.HIGH_PRICE:
			return self.HIGH_PRICE_COLOR
		if price>=constInfo.MIDDLE_PRICE:
			return self.MIDDLE_PRICE_COLOR
		else:
			return self.LOW_PRICE_COLOR

	def AppendPrice(self, price, count=1):
		if app.ENABLE_CHEQUE_SYSTEM:
			self.AppendTextLine(localeInfo.CHEQUE_SYSTEM_YANG  % (localeInfo.NumberToMoneyString(price)), self.GetPriceColor(price))
			self.AppendSpace(5)
		else:
			self.AppendSpace(5)
			self.AppendHorizontalLine()
			self.AppendTextLine(localeInfo.TOOLTIP_BUYPRICE % (localeInfo.NumberToMoneyString(price)), self.GetPriceColor(price))
			if count > 1:
				pricePerItem = price / count
				self.AppendTextLine(localeInfo.TOOLTIP_BUYPRICE_STACK % (localeInfo.NumberToMoneyString(pricePerItem)),
									self.GetPriceColor(pricePerItem))

	def AppendPriceBySecondaryCoin(self, price):
		self.AppendSpace(5)
		self.AppendTextLine(localeInfo.TOOLTIP_BUYPRICE % (localeInfo.NumberToSecondaryCoinString(price)), self.GetPriceColor(price))

	def AppendSellingPrice(self, price):
		if item.IsAntiFlag(item.ITEM_ANTIFLAG_SELL):
			self.AppendTextLine(localeInfo.TOOLTIP_ANTI_SELL, self.DISABLE_COLOR)
			self.AppendSpace(5)
		else:
			if app.ENABLE_CHEQUE_SYSTEM:
				self.AppendTextLine(localeInfo.CHEQUE_SYSTEM_YANG % (localeInfo.NumberToGold(price)), self.GetPriceColor(price))
				self.AppendSpace(5)
			else:
				self.AppendTextLine(localeInfo.TOOLTIP_SELLPRICE % (localeInfo.NumberToMoneyString(price)), self.GetPriceColor(price))
				self.AppendSpace(5)

	if app.ENABLE_CHEQUE_SYSTEM:
		def AppendCheque(self, price):
			if price <=0:
				return
			self.AppendTextLine(localeInfo.CHEQUE_SYSTEM_WON % (localeInfo.NumberToCheque(price)), grp.GenerateColor(0.0, 0.8470, 1.0, 1.0))

		def AppendSellingCheque(self, price):
			if price<=0:
				return
			self.AppendTextLine(localeInfo.CHEQUE_SYSTEM_WON % (localeInfo.NumberToCheque(price)), grp.GenerateColor(0.0, 0.8470, 1.0, 1.0))

		def AppendSellingCheque2(self, price):
			if price<=0:
				return
			self.AppendTextLine(localeInfo.CHEQUE_SYSTEM_SELL_PRICE, grp.GenerateColor(1.0, 0.9686, 0.3098, 1.0))
			self.AppendTextLine(localeInfo.CHEQUE_SYSTEM_WON % (localeInfo.NumberToCheque(price)), grp.GenerateColor(0.0, 0.8470, 1.0, 1.0))

	def AppendMetinInformation(self):
		if constInfo.ENABLE_FULLSTONE_DETAILS:
			for i in xrange(item.ITEM_APPLY_MAX_NUM):
				(affectType, affectValue) = item.GetAffect(i)
				affectString = localeInfo.GetApplyString(affectType, affectValue)
				if affectString:
					self.AppendSpace(5)
					self.AppendTextLine(affectString, self.GetChangeTextLineColor(affectValue))

	def AppendMetinWearInformation(self):

		self.AppendSpace(5)
		self.AppendTextLine(localeInfo.TOOLTIP_SOCKET_REFINABLE_ITEM, self.NORMAL_COLOR)

		flagList = (item.IsWearableFlag(item.WEARABLE_BODY),
					item.IsWearableFlag(item.WEARABLE_HEAD),
					item.IsWearableFlag(item.WEARABLE_FOOTS),
					item.IsWearableFlag(item.WEARABLE_WRIST),
					item.IsWearableFlag(item.WEARABLE_WEAPON),
					item.IsWearableFlag(item.WEARABLE_NECK),
					item.IsWearableFlag(item.WEARABLE_EAR),
					item.IsWearableFlag(item.WEARABLE_UNIQUE),
					item.IsWearableFlag(item.WEARABLE_SHIELD),
					item.IsWearableFlag(item.WEARABLE_ARROW))
		if app.ENABLE_PENDANT_SYSTEM:
			flagList += (item.IsWearableFlag(item.WEARABLE_PENDANT),)
		if app.ENABLE_GLOVE_SYSTEM:
			flagList += (item.IsWearableFlag(item.WEARABLE_GLOVE),)

		wearNames = ""
		for i in xrange(self.WEAR_COUNT):

			name = self.WEAR_NAMES[i]
			flag = flagList[i]

			if flag:
				wearNames += " "*2
				wearNames += name

		textLine = ui.TextLine()
		textLine.SetParent(self)
		textLine.SetFontName(self.defFontName)
		textLine.SetPosition(self.toolTipWidth/2, self.toolTipHeight)
		textLine.SetHorizontalAlignCenter()
		textLine.SetPackedFontColor(self.NORMAL_COLOR)
		textLine.SetText(wearNames)
		textLine.Show()
		self.childrenList.append(textLine)

		self.toolTipHeight += self.TEXT_LINE_HEIGHT
		self.ResizeToolTip()

	def GetMetinSocketType(self, number):
		if player.METIN_SOCKET_TYPE_NONE == number:
			return player.METIN_SOCKET_TYPE_NONE
		elif player.METIN_SOCKET_TYPE_SILVER == number:
			return player.METIN_SOCKET_TYPE_SILVER
		elif player.METIN_SOCKET_TYPE_GOLD == number:
			return player.METIN_SOCKET_TYPE_GOLD
		else:
			item.SelectItem(number)
			if item.METIN_NORMAL == item.GetItemSubType():
				return player.METIN_SOCKET_TYPE_SILVER
			elif item.METIN_GOLD == item.GetItemSubType():
				return player.METIN_SOCKET_TYPE_GOLD
			elif "USE_PUT_INTO_ACCESSORY_SOCKET" == item.GetUseType(number):
				return player.METIN_SOCKET_TYPE_SILVER
			elif "USE_PUT_INTO_RING_SOCKET" == item.GetUseType(number):
				return player.METIN_SOCKET_TYPE_SILVER
			elif "USE_PUT_INTO_BELT_SOCKET" == item.GetUseType(number):
				return player.METIN_SOCKET_TYPE_SILVER

		return player.METIN_SOCKET_TYPE_NONE

	def GetMetinItemIndex(self, number):
		if player.METIN_SOCKET_TYPE_SILVER == number:
			return 0
		if player.METIN_SOCKET_TYPE_GOLD == number:
			return 0

		return number

	def __AppendAccessoryMetinSlotInfo(self, metinSlot, mtrlVnum):
		ACCESSORY_SOCKET_MAX_SIZE = 3

		cur=min(metinSlot[0], ACCESSORY_SOCKET_MAX_SIZE)
		end=min(metinSlot[1], ACCESSORY_SOCKET_MAX_SIZE)

		affectType1, affectValue1 = item.GetAffect(0)
		affectList1=[0, max(1, affectValue1*10/100), max(2, affectValue1*20/100), max(3, affectValue1*40/100)]

		affectType2, affectValue2 = item.GetAffect(1)
		affectList2=[0, max(1, affectValue2*10/100), max(2, affectValue2*20/100), max(3, affectValue2*40/100)]

		affectType3, affectValue3 = item.GetAffect(2)
		affectList3=[0, max(1, affectValue3*10/100), max(2, affectValue3*20/100), max(3, affectValue3*40/100)]

		mtrlPos=0
		mtrlList=[mtrlVnum]*cur+[player.METIN_SOCKET_TYPE_SILVER]*(end-cur)
		for mtrl in mtrlList:
			affectString1 = localeInfo.GetApplyString(affectType1, affectList1[mtrlPos + 1] - affectList1[mtrlPos])
			affectString2 = localeInfo.GetApplyString(affectType2, affectList2[mtrlPos + 1] - affectList2[mtrlPos])
			affectString3 = localeInfo.GetApplyString(affectType3, affectList3[mtrlPos + 1] - affectList3[mtrlPos])

			leftTime = 0
			if cur == mtrlPos+1:
				leftTime=metinSlot[2]

			self.__AppendMetinSlotInfo_AppendMetinSocketData(mtrlPos, mtrl, affectString1, affectString2, affectString3, leftTime)
			mtrlPos+=1

	def __AppendMetinSlotInfo(self, metinSlot):
		if self.__AppendMetinSlotInfo_IsEmptySlotList(metinSlot):
			return

		for i in xrange(player.METIN_SOCKET_MAX_NUM):
			self.__AppendMetinSlotInfo_AppendMetinSocketData(i, metinSlot[i])

	def __AppendMetinSlotInfo_IsEmptySlotList(self, metinSlot):
		if 0 == metinSlot:
			return 1

		for i in xrange(player.METIN_SOCKET_MAX_NUM):
			metinSlotData=metinSlot[i]
			if 0 != self.GetMetinSocketType(metinSlotData):
				if 0 != self.GetMetinItemIndex(metinSlotData):
					return 0

		return 1

	def __AppendMetinSlotInfo_AppendMetinSocketData(self, index, metinSlotData, custumAffectString="", custumAffectString2="", custumAffectString3="", leftTime=0):

		slotType = self.GetMetinSocketType(metinSlotData)
		itemIndex = self.GetMetinItemIndex(metinSlotData)

		if 0 == slotType:
			return

		self.AppendSpace(5)

		slotImage = ui.ImageBox()
		slotImage.SetParent(self)
		slotImage.Show()

		## Name
		nameTextLine = ui.TextLine()
		nameTextLine.SetParent(self)
		nameTextLine.SetFontName(self.defFontName)
		nameTextLine.SetPackedFontColor(self.NORMAL_COLOR)
		nameTextLine.SetOutline()
		nameTextLine.SetFeather()
		nameTextLine.Show()

		self.childrenList.append(nameTextLine)

		if player.METIN_SOCKET_TYPE_SILVER == slotType:
			slotImage.LoadImage("d:/ymir work/ui/game/windows/metin_slot_silver.sub")
		elif player.METIN_SOCKET_TYPE_GOLD == slotType:
			slotImage.LoadImage("d:/ymir work/ui/game/windows/metin_slot_gold.sub")

		self.childrenList.append(slotImage)

		if localeInfo.IsARABIC():
			slotImage.SetPosition(self.toolTipWidth - slotImage.GetWidth() - 9, self.toolTipHeight-1)
			nameTextLine.SetPosition(self.toolTipWidth - 50, self.toolTipHeight + 2)
		else:
			slotImage.SetPosition(9, self.toolTipHeight-1)
			nameTextLine.SetPosition(50, self.toolTipHeight + 2)

		metinImage = ui.ImageBox()
		metinImage.SetParent(self)
		metinImage.Show()
		self.childrenList.append(metinImage)

		if itemIndex:

			item.SelectItem(itemIndex)

			## Image
			try:
				metinImage.LoadImage(item.GetIconImageFileName())
			except:
				dbg.TraceError("ItemToolTip.__AppendMetinSocketData() - Failed to find image file %d:%s" %
					(itemIndex, item.GetIconImageFileName())
				)

			nameTextLine.SetText(item.GetItemName())

			## Affect
			affectTextLine = ui.TextLine()
			affectTextLine.SetParent(self)
			affectTextLine.SetFontName(self.defFontName)
			affectTextLine.SetPackedFontColor(self.POSITIVE_COLOR)
			affectTextLine.SetOutline()
			affectTextLine.SetFeather()
			affectTextLine.Show()

			if localeInfo.IsARABIC():
				metinImage.SetPosition(self.toolTipWidth - metinImage.GetWidth() - 10, self.toolTipHeight)
				affectTextLine.SetPosition(self.toolTipWidth - 50, self.toolTipHeight + 16 + 2)
			else:
				metinImage.SetPosition(10, self.toolTipHeight)
				affectTextLine.SetPosition(50, self.toolTipHeight + 16 + 2)

			if custumAffectString:
				affectTextLine.SetText(custumAffectString)
			elif itemIndex!=constInfo.ERROR_METIN_STONE:
				affectType, affectValue = item.GetAffect(0)
				affectString = localeInfo.GetApplyString(affectType, affectValue)
				if affectString:
					affectTextLine.SetText(affectString)
			else:
				affectTextLine.SetText(localeInfo.TOOLTIP_APPLY_NOAFFECT)

			self.childrenList.append(affectTextLine)

			if constInfo.ENABLE_FULLSTONE_DETAILS and (not custumAffectString2) and (itemIndex!=constInfo.ERROR_METIN_STONE):
				custumAffectString2 = localeInfo.GetApplyString(*item.GetAffect(1))

			if custumAffectString2:
				affectTextLine = ui.TextLine()
				affectTextLine.SetParent(self)
				affectTextLine.SetFontName(self.defFontName)
				affectTextLine.SetPackedFontColor(self.POSITIVE_COLOR)
				affectTextLine.SetPosition(50, self.toolTipHeight + 16 + 2 + 16 + 2)
				affectTextLine.SetOutline()
				affectTextLine.SetFeather()
				affectTextLine.Show()
				affectTextLine.SetText(custumAffectString2)
				self.childrenList.append(affectTextLine)
				self.toolTipHeight += 16 + 2

			if constInfo.ENABLE_FULLSTONE_DETAILS and (not custumAffectString3) and (itemIndex!=constInfo.ERROR_METIN_STONE):
				custumAffectString3 = localeInfo.GetApplyString(*item.GetAffect(2))

			if custumAffectString3:
				affectTextLine = ui.TextLine()
				affectTextLine.SetParent(self)
				affectTextLine.SetFontName(self.defFontName)
				affectTextLine.SetPackedFontColor(self.POSITIVE_COLOR)
				affectTextLine.SetPosition(50, self.toolTipHeight + 16 + 2 + 16 + 2)
				affectTextLine.SetOutline()
				affectTextLine.SetFeather()
				affectTextLine.Show()
				affectTextLine.SetText(custumAffectString3)
				self.childrenList.append(affectTextLine)
				self.toolTipHeight += 16 + 2

			if 0 != leftTime:
				timeText = (localeInfo.LEFT_TIME + " : " + localeInfo.SecondToDHM(leftTime))

				timeTextLine = ui.TextLine()
				timeTextLine.SetParent(self)
				timeTextLine.SetFontName(self.defFontName)
				timeTextLine.SetPackedFontColor(self.POSITIVE_COLOR)
				timeTextLine.SetPosition(50, self.toolTipHeight + 16 + 2 + 16 + 2)
				timeTextLine.SetOutline()
				timeTextLine.SetFeather()
				timeTextLine.Show()
				timeTextLine.SetText(timeText)
				self.childrenList.append(timeTextLine)
				self.toolTipHeight += 16 + 2

		else:
			nameTextLine.SetText(localeInfo.TOOLTIP_SOCKET_EMPTY)

		self.toolTipHeight += 35
		self.ResizeToolTip()

	def __AppendFishInfo(self, size):
		if size > 0:
			self.AppendSpace(5)
			self.AppendTextLine(localeInfo.TOOLTIP_FISH_LEN % (float(size) / 100.0), self.NORMAL_COLOR)

	def AppendUniqueItemLastTime(self, restMin):
		if restMin > 0: # @fixme012
			restSecond = restMin*60
			self.AppendSpace(5)
			self.AppendTextLine(localeInfo.LEFT_USE_TIME + " : " + localeInfo.SecondToDHM(restSecond), self.NORMAL_COLOR)

	def AppendMallItemLastTime(self, endTime):
		if endTime > 0: # @fixme012
			if endTime >= app.GetGlobalTimeStamp():
				leftSec = endTime - app.GetGlobalTimeStamp()
				self.AppendSpace(5)
				self.AppendTextLine(localeInfo.LEFT_TIME + " : " + localeInfo.SecondToDHM(leftSec), self.NORMAL_COLOR)
			else:
				self.AppendSpace(5)
				self.AppendTextLine(localeInfo.LEFT_TIME + " : " + localeInfo.SecondToDHM(endTime), self.NORMAL_COLOR)

	def AppendTimerBasedOnWearLastTime(self, metinSlot):
		if 0 == metinSlot[0]:
			self.AppendSpace(5)
			self.AppendTextLine(localeInfo.CANNOT_USE, self.DISABLE_COLOR)
		else:
			endTime = app.GetGlobalTimeStamp() + metinSlot[0]
			self.AppendMallItemLastTime(endTime)

	def AppendRealTimeStartFirstUseLastTime(self, item, metinSlot, limitIndex):
		useCount = metinSlot[1]
		endTime = metinSlot[0]

		if 0 == useCount:
			if 0 == endTime:
				(limitType, limitValue) = item.GetLimit(limitIndex)
				endTime = limitValue

			endTime += app.GetGlobalTimeStamp()

		self.AppendMallItemLastTime(endTime)

	if app.ENABLE_ACCE_COSTUME_SYSTEM:
		def SetAcceResultItem(self, slotIndex, window_type = player.INVENTORY):
			(itemVnum, MinAbs, MaxAbs) = acce.GetResultItem()
			if not itemVnum:
				return

			self.ClearToolTip()

			metinSlot	= [player.GetItemMetinSocket(window_type, slotIndex, i)		for i in xrange(player.METIN_SOCKET_MAX_NUM)	]
			attrSlot	= [player.GetItemAttribute(window_type, slotIndex, i)		for i in xrange(player.ATTRIBUTE_SLOT_MAX_NUM)	]

			item.SelectItem(itemVnum)
			itemType = item.GetItemType()
			itemSubType = item.GetItemSubType()
			if itemType != item.ITEM_TYPE_COSTUME and itemSubType != item.COSTUME_TYPE_ACCE:
				return

			absChance = MaxAbs
			itemDesc = item.GetItemDescription()
			self.__AdjustMaxWidth(attrSlot, itemDesc)
			self.__SetItemTitle(itemVnum, metinSlot, attrSlot)
			self.AppendDescription(itemDesc, 26)
			self.AppendDescription(item.GetItemSummary(), 26, self.CONDITION_COLOR)
			self.__AppendLimitInformation()

			## ABSORPTION RATE
			if MinAbs == MaxAbs:
				self.AppendTextLine(localeInfo.ACCE_ABSORB_CHANCE % (MinAbs), self.CONDITION_COLOR)
			else:
				self.AppendTextLine(localeInfo.ACCE_ABSORB_CHANCE2 % (MinAbs, MaxAbs), self.CONDITION_COLOR)
			## END ABSOPRTION RATE

			itemAbsorbedVnum = int(metinSlot[acce.ABSORBED_SOCKET])
			if itemAbsorbedVnum:
				## ATTACK / DEFENCE
				item.SelectItem(itemAbsorbedVnum)
				if item.GetItemType() == item.ITEM_TYPE_WEAPON:
					if item.GetItemSubType() == item.WEAPON_FAN:
						self.__AppendMagicAttackInfo(absChance)
						item.SelectItem(itemAbsorbedVnum)
						self.__AppendAttackPowerInfo(absChance)
					else:
						self.__AppendAttackPowerInfo(absChance)
						item.SelectItem(itemAbsorbedVnum)
						self.__AppendMagicAttackInfo(absChance)
				elif item.GetItemType() == item.ITEM_TYPE_ARMOR:
					defGrade = item.GetValue(1)
					defBonus = item.GetValue(5) * 2
					defGrade = self.CalcAcceValue(defGrade, absChance)
					defBonus = self.CalcAcceValue(defBonus, absChance)

					if defGrade > 0:
						self.AppendSpace(5)
						self.AppendTextLine(localeInfo.TOOLTIP_ITEM_DEF_GRADE % (defGrade + defBonus), self.GetChangeTextLineColor(defGrade))

					item.SelectItem(itemAbsorbedVnum)
					self.__AppendMagicDefenceInfo(absChance)
				## END ATTACK / DEFENCE

				## EFFECT
				item.SelectItem(itemAbsorbedVnum)
				for i in xrange(item.ITEM_APPLY_MAX_NUM):
					(affectType, affectValue) = item.GetAffect(i)
					affectValue = self.CalcAcceValue(affectValue, absChance)
					affectString = localeInfo.GetApplyString(affectType, affectValue)
					if (not app.USE_ACCE_ABSORB_WITH_NO_NEGATIVE_BONUS and affectString and affectValue != 0)\
							or (app.USE_ACCE_ABSORB_WITH_NO_NEGATIVE_BONUS and affectString and affectValue > 0):
						self.AppendTextLine(affectString, self.GetChangeTextLineColor(affectValue))

					item.SelectItem(itemAbsorbedVnum)
				# END EFFECT

			item.SelectItem(itemVnum)
			## ATTR
			self.__AppendAttributeInformation(attrSlot, MaxAbs)
			# END ATTR

			self.AppendWearableInformation()
			self.ShowToolTip()

		def SetAcceResultAbsItem(self, slotIndex1, slotIndex2, window_type = player.INVENTORY):
			itemVnumAcce = player.GetItemIndex(window_type, slotIndex1)
			itemVnumTarget = player.GetItemIndex(window_type, slotIndex2)
			if not itemVnumAcce or not itemVnumTarget:
				return

			self.ClearToolTip()

			item.SelectItem(itemVnumAcce)
			itemType = item.GetItemType()
			itemSubType = item.GetItemSubType()
			if itemType != item.ITEM_TYPE_COSTUME and itemSubType != item.COSTUME_TYPE_ACCE:
				return

			metinSlot = [player.GetItemMetinSocket(window_type, slotIndex1, i) for i in xrange(player.METIN_SOCKET_MAX_NUM)]
			attrSlot = [player.GetItemAttribute(window_type, slotIndex2, i) for i in xrange(player.ATTRIBUTE_SLOT_MAX_NUM)]

			itemDesc = item.GetItemDescription()
			self.__SetItemTitle(itemVnumAcce, metinSlot, attrSlot)
			self.AppendDescription(itemDesc, 26)
			self.AppendDescription(item.GetItemSummary(), 26, self.CONDITION_COLOR)
			item.SelectItem(itemVnumAcce)
			self.__AppendLimitInformation()

			## ABSORPTION RATE
			self.AppendTextLine(localeInfo.ACCE_ABSORB_CHANCE % (metinSlot[acce.ABSORPTION_SOCKET]), self.CONDITION_COLOR)
			## END ABSOPRTION RATE

			## ATTACK / DEFENCE
			itemAbsorbedVnum = itemVnumTarget
			item.SelectItem(itemAbsorbedVnum)
			if item.GetItemType() == item.ITEM_TYPE_WEAPON:
				if item.GetItemSubType() == item.WEAPON_FAN:
					self.__AppendMagicAttackInfo(metinSlot[acce.ABSORPTION_SOCKET])
					item.SelectItem(itemAbsorbedVnum)
					self.__AppendAttackPowerInfo(metinSlot[acce.ABSORPTION_SOCKET])
				else:
					self.__AppendAttackPowerInfo(metinSlot[acce.ABSORPTION_SOCKET])
					item.SelectItem(itemAbsorbedVnum)
					self.__AppendMagicAttackInfo(metinSlot[acce.ABSORPTION_SOCKET])
			elif item.GetItemType() == item.ITEM_TYPE_ARMOR:
				defGrade = item.GetValue(1)
				defBonus = item.GetValue(5) * 2
				defGrade = self.CalcAcceValue(defGrade, metinSlot[acce.ABSORPTION_SOCKET])
				defBonus = self.CalcAcceValue(defBonus, metinSlot[acce.ABSORPTION_SOCKET])

				if defGrade > 0:
					self.AppendSpace(5)
					self.AppendTextLine(localeInfo.TOOLTIP_ITEM_DEF_GRADE % (defGrade + defBonus), self.GetChangeTextLineColor(defGrade))

				item.SelectItem(itemAbsorbedVnum)
				self.__AppendMagicDefenceInfo(metinSlot[acce.ABSORPTION_SOCKET])
			## END ATTACK / DEFENCE

			## EFFECT
			item.SelectItem(itemAbsorbedVnum)
			for i in xrange(item.ITEM_APPLY_MAX_NUM):
				(affectType, affectValue) = item.GetAffect(i)
				affectValue = self.CalcAcceValue(affectValue, metinSlot[acce.ABSORPTION_SOCKET])
				affectString = localeInfo.GetApplyString(affectType, affectValue)
				if (not app.USE_ACCE_ABSORB_WITH_NO_NEGATIVE_BONUS and affectString and affectValue != 0)\
						or (app.USE_ACCE_ABSORB_WITH_NO_NEGATIVE_BONUS and affectString and affectValue > 0):
					self.AppendTextLine(affectString, self.GetChangeTextLineColor(affectValue))

				item.SelectItem(itemAbsorbedVnum)
			## END EFFECT

			## ATTR
			item.SelectItem(itemAbsorbedVnum)
			for i in xrange(player.ATTRIBUTE_SLOT_MAX_NUM):
				type = attrSlot[i][0]
				value = attrSlot[i][1]
				if not value:
					continue

				affectValue = self.CalcAcceValue(value, metinSlot[acce.ABSORPTION_SOCKET])
				affectString = localeInfo.GetApplyString(type, affectValue)
				if (not app.USE_ACCE_ABSORB_WITH_NO_NEGATIVE_BONUS and affectString and affectValue != 0)\
						or (app.USE_ACCE_ABSORB_WITH_NO_NEGATIVE_BONUS and affectString and affectValue > 0):
					affectColor = self.__GetAttributeColor(i, affectValue)
					self.AppendTextLine(affectString, affectColor)

				item.SelectItem(itemAbsorbedVnum)
			## END ATTR

			## WEARABLE
			item.SelectItem(itemVnumAcce)
			self.AppendSpace(5)
			self.AppendTextLine(localeInfo.TOOLTIP_ITEM_WEARABLE_JOB, self.NORMAL_COLOR)

			item.SelectItem(itemVnumAcce)
			flagList = (
						not item.IsAntiFlag(item.ITEM_ANTIFLAG_WARRIOR),
						not item.IsAntiFlag(item.ITEM_ANTIFLAG_ASSASSIN),
						not item.IsAntiFlag(item.ITEM_ANTIFLAG_SURA),
						not item.IsAntiFlag(item.ITEM_ANTIFLAG_SHAMAN)
			)

			if app.ENABLE_WOLFMAN_CHARACTER:
				flagList += (not item.IsAntiFlag(item.ITEM_ANTIFLAG_WOLFMAN),)

			characterNames = ""
			for i in xrange(self.CHARACTER_COUNT):
				name = self.CHARACTER_NAMES[i]
				flag = flagList[i]
				if flag:
					characterNames += " "
					characterNames += name

			textLine = self.AppendTextLine(characterNames, self.NORMAL_COLOR, True)
			textLine.SetFeather()

			item.SelectItem(itemVnumAcce)
			if item.IsAntiFlag(item.ITEM_ANTIFLAG_MALE):
				textLine = self.AppendTextLine(localeInfo.FOR_FEMALE, self.NORMAL_COLOR, True)
				textLine.SetFeather()

			if item.IsAntiFlag(item.ITEM_ANTIFLAG_FEMALE):
				textLine = self.AppendTextLine(localeInfo.FOR_MALE, self.NORMAL_COLOR, True)
				textLine.SetFeather()
			## END WEARABLE

			self.ShowToolTip()

class HyperlinkItemToolTip(ItemToolTip):
	def __init__(self):
		ItemToolTip.__init__(self, isPickable=True)

	def IsUnique70LevelWeapon(self, vnum):
		item.SelectItem(vnum)
		(limitType, limitValue) = item.GetLimit(0)
		return limitValue == 70

	def SetHyperlinkItem(self, tokens):
		minTokenCount = 3 + player.METIN_SOCKET_MAX_NUM
		maxTokenCount = minTokenCount + 2 * player.ATTRIBUTE_SLOT_MAX_NUM
		if tokens and len(tokens) >= minTokenCount and len(tokens) <= maxTokenCount:
			head, vnum, flag = tokens[:3]
			itemVnum = int(vnum, 16)
			metinSlot = [int(metin, 16) for metin in tokens[3:6]]

			rests = tokens[6:]
			if rests:
				item.SelectItem(itemVnum)
				attrSlot = [(0, 0) for i in range(player.ATTRIBUTE_SLOT_MAX_NUM)]

				rests.reverse()
				idx = 0
				attrCount = len(rests) / 2
				while rests:
					key = int(rests.pop(), 16)
					if rests:
						val = int(rests.pop())

						if idx == attrCount - 1 and self.IsUnique70LevelWeapon(itemVnum):
							idx = 5

						attrSlot[idx] = (key, val)

					idx += 1


			else:
				attrSlot = [(0, 0)] * player.ATTRIBUTE_SLOT_MAX_NUM

			self.ClearToolTip()
			self.AddItemData(itemVnum, metinSlot, attrSlot)

			ItemToolTip.OnUpdate(self)

	def OnUpdate(self):
		pass

	def OnMouseLeftButtonDown(self):
		self.Hide()

class SkillToolTip(ToolTip):

	POINT_NAME_DICT = {
		player.LEVEL : localeInfo.SKILL_TOOLTIP_LEVEL,
		player.IQ : localeInfo.SKILL_TOOLTIP_INT,
	}

	SKILL_TOOL_TIP_WIDTH = 200
	PARTY_SKILL_TOOL_TIP_WIDTH = 340

	PARTY_SKILL_EXPERIENCE_AFFECT_LIST = (	( 2, 2, 10,),
											( 8, 3, 20,),
											(14, 4, 30,),
											(22, 5, 45,),
											(28, 6, 60,),
											(34, 7, 80,),
											(38, 8, 100,), )

	PARTY_SKILL_PLUS_GRADE_AFFECT_LIST = (	( 4, 2, 1, 0,),
											(10, 3, 2, 0,),
											(16, 4, 2, 1,),
											(24, 5, 2, 2,), )

	PARTY_SKILL_ATTACKER_AFFECT_LIST = (	( 36, 3, ),
											( 26, 1, ),
											( 32, 2, ), )

	SKILL_GRADE_NAME = {	player.SKILL_GRADE_MASTER : localeInfo.SKILL_GRADE_NAME_MASTER,
							player.SKILL_GRADE_GRAND_MASTER : localeInfo.SKILL_GRADE_NAME_GRAND_MASTER,
							player.SKILL_GRADE_PERFECT_MASTER : localeInfo.SKILL_GRADE_NAME_PERFECT_MASTER, }

	AFFECT_NAME_DICT =	{
							"HP" : localeInfo.TOOLTIP_SKILL_AFFECT_ATT_POWER,
							"ATT_GRADE" : localeInfo.TOOLTIP_SKILL_AFFECT_ATT_GRADE,
							"DEF_GRADE" : localeInfo.TOOLTIP_SKILL_AFFECT_DEF_GRADE,
							"ATT_SPEED" : localeInfo.TOOLTIP_SKILL_AFFECT_ATT_SPEED,
							"MOV_SPEED" : localeInfo.TOOLTIP_SKILL_AFFECT_MOV_SPEED,
							"DODGE" : localeInfo.TOOLTIP_SKILL_AFFECT_DODGE,
							"RESIST_NORMAL" : localeInfo.TOOLTIP_SKILL_AFFECT_RESIST_NORMAL,
							"REFLECT_MELEE" : localeInfo.TOOLTIP_SKILL_AFFECT_REFLECT_MELEE,
						}
	AFFECT_APPEND_TEXT_DICT =	{
									"DODGE" : "%",
									"RESIST_NORMAL" : "%",
									"REFLECT_MELEE" : "%",
								}

	def __init__(self):
		ToolTip.__init__(self, self.SKILL_TOOL_TIP_WIDTH)
	def __del__(self):
		ToolTip.__del__(self)

	def SetSkill(self, skillIndex, skillLevel = -1):

		if 0 == skillIndex:
			return

		if skill.SKILL_TYPE_GUILD == skill.GetSkillType(skillIndex):

			if self.SKILL_TOOL_TIP_WIDTH != self.toolTipWidth:
				self.toolTipWidth = self.SKILL_TOOL_TIP_WIDTH
				self.ResizeToolTip()

			self.AppendDefaultData(skillIndex)
			self.AppendSkillConditionData(skillIndex)
			self.AppendGuildSkillData(skillIndex, skillLevel)

		else:

			if self.SKILL_TOOL_TIP_WIDTH != self.toolTipWidth:
				self.toolTipWidth = self.SKILL_TOOL_TIP_WIDTH
				self.ResizeToolTip()

			slotIndex = player.GetSkillSlotIndex(skillIndex)
			skillGrade = player.GetSkillGrade(slotIndex)
			skillLevel = player.GetSkillLevel(slotIndex)
			skillCurrentPercentage = player.GetSkillCurrentEfficientPercentage(slotIndex)
			skillNextPercentage = player.GetSkillNextEfficientPercentage(slotIndex)

			if skillIndex == skill.SKILL_SPRINT:
				skillLevel = 1

			self.AppendDefaultData(skillIndex)
			self.AppendSkillConditionData(skillIndex)
			self.AppendSkillDataNew(slotIndex, skillIndex, skillGrade, skillLevel, skillCurrentPercentage, skillNextPercentage)
			self.AppendSkillRequirement(skillIndex, skillLevel)

		self.ShowToolTip()

	def SetSkillNew(self, slotIndex, skillIndex, skillGrade, skillLevel):

		if 0 == skillIndex:
			return

		if player.SKILL_INDEX_TONGSOL == skillIndex:

			slotIndex = player.GetSkillSlotIndex(skillIndex)
			skillLevel = player.GetSkillLevel(slotIndex)

			self.AppendDefaultData(skillIndex)
			self.AppendPartySkillData(skillGrade, skillLevel)

		elif player.SKILL_INDEX_MINING == skillIndex:
			slotIndex = player.GetSkillSlotIndex(skillIndex)
			skillLevel = player.GetSkillLevel(slotIndex)

			self.AppendDefaultData(skillIndex)
			self.AppendMiningSkillData(skillGrade, skillLevel)

		elif player.SKILL_INDEX_RIDING == skillIndex:

			slotIndex = player.GetSkillSlotIndex(skillIndex)
			self.AppendSupportSkillDefaultData(skillIndex, skillGrade, skillLevel, 30)

		elif player.SKILL_INDEX_SUMMON == skillIndex:

			maxLevel = 10

			self.ClearToolTip()
			self.__SetSkillTitle(skillIndex, skillGrade)

			## Description
			description = skill.GetSkillDescription(skillIndex)
			self.AppendDescription(description, 25)

			if skillLevel == 10:
				self.AppendSpace(5)
				self.AppendTextLine(localeInfo.TOOLTIP_SKILL_LEVEL_MASTER % (skillLevel), self.NORMAL_COLOR)
				self.AppendTextLine(localeInfo.SKILL_SUMMON_DESCRIPTION % (skillLevel*10), self.NORMAL_COLOR)

			else:
				self.AppendSpace(5)
				self.AppendTextLine(localeInfo.TOOLTIP_SKILL_LEVEL % (skillLevel), self.NORMAL_COLOR)
				self.__AppendSummonDescription(skillLevel, self.NORMAL_COLOR)

				self.AppendSpace(5)
				self.AppendTextLine(localeInfo.TOOLTIP_SKILL_LEVEL % (skillLevel+1), self.NEGATIVE_COLOR)
				self.__AppendSummonDescription(skillLevel+1, self.NEGATIVE_COLOR)

		elif skill.SKILL_TYPE_GUILD == skill.GetSkillType(skillIndex):

			if self.SKILL_TOOL_TIP_WIDTH != self.toolTipWidth:
				self.toolTipWidth = self.SKILL_TOOL_TIP_WIDTH
				self.ResizeToolTip()

			self.AppendDefaultData(skillIndex)
			self.AppendSkillConditionData(skillIndex)
			self.AppendGuildSkillData(skillIndex, skillLevel)

		else:

			if self.SKILL_TOOL_TIP_WIDTH != self.toolTipWidth:
				self.toolTipWidth = self.SKILL_TOOL_TIP_WIDTH
				self.ResizeToolTip()

			slotIndex = player.GetSkillSlotIndex(skillIndex)

			skillCurrentPercentage = player.GetSkillCurrentEfficientPercentage(slotIndex)
			skillNextPercentage = player.GetSkillNextEfficientPercentage(slotIndex)

			if skillIndex == skill.SKILL_SPRINT:
				skillLevel = 1

			self.AppendDefaultData(skillIndex, skillGrade)
			self.AppendSkillConditionData(skillIndex)
			self.AppendSkillDataNew(slotIndex, skillIndex, skillGrade, skillLevel, skillCurrentPercentage, skillNextPercentage)
			self.AppendSkillRequirement(skillIndex, skillLevel)

			if skillIndex == skill.SKILL_SPRINT:
				self.AppendSpace(5)
				self.AppendTextLine(localeInfo.TOOLTIP_SPRINT)

		self.ShowToolTip()

	def __SetSkillTitle(self, skillIndex, skillGrade):
		self.SetTitle(skill.GetSkillName(skillIndex, skillGrade))
		self.__AppendSkillGradeName(skillIndex, skillGrade)

	def __AppendSkillGradeName(self, skillIndex, skillGrade):
		if self.SKILL_GRADE_NAME.has_key(skillGrade):
			self.AppendSpace(5)
			self.AppendTextLine(self.SKILL_GRADE_NAME[skillGrade] % (skill.GetSkillName(skillIndex, 0)), self.CAN_LEVEL_UP_COLOR)

	def SetSkillOnlyName(self, slotIndex, skillIndex, skillGrade):
		if 0 == skillIndex:
			return

		slotIndex = player.GetSkillSlotIndex(skillIndex)

		self.toolTipWidth = self.SKILL_TOOL_TIP_WIDTH
		self.ResizeToolTip()

		self.ClearToolTip()
		self.__SetSkillTitle(skillIndex, skillGrade)
		self.AppendDefaultData(skillIndex, skillGrade)
		self.AppendSkillConditionData(skillIndex)
		self.ShowToolTip()

	def AppendDefaultData(self, skillIndex, skillGrade = 0):
		self.ClearToolTip()
		self.__SetSkillTitle(skillIndex, skillGrade)

		## Level Limit
		levelLimit = skill.GetSkillLevelLimit(skillIndex)
		if levelLimit > 0:

			color = self.NORMAL_COLOR
			if player.GetStatus(player.LEVEL) < levelLimit:
				color = self.NEGATIVE_COLOR

			self.AppendSpace(5)
			self.AppendTextLine(localeInfo.TOOLTIP_ITEM_LIMIT_LEVEL % (levelLimit), color)

		## Description
		description = skill.GetSkillDescription(skillIndex)
		self.AppendDescription(description, 25)

	def AppendSupportSkillDefaultData(self, skillIndex, skillGrade, skillLevel, maxLevel):
		self.ClearToolTip()
		self.__SetSkillTitle(skillIndex, skillGrade)

		## Description
		description = skill.GetSkillDescription(skillIndex)
		self.AppendDescription(description, 25)

		if 1 == skillGrade:
			skillLevel += 19
		elif 2 == skillGrade:
			skillLevel += 29
		elif 3 == skillGrade:
			skillLevel = 40

		self.AppendSpace(5)
		self.AppendTextLine(localeInfo.TOOLTIP_SKILL_LEVEL_WITH_MAX % (skillLevel, maxLevel), self.NORMAL_COLOR)

	def AppendSkillConditionData(self, skillIndex):
		conditionDataCount = skill.GetSkillConditionDescriptionCount(skillIndex)
		if conditionDataCount > 0:
			self.AppendSpace(5)
			for i in xrange(conditionDataCount):
				conditionText = skill.GetSkillConditionDescription(skillIndex, i)
				if "|" in conditionText:
					conditionTable = conditionText.split("|")
					for text in conditionTable:
						self.AppendTextLine(text, self.CONDITION_COLOR)
				else:
					self.AppendTextLine(conditionText, self.CONDITION_COLOR)

	def AppendGuildSkillData(self, skillIndex, skillLevel):
		skillMaxLevel = 7
		skillCurrentPercentage = float(skillLevel) / float(skillMaxLevel)
		skillNextPercentage = float(skillLevel+1) / float(skillMaxLevel)
		## Current Level
		if skillLevel > 0:
			if self.HasSkillLevelDescription(skillIndex, skillLevel):
				self.AppendSpace(5)
				if skillLevel == skillMaxLevel:
					self.AppendTextLine(localeInfo.TOOLTIP_SKILL_LEVEL_MASTER % (skillLevel), self.NORMAL_COLOR)
				else:
					self.AppendTextLine(localeInfo.TOOLTIP_SKILL_LEVEL % (skillLevel), self.NORMAL_COLOR)

				#####

				for i in xrange(skill.GetSkillAffectDescriptionCount(skillIndex)):
					self.AppendTextLine(skill.GetSkillAffectDescription(skillIndex, i, skillLevel, skillCurrentPercentage), self.ENABLE_COLOR)

				## Cooltime
				coolTime = skill.GetSkillCoolTime(skillIndex, skillCurrentPercentage)
				if coolTime > 0:
					self.AppendTextLine(localeInfo.TOOLTIP_SKILL_COOL_TIME + str(coolTime), self.ENABLE_COLOR)

				## SP
				needGSP = skill.GetSkillNeedSP(skillIndex, skillCurrentPercentage)
				if needGSP > 0:
					self.AppendTextLine(localeInfo.TOOLTIP_NEED_GSP % (needGSP), self.ENABLE_COLOR)

		## Next Level
		if skillLevel < skillMaxLevel:
			if self.HasSkillLevelDescription(skillIndex, skillLevel+1):
				self.AppendSpace(5)
				self.AppendTextLine(localeInfo.TOOLTIP_NEXT_SKILL_LEVEL_1 % (skillLevel+1, skillMaxLevel), self.DISABLE_COLOR)

				#####

				for i in xrange(skill.GetSkillAffectDescriptionCount(skillIndex)):
					self.AppendTextLine(skill.GetSkillAffectDescription(skillIndex, i, skillLevel+1, skillNextPercentage), self.DISABLE_COLOR)

				## Cooltime
				coolTime = skill.GetSkillCoolTime(skillIndex, skillNextPercentage)
				if coolTime > 0:
					self.AppendTextLine(localeInfo.TOOLTIP_SKILL_COOL_TIME + str(coolTime), self.DISABLE_COLOR)

				## SP
				needGSP = skill.GetSkillNeedSP(skillIndex, skillNextPercentage)
				if needGSP > 0:
					self.AppendTextLine(localeInfo.TOOLTIP_NEED_GSP % (needGSP), self.DISABLE_COLOR)

	def __GetRealSkillLevel(self, skillLevel, skillGrade):
		if skillGrade == 0:
			return skillLevel
		elif skillGrade == 1:
			return 19 + skillLevel
		elif skillGrade == 2:
			return 29 + skillLevel
		elif skillGrade == 3:
			return 40

	def AppendSkillDataNew(self, slotIndex, skillIndex, skillGrade, skillLevel, skillCurrentPercentage, skillNextPercentage):

		self.skillMaxLevelStartDict = { 0 : 17, 1 : 7, 2 : 10, }
		self.skillMaxLevelEndDict = { 0 : 20, 1 : 10, 2 : 10, }

		skillLevelUpPoint = 1
		realSkillGrade = player.GetSkillGrade(slotIndex)
		skillMaxLevelStart = self.skillMaxLevelStartDict.get(realSkillGrade, 15)
		skillMaxLevelEnd = self.skillMaxLevelEndDict.get(realSkillGrade, 20)

		if skillIndex == skill.SKILL_SPRINT:
			skillMaxLevelEnd = 1

		## Current Level
		if skillLevel > 0:
			if self.HasSkillLevelDescription(skillIndex, skillLevel):
				self.AppendSpace(5)
				if skillGrade == skill.SKILL_GRADE_COUNT or skillIndex == skill.SKILL_SPRINT:
					pass
				elif skillLevel == skillMaxLevelEnd:
					self.AppendTextLine(localeInfo.TOOLTIP_SKILL_LEVEL_MASTER % (skillLevel), self.NORMAL_COLOR)
				else:
					self.AppendTextLine(localeInfo.TOOLTIP_SKILL_LEVEL % (skillLevel), self.NORMAL_COLOR)
				self.AppendSkillLevelDescriptionNew(skillIndex, skillCurrentPercentage, self.__GetRealSkillLevel(skillLevel, skillGrade), self.ENABLE_COLOR)
			else:
				print "nara"
		## Next Level
		if skillGrade != skill.SKILL_GRADE_COUNT:
			if skillLevel < skillMaxLevelEnd:
				if self.HasSkillLevelDescription(skillIndex, skillLevel+skillLevelUpPoint):
					self.AppendSpace(5)
					if skillIndex == 141 or skillIndex == 142:
						self.AppendTextLine(localeInfo.TOOLTIP_NEXT_SKILL_LEVEL_3 % (skillLevel+1), self.DISABLE_COLOR)
					else:
						self.AppendTextLine(localeInfo.TOOLTIP_NEXT_SKILL_LEVEL_1 % (skillLevel+1, skillMaxLevelEnd), self.DISABLE_COLOR)
					self.AppendSkillLevelDescriptionNew(skillIndex, skillNextPercentage, self.__GetRealSkillLevel(skillLevel+1, skillGrade), self.DISABLE_COLOR)

	def AppendSkillLevelDescriptionNew(self, skillIndex, skillPercentage, skillLevel, color):

		affectDataCount = skill.GetNewAffectDataCount(skillIndex)
		if affectDataCount > 0:
			for i in xrange(affectDataCount):
				type, minValue, maxValue = skill.GetNewAffectData(skillIndex, i, skillPercentage)

				if not self.AFFECT_NAME_DICT.has_key(type):
					continue

				minValue = int(minValue)
				maxValue = int(maxValue)
				affectText = self.AFFECT_NAME_DICT[type]

				if "HP" == type:
					if minValue < 0 and maxValue < 0:
						minValue *= -1
						maxValue *= -1

					else:
						affectText = localeInfo.TOOLTIP_SKILL_AFFECT_HEAL

				affectText += str(minValue)
				if minValue != maxValue:
					affectText += " - " + str(maxValue)
				affectText += self.AFFECT_APPEND_TEXT_DICT.get(type, "")

				#import debugInfo
				#if debugInfo.IsDebugMode():
				#	affectText = "!!" + affectText

				self.AppendTextLine(affectText, color)

		else:
			for i in xrange(skill.GetSkillAffectDescriptionCount(skillIndex)):
				self.AppendTextLine(skill.GetSkillAffectDescription(skillIndex, i, skillLevel, skillPercentage), color)


		## Duration
		duration = skill.GetDuration(skillIndex, skillPercentage)
		if duration > 0:
			duration += duration * player.GetStatus(player.POINT_SKILL_DURATION) / 100
			duration += player.GetStatus(player.POINT_PARTY_BUFFER_BONUS)
			self.AppendTextLine(localeInfo.TOOLTIP_SKILL_DURATION % (duration), color)

		## Cooltime
		coolTime = skill.GetSkillCoolTime(skillIndex, skillPercentage)
		if coolTime > 0:
			realCoolTime = "{:.2f}".format(coolTime * player.GetCoolTimeReduction())
			if '.' in realCoolTime:
				realCoolTime = realCoolTime.rstrip('0').rstrip('.')
			coolTimeInfo = "{} s ".format(realCoolTime)
			self.AppendTextLine(localeInfo.TOOLTIP_SKILL_COOL_TIME + coolTimeInfo, color)

		## SP
		needSP = skill.GetSkillNeedSP(skillIndex, skillPercentage)
		if needSP != 0:
			continuationSP = skill.GetSkillContinuationSP(skillIndex, skillPercentage)

			if skill.IsUseHPSkill(skillIndex):
				self.AppendNeedHP(needSP, continuationSP, color)
			elif skill.IsUseStaminaSkill(skillIndex):
				self.AppendNeedStamina(needSP, continuationSP, color)
			else:
				self.AppendNeedSP(needSP, continuationSP, color)

	def AppendSkillRequirement(self, skillIndex, skillLevel):

		skillMaxLevel = skill.GetSkillMaxLevel(skillIndex)

		if skillIndex == skill.SKILL_SPRINT:
			return

		if skillLevel >= skillMaxLevel:
			return

		isAppendHorizontalLine = False

		## Requirement
		if skill.IsSkillRequirement(skillIndex):

			if not isAppendHorizontalLine:
				isAppendHorizontalLine = True
				self.AppendHorizontalLine()

			requireSkillName, requireSkillLevel = skill.GetSkillRequirementData(skillIndex)

			color = self.CANNOT_LEVEL_UP_COLOR
			if skill.CheckRequirementSueccess(skillIndex):
				color = self.CAN_LEVEL_UP_COLOR
			self.AppendTextLine(localeInfo.TOOLTIP_REQUIREMENT_SKILL_LEVEL % (requireSkillName, requireSkillLevel), color)

		## Require Stat
		requireStatCount = skill.GetSkillRequireStatCount(skillIndex)
		if requireStatCount > 0:

			for i in xrange(requireStatCount):
				type, level = skill.GetSkillRequireStatData(skillIndex, i)
				if self.POINT_NAME_DICT.has_key(type):

					if not isAppendHorizontalLine:
						isAppendHorizontalLine = True
						self.AppendHorizontalLine()

					name = self.POINT_NAME_DICT[type]
					color = self.CANNOT_LEVEL_UP_COLOR
					if player.GetStatus(type) >= level:
						color = self.CAN_LEVEL_UP_COLOR
					self.AppendTextLine(localeInfo.TOOLTIP_REQUIREMENT_STAT_LEVEL % (name, level), color)

	def HasSkillLevelDescription(self, skillIndex, skillLevel):
		if skill.GetSkillAffectDescriptionCount(skillIndex) > 0:
			return True
		if skill.GetSkillCoolTime(skillIndex, skillLevel) > 0:
			return True
		if skill.GetSkillNeedSP(skillIndex, skillLevel) > 0:
			return True

		return False

	def AppendMasterAffectDescription(self, index, desc, color):
		self.AppendTextLine(desc, color)

	def AppendNextAffectDescription(self, index, desc):
		self.AppendTextLine(desc, self.DISABLE_COLOR)

	def AppendNeedHP(self, needSP, continuationSP, color):

		self.AppendTextLine(localeInfo.TOOLTIP_NEED_HP % (needSP), color)

		if continuationSP > 0:
			self.AppendTextLine(localeInfo.TOOLTIP_NEED_HP_PER_SEC % (continuationSP), color)

	def AppendNeedStamina(self, needStamina, continuationStamina, color):

		self.AppendTextLine(localeInfo.TOOLTIP_NEED_STAMINA % (needStamina), color)

		if continuationStamina > 0:
			self.AppendTextLine(localeInfo.TOOLTIP_NEED_STAMINA_PER_SEC % (continuationStamina), color)

	def AppendNeedSP(self, needSP, continuationSP, color):

		if -1 == needSP:
			self.AppendTextLine(localeInfo.TOOLTIP_NEED_ALL_SP, color)

		else:
			self.AppendTextLine(localeInfo.TOOLTIP_NEED_SP % (needSP), color)

		if continuationSP > 0:
			self.AppendTextLine(localeInfo.TOOLTIP_NEED_SP_PER_SEC % (continuationSP), color)

	def AppendMiningSkillData(self, skillGrade, skillLevel):
		real_skill_level = skillLevel
		if skillGrade == 1:
			real_skill_level += 19
		elif skillGrade == 2:
			real_skill_level += 29
		elif skillGrade == 3:
			real_skill_level = 40

		if real_skill_level < 1:
			return

		skill_bonus_table = [
			0,
			1, 1, 1, 1,  # 1 - 4
			2, 2, 2, 2,  # 5 - 8
			3, 3, 3, 3,  # 9 - 12
			4, 4, 4, 4,  # 13 - 16
			5, 5, 5, 5,  # 17 - 20
			6, 6, 6, 6,  # 21 - 24
			7, 7, 7, 7,  # 25 - 28
			8, 8, 8, 8,  # 29 - 32
			9, 9, 9, 9,  # 33 - 36
			10, 10, 10,  # 37 - 39
			11,  # 40
		]

		self.AppendSpace(5)
		bonus = skill_bonus_table[real_skill_level]
		self.AppendTextLine(localeInfo.MINING_SKILL_BONUS % bonus)
		self.AppendTextLine(localeInfo.GATHER_SKILL_BONUS % bonus)

	def AppendPartySkillData(self, skillGrade, skillLevel, is_next_level=False):
		if is_next_level and skillLevel > 40:
			return

		# @fixme008 BEGIN
		def comma_fix(vl):
			return vl.replace("%,0f", "%.0f")
		# @fixme008 END

		if 1 == skillGrade:
			skillLevel += 19
		elif 2 == skillGrade:
			skillLevel += 29
		elif 3 == skillGrade:
			skillLevel = 40

		if skillLevel <= 0:
			return

		skillIndex = player.SKILL_INDEX_TONGSOL
		slotIndex = player.GetSkillSlotIndex(skillIndex)
		skillPower = player.GetSkillCurrentEfficientPercentage(slotIndex)
		k = skillPower # @fixme008

		color = self.NEGATIVE_COLOR if is_next_level else self.NORMAL_COLOR

		self.AppendSpace(5)
		if is_next_level:
			self.AppendTextLine(localeInfo.TOOLTIP_PARTY_NEXT_SKILL_LEVEL % skillLevel, color)
		else:
			self.AppendTextLine(localeInfo.TOOLTIP_PARTY_SKILL_LEVEL % skillLevel, color)

		# @fixme008 BEGIN
		if skillLevel >= 1:
			self.AppendTextLine(
				localeInfo.PARTY_ROLE_DEFENDER + " " + localeInfo.PARTY_SKILL_DEFENDER % chop(4 + 63 * k), color)

		if skillLevel >= 10:
			self.AppendTextLine(
				localeInfo.PARTY_ROLE_ATTACKER + " " + localeInfo.PARTY_SKILL_ATTACKER % chop(10 + 75 * k), color)

		if skillLevel >= 10:
			self.AppendTextLine(localeInfo.PARTY_ROLE_BUFFER + " " + localeInfo.PARTY_SKILL_BUFFER % chop(5 + 45 * k),
								color)

		if skillLevel >= 15:
			self.AppendTextLine(
				localeInfo.PARTY_ROLE_BERSERK + " " + localeInfo.PARTY_SKILL_BERSERKER % chop(2 + 12 * k), color)

		if skillLevel >= 20:
			self.AppendTextLine(
				localeInfo.PARTY_ROLE_TANKER + " " + localeInfo.PARTY_SKILL_TANKER % chop(50 + 1450 * k), color)

		if skillLevel >= 20:
			self.AppendTextLine(
				localeInfo.PARTY_ROLE_SKILL_MASTER + " " + localeInfo.PARTY_SKILL_SKILL_MASTER % chop(2 + 15 * k),
				color)
		# @fixme008 END

	def __AppendSummonDescription(self, skillLevel, color):
		if skillLevel > 1:
			self.AppendTextLine(localeInfo.SKILL_SUMMON_DESCRIPTION % (skillLevel * 10), color)
		elif 1 == skillLevel:
			self.AppendTextLine(localeInfo.SKILL_SUMMON_DESCRIPTION % (15), color)
		elif 0 == skillLevel:
			self.AppendTextLine(localeInfo.SKILL_SUMMON_DESCRIPTION % (10), color)

_instanceItemToolTip = None

def GetItemToolTipInstance():
	global _instanceItemToolTip

	if not _instanceItemToolTip:
		SetItemToolTipInstance(ItemToolTip())

	return _instanceItemToolTip

def SetItemToolTipInstance(instance):
	global _instanceItemToolTip

	if _instanceItemToolTip:
		del _instanceItemToolTip

	_instanceItemToolTip = instance

