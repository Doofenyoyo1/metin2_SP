# MT2009_PLUS_RUMI_V1 - Owsap's Rumi (Okey card game) window, after his
# uiminigamerumi.py (v6.2.6, __OKEY_EVENT_FLAG_RENEWAL__): the waiting page
# (the rules, the card and card set counters, Start) and the game page (the
# deck, five hand cards, three field cards, the score).
#
# The game is the server's (playerbot_rumi.h): this window only shows what the
# packets say. The exe (client-patches/exe) brings the packets - Owsap's names:
#   net.SendMiniGameRumiStart / Exit / DeckCardClick / HandCardClick(use, i) /
#   FieldCardClick(i) / RequestQuestFlag
# and calls the game window (game.py relays to the functions at the end):
#   MiniGameRumiStart, MiniGameRumiEnd, MiniGameRumiSetDeckCount(n),
#   MiniGameRumiIncreaseScore(score, total),
#   MiniGameRumiMoveCard(srcPos, srcIndex, srcColor, srcNumber, dstPos, dstIndex, dstColor, dstNumber),
#   MiniGameRumiFlagProcess(type, (cardPieces, cardSets)).
# An exe without them: the event's button says the game comes with an update.
#
# What Owsap's ui.py and exe had and ours has not is done here in python:
# the moving card (ui.MoveImageBox), the frame animations with key frames and
# a reset (AniImageBox.ResetFrame / SetKeyFrameEvent / SetScale) and the card
# slots (a SlotWindow drawn from grpImage pointers): every card is an image
# box of its own. The texts are Polish (CP1250 escapes), a localeInfo key of
# Owsap's name wins when the locale has it.
#
# Python 2.7 as the client has it.

import app
import chat
import event
import grp
import localeInfo
import net
import player
import ui
import uiCommon
import wndMgr
import uiminigameutil

from collections import deque

try:
	import ingameevent
except ImportError:
	ingameevent = None

STATE_NONE = 0
STATE_WAITING = 1
STATE_PLAY = 2

DESC_WIDTH_COUNT = 50
SHOW_LINE_COUNT_MAX = 9
DEFAULT_DESC_Y = 120

NONE_POS = 0
DECK_CARD = 1
HAND_CARD = 2
FIELD_CARD = 3
CARD_POS_MAX = 4

DECK_CARD_INDEX_MAX = 3
HAND_CARD_INDEX_MAX = 5
FIELD_CARD_INDEX_MAX = 3

EMPTY_CARD = 0
RED_CARD = 10
BLUE_CARD = 20
YELLOW_CARD = 30

CARD_COLOR_MAX = 3
CARD_NUMBER_END = 8
DECK_COUNT_MAX = CARD_COLOR_MAX * CARD_NUMBER_END

CARD_IMG_WIDTH = 38
CARD_IMG_HEIGHT = 52
HAND_CARD_GAP = 21
FIELD_CARD_GAP = 5

# Owsap's MoveImageBox speed, pixels a frame at 60 frames a second.
CARD_MOVE_SPEED = 25.0 * 60.0

DECK_FLUSH_IMG_GAP_X = 2
DECK_FLUSH_IMG_GAP_Y = 2

TOTAL_SCORE_LOW_FONT_COLOR = grp.GenerateColor(0.78, 0.78, 0.78, 1.0)
TOTAL_SCORE_MID_FONT_COLOR = grp.GenerateColor(1.0, 0.85, 0.39, 1.0)
TOTAL_SCORE_HIGH_FONT_COLOR = grp.GenerateColor(1.0, 0.0, 0.0, 1.0)

LOW_TOTAL_SCORE = 300
MID_TOTAL_SCORE = 400

RUMI_START_GOLD = 30000
RUMI_CARD_PIECE_COUNT_MAX = 24
RUMI_CARD_COUNT_MAX = 999

ITEM_VNUM_RUMI_CARD_PIECE = 79505
ITEM_VNUM_RUMI_CARD_PACK = 79506

# The flag sub-headers (Owsap's player.RUMI_GC_SUBHEADER_*), the exe's if it
# exports them.
RUMI_GC_SUBHEADER_SET_CARD_PIECE_FLAG = getattr(player, 'RUMI_GC_SUBHEADER_SET_CARD_PIECE_FLAG', 5)
RUMI_GC_SUBHEADER_SET_CARD_FLAG = getattr(player, 'RUMI_GC_SUBHEADER_SET_CARD_FLAG', 6)
RUMI_GC_SUBHEADER_SET_QUEST_FLAG = getattr(player, 'RUMI_GC_SUBHEADER_SET_QUEST_FLAG', 7)
RUMI_GC_SUBHEADER_NO_MORE_GAIN = getattr(player, 'RUMI_GC_SUBHEADER_NO_MORE_GAIN', 8)

RUMI_ROOT = "d:/ymir work/ui/minigame/rumi/"
CARD_ROOT = RUMI_ROOT + "card/"
EFFECT_ROOT = RUMI_ROOT + "card_completion_effect/"
NORMAL_BG = "d:/ymir work/ui/minigame/rumi_nor/rumi_nor_bg.tga"

CARD_IMG_DICT = {}
for _color, _name in ((RED_CARD, 'red'), (BLUE_CARD, 'blue'), (YELLOW_CARD, 'yellow')):
	for _number in xrange(1, CARD_NUMBER_END + 1):
		CARD_IMG_DICT[_color + _number] = CARD_ROOT + 'card_%s_%d.sub' % (_name, _number)

DECK_IMG_LIST = [RUMI_ROOT + "deck/deck1.sub", RUMI_ROOT + "deck/deck2.sub", RUMI_ROOT + "deck/deck3.sub"]
# The deck's three stack images lie one over another, 2 pixels apart.
DECK_SLOT_OFFSET = [(4, 4), (2, 2), (0, 0)]

SCORE_EFFECT_FRAMES = [EFFECT_ROOT + "card_completion_eff%d.sub" % i for i in xrange(1, 9)]
SCORE_TEXT_FRAMES = [EFFECT_ROOT + "card_completion_text_effect%d.sub" % i for i in (1, 5, 5, 5, 5, 5, 6, 6, 7, 8, 9)]
DECK_FLUSH_FRAMES = [RUMI_ROOT + "deck/deck_flash/deck_card_flush%d.sub" % i for i in (2, 3, 4, 5, 4, 3, 2, 1)]

