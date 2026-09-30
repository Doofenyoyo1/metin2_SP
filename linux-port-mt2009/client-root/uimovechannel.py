import app
import dbg
import ui
import exception
import localeInfo
import chat
import serverInfo
import net
import uiCommon
import eventManager
import constInfo
import chr

class MoveChannelWindow(ui.ScriptWindow):
	def __init__(self):
		ui.ScriptWindow.__init__(self)

		self.__Initialize()
		self.__LoadWindow()

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def __Initialize(self):
		self.titleBar = None
		self.channelButtonList = []
		self.channelIDs = []
		self.currentChannel = 0

		self.ingameChannel = 1

		self.board = None
		self.titleBar = None
		self.blackBoard = None
		self.acceptButton = None
		self.cancelButton = None

	@ui.WindowDestroy
	def Destroy(self):
		self.__Initialize()
		self.ClearDictionary()
		self.Hide()

	def Open(self):
		if self.ingameChannel < 99:
			self.SelectChannel(self.ingameChannel - 1)
		else:
			self.__RefreshChannelButtons()
		self.SetCenterPosition()
		self.Show()

	def Close(self):
		self.Hide()

	def OnPressEscapeKey(self):
		self.Close()
		return True

	def __LoadWindow(self):
		# run once
		if getattr(self, "IsLoaded", False):
			return
		self.IsLoaded = True

		# load ui
		try:
			pyScrLoader = ui.PythonScriptLoader()
			pyScrLoader.LoadScriptFile(self, "UIScript/MoveChannelDialog.py")
		except:
			exception.Abort("MoveChannelWindow.__LoadWindow.LoadScript")

		# getters
		try:
			self.board = self.GetChild("MoveChannelBoard")
			self.titleBar = self.GetChild("MoveChannelTitle")
			self.blackBoard = self.GetChild("BlackBoard")
			self.acceptButton = self.GetChild("AcceptButton")
			self.cancelButton = self.GetChild("CancelButton")
		except:
			exception.Abort("MoveChannelWindow.__LoadWindow.BindObject")

		# events
		self.titleBar.SetCloseEvent(ui.__mem_func__(self.Close))
		self.acceptButton.SetEvent(ui.__mem_func__(self.ChangeChannel))
		self.cancelButton.SetEvent(ui.__mem_func__(self.Close))

		# dynamic buttons
		self.__AddChannelButtons()

	def GetChannelCount(self):
		return len(serverInfo.SERVER_LIST[serverInfo.GetServerID()]["channel"])

	def __ListedChannels(self):
		# CH1, and the channels the login's state checker found answering:
		# the second and the fresh bots' third and fourth run only when the
		# server switches them on, as the login's list says.
		channels = serverInfo.SERVER_LIST[serverInfo.GetServerID()]["channel"]
		listed = []
		for i in xrange(self.GetChannelCount()):
			if i == 0 or channels[i]["state"] not in (serverInfo.STATE_NONE, serverInfo.STATE_DICT[0]):
				listed.append(i)
		return listed

	def __AddChannelButtons(self):
		ELEM_SIZE = 30
		self.channelIDs = self.__ListedChannels()
		BOARD_SIZE = ELEM_SIZE * len(self.channelIDs)
		self.SetSize(190, 80 + BOARD_SIZE)
		self.board.SetSize(190, 80 + BOARD_SIZE)
		self.blackBoard.SetSize(163, 7 + BOARD_SIZE)

		for n, i in enumerate(self.channelIDs):
			self.channelButtonList.append(ui.MakeRadioButton(self.blackBoard, 7, 7 + ELEM_SIZE * n, "d:/ymir work/ui/flamewind/public/", "big_button_01.sub", "big_button_02.sub", "big_button_03.sub"))
			self.channelButtonList[n].SetText(serverInfo.SERVER_LIST[serverInfo.GetServerID()]["channel"][i]["name"])
			self.channelButtonList[n].SetEvent(lambda arg=i: self.SelectChannel(arg))
			self.channelButtonList[n].Show()

	def __RefreshChannelButtons(self):
		for n, i in enumerate(self.channelIDs):
			if i == self.currentChannel:
				self.channelButtonList[n].Down()
			else:
				self.channelButtonList[n].SetUp()

	def __CanSelectPremiumChannel(self):
		if constInfo.AFFECT_DICT.has_key(chr.NEW_AFFECT_SUBSCRIPTION):
			return True

		if constInfo.IS_MARRIAGE_PREMIUM:
			return True

		return False

	def SelectChannel(self, channel):
		isPremium = (channel+1) in serverInfo.SERVER_LIST[serverInfo.GetServerID()]["main"]["premium_channels"]
		if isPremium and not self.__CanSelectPremiumChannel():
			eventManager.EventManager().send_event(uiCommon.POPUP_EVENT, localeInfo.CHANNEL_SWITCH_PREMIUM_ERROR)
			self.__RefreshChannelButtons()
			return

		self.currentChannel = channel
		self.__RefreshChannelButtons()

	def ChangeChannel(self):
		channelID = self.currentChannel + 1
		if channelID <= 0: #skip invalid channel ids
			chat.AppendChat(chat.CHAT_TYPE_INFO , localeInfo.MOVE_CHANNEL_NOT_MOVE)
			return

		self.Close()
		net.SendChatPacket("/change_channel {}".format(channelID))
