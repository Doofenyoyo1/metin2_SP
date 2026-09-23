import app

import eventManager
import ui
import player
import net
import wndMgr
import messenger
import guild
import chr
import nonplayer
import localeInfo
import constInfo
import uiAffectBar
import uiReport
from _weakref import proxy

if app.ENABLE_ELEMENTAL_TARGET:
	ELEMENTAL_PATH = "d:/ymir work/ui/game/12zi/element/{}.sub"

	SUB_RACE_FLAG_TO_FILENAME = {
		nonplayer.RACE_FLAG_ATT_ELEC:	ELEMENTAL_PATH.format("elect"),
		nonplayer.RACE_FLAG_ATT_FIRE:	ELEMENTAL_PATH.format("fire"),
		nonplayer.RACE_FLAG_ATT_ICE:	ELEMENTAL_PATH.format("ice"),
		nonplayer.RACE_FLAG_ATT_WIND:	ELEMENTAL_PATH.format("wind"),
		nonplayer.RACE_FLAG_ATT_EARTH:	ELEMENTAL_PATH.format("earth"),
		nonplayer.RACE_FLAG_ATT_DARK:	ELEMENTAL_PATH.format("dark"),
	}

	def GetElementalFilename(mobVnum):
		if not mobVnum:
			return ""

		dwRaceFlag = nonplayer.GetMonsterRaceFlag(mobVnum)
		for k,v in SUB_RACE_FLAG_TO_FILENAME.iteritems():
			curFlag = k
			if HAS_FLAG(dwRaceFlag, curFlag):
				return v
		return ""

	def HAS_FLAG(value, flag):
		return (value & flag) == flag