# --------------------------------------------------------------- the texts

def _T(key, default):
	return getattr(localeInfo, key, default)

TITLE = _T('MINI_GAME_RUMI_TITLE', 'Karcianka Okey')
TEXT_START = _T('MINI_GAME_RUMI_START_TEXT', 'Start')
TEXT_EXIT = _T('MINI_GAME_RUMI_EXIT', 'Zako\xf1cz')
TEXT_SCORE = _T('MINI_GAME_RUMI_SCORE', 'Punkty')
TEXT_SAFE_MODE = _T('MINI_GAME_RUMI_DISCARD_TEXT', 'Tryb bezpieczny')
TEXT_LBUTTON = _T('MINI_GAME_RUMI_LBUTTON_DESC', 'Dodaj kart\xea klikni\xeaciem LPM')
TEXT_RBUTTON = _T('MINI_GAME_RUMI_RBUTTON_DESC', 'Usu\xf1 kart\xea klikni\xeaciem PPM')
QUESTION_DISCARD = _T('MINI_GAME_RUMI_DISCARD_QUESTION', 'Na pewno chcesz wyrzuci\xe6 t\xea kart\xea?')
QUESTION_EXIT_1 = 'Chcesz zako\xf1czy\xe6 gr\xea i otrzyma\xe6 wygran\xb9?'
QUESTION_EXIT_2 = 'Twoje punkty i pozosta\xb3e karty zostan\xb9 zresetowane.'
QUESTION_START_1 = 'Gra kosztuje %s Yang i %d zestaw kart.'
QUESTION_START_2 = 'Chcesz zacz\xb9\xe6 gr\xea?'
MSG_NOT_ENOUGH = _T('OKEY_EVENT_MESSAGE_NOT_ENOUGHT_ITEM', 'Masz za ma\xb3o przedmiot\xf3w lub pieni\xeadzy, aby gra\xe6.')
MSG_PIECE_GAIN = _T('OKEY_EVENT_MESSAGE_CARD_PEICE_GAIN', 'Otrzymujesz karty Okey.')
MSG_CARD_GAIN = _T('OKEY_EVENT_MESSAGE_CARD_GAIN', 'Otrzymujesz zestaw (zestawy) kart Okey.')
MSG_NO_MORE_GAIN = _T('OKEY_EVENT_MESSAGE_NO_MORE_GAIN', 'Nie mo\xbfesz otrzyma\xe6 kolejnych zestaw\xf3w kart Okey.')
MSG_NO_EXE = 'Rumi (Okey): okno gry pojawi si\xea z aktualizacj\xb9 klienta.'

# The uiscripts' texts: ui.PythonScriptLoader's sandbox lets a uiscript import
# only uiScriptLocale, localeInfo and a few exe modules (an "import
# uiminigamerumi" there gives None and the load fails), so the texts go on
# uiScriptLocale under Owsap's names before any page is loaded.
import uiScriptLocale
for _key, _val in (('MINI_GAME_RUMI_TITLE', TITLE), ('MINI_GAME_RUMI_START_TEXT', TEXT_START),
		('MINI_GAME_RUMI_EXIT', TEXT_EXIT), ('MINI_GAME_RUMI_SCORE', TEXT_SCORE),
		('MINI_GAME_RUMI_DISCARD_TEXT', TEXT_SAFE_MODE), ('MINI_GAME_RUMI_LBUTTON_DESC', TEXT_LBUTTON),
		('MINI_GAME_RUMI_RBUTTON_DESC', TEXT_RBUTTON)):
	if not hasattr(uiScriptLocale, _key):
		setattr(uiScriptLocale, _key, _val)


def DescFile():
	return "%s/mini_game_okey_desc.txt" % app.GetLocalePath()


def HasExe():
	return hasattr(net, 'SendMiniGameRumiStart')


def IsNormal():
	"""Owsap's player.GetMiniGameOkeyNormal: the scheduler's Rumi (not the
	Christmas flag) - the plain table cloth."""
	if hasattr(player, 'GetMiniGameOkeyNormal'):
		try:
			return player.GetMiniGameOkeyNormal()
		except Exception:
			pass
	if ingameevent:
		return ingameevent.IsActive('rumi') or not ingameevent.IsActive('rumi_xmas')
	return True


def LoadScript(self, fileName):
	pyScrLoader = ui.PythonScriptLoader()
	pyScrLoader.LoadScriptFile(self, fileName)


# --------------------------------------------------------- the python shims

class FrameAnimation(ui.ExpandedImageBox):
	"""Owsap's AniImageBox with ResetFrame, SetEndFrameEvent, SetKeyFrameEvent
	and SetScale: frames switched in OnUpdate, delay counted in updates (as
	the exe's AniImageBox counts them)."""

	def __init__(self, frames, delay):
		ui.ExpandedImageBox.__init__(self)
		self.frames = list(frames)
		self.delay = delay
		self.counter = 0
		self.frame = 0
		self.endEvent = None
		self.keyEvent = None
		self.AddFlag('not_pick')
		self.__Load()
		# our exe calls no python OnUpdate on an image box (uiminigameutil)
		self.ticker = uiminigameutil.UpdateTicker(self)

	def __del__(self):
		ui.ExpandedImageBox.__del__(self)

	def Destroy(self):
		self.endEvent = None
		self.keyEvent = None

	def __Load(self):
		if self.frames:
			self.LoadImage(self.frames[self.frame])

	def SetDelay(self, delay):
		self.delay = delay

	def ResetFrame(self):
		self.frame = 0
		self.counter = 0
		self.__Load()

	def SetEndFrameEvent(self, func):
		self.endEvent = func

	def SetKeyFrameEvent(self, func):
		self.keyEvent = func

	def TickUpdate(self):
		if not self.frames:
			return
		self.counter += 1
		if self.counter < self.delay:
			return
		self.counter = 0
		self.frame += 1
		if self.frame >= len(self.frames):
			self.frame = 0
			self.__Load()
			if self.endEvent:
				self.endEvent()
			return
		self.__Load()
		if self.keyEvent:
			self.keyEvent(self.frame)


