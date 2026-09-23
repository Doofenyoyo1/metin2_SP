import ui
import uiScriptLocale
import wndMgr
import player
import miniMap
import localeInfo
import net
import app
import colorInfo
import constInfo
import background
import eventManager
import flamewindPath
import systemSetting
from constInfo import TextColor
import uiItemShop
import serverInfo as serverInfoModule

EVENT_SET_MINIMAP_HIDE_STATE = "EVENT_SET_MINIMAP_HIDE_STATE" # args | isHidden: boolean

class MapTextToolTip(ui.Window):
	def __init__(self):
		ui.Window.__init__(self)

		textLine = ui.TextLine()
		textLine.SetParent(self)
		textLine.SetHorizontalAlignCenter()
		textLine.SetOutline()
		textLine.SetHorizontalAlignRight()
		textLine.Show()
		self.textLine = textLine

	def __del__(self):
		ui.Window.__del__(self)

	def SetText(self, text):
		self.textLine.SetText(text)

	def SetTooltipPosition(self, PosX, PosY):
		self.textLine.SetPosition(PosX - 5, PosY)

	def SetTextColor(self, TextColor):
		self.textLine.SetPackedFontColor(TextColor)

	def GetTextSize(self):
		return self.textLine.GetTextSize()

class AtlasWindow(ui.ScriptWindow):

	class AtlasRenderer(ui.Window):
		def __init__(self):
			ui.Window.__init__(self)
			self.AddFlag("not_pick")

		def OnUpdate(self):
			miniMap.UpdateAtlas()

		def OnRender(self):
			(x, y) = self.GetGlobalPosition()
			fx = float(x)
			fy = float(y)
			miniMap.RenderAtlas(fx, fy)

		def HideAtlas(self):
			miniMap.HideAtlas()

		def ShowAtlas(self):
			miniMap.ShowAtlas()

	def __init__(self):
		self.tooltipInfo = MapTextToolTip()
		self.tooltipInfo.Hide()
		self.infoGuildMark = ui.MarkBox()
		self.infoGuildMark.Hide()
		self.AtlasMainWindow = None
		self.mapName = ""
		self.board = 0
		self.scaleButton = 0
		self.scale = 1

		ui.ScriptWindow.__init__(self)

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def SetMapName(self, mapName):
		self.mapName = mapName

	def LoadWindow(self):
		try:
			pyScrLoader = ui.PythonScriptLoader()
			pyScrLoader.LoadScriptFile(self, "UIScript/AtlasWindow.py")
		except:
			import exception
			exception.Abort("AtlasWindow.LoadWindow.LoadScript")

		try:
			self.board = self.GetChild("board")
			self.scaleButton = self.GetChild("ToggleScaleButton")

		except:
			import exception
			exception.Abort("AtlasWindow.LoadWindow.BindObject")

		self.AtlasMainWindow = self.AtlasRenderer()
		self.board.SetCloseEvent(self.Hide)
		self.AtlasMainWindow.SetParent(self.board)
		self.AtlasMainWindow.SetPosition(7, 30)
		self.tooltipInfo.SetParent(self.board)
		self.infoGuildMark.SetParent(self.board)
		self.SetPosition(wndMgr.GetScreenWidth() - 136 - 256 - 10, 0)
		self.Hide()

		miniMap.RegisterAtlasWindow(self)

		miniMap.SetAtlasScale(self.scale, self.scale)  # Change the scale / size of Atlas.
		self.board.SAFE_SetEvent("MOUSE_LEFT_BUTTON", self.__WarpForGM)
		self.scaleButton.SetEvent(ui.__mem_func__(self.OnScaleButton))
		(bGet, atlasSizeX, atlasSizeY) = miniMap.GetAtlasSize()
		self.atlasSizeX = atlasSizeX
		self.atlasSizeY = atlasSizeY

	@ui.WindowDestroy
	def Destroy(self):
		miniMap.UnregisterAtlasWindow()
		self.ClearDictionary()
		self.AtlasMainWindow = None
		self.tooltipAtlasClose = 0
		self.tooltipInfo = None
		self.infoGuildMark = None
		self.board = None
		self.warp_pos = (0, 0)

	def CanOpenMap(self):
		CANNOT_SEE_MAP_DICT = {
			"metin2_map_trent02": False,
			"metin2_map_nusluck01": False,
		}

		if constInfo.IS_GAMEMASTER:
			return True

		if CANNOT_SEE_MAP_DICT.has_key(self.mapName):
			return False

		return True

	def __WarpForGM(self):
		# Teleport GM-a w klikniete miejsce mapy (M + Ctrl + lewy przycisk).
		# Ctrl jest wymagany, zeby zwykle klikanie po mapie niczego nie robilo.
		#
		# IsGM ustawia nasza komenda SetGMFlag (char.cpp), IS_GAMEMASTER -
		# natywna sciezka vendora; wystarczy jedna z nich, bo i tak serwer
		# sprawdza gm_level przy samej komendzie.
		if not (constInfo.IS_GAMEMASTER or getattr(constInfo, "IsGM", False)):
			return
		if not (app.IsPressed(app.DIK_LCONTROL) or app.IsPressed(app.DIK_RCONTROL)):
			return
		# warp_pos jest LOKALNE dla biezacej mapy (OnUpdate powyzej: ulamek
		# szerokosci atlasu * rozmiar mapy), a WarpSet chce globalnych - baze
		# mapy dokleja serwer w do_gmpanel_warp_map. Natywne /go w tym silniku
		# nie istnieje, wiec ta komenda nie robila dotad nic.
		net.SendChatPacket("/gmpanel_warp_map %d %d" % (self.warp_pos[0], self.warp_pos[1]))

	def OnScaleButton(self):
		self.__ToggleScale()

	def SetMapScale(self, scale):
		self.scale = scale
		miniMap.SetAtlasScale(self.scale, self.scale)
		(_, atlasSizeX, atlasSizeY) = miniMap.GetAtlasSize()
		self.board.SetSize(atlasSizeX + 15, atlasSizeY + 38)
		self.SetSize(atlasSizeX + 15, atlasSizeY + 38)
		(x, y) = self.GetGlobalPosition()
		fx = float(x)
		fy = float(y)
		miniMap.RenderAtlas(fx, fy)

	def __ToggleScale(self):
		if systemSetting.IsShrinkMap():
			self.SetMapScale(1)
			systemSetting.SetShrinkMap(False)
		else:
			self.SetMapScale(0.75)
			systemSetting.SetShrinkMap(True)

		self.__RefreshScaleButton()

	def __RefreshScale(self):
		if systemSetting.IsShrinkMap():
			self.SetMapScale(0.75)
		else:
			self.SetMapScale(1)
		self.__RefreshScaleButton()

	def __RefreshScaleButton(self):
		if self.scale < 1:
			self.scaleButton.SetUpVisual(flamewindPath.GetPublic("strech_button_01"))
			self.scaleButton.SetOverVisual(flamewindPath.GetPublic("strech_button_02"))
			self.scaleButton.SetDownVisual(flamewindPath.GetPublic("strech_button_03"))
			self.scaleButton.SetToolTipText(localeInfo.MINIMAP_STRECH)
		else:
			self.scaleButton.SetUpVisual(flamewindPath.GetPublic("shrink_button_01"))
			self.scaleButton.SetOverVisual(flamewindPath.GetPublic("shrink_button_02"))
			self.scaleButton.SetDownVisual(flamewindPath.GetPublic("shrink_button_03"))
			self.scaleButton.SetToolTipText(localeInfo.MINIMAP_SHRINK)

		self.scaleButton.RefreshPosition()

	def OnUpdate(self):

		if not self.tooltipInfo:
			return

		if not self.infoGuildMark:
			return

		self.infoGuildMark.Hide()
		self.tooltipInfo.Hide()

		if False == self.board.IsIn():
			return

		(mouseX, mouseY) = wndMgr.GetMousePosition()
		(bFind, sName, iPosX, iPosY, dwTextColor, dwGuildID) = miniMap.GetAtlasInfo(mouseX, mouseY)

		(miniMapMousePosX, miniMapMousePosY) = self.AtlasMainWindow.GetMouseLocalPosition()
		(bGet, atlasSizeX, atlasSizeY) = miniMap.GetAtlasSize()
		(bGet, maxSizeX, maxSizeY) = miniMap.GetAtlasMaxSize()
		maxSizeX /= 100
		maxSizeY /= 100

		sizeX = float(maxSizeX) / atlasSizeX
		sizeY = float(maxSizeY) / atlasSizeY
		localMapPosX = miniMapMousePosX * sizeX
		localMapPosY = miniMapMousePosY * sizeY
		self.warp_pos = (localMapPosX, localMapPosY)

		if False == bFind:
			return

		if "empty_guild_area" == sName:
			sName = localeInfo.GUILD_EMPTY_AREA

		self.tooltipInfo.SetText("%s(%d, %d)" % (sName, iPosX, iPosY))

		(x, y) = self.GetGlobalPosition()
		self.tooltipInfo.SetTooltipPosition(mouseX - x, mouseY - y)
		self.tooltipInfo.SetTextColor(dwTextColor)
		self.tooltipInfo.Show()
		self.tooltipInfo.SetTop()

		if 0 != dwGuildID:
			textWidth, textHeight = self.tooltipInfo.GetTextSize()
			self.infoGuildMark.SetIndex(dwGuildID)
			self.infoGuildMark.SetPosition(mouseX - x - textWidth - 18 - 5, mouseY - y)
			self.infoGuildMark.Show()

	def Hide(self):
		if self.AtlasMainWindow:
			self.AtlasMainWindow.HideAtlas()
			self.AtlasMainWindow.Hide()
		ui.ScriptWindow.Hide(self)

	def Show(self):
		if not self.CanOpenMap():
			return

		if self.AtlasMainWindow:
			(bGet, iSizeX, iSizeY) = miniMap.GetAtlasSize()
			if bGet:
				self.SetSize(iSizeX + 15, iSizeY + 38)

				self.board.SetSize(iSizeX + 15, iSizeY + 38)
				#self.AtlasMainWindow.SetSize(iSizeX, iSizeY)
				self.AtlasMainWindow.ShowAtlas()
				self.AtlasMainWindow.Show()

		self.__RefreshScale()
		ui.ScriptWindow.Show(self)
		self.SetTop()

	def SetCenterPositionAdjust(self, x, y):
		self.SetPosition((wndMgr.GetScreenWidth() - self.GetWidth()) / 2 + x, (wndMgr.GetScreenHeight() - self.GetHeight()) / 2 + y)

	def OnPressEscapeKey(self):
		self.Hide()
		return True

