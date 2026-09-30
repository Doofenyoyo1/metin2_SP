#-*- coding: iso-8859-1 -*-
#
# Title: Dungeon Information System
# Author: Owsap
# Description: List of all available dungeons.
# Date: 2021.01.09
# Last Update: 2021.06.03
# Version 2.0.0.2
#
# Skype: owsap.
# Discord: Owsap#0905
#
# 0x0
#
# Web: https://owsap-productions.com/
# GitHub: https://github.com/Owsap
#
#
# MT2009_PLUS_DUNGEON_PANEL_V1 (MT2009 PLUS): the Arezzo client's window (the owner has the rights to
# the Arezzo files) on our client. The exe 2.0.25 has no C++ "dungeonInfo" module: dungeoninfo.py
# is a Python module with the same functions, filled by the server's "DungeonInfo" command lines
# (playerbot_dungeon_panel.h, "/lochy"). Changed here: no emoji / uiChestDropInfo / item.HasDropInfo
# / SMOOTH_SCROLL / player.GetLevel (the exe has none of them), the cooldown clock of the
# list is on (the server sends each dungeon's cooldown), the teleport question closes by itself after
# its 5 seconds, the Arezzo-only bonus tables are gone.
#

import app
import ui

import localeInfo
# import uiScriptLocale
import wndMgr
import item
# import nonplayer
import uiToolTip
import grp
import player
import dungeonInfo
import uiCommon
import chat

from _weakref import proxy

AFFECT_DATA = {}

ATT_BONUS_INDEX = 0
DEF_BONUS_INDEX = 1

RANK_SCORE = 1
RANK_TIME = 2
RANK_DAMAGE = 3

ROOT = "d:/ymir work/ui/game/dungeon_info/"
ICON_ROOT = "d:/ymir work/ui/dungeon_info/"
#ICON_ROOT = "locale/common/dungeon_icon/"
PATH = "d:/ymir work/ui/game/dungeon_info/"

MAX_DUNGEON_SCROLL = 8

SMOOTH_SCROLL = True
ITEM_TYPE_GACHA = getattr(item, "ITEM_TYPE_GACHA", -99)

def _PlayerLevel():
	return player.GetStatus(player.LEVEL)

def _HasDropInfo(vnum, isMain):
	return False

# The teleport question: closes as "no" when its time is up (Arezzo's exe has SetCancelOnTimeOver).
# MT2009: built on QuestionDialog2 with its own countdown - our QuestionDialogWithTimeLimit skips
# QuestionDialog.__init__ (no closeEvent), so its Close() raised and the buttons did nothing.
class TeleportQuestionDialog(uiCommon.QuestionDialog2):
	def __init__(self):
		uiCommon.QuestionDialog2.__init__(self)
		self.cancelOnTimeOver = False
		self.cancelEventMt = None
		self.endTime = 0
		self.key = 0

	def Open(self, msg, timeout):
		self.SetText1(msg)
		self.endTime = app.GetTime() + timeout
		self.__UpdateLeft()
		uiCommon.QuestionDialog2.Open(self)

	def SetCancelEvent(self, event):
		self.cancelEventMt = event
		uiCommon.QuestionDialog2.SetCancelEvent(self, event)

	def SetCancelOnTimeOver(self):
		self.cancelOnTimeOver = True

	def __UpdateLeft(self):
		left = int(max(0, self.endTime - app.GetTime()) + 0.99)
		self.SetText2("Pozosta\xb3y czas: %d s" % left)

	def OnUpdate(self):
		self.__UpdateLeft()
		if self.cancelOnTimeOver and self.endTime and app.GetTime() >= self.endTime:
			self.cancelOnTimeOver = False
			if self.cancelEventMt:
				self.cancelEventMt()
			else:
				self.Close()

	def OnPressEscapeKey(self):
		if self.cancelEventMt:
			self.cancelEventMt()
		else:
			self.Close()
		return True

TABLICA_BONUSOW_REKAWICA = []

def SecondToHMSGolbal(time):
	second = int(time % 60)
	minute = int((time / 60) % 60)
	hour = int((time / 60) / 60) % 24

	text = ""

	if hour > 9:
		text += str(hour) + ":"
	else:
		text += "0" + str(hour) + ":"

	if minute > 9:
		text += str(minute) + ":"
	else:
		text += "0" + str(minute) + ":"

	if second > 9:
		text += str(second)
	else:
		text += "0" + str(second)

	return text

def NumberToDamageString(n) :
	if n <= 0 :
		return "0"

	return "%s" % ('.'.join([ i-3<0 and str(n)[:i] or str(n)[i-3:i] for i in range(len(str(n))%3, len(str(n))+1, 3) if i ]))

DUNGEON_CLOSED = 0
DUNGEON_AVAILABLE = 1
DUNGEON_COOLDOWN = 2
DUNGEON_LEVEL_HIGH = 3
DUNGEON_LEVEL_LOW = 4