class MovingCard(ui.ImageBox):
	"""Owsap's MoveImageBox: an image that flies to a point of its parent and
	says when it is there."""

	def __init__(self):
		ui.ImageBox.__init__(self)
		self.AddFlag('not_pick')
		self.moving = False
		self.target = (0, 0)
		self.pos = (0.0, 0.0)
		self.lastTime = 0.0
		self.endEvent = None
		# our exe calls no python OnUpdate on an image box (uiminigameutil)
		self.ticker = uiminigameutil.UpdateTicker(self)

	def __del__(self):
		ui.ImageBox.__del__(self)

	def Destroy(self):
		self.endEvent = None

	def SetEndMoveEvent(self, func):
		self.endEvent = func

	def GetMove(self):
		return self.moving

	def MoveStart(self, x, y, dstX, dstY):
		self.pos = (float(x), float(y))
		self.target = (dstX, dstY)
		self.SetPosition(x, y)
		self.lastTime = app.GetTime()
		self.moving = True
		self.Show()

	def SetMovePosition(self, dstX, dstY):
		self.target = (dstX, dstY)

	def TickUpdate(self):
		if not self.moving:
			return
		now = app.GetTime()
		step = max(0.0, now - self.lastTime) * CARD_MOVE_SPEED
		self.lastTime = now
		x, y = self.pos
		tx, ty = self.target
		dx = tx - x
		dy = ty - y
		dist = (dx * dx + dy * dy) ** 0.5
		if dist <= step or dist < 1.0:
			self.pos = (float(tx), float(ty))
			self.SetPosition(int(tx), int(ty))
			self.moving = False
			if self.endEvent:
				self.endEvent()
			return
		x += dx * step / dist
		y += dy * step / dist
		self.pos = (x, y)
		self.SetPosition(int(x), int(y))


class CardBox(ui.ImageBox):
	"""One card place (deck, hand or field): an image when it holds a card,
	the mouse to the page."""

	def __init__(self, page, pos, index):
		ui.ImageBox.__init__(self)
		self.page = page
		self.pos = pos
		self.index = index
		self.card = 0

	def __del__(self):
		ui.ImageBox.__del__(self)

	def Destroy(self):
		self.page = None

	def SetCard(self, card):
		self.card = card
		image = CARD_IMG_DICT.get(card)
		if image:
			self.LoadImage(image)
			self.Show()
		else:
			self.card = 0
			self.Hide()

	def Clear(self):
		self.card = 0
		self.Hide()

	def HasCard(self):
		return self.card != 0

	def OnMouseLeftButtonUp(self):
		if self.page:
			self.page.OnCardClick(self.pos, self.index, True)
		return True

	def OnMouseRightButtonUp(self):
		if self.page:
			self.page.OnCardClick(self.pos, self.index, False)
		return True


class DeckBox(ui.ImageBox):
	def __init__(self, page):
		ui.ImageBox.__init__(self)
		self.page = page

	def __del__(self):
		ui.ImageBox.__del__(self)

	def Destroy(self):
		self.page = None

	def OnMouseLeftButtonUp(self):
		if self.page:
			self.page.LButtonClickDeck()
		return True


# -------------------------------------------------------- the waiting page

