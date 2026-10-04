# MT2009_PLUS_YUTNORI_V1 - Yut Nori (Yutnori), Owsap's uiMiniGameYutnori.py
# (v6.2.6) for MT2009 PLUS.
#
# The game is the server's (playerbot_yutnori.h, packets 182): this window only
# shows what it is told and sends the player's clicks. The exe of the new client
# brings what Owsap's python calls (client-patches/exe, the Yut Nori part):
#   net.SendMiniGameYutnori{Start,Giveup,Prob,CharClick,Throw,Move,Reward,
#       RequestComAction,RequestQuestFlag}, player.YutnoriShow/YutnoriChangeMotion,
#   app.YutnoriCreate, app.RENDER_TARGET_INDEX_YUTNORI, item.ITEM_VNUM_YUT_PIECE/BOARD,
#   wndMgr.RegisterMoveScaleImageBox/RegisterMoveTextLine/RegisterRenderTarget/
#   SetRenderTarget/SetMovePosition/SetMoveSpeed/MoveStart/MoveStop/GetMove/
#   SetMaxScale/SetMaxScaleRate/SetScalePivotCenter (needed) and EnableFlash/
#   DisableFlash/ResetFrame/SetAniImgScale (used when there), and the game
#   window's YutnoriProcess(type, data) / YutnoriFlagProcess(type, data) (game.py).
# An older exe has none of it: the hub's button then says the game needs the new
# client (IsSupported).
#
# What differs from Owsap's file:
#  - our ui.py has no MoveScaleImageBox, MoveTextLine, RenderTarget, keyed
#    AniImageBox events, Button.EnableFlash, AniImageBox.ResetFrame / key frames:
#    they are here (the _Widget classes and helpers), on the exe's wndMgr. A key
#    frame the exe did not report is played at the animation's end, so the goal
#    effects' chain (and the turn after it) never stops.
#  - the render target and the goal effects are made in code (the uiscripts have
#    plain windows), Owsap's PopupDialog2 / QuestionDialog sizing and the Arabic
#    layout are gone, the window is held still while a piece walks (Owsap's
#    "not_move" flag is not in our exe).
#  - the texts are Polish here (L, CP1250 escapes - Owsap's pl locale), the rules
#    are locale/pl/yutnori_event_desc.txt.
#  - the hub (uiingameevent.py) opens it: Register() from game.py.
#
# Python 2.7. CP1250 escapes for the Polish letters.

import ui
import uiCommon
import player
import net
import app
import grp
import event
import uiToolTip
import wndMgr
import snd
import item
import chat
import uiminigameutil

from _weakref import proxy
from collections import deque

# ---------------------------------------------------------------- the texts

class L:
	TITLE = 'Yutnori'
	THROW = 'Rzu\xe6'
	SCORE = 'Punktacja'
	REMAIN_COUNT = 'Ruchy'
	PROBABILITY = 'Szansa'
	PROB_DESC = 'Wybierz rzut Yut, by doda\xe6 mu wi\xeakszej szansy.'
	PLAYER_TEXT = 'Gracz'
	COM_TEXT = 'Komputer'
	REWARD_TEXT = 'Nagroda'
	START_TEXT = 'Start'
	YUTSEM1 = 'Do'
	YUTSEM2 = 'Ge'
	YUTSEM3 = 'Geol'
	YUTSEM4 = 'Yut'
	YUTSEM5 = 'Mo'
	YUTSEM6 = 'Wstecz do'
	GIVEUP_QUESTION = 'Na pewno chcesz przerwa\xe6 gr\xea? Nie otrzymasz wtedy nagrody.'
	NOTICE_1 = 'Kliknij na "Rzu\xe6", by ustali\xe6, kto zacznie.'
	NOTICE_2 = 'Rzu\xe6 pa\xb3eczkami Yut.'
	NOTICE_3 = 'Komputer rzuca pa\xb3eczkami Yut, by ustali\xe6, kto zacznie gr\xea.'
	NOTICE_4 = 'Komputer osi\xb9gn\xb9\xb3 ni\xbfszy lub taki sam wynik i mo\xbfe zaczyna\xe6.'
	NOTICE_5 = 'Uda\xb3o ci si\xea osi\xb9gn\xb9\xe6 ni\xbfszy wynik, wi\xeac mo\xbfesz zaczyna\xe6.'
	NOTICE_6 = 'Tw\xf3j rzut: %s. Posu\xf1 si\xea o nast\xeapuj\xb9c\xb9 liczb\xea p\xf3l: %d.'
	NOTICE_7 = 'Tw\xf3j rzut: %s. Musisz cofn\xb9\xe6 si\xea o 1 pole.'
	NOTICE_8 = 'Rzut: %s'
	NOTICE_9 = 'Trwa kolejka komputera.'
	NOTICE_10 = 'Ha! Uda\xb3o ci si\xea zbi\xe6 komputer!'
	NOTICE_11 = 'Co za pech! Komputer ci\xea zbi\xb3.'
	NOTICE_12 = 'Uda\xb3o ci si\xea wyrzuci\xe6 Yut lub Mo i otrzymujesz dodatkowy ruch.'
	NOTICE_13 = 'Gra dobieg\xb3a ko\xf1ca. Odbierz swoj\xb9 nagrod\xea.'
	COM_WIN = 'Czasami si\xea przegrywa, a czasami wygrywaj\xb9 inni.'
	PC_WIN = 'I am the czempion! Punktacja: %d'
	RETHROW_POPUP = 'Ale fart! Mo\xbfesz rzuci\xe6 jeszcze raz.'
	START_QUESTION = 'Aby zagra\xe6 w Yutnori, potrzebujesz %d Yang i %d plansz\xea do Yutnori. Chcesz zagra\xe6 teraz?'
	NOT_ENOUGH = 'Masz za ma\xb3o przedmiot\xf3w lub pieni\xeadzy, aby gra\xe6.'
	NO_MORE_GAIN = 'Masz ju\xbf maksymaln\xb9 liczb\xea plansz do Yutnori.'
	NEEDS_CLIENT = 'Yutnori: ta gra wymaga nowego klienta (aktualizacja exe).'
	SCORE_FONT = 'Tahoma:24b'

DESC_FILE = 'yutnori_event_desc.txt'

# The uiscripts' texts: a uiscript imports only uiScriptLocale and a few more
# (ui.PythonScriptLoader's sandbox), so they are Owsap's names on it. A locale
# file that has them already keeps its own.
import uiScriptLocale
for _key in ('TITLE', 'THROW', 'SCORE', 'REMAIN_COUNT', 'PROBABILITY', 'PLAYER_TEXT', 'COM_TEXT',
		'REWARD_TEXT', 'START_TEXT', 'YUTSEM1'):
	if not hasattr(uiScriptLocale, 'MINI_GAME_YUTNORI_' + _key):
		setattr(uiScriptLocale, 'MINI_GAME_YUTNORI_' + _key, getattr(L, _key))

# ---------------------------------------------------------------- Owsap's constants

YUTNORI_YUTSEM1 = 0
YUTNORI_YUTSEM2 = 1
YUTNORI_YUTSEM3 = 2
YUTNORI_YUTSEM4 = 3
YUTNORI_YUTSEM5 = 4
YUTNORI_YUTSEM6 = 5
YUTNORI_YUTSEM_MAX = 6

YUTNORI_GOAL_AREA = 11
YUTNORI_IN_DE_CREASE_SCORE = 10

YUT_ROOT = "d:/ymir work/ui/minigame/yutnori/"
_B = YUT_ROOT + "yut_back_img.sub"
_F = YUT_ROOT + "yut_front_img.sub"
_P = YUT_ROOT + "yut_point_img.sub"
yut_img_path = {
	YUTNORI_YUTSEM1: (_B, _B, _B, _F),
	YUTNORI_YUTSEM2: (_B, _B, _F, _P),
	YUTNORI_YUTSEM3: (_B, _F, _P, _F),
	YUTNORI_YUTSEM4: (_F, _F, _P, _F),
	YUTNORI_YUTSEM5: (_B, _B, _B, _B),
	YUTNORI_YUTSEM6: (_B, _B, _P, _B),
}

SIGN_IMAGES = ["D:/Ymir Work/UI/minigame/yutnori/sign/%d.sub" % i for i in (2, 3, 4, 5, 4, 3, 2, 1, 1, 1, 1, 1)]
ARROW_IMAGES = ["D:/Ymir Work/UI/minigame/yutnori/move_arrow/%d.sub" % i for i in (1, 2, 3, 4, 5, 4, 3, 2)]
EFFECT_IMAGES = ["D:/Ymir Work/UI/minigame/rumi/card_completion_effect/card_completion_eff%d.sub" % i for i in xrange(1, 9)]
TEXT_EFFECT_IMAGES = ["D:/Ymir Work/UI/minigame/rumi/card_completion_effect/card_completion_text_effect%d.sub" % i for i in (1, 5, 5, 5, 5, 5, 6, 6, 7, 8, 9)]
CATCH_IMAGES = ["D:/Ymir Work/UI/minigame/yutnori/catch/catch%02d.tga" % i for i in xrange(1, 10)]

DEFAULT_DESC_Y = 7
VISIBLE_LINE_COUNT = 17
DESC_WIDTH_COUNT = 50

STATE_NONE = 0
STATE_WAITING = 1
STATE_PLAY = 2

EVENT_TYPE_NOTICE = 0
EVENT_TYPE_INSER_DELAY = 1
EVENT_TYPE_DELAY = 2
EVENT_TYPE_COM_YUT_THROW = 3
EVENT_TYPE_CHANGE_TEXT_COLOR = 4
EVENT_TYPE_SHOW_UNIT = 5
EVENT_TYPE_REQUEST_COM_ACTION = 6
EVENT_TYPE_BUTTON_FLASH = 7
EVENT_TYPE_CALL_TURN_CHECK = 8

YUTNORI_STATE_THROW = 0
YUTNORI_STATE_RE_THROW = 1
YUTNORI_STATE_MOVE = 2
YUTNORI_BEFORE_TURN_SELECT = 3
YUTNORI_AFTER_TURN_SELECT = 4
YUTNORI_STATE_END = 5

LOW_TOTAL_SCORE = 150
MID_TOTAL_SCORE = 220

TOTAL_SCORE_LOW_FONT_COLOR = grp.GenerateColor(0.78, 0.78, 0.78, 1.0)
TOTAL_SCORE_MID_FONT_COLOR = 0xffEEA900
TOTAL_SCORE_HIGH_FONT_COLOR = 0xffFFFF99

REMAIN_COUNT_LOW_FONT_COLOR = grp.GenerateColor(1.0, 0.0, 0.0, 1.0)
REMAIN_COUNT_DEFAULT_FONT_COLOR = 0xffEEA900

YUTNORI_START_GOLD = 30000
YUT_PIECE_COUNT_MAX = 28
YUT_BOARD_COUNT_MAX = 999

