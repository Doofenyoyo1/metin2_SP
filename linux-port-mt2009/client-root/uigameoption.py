import ui
import snd
import systemSetting
import net
import chat
import app
import localeInfo
import constInfo
import chrmgr
import player
import uiPrivateShopBuilder
import interfaceModule
import uiScriptLocale
import eventManager

blockMode = 0
viewChatMode = 0

class OptionDialog(ui.ScriptWindow):

	def __init__(self):
		ui.ScriptWindow.__init__(self)
		self.__Initialize()
		self.__Load()
		self.RefreshViewChat()
		self.RefreshAlwaysShowName()
		self.RefreshShowDamage()
		self.RefreshShowSalesText()
		self.RefreshShopVisible()
		if app.WJ_SHOW_MOB_INFO:
			self.RefreshShowMobInfo()

		self.RefreshNightModeButtons()
		self.RefreshQuestTextButtons()
		self.RefreshFloatingTextButtons()
		self.RefreshBotTitleButtons()

	def __del__(self):
		ui.ScriptWindow.__del__(self)
		print " -------------------------------------- DELETE GAME OPTION DIALOG"

	def __Initialize(self):
		self.titleBar = 0
		self.nameColorModeButtonList = []
		self.viewTargetBoardButtonList = []
		self.pvpModeButtonDict = {}
		self.blockButtonList = []
		self.viewChatButtonList = []
		self.alwaysShowNameButtonList = []
		self.showDamageButtonList = []
		self.showsalesTextButtonList = []
		self.shopVisibleButtonList = []
		if app.WJ_SHOW_MOB_INFO:
			self.showMobInfoButtonList = []
		self.nightButtonList = []
		self.questTextButtonList = []
		self.floatingTextButtonList = []
		self.botTitleButtonList = []
		self.toolTip = None

	@ui.WindowDestroy
	def Destroy(self):
		self.ClearDictionary()

		self.__Initialize()
		if app.ENABLE_IKASHOP_RENEWAL:
			shopChildren = ('shopRangeViewHeader', 'shopFloatBar')
			for childname in shopChildren:
				obj = getattr(self, childname, None)
				if obj is not None:
					obj.Destroy()
					setattr(self, childname, None)


		print " -------------------------------------- DESTROY GAME OPTION DIALOG"

	def SetToolTip(self, tooltip):
		self.toolTip = tooltip

	def __Load_LoadScript(self, fileName):
		try:
			pyScriptLoader = ui.PythonScriptLoader()
			pyScriptLoader.LoadScriptFile(self, fileName)
		except:
			import exception
			exception.Abort("OptionDialog.__Load_LoadScript")

	def __Load_BindObject(self):
		try:
			GetObject = self.GetChild
			self.titleBar = GetObject("titlebar")
			self.nameColorModeButtonList.append(GetObject("name_color_normal"))
			self.nameColorModeButtonList.append(GetObject("name_color_empire"))
			self.viewTargetBoardButtonList.append(GetObject("target_board_no_view"))
			self.viewTargetBoardButtonList.append(GetObject("target_board_view"))
			self.pvpModeButtonDict[player.PK_MODE_PEACE] = GetObject("pvp_peace")
			self.pvpModeButtonDict[player.PK_MODE_REVENGE] = GetObject("pvp_revenge")
			self.pvpModeButtonDict[player.PK_MODE_GUILD] = GetObject("pvp_guild")
			self.pvpModeButtonDict[player.PK_MODE_FREE] = GetObject("pvp_free")
			self.blockButtonList.append(GetObject("block_exchange_button"))
			self.blockButtonList.append(GetObject("block_party_button"))
			self.blockButtonList.append(GetObject("block_guild_button"))
			self.blockButtonList.append(GetObject("block_whisper_button"))
			self.blockButtonList.append(GetObject("block_friend_button"))
			self.blockButtonList.append(GetObject("block_party_request_button"))
			self.viewChatButtonList.append(GetObject("view_chat_on_button"))
			self.viewChatButtonList.append(GetObject("view_chat_off_button"))
			self.alwaysShowNameButtonList.append(GetObject("always_show_name_on_button"))
			self.alwaysShowNameButtonList.append(GetObject("always_show_name_group_button"))
			self.alwaysShowNameButtonList.append(GetObject("always_show_name_off_button"))
			self.showDamageButtonList.append(GetObject("show_damage_all_button"))
			self.showDamageButtonList.append(GetObject("show_damage_target_button"))
			self.showDamageButtonList.append(GetObject("show_damage_off_button"))
			self.showsalesTextButtonList.append(GetObject("salestext_on_button"))
			self.showsalesTextButtonList.append(GetObject("salestext_off_button"))
			self.shopVisibleButtonList.append(GetObject("shop_visible_on_button"))
			self.shopVisibleButtonList.append(GetObject("shop_visible_found_button"))
			self.shopFloatBar = GetObject("salestext_range_controller")
			if app.WJ_SHOW_MOB_INFO:
				self.showMobInfoButtonList.append(GetObject("show_mob_level_button"))
				self.showMobInfoButtonList.append(GetObject("show_mob_AI_flag_button"))

			self.nightButtonList.append(GetObject("night_on_button"))
			self.nightButtonList.append(GetObject("night_off_button"))
			self.nightButtonList.append(GetObject("night_auto_button"))

			self.questTextButtonList.append(GetObject("quest_text_default"))
			self.questTextButtonList.append(GetObject("quest_text_fast"))
			self.questTextButtonList.append(GetObject("quest_text_no_delay"))

			self.floatingTextButtonList.append(GetObject("floating_text_left"))
			self.floatingTextButtonList.append(GetObject("floating_text_right"))
			self.floatingTextButtonList.append(GetObject("floating_text_off"))

			self.botTitleButtonList.append(GetObject("bot_title_personality_button"))
			self.botTitleButtonList.append(GetObject("bot_title_classic_button"))

		except:
			import exception
			exception.Abort("OptionDialog.__Load_BindObject")

	def __Load(self):
		self.__Load_LoadScript("uiscript/gameoptiondialog.py")

		self.__Load_BindObject()

		self.SetCenterPosition()

		self.titleBar.SetCloseEvent(ui.__mem_func__(self.Close))

		self.nameColorModeButtonList[0].SAFE_SetEvent(self.__OnClickNameColorModeNormalButton)
		self.nameColorModeButtonList[1].SAFE_SetEvent(self.__OnClickNameColorModeEmpireButton)

		self.viewTargetBoardButtonList[0].SAFE_SetEvent(self.__OnClickTargetBoardViewButton)
		self.viewTargetBoardButtonList[1].SAFE_SetEvent(self.__OnClickTargetBoardNoViewButton)

		self.pvpModeButtonDict[player.PK_MODE_PEACE].SAFE_SetEvent(self.__OnClickPvPModePeaceButton)
		self.pvpModeButtonDict[player.PK_MODE_REVENGE].SAFE_SetEvent(self.__OnClickPvPModeRevengeButton)
		self.pvpModeButtonDict[player.PK_MODE_GUILD].SAFE_SetEvent(self.__OnClickPvPModeGuildButton)
		self.pvpModeButtonDict[player.PK_MODE_FREE].SAFE_SetEvent(self.__OnClickPvPModeFreeButton)

		self.blockButtonList[0].SetToggleUpEvent(self.__OnClickBlockExchangeButton)
		self.blockButtonList[1].SetToggleUpEvent(self.__OnClickBlockPartyButton)
		self.blockButtonList[2].SetToggleUpEvent(self.__OnClickBlockGuildButton)
		self.blockButtonList[3].SetToggleUpEvent(self.__OnClickBlockWhisperButton)
		self.blockButtonList[4].SetToggleUpEvent(self.__OnClickBlockFriendButton)
		self.blockButtonList[5].SetToggleUpEvent(self.__OnClickBlockPartyRequest)

		self.blockButtonList[0].SetToggleDownEvent(self.__OnClickBlockExchangeButton)
		self.blockButtonList[1].SetToggleDownEvent(self.__OnClickBlockPartyButton)
		self.blockButtonList[2].SetToggleDownEvent(self.__OnClickBlockGuildButton)
		self.blockButtonList[3].SetToggleDownEvent(self.__OnClickBlockWhisperButton)
		self.blockButtonList[4].SetToggleDownEvent(self.__OnClickBlockFriendButton)
		self.blockButtonList[5].SetToggleDownEvent(self.__OnClickBlockPartyRequest)

		self.viewChatButtonList[0].SAFE_SetEvent(self.__OnClickViewChatOnButton)
		self.viewChatButtonList[1].SAFE_SetEvent(self.__OnClickViewChatOffButton)

		self.alwaysShowNameButtonList[0].SAFE_SetEvent(self.__OnClickAlwaysShowNameOnButton)
		self.alwaysShowNameButtonList[1].SAFE_SetEvent(self.__OnClickAlwaysShowNameGroupButton)
		self.alwaysShowNameButtonList[2].SAFE_SetEvent(self.__OnClickAlwaysShowNameOffButton)

		self.showDamageButtonList[0].SAFE_SetEvent(self.__OnClickShowDamageAllButton)
		self.showDamageButtonList[1].SAFE_SetEvent(self.__OnClickShowDamageTargetButton)
		self.showDamageButtonList[2].SAFE_SetEvent(self.__OnClickShowDamageOffButton)

		self.showsalesTextButtonList[0].SAFE_SetEvent(self.__OnClickSalesTextOnButton)
		self.showsalesTextButtonList[1].SAFE_SetEvent(self.__OnClickSalesTextOffButton)

		self.shopVisibleButtonList[0].SAFE_SetEvent(self.__OnClickShopVisibleOnButton)
		self.shopVisibleButtonList[1].SAFE_SetEvent(self.__OnClickShopVisibleFoundButton)

		if app.WJ_SHOW_MOB_INFO:
			self.showMobInfoButtonList[0].SetToggleUpEvent(self.__OnClickShowMobLevelButton)
			self.showMobInfoButtonList[0].SetToggleDownEvent(self.__OnClickShowMobLevelButton)
			self.showMobInfoButtonList[1].SetToggleUpEvent(self.__OnClickShowMobAIFlagButton)
			self.showMobInfoButtonList[1].SetToggleDownEvent(self.__OnClickShowMobAIFlagButton)

		self.floatingTextButtonList[0].SAFE_SetEvent(self.__OnClickFloatingTextButton, 1)
		self.floatingTextButtonList[1].SAFE_SetEvent(self.__OnClickFloatingTextButton, 2)
		self.floatingTextButtonList[2].SAFE_SetEvent(self.__OnClickFloatingTextButton, 0)

		self.botTitleButtonList[0].SAFE_SetEvent(self.__OnClickBotTitleButton, 1)
		self.botTitleButtonList[1].SAFE_SetEvent(self.__OnClickBotTitleButton, 0)

		for i in range(len(self.nightButtonList)):
			self.nightButtonList[i].SAFE_SetEvent(self.__OnClickNightModeButton, i)

		self.nightButtonList[1].SAFE_SetStringEvent("MOUSE_OVER_IN", self.__OnNightModeMouseOverIn, 1)
		self.nightButtonList[1].SAFE_SetStringEvent("MOUSE_OVER_OUT", self.__OnMouseOverOut)
		self.nightButtonList[2].SAFE_SetStringEvent("MOUSE_OVER_IN", self.__OnNightModeMouseOverIn, 2)
		self.nightButtonList[2].SAFE_SetStringEvent("MOUSE_OVER_OUT", self.__OnMouseOverOut)

		self.questTextButtonList[0].SAFE_SetEvent(self.__OnClickQuestTextDelayButton, 10)
		self.questTextButtonList[1].SAFE_SetEvent(self.__OnClickQuestTextDelayButton, 5)
		self.questTextButtonList[2].SAFE_SetEvent(self.__OnClickQuestTextDelayButton, 0)

		self.__ClickRadioButton(self.nameColorModeButtonList, constInfo.GET_CHRNAME_COLOR_INDEX())
		self.__ClickRadioButton(self.viewTargetBoardButtonList, constInfo.GET_VIEW_OTHER_EMPIRE_PLAYER_TARGET_BOARD())
		self.__SetPeacePKMode()
		if app.ENABLE_IKASHOP_RENEWAL:
			if app.EXTEND_IKASHOP_PRO:
				distance = systemSetting.GetShopRangeView()
				pct = (distance - 600.0) / 4400.0
				self.shopFloatBar.SetEvent(self._OnSlideShopRangeView)
				self.shopFloatBar.SetSliderPos(pct)

	def __ClickRadioButton(self, buttonList, buttonIndex):
		try:
			selButton=buttonList[buttonIndex]
		except IndexError:
			return

		for eachButton in buttonList:
			eachButton.SetUp()

		selButton.Down()
		
	if app.ENABLE_IKASHOP_RENEWAL:
		if app.EXTEND_IKASHOP_PRO:
			def _OnSlideShopRangeView(self):
				pos = self.shopFloatBar.GetSliderPos()
				distance = pos * 4400.0 + 600
				systemSetting.SetShopRangeView(distance)

	def __SetNameColorMode(self, index):
		constInfo.SET_CHRNAME_COLOR_INDEX(index)
		self.__ClickRadioButton(self.nameColorModeButtonList, index)

	def __SetTargetBoardViewMode(self, flag):
		constInfo.SET_VIEW_OTHER_EMPIRE_PLAYER_TARGET_BOARD(flag)
		self.__ClickRadioButton(self.viewTargetBoardButtonList, flag)

	def __OnClickNameColorModeNormalButton(self):
		self.__SetNameColorMode(0)

	def __OnClickNameColorModeEmpireButton(self):
		self.__SetNameColorMode(1)

	def __OnClickTargetBoardViewButton(self):
		self.__SetTargetBoardViewMode(0)

	def __OnClickTargetBoardNoViewButton(self):
		self.__SetTargetBoardViewMode(1)

	def __OnClickCameraModeShortButton(self):
		self.__SetCameraMode(0)

	def __OnClickCameraModeLongButton(self):
		self.__SetCameraMode(1)

	def __OnClickFogModeLevel0Button(self):
		self.__SetFogLevel(0)

	def __OnClickFogModeLevel1Button(self):
		self.__SetFogLevel(1)

	def __OnClickFogModeLevel2Button(self):
		self.__SetFogLevel(2)

	def __OnClickBlockExchangeButton(self):
		self.RefreshBlock()
		global blockMode
		net.SendChatPacket("/setblockmode " + str(blockMode ^ player.BLOCK_EXCHANGE))
	def __OnClickBlockPartyButton(self):
		self.RefreshBlock()
		global blockMode
		net.SendChatPacket("/setblockmode " + str(blockMode ^ player.BLOCK_PARTY))
	def __OnClickBlockGuildButton(self):
		self.RefreshBlock()
		global blockMode
		net.SendChatPacket("/setblockmode " + str(blockMode ^ player.BLOCK_GUILD))
	def __OnClickBlockWhisperButton(self):
		self.RefreshBlock()
		global blockMode
		net.SendChatPacket("/setblockmode " + str(blockMode ^ player.BLOCK_WHISPER))
	def __OnClickBlockFriendButton(self):
		self.RefreshBlock()
		global blockMode
		net.SendChatPacket("/setblockmode " + str(blockMode ^ player.BLOCK_FRIEND))
	def __OnClickBlockPartyRequest(self):
		self.RefreshBlock()
		global blockMode
		net.SendChatPacket("/setblockmode " + str(blockMode ^ player.BLOCK_PARTY_REQUEST))

	def __OnClickViewChatOnButton(self):
		global viewChatMode
		viewChatMode = 1
		systemSetting.SetViewChatFlag(viewChatMode)
		self.RefreshViewChat()
	def __OnClickViewChatOffButton(self):
		global viewChatMode
		viewChatMode = 0
		systemSetting.SetViewChatFlag(viewChatMode)
		self.RefreshViewChat()

	def __OnClickAlwaysShowNameOnButton(self):
		systemSetting.SetShowNameFlag(2)
		self.RefreshAlwaysShowName()

	def __OnClickAlwaysShowNameGroupButton(self):
		systemSetting.SetShowNameFlag(1)
		self.RefreshAlwaysShowName()

	def __OnClickAlwaysShowNameOffButton(self):
		systemSetting.SetShowNameFlag(0)
		self.RefreshAlwaysShowName()

	def __OnClickShowDamageAllButton(self):
		systemSetting.SetShowDamageFlag(systemSetting.SHOW_DAMAGE_ALL)
		self.RefreshShowDamage()

	def __OnClickShowDamageTargetButton(self):
		systemSetting.SetShowDamageFlag(systemSetting.SHOW_DAMAGE_TARGET)
		self.RefreshShowDamage()

	def __OnClickShowDamageOffButton(self):
		systemSetting.SetShowDamageFlag(systemSetting.SHOW_DAMAGE_NONE)
		self.RefreshShowDamage()

	def __OnClickSalesTextOnButton(self):
		systemSetting.SetShowSalesTextFlag(True)
		self.RefreshShowSalesText()
		uiPrivateShopBuilder.UpdateADBoard(True)

	def __OnClickSalesTextOffButton(self):
		systemSetting.SetShowSalesTextFlag(False)
		self.RefreshShowSalesText()
		uiPrivateShopBuilder.UpdateADBoard(False)

	def __OnClickShopVisibleOnButton(self):
		systemSetting.SetShopViewOnlyFound(False)
		self.RefreshShopVisible()

	def __OnClickShopVisibleFoundButton(self):
		systemSetting.SetShopViewOnlyFound(True)
		self.RefreshShopVisible()

	if app.WJ_SHOW_MOB_INFO:
		def __OnClickShowMobLevelButton(self):
			systemSetting.SetShowMobLevel(not systemSetting.IsShowMobLevel())
			self.RefreshShowMobInfo()
		def __OnClickShowMobAIFlagButton(self):
			systemSetting.SetShowMobAIFlag(not systemSetting.IsShowMobAIFlag())
			self.RefreshShowMobInfo()

	def __OnClickNightModeButton(self, mode):
		systemSetting.SetNightMode(mode)
		self.RefreshNightModeButtons()
		eventManager.EventManager().send_event(eventManager.CHANGE_NIGHT_MODE, mode)

	def __OnClickQuestTextDelayButton(self, delay):
		systemSetting.SetQuestTextDelay(delay)
		self.RefreshQuestTextButtons()

	def __OnClickFloatingTextButton(self, state):
		systemSetting.SetShowFloatingText(state)
		self.RefreshFloatingTextButtons()

	# Whether a bot's personality shows in its own row over its head
	# (NerrVoVy, 15 September; that row moved off the ranga/title spot
	# 2026-09-16 - see playerbot_status_tail.py). playerbot_titles.cfg keeps
	# the choice; the classic alignment title (ranga) is unaffected either way.
	def __OnClickBotTitleButton(self, enabled):
		import playerbot_status_tail
		playerbot_status_tail.SetTitlesEnabled(enabled)
		self.RefreshBotTitleButtons()

	def RefreshBotTitleButtons(self):
		import playerbot_status_tail
		for btn in self.botTitleButtonList:
			btn.SetUp()
		if playerbot_status_tail.TitlesEnabled():
			self.botTitleButtonList[0].Down()
		else:
			self.botTitleButtonList[1].Down()

	def __CheckPvPProtectedLevelPlayer(self):
		if player.GetStatus(player.LEVEL)<constInfo.PVPMODE_PROTECTED_LEVEL:
			self.__SetPeacePKMode()
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.OPTION_PVPMODE_PROTECT % (constInfo.PVPMODE_PROTECTED_LEVEL))
			return 1

		return 0

	def __SetPKMode(self, mode):
		for btn in self.pvpModeButtonDict.values():
			btn.SetUp()
		if self.pvpModeButtonDict.has_key(mode):
			self.pvpModeButtonDict[mode].Down()

	def __SetPeacePKMode(self):
		self.__SetPKMode(player.PK_MODE_PEACE)

	def __RefreshPVPButtonList(self):
		self.__SetPKMode(player.GetPKMode())

	def __OnClickPvPModePeaceButton(self):
		if self.__CheckPvPProtectedLevelPlayer():
			return

		self.__RefreshPVPButtonList()

		if constInfo.PVPMODE_ENABLE:
			net.SendChatPacket("/pkmode 0", chat.CHAT_TYPE_TALKING)
		else:
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.OPTION_PVPMODE_NOT_SUPPORT)

	def __OnClickPvPModeRevengeButton(self):
		if self.__CheckPvPProtectedLevelPlayer():
			return

		self.__RefreshPVPButtonList()

		if constInfo.PVPMODE_ENABLE:
			net.SendChatPacket("/pkmode 1", chat.CHAT_TYPE_TALKING)
		else:
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.OPTION_PVPMODE_NOT_SUPPORT)

	def __OnClickPvPModeFreeButton(self):
		if self.__CheckPvPProtectedLevelPlayer():
			return

		self.__RefreshPVPButtonList()

		if constInfo.PVPMODE_ENABLE:
			net.SendChatPacket("/pkmode 2", chat.CHAT_TYPE_TALKING)
		else:
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.OPTION_PVPMODE_NOT_SUPPORT)

	def __OnClickPvPModeGuildButton(self):
		if self.__CheckPvPProtectedLevelPlayer():
			return

		self.__RefreshPVPButtonList()

		if 0 == player.GetGuildID():
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.OPTION_PVPMODE_CANNOT_SET_GUILD_MODE)
			return

		if constInfo.PVPMODE_ENABLE:
			net.SendChatPacket("/pkmode 4", chat.CHAT_TYPE_TALKING)
		else:
			chat.AppendChat(chat.CHAT_TYPE_INFO, localeInfo.OPTION_PVPMODE_NOT_SUPPORT)

	def OnChangePKMode(self):
		self.__RefreshPVPButtonList()

	def OnCloseInputDialog(self):
		self.inputDialog.Close()
		self.inputDialog = None
		return True

	def OnCloseQuestionDialog(self):
		self.questionDialog.Close()
		self.questionDialog = None
		return True

	def OnPressEscapeKey(self):
		self.Close()
		return True

	def RefreshBlock(self):
		global blockMode
		for i in xrange(len(self.blockButtonList)):
			if 0 != (blockMode & (1 << i)):
				self.blockButtonList[i].Down()
			else:
				self.blockButtonList[i].SetUp()

	def RefreshViewChat(self):
		if systemSetting.IsViewChat():
			self.viewChatButtonList[0].Down()
			self.viewChatButtonList[1].SetUp()
		else:
			self.viewChatButtonList[0].SetUp()
			self.viewChatButtonList[1].Down()

	def RefreshAlwaysShowName(self):
		for i in self.alwaysShowNameButtonList:
			i.SetUp()

		if systemSetting.IsAlwaysShowName():
			self.alwaysShowNameButtonList[0].Down()
		elif systemSetting.IsGroupShowName():
			self.alwaysShowNameButtonList[1].Down()
		else:
			self.alwaysShowNameButtonList[2].Down()

	def RefreshShowDamage(self):
		for i in self.showDamageButtonList:
			i.SetUp()

		self.showDamageButtonList[len(self.showDamageButtonList) - 1 - systemSetting.GetShowDamage()].Down()

	def RefreshShowSalesText(self):
		if systemSetting.IsShowSalesText():
			self.showsalesTextButtonList[0].Down()
			self.showsalesTextButtonList[1].SetUp()
		else:
			self.showsalesTextButtonList[0].SetUp()
			self.showsalesTextButtonList[1].Down()

	def RefreshShopVisible(self):
		if not systemSetting.IsShopViewOnlyFound():
			self.shopVisibleButtonList[0].Down()
			self.shopVisibleButtonList[1].SetUp()
		else:
			self.shopVisibleButtonList[0].SetUp()
			self.shopVisibleButtonList[1].Down()

	if app.WJ_SHOW_MOB_INFO:
		def RefreshShowMobInfo(self):
			if systemSetting.IsShowMobLevel():
				self.showMobInfoButtonList[0].Down()
			else:
				self.showMobInfoButtonList[0].SetUp()
			if systemSetting.IsShowMobAIFlag():
				self.showMobInfoButtonList[1].Down()
			else:
				self.showMobInfoButtonList[1].SetUp()

	def RefreshQuestTextButtons(self):
		delay = systemSetting.GetQuestTextDelay()
		for btn in self.questTextButtonList:
			btn.SetUp()

		if delay < 1:
			self.questTextButtonList[2].Down()
		elif delay >= 10:
			self.questTextButtonList[0].Down()
		else:
			self.questTextButtonList[1].Down()

	def RefreshFloatingTextButtons(self):
		state = systemSetting.GetShowFloatingText()
		for btn in self.floatingTextButtonList:
			btn.SetUp()

		if state == 1:
			self.floatingTextButtonList[0].Down()
		elif state == 2:
			self.floatingTextButtonList[1].Down()
		else:
			self.floatingTextButtonList[2].Down()

	def RefreshNightModeButtons(self):
		for btn in self.nightButtonList:
			btn.SetUp()

		mode = systemSetting.GetNightMode()
		self.nightButtonList[mode].Down()

	def __OnNightModeMouseOverIn(self, mode):
		if self.toolTip:
			self.toolTip.ClearToolTip()
			self.toolTip.AppendTextLine(uiScriptLocale.GAME_OPTIONS_NIGHT_INFO1)
			if mode == 2:
				self.toolTip.AppendTextLine(uiScriptLocale.GAME_OPTIONS_NIGHT_INFO_AUTO1)
				self.toolTip.AppendTextLine(uiScriptLocale.GAME_OPTIONS_NIGHT_INFO_AUTO2)
			self.toolTip.ShowToolTip()

	def __OnMouseOverOut(self):
		if self.toolTip:
			self.toolTip.HideToolTip()

	def OnBlockMode(self, mode):
		global blockMode
		blockMode = mode
		self.RefreshBlock()

	def Show(self):
		self.RefreshBlock()
		ui.ScriptWindow.Show(self)

	def Close(self):
		self.Hide()