class RumiWaitingPage(ui.ScriptWindow):
	def __init__(self):
		ui.ScriptWindow.__init__(self)

		self.isLoaded = 0

		self.startButton = None
		self.desc_board = None
		self.description_box = None

		self.desc_index = -1
		self.desc_y = DEFAULT_DESC_Y

		self.start_question_dialog = None

		self.confirm_window_check_button = None
		self.check_image = None

		self.prev_button = None
		self.next_button = None

		self.is_data_requested = False

		self.rumi_card_piece_count = 0
		self.rumi_card_piece_slot = None
		self.rumi_card_piece_count_text = None

		self.rumi_card_count = 0
		self.rumi_card_slot = None
		self.rumi_card_count_text = None

		self.__LoadWindow()

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def __LoadWindow(self):
		if self.isLoaded == 1:
			return

		self.isLoaded = 1

		try:
			LoadScript(self, "UIScript/MiniGameRumiWaitingPage.py")
		except:
			uiminigameutil.LoadError("MiniGameRumiWaitingPage.LoadWindow.LoadObject")

		try:
			self.GetChild("board").SetCloseEvent(ui.__mem_func__(self.Close))

			self.startButton = self.GetChild("game_start_button")
			self.startButton.SetEvent(ui.__mem_func__(self.__ClickStartButton))

			self.desc_board = self.GetChild("desc_board")
			# MT2009_PLUS_MINIGAME_DESC_V1: the rules as TextLines in the box
			self.description_box = uiminigameutil.DescriptionText(self.desc_board, 7, DEFAULT_DESC_Y,
				self.desc_board.GetWidth() - 14, self.desc_board.GetHeight() - DEFAULT_DESC_Y,
				SHOW_LINE_COUNT_MAX, 18)
			self.description_box.Show()

			self.confirm_window_check_button = self.GetChild("confirm_check_button")
			self.confirm_window_check_button.SetEvent(ui.__mem_func__(self.__ClickConfirmCheckButton), "mouse_click", 0)

			self.check_image = self.GetChild("check_image")
			self.check_image.Show()

			self.prev_button = self.GetChild("prev_button")
			self.prev_button.SetEvent(ui.__mem_func__(self.__ClickPrevButton))

			self.next_button = self.GetChild("next_button")
			self.next_button.SetEvent(ui.__mem_func__(self.__ClickNextButton))

			self.rumi_card_piece_slot = self.GetChild("rumi_card_piece_slot")
			self.rumi_card_piece_slot.SetOverInItemEvent(ui.__mem_func__(self.__SlotOverInPiece))
			self.rumi_card_piece_slot.SetOverOutItemEvent(ui.__mem_func__(self.__SlotOverOutItem))
			self.rumi_card_piece_count_text = self.GetChild("rumi_card_piece_count_text")

			self.rumi_card_slot = self.GetChild("rumi_card_slot")
			self.rumi_card_slot.SetOverInItemEvent(ui.__mem_func__(self.__SlotOverInPack))
			self.rumi_card_slot.SetOverOutItemEvent(ui.__mem_func__(self.__SlotOverOutItem))
			self.rumi_card_count_text = self.GetChild("rumi_card_count_text")
			self.__RefreshCounters()
		except:
			uiminigameutil.LoadError("MiniGameRumiWaitingPage.LoadWindow.BindObject")

		self.Hide()

	def Close(self):
		self.Hide()

		self.CloseStartDlg()

		if self.description_box:
			self.description_box.Hide()
		_HideToolTip()

	def Destroy(self):
		self.Close()
		if self.description_box:
			self.description_box.Destroy()
		self.isLoaded = 0

		self.startButton = None
		self.desc_board = None
		self.description_box = None

		self.confirm_window_check_button = None
		self.check_image = None

		self.prev_button = None
		self.next_button = None

		self.is_data_requested = False

		self.rumi_card_piece_slot = None
		self.rumi_card_piece_count_text = None
		self.rumi_card_slot = None
		self.rumi_card_count_text = None
		self.ClearDictionary()

	def OnPressEscapeKey(self):
		self.Close()
		return True

	def GetConfirmWindowCheck(self):
		if self.check_image:
			return self.check_image.IsShow()
		return False

	def __ClickConfirmCheckButton(self, *args):
		if self.check_image:
			if self.check_image.IsShow():
				self.check_image.Hide()
			else:
				self.check_image.Show()

	def __ClickStartButton(self):
		if self.rumi_card_count == 0 or player.GetElk() < RUMI_START_GOLD:
			chat.AppendChat(chat.CHAT_TYPE_INFO, MSG_NOT_ENOUGH)
			return

		if None == self.start_question_dialog:
			dialog = uiCommon.QuestionDialog2()
			dialog.SetText1(QUESTION_START_1 % (localeInfo.NumberToString(RUMI_START_GOLD) if hasattr(localeInfo, 'NumberToString') else str(RUMI_START_GOLD), 1))
			dialog.SetText2(QUESTION_START_2)
			dialog.SetAcceptEvent(ui.__mem_func__(self.__StartAccept))
			dialog.SetCancelEvent(ui.__mem_func__(self.__StartCancel))
			self.start_question_dialog = dialog

		self.start_question_dialog.Open()

	def CloseStartDlg(self):
		if self.start_question_dialog:
			self.start_question_dialog.Close()
			self.start_question_dialog = None

	def __StartAccept(self):
		if self.start_question_dialog:
			self.start_question_dialog.Close()
		net.SendMiniGameRumiStart()

	def __StartCancel(self):
		if self.start_question_dialog:
			self.start_question_dialog.Close()

	def Show(self):
		if not self.is_data_requested:
			net.SendMiniGameRumiRequestQuestFlag()
			self.is_data_requested = True

		ui.ScriptWindow.Show(self)

		if self.description_box:
			self.description_box.LoadFile(DescFile())
			self.description_box.Show()

		if self.check_image:
			self.check_image.Show()

		self.rumi_card_piece_slot.SetItemSlot(0, ITEM_VNUM_RUMI_CARD_PIECE, 0)
		self.rumi_card_piece_slot.RefreshSlot()
		self.rumi_card_slot.SetItemSlot(0, ITEM_VNUM_RUMI_CARD_PACK, 0)
		self.rumi_card_slot.RefreshSlot()

	def __ClickPrevButton(self):
		if self.description_box:
			self.description_box.PrevPage()

	def __ClickNextButton(self):
		if self.description_box:
			self.description_box.NextPage()

	def __SlotOverInPiece(self, slot_index):
		_ShowItemToolTip(ITEM_VNUM_RUMI_CARD_PIECE)

	def __SlotOverInPack(self, slot_index):
		_ShowItemToolTip(ITEM_VNUM_RUMI_CARD_PACK)

	def __SlotOverOutItem(self):
		_HideToolTip()

	def __RefreshCounters(self):
		if self.rumi_card_piece_count_text:
			self.rumi_card_piece_count_text.SetText("%d/%d" % (self.rumi_card_piece_count, RUMI_CARD_PIECE_COUNT_MAX))
		if self.rumi_card_count_text:
			self.rumi_card_count_text.SetText("%d/%d" % (self.rumi_card_count, RUMI_CARD_COUNT_MAX))

	def DecreaseMiniGameRumiCardCount(self):
		self.rumi_card_count = max(0, self.rumi_card_count - 1)
		self.__RefreshCounters()

	def MiniGameRumiFlagProcess(self, process_type, data):
		(self.rumi_card_piece_count, self.rumi_card_count) = data
		self.__RefreshCounters()


# ----------------------------------------------------------- the game page