ITEM_VNUM_YUT_PIECE = getattr(item, "ITEM_VNUM_YUT_PIECE", 79507)
ITEM_VNUM_YUT_BOARD = getattr(item, "ITEM_VNUM_YUT_BOARD", 79508)

# The exe's flag sub headers (YutnoriFlagProcess): SET_YUT_PIECE_FLAG,
# SET_YUT_BOARD_FLAG, SET_QUEST_FLAG, NO_MORE_GAIN.
FLAG_NO_MORE_GAIN = 13

# ---------------------------------------------------------------- what the exe brings

def _Has(module, *names):
	for name in names:
		if not hasattr(module, name):
			return False
	return True

def IsSupported():
	"""The new exe: the packets, the 3D thrower and the moving widgets."""
	return _Has(net, "SendMiniGameYutnoriStart", "SendMiniGameYutnoriThrow", "SendMiniGameYutnoriRequestQuestFlag") \
		and _Has(player, "YutnoriShow", "YutnoriChangeMotion") \
		and _Has(wndMgr, "RegisterMoveScaleImageBox", "RegisterMoveTextLine", "RegisterRenderTarget", "SetRenderTarget",
				"SetMovePosition", "SetMoveSpeed", "MoveStart", "MoveStop", "GetMove", "SetMaxScale", "SetMaxScaleRate",
				"SetScalePivotCenter")

def _YutnoriShow(show):
	if hasattr(player, "YutnoriShow"):
		player.YutnoriShow(show)

def _EnableFlash(button):
	if button and hasattr(wndMgr, "EnableFlash"):
		wndMgr.EnableFlash(button.hWnd)

def _DisableFlash(button):
	if button and hasattr(wndMgr, "DisableFlash"):
		wndMgr.DisableFlash(button.hWnd)

def LoadScript(self, fileName):
	pyScrLoader = ui.PythonScriptLoader()
	pyScrLoader.LoadScriptFile(self, fileName)

# ---------------------------------------------------------------- the widgets Owsap's ui.py had

class _AniImageBox(ui.AniImageBox):
	"""Owsap's AniImageBox: keyed mouse events, end-frame and key-frame events,
	ResetFrame. A key frame the exe does not report is played at the end."""

	def __init__(self, layer = "UI"):
		ui.AniImageBox.__init__(self, layer)
		self.end_frame_event = None
		self.end_frame_args = ()
		self.key_frame_event = None
		self.key_frames_seen = False
		self.eventFunc = {"mouse_click" : None, "mouse_over_in" : None, "mouse_over_out" : None}
		self.eventArgs = {"mouse_click" : None, "mouse_over_in" : None, "mouse_over_out" : None}

	def __del__(self):
		ui.AniImageBox.__del__(self)
		self.end_frame_event = None
		self.key_frame_event = None
		self.eventFunc = None
		self.eventArgs = None

	def SetEvent(self, func, *args):
		if self.eventFunc.has_key(args[0]):
			self.eventFunc[args[0]] = func
			self.eventArgs[args[0]] = args

	def OnMouseLeftButtonUp(self):
		if self.eventFunc and self.eventFunc["mouse_click"]:
			apply(self.eventFunc["mouse_click"], self.eventArgs["mouse_click"])

	def OnMouseOverIn(self):
		if self.eventFunc and self.eventFunc["mouse_over_in"]:
			apply(self.eventFunc["mouse_over_in"], self.eventArgs["mouse_over_in"])

	def OnMouseOverOut(self):
		if self.eventFunc and self.eventFunc["mouse_over_out"]:
			apply(self.eventFunc["mouse_over_out"], self.eventArgs["mouse_over_out"])

	def SetEndFrameEvent(self, event, *args):
		self.end_frame_event = event
		self.end_frame_args = args

	def SetKeyFrameEvent(self, event):
		self.key_frame_event = event

	def OnKeyFrame(self, cur_frame):
		self.key_frames_seen = True
		if self.key_frame_event:
			self.key_frame_event(cur_frame)

	def OnEndFrame(self):
		if self.key_frame_event and not self.key_frames_seen:
			for frame in (1, 2):
				self.key_frame_event(frame)
		self.key_frames_seen = False
		if self.end_frame_event:
			apply(self.end_frame_event, self.end_frame_args)

	def ResetFrame(self):
		self.key_frames_seen = False
		if hasattr(wndMgr, "ResetFrame"):
			wndMgr.ResetFrame(self.hWnd)

	def SetScale(self, xScale, yScale):
		if hasattr(wndMgr, "SetAniImgScale"):
			wndMgr.SetAniImgScale(self.hWnd, xScale, yScale)


class _MoveScaleImageBox(ui.ImageBox):
	"""Owsap's MoveScaleImageBox (the exe's CMoveScaleImageBox): the image walks
	to a screen position, growing to its maximum scale on the way."""

	def __init__(self, layer = "UI"):
		ui.ImageBox.__init__(self, layer)
		self.end_move_event = None

	def __del__(self):
		ui.ImageBox.__del__(self)
		self.end_move_event = None

	def RegisterWindow(self, layer):
		self.hWnd = wndMgr.RegisterMoveScaleImageBox(self, layer)

	def MoveStart(self):
		wndMgr.MoveStart(self.hWnd)

	def MoveStop(self):
		wndMgr.MoveStop(self.hWnd)

	def GetMove(self):
		return wndMgr.GetMove(self.hWnd)

	def SetMovePosition(self, dst_x, dst_y):
		wndMgr.SetMovePosition(self.hWnd, dst_x, dst_y)

	def SetMoveSpeed(self, speed):
		wndMgr.SetMoveSpeed(self.hWnd, speed)

	def SetMaxScale(self, scale):
		wndMgr.SetMaxScale(self.hWnd, scale)

	def SetMaxScaleRate(self, pivot):
		wndMgr.SetMaxScaleRate(self.hWnd, pivot)

	def SetScalePivotCenter(self, flag):
		wndMgr.SetScalePivotCenter(self.hWnd, flag)

	def OnEndMove(self):
		if self.end_move_event:
			self.end_move_event()

	def SetEndMoveEvent(self, event):
		self.end_move_event = event


class _MoveTextLine(ui.TextLine):
	"""Owsap's MoveTextLine (the exe's CMoveTextLine): the score that flies."""

	def __init__(self):
		ui.TextLine.__init__(self)
		self.end_move_event_func = None
		self.end_move_event_args = ()

	def __del__(self):
		ui.TextLine.__del__(self)
		self.end_move_event_func = None

	def RegisterWindow(self, layer):
		self.hWnd = wndMgr.RegisterMoveTextLine(self, layer)

	def SetMovePosition(self, dst_x, dst_y):
		wndMgr.SetMovePosition(self.hWnd, dst_x, dst_y)

	def SetMoveSpeed(self, speed):
		wndMgr.SetMoveSpeed(self.hWnd, speed)

	def MoveStart(self):
		wndMgr.MoveStart(self.hWnd)

	def MoveStop(self):
		wndMgr.MoveStop(self.hWnd)

	def GetMove(self):
		return wndMgr.GetMove(self.hWnd)

	def OnEndMove(self):
		if self.end_move_event_func:
			apply(self.end_move_event_func, self.end_move_event_args)

	def SetEndMoveEvent(self, event, *args):
		self.end_move_event_func = event
		self.end_move_event_args = args


class _RenderTarget(ui.Window):
	"""The exe's render target: the 3D thrower (race 20505) Yutnori draws."""

	def __init__(self, layer = "UI"):
		ui.Window.__init__(self, layer)
		self.number = -1

	def __del__(self):
		ui.Window.__del__(self)

	def RegisterWindow(self, layer):
		self.hWnd = wndMgr.RegisterRenderTarget(self, layer)

	def SetRenderTarget(self, number):
		self.number = number
		wndMgr.SetRenderTarget(self.hWnd, self.number)


class _Button(ui.Button):
	"""A button with Owsap's over / over-out events."""

	def SetOverEvent(self, func, *args):
		ui.Window.SetEvent(self, "MOUSE_OVER_IN", func, *args)

	def SetOverOutEvent(self, func, *args):
		ui.Window.SetEvent(self, "MOUSE_OVER_OUT", func, *args)


def _NewAni(parent, images, delay, x, y):
	ani = _AniImageBox()
	ani.SetParent(parent)
	ani.SetDelay(delay)
	for image in images:
		ani.AppendImage(image)
	ani.SetPosition(x, y)
	return ani

# ---------------------------------------------------------------- the waiting page