def __RegisterMiniMapColor(type, rgb):
	miniMap.RegisterColor(type, rgb[0], rgb[1], rgb[2])

class MiniMap(ui.ScriptWindow):

	CANNOT_SEE_INFO_MAP_DICT = {
		"metin2_map_monkeydungeon" : False,
		"metin2_map_monkeydungeon_02" : False,
		"metin2_map_monkeydungeon_03" : False,
		"metin2_map_devilsCatacomb" : False,
		"metin2_map_labirynth": False,
	}

	def __init__(self):
		ui.ScriptWindow.__init__(self)

		self.__Initialize()

		miniMap.Create()
		miniMap.SetScale(2.0)

		self.AtlasWindow = AtlasWindow()
		self.AtlasWindow.LoadWindow()
		self.AtlasWindow.Hide()

		self.tooltipMiniMapOpen = MapTextToolTip()
		self.tooltipMiniMapOpen.SetText(localeInfo.MINIMAP)
		self.tooltipMiniMapOpen.Show()
		self.tooltipMiniMapClose = MapTextToolTip()
		self.tooltipMiniMapClose.SetText(localeInfo.UI_CLOSE)
		self.tooltipMiniMapClose.Show()
		self.tooltipScaleUp = MapTextToolTip()
		self.tooltipScaleUp.SetText(localeInfo.MINIMAP_INC_SCALE)
		self.tooltipScaleUp.Show()
		self.tooltipScaleDown = MapTextToolTip()
		self.tooltipScaleDown.SetText(localeInfo.MINIMAP_DEC_SCALE)
		self.tooltipScaleDown.Show()
		self.tooltipAtlasOpen = MapTextToolTip()
		self.tooltipAtlasOpen.SetText(localeInfo.MINIMAP_SHOW_AREAMAP)
		self.tooltipAtlasOpen.Show()
		self.tooltipInfo = MapTextToolTip()
		self.tooltipInfo.Show()

		self.tooltipItemShop = MapTextToolTip()
		self.tooltipItemShop.SetText(localeInfo.ITEMSHOP_TASKBAR_TOOLTIP)
		self.tooltipItemShop.Show()

		if miniMap.IsAtlas():
			self.tooltipAtlasOpen.SetText(localeInfo.MINIMAP_SHOW_AREAMAP)
		else:
			self.tooltipAtlasOpen.SetText(localeInfo.MINIMAP_CAN_NOT_SHOW_AREAMAP)

		self.tooltipInfo = MapTextToolTip()
		self.tooltipInfo.Show()

		self.mapName = ""

		self.isLoaded = 0
		self.canSeeInfo = True

		# AUTOBAN
		self.imprisonmentDuration = 0
		self.imprisonmentEndTime = 0
		self.imprisonmentEndTimeText = ""
		# END_OF_AUTOBAN

		eventManager.EventManager().add_observer(eventManager.SPECIAL_FLAG_UPDATE, self.__OnSpecialFlag)

	def __del__(self):
		miniMap.Destroy()
		ui.ScriptWindow.__del__(self)

	def __Initialize(self):
		self.positionInfo = 0
		self.observerCount = 0

		self.OpenWindow = 0
		self.CloseWindow = 0
		self.ScaleUpButton = 0
		self.ScaleDownButton = 0
		self.MiniMapHideButton = 0
		self.MiniMapShowButton = 0
		self.AtlasShowButton = 0

		self.tooltipMiniMapOpen = 0
		self.tooltipMiniMapClose = 0
		self.tooltipScaleUp = 0
		self.tooltipScaleDown = 0
		self.tooltipAtlasOpen = 0
		self.tooltipItemShop = 0
		self.tooltipInfo = None
		self.serverInfo = None

	def SetMapName(self, mapName):
		self.mapName=mapName
		self.AtlasWindow.SetMapName(mapName)

		if self.CANNOT_SEE_INFO_MAP_DICT.has_key(mapName):
			self.canSeeInfo = False
			self.HideMiniMap()
			self.tooltipMiniMapOpen.SetText(localeInfo.MINIMAP_CANNOT_SEE)
		else:
			self.canSeeInfo = True
			self.ShowMiniMap()
			self.tooltipMiniMapOpen.SetText(localeInfo.MINIMAP)

	# AUTOBAN
	def SetImprisonmentDuration(self, duration):
		self.imprisonmentDuration = duration
		self.imprisonmentEndTime = app.GetGlobalTimeStamp() + duration

		self.__UpdateImprisonmentDurationText()

	def __UpdateImprisonmentDurationText(self):
		restTime = max(self.imprisonmentEndTime - app.GetGlobalTimeStamp(), 0)

		imprisonmentEndTimeText = localeInfo.SecondToDHM(restTime)
		if imprisonmentEndTimeText != self.imprisonmentEndTimeText:
			self.imprisonmentEndTimeText = imprisonmentEndTimeText
			self.serverInfo.SetText("%s: %s" % (uiScriptLocale.AUTOBAN_QUIZ_REST_TIME, self.imprisonmentEndTimeText))
	# END_OF_AUTOBAN

	def Show(self):
		self.__LoadWindow()

		ui.ScriptWindow.Show(self)

	def __LoadWindow(self):
		if self.isLoaded == 1:
			return

		self.isLoaded = 1

		try:
			pyScrLoader = ui.PythonScriptLoader()
			if localeInfo.IsARABIC():
				pyScrLoader.LoadScriptFile(self, uiScriptLocale.LOCALE_UISCRIPT_PATH + "Minimap.py")
			else:
				pyScrLoader.LoadScriptFile(self, "UIScript/MiniMap.py")
		except:
			import exception
			exception.Abort("MiniMap.LoadWindow.LoadScript")

		try:
			self.OpenWindow = self.GetChild("OpenWindow")
			self.MiniMapWindow = self.GetChild("MiniMapWindow")
			self.ScaleUpButton = self.GetChild("ScaleUpButton")
			self.ScaleDownButton = self.GetChild("ScaleDownButton")
			self.MiniMapHideButton = self.GetChild("MiniMapHideButton")
			self.AtlasShowButton = self.GetChild("AtlasShowButton")
			self.CloseWindow = self.GetChild("CloseWindow")
			self.MiniMapShowButton = self.GetChild("MiniMapShowButton")
			self.positionInfo = self.GetChild("PositionInfo")
			self.observerCount = self.GetChild("ObserverCount")
			self.serverInfo = self.GetChild("ServerInfo")
			self.FPSInfo = self.GetChild("FPSInfo")
			self.FPSInfo.Hide()

			self.spyInfo = self.GetChild("SpyInfo")
			self.spyInfo.Hide()

			self.itemShopButton = self.GetChild("ItemShopButton")

		except:
			import exception
			exception.Abort("MiniMap.LoadWindow.Bind")

		if constInfo.MINIMAP_POSITIONINFO_ENABLE==0:
			self.positionInfo.Hide()

		self.serverInfo.SetText(net.GetServerInfo())
		self.ScaleUpButton.SetEvent(ui.__mem_func__(self.ScaleUp))
		self.ScaleDownButton.SetEvent(ui.__mem_func__(self.ScaleDown))
		self.MiniMapHideButton.SetEvent(ui.__mem_func__(self.HideMiniMap))
		self.MiniMapShowButton.SetEvent(ui.__mem_func__(self.ShowMiniMap))
		self.itemShopButton.SAFE_SetStringEvent("MOUSE_LEFT_BUTTON_UP", self.__OnItemShopButtonClick)

		if miniMap.IsAtlas():
			self.AtlasShowButton.SetEvent(ui.__mem_func__(self.ToggleAtlasWindow)) # @fixme014 ShowAtlas

		self.RefreshTooltipPosition()

		self.ShowMiniMap()

	def RefreshTooltipPosition(self):
		if self.MiniMapShowButton and self.tooltipMiniMapOpen:
			(ButtonPosX, ButtonPosY) = self.MiniMapShowButton.GetGlobalPosition()
			self.tooltipMiniMapOpen.SetTooltipPosition(ButtonPosX, ButtonPosY)

		if self.MiniMapHideButton and self.tooltipMiniMapClose:
			(ButtonPosX, ButtonPosY) = self.MiniMapHideButton.GetGlobalPosition()
			self.tooltipMiniMapClose.SetTooltipPosition(ButtonPosX, ButtonPosY)

		if self.ScaleUpButton and self.tooltipScaleUp:
			(ButtonPosX, ButtonPosY) = self.ScaleUpButton.GetGlobalPosition()
			self.tooltipScaleUp.SetTooltipPosition(ButtonPosX, ButtonPosY)

		if self.ScaleDownButton and self.tooltipScaleDown:
			(ButtonPosX, ButtonPosY) = self.ScaleDownButton.GetGlobalPosition()
			self.tooltipScaleDown.SetTooltipPosition(ButtonPosX, ButtonPosY)

		if self.AtlasShowButton and self.tooltipAtlasOpen:
			(ButtonPosX, ButtonPosY) = self.AtlasShowButton.GetGlobalPosition()
			self.tooltipAtlasOpen.SetTooltipPosition(ButtonPosX, ButtonPosY)

		if self.itemShopButton and self.tooltipItemShop:
			(ButtonPosX, ButtonPosY) = self.itemShopButton.GetGlobalPosition()
			self.tooltipItemShop.SetTooltipPosition(ButtonPosX, ButtonPosY)

	@ui.WindowDestroy
	def Destroy(self):
		self.HideMiniMap()
		if self.AtlasWindow:
			self.AtlasWindow.Destroy()
			self.AtlasWindow = None

		self.ClearDictionary()

		self.__Initialize()

	def UpdateCurrentChannel(self, channelID):
		serverData = serverInfoModule.SERVER_LIST[serverInfoModule.GetServerID()]
		(serverName, channelName) = net.GetServerInfo().split(",")
		channelName = ""
		if channelID == 99:
			channelName = localeInfo.CHANNEL_99
		else:
			channelName = TextColor("{}".format(serverData["channel"][channelID-1]["name"]), "FFffFF")
		net.SetServerInfo("{}, {}".format(serverName, channelName))
		if self.serverInfo:
			self.serverInfo.SetText(net.GetServerInfo())


	def __OnSpecialFlag(self, flag, value):
		if flag == "admin_spy":
			self.SetSpyMode(value > 0)

	def SetSpyMode(self, isShow):
		if isShow:
			self.spyInfo.Show()
		else:
			self.spyInfo.Hide()

	def UpdateObserverCount(self, observerCount):
		if observerCount>0:
			self.observerCount.Show()
		elif observerCount<=0:
			self.observerCount.Hide()

		self.observerCount.SetText(localeInfo.MINIMAP_OBSERVER_COUNT % observerCount)

	def OnUpdate(self):
		(x, y, z) = player.GetMainCharacterPosition()
		miniMap.Update(x, y)

		self.positionInfo.SetText("(%.0f, %.0f)" % (x/100, y/100))

		if self.tooltipInfo:
			if True == self.MiniMapWindow.IsIn():
				(mouseX, mouseY) = wndMgr.GetMousePosition()
				(bFind, sName, iPosX, iPosY, dwTextColor) = miniMap.GetInfo(mouseX, mouseY)
				if bFind == 0:
					self.tooltipInfo.Hide()
				elif not self.canSeeInfo:
					self.tooltipInfo.SetText("%s(%s)" % (sName, localeInfo.UI_POS_UNKNOWN))
					self.tooltipInfo.SetTooltipPosition(mouseX - 5, mouseY)
					self.tooltipInfo.SetTextColor(dwTextColor)
					self.tooltipInfo.Show()
				else:
					if localeInfo.IsARABIC() and sName[-1].isalnum():
						self.tooltipInfo.SetText("(%s)%d, %d" % (sName, iPosX, iPosY))
					else:
						self.tooltipInfo.SetText("%s(%d, %d)" % (sName, iPosX, iPosY))
					self.tooltipInfo.SetTooltipPosition(mouseX - 5, mouseY)
					self.tooltipInfo.SetTextColor(dwTextColor)
					self.tooltipInfo.Show()
			else:
				self.tooltipInfo.Hide()

			# AUTOBAN
			if self.imprisonmentDuration:
				self.__UpdateImprisonmentDurationText()
			# END_OF_AUTOBAN

		if True == self.MiniMapShowButton.IsIn():
			self.tooltipMiniMapOpen.Show()
		else:
			self.tooltipMiniMapOpen.Hide()

		if True == self.MiniMapHideButton.IsIn():
			self.tooltipMiniMapClose.Show()
		else:
			self.tooltipMiniMapClose.Hide()

		if True == self.ScaleUpButton.IsIn():
			self.tooltipScaleUp.Show()
		else:
			self.tooltipScaleUp.Hide()

		if True == self.ScaleDownButton.IsIn():
			self.tooltipScaleDown.Show()
		else:
			self.tooltipScaleDown.Hide()

		if True == self.AtlasShowButton.IsIn():
			self.tooltipAtlasOpen.Show()
		else:
			self.tooltipAtlasOpen.Hide()

		if True == self.itemShopButton.IsIn():
			self.tooltipItemShop.Show()
		else:
			self.tooltipItemShop.Hide()

	def OnRender(self):
		(x, y) = self.GetGlobalPosition()
		fx = float(x)
		fy = float(y)
		miniMap.Render(fx + 4.0, fy + 5.0)

	def Close(self):
		self.HideMiniMap()

	def HideMiniMap(self):
		eventManager.EventManager().send_event(EVENT_SET_MINIMAP_HIDE_STATE, True)
		miniMap.Hide()
		if self.OpenWindow:
			self.OpenWindow.Hide()

		if self.CloseWindow:
			self.CloseWindow.Show()

	def ShowMiniMap(self):
		if not self.canSeeInfo:
			return

		eventManager.EventManager().send_event(EVENT_SET_MINIMAP_HIDE_STATE, False)
		self.CloseWindow.Hide()
		miniMap.Show()
		self.OpenWindow.Show()

	def isShowMiniMap(self):
		return miniMap.isShow()

	def ScaleUp(self):
		miniMap.ScaleUp()

	def ScaleDown(self):
		miniMap.ScaleDown()

	def ShowAtlas(self):
		if not miniMap.IsAtlas():
			return
		if not self.AtlasWindow.IsShow():
			self.AtlasWindow.Show()

	def ToggleAtlasWindow(self):
		if not miniMap.IsAtlas():
			return
		if self.AtlasWindow.IsShow():
			self.AtlasWindow.Hide()
		else:
			self.AtlasWindow.Show()

	def __OnItemShopButtonClick(self):
		eventManager.EventManager().send_event(uiItemShop.EVENT_OPEN_ITEMSHOP)