class RumiGamePage(ui.ScriptWindow):
	def __init__(self):
		ui.ScriptWindow.__init__(self)

		self.isLoaded = 0
		self.board = None
		self.slot_windows = {}
		self.cards = {DECK_CARD: [], HAND_CARD: [], FIELD_CARD: []}
		self.deck_box = None

		self.cur_score_text = None
		self.cur_score = 0
		self.total_score_text = None
		self.total_score = 0

		self.deck_card_cnt_text = None
		self.deck_card_cnt = 0

		self.hand_card_cnt = 0
		self.field_card_cnt = 0

		self.score_text_effect = None
		self.score_effect1 = None
		self.score_effect2 = None
		self.score_effect3 = None
		self.deck_flush_effect = None
		self.lock = False
		self.clear_field_after_moves = False

		self.move_img = None
		self.deck_cur_index = DECK_CARD_INDEX_MAX - 1

		self.card_move_queue = deque()

		self.exit_question_dialog = None

		self.confirm_window_on = True
		self.discard_confirm_dialog = None
		self.discard_index = -1

		self.back_ground = None

		self.__LoadWindow()

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def __LoadWindow(self):
		if self.isLoaded == 1:
			return

		self.isLoaded = 1

		try:
			LoadScript(self, "UIScript/MiniGameRumiGamePage.py")
		except:
			uiminigameutil.LoadError("MiniGameRumiGamePage.LoadWindow.LoadObject")

		try:
			self.board = self.GetChild("board")
			self.GetChild("titlebar").SetCloseEvent(ui.__mem_func__(self.Close))
			self.GetChild("game_exit_button").SetEvent(ui.__mem_func__(self.__ExitButtonClick))
			self.cur_score_text = self.GetChild("score_number_text")
			self.cur_score_text.SetText("")
			self.total_score_text = self.GetChild("total_score")
			self.total_score_text.SetText("0")
			self.deck_card_cnt_text = self.GetChild("card_cnt_text")

			## the normal event's table cloth over the Christmas one
			self.back_ground = ui.ExpandedImageBox()
			self.back_ground.SetParent(self.GetChild("BG"))
			self.back_ground.AddFlag('not_pick')
			self.back_ground.LoadImage(NORMAL_BG)
			self.back_ground.SetPosition(0, 0)
			self.back_ground.Hide()

			## the card places
			self.slot_windows[DECK_CARD] = self.GetChild("DeckCardSlot")
			self.slot_windows[HAND_CARD] = self.GetChild("HandCardSlot")
			self.slot_windows[FIELD_CARD] = self.GetChild("FieldCardSlot")

			deck_box = DeckBox(self)
			deck_box.SetParent(self.board)
			deck_box.Hide()
			self.deck_box = deck_box

			for index in xrange(HAND_CARD_INDEX_MAX):
				self.cards[HAND_CARD].append(self.__MakeCard(HAND_CARD, index))
			for index in xrange(FIELD_CARD_INDEX_MAX):
				self.cards[FIELD_CARD].append(self.__MakeCard(FIELD_CARD, index))

			## score completion text effect
			self.score_text_effect = self.__MakeAnimation(SCORE_TEXT_FRAMES, 0, 91, 133)
			self.score_text_effect.SetEndFrameEvent(ui.__mem_func__(self.__ScoreTextEffectEndFrameEvent))

			## score completion side effect
			self.score_effect1 = self.__MakeAnimation(SCORE_EFFECT_FRAMES, 6, 57, 100)
			self.score_effect2 = self.__MakeAnimation(SCORE_EFFECT_FRAMES, 6, 100, 100)
			self.score_effect3 = self.__MakeAnimation(SCORE_EFFECT_FRAMES, 6, 143, 100)
			for effect in (self.score_effect1, self.score_effect2, self.score_effect3):
				effect.SetScale(1.2, 1.2)
			self.score_effect1.SetEndFrameEvent(ui.__mem_func__(self.__ScoreEffectEndFrameEvent1))
			self.score_effect2.SetEndFrameEvent(ui.__mem_func__(self.__ScoreEffectEndFrameEvent2))
			self.score_effect3.SetEndFrameEvent(ui.__mem_func__(self.__ScoreEffectEndFrameEvent3))
			self.score_effect1.SetKeyFrameEvent(ui.__mem_func__(self.__ScoreEffectKeyFrameEvent1))
			self.score_effect2.SetKeyFrameEvent(ui.__mem_func__(self.__ScoreEffectKeyFrameEvent2))
			self.__ClearScoreCompletionEffect()

			## deck flush effect
			self.deck_flush_effect = self.__MakeAnimation(DECK_FLUSH_FRAMES, 8, 46, 234)
			self.__ClearDeckFlushEffect()

			## the moving card
			self.move_img = MovingCard()
			self.move_img.SetParent(self.board)
			self.move_img.SetEndMoveEvent(ui.__mem_func__(self.CardMoveEndEvnet))
			self.move_img.Hide()
		except:
			uiminigameutil.LoadError("MiniGameRumiGamePage.LoadWindow.BindObject")

		self.Hide()

	def __MakeCard(self, pos, index):
		card = CardBox(self, pos, index)
		card.SetParent(self.board)
		(x, y) = self.__SlotLocalPosition(pos, index)
		card.SetPosition(x, y)
		card.Hide()
		return card

	def __MakeAnimation(self, frames, delay, x, y):
		animation = FrameAnimation(frames, delay)
		animation.SetParent(self.board)
		animation.SetPosition(x, y)
		animation.Hide()
		return animation

	def __SlotLocalPosition(self, pos, index):
		"""A card place's position on the board (Owsap's slot coordinates)."""
		window = self.slot_windows.get(pos)
		if not window:
			return (0, 0)
		(x, y) = window.GetLocalPosition()
		if pos == HAND_CARD:
			return (x + index * (CARD_IMG_WIDTH + HAND_CARD_GAP), y)
		if pos == FIELD_CARD:
			return (x + index * (CARD_IMG_WIDTH + FIELD_CARD_GAP), y)
		if pos == DECK_CARD:
			(dx, dy) = DECK_SLOT_OFFSET[max(0, min(DECK_CARD_INDEX_MAX - 1, index))]
			return (x + dx, y + dy)
		return (x, y)

	def Destroy(self):
		self.__CloseExitQuestionDialog()
		if self.discard_confirm_dialog:
			self.discard_confirm_dialog.Close()
		self.discard_confirm_dialog = None
		self.card_move_queue.clear()
		for key in self.cards:
			for card in self.cards[key]:
				card.Destroy()
		self.cards = {DECK_CARD: [], HAND_CARD: [], FIELD_CARD: []}
		for effect in (self.score_text_effect, self.score_effect1, self.score_effect2, self.score_effect3, self.deck_flush_effect):
			if effect:
				effect.Destroy()
		if self.deck_box:
			self.deck_box.Destroy()
		if self.move_img:
			self.move_img.Destroy()
		self.score_text_effect = None
		self.score_effect1 = None
		self.score_effect2 = None
		self.score_effect3 = None
		self.deck_flush_effect = None
		self.deck_box = None
		self.move_img = None
		self.back_ground = None
		self.board = None
		self.slot_windows = {}
		self.isLoaded = 0
		self.ClearDictionary()

	def OnPressEscapeKey(self):
		self.Close()
		return True

	def SetConfirmWindowCheck(self, bFlag):
		self.confirm_window_on = bFlag

	LOCK_TIMEOUT = 6.0

	def OnUpdate(self):
		self.__DeckFlushEffectCheck()

		if len(self.card_move_queue) > 0 and self.move_img and not self.move_img.GetMove():
			self.CardMoveStartEvent()

		self.__LockFallback()

	def __LockFallback(self):
		# MT2009: never leave the cards locked. The moves and the score
		# effects end the lock; if their end does not come (an animation that
		# does not run), everything owed is done at once after LOCK_TIMEOUT.
		if not self.lock:
			self.lock_since = 0.0
			return
		now = app.GetTime()
		if not getattr(self, 'lock_since', 0.0):
			self.lock_since = now
			return
		if now - self.lock_since < self.LOCK_TIMEOUT:
			return
		import dbg
		dbg.TraceError("Rumi: the cards stayed locked - going on")
		self.lock_since = 0.0
		if self.move_img:
			self.move_img.moving = False
		while len(self.card_move_queue) > 0:
			self.CardMoveEndEvnet()
		if self.__EffectShown():
			self.__ClearScoreCompletionEffect()
			self.__SetScore(0)
			self.__ClearFieldCardSlot()
		self.clear_field_after_moves = False
		self.lock = False

	def CardMoveStartEvent(self):
		if len(self.card_move_queue) == 0:
			return
		(src_pos, src_index, src_color, src_number) = self.card_move_queue[0][0]
		(dst_pos, dst_index, dst_color, dst_number) = self.card_move_queue[0][1]

		if DECK_CARD != src_pos:
			self.cards[src_pos][src_index].Clear()

		image = CARD_IMG_DICT.get(dst_color + dst_number)
		if image:
			self.move_img.LoadImage(image)

		if src_pos == DECK_CARD:
			src_index = self.deck_cur_index
		(src_x, src_y) = self.__SlotLocalPosition(src_pos, src_index)
		(dst_x, dst_y) = self.__SlotLocalPosition(dst_pos, dst_index)

		if DECK_CARD == src_pos and HAND_CARD == dst_pos:
			self.SetDeckCount(self.deck_card_cnt - 1)

		self.move_img.SetTop()
		self.move_img.MoveStart(src_x, src_y, dst_x, dst_y)

	def CardMoveEndEvnet(self):
		if len(self.card_move_queue) > 0:
			[srcCard, dstCard] = self.card_move_queue.popleft()
			(dst_pos, dst_index, dst_color, dst_number) = dstCard
			self.cards[dst_pos][dst_index].SetCard(dst_color + dst_number)

		self.move_img.Hide()

		if len(self.card_move_queue) == 0:
			if self.clear_field_after_moves:
				self.clear_field_after_moves = False
				self.__ClearFieldCardSlot()
			if not self.__EffectShown():
				self.lock = False

	def __EffectShown(self):
		for effect in (self.score_text_effect, self.score_effect1, self.score_effect2, self.score_effect3):
			if effect and effect.IsShow():
				return True
		return False

	def Clear(self):
		self.card_move_queue.clear()
		if self.move_img:
			self.move_img.moving = False
			self.move_img.Hide()
		self.__ClearHandCardSlot()
		self.__ClearFieldCardSlot()

		self.__SetScore(0)
		self.__SetTotalScore(0)

		self.lock = False
		self.clear_field_after_moves = False

		self.confirm_window_on = True
		self.discard_index = -1

		self.__ClearScoreCompletionEffect()
		self.__ClearDeckFlushEffect()

		self.deck_card_cnt = 0
		if self.deck_card_cnt_text:
			self.deck_card_cnt_text.SetText("0")
		self.__HideDeck()

		self.__CloseExitQuestionDialog()

	def __ClearHandCardSlot(self):
		self.hand_card_cnt = 0
		for card in self.cards[HAND_CARD]:
			card.Clear()

	def __ClearFieldCardSlot(self):
		self.field_card_cnt = 0
		for card in self.cards[FIELD_CARD]:
			card.Clear()

	def __ClearScoreCompletionEffect(self):
		for effect in (self.score_text_effect, self.score_effect1, self.score_effect2, self.score_effect3):
			if effect:
				effect.Hide()
				effect.ResetFrame()
		if self.score_text_effect:
			self.score_text_effect.SetDelay(0)
		for effect in (self.score_effect1, self.score_effect2, self.score_effect3):
			if effect:
				effect.SetDelay(6)

	def __ClearDeckFlushEffect(self):
		if self.deck_flush_effect:
			self.deck_flush_effect.Hide()
			self.deck_flush_effect.ResetFrame()

	def __StartDeckFlushEffect(self):
		self.__ClearDeckFlushEffect()
		(x, y) = self.__SlotLocalPosition(DECK_CARD, self.deck_cur_index)
		self.deck_flush_effect.SetPosition(x + DECK_FLUSH_IMG_GAP_X, y + DECK_FLUSH_IMG_GAP_Y)
		self.deck_flush_effect.Show()
		self.deck_flush_effect.SetTop()

	def __DeckFlushEffectCheck(self):
		if not self.deck_flush_effect:
			return

		if HAND_CARD_INDEX_MAX > self.hand_card_cnt and 0 == self.field_card_cnt and self.deck_card_cnt > 0:
			if not self.deck_flush_effect.IsShow():
				self.__StartDeckFlushEffect()
		else:
			self.deck_flush_effect.Hide()

	def RumiIncreaseScore(self, score, total_score):
		self.lock = True

		self.__SetScore(score)
		self.__SetTotalScore(total_score)

		self.__ClearScoreCompletionEffect()
		self.score_effect1.Show()
		self.score_effect1.SetTop()

	def RumiMoveCard(self, srcCard, dstCard):
		(src_pos, src_index, src_color, src_number) = srcCard
		(dst_pos, dst_index, dst_color, dst_number) = dstCard

		if not self.__IsExistSlotIndex(src_pos, src_index, src_color, src_number):
			return

		if not self.__IsExistSlotIndex(dst_pos, dst_index, dst_color, dst_number):
			return

		self.lock = True

		if DECK_CARD == src_pos and HAND_CARD == dst_pos:
			self.hand_card_cnt += 1
			self.card_move_queue.append([srcCard, dstCard])

		elif HAND_CARD == src_pos and FIELD_CARD == dst_pos:
			self.hand_card_cnt -= 1
			self.field_card_cnt += 1
			self.card_move_queue.append([srcCard, dstCard])

		elif FIELD_CARD == src_pos and HAND_CARD == dst_pos:
			self.hand_card_cnt += 1
			self.field_card_cnt -= 1
			self.card_move_queue.append([srcCard, dstCard])

		elif HAND_CARD == src_pos and NONE_POS == dst_pos:
			self.hand_card_cnt -= 1
			self.cards[src_pos][src_index].Clear()
			if len(self.card_move_queue) == 0 and not self.__EffectShown():
				self.lock = False

		elif len(self.card_move_queue) == 0:
			self.lock = False

	def __IsExistSlotIndex(self, card_pos, slot_index, color, number):
		if number < 0 or number > CARD_NUMBER_END:
			return False

		if color not in (EMPTY_CARD, RED_CARD, BLUE_CARD, YELLOW_CARD):
			return False

		if card_pos < 0 or card_pos >= CARD_POS_MAX:
			return False

		if card_pos in (HAND_CARD, FIELD_CARD) and not (0 <= slot_index < len(self.cards[card_pos])):
			return False

		return True

	def Close(self):
		if self.discard_confirm_dialog and self.discard_confirm_dialog.IsShow():
			self.discard_confirm_dialog.Close()
		self.Hide()

	def Show(self):
		self.SetOkeyNormalBG()
		ui.ScriptWindow.Show(self)

	def SetDeckCount(self, deck_card_count):
		self.deck_card_cnt = max(0, deck_card_count)
		self.deck_card_cnt_text.SetText(str(self.deck_card_cnt))
		self.__SetDeckImg(self.deck_card_cnt)

	def __SetDeckImg(self, deck_cnt):
		if deck_cnt <= 0:
			self.__HideDeck()
			return

		for i in xrange(CARD_COLOR_MAX, 0, -1):
			if deck_cnt == (CARD_NUMBER_END * i):
				self.deck_cur_index = i - 1
				(x, y) = self.__SlotLocalPosition(DECK_CARD, i - 1)
				self.deck_box.LoadImage(DECK_IMG_LIST[i - 1])
				self.deck_box.SetPosition(x, y)
				self.deck_box.Show()

	def __HideDeck(self):
		if self.deck_box:
			self.deck_box.Hide()

	def __SetScore(self, score):
		self.cur_score = score
		if self.cur_score == 0:
			self.cur_score_text.SetText("")
		else:
			self.cur_score_text.SetText(str(self.cur_score))

	def __SetTotalScore(self, total_score):
		self.total_score = total_score

		if self.total_score < LOW_TOTAL_SCORE:
			self.total_score_text.SetPackedFontColor(TOTAL_SCORE_LOW_FONT_COLOR)
		elif self.total_score < MID_TOTAL_SCORE:
			self.total_score_text.SetPackedFontColor(TOTAL_SCORE_MID_FONT_COLOR)
		else:
			self.total_score_text.SetPackedFontColor(TOTAL_SCORE_HIGH_FONT_COLOR)

		self.total_score_text.SetText(str(self.total_score))

	def __ExitButtonClick(self):
		if None == self.exit_question_dialog:
			dialog = uiCommon.QuestionDialog2()
			dialog.SetText1(QUESTION_EXIT_1)
			dialog.SetText2(QUESTION_EXIT_2)
			dialog.SetAcceptEvent(ui.__mem_func__(self.__AcceptExit))
			dialog.SetCancelEvent(ui.__mem_func__(self.__CancelExit))
			self.exit_question_dialog = dialog

		self.exit_question_dialog.Open()

	def __AcceptExit(self):
		net.SendMiniGameRumiExit()
		if self.exit_question_dialog:
			self.exit_question_dialog.Close()

	def __CancelExit(self):
		if self.exit_question_dialog:
			self.exit_question_dialog.Close()

	def __CloseExitQuestionDialog(self):
		if self.exit_question_dialog:
			self.exit_question_dialog.Close()
			self.exit_question_dialog = None

	def __DiscardConfirmDialog(self, index):
		if None == self.discard_confirm_dialog:
			dialog = uiCommon.QuestionDialog()
			dialog.SetText(QUESTION_DISCARD)
			dialog.SetAcceptEvent(ui.__mem_func__(self.__AcceptDiscard))
			dialog.SetCancelEvent(ui.__mem_func__(self.__CancelDiscard))
			self.discard_confirm_dialog = dialog

		self.discard_index = index
		self.discard_confirm_dialog.Open()
		self.discard_confirm_dialog.SetTop()

	def __AcceptDiscard(self):
		if self.discard_confirm_dialog:
			self.discard_confirm_dialog.Close()

		if -1 == self.discard_index:
			return

		net.SendMiniGameRumiHandCardClick(False, self.discard_index)
		self.discard_index = -1

	def __CancelDiscard(self):
		if self.discard_confirm_dialog:
			self.discard_confirm_dialog.Close()
		self.discard_index = -1

	def __CloseDiscardConfirmDialog(self):
		if self.discard_confirm_dialog and self.discard_confirm_dialog.IsShow():
			self.discard_confirm_dialog.Close()
			return True
		return False

	def OnCardClick(self, pos, index, left):
		if pos == HAND_CARD:
			if left:
				self.LButtonClickHand(index)
			else:
				self.RButtonClickHand(index)
		elif pos == FIELD_CARD and left:
			self.LButtonClickField(index)

	def LButtonClickDeck(self):
		if self.lock:
			return
		if self.__CloseDiscardConfirmDialog():
			return
		net.SendMiniGameRumiDeckCardClick()

	def LButtonClickHand(self, index):
		if self.lock:
			return
		if self.__CloseDiscardConfirmDialog():
			return
		net.SendMiniGameRumiHandCardClick(True, index)

	def RButtonClickHand(self, index):
		if self.lock:
			return
		if self.confirm_window_on:
			self.__DiscardConfirmDialog(index)
		else:
			net.SendMiniGameRumiHandCardClick(False, index)

	def LButtonClickField(self, index):
		if self.lock:
			return
		if self.__CloseDiscardConfirmDialog():
			return
		net.SendMiniGameRumiFieldCardClick(index)

	def __ScoreTextEffectEndFrameEvent(self):
		if self.score_text_effect:
			self.score_text_effect.Hide()

		self.__SetScore(0)
		# The third card may still be on its way to the field: clear it there.
		if len(self.card_move_queue) > 0:
			self.clear_field_after_moves = True
		else:
			self.__ClearFieldCardSlot()
			self.lock = False

	def __ScoreEffectKeyFrameEvent1(self, cur_frame):
		if cur_frame == 2:
			if self.score_text_effect:
				self.score_text_effect.Show()
				self.score_text_effect.SetTop()
			if self.score_effect2:
				self.score_effect2.Show()

	def __ScoreEffectKeyFrameEvent2(self, cur_frame):
		if cur_frame == 1 and self.score_effect3:
			self.score_effect3.Show()

	def __ScoreEffectEndFrameEvent1(self):
		if self.score_effect1:
			self.score_effect1.Hide()

	def __ScoreEffectEndFrameEvent2(self):
		if self.score_effect2:
			self.score_effect2.Hide()

	def __ScoreEffectEndFrameEvent3(self):
		if self.score_effect3:
			self.score_effect3.Hide()

	def SetOkeyNormalBG(self):
		if self.back_ground:
			if IsNormal():
				self.back_ground.Show()
			else:
				self.back_ground.Hide()