class YutnoriWaitingPage(ui.ScriptWindow):

	def __init__(self):
		ui.ScriptWindow.__init__(self)

		self.isLoaded = 0
		self.startButton = None
		self.descBoard = None
		self.descriptionBox = None
		self.descIndex = -1
		self.desc_y = DEFAULT_DESC_Y
		self.btnPrev = None
		self.btnNext = None
		self.start_question_dialog = None

		self.is_data_requested = False
		self.yut_piece_count = 0
		self.yut_piece_slot = None
		self.yut_piece_text = None
		self.yut_board_count = 0
		self.yut_board_slot = None
		self.yut_board_text = None
		self.tooltip_item = None

		self.__LoadWindow()

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def __LoadWindow(self):
		if self.isLoaded == 1:
			return

		self.isLoaded = 1

		try:
			LoadScript(self, "UIScript/MiniGameYutnoriWaitingPage.py")
		except:
			uiminigameutil.LoadError("MiniGameYutnoriWaitingPage.LoadWindow.LoadObject")

		try:
			self.GetChild("board").SetCloseEvent(ui.__mem_func__(self.Close))

			self.startButton = self.GetChild("game_start_button")
			self.startButton.SetEvent(ui.__mem_func__(self.__ClickStartButton))

			self.descBoard = self.GetChild("desc_board")
			# MT2009_PLUS_MINIGAME_DESC_V1: the rules as TextLines in the box
			self.descriptionBox = uiminigameutil.DescriptionText(self.descBoard, 7, DEFAULT_DESC_Y,
				self.descBoard.GetWidth() - 14, self.descBoard.GetHeight() - DEFAULT_DESC_Y,
				VISIBLE_LINE_COUNT, 16)
			self.descriptionBox.Show()

			self.btnPrev = self.GetChild("prev_button")
			self.btnNext = self.GetChild("next_button")
			self.btnPrev.SetEvent(ui.__mem_func__(self.PrevDescriptionPage))
			self.btnNext.SetEvent(ui.__mem_func__(self.NextDescriptionPage))

			self.yut_piece_slot = self.GetChild("yut_piece_count_slot")
			self.yut_piece_slot.SetOverInItemEvent(ui.__mem_func__(self.__SlotOverInPiece))
			self.yut_piece_slot.SetOverOutItemEvent(ui.__mem_func__(self.__SlotOverOutItem))
			self.yut_piece_text = self.GetChild("yut_piece_count_text")
			self.yut_piece_text.SetText("%d/%d" % (0, YUT_PIECE_COUNT_MAX))

			self.yut_board_slot = self.GetChild("yut_board_count_slot")
			self.yut_board_slot.SetOverInItemEvent(ui.__mem_func__(self.__SlotOverInBoard))
			self.yut_board_slot.SetOverOutItemEvent(ui.__mem_func__(self.__SlotOverOutItem))
			self.yut_board_text = self.GetChild("yut_board_count_text")
			self.yut_board_text.SetText("%d/%d" % (0, YUT_BOARD_COUNT_MAX))
		except:
			uiminigameutil.LoadError("MiniGameYutnoriWaitingPage.LoadWindow.BindObject")

		self.Hide()

	def Close(self):
		self.Hide()
		self.CloseStartDlg()

		if self.descriptionBox:
			self.descriptionBox.Hide()

	def Destroy(self):
		self.CloseStartDlg()
		if self.descriptionBox:
			self.descriptionBox.Destroy()
		self.ClearDictionary()
		self.isLoaded = 0
		self.startButton = None
		self.descBoard = None
		self.descriptionBox = None
		self.descIndex = -1
		self.desc_y = DEFAULT_DESC_Y
		self.btnPrev = None
		self.btnNext = None
		self.is_data_requested = False
		self.yut_piece_count = 0
		self.yut_piece_slot = None
		self.yut_piece_text = None
		self.yut_board_count = 0
		self.yut_board_slot = None
		self.yut_board_text = None
		self.tooltip_item = None

	def OnPressEscapeKey(self):
		self.Close()
		return True

	def __ClickStartButton(self):
		if self.yut_board_count == 0 or player.GetElk() < YUTNORI_START_GOLD:
			chat.AppendChat(chat.CHAT_TYPE_INFO, L.NOT_ENOUGH)
			return

		self.CloseStartDlg()
		self.start_question_dialog = uiCommon.QuestionDialog()
		self.start_question_dialog.SetText(L.START_QUESTION % (YUTNORI_START_GOLD, 1))
		self.start_question_dialog.SetAcceptEvent(ui.__mem_func__(self.__StartAccept))
		self.start_question_dialog.SetCancelEvent(ui.__mem_func__(self.__StartCancel))
		self.start_question_dialog.Open()

	def CloseStartDlg(self):
		if self.start_question_dialog:
			self.start_question_dialog.Close()
			self.start_question_dialog = None

	def __StartAccept(self):
		net.SendMiniGameYutnoriStart()
		self.CloseStartDlg()

	def __StartCancel(self):
		self.CloseStartDlg()

	def Show(self):
		# Every opening asks again (Owsap asked once): the counters change
		# with kills and a box earned earlier is handed out then.
		net.SendMiniGameYutnoriRequestQuestFlag()
		self.is_data_requested = True

		ui.ScriptWindow.Show(self)

		if self.descriptionBox:
			self.descriptionBox.LoadLocaleFile(DESC_FILE)
			self.descriptionBox.Show()
			if self.descriptionBox.HasMorePages():
				self.btnPrev.Show()
				self.btnNext.Show()
			else:
				self.btnPrev.Hide()
				self.btnNext.Hide()

		try:
			item.SelectItem(ITEM_VNUM_YUT_PIECE)
			self.yut_piece_slot.SetSlot(0, 0, 32, 32, item.GetIconImage(), (1.0, 1.0, 1.0, 1.0))
			item.SelectItem(ITEM_VNUM_YUT_BOARD)
			self.yut_board_slot.SetSlot(0, 0, 32, 32, item.GetIconImage(), (1.0, 1.0, 1.0, 1.0))
		except:
			pass	# a client without the items' rows: the counters still show

	def PrevDescriptionPage(self):
		if self.descriptionBox:
			self.descriptionBox.PrevPage()

	def NextDescriptionPage(self):
		if self.descriptionBox:
			self.descriptionBox.NextPage()

	def __SlotOverInPiece(self, slot_index):
		if self.tooltip_item:
			self.tooltip_item.SetItemToolTip(ITEM_VNUM_YUT_PIECE)

	def __SlotOverInBoard(self, slot_index):
		if self.tooltip_item:
			self.tooltip_item.SetItemToolTip(ITEM_VNUM_YUT_BOARD)

	def __SlotOverOutItem(self):
		if self.tooltip_item:
			self.tooltip_item.HideToolTip()

	def SetItemToolTip(self, tooltip):
		self.tooltip_item = tooltip

	def DecreaseMiniGameYutBoardCount(self):
		self.yut_board_count = max(0, self.yut_board_count - 1)
		self.yut_board_text.SetText("%d/%d" % (self.yut_board_count, YUT_BOARD_COUNT_MAX))

	def YutnoriFlagProcess(self, process_type, data):
		(yut_piece_count, yut_board_count) = data

		self.yut_piece_count = yut_piece_count
		self.yut_board_count = yut_board_count

		if self.yut_piece_text:
			self.yut_piece_text.SetText("%d/%d" % (self.yut_piece_count, YUT_PIECE_COUNT_MAX))
		if self.yut_board_text:
			self.yut_board_text.SetText("%d/%d" % (self.yut_board_count, YUT_BOARD_COUNT_MAX))

# ---------------------------------------------------------------- the board

class YutArea:
	def __init__(self, parent, func, x, y, index, prev, next, shortcut_next):
		self.pos = (x, y)
		self.ani_image = None
		self.index = index
		self.prev = prev
		self.next = next
		self.shortcut_next = shortcut_next
		self.arrow_img = None

		self.__CreateSign(parent, func, index)
		self.__CreateArrowImg(parent)

	def __del__(self):
		self.pos = ()
		self.ani_image = None
		self.prev = None
		self.next = None
		self.shortcut_next = None
		self.arrow_img = None

	def GetIndex(self):
		return self.index

	def GetPrevArea(self):
		return self.prev

	def GetNextArea(self):
		return self.next

	def GetShortcutNextArea(self):
		if 0 == self.shortcut_next:
			return self.next
		return self.shortcut_next

	def __CreateSign(self, parent, func, index):
		(x, y) = self.pos
		self.ani_image = _NewAni(proxy(parent), SIGN_IMAGES, 6, x, y)
		self.ani_image.SetSize(32, 32)
		self.ani_image.SetPickAlways()
		self.ani_image.Hide()
		self.ani_image.AddFlag("float")

		if func:
			self.ani_image.SetEvent(ui.__mem_func__(func), "mouse_click", index)

	def __CreateArrowImg(self, parent):
		(x, y) = self.pos
		self.arrow_img = _NewAni(proxy(parent), ARROW_IMAGES, 10, x + 7, y - 34)
		self.arrow_img.Hide()
		self.arrow_img.AddFlag("not_pick")
		self.arrow_img.AddFlag("float")

	def GetPos(self):
		return self.pos

	def GetGlobalPosition(self):
		return self.ani_image.GetGlobalPosition()

	def GetLocalPosition(self):
		return self.ani_image.GetLocalPosition()

	def Show(self):
		if self.ani_image:
			self.ani_image.ResetFrame()
			self.ani_image.SetTop()
			self.ani_image.Show()

		self.ArrowImgShow()

	def Hide(self):
		if self.ani_image:
			self.ani_image.Hide()

		self.ArrowImgHide()

	def ArrowImgShow(self):
		if self.arrow_img:
			self.arrow_img.SetTop()
			self.arrow_img.ResetFrame()
			self.arrow_img.Show()

	def ArrowImgHide(self):
		if self.arrow_img:
			self.arrow_img.Hide()