class ScrollBar(ui.Window):
	SCROLLBAR_WIDTH = 17
	SCROLLBAR_MIDDLE_HEIGHT = 9
	SCROLLBAR_BUTTON_HEIGHT = 0
	MIDDLE_BAR_POS = 8
	MIDDLE_BAR_UPPER_PLACE = 3
	MIDDLE_BAR_DOWNER_PLACE = 5
	TEMP_SPACE = MIDDLE_BAR_UPPER_PLACE + MIDDLE_BAR_DOWNER_PLACE
	if SMOOTH_SCROLL:
		SMOOTH_RATIO = 4.0 # UPPER = SLOWER

	def __init__(self):
		ui.Window.__init__(self)

		self.pageSize = 1
		self.curPos = 0.0
		self.eventScroll = lambda *arg: None
		self.lockFlag = False
		self.scrollStep = 0.20
		if SMOOTH_SCROLL:
			self.smooth_mode = False
			self.actual_pos = 0.0
			self.target_pos = 0.0

		self.CreateScrollBar()

	def __del__(self):
		ui.Window.__del__(self)

	def CreateScrollBar(self):
		barSlot = ui.ExpandedImageBox()
		barSlot.SetParent(self)
		barSlot.AddFlag("not_pick")
		barSlot.LoadImage(PATH+"scroll_slot.png")
		barSlot.Show()

		middleBar = ui.DragButton()
		middleBar.SetParent(self)
		middleBar.AddFlag("movable")
		middleBar.SetMoveEvent(ui.__mem_func__(self.OnMove))
		middleBar.SetUpVisual(PATH+"scroll_button.png")
		middleBar.SetOverVisual(PATH+"scroll_button.png")
		middleBar.SetDownVisual(PATH+"scroll_button.png")
		middleBar.Show()

		self.middleBar = middleBar
		self.barSlot = barSlot

		self.SCROLLBAR_WIDTH = self.middleBar.GetWidth()
		self.SCROLLBAR_MIDDLE_HEIGHT = self.middleBar.GetHeight()

	def Destroy(self):
		self.middleBar = None
		self.eventScroll = lambda *arg: None

	def SetScrollEvent(self, event):
		self.eventScroll = event

	def SetMiddleBarSize(self, pageScale):
		pass

	def SetScrollBarSize(self, height):
		self.pageSize = (height - self.SCROLLBAR_BUTTON_HEIGHT * 2) - self.SCROLLBAR_MIDDLE_HEIGHT - (self.TEMP_SPACE)
		self.SetSize(self.SCROLLBAR_WIDTH, height)
		self.middleBar.SetRestrictMovementArea(self.MIDDLE_BAR_POS,
											   self.SCROLLBAR_BUTTON_HEIGHT + self.MIDDLE_BAR_UPPER_PLACE,
											   self.MIDDLE_BAR_POS + 2,
											   height - self.SCROLLBAR_BUTTON_HEIGHT * 2 - self.TEMP_SPACE)
		self.middleBar.SetPosition(self.MIDDLE_BAR_POS, 0)

		self.UpdateBarSlot()

	def UpdateBarSlot(self):
		self.barSlot.SetScale(1.0, float(self.GetHeight()) / float(self.barSlot.GetHeight()))

	def GetPos(self):
		return self.curPos

	def SetPos(self, pos):
		pos = max(0.0, pos)
		pos = min(1.0, pos)

		newPos = float(self.pageSize) * pos
		self.middleBar.SetPosition(self.MIDDLE_BAR_POS,
								   int(newPos) + self.SCROLLBAR_BUTTON_HEIGHT + self.MIDDLE_BAR_UPPER_PLACE)
		self.OnMove()

	def SetScrollStep(self, step):
		self.scrollStep = step

	def GetScrollStep(self):
		return self.scrollStep

	def OnMouseWheel(self, nLen):
		if nLen > 0:
			self.OnUp()
			return True
		elif nLen < 0:
			self.OnDown()
			return True
		return False

	def OnUp(self):
		if SMOOTH_SCROLL and self.smooth_mode:
			self.actual_pos = min(1.0, max(0.0, self.curPos))
			self.target_pos = min(1.0, max(0.0, self.curPos-self.scrollStep))
		else:
			self.SetPos(self.curPos-self.scrollStep)

	def OnDown(self):
		if SMOOTH_SCROLL and self.smooth_mode:
			self.actual_pos = min(1.0, max(0.0, self.curPos))
			self.target_pos = min(1.0, max(0.0, self.curPos+self.scrollStep))
		else:
			self.SetPos(self.curPos+self.scrollStep)

	def OnMove(self):

		if self.lockFlag:
			return

		if 0 == self.pageSize:
			return

		(xLocal, yLocal) = self.middleBar.GetLocalPosition()
		self.curPos = float(yLocal - self.SCROLLBAR_BUTTON_HEIGHT - self.MIDDLE_BAR_UPPER_PLACE) / float(self.pageSize)

		self.eventScroll()

	def OnMouseLeftButtonDown(self):
		(xMouseLocalPosition, yMouseLocalPosition) = self.GetMouseLocalPosition()
		pickedPos = yMouseLocalPosition - self.SCROLLBAR_BUTTON_HEIGHT - self.SCROLLBAR_MIDDLE_HEIGHT / 2
		newPos = float(pickedPos) / float(self.pageSize)
		if SMOOTH_SCROLL and self.smooth_mode:
			self.actual_pos = min(1.0, max(0.0, self.curPos))
			self.target_pos = min(1.0, max(0.0, newPos))
		else:
			self.SetPos(newPos)

	def LockScroll(self):
		self.lockFlag = True

	def UnlockScroll(self):
		self.lockFlag = False

	if SMOOTH_SCROLL:
		def EnableSmoothMode(self):
			self.smooth_mode = True

		def OnUpdate(self):
			if not self.smooth_mode:
				return
			
			if self.lockFlag:
				return
			
			if 0 == self.pageSize:
				return
			
			if self.actual_pos == self.target_pos:
				return

			distance = abs(self.actual_pos - self.target_pos)
			smooth_step = max(distance / ScrollBar.SMOOTH_RATIO, 0.005)
		
			if self.actual_pos < self.target_pos:
				self.actual_pos = min(self.actual_pos + smooth_step, self.target_pos)
			elif self.actual_pos > self.target_pos:
				self.actual_pos = max(self.actual_pos - smooth_step, self.target_pos)
			
			self.SetPos(self.actual_pos)


class ListToggleButton(object):
	def __init__(self, parent = None, key = 0):
		self.parent = parent
		self.key = key

		self.event = lambda *arg: None
		self.arg = 0

		button = ui.RadioButton()
		button.SetParent(parent)
		button.SetPosition(4, 4 + ((50 - 2) * key))
		button.SetUpVisual(ROOT + "list_btn_default.png")
		button.SetOverVisual(ROOT + "list_btn_over.png")
		button.SetDownVisual(ROOT + "list_btn_down.png")
		button.Show()
		self.button = button

		name = ui.TextLine()
		name.SetParent(button)
		name.SetPosition(120, 8)
		name.SetText("")
		name.SetOutline(True)
		name.SetHorizontalAlignCenter()
		name.Show()
		self.name = name

		icon = ui.ImageBox()
		icon.SetParent(button)
		icon.SetPosition(3, 3)
		icon.LoadImage(ICON_ROOT + "0.png")
		icon.Show()
		self.icon = icon

		clock = ui.ImageBox()
		clock.SetParent(button)
		clock.SetPosition(190, 5)
		clock.LoadImage(ROOT + "clock.png")
		clock.Hide()
		self.clock = clock

		clockTime = ui.TextLine()
		clockTime.SetParent(clock)
		clockTime.SetPosition(10, 20)
		clockTime.SetText("00:00:00")
		clockTime.SetHorizontalAlignCenter()
		clockTime.Hide()
		self.clockTime = clockTime

		status = ui.TextLine()
		status.SetParent(button)
		status.SetPosition(120, 25)
		status.SetText(localeInfo.DUNGEON_INFO_STATUS_AVAILABLE)
		status.SetFontColor((68.00 / 255), (162.00 / 255), (69.00 / 255))
		status.SetHorizontalAlignCenter()
		status.SetOutline(True)
		status.Show()
		self.status = status

		self.cooldown = 0
		self.ticking = False

	def Show(self):
		if self.button:
			self.button.Show()

	def Hide(self):
		if self.button:
			self.button.Hide()

	def __del__(self):
		self.parent = None
		self.key = None

		self.enable = True
		self.event = lambda *arg: None
		self.arg = 0

		self.button = None
		self.icon = None

		self.status = None

		self.clock = None
		self.clockTime = None

		self.cooldown = 0
		self.ticking = False

	def SetKey(self, key):
		self.key = key

	def GetKey(self):
		return self.key

	def SetText(self, map_index):
		if self.name:
			self.name.SetText(localeInfo.GetMiniMapZoneNameByIdx(map_index))

	def SetStatus(self, dataDict):
		if self.status:
			try:
				text = dataDict[0]
				r, g, b = dataDict[1]
				self.status.SetText(text)
				self.status.SetFontColor(r, g, b)
			except KeyError:
				self.status.SetText("")

	def SetIcon(self, map_index):
		if self.icon:
			iconFile = ICON_ROOT + "%d.png" % map_index
			if app.IsExistFile(iconFile):
				self.icon.LoadImage(iconFile)
			else:
				self.icon.LoadImage(ICON_ROOT + "0.png")

	def ResetClock(self, trigger, time):
		self.ticking = False
		if not self.cooldown:
			self.SetClock(trigger, time)

	def SetClock(self, trigger, time = 0, reset = False):
		self.ShowClock()

		if not self.ticking:
			self.clockTime.SetText("%s" % SecondToHMSGolbal(time))
			self.cooldown = app.GetGlobalTimeStamp() + time

			self.ticking = True

	def ShowClock(self):
		if self.clock and self.clockTime:
			self.clock.Show()
			self.clockTime.Show()

	def HideClock(self):
		self.clock.Hide()
		self.clockTime.Hide()

		self.ticking = False

	def OnUpdate(self):
		if self.cooldown and self.ticking:
			leftSec = max(0, self.cooldown - app.GetGlobalTimeStamp())
			if leftSec > 0:
				self.clockTime.SetText("%s" % SecondToHMSGolbal(leftSec))
			else:
				self.cooldown = 0
				self.HideClock()
		else:
			self.HideClock()

	def SetEvent(self, event, arg):
		self.event = event
		self.arg = arg

		if self.button:
			self.button.SetEvent(self.event, self.arg)

	def GetObject(self):
		if self.button:
			return self.button