# ------------------------------------------------------------- the window

class MiniGameRumi(ui.Window):
	def __init__(self):
		ui.Window.__init__(self)
		self.isLoaded = 0
		self.state = STATE_NONE

		self.cur_page = None
		self.waiting_page = None
		self.game_page = None

		self.__LoadWindow()

	def __del__(self):
		ui.Window.__del__(self)

	def Close(self):
		self.Hide()
		if self.cur_page:
			self.cur_page.Close()

	def Destroy(self):
		self.isLoaded = 0
		self.state = STATE_NONE
		self.cur_page = None

		if self.waiting_page:
			self.waiting_page.Destroy()
			self.waiting_page = None

		if self.game_page:
			self.game_page.Destroy()
			self.game_page = None

	def __LoadWindow(self):
		if self.isLoaded == 1:
			return

		self.isLoaded = 1
		self.state = STATE_WAITING

		try:
			self.waiting_page = RumiWaitingPage()
			self.game_page = RumiGamePage()
		except:
			uiminigameutil.LoadError("MiniGameRumi.LoadWindow")

		self.Hide()

	def Open(self, toggle=True):
		if STATE_WAITING == self.state:
			self.cur_page = self.waiting_page
		elif STATE_PLAY == self.state:
			self.cur_page = self.game_page
		else:
			return

		if self.cur_page.IsShow() and toggle:
			self.cur_page.Close()
		else:
			self.cur_page.Show()
			self.cur_page.SetTop()

	def GameStart(self):
		if self.waiting_page:
			self.waiting_page.DecreaseMiniGameRumiCardCount()
		if self.cur_page:
			self.cur_page.Close()

		self.state = STATE_PLAY

		if self.game_page:
			self.game_page.Clear()
			self.game_page.SetConfirmWindowCheck(self.waiting_page.GetConfirmWindowCheck())

		self.Open(False)

	def GameEnd(self):
		self.state = STATE_WAITING
		if self.game_page:
			self.game_page.Clear()
			self.game_page.Close()
		self.cur_page = self.waiting_page

	def RumiMoveCard(self, srcCard, dstCard):
		if self.game_page:
			self.game_page.RumiMoveCard(srcCard, dstCard)

	def SetDeckCount(self, deck_card_count):
		if self.game_page:
			self.game_page.SetDeckCount(deck_card_count)

	def RumiIncreaseScore(self, score, total_score):
		if self.game_page:
			self.game_page.RumiIncreaseScore(score, total_score)

	def SetOkeyNormalBG(self):
		if self.game_page:
			self.game_page.SetOkeyNormalBG()

	def MiniGameRumiFlagProcess(self, process_type, data):
		if self.waiting_page:
			self.waiting_page.MiniGameRumiFlagProcess(process_type, data)