class Yut:
	def __init__(self, is_pc, parent, click_func, move_end_func, explosion_end_func, goal_score_func, num, x, y):
		self.is_pc = is_pc
		self.num = num
		self.pos = (x, y)
		self.char_image = None
		self.ani_image = None
		self.cur_index = -1
		self.available_index = -1
		self.move_deque = deque()
		self.move_end_func = move_end_func
		self.slow_motion = False
		self.catch_motion = False
		self.join = False
		self.is_flash = True
		self.join_member = []
		self.is_goal = False
		self.before_goal_cover_img = None
		self.explosion_ani_img = None
		self.explosion_end_func = explosion_end_func
		self.goal_move = False
		self.arrow_img = None
		self.goal_score_func = goal_score_func

		self.__CreateChar(parent, click_func, num)
		self.__CreateSign(parent)
		self.__CreateCoverImg(parent)

		if True == is_pc:
			self.__CreateArrowImg(parent)
			self.__CreateExplosionEffect(parent)

	def __del__(self):
		self.Destroy()

	def Destroy(self):
		self.char_image = None
		self.ani_image = None
		if self.move_deque:
			self.move_deque.clear()
		self.move_end_func = None
		self.join_member = []
		self.before_goal_cover_img = None
		self.explosion_ani_img = None
		self.explosion_end_func = None
		self.arrow_img = None
		self.goal_score_func = None

	def DisableFlash(self):
		self.is_flash = False

	def CatchPostProcess(self):
		self.cur_index = -1
		self.available_index = -1
		self.catch_motion = False
		self.join = False

		(x, y) = self.pos
		self.SetPosition(x, y)

	def CatchPreProcess(self):
		self.is_flash = True
		self.catch_motion = True

		if self.char_image:
			if self.is_pc:
				self.char_image.LoadImage(YUT_ROOT + "player_img.sub")
			else:
				self.char_image.LoadImage(YUT_ROOT + "enemy_img.sub")
			self.char_image.Show()

	def SetJoin(self):
		self.join = True

		if self.is_pc:
			self.char_image.LoadImage(YUT_ROOT + "player_join_img.sub")
		else:
			self.char_image.LoadImage(YUT_ROOT + "enemy_join_img.sub")

	def SetJoinMember(self, index):
		if index not in self.join_member:
			self.join_member.append(index)

	def GetJoinMember(self):
		return self.join_member

	def GetJoin(self):
		return self.join

	def __CreateChar(self, parent, func, num):
		(x, y) = self.pos
		self.char_image = _MoveScaleImageBox()
		self.char_image.SetParent(proxy(parent))
		if self.is_pc:
			self.char_image.LoadImage(YUT_ROOT + "player_img.sub")
		else:
			self.char_image.LoadImage(YUT_ROOT + "enemy_img.sub")
		self.char_image.SetPosition(x, y)
		self.char_image.Show()
		if func:
			self.char_image.SetEvent(ui.__mem_func__(func), "mouse_click", num)

		self.char_image.SetEndMoveEvent(ui.__mem_func__(self.__OnMoveEnd))
		self.char_image.SetMoveSpeed(2.5)
		self.char_image.SetMaxScale(1.5)
		self.char_image.SetScalePivotCenter(True)
		self.char_image.AddFlag("float")

	def __CreateSign(self, parent):
		(x, y) = self.pos
		self.ani_image = _NewAni(proxy(parent), SIGN_IMAGES, 6, x, y)
		self.ani_image.Hide()
		self.ani_image.AddFlag("not_pick")
		self.ani_image.AddFlag("float")

	def __CreateCoverImg(self, parent):
		(x, y) = self.pos
		self.before_goal_cover_img = ui.ImageBox()
		self.before_goal_cover_img.SetParent(proxy(parent))
		self.before_goal_cover_img.LoadImage(YUT_ROOT + "before_goal_img.sub")
		self.before_goal_cover_img.SetPosition(x - 2, y - 2)
		self.before_goal_cover_img.Hide()

	def __CreateArrowImg(self, parent):
		(x, y) = self.pos
		self.arrow_img = _NewAni(proxy(parent), ARROW_IMAGES, 10, x + 7, y - 34)
		self.arrow_img.Hide()
		self.arrow_img.AddFlag("not_pick")
		self.arrow_img.AddFlag("float")

	def __CreateExplosionEffect(self, parent):
		(x, y) = self.pos
		self.explosion_ani_img = _NewAni(proxy(parent), EFFECT_IMAGES, 6, x - 50, y - 32)
		self.explosion_ani_img.SetEndFrameEvent(ui.__mem_func__(self.__ExplosionEffectEnd))
		self.explosion_ani_img.Hide()
		self.explosion_ani_img.AddFlag("not_pick")
		self.explosion_ani_img.AddFlag("float")

	def __ExplosionEffectEnd(self):
		if self.explosion_ani_img:
			self.explosion_ani_img.Hide()

		if self.explosion_end_func:
			self.explosion_end_func()

	def GetPos(self):
		return self.pos

	def GetLocalPosition(self):
		if self.char_image:
			return self.char_image.GetLocalPosition()
		return (0, 0)

	def SetPosition(self, x, y):
		if self.char_image:
			self.char_image.SetPosition(x, y)
		if self.ani_image:
			self.ani_image.SetPosition(x, y)
		if self.arrow_img:
			self.arrow_img.SetPosition(x + 7, y - 34)

	def SetIndex(self, index):
		self.cur_index = index

		if YUTNORI_GOAL_AREA == index:
			self.is_goal = True

	def GetIndex(self):
		return self.cur_index

	def SetAvailableIndex(self, index):
		self.available_index = index

	def GetAvailableIndex(self):
		return self.available_index

	def IsGoal(self):
		return self.is_goal

	def FlashShow(self):
		if self.ani_image and True == self.is_flash and False == self.is_goal:
			self.ani_image.ResetFrame()
			self.ani_image.Show()

	def FlashHide(self):
		if self.ani_image:
			self.ani_image.Hide()

	def CharHide(self):
		if self.char_image:
			self.char_image.Hide()

	def SetTop(self):
		if self.char_image:
			self.char_image.SetTop()
		if self.ani_image:
			self.ani_image.SetTop()
		if self.explosion_ani_img:
			self.explosion_ani_img.SetTop()
		if self.arrow_img:
			self.arrow_img.SetTop()

	def GetMove(self):
		if self.char_image:
			return self.char_image.GetMove()
		return True

	def OnUpdate(self, parent_x, parent_y):
		self.__UpdateMove(parent_x, parent_y)

	def __UpdateMove(self, parent_x, parent_y):
		if len(self.move_deque) > 0:
			if False == self.char_image.GetMove():
				pos = self.move_deque[0]
				self.char_image.SetMovePosition(parent_x + pos[0], parent_y + pos[1])

				if 1 == len(self.move_deque) and True == self.slow_motion:
					self.char_image.SetMoveSpeed(1.5)
					self.char_image.SetMaxScale(1.8)
					self.char_image.SetMaxScaleRate(0.7)
				elif 1 == len(self.move_deque) and True == self.goal_move:
					self.char_image.SetMoveSpeed(10.0)
					self.char_image.SetMaxScale(1.0)
					self.char_image.SetMaxScaleRate(1.0)
				elif True == self.catch_motion:
					self.char_image.SetMoveSpeed(8.0)
					self.char_image.SetMaxScale(1.0)
					self.char_image.SetMaxScaleRate(1.0)
				else:
					self.char_image.SetMoveSpeed(2.5)
					self.char_image.SetMaxScale(1.5)
					self.char_image.SetMaxScaleRate(0.5)

				self.char_image.MoveStart()

	def IsMoving(self):
		return len(self.move_deque) > 0

	def Start(self, trace_index_list):
		if len(trace_index_list) < 2:
			return

		self.SetTop()
		(x, y) = trace_index_list[0]
		self.SetPosition(x, y)

		for pos in trace_index_list[1:]:
			self.move_deque.append(pos)

	def __OnMoveEnd(self):
		if len(self.move_deque) > 0:
			(x, y) = self.move_deque.popleft()
			self.SetPosition(x, y)

			if len(self.move_deque) == 1:
				if True == self.goal_move:
					if self.goal_score_func:
						self.goal_score_func(self.is_pc, self.join)

			if len(self.move_deque) == 0:
				if True == self.slow_motion:
					self.slow_motion = False

				is_call_move_end_func = True
				if True == self.catch_motion:
					if True == self.join and 1 == self.num:
						is_call_move_end_func = False
					self.CatchPostProcess()

				if True == self.is_goal:
					self.FlashHide()

				if True == self.goal_move:
					self.goal_move = False

				if self.move_end_func and True == is_call_move_end_func:
					self.move_end_func(self.is_pc, self.num, self.is_goal)

	def SetSlowMotion(self, is_slow):
		self.slow_motion = is_slow

	def ShowGoalCoverImg(self):
		if self.before_goal_cover_img:
			self.before_goal_cover_img.Show()

	def ShowExplosionEffect(self):
		if self.explosion_ani_img:
			(x, y) = self.GetLocalPosition()
			self.explosion_ani_img.SetPosition(x - 50, y - 32)
			self.explosion_ani_img.ResetFrame()
			self.explosion_ani_img.SetDelay(6)
			self.explosion_ani_img.SetTop()
			self.explosion_ani_img.Show()

	def SetGoalMove(self, flag):
		self.goal_move = flag

	def ArrowImgShow(self):
		if self.arrow_img and True == self.is_flash and False == self.is_goal:
			if True == self.join and 1 == self.num:
				return
			self.arrow_img.SetTop()
			self.arrow_img.ResetFrame()
			self.arrow_img.Show()

	def ArrowImgHide(self):
		if self.arrow_img:
			self.arrow_img.Hide()

# ---------------------------------------------------------------- the game page