class DungeonRankingWindow(ui.ScriptWindow):
	SLOT_RANKING = 0
	SLOT_NAME = 1
	SLOT_LEVEL = 2
	SLOT_POINT = 3

	MAX_LINE_COUNT = dungeonInfo.MAX_RANKING_LINES

	def __init__(self):
		ui.ScriptWindow.__init__(self)
		self.Initialize()

		self.__LoadWindow()

	def __del__(self):
		ui.ScriptWindow.__del__(self)
		self.Initialize()

	def Initialize(self):
		self.isLoaded = False
		self.type = None

		self.resultButtonList = []
		self.resultSlotList = {}
		self.myResultSlotList = []

	def Destroy(self):
		self.Initialize()

	def __LoadWindow(self):
		if self.isLoaded:
			return

		self.isLoaded = True

		try:
			pyScrLoader = ui.PythonScriptLoader()
			pyScrLoader.LoadScriptFile(self, "UIScript/DungeonRankingWindow.py")
		except:
			import exception
			exception.Abort("DungeonRankingWindow.__LoadWindow")

		try:
			self.titleBar = self.GetChild("TitleBar")
			self.titleBarName = self.GetChild("TitleBarName")
			self.waitAniImg = self.GetChild("WaitAniImg")

		except:
			import exception
			exception.Abort("DungeonRankingWindow.__LoadWindow.SetObject")

		if self.titleBar:
			self.titleBar.SetCloseEvent(ui.__mem_func__(self.Close))

		self.MakeUIBoard()

	def MakeUIBoard(self):
		yPos = 0
		for i in range(0, self.MAX_LINE_COUNT + 1):
			yPos = 57 + i * 24
			if i == 10:
				yPos += 9

			itemSlotButtonImage = ui.MakeButton(self, 19, yPos, "", "d:/ymir work/ui/game/dungeon_info/", "ranking_slot.png", "ranking_slot.png", "ranking_slot.png")
			itemSlotButtonImage.Show()
			itemSlotButtonImage.Disable()
			self.Children.append(itemSlotButtonImage)

			if i < self.MAX_LINE_COUNT:
				self.resultButtonList.append(itemSlotButtonImage)

			## Position
			rankingSlotImage = ui.MakeImageBox(self, "d:/ymir work/ui/public/parameter_slot_00.sub", 25, yPos)
			rankingSlotImage.SetAlpha(0)
			rankingSlot = ui.MakeTextLine(rankingSlotImage)
			self.Children.append(rankingSlotImage)
			self.Children.append(rankingSlot)

			## Name
			nameImage = ui.MakeImageBox(self, "d:/ymir work/ui/public/parameter_slot_04.sub", 108, yPos)
			nameImage.SetAlpha(0)
			nameSlot = ui.MakeTextLine(nameImage)
			self.Children.append(nameImage)
			self.Children.append(nameSlot)

			## Level
			levelSlotImage = ui.MakeImageBox(self, "d:/ymir work/ui/public/parameter_slot_00.sub", 261, yPos)
			levelSlotImage.SetAlpha(0)
			levelSlot = ui.MakeTextLine(levelSlotImage)
			self.Children.append(levelSlotImage)
			self.Children.append(levelSlot)

			## Points (Rank Type)
			pointSlotImage = ui.MakeImageBox(self, "d:/ymir work/ui/public/parameter_slot_00.sub", 320, yPos)
			pointSlotImage.SetAlpha(0)
			pointSlot = ui.MakeTextLine(pointSlotImage)
			self.Children.append(pointSlotImage)
			self.Children.append(pointSlot)

			if i < self.MAX_LINE_COUNT:
				tempRankingSlotList = []
				tempRankingSlotList.append(rankingSlot)
				tempRankingSlotList.append(nameSlot)
				tempRankingSlotList.append(levelSlot)
				tempRankingSlotList.append(pointSlot)
				self.resultSlotList[i] = tempRankingSlotList
			else:
				self.myResultSlotList.append(rankingSlot)
				self.myResultSlotList.append(nameSlot)
				self.myResultSlotList.append(levelSlot)
				self.myResultSlotList.append(pointSlot)

		self.AllClear()
		if self.waitAniImg:
			self.waitAniImg.Show()

	def RefreshRankingBoard(self):
		self.AllClear()
		if self.waitAniImg:
			self.waitAniImg.Hide()

		myRank = 0
		myPoints = 0

		for line, slot in self.resultSlotList.items():
			nowIndex = line

			if nowIndex >= dungeonInfo.GetRankingCount():
				break

			(name, level, points) = dungeonInfo.GetRankingByLine(nowIndex)

			slot[self.SLOT_RANKING].SetText(str(nowIndex + 1))
			slot[self.SLOT_NAME].SetText(name)
			slot[self.SLOT_LEVEL].SetText(str(level))

			if self.type == RANK_SCORE:
				slot[self.SLOT_POINT].SetText(str(points))
			elif self.type == RANK_TIME:
				slot[self.SLOT_POINT].SetText(SecondToHMSGolbal(points))
			elif self.type == RANK_DAMAGE:
				slot[self.SLOT_POINT].SetText(NumberToDamageString(points))
			else:
				slot[self.SLOT_POINT].SetText(str(points))

			self.resultButtonList[line].Show()

			if player.GetName() == name:
				myPoints = points
				self.resultButtonList[line].Down()

		self.myResultSlotList[self.SLOT_NAME].SetText(player.GetName())
		self.myResultSlotList[self.SLOT_LEVEL].SetText(str(player.GetStatus(player.LEVEL)))

		myRank = dungeonInfo.GetMyRankingLine()
		if myRank:
			self.myResultSlotList[self.SLOT_RANKING].SetText(str(myRank))

			if myPoints:
				if self.type == RANK_SCORE:
					self.myResultSlotList[self.SLOT_POINT].SetText(str(myPoints))
				elif self.type == RANK_TIME:
					self.myResultSlotList[self.SLOT_POINT].SetText(SecondToHMSGolbal(myPoints))
				elif self.type == RANK_DAMAGE:
					self.myResultSlotList[self.SLOT_POINT].SetText(NumberToDamageString(myPoints))
				else:
					self.myResultSlotList[self.SLOT_POINT].SetText(myPoints)

	def AllClear(self):
		for line, resultSlotList in self.resultSlotList.items():
			resultSlotList[self.SLOT_RANKING].SetText("")
			resultSlotList[self.SLOT_NAME].SetText("")
			resultSlotList[self.SLOT_LEVEL].SetText("")
			resultSlotList[self.SLOT_POINT].SetText("")

			self.resultButtonList[line].SetUp()
			self.resultButtonList[line].Hide()

		self.myResultSlotList[self.SLOT_RANKING].SetText("-")
		self.myResultSlotList[self.SLOT_NAME].SetText("-")
		self.myResultSlotList[self.SLOT_LEVEL].SetText("-")
		self.myResultSlotList[self.SLOT_POINT].SetText("-")

	# On refresh board.
	def Refresh(self, type):
		self.type = type

		if self.waitAniImg:
			self.waitAniImg.Show()

		self.RefreshRankingBoard()

	# On open board.
	def Show(self):
		# Load board.
		self.__LoadWindow()

		# Show board.
		ui.ScriptWindow.Show(self)
		self.SetCenterPosition()
		self.SetTop()

	# On close board.
	def Close(self):
		# Clear dungeon ranking.
		dungeonInfo.ClearRanking()

		# Hide board.
		self.Hide()

	def OnPressEscapeKey(self):
		self.Close()
		return True

	def OnPressExitKey(self):
		self.Close()
		return True