# ------------------------------------------------------- the module's face

_data = {'window': None, 'tooltip': None, 'started': False}


def _ItemToolTip():
	tooltip = _data['tooltip']
	if not tooltip:
		import uiToolTip
		tooltip = uiToolTip.ItemToolTip()
		tooltip.HideToolTip()
		_data['tooltip'] = tooltip
	return tooltip


def _ShowItemToolTip(vnum):
	tooltip = _ItemToolTip()
	tooltip.ClearToolTip()
	tooltip.SetItemToolTip(vnum)


def _HideToolTip():
	tooltip = _data['tooltip']
	if tooltip:
		tooltip.HideToolTip()


def SetItemToolTip(tooltip):
	_data['tooltip'] = tooltip


def GetWindow():
	if not _data['window']:
		_data['window'] = uiminigameutil.SafeCreate(MiniGameRumi, "Okey")
	return _data['window']


def Open():
	"""The event list's button (uiingameevent.RegisterOpener) and the table's
	"Zagraj w Okey" (game.py's "MiniGameRumiOpen")."""
	if not HasExe():
		chat.AppendChat(chat.CHAT_TYPE_INFO, MSG_NO_EXE)
		return
	window = GetWindow()
	if window:
		window.Open()


def OpenFromTable():
	if not HasExe():
		chat.AppendChat(chat.CHAT_TYPE_INFO, MSG_NO_EXE)
		return
	window = GetWindow()
	if window:
		window.Open(False)