class YutnoriGamePage(ui.ScriptWindow):
	def __init__(self):
		ui.ScriptWindow.__init__(self)
		self.isLoaded = 0

		self.prob_name_tuple = (L.YUTSEM1, L.YUTSEM2, L.YUTSEM3, L.YUTSEM4, L.YUTSEM5, L.YUTSEM6)

		self.__Reset()
		self.__LoadWindow()

	def __Reset(self):
		self.prob_select_button = None
		self.prob_select_list_open = False
		self.prob_select_window = None
		self.prob_select_button_list = []
		self.prob_select_over_img = None
		self.prob_select_text = None
		self.prob_text_window = None
		self.prob_index = 0
		self.prob_title_widow = None
		self.reward_button = None
		self.yut_throw_button = None
		self.yut_img = []
		self.yut_alpha_update = False
		self.yut_cur_alpha = 0.0
		self.giveup_dialog = None
		self.toolTip = None
		self.tooltip_shown = False
		self.score_text = None
		self.remain_count_text = None
		self.notice_text = None
		self.player_text = None
		self.com_text = None
		self.player_list = []
		self.enemy_list = []
		self.player_index = -1
		self.pc_turn = True
		self.yut_result_pc = YUTNORI_YUTSEM_MAX
		self.yut_result_com = YUTNORI_YUTSEM_MAX
		self.area_dict = {}
		self.event_deque = None
		self.board = None
		self.next_turn = True
		self.yutnori_state = YUTNORI_BEFORE_TURN_SELECT
		self.model_view = None
		self.catch_deque = None
		self.catch_ani = None
		self.is_actionable = True
		self.end_img = None
		self.score = 250
		self.before_score = 250

		self.re_throw_popup = None
		self.com_win_popup = None
		self.pc_win_popup = None
		self.player_text_cover_over_img = None
		self.com_text_cover_over_img = None

		self.goal_effect1 = None
		self.goal_effect2 = None
		self.goal_effect3 = None
		self.goal_text_effect = None
		self.is_goal_text_effect = False
		self.goal_effect_end_frame_func = None

		self.arrow_img = None
		self.score_effect = None
		self.move_text_dict = {}
		self.move_text_key = 0
		self.held_position = None

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def Destroy(self):
		_YutnoriShow(False)
		for dialog in (self.giveup_dialog, self.re_throw_popup, self.com_win_popup, self.pc_win_popup):
			if dialog:
				dialog.Close()
		for unit in self.player_list + self.enemy_list:
			unit.Destroy()
		if self.toolTip:
			self.toolTip.HideToolTip()
		self.ClearDictionary()
		self.isLoaded = 0
		self.__Reset()

	def OnPressEscapeKey(self):
		self.Close()
		return True

	def Show(self):
		self.__LoadWindow()
		ui.ScriptWindow.Show(self)

	def Close(self, is_giveup = False):
		if False == is_giveup:
			if False == self.pc_turn:
				return
			if False == self.is_actionable:
				return

		_YutnoriShow(False)

		if self.giveup_dialog:
			self.giveup_dialog.Close()

		if self.toolTip:
			self.toolTip.HideToolTip()

		self.__ClearEffect()
		self.Hide()

	def __LoadWindow(self):
		_YutnoriShow(False)

		if self.isLoaded == 1:
			return

		self.isLoaded = 1

		try:
			LoadScript(self, "UIScript/MiniGameYutnoriGamePage.py")
		except:
			uiminigameutil.LoadError("YutnoriGame.LoadWindow.LoadObject")

		try:
			self.__BindObject()
		except:
			uiminigameutil.LoadError("YutnoriGame.LoadWindow.__BindObject")

		try:
			self.__BindEvent()
		except:
			uiminigameutil.LoadError("YutnoriGame.LoadWindow.__BindEvent")

		self.__CreateProbSelectButton()
		self.__CreateYutImg()
		self.__CreateCatchImage()
		self.__CreateChar()
		self.__CreateYutArea()
		self.__CreateArrowImg()

		self.Hide()

	def __BindObject(self):
		self.prob_select_button = self.GetChild("prob_select_button")
		self.prob_select_window = self.GetChild("prob_select_window")
		self.prob_select_over_img = self.GetChild("mouse_over_image")
		self.prob_select_over_img.Hide()
		self.prob_select_text = self.GetChild("probability_text")
		self.prob_text_window = self.GetChild("probability_text_window")
		self.prob_title_widow = self.GetChild("probability_title_window")
		self.reward_button = self.GetChild("reward_button")
		self.yut_throw_button = self.GetChild("yut_throw_button")
		self.score_text = self.GetChild("score_text")
		self.remain_count_text = self.GetChild("remain_count_text")
		self.notice_text = self.GetChild("notice_text")
		self.player_text = self.GetChild("player_text")
		self.com_text = self.GetChild("enemy_text")
		self.board = self.GetChild("board")

		# The exe's render target over the board's model area (uiscript: a window).
		area = self.GetChild("model_view")
		(x, y) = area.GetLocalPosition()
		self.model_view = _RenderTarget()
		self.model_view.SetParent(self.board)
		self.model_view.SetPosition(x, y)
		self.model_view.SetSize(area.GetWidth(), area.GetHeight())
		self.model_view.AddFlag("not_pick")
		self.model_view.AddFlag("float")
		self.model_view.SetRenderTarget(getattr(app, "RENDER_TARGET_INDEX_YUTNORI", 2))
		self.model_view.Show()

		self.event_deque = deque()
		self.toolTip = uiToolTip.ToolTip()
		self.toolTip.HideToolTip()
		self.catch_deque = deque()
		self.end_img = self.GetChild("end_img")
		self.end_img.Hide()

		self.player_text_cover_over_img = self.GetChild("player_text_cover_over_img")
		self.com_text_cover_over_img = self.GetChild("enemy_text_cover_over_img")

		self.goal_effect1 = _NewAni(self.board, EFFECT_IMAGES, 6, 57, 100)
		self.goal_effect2 = _NewAni(self.board, EFFECT_IMAGES, 6, 100, 100)
		self.goal_effect3 = _NewAni(self.board, EFFECT_IMAGES, 6, 143, 100)
		self.goal_text_effect = _NewAni(self.board, TEXT_EFFECT_IMAGES, 0, 91, 133)
		for effect in (self.goal_effect1, self.goal_effect2, self.goal_effect3, self.goal_text_effect):
			effect.AddFlag("not_pick")
			effect.AddFlag("float")

	def __BindEvent(self):
		if self.board:
			self.board.SetCloseEvent(ui.__mem_func__(self.__ShowGiveupDialog))

		if self.prob_select_button:
			self.prob_select_button.SetEvent(ui.__mem_func__(self.__ClickProbSelectButton))

		if self.prob_text_window:
			self.prob_text_window.SetOnMouseLeftButtonUpEvent(self.__ClickProbSelectButton)	# our ui.py wraps it itself

		if self.prob_select_text:
			self.prob_select_text.SetText(self.prob_name_tuple[0])

		if self.reward_button:
			self.reward_button.SetEvent(ui.__mem_func__(self.__ClickRewardButton))
			self.reward_button.Disable()

		if self.yut_throw_button:
			self.yut_throw_button.SetEvent(ui.__mem_func__(self.__ClickThrowButton))
			_EnableFlash(self.yut_throw_button)

		if self.notice_text:
			self.notice_text.SetText(L.NOTICE_1)

		if self.player_text_cover_over_img:
			self.player_text_cover_over_img.Show()
		if self.com_text_cover_over_img:
			self.com_text_cover_over_img.Hide()

		if self.goal_effect1:
			self.goal_effect1.SetScale(1.2, 1.2)
			self.goal_effect1.Hide()
			self.goal_effect1.SetEndFrameEvent(ui.__mem_func__(self.__GoalEffectEndFrameEvent1))
			self.goal_effect1.SetKeyFrameEvent(ui.__mem_func__(self.__GoalEffectKeyFrameEvent1))

		if self.goal_effect2:
			self.goal_effect2.SetScale(1.2, 1.2)
			self.goal_effect2.Hide()
			self.goal_effect2.SetEndFrameEvent(ui.__mem_func__(self.__GoalEffectEndFrameEvent2))
			self.goal_effect2.SetKeyFrameEvent(ui.__mem_func__(self.__GoalEffectKeyFrameEvent2))

		if self.goal_effect3:
			self.goal_effect3.SetScale(1.2, 1.2)
			self.goal_effect3.Hide()
			self.goal_effect3.SetEndFrameEvent(ui.__mem_func__(self.__GoalEffectEndFrameEvent3))

		if self.goal_text_effect:
			self.goal_text_effect.SetEndFrameEvent(ui.__mem_func__(self.__GoalTextEffectEndFrameEvent))
			self.goal_text_effect.Hide()

	def __CreateCatchImage(self):
		if self.board:
			self.catch_ani = _NewAni(proxy(self.board), CATCH_IMAGES, 6, 0, 0)
			self.catch_ani.SetEndFrameEvent(ui.__mem_func__(self.__CatchAniEndFrameEvent))
			self.catch_ani.AddFlag("not_pick")
			self.catch_ani.Hide()

	def __CreateProbSelectButton(self):
		if None == self.prob_select_window:
			return

		for i in xrange(6):
			button = _Button()
			button.SetParent(self.prob_select_window)
			button.SetPosition(0, 16 * i)

			if i == 0:
				image = YUT_ROOT + "list_top.sub"
			elif i == 5:
				image = YUT_ROOT + "list_bottom.sub"
			else:
				image = YUT_ROOT + "list_middle.sub"
			button.SetUpVisual(image)
			button.SetDownVisual(image)
			button.SetOverVisual(image)

			button.SetEvent(ui.__mem_func__(self.__ClickProbButton), i)
			button.SetOverEvent(ui.__mem_func__(self.__ClickProbButtonOver), i)
			button.SetOverOutEvent(ui.__mem_func__(self.__ClickProbButtonOverOut), i)
			button.SetText(self.prob_name_tuple[i])
			button.Hide()

			self.prob_select_button_list.append(button)

	def __CreateChar(self):
		if self.board:
			self.player_list.append(Yut(True, self.board, self.__ClickChar, self.__MoveEnd, self.__ComWinPopup, self.__GoalScore, 0, 21, 357))
			self.player_list.append(Yut(True, self.board, self.__ClickChar, self.__MoveEnd, None, self.__GoalScore, 1, 60, 357))
			self.enemy_list.append(Yut(False, self.board, None, self.__MoveEnd, None, self.__GoalScore, 0, 183, 357))
			self.enemy_list.append(Yut(False, self.board, None, self.__MoveEnd, None, self.__GoalScore, 1, 222, 357))

	def __CreateYutArea(self):
		if not self.board:
			return
		# index: (x, y, prev, next, shortcut next) - Owsap's board.
		areas = {
			11: (270, 292, 0, 10, 0), 10: (270, 244, 11, 9, 0), 9: (270, 196, 10, 8, 0), 8: (270, 148, 9, 7, 0),
			7: (270, 100, 8, 6, 0), 6: (270, 52, 7, 5, 26), 5: (222, 52, 6, 4, 0), 4: (174, 52, 5, 3, 0),
			3: (126, 52, 4, 2, 0), 2: (78, 52, 3, 1, 0), 1: (30, 52, 2, 20, 21), 20: (30, 100, 1, 19, 0),
			19: (30, 148, 20, 18, 0), 18: (30, 196, 19, 17, 0), 17: (30, 244, 18, 16, 0), 16: (30, 292, 17, 15, 0),
			15: (78, 292, 16, 14, 0), 14: (126, 292, 15, 13, 0), 13: (174, 292, 14, 12, 0), 12: (222, 292, 13, 11, 0),
			21: (70, 92, 1, 22, 0), 22: (110, 132, 21, 23, 0), 23: (150, 172, 27, 28, 24), 24: (190, 212, 23, 25, 0),
			25: (230, 252, 24, 11, 0), 26: (230, 92, 6, 27, 0), 27: (190, 132, 26, 23, 0), 28: (110, 212, 23, 29, 0),
			29: (70, 252, 28, 16, 0),
		}
		for index, (x, y, prev, next, shortcut) in areas.items():
			self.area_dict[index] = YutArea(self.board, self.__ClickArea, x, y, index, prev, next, shortcut)

	def __CreateArrowImg(self):
		if not self.yut_throw_button:
			return

		(x, y) = self.yut_throw_button.GetLocalPosition()
		self.arrow_img = _NewAni(self, ARROW_IMAGES, 10, x + 7, y - 34)
		self.arrow_img.Show()
		self.arrow_img.AddFlag("not_pick")
		self.arrow_img.AddFlag("float")

	def __CreateYutImg(self):
		yut_result_img = self.GetChild("yut_result_img")

		for i in xrange(4):
			yut_imagebox = ui.ImageBox()
			yut_imagebox.SetParent(yut_result_img)
			yut_imagebox.SetPosition(12 + i * 21 + i * 2, 26)
			yut_imagebox.Hide()
			self.yut_img.append(yut_imagebox)

	# The score that flies to the counter: +blue, -orange.
	def __CreateScoreEffect(self, is_increase, score, start_x, start_y):
		move_text = _MoveTextLine()
		move_text.SetParent(self)
		move_text.SetPosition(start_x, start_y)
		move_text.SetVerticalAlignCenter()
		move_text.SetHorizontalAlignCenter()
		move_text.SetFontName(L.SCORE_FONT)

		if True == is_increase:
			score_str = "+"
			move_text.SetPackedFontColor(0xFF2F97FF)
		else:
			score_str = "-"
			move_text.SetPackedFontColor(0xFFFF7F27)

		score_str += str(score)
		move_text.SetText(score_str)
		move_text.SetMoveSpeed(3.0)
		(parent_global_x, parent_global_y) = self.GetGlobalPosition()
		move_text.SetMovePosition(parent_global_x + 380, parent_global_y + 65)

		self.move_text_key += 1
		key = self.move_text_key
		move_text.SetEndMoveEvent(ui.__mem_func__(self.__ScoreEffectEndEvent), key)
		move_text.Show()
		move_text.MoveStart()

		self.move_text_dict[key] = move_text

	def __ScoreEffectEndEvent(self, index):
		if index in self.move_text_dict:
			self.move_text_dict[index].Hide()
			del self.move_text_dict[index]

		self.__RefreshScore()

	def __GoalEffectEndFrameEvent1(self):
		if self.goal_effect1:
			self.goal_effect1.Hide()

	def __GoalEffectEndFrameEvent2(self):
		if self.goal_effect2:
			self.goal_effect2.Hide()

	def __GoalEffectEndFrameEvent3(self):
		if self.goal_effect3:
			self.goal_effect3.Hide()

		self.__EffectEndCheck()

	def __GoalTextEffectEndFrameEvent(self):
		if self.goal_text_effect:
			self.goal_text_effect.Hide()

		self.__EffectEndCheck()

	def __EffectEndCheck(self):
		if False == self.goal_effect1.IsShow()\
			and False == self.goal_effect2.IsShow()\
			and False == self.goal_effect3.IsShow()\
			and False == self.goal_text_effect.IsShow():

			if self.goal_effect_end_frame_func:
				func = self.goal_effect_end_frame_func
				self.goal_effect_end_frame_func = None
				func()

	def __GoalEffectKeyFrameEvent1(self, cur_frame):
		if cur_frame == 2:
			if self.goal_effect2:
				self.goal_effect2.ResetFrame()
				self.goal_effect2.Show()
			if self.goal_text_effect and self.is_goal_text_effect:
				self.goal_text_effect.ResetFrame()
				self.goal_text_effect.Show()

	def __GoalEffectKeyFrameEvent2(self, cur_frame):
		if cur_frame == 1:
			if self.goal_effect3:
				self.goal_effect3.ResetFrame()
				self.goal_effect3.Show()

	def __ClearEffect(self):
		for effect in (self.goal_effect1, self.goal_effect2, self.goal_effect3, self.goal_text_effect):
			if effect:
				effect.Hide()
				effect.ResetFrame()
				effect.SetDelay(0 if effect is self.goal_text_effect else 6)

	def __ShowGiveupDialog(self):
		if False == self.pc_turn:
			return

		if None == self.giveup_dialog:
			self.giveup_dialog = uiCommon.QuestionDialog()
			self.giveup_dialog.SetText(L.GIVEUP_QUESTION)
			self.giveup_dialog.SetAcceptEvent(ui.__mem_func__(self.__GiveupAccept))
			self.giveup_dialog.SetCancelEvent(ui.__mem_func__(self.__GiveupCancel))

		self.giveup_dialog.Open()
		self.giveup_dialog.SetTop()

	def __GiveupAccept(self):
		self.__GiveupCancel()
		if YUTNORI_STATE_END != self.yutnori_state:
			if False == self.pc_turn:
				return
			if False == self.is_actionable:
				return
		net.SendMiniGameYutnoriGiveup()

	def __GiveupCancel(self):
		if self.giveup_dialog:
			self.giveup_dialog.Close()

	def __HideYutArea(self):
		for area in self.area_dict.values():
			area.Hide()

	# The exe's thrower finished its motion (YutnoriProcess(10, 0)).
	def SetYut(self):
		self.yut_cur_alpha = 0.0

		if True == self.pc_turn:
			index = self.yut_result_pc
		else:
			index = self.yut_result_com

		if not yut_img_path.has_key(index):
			return

		for i in xrange(4):
			self.yut_img[i].LoadImage(yut_img_path[index][i])
			self.yut_img[i].SetAlpha(self.yut_cur_alpha)
			self.yut_img[i].Show()

		self.yut_alpha_update = True

	def __ClickChar(self, event_type, index):
		if "mouse_click" != event_type:
			return

		if YUTNORI_STATE_MOVE != self.yutnori_state:
			return

		if False == self.is_actionable:
			return

		if False == self.pc_turn:
			return

		if -1 == self.player_list[index].GetIndex():
			if YUTNORI_YUTSEM6 == self.yut_result_pc:
				return
			if True == self.player_list[index].IsGoal():
				return

		self.__HideYutArea()
		self.__HideAllUnitFlash()

		if index == self.player_index:
			self.player_index = -1
			self.__ShowUnitFlash(True)
			return

		self.player_index = index

		available_index = self.player_list[index].GetAvailableIndex()
		if -1 == available_index:
			net.SendMiniGameYutnoriCharClick(self.player_index)
		else:
			self.AvailableAreaShow((index, available_index))

	def AvailableAreaShow(self, data):
		(player_index, available_index) = data
		if player_index < 0 or player_index >= len(self.player_list):
			return

		if self.area_dict.has_key(available_index):
			self.area_dict[available_index].Show()

		self.player_list[player_index].SetAvailableIndex(available_index)

	def __GetMoveCount(self, unit):
		# Do 1, Ge 2, Geol 3, Yut 4, Mo 5, Back-do -1
		return {0: 1, 1: 2, 2: 3, 3: 4, 4: 5, 5: -1}.get(unit, 0)

	def __ClickArea(self, event_type, index):
		if "mouse_click" != event_type:
			return

		if False == self.pc_turn:
			return

		if YUTNORI_STATE_MOVE != self.yutnori_state:
			return

		if -1 == self.player_index:
			return

		net.SendMiniGameYutnoriMove(self.player_index)
		self.player_index = -1

	def __ShowUnitFlash(self, is_pc):
		if True == is_pc:
			for yut in self.player_list:
				if YUTNORI_YUTSEM6 == self.yut_result_pc:
					if -1 == yut.GetIndex():
						continue

				yut.FlashShow()
				yut.ArrowImgShow()
		else:
			for yut in self.enemy_list:
				yut.FlashShow()
				yut.ArrowImgHide()

	def __HideAllUnitFlash(self):
		for yut in self.player_list + self.enemy_list:
			yut.FlashHide()
			yut.ArrowImgHide()

	def __ChangeTextColor(self, is_pc_turn):
		grey = grp.GenerateColor(0.7607, 0.7607, 0.7607, 1.0)
		if True == is_pc_turn:
			if self.player_text:
				self.player_text.SetPackedFontColor(0xffEEA900)
			if self.com_text:
				self.com_text.SetPackedFontColor(grey)
			if self.player_text_cover_over_img:
				self.player_text_cover_over_img.Show()
			if self.com_text_cover_over_img:
				self.com_text_cover_over_img.Hide()
		else:
			if self.player_text:
				self.player_text.SetPackedFontColor(grey)
			if self.com_text:
				self.com_text.SetPackedFontColor(0xffEEA900)
			if self.player_text_cover_over_img:
				self.player_text_cover_over_img.Hide()
			if self.com_text_cover_over_img:
				self.com_text_cover_over_img.Show()

	def __HideYut(self):
		for i in xrange(4):
			self.yut_img[i].Hide()

	def __ClickProbSelectButton(self):
		self.__ProbSelectWindowOpen(not self.prob_select_list_open)

	def __ProbSelectWindowOpen(self, open):
		self.prob_select_list_open = open

		if True == open:
			if self.prob_select_window:
				self.prob_select_window.SetSize(115, 16 * 6)
			for button in self.prob_select_button_list:
				button.Show()
		else:
			if self.prob_select_window:
				self.prob_select_window.SetSize(115, 0)
			for button in self.prob_select_button_list:
				button.Hide()
			if self.prob_select_over_img:
				self.prob_select_over_img.Hide()

	def __ClickProbButton(self, index):
		if None == self.prob_select_text:
			return

		if len(self.prob_name_tuple) <= index:
			return

		self.__ProbSelectWindowOpen(False)
		net.SendMiniGameYutnoriProb(index)

	def SetProb(self, index):
		if None == self.prob_select_text:
			return

		if len(self.prob_name_tuple) <= index:
			return

		self.prob_index = index
		self.prob_select_text.SetText(self.prob_name_tuple[index])

	def __ClickProbButtonOver(self, index):
		if not self.prob_select_over_img:
			return
		if index >= len(self.prob_select_button_list):
			return

		self.prob_select_over_img.SetPosition(328, 168 + 16 * index)
		self.prob_select_over_img.SetTop()
		self.prob_select_over_img.Show()

	def __ClickProbButtonOverOut(self, index):
		if not self.prob_select_over_img:
			return
		self.prob_select_over_img.Hide()

	def __ClickRewardButton(self):
		net.SendMiniGameYutnoriReward()

	def __ClickThrowButton(self):
		if False == self.pc_turn:
			return
		if False == self.is_actionable:
			return
		if self.yutnori_state not in [ YUTNORI_STATE_THROW, YUTNORI_STATE_RE_THROW, YUTNORI_BEFORE_TURN_SELECT ]:
			return

		net.SendMiniGameYutnoriThrow(True)

		self.ArrowImgHide()
		_DisableFlash(self.yut_throw_button)

		# MT2009_PLUS_HEAVEN_OIL_V1 (client fixes, Autor: Digi Rasta, nowy-system v0.17.2):
		# the re-throw popup's accept event is this very method, and its Close() calls
		# the event again - a recursion to Python's limit (RuntimeError in syserr) and
		# hundreds of throw packets. Hide() closes it without the event.
		if self.re_throw_popup:
			self.re_throw_popup.Hide()

	def __UpdateToolTip(self):
		if not self.toolTip:
			return
		over = (self.prob_title_widow and self.prob_title_widow.IsIn()) or (self.prob_select_button and self.prob_select_button.IsIn())
		if over and not self.tooltip_shown:
			self.toolTip.ClearToolTip()
			self.toolTip.AppendTextLine(L.PROB_DESC)
			self.toolTip.ShowToolTip()
			self.tooltip_shown = True
		elif not over and self.tooltip_shown:
			self.toolTip.HideToolTip()
			self.tooltip_shown = False

	def OnUpdate(self):
		self.__HoldPosition()
		self.__UpdateToolTip()
		self.__UpdateAlpha()
		self.__UpdateChar()
		self.__UpdateEvent()

	# Owsap's "not_move": the window stays put while a piece walks to a
	# screen position.
	def __HoldPosition(self):
		moving = False
		for unit in self.player_list + self.enemy_list:
			if unit.IsMoving():
				moving = True
				break
		if moving:
			if self.held_position is None:
				self.held_position = self.GetLocalPosition()
			elif self.GetLocalPosition() != self.held_position:
				(x, y) = self.held_position
				self.SetPosition(x, y)
		else:
			self.held_position = None

	def __UpdateAlpha(self):
		if False == self.yut_alpha_update:
			return

		self.yut_cur_alpha = self.yut_cur_alpha + 0.02

		for i in xrange(4):
			self.yut_img[i].SetAlpha(self.yut_cur_alpha)

		# The throw's result shown: the next step.
		if self.yut_cur_alpha >= 1.0:
			self.yut_alpha_update = False
			self.is_actionable = True

			if self.yutnori_state in [ YUTNORI_BEFORE_TURN_SELECT, YUTNORI_AFTER_TURN_SELECT ]:
				yut_result = self.yut_result_pc if True == self.pc_turn else self.yut_result_com
				if self.notice_text and yut_result < len(self.prob_name_tuple):
					self.notice_text.SetText(L.NOTICE_8 % self.prob_name_tuple[yut_result])

			elif True == self.pc_turn and self.yut_result_pc < len(self.prob_name_tuple):
				move_count = self.__GetMoveCount(self.yut_result_pc)
				if move_count > 0:
					notice_str = L.NOTICE_6 % (self.prob_name_tuple[self.yut_result_pc], move_count)
				else:
					notice_str = L.NOTICE_7 % self.prob_name_tuple[self.yut_result_pc]

				if self.notice_text:
					self.notice_text.SetText(notice_str)

			self.__TurnCheck()

	def __UpdateEvent(self):
		if not self.event_deque:
			return
		# MT2009_PLUS_SIDEKICK_WARP_SAFE_V1: the queued throws and the
		# computer's moves wait while not in the game phase (warpsafe.py).
		import warpsafe
		if not warpsafe.InGame():
			return

		[event_type, data] = self.event_deque[0]

		if EVENT_TYPE_NOTICE == event_type:
			self.event_deque.popleft()
			if self.notice_text:
				self.notice_text.SetText(data)

		elif EVENT_TYPE_INSER_DELAY == event_type:
			self.event_deque.popleft()
			delay_value = app.GetGlobalTime() + data
			self.event_deque.appendleft([ EVENT_TYPE_DELAY, delay_value ])

		elif EVENT_TYPE_DELAY == event_type:
			if app.GetGlobalTime() > data:
				self.event_deque.popleft()

		elif EVENT_TYPE_COM_YUT_THROW == event_type:
			self.event_deque.popleft()
			net.SendMiniGameYutnoriThrow(False)

		elif EVENT_TYPE_CHANGE_TEXT_COLOR == event_type:
			self.event_deque.popleft()
			self.__ChangeTextColor(data)

		elif EVENT_TYPE_SHOW_UNIT == event_type:
			self.event_deque.popleft()
			self.__HideAllUnitFlash()
			self.__ShowUnitFlash(data)

		elif EVENT_TYPE_REQUEST_COM_ACTION == event_type:
			self.event_deque.popleft()
			net.SendMiniGameYutnoriRequestComAction()

		elif EVENT_TYPE_BUTTON_FLASH == event_type:
			self.event_deque.popleft()
			(button, enable) = data
			if not button:
				return

			if True == enable:
				_EnableFlash(button)
				self.ArrowImgShow()
			else:
				_DisableFlash(button)
				self.ArrowImgHide()

		elif EVENT_TYPE_CALL_TURN_CHECK == event_type:
			self.event_deque.popleft()
			self.__TurnCheck()

	def __UpdateChar(self):
		if not self.board:
			return

		(x, y) = self.board.GetGlobalPosition()

		for unit in self.player_list + self.enemy_list:
			unit.OnUpdate(x, y)

	def ArrowImgShow(self):
		if self.arrow_img:
			self.arrow_img.ResetFrame()
			self.arrow_img.Show()

	def ArrowImgHide(self):
		if self.arrow_img:
			self.arrow_img.Hide()

	def ThrowResult(self, data):
		self.is_actionable = False

		(bPC, result) = data
		if result < 0 or result > 5:
			return

		self.ArrowImgHide()
		self.__HideYut()
		if self.model_view:
			self.model_view.SetTop()

		_YutnoriShow(True)
		if hasattr(player, "YutnoriChangeMotion"):
			player.YutnoriChangeMotion(result)
		snd.PlaySound("D:/ymir work/ui/minigame/yutnori/yut_throw.wav")
		self.__ThrowScoreCheck()
		if True == bPC:
			self.yut_result_pc = result
		else:
			self.yut_result_com = result

	def YutMove(self, data):
		self.__HideAllUnitFlash()
		self.__HideYutArea()

		(is_pc, unit_index, is_catch, start_index, dest_index) = data
		if unit_index < 0 or unit_index > 1 or not self.area_dict.has_key(start_index) or not self.area_dict.has_key(dest_index):
			return

		if is_pc:
			move_count = self.__GetMoveCount(self.yut_result_pc)
		else:
			move_count = self.__GetMoveCount(self.yut_result_com)

		move_index_list = []
		move_index_list.append(self.area_dict[start_index].GetLocalPosition())

		goal_move = False
		goal_pos = (126, 252) if True == is_pc else (174, 252)

		if -1 == move_count:
			move_index_list.append(self.area_dict[dest_index].GetLocalPosition())

			if YUTNORI_GOAL_AREA == dest_index:
				goal_move = True
				move_index_list.append(goal_pos)
		else:
			move_index = start_index
			cross_check_index = 0
			for i in xrange(move_count):
				# Past 22 or 27 the centre (23) goes on the way it came.
				if 22 == move_index or 27 == move_index:
					cross_check_index = move_index

				if 0 == i:
					move_index = self.area_dict[move_index].GetShortcutNextArea()
				elif 23 == move_index and 0 != cross_check_index:
					if 22 == cross_check_index:
						move_index = self.area_dict[move_index].GetShortcutNextArea()
					else:
						move_index = self.area_dict[move_index].GetNextArea()
				else:
					move_index = self.area_dict[move_index].GetNextArea()

				if not self.area_dict.has_key(move_index):
					break
				move_index_list.append(self.area_dict[move_index].GetLocalPosition())

				if YUTNORI_GOAL_AREA == move_index:
					goal_move = True
					move_index_list.append(goal_pos)
					break

		units = self.player_list if is_pc else self.enemy_list
		unit = units[unit_index]
		unit.SetIndex(dest_index)

		if unit.GetJoin():
			for member_index in unit.GetJoinMember():
				units[member_index].SetIndex(dest_index)

		unit.FlashHide()
		unit.ArrowImgHide()
		unit.SetSlowMotion(is_catch)
		unit.SetGoalMove(goal_move)
		unit.Start(move_index_list)

		self.is_actionable = False
		self.__ClearAvailableIndex(is_pc)

	def __ClearAvailableIndex(self, is_pc):
		for unit in (self.player_list if is_pc else self.enemy_list):
			unit.SetAvailableIndex(-1)

	def __CatchAniEndFrameEvent(self):
		if self.catch_ani:
			self.catch_ani.Hide()

	def __GoalCoverCheck(self):
		for i in xrange(2):
			if self.player_list[i].IsGoal():
				self.player_list[i].ShowGoalCoverImg()
			if self.enemy_list[i].IsGoal():
				self.enemy_list[i].ShowGoalCoverImg()

	def __MoveEnd(self, is_pc, index, goal_effect):
		if self.end_img:
			self.end_img.Show()

		# The walk is over.
		self.is_actionable = True

		self.__GoalCoverCheck()
		self.__RefreshJoinMemberPosition()
		self.__JoinCheck()

		# A piece of the player's home (and the game goes on): the effects,
		# then __TurnCheck.
		if True == is_pc and True == goal_effect and YUTNORI_STATE_END != self.yutnori_state:
			self.__ClearEffect()
			self.is_goal_text_effect = False
			self.goal_effect_end_frame_func = ui.__mem_func__(self.__TurnCheck)
			if self.goal_effect1:
				self.goal_effect1.Show()
			return

		# A catch: the caught piece walks home first, then this comes again.
		bCatch = self.__CatchCheck()
		if False == bCatch:
			self.__TurnCheck()

	def __RefreshJoinMemberPosition(self):
		if self.player_list[0].GetJoin():
			(x, y) = self.player_list[0].GetLocalPosition()
			self.player_list[1].SetPosition(x, y)

		if self.enemy_list[0].GetJoin():
			(x, y) = self.enemy_list[0].GetLocalPosition()
			self.enemy_list[1].SetPosition(x, y)

	def __JoinCheck(self):
		units = self.player_list if True == self.pc_turn else self.enemy_list
		if -1 != units[0].GetIndex() and units[0].GetIndex() == units[1].GetIndex():
			units[0].SetJoin()
			units[0].SetJoinMember(1)
			units[1].SetJoin()
			units[1].SetJoinMember(0)
			units[1].CharHide()
			units[1].DisableFlash()

	def __CatchCheck(self):
		bCatch = False

		catch_count = len(self.catch_deque)
		score_effect_create = True

		while len(self.catch_deque) > 0:
			(is_pc, index) = self.catch_deque.popleft()
			if index < 0 or index > 1:
				continue

			unit = self.player_list[index] if is_pc else self.enemy_list[index]
			(pos_sx, pos_sy) = unit.GetLocalPosition()
			(pos_ex, pos_ey) = unit.GetPos()
			catch_ani_pos = (pos_sx - 16, pos_sy - 16)
			unit.CatchPreProcess()
			unit.Start([ (pos_sx, pos_sy), (pos_ex, pos_ey) ])

			if is_pc:
				notice_str = L.NOTICE_11	# the computer caught the player's piece
			else:
				notice_str = L.NOTICE_10	# the player caught the computer's

			if True == score_effect_create:
				score_effect_create = False
				self.__CreateScoreEffect(not is_pc, YUTNORI_IN_DE_CREASE_SCORE * catch_count, pos_sx, pos_sy)

			if self.notice_text and notice_str:
				self.notice_text.SetText(notice_str)

			if self.catch_ani:
				self.catch_ani.SetPosition(catch_ani_pos[0], catch_ani_pos[1])
				self.catch_ani.ResetFrame()
				self.catch_ani.SetTop()
				self.catch_ani.Show()

			bCatch = True

		return bCatch

	def __TurnCheck(self):
		if YUTNORI_STATE_END == self.yutnori_state:
			if self.notice_text:
				self.notice_text.SetText(L.NOTICE_13)

			_DisableFlash(self.yut_throw_button)

			if self.reward_button:
				self.reward_button.Enable()
				_EnableFlash(self.reward_button)

			self.__ClearEffect()

			if True == self.next_turn:
				self.is_goal_text_effect = True
				self.goal_effect_end_frame_func = ui.__mem_func__(self.__PlayerWinPopup)
				if self.goal_effect1:
					self.goal_effect1.Show()
			else:
				for i in xrange(2):
					self.player_list[i].ShowExplosionEffect()

			return

		if self.next_turn != self.pc_turn:
			self.pc_turn = self.next_turn

		# The player's turn.
		if True == self.next_turn:
			if YUTNORI_AFTER_TURN_SELECT == self.yutnori_state:
				self.yutnori_state = YUTNORI_STATE_THROW
				self.event_deque.append([ EVENT_TYPE_INSER_DELAY, 1000 ])
				self.event_deque.append([ EVENT_TYPE_NOTICE, L.NOTICE_5 ])
				self.event_deque.append([ EVENT_TYPE_INSER_DELAY, 2000 ])
				self.event_deque.append([ EVENT_TYPE_CALL_TURN_CHECK, None ])

			elif YUTNORI_BEFORE_TURN_SELECT == self.yutnori_state:
				pass

			elif YUTNORI_STATE_THROW == self.yutnori_state:
				self.event_deque.append([ EVENT_TYPE_NOTICE, L.NOTICE_2 ])
				self.event_deque.append([ EVENT_TYPE_CHANGE_TEXT_COLOR, True ])
				self.event_deque.append([ EVENT_TYPE_BUTTON_FLASH, (self.yut_throw_button, True) ])

			elif YUTNORI_STATE_RE_THROW == self.yutnori_state:
				self.event_deque.append([ EVENT_TYPE_NOTICE, L.NOTICE_12 ])
				self.event_deque.append([ EVENT_TYPE_CHANGE_TEXT_COLOR, True ])
				self.event_deque.append([ EVENT_TYPE_BUTTON_FLASH, (self.yut_throw_button, True) ])
				self.__OpenReThrowPopup()

			elif YUTNORI_STATE_MOVE == self.yutnori_state:
				self.event_deque.append([ EVENT_TYPE_BUTTON_FLASH, (self.yut_throw_button, False) ])
				self.event_deque.append([ EVENT_TYPE_SHOW_UNIT, True ])

		# The computer's turn.
		else:
			if YUTNORI_AFTER_TURN_SELECT == self.yutnori_state:
				self.yutnori_state = YUTNORI_STATE_THROW
				self.event_deque.append([ EVENT_TYPE_INSER_DELAY, 1000 ])
				self.event_deque.append([ EVENT_TYPE_BUTTON_FLASH, (self.yut_throw_button, False) ])
				self.event_deque.append([ EVENT_TYPE_NOTICE, L.NOTICE_4 ])
				self.event_deque.append([ EVENT_TYPE_INSER_DELAY, 2000 ])
				self.event_deque.append([ EVENT_TYPE_CALL_TURN_CHECK, None ])

			elif YUTNORI_BEFORE_TURN_SELECT == self.yutnori_state:
				self.event_deque.append([ EVENT_TYPE_INSER_DELAY, 2000 ])
				self.event_deque.append([ EVENT_TYPE_BUTTON_FLASH, (self.yut_throw_button, False) ])
				self.event_deque.append([ EVENT_TYPE_NOTICE, L.NOTICE_3 ])
				self.event_deque.append([ EVENT_TYPE_CHANGE_TEXT_COLOR, False ])
				self.event_deque.append([ EVENT_TYPE_COM_YUT_THROW, None ])

			elif self.yutnori_state in [ YUTNORI_STATE_THROW, YUTNORI_STATE_RE_THROW ]:
				self.event_deque.append([ EVENT_TYPE_BUTTON_FLASH, (self.yut_throw_button, False) ])
				self.event_deque.append([ EVENT_TYPE_NOTICE, L.NOTICE_9 ])
				self.event_deque.append([ EVENT_TYPE_CHANGE_TEXT_COLOR, False ])
				self.event_deque.append([ EVENT_TYPE_COM_YUT_THROW, None ])

			elif YUTNORI_STATE_MOVE == self.yutnori_state:
				self.event_deque.append([ EVENT_TYPE_BUTTON_FLASH, (self.yut_throw_button, False) ])
				self.event_deque.append([ EVENT_TYPE_SHOW_UNIT, False ])
				self.event_deque.append([ EVENT_TYPE_REQUEST_COM_ACTION, None ])

	def PushNextTurn(self, data):
		(next_turn, state) = data
		self.next_turn = bool(next_turn)
		self.yutnori_state = state

	def PushCatchYut(self, data):
		self.catch_deque.append(data)

	def SetScore(self, score):
		self.before_score = self.score
		self.score = score

	def __RefreshScore(self):
		if not self.score_text:
			return

		if self.score < LOW_TOTAL_SCORE:
			self.score_text.SetPackedFontColor(TOTAL_SCORE_LOW_FONT_COLOR)
		elif self.score < MID_TOTAL_SCORE:
			self.score_text.SetPackedFontColor(TOTAL_SCORE_MID_FONT_COLOR)
		else:
			self.score_text.SetPackedFontColor(TOTAL_SCORE_HIGH_FONT_COLOR)

		self.score_text.SetText(str(self.score))

	# A throw costs the player 10 points.
	def __ThrowScoreCheck(self):
		if not self.yut_throw_button:
			return

		if False == self.pc_turn:
			return

		if self.before_score == self.score:
			return

		(s_x, s_y) = self.yut_throw_button.GetLocalPosition()
		self.__CreateScoreEffect(False, YUTNORI_IN_DE_CREASE_SCORE, s_x + 52, s_y)

	def __GoalScore(self, is_pc, is_join):
		score = YUTNORI_IN_DE_CREASE_SCORE
		if True == is_join:
			score = score * 2

		self.__CreateScoreEffect(True == is_pc, score, 283, 307)

	def SetRemainCount(self, remain_count):
		if not self.remain_count_text:
			return

		if remain_count < 6:
			self.remain_count_text.SetPackedFontColor(REMAIN_COUNT_LOW_FONT_COLOR)
		else:
			self.remain_count_text.SetPackedFontColor(REMAIN_COUNT_DEFAULT_FONT_COLOR)

		self.remain_count_text.SetText(str(remain_count))

	def __OpenReThrowPopup(self):
		if not self.re_throw_popup:
			self.re_throw_popup = uiCommon.PopupDialog()
			self.re_throw_popup.SetText(L.RETHROW_POPUP)
			self.re_throw_popup.SetAcceptEvent(ui.__mem_func__(self.__ClickThrowButton))
			self.re_throw_popup.SetButtonName(L.THROW)

		self.re_throw_popup.Open()

	def __PlayerWinPopup(self):
		if not self.pc_win_popup:
			self.pc_win_popup = uiCommon.PopupDialog()

		self.pc_win_popup.SetText(L.PC_WIN % self.score)
		self.pc_win_popup.Open()

	def __ComWinPopup(self):
		if not self.com_win_popup:
			self.com_win_popup = uiCommon.PopupDialog()
			self.com_win_popup.SetText(L.COM_WIN)

		self.com_win_popup.Open()