class TargetBoard(ui.ThinBoard):

	BUTTON_NAME_LIST = (
		localeInfo.TARGET_BUTTON_WHISPER,
		localeInfo.TARGET_BUTTON_EXCHANGE,
		localeInfo.TARGET_BUTTON_FIGHT,
		localeInfo.TARGET_BUTTON_ACCEPT_FIGHT,
		localeInfo.TARGET_BUTTON_AVENGE,
		localeInfo.TARGET_BUTTON_FRIEND,
		localeInfo.TARGET_BUTTON_INVITE_PARTY,
		localeInfo.TARGET_BUTTON_LEAVE_PARTY,
		localeInfo.TARGET_BUTTON_EXCLUDE,
		localeInfo.TARGET_BUTTON_INVITE_GUILD,
		localeInfo.TARGET_BUTTON_DISMOUNT,
		localeInfo.TARGET_BUTTON_EXIT_OBSERVER,
		localeInfo.TARGET_BUTTON_VIEW_EQUIPMENT,
		localeInfo.TARGET_BUTTON_REQUEST_ENTER_PARTY,
		localeInfo.TARGET_BUTTON_BUILDING_DESTROY,
		localeInfo.TARGET_BUTTON_EMOTION_ALLOW,
		"VOTE_BLOCK_CHAT",
		localeInfo.TARGET_BUTTON_REPORT,
		"Sprawdz",
		"EQ",
	)

	GRADE_NAME =	{
						nonplayer.PAWN : localeInfo.TARGET_LEVEL_PAWN,
						nonplayer.S_PAWN : localeInfo.TARGET_LEVEL_S_PAWN,
						nonplayer.KNIGHT : localeInfo.TARGET_LEVEL_KNIGHT,
						nonplayer.S_KNIGHT : localeInfo.TARGET_LEVEL_S_KNIGHT,
						nonplayer.BOSS : localeInfo.TARGET_LEVEL_BOSS,
						nonplayer.KING : localeInfo.TARGET_LEVEL_KING,
					}
	EXCHANGE_LIMIT_RANGE = 3000

	def __init__(self):
		ui.ThinBoard.__init__(self)

		affectBar = ui.Window()
		affectBar.SetSize(0, 0)
		affectBar.Hide()
		self.affectBar = affectBar

		name = ui.TextLine()
		name.SetParent(self)
		name.SetDefaultFontName()
		name.SetOutline()
		name.Show()

		hpGauge = ui.Gauge()
		hpGauge.SetParent(self)
		hpGauge.MakeGauge(130, "red")
		hpGauge.Hide()

		if app.ENABLE_ELEMENTAL_TARGET:
			elementImage = ui.ImageBox()
			elementImage.SetParent(self)
			elementImage.Hide()
			self.elementImage = elementImage

		closeButton = ui.Button()
		closeButton.SetParent(self)
		closeButton.SetUpVisual("d:/ymir work/ui/public/close_button_01.sub")
		closeButton.SetOverVisual("d:/ymir work/ui/public/close_button_02.sub")
		closeButton.SetDownVisual("d:/ymir work/ui/public/close_button_03.sub")
		closeButton.SetPosition(30, 13)

		if localeInfo.IsARABIC():
			hpGauge.SetPosition(55, 17)
			hpGauge.SetWindowHorizontalAlignLeft()
			closeButton.SetWindowHorizontalAlignLeft()
		else:
			hpGauge.SetPosition(175, 17)
			hpGauge.SetWindowHorizontalAlignRight()
			closeButton.SetWindowHorizontalAlignRight()

		closeButton.SetEvent(ui.__mem_func__(self.OnPressedCloseButton))
		closeButton.Show()

		self.buttonDict = {}
		self.showingButtonList = []
		for buttonName in self.BUTTON_NAME_LIST:
			button = ui.Button()
			button.SetParent(self)

			button.SetUpVisual("d:/ymir work/ui/public/small_thin_button_01.sub")
			button.SetOverVisual("d:/ymir work/ui/public/small_thin_button_02.sub")
			button.SetDownVisual("d:/ymir work/ui/public/small_thin_button_03.sub")

			button.SetWindowHorizontalAlignCenter()
			button.SetText(buttonName)
			button.Hide()
			self.buttonDict[buttonName] = button
			self.showingButtonList.append(button)

		self.buttonDict[localeInfo.TARGET_BUTTON_WHISPER].SetEvent(ui.__mem_func__(self.OnWhisper))
		self.buttonDict[localeInfo.TARGET_BUTTON_EXCHANGE].SetEvent(ui.__mem_func__(self.OnExchange))
		self.buttonDict[localeInfo.TARGET_BUTTON_FIGHT].SetEvent(ui.__mem_func__(self.OnPVP))
		self.buttonDict[localeInfo.TARGET_BUTTON_ACCEPT_FIGHT].SetEvent(ui.__mem_func__(self.OnPVP))
		self.buttonDict[localeInfo.TARGET_BUTTON_AVENGE].SetEvent(ui.__mem_func__(self.OnPVP))
		self.buttonDict[localeInfo.TARGET_BUTTON_FRIEND].SetEvent(ui.__mem_func__(self.OnAppendToMessenger))
		self.buttonDict[localeInfo.TARGET_BUTTON_FRIEND].SetEvent(ui.__mem_func__(self.OnAppendToMessenger))
		self.buttonDict[localeInfo.TARGET_BUTTON_INVITE_PARTY].SetEvent(ui.__mem_func__(self.OnPartyInvite))
		self.buttonDict[localeInfo.TARGET_BUTTON_LEAVE_PARTY].SetEvent(ui.__mem_func__(self.OnPartyExit))
		self.buttonDict[localeInfo.TARGET_BUTTON_EXCLUDE].SetEvent(ui.__mem_func__(self.OnPartyRemove))

		self.buttonDict[localeInfo.TARGET_BUTTON_INVITE_GUILD].SAFE_SetEvent(self.__OnGuildAddMember)
		self.buttonDict[localeInfo.TARGET_BUTTON_DISMOUNT].SAFE_SetEvent(self.__OnDismount)
		self.buttonDict[localeInfo.TARGET_BUTTON_EXIT_OBSERVER].SAFE_SetEvent(self.__OnExitObserver)
		self.buttonDict[localeInfo.TARGET_BUTTON_VIEW_EQUIPMENT].SAFE_SetEvent(self.__OnViewEquipment)
		self.buttonDict[localeInfo.TARGET_BUTTON_REQUEST_ENTER_PARTY].SAFE_SetEvent(self.__OnRequestParty)
		self.buttonDict[localeInfo.TARGET_BUTTON_BUILDING_DESTROY].SAFE_SetEvent(self.__OnDestroyBuilding)
		self.buttonDict[localeInfo.TARGET_BUTTON_EMOTION_ALLOW].SAFE_SetEvent(self.__OnEmotionAllow)

		self.buttonDict["VOTE_BLOCK_CHAT"].SetEvent(ui.__mem_func__(self.__OnVoteBlockChat))

		self.buttonDict[localeInfo.TARGET_BUTTON_REPORT].SetEvent(ui.__mem_func__(self.__OnReportPlayer))

		# Panel GM (F9): "Sprawdz" otwiera zakladke "Sprawdz Gracza" z tym
		# celem, "EQ" pokazuje ekwipunek przez tekstowa odpowiedz serwera
		# (/gmpanel_view_equip) - oba tylko gdy constInfo.IsGM (RefreshButton).
		self.buttonDict["Sprawdz"].SAFE_SetEvent(self.__OnGMCheck)
		self.buttonDict["EQ"].SAFE_SetEvent(self.__OnEQClick)

		self.affectDict = {}

		self.name = name
		self.hpGauge = hpGauge
		self.closeButton = closeButton
		self.nameString = 0
		self.nameLength = 0
		self.vid = 0
		self.eventWhisper = None
		self.eventGMCheck = None
		self.eventEQ = None
		self.isShowButton = False

		self.__Initialize()
		self.ResetTargetBoard()

	def __del__(self):
		ui.ThinBoard.__del__(self)
		print "===================================================== DESTROYED TARGET BOARD"

	def __Initialize(self):
		self.nameString = ""
		self.nameLength = 0
		self.vid = 0
		self.isShowButton = False
		self.refreshTargetAffectList = []

	def Destroy(self):
		self.eventWhisper = None
		self.closeButton = None
		self.showingButtonList = None
		self.buttonDict = None
		self.name = None
		self.hpGauge = None
		self.affectDict = None
		self.affectBar = None
		self.tooltip = None
		if app.ENABLE_ELEMENTAL_TARGET:
			self.elementImage = None
		self.__Initialize()

	def SetToolTip(self, tooltip):
		self.tooltip = proxy(tooltip)

	def OnPressedCloseButton(self):
		player.ClearTarget()
		self.Close()

	def Close(self):
		self.__Initialize()
		self.Hide()
		self.affectBar.Hide()

	def Open(self, vid, name):
		if vid:
			if not constInfo.GET_VIEW_OTHER_EMPIRE_PLAYER_TARGET_BOARD():
				if not player.IsSameEmpire(vid):
					self.Hide()
					return

			if vid != self.GetTargetVID():
				self.ResetTargetBoard()
				self.SetTargetVID(vid)
				self.SetTargetName(name)

			if player.IsMainCharacterIndex(vid):
				self.__ShowMainCharacterMenu()
			elif chr.INSTANCE_TYPE_BUILDING == chr.GetInstanceType(self.vid):
				self.Hide()
			else:
				self.RefreshButton()
				self.Show()
		else:
			self.HideAllButton()
			self.__ShowButton(localeInfo.TARGET_BUTTON_WHISPER)
			self.__ShowButton("VOTE_BLOCK_CHAT")
			self.__ArrangeButtonPosition()
			self.SetTargetName(name)
			self.Show()

	def Refresh(self):
		if self.IsShow():
			if self.IsShowButton():
				self.RefreshButton()

	def RefreshByVID(self, vid):
		if vid == self.GetTargetVID():
			self.Refresh()

	def RefreshByName(self, name):
		if name == self.GetTargetName():
			self.Refresh()

	def __ShowMainCharacterMenu(self):
		canShow=0

		self.HideAllButton()

		if player.IsMountingHorse():
			self.__ShowButton(localeInfo.TARGET_BUTTON_DISMOUNT)
			canShow=1

		if player.IsObserverMode():
			self.__ShowButton(localeInfo.TARGET_BUTTON_EXIT_OBSERVER)
			canShow=1

		if canShow:
			self.__ArrangeButtonPosition()
			self.Show()
		else:
			self.Hide()

	def __ShowNameOnlyMenu(self):
		self.HideAllButton()

	def SetWhisperEvent(self, event):
		self.eventWhisper = event

	def SetGMCheckEvent(self, event):
		self.eventGMCheck = event

	def SetEQEvent(self, event):
		self.eventEQ = event

	def __OnGMCheck(self):
		if None != self.eventGMCheck:
			self.eventGMCheck(self.nameString)

	def __OnEQClick(self):
		if None != self.eventEQ:
			self.eventEQ(self.vid, self.nameString)

	def UpdatePosition(self):
		self.SetPosition(wndMgr.GetScreenWidth()/2 - self.GetWidth()/2, 10)

		x, y = self.GetGlobalPosition()
		self.affectBar.SetPosition(x, y + self.GetHeight() + 4)

	def ResetTargetBoard(self):

		for btn in self.buttonDict.values():
			btn.Hide()

		self.__Initialize()

		self.name.SetPosition(0, 13)
		self.name.SetHorizontalAlignCenter()
		self.name.SetWindowHorizontalAlignCenter()
		self.hpGauge.Hide()
		if app.ENABLE_ELEMENTAL_TARGET:
			self.elementImage.Hide()

		self.ClearTargetAffect()
		self.SetSize(250, 40)

	def SetTargetVID(self, vid):
		self.vid = vid

	if app.ENABLE_ELEMENTAL_TARGET:
		def SetRaceElement(self, vid):
			mobVnum = nonplayer.GetVnumByVID(vid)
			imagePath = GetElementalFilename(mobVnum)
			if imagePath:
				self.elementImage.LoadImage(imagePath)
				self.elementImage.SetPosition(-self.elementImage.GetWidth(), 0)
				self.elementImage.Show()

	def SetEnemyVID(self, vid):
		self.SetTargetVID(vid)
		if app.ENABLE_ELEMENTAL_TARGET:
			self.SetRaceElement(vid)

		name = chr.GetNameByVID(vid)
		level = nonplayer.GetLevelByVID(vid)
		grade = nonplayer.GetGradeByVID(vid)

		nameFront = ""
		if -1 != level:
			nameFront += "Lv." + str(level) + " "
		if self.GRADE_NAME.has_key(grade):
			nameFront += "(" + self.GRADE_NAME[grade] + ") "

		self.SetTargetName(nameFront + name, nonplayer.GetVnumByVID(vid))

	def GetTargetVID(self):
		return self.vid

	def GetTargetName(self):
		return self.nameString

	def SetTargetName(self, name, vnum=0):
		self.nameString = name
		self.nameLength = len(name)

		text = ""
		if constInfo.IS_GAMEMASTER and vnum > 10:
			text = " (vnum: %d)" % vnum
		self.nameString += text
		self.nameLength += len(text)

		self.name.SetText(self.nameString)

	def SetHP(self, hpPercentage):
		if not self.hpGauge.IsShow():

			self.SetSize(200 + 7*self.nameLength, self.GetHeight())
			self.name.SetPosition(23, 13)

			self.name.SetWindowHorizontalAlignLeft()
			self.name.SetHorizontalAlignLeft()
			self.hpGauge.Show()
			self.UpdatePosition()

		self.hpGauge.SetPercentage(hpPercentage, 100)

	def StartTargetAffect(self):
		self.refreshTargetAffectList = []

	def AddTargetAffect(self, affect_type, duration):
		self.refreshTargetAffectList.append(affect_type)
		view_data = uiAffectBar.AFFECT_SHOW_DATA

		if self.affectDict.has_key(affect_type):
			self.affectDict[affect_type][2] = int(app.GetGlobalTimeStamp()) + duration
			self.UpdateTargetAffectDuration()

		if not self.affectDict.has_key(affect_type) and view_data.has_key(affect_type):
			aff = ui.ExpandedImageBox()
			aff.SetParent(self.affectBar)
			aff.LoadImage(view_data[affect_type]["icon"])
			aff.SetScale(0.8, 0.8)
			aff.SAFE_SetStringEvent("MOUSE_OVER_IN", self.__OnMouseOverInAffect, affect_type, int(app.GetGlobalTimeStamp()) + duration)
			aff.SAFE_SetStringEvent("MOUSE_OVER_OUT", self.__OnMouseOverOutAffect)
			aff.SetPosition(27 * (len(self.refreshTargetAffectList) - 1), 0)
			aff.Show()

			durationText = ui.TextLine()
			durationText.SetParent(aff)
			durationText.SetOutline(True)
			durationText.SetText(localeInfo.SecondToAffectTime(duration))
			durationText.SetWindowHorizontalAlignCenter()
			durationText.SetHorizontalAlignCenter()
			durationText.SetWindowVerticalAlignBottom()
			durationText.SetVerticalAlignBottom()
			durationText.SetPosition(0, -9)
			durationText.Show()
			self.affectDict[affect_type] = [aff, durationText, int(app.GetGlobalTimeStamp()) + duration]

	def UpdateTargetAffectDuration(self):
		isClear = False
		for key, data in self.affectDict.iteritems():
			duration_label = data[1]
			affect_endtime = data[2]
			duration = affect_endtime - int(app.GetGlobalTimeStamp())
			duration_label.SetText(localeInfo.SecondToAffectTime(duration))

			if duration < 0 and key in self.refreshTargetAffectList:
				self.refreshTargetAffectList.remove(key)
				isClear = True

		if isClear:
			self.ClearTargetAffect()

	def ClearTargetAffect(self):
		temp = []
		for affect_type in self.affectDict.iterkeys():
			if affect_type not in self.refreshTargetAffectList:
				self.affectDict[affect_type][0].Hide()
				if self.tooltip and self.tooltip.IsShow():
					self.tooltip.HideToolTip()
				temp.append(affect_type)

		for i in temp:
			self.affectDict.pop(i)

		self.__RefreshTargetAffectPosition()

	def __OnMouseOverInAffect(self, affect_type, duration):
		if self.tooltip:
			self.tooltip.ClearToolTip()
			self.tooltip.AppendTextLine(uiAffectBar.AFFECT_SHOW_DATA[affect_type]["description"])
			self.tooltip.AppendTextLine(localeInfo.SecondToSmartDHMS(duration -  int(app.GetGlobalTimeStamp())))
			self.tooltip.ShowToolTip()

	def __OnMouseOverOutAffect(self):
		if self.tooltip:
			self.tooltip.HideToolTip()

	def __RefreshTargetAffectPosition(self):
		idx = 0
		for affect_type in self.refreshTargetAffectList:
			self.affectDict[affect_type][0].SetPosition(27 * idx, -4)
			idx += 1

		self.affectBar.SetSize(idx * (27 + 2), 27)
		self.affectBar.Show()

	def ShowDefaultButton(self):

		self.isShowButton = True
		self.showingButtonList.append(self.buttonDict[localeInfo.TARGET_BUTTON_WHISPER])
		self.showingButtonList.append(self.buttonDict[localeInfo.TARGET_BUTTON_EXCHANGE])
		self.showingButtonList.append(self.buttonDict[localeInfo.TARGET_BUTTON_FIGHT])
		self.showingButtonList.append(self.buttonDict[localeInfo.TARGET_BUTTON_EMOTION_ALLOW])
		self.showingButtonList.append(self.buttonDict[localeInfo.TARGET_BUTTON_REPORT])
		for button in self.showingButtonList:
			button.Show()

	def HideAllButton(self):
		self.isShowButton = False
		for button in self.showingButtonList:
			button.Hide()
		self.showingButtonList = []

	def __ShowButton(self, name):

		if not self.buttonDict.has_key(name):
			return

		self.buttonDict[name].Show()
		self.showingButtonList.append(self.buttonDict[name])

	def __HideButton(self, name):

		if not self.buttonDict.has_key(name):
			return

		button = self.buttonDict[name]
		button.Hide()

		for btnInList in self.showingButtonList:
			if btnInList == button:
				self.showingButtonList.remove(button)
				break

	def OnWhisper(self):
		if None != self.eventWhisper:
			self.eventWhisper(self.nameString)

	def OnExchange(self):
		net.SendExchangeStartPacket(self.vid)

	def OnPVP(self):
		net.SendChatPacket("/pvp %d" % (self.vid))

	def OnAppendToMessenger(self):
		net.SendMessengerAddByVIDPacket(self.vid)

	def OnPartyInvite(self):
		net.SendPartyInvitePacket(self.vid)

	def OnPartyExit(self):
		net.SendPartyExitPacket()

	def OnPartyRemove(self):
		net.SendPartyRemovePacketVID(self.vid)

	def __OnGuildAddMember(self):
		net.SendGuildAddMemberPacket(self.vid)

	def __OnDismount(self):
		net.SendChatPacket("/unmount")

	def __OnExitObserver(self):
		net.SendChatPacket("/observer_exit")

	def __OnViewEquipment(self):
		net.SendChatPacket("/view_equip " + str(self.vid))

	def __OnRequestParty(self):
		net.SendChatPacket("/party_request " + str(self.vid))

	def __OnDestroyBuilding(self):
		net.SendChatPacket("/build d %d" % (self.vid))

	def __OnEmotionAllow(self):
		net.SendChatPacket("/emotion_allow %d" % (self.vid))

	def __OnVoteBlockChat(self):
		cmd = "/vote_block_chat %s" % (self.nameString)
		net.SendChatPacket(cmd)

	def __OnReportPlayer(self):
		eventManager.EventManager().send_event(uiReport.EVENT_REPORT_PLAYER, self.nameString, self.vid)

	def OnPressEscapeKey(self):
		self.OnPressedCloseButton()
		return True

	def IsShowButton(self):
		return self.isShowButton

	def RefreshButton(self):

		self.HideAllButton()

		if chr.INSTANCE_TYPE_BUILDING == chr.GetInstanceType(self.vid):
			#self.__ShowButton(localeInfo.TARGET_BUTTON_BUILDING_DESTROY)
			#self.__ArrangeButtonPosition()
			return

		if player.IsPVPInstance(self.vid) or player.IsObserverMode():
			# PVP_INFO_SIZE_BUG_FIX
			self.SetSize(200 + 7*self.nameLength, 40)
			self.UpdatePosition()
			# END_OF_PVP_INFO_SIZE_BUG_FIX
			return

		self.ShowDefaultButton()

		if self.nameString[0] == '[':
			self.__HideButton(localeInfo.TARGET_BUTTON_REPORT)

		if guild.MainPlayerHasAuthority(guild.AUTH_ADD_MEMBER):
			if not guild.IsMemberByName(self.nameString):
				if 0 == chr.GetGuildID(self.vid):
					self.__ShowButton(localeInfo.TARGET_BUTTON_INVITE_GUILD)

		if not messenger.IsFriendByName(self.nameString):
			self.__ShowButton(localeInfo.TARGET_BUTTON_FRIEND)

		if player.IsPartyMember(self.vid):

			self.__HideButton(localeInfo.TARGET_BUTTON_FIGHT)

			if player.IsPartyLeader(self.vid):
				self.__ShowButton(localeInfo.TARGET_BUTTON_LEAVE_PARTY)
			elif player.IsPartyLeader(player.GetMainCharacterIndex()):
				self.__ShowButton(localeInfo.TARGET_BUTTON_EXCLUDE)

		else:
			if player.IsPartyMember(player.GetMainCharacterIndex()):
				if player.IsPartyLeader(player.GetMainCharacterIndex()):
					self.__ShowButton(localeInfo.TARGET_BUTTON_INVITE_PARTY)
			else:
				if chr.IsPartyMember(self.vid):
					self.__ShowButton(localeInfo.TARGET_BUTTON_REQUEST_ENTER_PARTY)
				else:
					self.__ShowButton(localeInfo.TARGET_BUTTON_INVITE_PARTY)

			if player.IsRevengeInstance(self.vid):
				self.__HideButton(localeInfo.TARGET_BUTTON_FIGHT)
				self.__ShowButton(localeInfo.TARGET_BUTTON_AVENGE)
			elif player.IsChallengeInstance(self.vid):
				self.__HideButton(localeInfo.TARGET_BUTTON_FIGHT)
				self.__ShowButton(localeInfo.TARGET_BUTTON_ACCEPT_FIGHT)
			elif player.IsCantFightInstance(self.vid):
				self.__HideButton(localeInfo.TARGET_BUTTON_FIGHT)

			if not player.IsSameEmpire(self.vid):
				self.__HideButton(localeInfo.TARGET_BUTTON_INVITE_PARTY)
				self.__HideButton(localeInfo.TARGET_BUTTON_FRIEND)
				self.__HideButton(localeInfo.TARGET_BUTTON_FIGHT)

		distance = player.GetCharacterDistance(self.vid)
		if distance > self.EXCHANGE_LIMIT_RANGE:
			self.__HideButton(localeInfo.TARGET_BUTTON_EXCHANGE)
			self.__ArrangeButtonPosition()

		# Panel GM: "Sprawdz" i "EQ" tylko dla GM-a (flaga SetGMFlag z serwera,
		# game.py __SetGMFlag). Serwer i tak sprawdza gm_level przy kazdej
		# komendzie /gmpanel_*, wiec to tylko ukrywa przyciski przed graczem.
		if constInfo.IsGM:
			self.__ShowButton("Sprawdz")
			self.__ShowButton("EQ")

		self.__ArrangeButtonPosition()

	def __ArrangeButtonPosition(self):
		showingButtonCount = len(self.showingButtonList)

		pos = -(showingButtonCount / 2) * 68
		if 0 == showingButtonCount % 2:
			pos += 34

		for button in self.showingButtonList:
			button.SetPosition(pos, 33)
			pos += 68

		self.SetSize(max(150, showingButtonCount * 75), 65)
		self.UpdatePosition()

	def OnUpdate(self):
		if self.isShowButton:

			exchangeButton = self.buttonDict[localeInfo.TARGET_BUTTON_EXCHANGE]
			distance = player.GetCharacterDistance(self.vid)

			if distance < 0:
				return

			if exchangeButton.IsShow():
				if distance > self.EXCHANGE_LIMIT_RANGE:
					self.RefreshButton()

			else:
				if distance < self.EXCHANGE_LIMIT_RANGE:
					self.RefreshButton()