def Start():
	"""From game.py, a little after entering the world: the event list's
	button and the card counters (which also tells the server this exe takes
	the Rumi packets)."""
	if not HasExe():
		return
	try:
		import uiingameevent
		uiingameevent.RegisterOpener('rumi', Open)
	except ImportError:
		pass
	net.SendMiniGameRumiRequestQuestFlag()
	_data['started'] = True


def DestroyWindow():
	try:
		import uiingameevent
		uiingameevent.UnregisterOpener('rumi')
	except ImportError:
		pass
	window = _data['window']
	if window:
		window.Close()
		window.Destroy()
	_data['window'] = None
	_data['tooltip'] = None
	_data['started'] = False


# What the exe calls on the game window (game.py relays these).

def OnStart():
	window = GetWindow()
	if window:
		window.GameStart()


def OnEnd():
	window = _data['window']
	if window:
		window.GameEnd()


def OnMoveCard(src_pos, src_index, src_color, src_number, dst_pos, dst_index, dst_color, dst_number):
	window = _data['window']
	if window and window.state == STATE_PLAY:
		window.RumiMoveCard((src_pos, src_index, src_color, src_number), (dst_pos, dst_index, dst_color, dst_number))


def OnSetDeckCount(deck_card_count):
	window = _data['window']
	if window and window.state == STATE_PLAY:
		window.SetDeckCount(deck_card_count)


def OnIncreaseScore(score, total_score):
	window = _data['window']
	if window and window.state == STATE_PLAY:
		window.RumiIncreaseScore(score, total_score)


def OnFlagProcess(process_type, data):
	if RUMI_GC_SUBHEADER_SET_CARD_PIECE_FLAG == process_type:
		chat.AppendChat(chat.CHAT_TYPE_INFO, MSG_PIECE_GAIN + ' (%d/%d)' % (data[0], RUMI_CARD_PIECE_COUNT_MAX))
	elif RUMI_GC_SUBHEADER_SET_CARD_FLAG == process_type:
		chat.AppendChat(chat.CHAT_TYPE_INFO, MSG_CARD_GAIN)
	elif RUMI_GC_SUBHEADER_NO_MORE_GAIN == process_type:
		chat.AppendChat(chat.CHAT_TYPE_INFO, MSG_NO_MORE_GAIN)
	window = _data['window']
	if window:
		window.MiniGameRumiFlagProcess(process_type, data)