# ---------------------------------------------------------------- the window

class MiniGameYutnori(ui.Window):

	def __init__(self):
		ui.Window.__init__(self)
		self.isLoaded = 0
		self.state = STATE_NONE

		self.cur_page = None
		self.waiting_page = None
		self.game_page = None
		self.tooltip_item = None

		self.__LoadWindow()

	def __del__(self):
		ui.Window.__del__(self)

	def Show(self):
		self.__LoadWindow()
		ui.Window.Show(self)

	def Close(self):
		self.Hide()

		if self.cur_page:
			self.cur_page.Close()

	def Destroy(self):
		self.isLoaded = 0
		self.state = STATE_NONE
		self.cur_page = None

		if self.waiting_page:
			self.waiting_page.Close()
			self.waiting_page.Destroy()
			self.waiting_page = None

		if self.game_page:
			self.game_page.Close(True)
			self.game_page.Destroy()
			self.game_page = None

	def __LoadWindow(self):
		if self.isLoaded == 1:
			return

		self.isLoaded = 1
		self.state = STATE_WAITING

		try:
			self.waiting_page = YutnoriWaitingPage()
			self.game_page = YutnoriGamePage()
		except:
			uiminigameutil.LoadError("MiniGameYutnori.LoadWindow")

		if self.tooltip_item:
			self.waiting_page.SetItemToolTip(self.tooltip_item)

		self.Hide()

	def Open(self):
		self.__LoadWindow()

		if STATE_WAITING == self.state:
			self.cur_page = self.waiting_page
		elif STATE_PLAY == self.state:
			self.cur_page = self.game_page
		else:
			return

		if self.cur_page.IsShow():
			self.cur_page.Close()
		else:
			self.cur_page.Show()
			self.cur_page.SetTop()

	def YutnoriProcess(self, type, data):
		self.__LoadWindow()

		if not self.waiting_page or not self.game_page:
			return

		if 10 == type:
			if STATE_PLAY == self.state:
				self.game_page.SetYut()

		elif 0 == type:
			self.state = STATE_PLAY

			if self.waiting_page:
				self.waiting_page.DecreaseMiniGameYutBoardCount()
				self.waiting_page.Close()

			if self.game_page:
				self.game_page.Close(True)
				self.game_page.Destroy()

			self.cur_page = None
			self.Open()

		elif 1 == type:
			if STATE_PLAY == self.state:
				self.state = STATE_WAITING
				self.game_page.Close(True)
				self.game_page.Destroy()
				self.cur_page = None

		elif 2 == type:
			if STATE_PLAY == self.state:
				self.game_page.SetProb(data)

		elif 3 == type:
			if STATE_PLAY == self.state:
				self.game_page.ThrowResult(data)

		elif 4 == type:
			if STATE_PLAY == self.state:
				self.game_page.YutMove(data)

		elif 5 == type:
			if STATE_PLAY == self.state:
				self.game_page.AvailableAreaShow(data)

		elif 6 == type:
			if STATE_PLAY == self.state:
				self.game_page.PushCatchYut(data)

		elif 7 == type:
			if STATE_PLAY == self.state:
				self.game_page.SetScore(data)

		elif 8 == type:
			if STATE_PLAY == self.state:
				self.game_page.SetRemainCount(data)

		elif 9 == type:
			if STATE_PLAY == self.state:
				self.game_page.PushNextTurn(data)

	def SetItemToolTip(self, tooltip):
		self.tooltip_item = tooltip
		if self.waiting_page:
			self.waiting_page.SetItemToolTip(tooltip)

	def YutnoriFlagProcess(self, type, data):
		if FLAG_NO_MORE_GAIN == type:
			chat.AppendChat(chat.CHAT_TYPE_INFO, L.NO_MORE_GAIN)
		if self.waiting_page:
			self.waiting_page.YutnoriFlagProcess(type, data)