class DungeonInfoWindow(ui.ScriptWindow):
	class ItemGrid(object):
		def __init__(self, width, height):
			self.grid = {}
			self.gridWidth = width
			self.gridHeight = height
			self.gridSize = width * height
			self.Clear()

		def __del__(self):
			self.grid = {}
			self.gridWidth = 0
			self.gridHeight = 0
			self.gridSize = 0

		def Clear(self):
			for pos in range(self.gridSize):
				self.grid[pos] = False

		def IsEmpty(self, pos, width, height):
			row = pos / self.gridWidth

			if row + height > self.gridHeight:
				return False

			if pos + width > row * self.gridWidth + self.gridWidth:
				return False

			for y in range(height):
				start = pos + (y * self.gridWidth)
				if self.grid[start] == True:
					return False

				x = 1
				while x < width:
					x =+ 1
					if self.grid[start + x] == True:
						return False

			return True

		def FindBlank(self, width, height):
			if width > self.gridWidth or height > self.gridHeight:
				return -1

			for row in range(self.gridHeight):
				for col in range(self.gridWidth):
					index = row * self.gridWidth + col
					if self.IsEmpty(index, width, height):
						return index

			return -1

		def Put(self, pos, width, height):
			if not self.IsEmpty(pos, width, height):
				return False

			for y in range(height):
				start = pos + (y * self.gridWidth)
				self.grid[start] = True

				x = 1
				while x < width:
					x += 1
					self.grid[start + x] = True

			return True

	DUNGEON_TYPE_DICT = {
		0 : localeInfo.DUNGEON_INFO_TYPE_01,
		1 : localeInfo.DUNGEON_INFO_TYPE_02,
		2 : localeInfo.DUNGEON_INFO_TYPE_03,
	}

	DUNGEON_ELEMENT_DICT = {
		0 : ROOT + "unkown-element.png", # RACE_UNKOWN
		1 : "d:/ymir work/ui/game/12zi/element/elect.sub", # RACE_FLAG_ATT_ELEC
		2 : "d:/ymir work/ui/game/12zi/element/fire.sub", # RACE_FLAG_ATT_FIRE
		3 : "d:/ymir work/ui/game/12zi/element/ice.sub", # RACE_FLAG_ATT_ICE
		4 : "d:/ymir work/ui/game/12zi/element/wind.sub", # RACE_FLAG_ATT_WIND
		5 : "d:/ymir work/ui/game/12zi/element/earth.sub", # RACE_FLAG_ATT_EARTH
		6 : "d:/ymir work/ui/game/12zi/element/dark.sub", # RACE_FLAG_ATT_DARK
	}

	DUNGEON_HELP_TOOLTIP_DICT = {
		1 : localeInfo.DUNGEON_INFO_TOOL_TIP_01,
		2 : localeInfo.DUNGEON_INFO_TOOL_TIP_02,
		3 : localeInfo.DUNGEON_INFO_TOOL_TIP_03,
		4 : localeInfo.DUNGEON_INFO_TOOL_TIP_04,
		5 : "[ENTER]",
		6 : localeInfo.DUNGEON_INFO_TOOL_TIP_05,
		7 : localeInfo.DUNGEON_INFO_TOOL_TIP_06,
		8 : localeInfo.DUNGEON_INFO_TOOL_TIP_07,
		9 : localeInfo.DUNGEON_INFO_TOOL_TIP_08,
	}

	DUNGEON_STATUS_DICT = {
		DUNGEON_CLOSED : [
			localeInfo.DUNGEON_INFO_STATUS_CLOSED,
			((255.00 / 255), (0.00 / 255), (0.00 / 255))
		],
		DUNGEON_AVAILABLE : [
			localeInfo.DUNGEON_INFO_STATUS_AVAILABLE,
			((0.00 / 255), (255.00 / 255), (0.00 / 255))
		],
		DUNGEON_COOLDOWN : [
			localeInfo.DUNGEON_INFO_STATUS_COOLDOWN,
			((255.00 / 255), (0.00 / 255), (0.00 / 255))
		],
		DUNGEON_LEVEL_HIGH : [
			localeInfo.DUNGEON_INFO_STATUS_HIGH_LEVEL,
			((255.00 / 255), (0.00 / 255), (0.00 / 255))
		],
		DUNGEON_LEVEL_LOW : [
			localeInfo.DUNGEON_INFO_STATUS_LOW_LEVEL,
			((255.00 / 255), (0.00 / 255), (0.00 / 255))
		],
	}

	DUNGEON_BOX_MAX_SLOTS = 60

	def __init__(self):
		ui.ScriptWindow.__init__(self)

		self.toolTip = uiToolTip.ToolTip()
		self.toolTipItem = uiToolTip.ItemToolTip()
		self.toolTipHelp = None

		self.boxGrid = self.ItemGrid(6, 10)

		self.rankWnd = DungeonRankingWindow()

		self.Init()

	def __del__(self):
		ui.ScriptWindow.__del__(self)

		self.toolTip = None
		self.toolTipItem = None
		self.toolTipHelp = None

		self.boxGrid = {}

		self.rankWnd = None

		self.Init()

	def Destroy(self):
		if self.rankWnd:
			self.rankWnd.Destroy()
			self.rankWnd = None

		self.Init()

	def Init(self):
		self.isLoaded = False

		self.tabIndex = -1
		self.listIndex = -1
		self.toggleButtonObjList = []

		self.reqItemVnumList = [0 for slot in range(0, 3)]
		self.reqItemCountList = [0 for slot in range(0, 3)]
		self.scrollPos = 0

		self.showBoxWnd = False
		self.boxItems = {}

		self.questionDialog = None
		self.rankType = 0

		self.chestdropinfo = None

		self.popUp = None

	def __LoadWindow(self):
		try:
			pyScrLoader = ui.PythonScriptLoader()
			pyScrLoader.LoadScriptFile(self, "UIScript/DungeonInfoWindow.py")
		except:
			import exception
			exception.Abort("DungeonInfoWindow.__LoadWindow")

		try:
			self.__BindObject()
		except:
			import exception
			exception.Abort("DungeonInfoWindow.__BindObject")

		try:
			self.__BindEvent()
		except:
			import exception
			exception.Abort("DungeonInfoWindow.__BindEvent")

		self.isLoaded = True

	def Initialize(self):
		self.toggleButtonObjList = []

		if dungeonInfo.GetCount() > 0:
			self.OnLockButtons(False)
		else:
			self.OnLockButtons(True)

			popUp = uiCommon.PopupDialog()
			popUp.SetText(localeInfo.DUNGEON_INFO_NOT_FOUND)
			popUp.SetAcceptEvent(self.Close)
			popUp.Open()
			self.popUp = popUp

		for key in xrange(min(MAX_DUNGEON_SCROLL, dungeonInfo.GetCount())):
			toggleButton = ListToggleButton(self.buttonListThinBoard, key)

			toggleButton.SetKey(key)
			toggleButton.SetText(dungeonInfo.GetMapIndex(key))
			toggleButton.SetIcon(dungeonInfo.GetMapIndex(key))
			# if dungeonInfo.GetCooldown(key) > 0:
			# 	toggleButton.SetClock(True, dungeonInfo.GetCooldown(key), True)
			toggleButton.SetEvent(ui.__mem_func__(self.__OnClickListButton), key)

			self.toggleButtonObjList.append(toggleButton)

		if self.toggleButtonObjList:
			self.__OnClickListButton(0)

		if dungeonInfo.GetCount() > MAX_DUNGEON_SCROLL:
			self.scrollBar.SetMiddleBarSize(float(MAX_DUNGEON_SCROLL) / float(dungeonInfo.GetCount()))
			self.scrollBar.SetScrollEvent(self.OnScroll)
			self.scrollBar.Show()
		else:
			self.scrollBar.Hide()

	"""
	# Server Reply
	"""

	def OnRefreshRanking(self):
		if self.rankType != 0:
			if self.rankWnd:
				self.rankWnd.Refresh(self.rankType)

	def OnOpen(self):
		# Check if buttons aren't already set.
		if not self.toggleButtonObjList:
			# Clear the entire list and reload all data.
			self.Initialize()

	def OnReload(self, onReset):
		# NOTE : Reserved function.
		# In order to keep this function safe we should not reset the entire list, instead we
		# must create a newer function when "/reload dungeon" is called.
		#
		# This function is reserved only for updateing "texts, images and models"
		#

		if onReset:
			self.Initialize()

		if not self.toggleButtonObjList:
			return

		try:
			key = self.listIndex
			self.__OnLoadInformation(key) # < key, reloadRender? >
		except IndexError:
			return

	def OnScroll(self):
		if not self.toggleButtonObjList:
			return

		count = dungeonInfo.GetCount()
		scrollLineCount = max(0, count - MAX_DUNGEON_SCROLL)
		self.scrollPos = int(scrollLineCount * self.scrollBar.GetPos())

		for key in xrange(min(MAX_DUNGEON_SCROLL, dungeonInfo.GetCount())):
			scrollKey = key + self.scrollPos

			try:
				toggleButton = self.toggleButtonObjList[key]

				toggleButton.SetKey(scrollKey)
				toggleButton.SetText(dungeonInfo.GetMapIndex(scrollKey))
				toggleButton.SetIcon(dungeonInfo.GetMapIndex(scrollKey))
				toggleButton.HideClock()
				toggleButton.SetEvent(ui.__mem_func__(self.__OnClickListButton), key)

			except KeyError:
				return

		self.__OnLoadInformation(self.listIndex)

	def GetRealKey(self):
		key = self.listIndex
		try:
			toggleBtn = self.toggleButtonObjList[self.listIndex]
			key = toggleBtn.GetKey()
		except KeyError:
			return key
		return key

	def __OnClickListButton(self, key):
		try:
			toggleBtn = self.toggleButtonObjList[key].GetObject()
		except IndexError:
			return

		for eachToggleBtn in self.toggleButtonObjList:
			eachToggleBtn.GetObject().SetUp()

		realKey = key

		toggleBtn.Down()

		#try:
		#	realKey = self.toggleButtonObjList[key].GetKey()
		#except IndexError:
		#	return

		self.listIndex = realKey

		self.__OnLoadInformation(realKey)

	def OnUpdate(self):
		for eachToggleBtn in self.toggleButtonObjList:
			eachToggleBtn.OnUpdate()

		pos_x, pos_y = wndMgr.GetMousePosition()
		self.toolTip.SetToolTipPosition(pos_x,pos_y+85)

		self.CheckDungeonStatus()

	def CheckDungeonStatus(self):
		for key in xrange(len(self.toggleButtonObjList)):
			try:
				toggleBtn = self.toggleButtonObjList[key]
				realKey = toggleBtn.GetKey()

				if dungeonInfo.GetType(realKey) == -1:
					toggleBtn.SetStatus(self.DUNGEON_STATUS_DICT[DUNGEON_CLOSED])

				elif _PlayerLevel() < dungeonInfo.GetLevelLimit(realKey, 0):
					toggleBtn.SetStatus(self.DUNGEON_STATUS_DICT[DUNGEON_LEVEL_HIGH])

				elif _PlayerLevel() > dungeonInfo.GetLevelLimit(realKey, 1):
					toggleBtn.SetStatus(self.DUNGEON_STATUS_DICT[DUNGEON_LEVEL_LOW])

				else:
					if dungeonInfo.GetCooldown(realKey) > 0:
						toggleBtn.SetStatus(self.DUNGEON_STATUS_DICT[DUNGEON_COOLDOWN])
						toggleBtn.SetClock(True, dungeonInfo.GetCooldown(realKey), True)
					else:
						toggleBtn.SetStatus(self.DUNGEON_STATUS_DICT[DUNGEON_AVAILABLE])
						toggleBtn.HideClock()

			except KeyError:
				return

	def __OnLoadInformation(self, key):
		try:
			toggleBtn = self.toggleButtonObjList[key]
			key = toggleBtn.GetKey()

			self.titleNameText.SetText("%s" % localeInfo.GetMiniMapZoneNameByIdx(dungeonInfo.GetMapIndex(key)))
			self.typeText.SetText(localeInfo.DUNGEON_INFO_TYPE % self.DUNGEON_TYPE_DICT[dungeonInfo.GetType(key)])

			minLevel = dungeonInfo.GetLevelLimit(key, 0)
			maxLevel = dungeonInfo.GetLevelLimit(key, 1)
			self.levelLimitText.SetText(localeInfo.DUNGEON_INFO_LEVEL_LIMIT % (minLevel, maxLevel))

			if _PlayerLevel() < minLevel or _PlayerLevel() > maxLevel:
				self.warpButton.Disable()
				self.warpButton.Down()
			else:
				self.warpButton.Enable()
				self.warpButton.SetUp()

			self.memberLimitText.SetText(localeInfo.DUNGEON_INFO_PARTY_LIMIT % (dungeonInfo.GetMemberLimit(key, 0), dungeonInfo.GetMemberLimit(key, 1)))

			# if dungeonInfo.GetDuration(key) > 0:
			# 	self.durationText.SetText(localeInfo.DUNGEON_INFO_DURATION % SecondToHMSGolbal(dungeonInfo.GetDuration(key)))
			# else:
			# 	self.durationText.SetText(localeInfo.DUNGEON_INFO_DURATION % localeInfo.DUNGEON_INFO_NONE)

			# if dungeonInfo.GetCooldown(key) > 0:
			# 	self.cooldownText.SetText(localeInfo.DUNGEON_INFO_COOLDOWN % SecondToHMSGolbal(dungeonInfo.GetCooldown(key)))
			# else:
			# 	self.cooldownText.SetText(localeInfo.DUNGEON_INFO_COOLDOWN % localeInfo.DUNGEON_INFO_NONE)

			self.locationText.SetText(localeInfo.DUNGEON_INFO_LOCATION % localeInfo.GetMiniMapZoneNameByIdx(dungeonInfo.GetMapIndex(key)))
			self.entranceText.SetText(localeInfo.DUNGEON_INFO_ENTRACE % localeInfo.GetMiniMapZoneNameByIdx(dungeonInfo.GetEntryMapIndex(key)))
			# self.elementalImg.LoadImage(self.DUNGEON_ELEMENT_DICT[dungeonInfo.GetElement(key)])

			self.totalFinishedText.SetText(localeInfo.DUNGEON_INFO_TOTAL_FINISHED % dungeonInfo.GetFinish(key))
			self.fastestTimeText.SetText(localeInfo.DUNGEON_INFO_FASTEST_TIME % SecondToHMSGolbal(dungeonInfo.GetFinishTime(key)))
			self.highestDamageText.SetText(localeInfo.DUNGEON_INFO_HIGHEST_DMG % NumberToDamageString(dungeonInfo.GetFinishDamage(key)))

			# Required Item Slot
			for slotPos in xrange(self.reqItemSlot.GetSlotCount()):
				itemVnum = dungeonInfo.GetRequiredItemVnum(key, slotPos)
				itemCount = dungeonInfo.GetRequiredItemCount(key, slotPos)
				itemPercentage = dungeonInfo.GetBossDropItemPercentage(key, slotPos)

				if itemVnum != 0:
					self.reqItemVnumList[slotPos] = itemVnum
					self.reqItemCountList[slotPos] = itemVnum

					item.SelectItem(itemVnum)
					itemIcon = item.GetIconImage()
					(width, height) = item.GetItemSize()
					self.reqItemSlot.SetSlot(slotPos, 0, width, height, itemIcon, (1.0, 1.0, 1.0, 0.5))
					self.reqItemSlot.SetSlotCount(slotPos, itemCount)
					self.reqItemSlot.HideSlotBaseImage(slotPos)
				else:
					self.reqItemVnumList[slotPos] = 0
					self.reqItemCountList[slotPos] = 0

					self.reqItemSlot.ClearSlot(slotPos)
					self.reqItemSlot.ShowSlotBaseImage(slotPos)

			self.reqItemSlot.RefreshSlot()

			## Box Inventory
			if self.boxWnd and self.showBoxWnd:
				self.RefreshBoxInventory()

		except IndexError:
			return

	# Bind objects.
	def __BindObject(self):
		Child = self.GetChild
		self.board = Child("Board")
		self.titleBar = Child("TitleBar")

		# self.titleBackgroundImg = Child("TitleBackgroundImage")
		self.titleNameText = Child("TitleNameText")
		self.buttonListThinBoard = Child("ButtonListThinBoard")
		#self.listButton = Child("ListButton")

		self.infoWnd = Child("InformationWindow")
		self.rankScoreButton = Child("RankScoreButton")
		self.rankTimeButton = Child("RankTimeButton")
		self.rankDamageButton = Child("RankDamageButton")
		self.reqItemSlot = Child("RequiredItemSlot")
		self.DungeonInfoItem = Child("DungeonInfoItem")

		self.infoThinBoard = Child("InformationThinBoard")
		self.typeText = Child("TypeText")
		self.levelLimitText = Child("LevelLimitText")
		self.memberLimitText = Child("MemberLimitText")
		# self.durationText = Child("DurationText")
		# self.cooldownText = Child("CooldownText")
		self.locationText = Child("LocationText")
		self.entranceText = Child("EntraceText")
		self.elementalImg = Child("ElementalImage")
		self.warpButton = Child("WarpButton")
		self.boxButton = Child("BoxButton")
		self.boxWnd = Child("BoxWindow")
		self.boxWnd.Hide()
		self.boxBackgroundImg = Child("BoxBackgroundImg")
		self.boxItemSlot = Child("BoxItemSlot")

		self.totalFinishedText = Child("TotalFinishedText")
		self.fastestTimeText = Child("FastestTimeText")
		self.highestDamageText = Child("HighestDamageText")

	# Bind events to objects.
	def __BindEvent(self):
		# Board titlebar.
		if self.titleBar:
			self.titleBar.SetCloseEvent(ui.__mem_func__(self.Close))

		# Required item slots.
		if self.reqItemSlot:
			self.reqItemSlot.SetOverInItemEvent(ui.__mem_func__(self.__SlotOverInItem))
			self.reqItemSlot.SetOverOutItemEvent(ui.__mem_func__(self.__SlotOverOutItem))

		# Element (bonus) image.
		if self.elementalImg:
			self.elementalImg.OnMouseOverIn = lambda : ui.__mem_func__(self.__OnOverElementImg)()
			self.elementalImg.OnMouseOverOut = lambda : ui.__mem_func__(self.__OnOverOutElementImg)()

		# Rank Buttons.
		if self.rankScoreButton:
			self.rankScoreButton.SetEvent(lambda arg = RANK_SCORE : ui.__mem_func__(self.__OnClickRankingButton)(arg))

		if self.rankTimeButton:
			self.rankTimeButton.SetEvent(lambda arg = RANK_TIME : ui.__mem_func__(self.__OnClickRankingButton)(arg))

		if self.rankDamageButton:
			self.rankDamageButton.SetEvent(lambda arg = RANK_DAMAGE : ui.__mem_func__(self.__OnClickRankingButton)(arg))

		# Warp button.
		if self.warpButton:
			self.warpButton.SetEvent(ui.__mem_func__(self.__OnWarpButton))

		# Box icon button.
		if self.boxButton:
			self.boxButton.SetToggleUpEvent(ui.__mem_func__(self.__OnClickBoxButton))
			self.boxButton.SetToggleDownEvent(ui.__mem_func__(self.__OnClickBoxButton))

		# Box item slot.
		if self.boxItemSlot:
			self.boxItemSlot.SetOverInItemEvent(ui.__mem_func__(self.__OnOverInBoxItem))
			self.boxItemSlot.SetOverOutItemEvent(ui.__mem_func__(self.__OnOverOutBoxItem))
			self.boxItemSlot.SAFE_SetButtonEvent("RIGHT", "EXIST", self.UseSlotEvent)

		self.scrollBar = ScrollBar()
		self.scrollBar.SetParent(self)
		self.scrollBar.SetScrollBarSize(395)
		self.scrollBar.SetPosition(255, 57)
		self.scrollBar.EnableSmoothMode()
		self.scrollBar.Hide()

	def OnLockButtons(self, lock):
		if lock:
			if self.warpButton:
				self.warpButton.Disable()
				self.warpButton.Down()

			if self.rankScoreButton:
				self.rankScoreButton.Disable()
				self.rankScoreButton.Down()

			if self.rankTimeButton:
				self.rankTimeButton.Disable()
				self.rankTimeButton.Down()

			if self.rankDamageButton:
				self.rankDamageButton.Disable()
				self.rankDamageButton.Down()
		else:
			if self.warpButton:
				self.warpButton.Enable()
				self.warpButton.SetUp()

			if self.rankScoreButton:
				self.rankScoreButton.Enable()
				self.rankScoreButton.SetUp()

			if self.rankTimeButton:
				self.rankTimeButton.Enable()
				self.rankTimeButton.SetUp()

			if self.rankDamageButton:
				self.rankDamageButton.Enable()
				self.rankDamageButton.SetUp()

	# On rank button
	def __OnClickRankingButton(self, type):
		if type > 0 and self.listIndex != -1:
			self.rankType = type

			if not self.rankWnd:
				self.rankWnd = DungeonRankingWindow()

			if not self.rankWnd.IsShow():
				self.rankWnd.Show()
				dungeonInfo.Ranking(self.GetRealKey(), self.rankType)

	# On warp button.
	def __OnWarpButton(self):
		key = self.GetRealKey()

		# Get map name.
		mapName = localeInfo.GetMiniMapZoneNameByIdx(dungeonInfo.GetMapIndex(key))

		# Get <min, max> level limit.
		minLevel= dungeonInfo.GetLevelLimit(key, 0)
		maxLevel = dungeonInfo.GetLevelLimit(key, 1)

		# Check player level.
		if _PlayerLevel() < minLevel:
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.DUNGEON_INFO_STATUS_HIGH_LEVEL)
			return

		if _PlayerLevel() > maxLevel:
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.DUNGEON_INFO_STATUS_LOW_LEVEL)
			return

		# Create teleport question dialog.
		self.questionDialog = TeleportQuestionDialog()
		self.questionDialog.Open(localeInfo.DUNGEON_INFO_DO_YOU_TELEPORT % mapName, 10)
		self.questionDialog.SetAcceptText(localeInfo.UI_ACCEPT)
		self.questionDialog.SetCancelText(localeInfo.UI_DENY)
		self.questionDialog.SetAcceptEvent(lambda arg = True : ui.__mem_func__(self.OnAnswerTeleport)(arg))
		self.questionDialog.SetCancelEvent(lambda arg = False : ui.__mem_func__(self.OnAnswerTeleport)(arg))
		self.questionDialog.key = key
		self.questionDialog.SetCancelOnTimeOver()
		self.questionDialog.SetTop()

	# On answer teleport question dialog.
	def OnAnswerTeleport(self, answer):
		# Check if dialog exists.
		if not self.questionDialog:
			return

		# Check answer.
		if answer == True:
			dungeonInfo.Warp(self.questionDialog.key)

		# Close dialog.
		self.questionDialog.Close()
		self.questionDialog = None

	# Mouse in event of box item.
	def __OnOverInBoxItem(self, slotIndex):
		# Show box item tooltip.
		self.__ShowBoxItemToolTip(slotIndex)

	# Mouse out event of box item.
	def __OnOverOutBoxItem(self):
		# Hide tooltip.
		if self.toolTipItem:
			self.toolTipItem.HideToolTip()

	def UseSlotEvent(self, slotIndex):
		item.SelectItem(self.boxItems[slotIndex][0])
		if item.GetItemType() == item.ITEM_TYPE_GIFTBOX or item.GetItemType() == ITEM_TYPE_GACHA:
			if app.IsPressed(app.DIK_LCONTROL):
				isMain = not app.IsPressed(app.DIK_LSHIFT)
				if _HasDropInfo(self.boxItems[slotIndex][0], isMain):
					self.chestdropinfo.Open(self.boxItems[slotIndex][0], isMain)
				return

	# Show box item tooltip.
	def __ShowBoxItemToolTip(self, slotIndex):
		# Check if object exists.
		if self.toolTipItem:
			# Clear tooltip data.
			self.toolTipItem.ClearToolTip()

			# Set metin slot.
			metinSlot = []
			for i in xrange(player.METIN_SOCKET_MAX_NUM):
				metinSlot.append(0)

			# Set attr. slot.
			attrSlot = []
			for i in xrange(player.ATTRIBUTE_SLOT_MAX_NUM):
				attrSlot.append((0, 0))

			# Set tooltip item data.
			self.toolTipItem.AddItemData(self.boxItems[slotIndex][0], metinSlot, attrSlot)

			self.toolTipItem.AppendSpace(5)
			self.toolTipItem.AppendTextLine("|cFFc9f1c0Szansa na drop: "+str(self.boxItems[slotIndex][2])+"%")
			# self.toolTipItem.AppendSpace(5)

			item.SelectItem(self.boxItems[slotIndex][0])
			# if item.GetItemType() == item.ITEM_TYPE_GIFTBOX or item.GetItemType() == ITEM_TYPE_GACHA:
			if item.GetItemType() == ITEM_TYPE_GACHA:
				self.toolTipItem.AppendSpace(6)
				self.toolTipItem.AppendHorizontalLine()
				self.toolTipItem.AppendSpace(6)
				self.toolTipItem.AppendSpace(8)
				self.toolTipItem.AppendSpace(5)
				

			if self.boxItems[slotIndex][0] == 23000:
				self.toolTipItem.AppendSpace(5)
				self.toolTipItem.AppendHorizontalLine()
				self.toolTipItem.AppendSpace(5)
				self.toolTipItem.AppendTextLine("|cFFecffb7Mo¿liwe wbudowane bonusy (1 - 3):")
				self.toolTipItem.AppendSpace(3)
				for i in xrange(len(TABLICA_BONUSOW_REKAWICA)):
					self.toolTipItem.AppendSpace(2)
					self.toolTipItem.AppendGreenTextLine(TABLICA_BONUSOW_REKAWICA[i][1](TABLICA_BONUSOW_REKAWICA[i][0]))

	# On running mouse wheel.
	def OnRunMouseWheel(self, nLen):
		if nLen > 0:
			# Set scroll step up.
			if self.scrollBar:
				self.scrollBar.OnUp()
		else:
			# Set scroll step down.
			if self.scrollBar:
				self.scrollBar.OnDown()

	# Hide info window.
	def HideInfoWindow(self):
		# Check if objects exist.
		# if self.infoThinBoard and self.elementalImg:
		if self.infoThinBoard:
			# Hide info thinboard and info objects.
			self.infoThinBoard.Hide()
			self.elementalImg.Hide()

		# Hide warp button.
		if self.warpButton:
			self.warpButton.Hide()

	# Show info window.
	def ShowInfoWindow(self):
		# Check if objects exist.
		# if self.infoThinBoard and self.elementalImg:
		if self.infoThinBoard:
			# Show info thinboard and info objects.
			self.infoThinBoard.Show()
			self.elementalImg.Show()

		# Show warp button.
		if self.warpButton:
			self.warpButton.Show()

	# On click box icon button.
	def __OnClickBoxButton(self, refresh = True):
		# Show box window if isn't showing already.
		if not self.showBoxWnd:
			self.OnShowBoxWindow()

			if refresh:
				# Hide info window and refresh box inventory.
				# self.HideInfoWindow()
				self.RefreshBoxInventory()

		else:
			self.OnHideBoxWindow()

			if refresh:
				# Show info window.
				self.ShowInfoWindow()

	def OnShowBoxWindow(self):
		self.showBoxWnd = True

		# Show box window and set box icon button down.
		self.warpButton.Hide()
		self.DungeonInfoItem.Hide()
		self.boxWnd.Show()
		self.boxButton.Down()

	def OnHideBoxWindow(self):
		self.showBoxWnd = False

		# Hide box window and set box icon button up.
		self.warpButton.Show()
		self.DungeonInfoItem.Show()
		self.boxButton.SetUp()
		self.boxWnd.Hide()

	# Clear boss box inventory.
	def ClearBoxInventory(self):
		# Clear all slots from box inventory.
		for slotPos in xrange(self.boxItemSlot.GetSlotCount()):
			self.boxItemSlot.ClearSlot(slotPos)
			self.boxItemSlot.HideSlotBaseImage(slotPos)
			self.boxItemSlot.EnableCoverButton(slotPos)

		# Clear box grid.
		if self.boxGrid:
			self.boxGrid.Clear()

		# Clear box item dictionary.
		if self.boxItems:
			self.boxItems = {}

	# Get box grid global slot position.
	# def __GetBoxGridGlobalSlotPos(self, slotPos):
		# return self.boxScrollPos + slotPos

	 # Refresh boss box inventory.
	# Refresh boss box inventory.
	def RefreshBoxInventory(self):
		# Clear box inventory always on refresh.
		self.ClearBoxInventory()

		key = self.GetRealKey()

		# Get boss drop (item) count.
		bossDropCount = dungeonInfo.GetBossDropCount(key)

		# Check if there are drops.
		if bossDropCount != 0:
			for slotPos in xrange(self.boxItemSlot.GetSlotCount()):
				#try:
				globalPos = slotPos #self.__GetBoxGridGlobalSlotPos(slotPos)

				# Get vnum and count of item.
				itemVnum = dungeonInfo.GetBossDropItemVnum(key, globalPos)
				itemCount = dungeonInfo.GetBossDropItemCount(key, globalPos)
				itemPercentage = dungeonInfo.GetBossDropItemPercentage(key, globalPos)
				# if itemCount <= 1:
					# itemCount = 0

				if itemVnum <= 0:
					continue


				#except IndexError:
				#    return

				# Get item icon and size.
				item.SelectItem(itemVnum)
				itemIcon = item.GetIconImage()
				(width, height) = item.GetItemSize()

				pos = self.boxGrid.FindBlank(width, height)
				if pos == -1:
					break

				self.boxGrid.Put(pos, width, height)
				self.boxItems.update({ pos : [ itemVnum, itemCount, itemPercentage] })
				self.boxItemSlot.SetItemSlot(pos, itemVnum, itemCount)
				self.boxItemSlot.HideSlotBaseImage(pos)

			self.boxItemSlot.RefreshSlot()

	# Mouse in event of element (bonus) image.
	def __OnOverElementImg(self):
		# Clean tooltip data.
		self.toolTip.ClearToolTip()

		key = self.GetRealKey()

		# Get att & def bonus count.
		attBonusCount = dungeonInfo.GetAttBonusCount(key)
		defBonusCount = dungeonInfo.GetDefBonusCount(key)

		# Check if there are att. or def. bonuses.
		if attBonusCount != 0 or defBonusCount != 0:
			hasAttBonus = False # Has att. bonus?

			if attBonusCount != 0:
				self.toolTip.SetTitle(localeInfo.DUNGEON_INFO_ELEMENT_ATK_BONUS)

				for index in xrange(attBonusCount):
					# Get affect type of att. bonus.
					affectType = dungeonInfo.GetAttBonus(key, index)
					if affectType != 0 and affectType < 255:
						try:
							desc = AFFECT_DATA[affectType]
						except KeyError:
							desc = "UNKNOWN_KEY[%d]" % affectType

						hasAttBonus = True # Set att. bonus.

						if affectType == 48:
							self.toolTip.AppendTextLine("Silny przeciwko Diab³om (+"+str(player.GetStatus(affectType))+")", grp.GenerateColor(0.5411, 0.7254, 0.5568, 1.0))
						if affectType == 45:
							self.toolTip.AppendTextLine("Silny przeciwko Orkom (+"+str(player.GetStatus(affectType))+"%)", grp.GenerateColor(0.5411, 0.7254, 0.5568, 1.0))
						self.toolTip.AppendSpace(1)

			# Check if there att. or def. bonuses.
			if defBonusCount != 0:
				if hasAttBonus:
					# If there was previously an att. bonus show a horizontal line for def. bonus.
					self.toolTip.AppendHorizontalLine()

				self.toolTip.SetTitle(localeInfo.DUNGEON_INFO_ELEMENT_DEF_AND_RES_BONUS)

				for index in range(0, defBonusCount):
					# Get affect type of def. bonus.
					affectType = dungeonInfo.GetDefBonus(key, index + 1)
					if affectType != 0 and affectType < 255:
						try:
							desc = AFFECT_DATA[affectType]
						except KeyError:
							desc = "UNKNOWN_KEY[%d]" % affectType

						# Show def. bonus text.
						self.toolTip.AppendTextLine(desc, grp.GenerateColor(0.5411, 0.7254, 0.5568, 1.0))
						self.toolTip.AppendSpace(1)
		else:
			# No bonus, show no bonus information.
			self.toolTip.AppendTextLine(localeInfo.DUNGEON_INFO_ELEMENT_UNKOWN_BONUS)
			self.toolTip.AppendSpace(1)

		getattr(self.toolTip, "TextAlignHorizonalCenter", lambda : None)()
		self.toolTip.SetTop()
		self.toolTip.Show()

		#if self.infoThinBoard:
		#	self.infoThinBoard.Hide()

	# Mouse out event of element (bonus) image.
	def __OnOverOutElementImg(self):
		# Hide tooltip.
		if self.toolTip:
			self.toolTip.Hide()

		#if self.infoThinBoard:
		#	self.infoThinBoard.Show()

	# Hide all first tab objects.
	def HideAllFirstTabObjects(self):
		# Hide required item grid.
		if self.reqItemBackgroundImg:
			self.reqItemBackgroundImg.Hide()

		# Hide box icon button.
		if self.boxButton:
			self.boxButton.Hide()

		# Hide entire info thinboard.
		if self.infoThinBoard:
			self.infoThinBoard.Hide()

		# Hide element (bonus) image.
		if self.elementalImg:
			self.elementalImg.Hide()

		# Hide warp button.
		if self.warpButton:
			self.warpButton.Hide()

	# Show all first tab objects.
	def ShowAllFirstTabObjects(self):
		# Show required item grid.
		if self.reqItemBackgroundImg:
			self.reqItemBackgroundImg.Show()

		# Show box icon button.
		if self.boxButton:
			self.boxButton.Show()

		# Show entire info thinboard.
		if self.infoThinBoard:
			self.infoThinBoard.Show()

		# Show element (bonus) image.
		if self.elementalImg:
			self.elementalImg.Show()

		# Show warp button.
		if self.warpButton:
			self.warpButton.Show()

	# Create game type info tooltip.
	def __CreateGameTypeToolTip(self, title, descList):
		toolTip = uiToolTip.ToolTip()
		toolTip.SetTitle(title)

		for desc in descList.itervalues():
			if desc == "[ENTER]":
				# Show horizontal line if token is found in desc.
				toolTip.AppendHorizontalLine()
			else:
				toolTip.AppendTextLine(desc)
				toolTip.AppendSpace(1)

		getattr(toolTip, "TextAlignHorizonalCenter", lambda : None)()
		toolTip.SetTop()
		return toolTip

	# Mouse in event of required item.
	def __SlotOverInItem(self, slotIndex):
		# Check if object exists.
		if self.toolTipItem:
			# Show tooltip.
			if self.reqItemVnumList and self.reqItemVnumList[slotIndex] > 0:
				# Set item tooltip.
				self.toolTipItem.SetItemToolTip(self.reqItemVnumList[slotIndex])

	# Mouse out event of required item.
	def __SlotOverOutItem(self):
		# Check if object exists.
		if self.toolTipItem:
			# Hide tooltip.
			self.toolTipItem.HideToolTip()

	# Set item tooltip.
	def SetItemToolTip(self, toolTip):
		self.toolTipItem = toolTip

	# Key escape.
	def OnPressEscapeKey(self):
		# Close board.
		self.Close()
		return True

	# Key exit.
	def OnPressExitKey(self):
		# Close board.
		self.Close()
		return True

	# Close board.
	def Close(self):
		# Close dungeon info (data).
		dungeonInfo.Close()

		# Hide ranking board.
		if self.rankWnd:
			self.rankWnd.Hide()

		# Hide board.
		self.Hide()

	def Open(self):
		# Check if window isn't loaded already.
		if not self.isLoaded:
			# Load window.
			self.__LoadWindow()

		# Open dungeon info (data).
		dungeonInfo.Open()

		# Set board position @ center and on top.
		self.SetCenterPosition()
		self.SetTop()

		# Show board.
		ui.ScriptWindow.Show(self)


# MT2009_PLUS_DUNGEON_PANEL_V1: one window per client, as uigoblin.py.
_window = [None]

def GetWindow():
	if not _window[0]:
		_window[0] = DungeonInfoWindow()
	return _window[0]

def ToggleWindow():
	wnd = GetWindow()
	if wnd.IsShow():
		wnd.Close()
	else:
		wnd.Open()

def DestroyWindow():
	wnd = _window[0]
	if wnd:
		try:
			wnd.Hide()
			wnd.Destroy()
		except Exception:
			pass
	_window[0] = None