# ---------------------------------------------------------------- the module's face

_data = {'window': None, 'tooltip': None, 'created': False}

def _CreateThrower():
	# The exe's 3D thrower (race 20505) and its render target, once a game window.
	if not _data['created'] and hasattr(app, "YutnoriCreate"):
		app.YutnoriCreate()
		_data['created'] = True

def _ItemToolTip():
	if not _data['tooltip']:
		_data['tooltip'] = uiToolTip.ItemToolTip()
		_data['tooltip'].HideToolTip()
	return _data['tooltip']

def GetWindow():
	if not _data['window']:
		window = uiminigameutil.SafeCreate(MiniGameYutnori, "Yutnori")
		if window:
			window.SetItemToolTip(_ItemToolTip())
		_data['window'] = window
	return _data['window']

def OpenWindow():
	"""The hub's button, the table NPC's "Zagraj" (YutnoriOpen)."""
	if not IsSupported():
		chat.AppendChat(chat.CHAT_TYPE_INFO, L.NEEDS_CLIENT)
		return
	_CreateThrower()
	window = GetWindow()
	if window:
		window.Open()

def Process(type, data):
	"""game.py's YutnoriProcess (the exe, packet 182)."""
	_CreateThrower()
	window = GetWindow()
	if window:
		window.YutnoriProcess(type, data)

def FlagProcess(type, data):
	"""game.py's YutnoriFlagProcess (the exe, packet 182's flags)."""
	window = GetWindow()
	if window:
		window.YutnoriFlagProcess(type, data)

def Register():
	"""From game.py when the hub starts: the hub's Yut Nori button opens this."""
	try:
		import uiingameevent
		uiingameevent.RegisterOpener('yutnori', OpenWindow)
	except ImportError:
		pass
	_CreateThrower()

def Destroy():
	"""From game.py's Close."""
	window = _data['window']
	if window:
		window.Destroy()
	_data['window'] = None
	if _data['tooltip']:
		_data['tooltip'].HideToolTip()
	_data['tooltip'] = None
	_data['created'] = False
	try:
		import uiingameevent
		uiingameevent.UnregisterOpener('yutnori')
	except ImportError:
		pass
