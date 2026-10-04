# MT2009_PLUS_CATCH_KING_V1 - Catch the King ("Zlap Krola"), after Owsap's
# uiMiniGameCatchKing.py (v6.2.6, ENABLE_CATCH_KING_EVENT_FLAG_RENEWAL).
#
# The game is the server's (playerbot_catchking.h): this window only draws
# what the packets say. Packets CG 226 / GC 238 go through the new exe
# (client-patches/exe), with Owsap's python names: net.SendMiniGameCatchKing
# (sub, arg), net.SendMiniGameCatchKingRequestQuestFlag(), and the game
# window's MiniGameCatchKingEventStart / SetHandCard / ResultField /
# SetEndCard / Reward and CatchKingFlagProcess (game.py relays them here).
# Without those net functions (an older exe) the event button says the game
# comes with an update (uiingameevent.py), and nothing here is created.
#
# What differs from Owsap's file:
#  - no Owsap ui.py extensions: the animations (AniImageBox.ResetFrame,
#    SetEndFrameEvent, SetKeyFrameEvent) are AniImage below, a plain ImageBox
#    stepped in OnUpdate; the slots' over events take no extra argument;
#  - the texts are Polish and here (no locale_game keys), the rules are
#    locale/pl/catchking_event_desc.txt (uiminigameutil.DescriptionText);
#  - the King's Loots are 50968-50970 (Owsap's 50928-50930 are this world's
#    "Receptura" items); the card and deck stay 79603 / 79604;
#  - every result popup is a new dialog (ours adds lines to an old one);
#  - the counts are asked for whenever the window opens (cards from kills
#    keep coming while it is shut), and once at login so the server knows
#    this exe can take the game's packets.
# Python 2.7, CP1250 escapes for the Polish letters.

import app
import ui
import player
import net
import event
import uiCommon
import grp
import chat
import item
import uiToolTip
import uiminigameutil

from _weakref import proxy

STATE_NONE = 0
STATE_WAITING = 1
STATE_PLAY = 2

DESC_WIDTH_COUNT = 57
DEFAULT_DESC_Y = 7

HAND_CARD_MAX = 6
FIELD_CARD_MAX = 25

ROOT = "d:/ymir work/ui/minigame/catchking/"

card_img_path = {
	0 : ROOT + "card_number_k.sub",
	1 : ROOT + "card_number_1.sub",
	2 : ROOT + "card_number_2.sub",
	3 : ROOT + "card_number_3.sub",
	4 : ROOT + "card_number_4.sub",
	5 : ROOT + "card_number_5.sub",
	6 : ROOT + "card_number_6.sub",
	7 : ROOT + "card_pack.sub"
}

card_end_img_path = {
	1 : ROOT + "end_card_number_1.sub",
	2 : ROOT + "end_card_number_2.sub",
	3 : ROOT + "end_card_number_3.sub",
	4 : ROOT + "end_card_number_4.sub",
	5 : ROOT + "end_card_number_5.sub",
	6 : ROOT + "end_card_number_6.sub"
}

EXPLOSION_IMAGES = ["D:/Ymir Work/UI/minigame/catchking/effect/explosion/%d.sub" % i for i in xrange(1, 9)]
OVER5_IMAGES = ["D:/Ymir Work/UI/minigame/catchking/effect/over5/%d.sub" % i for i in (1, 1, 2, 3, 4, 5, 6)]
ARROW_IMAGES = ["D:/Ymir Work/UI/minigame/yutnori/move_arrow/%d.sub" % i for i in (1, 2, 3, 4, 5, 4, 3, 2)]
COMPLETION_IMAGES = ["D:/Ymir Work/UI/minigame/rumi/card_completion_effect/card_completion_eff%d.sub" % i for i in xrange(1, 9)]
COMPLETION_TEXT_IMAGES = ["D:/Ymir Work/UI/minigame/rumi/card_completion_effect/card_completion_text_effect%d.sub" % i for i in (1, 5, 5, 6, 6, 7, 8, 9)]

# The King's Loots by score (server: playerbot_catchking.h VNUM_LOOT).
LOW_SCORE_VNUM = 50970
MID_SCORE_VNUM = 50969
HIGH_SCORE_VNUM = 50968

LOW_TOTAL_SCORE = 400
MID_TOTAL_SCORE = 550

CATCHKING_START_YANG = 30000
CATCHKING_CHALLENGE_MAX = 5
CATCHKING_CARD_PIECE_COUNT_MAX = 25
CATCHKING_CARD_COUNT_MAX = 999

ITEM_VNUM_CATCH_KING_PIECE = getattr(item, "ITEM_VNUM_CATCH_KING_PIECE", 79603)
ITEM_VNUM_CATCH_KING_PACK = getattr(item, "ITEM_VNUM_CATCH_KING_PACK", 79604)

# Owsap's sub-headers (the exe's player.CATCHKING_GC_*, when it has them).
CATCHKING_CG_START = 0
CATCHKING_CG_CLICK_HAND = 1
CATCHKING_CG_CLICK_CARD = 2
CATCHKING_CG_REWARD = 3
CATCHKING_GC_SET_CARD_PIECE_FLAG = getattr(player, "CATCHKING_GC_SET_CARD_PIECE_FLAG", 5)
CATCHKING_GC_SET_CARD_FLAG = getattr(player, "CATCHKING_GC_SET_CARD_FLAG", 6)
CATCHKING_GC_SET_QUEST_FLAG = getattr(player, "CATCHKING_GC_SET_QUEST_FLAG", 7)
CATCHKING_GC_NO_MORE_GAIN = getattr(player, "CATCHKING_GC_NO_MORE_GAIN", 8)

# ---------------------------------------------------------------- the texts
TITLE = "Z\xb3ap Kr\xf3la"
TEXT_CHALLENGE = "Stawka"
TEXT_START = "Start"
TEXT_HIGH_SCORE = "Rekord"
TEXT_MYNUMBER = "Moja karta"
TEXT_SELECTION_NUMBER = "Odkryta karta"
TEXT_SCORE = "Punkty"
TEXT_REWARD = "Twoja wygrana"
TEXT_RETRY = "Graj od nowa"
TEXT_POPUP_CONFIRM = "Podpowiedzi"

MSG_CARD_PIECE_GAIN = "Otrzymujesz kart\xea kr\xf3lewsk\xb9."
MSG_CARD_GAIN = "25 kart kr\xf3lewskich da\xb3o now\xb9 Tali\xea Kr\xf3lewsk\xb9."
MSG_NO_MORE_GAIN = "Nie mo\xbfesz otrzyma\xe6 kolejnych kr\xf3lewskich talii."
MSG_NOT_ENOUGH = "Masz za ma\xb3o Talii Kr\xf3lewskich lub Yang, aby gra\xe6."
MSG_START_QUESTION1 = "Za t\xea rund\xea zap\xb3acisz %s Yang i %d x %s."
MSG_START_QUESTION2 = "Czy chcesz teraz zagra\xe6?"
MSG_CHALLENGE_TOOLTIP1 = "Za ka\xbfd\xb9 postawion\xb9 tali\xea (%s) dostaniesz jeden \xa3up."
MSG_CHALLENGE_TOOLTIP2 = "Najwi\xeacej talii na jedn\xb9 gr\xea: %d."
MSG_HELP1 = "Wielka szkoda! Twoja karta jest ni\xbfsza.[ENTER]Nie otrzymujesz punkt\xf3w. Wybierz now\xb9 kart\xea ze swojej talii."
MSG_HELP2 = "Dobra robota! Karty maj\xb9 t\xea sam\xb9 warto\x9c\xe6.[ENTER]To koniec twojej kolejki. Wybierz now\xb9 kart\xea ze swojej talii."
MSG_HELP3 = "Znakomicie! Twoja karta jest wy\xbfsza.[ENTER]Odkryj kolejn\xb9 kart\xea."
MSG_HELP4 = "Zakryta [5] le\xbfa\xb3a obok odkrytej karty.[ENTER]Niestety nie otrzymujesz \xbfadnych punkt\xf3w. Wybierz teraz swojego kr\xf3la [K]."
MSG_CATCH_FAIL = "Kr\xf3l uciek\xb3! Gra dobieg\xb3a ko\xf1ca.[ENTER]Je\xbfeli masz co najmniej 10 punkt\xf3w, odbierz swoj\xb9 wygran\xb9."
MSG_REWARD_FAIL = "Masz mniej ni\xbf 10 punkt\xf3w - tym razem bez nagrody."
MSG_SEARCH_NUMBER5 = "W pobli\xbfu jest zakryta [5]."
MSG_TOOLTIP_CARD = {
	1 : ("Mo\xbfe schwyta\xe6 karty: [1]",),
	2 : ("Mo\xbfe schwyta\xe6 karty: [1], [2]",),
	3 : ("Mo\xbfe schwyta\xe6 karty: [1], [2], [3]",),
	4 : ("Mo\xbfe schwyta\xe6 karty: [1], [2], [3], [4]",),
	5 : ("Mo\xbfe schwyta\xe6 karty: [1], [2], [3], [4], [5]", "Je\xbfeli zakryta [5] graniczy z odkrywan\xb9 kart\xb9,", "rozdanie ko\xf1czy si\xea bez punkt\xf3w."),
	6 : ("Mo\xbfe schwyta\xe6 karty: [K]",),
}
MSG_TOOLTIP_SCORE1 = "%s: 550 punkt\xf3w lub wi\xeacej"
MSG_TOOLTIP_SCORE2 = "%s: od 400 do 549 punkt\xf3w"
MSG_TOOLTIP_SCORE3 = "%s: od 10 do 399 punkt\xf3w"
MSG_TOOLTIP_SCORE4 = "Wszystkie karty w rz\xeadzie lub kolumnie: +10 punkt\xf3w."


def DescFile():
	return app.GetLocalePath() + "/catchking_event_desc.txt"


def LoadScript(self, fileName):
	try:
		pyScrLoader = ui.PythonScriptLoader()
		pyScrLoader.LoadScriptFile(self, fileName)
	except:
		uiminigameutil.LoadError("MiniGameCatchKing.LoadScript")


def Popup(old, text):
	"""A fresh popup each time: ours keeps the lines of the last SetText."""
	if old:
		old.Close()
	dlg = uiCommon.PopupDialog()
	dlg.SetWidth(390)
	dlg.SetText(text)
	dlg.Open()
	return dlg


class AniImage(ui.ImageBox):
	"""Owsap's AniImageBox with ResetFrame, SetEndFrameEvent and
	SetKeyFrameEvent, in python: an ImageBox whose picture steps every
	`delay` frames (60 a second, as the exe's own animation counts)."""

	FRAME_TIME = 1.0 / 60.0

	def __init__(self):
		ui.ImageBox.__init__(self)
		self.images = []
		self.delay = 6
		self.frame = 0
		self.nextTime = 0.0
		self.endEvent = None
		self.endArgs = ()
		self.keyEvent = None
		self.running = False
		# our exe calls no python OnUpdate on an image box (uiminigameutil)
		self.ticker = uiminigameutil.UpdateTicker(self)

	def __del__(self):
		ui.ImageBox.__del__(self)

	def SetDelay(self, delay):
		self.delay = max(1, int(delay))

	def AppendImage(self, filename):
		self.images.append(filename)
		if len(self.images) == 1:
			self.LoadImage(filename)

	def SetEndFrameEvent(self, func, *args):
		self.endEvent = func
		self.endArgs = args

	def SetKeyFrameEvent(self, func):
		self.keyEvent = func

	def ResetFrame(self):
		self.frame = 0
		if self.images:
			self.LoadImage(self.images[0])
		self.nextTime = app.GetTime() + self.delay * self.FRAME_TIME
		self.running = True

	def Show(self):
		if not self.running:
			self.ResetFrame()
		ui.ImageBox.Show(self)

	def Hide(self):
		self.running = False
		ui.ImageBox.Hide(self)

	def TickUpdate(self):
		if not self.running or not self.images:
			return
		now = app.GetTime()
		while self.running and now >= self.nextTime:
			self.nextTime += self.delay * self.FRAME_TIME
			self.frame += 1
			if self.frame >= len(self.images):
				# one pass, then the end event (which usually hides it)
				self.frame = 0
				self.LoadImage(self.images[0])
				if self.endEvent:
					apply(self.endEvent, self.endArgs)
				break
			self.LoadImage(self.images[self.frame])
			if self.keyEvent:
				self.keyEvent(self.frame)


class LoopImage(AniImage):
	"""The arrows: the same animation round and round."""

	def TickUpdate(self):
		if not self.running or not self.images:
			return
		now = app.GetTime()
		if now >= self.nextTime:
			self.nextTime = now + self.delay * self.FRAME_TIME
			self.frame = (self.frame + 1) % len(self.images)
			self.LoadImage(self.images[self.frame])


def MakeAni(parent, images, delay, x = 0, y = 0, loop = False):
	ani = LoopImage() if loop else AniImage()
	ani.SetParent(parent)
	ani.SetDelay(delay)
	for image in images:
		ani.AppendImage(image)
	ani.SetPosition(x, y)
	ani.AddFlag("not_pick")
	ani.AddFlag("float")
	ani.Hide()
	return ani


class CatchKingWaitingPage(ui.ScriptWindow):

	VISIBLE_LINE_COUNT = 8

	def __init__(self):
		ui.ScriptWindow.__init__(self)
		self.is_loaded = False

		self.start_button = None
		self.desc_board = None
		self.description_box = None

		self.desc_index = -1
		self.desc_y = DEFAULT_DESC_Y

		self.challenge_item_slot = None
		self.challenge_item_count_text = None
		self.challenge_text_window = None
		self.challenge_up_arrow_button = None
		self.challenge_down_arrow_button = None

		self.cur_challenge_count = 1
		self.tooltip_challenge = None

		self.start_question_dialog = None

		self.btn_prev = None
		self.btn_next = None

		self.card_piece_count = 0
		self.card_piece_slot = None
		self.card_piece_text = None

		self.card_pack_count = 0
		self.card_pack_slot = None
		self.card_pack_text = None

		self.tooltip_item = None

		self.__LoadWindow()

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def __LoadWindow(self):
		if self.is_loaded == 1:
			return

		self.is_loaded = 1

		try:
			pyScrLoader = ui.PythonScriptLoader()
			pyScrLoader.LoadScriptFile(self, "UIScript/MiniGameCatchKingWaitingPage.py")
		except:
			uiminigameutil.LoadError("CatchKingWaitingPage.LoadWindow.LoadObject")

		try:
			self.GetChild("board").SetCloseEvent(ui.__mem_func__(self.Close))

			self.start_button = self.GetChild("game_start_button")
			self.start_button.SetEvent(ui.__mem_func__(self.__ClickStartButton))

			self.desc_board = self.GetChild("desc_board")
			# MT2009_PLUS_MINIGAME_DESC_V1: the rules as TextLines in the box
			self.description_box = uiminigameutil.DescriptionText(self.desc_board, 7, DEFAULT_DESC_Y,
				self.desc_board.GetWidth() - 14, self.desc_board.GetHeight() - DEFAULT_DESC_Y,
				self.VISIBLE_LINE_COUNT, 19)
			self.description_box.Show()

			self.btn_prev = self.GetChild("prev_button")
			self.btn_prev.SetEvent(ui.__mem_func__(self.PrevDescriptionPage))

			self.btn_next = self.GetChild("next_button")
			self.btn_next.SetEvent(ui.__mem_func__(self.NextDescriptionPage))

			self.challenge_text_window = self.GetChild("challenge_text_window")
			self.challenge_text_window.OnMouseOverIn = ui.__mem_func__(self.__OverInChallengeText)
			self.challenge_text_window.OnMouseOverOut = ui.__mem_func__(self.__OverOutChallengeText)

			self.challenge_item_slot = self.GetChild("ChallengeItemSlot")
			self.challenge_item_slot.SetOverInItemEvent(ui.__mem_func__(self.__SlotOverInPack))
			self.challenge_item_slot.SetOverOutItemEvent(ui.__mem_func__(self.__SlotOverOutItem))

			self.challenge_item_count_text = self.GetChild("challenge_count_text")

			self.challenge_up_arrow_button = self.GetChild("up_arrow_button")
			self.challenge_up_arrow_button.SetEvent(ui.__mem_func__(self.__ClickUpArrowButton))
			self.challenge_down_arrow_button = self.GetChild("down_arrow_button")
			self.challenge_down_arrow_button.SetEvent(ui.__mem_func__(self.__ClickDownArrowButton))

			self.tooltip_challenge = uiToolTip.ToolTip()
			self.tooltip_challenge.HideToolTip()

			self.card_piece_slot = self.GetChild("challenge_piece_item_count_slot")
			self.card_piece_slot.SetOverInItemEvent(ui.__mem_func__(self.__SlotOverInPiece))
			self.card_piece_slot.SetOverOutItemEvent(ui.__mem_func__(self.__SlotOverOutItem))

			self.card_piece_text = self.GetChild("challenge_piece_item_count_text")
			self.card_piece_text.SetText("%d/%d" % (0, CATCHKING_CARD_PIECE_COUNT_MAX))

			self.card_pack_slot = self.GetChild("challenge_pack_item_count_slot")
			self.card_pack_slot.SetOverInItemEvent(ui.__mem_func__(self.__SlotOverInPack))
			self.card_pack_slot.SetOverOutItemEvent(ui.__mem_func__(self.__SlotOverOutItem))
			self.card_pack_text = self.GetChild("challenge_pack_item_count_text")
			self.card_pack_text.SetText("%d/%d" % (0, CATCHKING_CARD_COUNT_MAX))
		except:
			uiminigameutil.LoadError("CatchKingWaitingPage.LoadWindow.BindObject")

		self.Hide()

	def Open(self):
		self.Show()

	def Show(self):
		# The counts change with every kill while the window is shut.
		RequestQuestFlag()

		ui.ScriptWindow.Show(self)

		if self.description_box:
			self.description_box.LoadFile(DescFile())
			self.description_box.Show()

		try:
			item.SelectItem(ITEM_VNUM_CATCH_KING_PACK)
			self.challenge_item_slot.SetSlot(0, 0, 32, 32, item.GetIconImage(), (1.0, 1.0, 1.0, 1.0))

			item.SelectItem(ITEM_VNUM_CATCH_KING_PIECE)
			self.card_piece_slot.SetSlot(0, 0, 32, 32, item.GetIconImage(), (1.0, 1.0, 1.0, 1.0))

			item.SelectItem(ITEM_VNUM_CATCH_KING_PACK)
			self.card_pack_slot.SetSlot(0, 0, 32, 32, item.GetIconImage(), (1.0, 1.0, 1.0, 1.0))
		except:
			import dbg
			dbg.TraceError("CatchKingWaitingPage.Show: no icon for the King Card / Deck (item_proto)")

	def Close(self):
		self.Hide()

		self.CloseStartDlg()

		if self.description_box:
			self.description_box.Hide()

		if self.tooltip_challenge:
			self.tooltip_challenge.Hide()

		self.__SlotOverOutItem()

	def Destroy(self):
		self.CloseStartDlg()
		if self.description_box:
			self.description_box.Destroy()
		self.ClearDictionary()
		self.is_loaded = 0

		self.start_button = None
		self.desc_board = None
		self.description_box = None

		self.desc_index = -1
		self.desc_y = DEFAULT_DESC_Y

		self.challenge_item_slot = None
		self.challenge_item_count_text = None
		self.challenge_text_window = None
		self.challenge_up_arrow_button = None
		self.challenge_down_arrow_button = None

		self.cur_challenge_count = 1
		self.tooltip_challenge = None

		self.btn_prev = None
		self.btn_next = None

		self.card_piece_count = 0
		self.card_piece_slot = None
		self.card_piece_text = None

		self.card_pack_count = 0
		self.card_pack_slot = None
		self.card_pack_text = None

		self.tooltip_item = None

	def OnPressEscapeKey(self):
		self.Close()
		return True

	def PrevDescriptionPage(self):
		if self.description_box:
			self.description_box.PrevPage()

	def NextDescriptionPage(self):
		if self.description_box:
			self.description_box.NextPage()

	def __ClickUpArrowButton(self):
		if self.cur_challenge_count >= CATCHKING_CHALLENGE_MAX:
			return

		self.cur_challenge_count += 1
		self.challenge_item_count_text.SetText(str(self.cur_challenge_count))

	def __ClickDownArrowButton(self):
		if self.cur_challenge_count <= 1:
			return

		self.cur_challenge_count -= 1
		self.challenge_item_count_text.SetText(str(self.cur_challenge_count))

	def __ClickStartButton(self):
		# The server checks it all again; this only saves a useless packet.
		if self.card_pack_count < self.cur_challenge_count or player.GetElk() < CATCHKING_START_YANG * self.cur_challenge_count:
			chat.AppendChat(chat.CHAT_TYPE_INFO, MSG_NOT_ENOUGH)
			return

		self.CloseStartDlg()
		dlg = uiCommon.QuestionDialog2()
		dlg.SetAcceptEvent(ui.__mem_func__(self.__StartAccept))
		dlg.SetCancelEvent(ui.__mem_func__(self.CloseStartDlg))

		item.SelectItem(ITEM_VNUM_CATCH_KING_PACK)
		dlg.SetText1(MSG_START_QUESTION1 % (Money(CATCHKING_START_YANG * self.cur_challenge_count), self.cur_challenge_count, item.GetItemName()))
		dlg.SetText2(MSG_START_QUESTION2)
		dlg.SetWidth(420)
		dlg.Open()
		self.start_question_dialog = dlg

	def __OverInChallengeText(self):
		if not self.tooltip_challenge:
			return

		item.SelectItem(ITEM_VNUM_CATCH_KING_PACK)

		self.tooltip_challenge.ClearToolTip()
		self.tooltip_challenge.AutoAppendTextLine(MSG_CHALLENGE_TOOLTIP1 % item.GetItemName())
		self.tooltip_challenge.AutoAppendTextLine(MSG_CHALLENGE_TOOLTIP2 % CATCHKING_CHALLENGE_MAX)
		self.tooltip_challenge.AlignHorizonalCenter()
		self.tooltip_challenge.Show()

	def __OverOutChallengeText(self):
		if self.tooltip_challenge:
			self.tooltip_challenge.Hide()

	def __StartAccept(self):
		net.SendMiniGameCatchKing(CATCHKING_CG_START, self.cur_challenge_count)
		self.CloseStartDlg()

	def CloseStartDlg(self):
		if self.start_question_dialog:
			self.start_question_dialog.Close()
			self.start_question_dialog = None

	def __SlotOverInPiece(self, slot_index = 0):
		if self.tooltip_item:
			self.tooltip_item.SetItemToolTip(ITEM_VNUM_CATCH_KING_PIECE)

	def __SlotOverInPack(self, slot_index = 0):
		if self.tooltip_item:
			self.tooltip_item.SetItemToolTip(ITEM_VNUM_CATCH_KING_PACK)

	def __SlotOverOutItem(self):
		if self.tooltip_item:
			self.tooltip_item.HideToolTip()

	def SetItemToolTip(self, tooltip):
		self.tooltip_item = tooltip

	def DecreaseMiniGameCatchKingCardCount(self, count):
		self.card_pack_count = max(0, self.card_pack_count - count)
		self.card_pack_text.SetText("%d/%d" % (self.card_pack_count, CATCHKING_CARD_COUNT_MAX))

	def GetChallengeCount(self):
		return self.cur_challenge_count

	def CatchKingFlagProcess(self, type, data):
		(card_piece_count, card_pack_count) = data

		self.card_piece_count = card_piece_count
		self.card_pack_count = card_pack_count

		self.card_piece_text.SetText("%d/%d" % (self.card_piece_count, CATCHKING_CARD_PIECE_COUNT_MAX))
		self.card_pack_text.SetText("%d/%d" % (self.card_pack_count, CATCHKING_CARD_COUNT_MAX))


class CatchKingCard:
	def __init__(self, miniGame, cardType, parent, x, y, clickFunc, clickArg):
		self.pos = (x, y)
		self.posGlobal = parent.GetLocalPosition()

		self.backgroundImage = None
		self.backgroundOverImage = None
		self.backgroundRowImage = None
		self.fiveNearEffect = None
		self.arrowImage = None
		self.cardCountImage = None
		self.cardCountText = None
		self.destroyCardEffect = None
		self.miniGame = miniGame
		self.cardNumber = 0
		self.cardCount = 0
		self.isTheEnd = False

		self.__CreateBackgroundImg(parent, cardType, clickFunc, clickArg)

		if cardType == 0:
			self.__CreateBackgroundOverImg(parent)
			self.__CreateCardDestroyEffect(parent)
			self.__CreateFiveNearEffect(parent)
			self.__CreateBackgroundRowImg(parent)

		self.__CreateArrowImg(parent, cardType)

		if cardType == 1:
			self.__CreateCardCount(parent)

	def Destroy(self):
		for w in (self.backgroundImage, self.backgroundOverImage, self.backgroundRowImage, self.arrowImage,
				self.fiveNearEffect, self.cardCountImage, self.cardCountText, self.destroyCardEffect):
			if w:
				w.Hide()
		self.backgroundImage = None
		self.backgroundOverImage = None
		self.backgroundRowImage = None
		self.arrowImage = None
		self.fiveNearEffect = None
		self.cardCountImage = None
		self.cardCountText = None
		self.destroyCardEffect = None
		self.miniGame = None

	def SetCardNumber(self, cardNumber, isEmpty = True, cardType = 0, overInFunc = None, overOutFunc = None):
		self.cardNumber = cardNumber

		if self.backgroundImage:
			if cardType == 1 and overInFunc and overOutFunc:
				self.backgroundImage.SetEvent(ui.__mem_func__(overInFunc), "mouse_over_in", self.cardNumber)
				self.backgroundImage.SetEvent(ui.__mem_func__(overOutFunc), "mouse_over_out")

			if isEmpty:
				self.isTheEnd = False

				if self.backgroundRowImage:
					self.backgroundRowImage.Hide()

				self.backgroundImage.LoadImage(card_img_path[0])

				if self.destroyCardEffect:
					self.destroyCardEffect.Hide()
			else:
				self.backgroundImage.LoadImage(card_img_path[cardNumber])

	def SetEndCardNumber(self, cardNumber):
		self.isTheEnd = True
		if self.backgroundImage:
			self.backgroundImage.LoadImage(card_end_img_path[cardNumber])

	def ShowRowBackground(self):
		if self.backgroundRowImage:
			self.backgroundRowImage.Show()

	def GetCardNumber(self):
		return self.cardNumber

	def __CreateCardDestroyEffect(self, parent):
		self.destroyCardEffect = MakeAni(proxy(parent), EXPLOSION_IMAGES, 6)
		self.destroyCardEffect.SetEndFrameEvent(ui.__mem_func__(self.__DestroyEffectEnd))

	def __DestroyEffectEnd(self):
		self.SetCardNumber(0, True)

	def __CreateFiveNearEffect(self, parent):
		(x, y) = self.pos
		self.fiveNearEffect = MakeAni(proxy(parent), OVER5_IMAGES, 6, x, y)
		self.fiveNearEffect.SetEndFrameEvent(ui.__mem_func__(self.HideFiveNearEffect))

	def HideFiveNearEffect(self):
		if self.fiveNearEffect:
			self.fiveNearEffect.Hide()

	def ShowFiveNearEffect(self, endFunc, *args):
		if self.fiveNearEffect:
			if endFunc:
				self.fiveNearEffect.SetEndFrameEvent(endFunc, *args)
			else:
				self.fiveNearEffect.SetEndFrameEvent(ui.__mem_func__(self.HideFiveNearEffect))
			self.fiveNearEffect.SetTop()
			self.fiveNearEffect.ResetFrame()
			self.fiveNearEffect.Show()

	def ShowCardDestroyEffect(self):
		if self.destroyCardEffect:
			(x, y) = self.pos
			self.destroyCardEffect.SetPosition(x - 39, y - 50)
			self.destroyCardEffect.SetTop()
			self.destroyCardEffect.ResetFrame()
			self.destroyCardEffect.Show()

	def __CreateBackgroundImg(self, parent, cardType, clickFunc, clickArg):
		(x, y) = self.pos
		self.backgroundImage = ui.ImageBox()
		self.backgroundImage.SetParent(proxy(parent))
		self.backgroundImage.LoadImage(card_img_path[0])

		if cardType == 0:
			self.backgroundImage.SetEvent(ui.__mem_func__(self.__OverInFunc), "mouse_over_in")
			self.backgroundImage.SetEvent(ui.__mem_func__(self.__OverOutFunc), "mouse_over_out")

		if clickFunc:
			self.backgroundImage.SetEvent(ui.__mem_func__(clickFunc), "mouse_click", clickArg)

		self.backgroundImage.SetPosition(x, y)
		self.backgroundImage.Show()
		self.backgroundImage.AddFlag("float")

	def __CreateBackgroundOverImg(self, parent):
		(x, y) = self.pos
		self.backgroundOverImage = ui.ImageBox()
		self.backgroundOverImage.SetParent(proxy(parent))
		self.backgroundOverImage.LoadImage(ROOT + "card_over_img.sub")
		self.backgroundOverImage.SetPosition(x, y)
		self.backgroundOverImage.Hide()
		self.backgroundOverImage.AddFlag("not_pick")
		self.backgroundOverImage.AddFlag("float")

	def __CreateBackgroundRowImg(self, parent):
		(x, y) = self.pos
		self.backgroundRowImage = ui.ImageBox()
		self.backgroundRowImage.SetParent(proxy(parent))
		self.backgroundRowImage.LoadImage(ROOT + "card_cover_bingo.sub")
		self.backgroundRowImage.SetPosition(x, y)
		self.backgroundRowImage.Hide()
		self.backgroundRowImage.AddFlag("not_pick")
		self.backgroundRowImage.AddFlag("float")

	def __CreateArrowImg(self, parent, cardType):
		(x, y) = self.pos
		if cardType == 0:
			self.arrowImage = MakeAni(proxy(parent), ARROW_IMAGES, 10, x + 17, y - 10, True)
		else:
			self.arrowImage = MakeAni(proxy(parent), ARROW_IMAGES, 10, x + 17, y - 34, True)

	def __CreateCardCount(self, parent):
		(x, y) = self.pos
		self.cardCountImage = ui.ImageBox()
		self.cardCountImage.SetParent(proxy(parent))
		self.cardCountImage.LoadImage(ROOT + "card_count_bg.sub")
		self.cardCountImage.SetPosition(x + 29, y - 6)
		self.cardCountImage.Hide()
		self.cardCountImage.AddFlag("not_pick")
		self.cardCountImage.AddFlag("float")

		self.cardCountText = ui.TextLine()
		self.cardCountText.SetParent(self.cardCountImage)
		self.cardCountText.SetWindowHorizontalAlignCenter()
		self.cardCountText.SetWindowVerticalAlignCenter()
		self.cardCountText.SetVerticalAlignCenter()
		self.cardCountText.SetHorizontalAlignCenter()
		self.cardCountText.Show()

	def SetCardCount(self, cardCount):
		if not cardCount:
			if self.cardCountImage:
				self.cardCountImage.Hide()

			if self.backgroundImage:
				self.backgroundImage.Hide()
		else:
			if self.cardCountImage:
				self.cardCountImage.Show()

			if self.backgroundImage:
				self.backgroundImage.Show()

		self.cardCount = cardCount

		if self.cardCountText:
			self.cardCountText.SetText("X%d" % cardCount)

	def GetCardCount(self):
		return self.cardCount

	def GetPos(self):
		return self.pos

	def GetGlobalPos(self):
		return self.posGlobal

	def GetLocalPosition(self):
		if self.backgroundImage:
			return self.backgroundImage.GetLocalPosition()

		return (0, 0)

	def SetTop(self):
		if self.backgroundImage:
			self.backgroundImage.SetTop()

		if self.backgroundOverImage:
			self.backgroundOverImage.SetTop()

		if self.arrowImage:
			self.arrowImage.SetTop()

	def __OverInFunc(self):
		if self.cardNumber:
			return

		if self.isTheEnd:
			return

		if self.backgroundOverImage:
			self.backgroundOverImage.Show()

		if self.miniGame:
			if self.miniGame.HaveCardInHand():
				self.ArrowImgShow()

	def __OverOutFunc(self):
		if self.backgroundOverImage:
			self.backgroundOverImage.Hide()

		self.ArrowImgHide()

	def IsShowArrow(self):
		if self.arrowImage:
			return self.arrowImage.IsShow()

		return False

	def ArrowImgShow(self):
		if self.arrowImage:
			self.arrowImage.SetTop()
			self.arrowImage.ResetFrame()
			self.arrowImage.Show()

	def ArrowImgHide(self):
		if self.arrowImage:
			self.arrowImage.Hide()


class CatchKingGamePage(ui.ScriptWindow):
	def __init__(self, wndMiniGameCatchKing):
		ui.ScriptWindow.__init__(self)

		self.isLoaded = 0
		self.wndMiniGameCatchKing = proxy(wndMiniGameCatchKing)

		self.fieldBackground = None

		self.myHandCardImage = None
		self.selectedCardImage = None

		self.gameCardList = []
		self.handCardList = []
		self.scoreInfo = []

		self.handCardNumber = 0

		self.infoTooltip = None

		self.confirmWindowCheckButton = None
		self.checkImage = None

		self.bigScoreText = None
		self.scoreWnd = None
		self.scoreText = None

		self.rewardButton = None
		self.rewardMode = 0	# 0 the reward, 1 play again

		self.myHandCardBg = None
		self.selectedCardBg = None

		self.destroyCardEffect = None
		self.popupResult = None

		self.scoreTextEffect = None
		self.scoreEffect1 = None
		self.scoreEffect2 = None
		self.scoreEffect3 = None

		self.isLocked = False
		# the board's lock fallback: the step a missed animation end still owes
		self.lockTime = 0.0
		self.pendingFiveNear = None
		self.pendingResult = None

		self.__LoadWindow()

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def __LoadWindow(self):
		if self.isLoaded == 1:
			return

		self.isLoaded = 1

		LoadScript(self, "UIScript/MiniGameCatchKingGamePage.py")

		try:
			board = self.GetChild("board")
			board.SetCloseEvent(ui.__mem_func__(self.Close))
			self.fieldBackground = self.GetChild("field_bg")

			self.confirmWindowCheckButton = self.GetChild("confirm_check_button")
			self.confirmWindowCheckButton.SetEvent(ui.__mem_func__(self.__ClickConfirmCheckButton), "mouse_click")

			self.checkImage = self.GetChild("check_image")
			self.checkImage.Show()

			self.bigScoreText = self.GetChild("high_score_text")
			self.scoreText = self.GetChild("score_text")
			self.scoreWnd = self.GetChild("score_window")
			self.myHandCardBg = self.GetChild("my_number_card_bg")
			self.selectedCardBg = self.GetChild("selection_number_card_bg")

			self.rewardButton = self.GetChild("reward_button")
			self.rewardButton.SetEvent(ui.__mem_func__(self.__ClickRewardButton))

			# Owsap's four ani_image elements (the uiscript has none now).
			self.scoreEffect1 = MakeAni(board, COMPLETION_IMAGES, 6, 8 + 135 - 128 / 2 - 64, 31 + 58 - 20)
			self.scoreEffect2 = MakeAni(board, COMPLETION_IMAGES, 6, 8 + 135 - 128 / 2, 31 + 58 - 20)
			self.scoreEffect3 = MakeAni(board, COMPLETION_IMAGES, 6, 8 + 135, 31 + 58 - 20)
			self.scoreTextEffect = MakeAni(board, COMPLETION_TEXT_IMAGES, 6, 8 + 50, 31 + 58)
			self.scoreTextEffect.SetEndFrameEvent(ui.__mem_func__(self.__ScoreTextEffectEndFrameEvent))
			self.scoreEffect1.SetEndFrameEvent(ui.__mem_func__(self.__ScoreEffectEndFrameEvent1))
			self.scoreEffect2.SetEndFrameEvent(ui.__mem_func__(self.__ScoreEffectEndFrameEvent2))
			self.scoreEffect3.SetEndFrameEvent(ui.__mem_func__(self.__ScoreEffectEndFrameEvent3))
			self.scoreEffect1.SetKeyFrameEvent(ui.__mem_func__(self.__ScoreEffectKeyFrameEvent1))
			self.scoreEffect2.SetKeyFrameEvent(ui.__mem_func__(self.__ScoreEffectKeyFrameEvent2))

			self.infoTooltip = uiToolTip.ToolTip()
			self.infoTooltip.ClearToolTip()

			self.scoreWnd.OnMouseOverIn = ui.__mem_func__(self.OverInScoreToolTip)
			self.scoreWnd.OnMouseOverOut = ui.__mem_func__(self.OverOutToolTip)

			for i in xrange(FIELD_CARD_MAX):
				self.gameCardList.append(CatchKingCard(self, 0, self.fieldBackground, 8 + ((i % 5) * 51), 8 + ((i / 5) * 37), self.ClickCard, i))
				self.gameCardList[i].SetCardNumber(0, True)

			for k in xrange(HAND_CARD_MAX):
				self.handCardList.append(CatchKingCard(self, 1, self.GetChild("hand_card_bg%d" % int(k + 1)), 4, 4, self.ClickHandCard, k))
				self.handCardList[k].SetCardNumber(k + 1, False, 1, self.OverInToolTip, self.OverOutToolTip)

			self.myHandCardImage = ui.ImageBox()
			self.myHandCardImage.SetParent(self.myHandCardBg)
			self.myHandCardImage.LoadImage(card_img_path[0])
			self.myHandCardImage.SetPosition(8, 8)
			self.myHandCardImage.AddFlag("float")
			self.myHandCardImage.Hide()

			self.selectedCardImage = ui.ImageBox()
			self.selectedCardImage.SetParent(self.selectedCardBg)
			self.selectedCardImage.LoadImage(card_img_path[0])
			self.selectedCardImage.SetPosition(8, 8)
			self.selectedCardImage.AddFlag("float")
			self.selectedCardImage.Hide()

			self.destroyCardEffect = MakeAni(self.myHandCardBg, EXPLOSION_IMAGES, 6)
		except:
			uiminigameutil.LoadError("CatchKingGamePage.LoadWindow.BindObject")

		self.CreateScoreTooltip()
		self.__ClearScoreCompletionEffect()

		self.Hide()

	def __ClickRewardButton(self):
		if self.rewardMode == 1:
			self.__ClickRetryButton()
			return

		if self.HaveCardInHand():
			return

		if self.isLocked:
			return

		net.SendMiniGameCatchKing(CATCHKING_CG_REWARD, 0)

	def __ScoreTextEffectEndFrameEvent(self):
		if self.scoreTextEffect:
			self.scoreTextEffect.Hide()

	def __ScoreEffectKeyFrameEvent1(self, cur_frame):
		if cur_frame == 2:
			if self.scoreTextEffect:
				self.scoreTextEffect.ResetFrame()
				self.scoreTextEffect.Show()

			if self.scoreEffect2:
				self.scoreEffect2.ResetFrame()
				self.scoreEffect2.Show()

	def __ScoreEffectKeyFrameEvent2(self, cur_frame):
		if cur_frame == 1:
			if self.scoreEffect3:
				self.scoreEffect3.ResetFrame()
				self.scoreEffect3.Show()

	def __ScoreEffectEndFrameEvent1(self):
		if self.scoreEffect1:
			self.scoreEffect1.Hide()

	def __ScoreEffectEndFrameEvent2(self):
		if self.scoreEffect2:
			self.scoreEffect2.Hide()

	def __ScoreEffectEndFrameEvent3(self):
		if self.scoreEffect3:
			self.scoreEffect3.Hide()

	def __ClearScoreCompletionEffect(self):
		for effect in (self.scoreTextEffect, self.scoreEffect1, self.scoreEffect2, self.scoreEffect3):
			if effect:
				effect.Hide()

	def CreateScoreTooltip(self):
		self.scoreInfo = []
		item.SelectItem(LOW_SCORE_VNUM)
		self.scoreInfo.append(MSG_TOOLTIP_SCORE3 % item.GetItemName())

		item.SelectItem(MID_SCORE_VNUM)
		self.scoreInfo.append(MSG_TOOLTIP_SCORE2 % item.GetItemName())

		item.SelectItem(HIGH_SCORE_VNUM)
		self.scoreInfo.append(MSG_TOOLTIP_SCORE1 % item.GetItemName())

		self.scoreInfo.append(MSG_TOOLTIP_SCORE4)

	def SetEndEffectCardEvent(self, func, keepFieldCard, destroyHandCard, cardValue, isFiveNear):
		self.pendingResult = (keepFieldCard, destroyHandCard, cardValue, isFiveNear)
		self.lockTime = app.GetTime()
		if self.destroyCardEffect:
			self.destroyCardEffect.SetEndFrameEvent(ui.__mem_func__(func), keepFieldCard, destroyHandCard, cardValue, isFiveNear)

	def ShowCardDestroyEffect(self):
		if not self.myHandCardImage:
			return

		if self.destroyCardEffect:
			self.destroyCardEffect.SetPosition(-32, -42)
			self.destroyCardEffect.SetTop()
			self.destroyCardEffect.ResetFrame()
			self.destroyCardEffect.Show()

	def IsCheckShowPopUp(self):
		if self.checkImage:
			return self.checkImage.IsShow()

		return False

	def __ClickConfirmCheckButton(self, event_type):
		if self.checkImage and "mouse_click" == event_type:
			if self.checkImage.IsShow():
				self.checkImage.Hide()
			else:
				self.checkImage.Show()

	def ClickCard(self, eventType, cardNumber):
		if not self.HaveCardInHand():
			return

		if self.isLocked:
			return

		net.SendMiniGameCatchKing(CATCHKING_CG_CLICK_CARD, cardNumber)

	def ClickHandCard(self, eventType, cardNumber):
		if self.HaveCardInHand():
			return

		if not self.handCardList[cardNumber].IsShowArrow():
			return

		if self.isLocked:
			return

		net.SendMiniGameCatchKing(CATCHKING_CG_CLICK_HAND, 0)

	def SetEndCard(self, cardPos, cardNumber):
		if cardPos < 0 or cardPos >= FIELD_CARD_MAX:
			return

		if cardNumber < 1 or cardNumber > HAND_CARD_MAX:
			return

		self.gameCardList[cardPos].SetEndCardNumber(cardNumber)

	def SetHandCard(self, cardNumber):
		if cardNumber < 1 or cardNumber > HAND_CARD_MAX:
			return

		cardPos = cardNumber - 1

		self.handCardList[cardPos].ArrowImgHide()
		self.handCardList[cardPos].SetCardCount(self.handCardList[cardPos].GetCardCount() - 1)

		self.handCardNumber = cardNumber

		if self.myHandCardImage:
			self.myHandCardImage.LoadImage(card_img_path[self.handCardNumber])
			self.myHandCardImage.Show()

	def ShowPopupDialog(self, handCard, fieldCard, isFiveNear):
		if handCard == HAND_CARD_MAX - 1 and isFiveNear:
			text = MSG_HELP4
		elif handCard == HAND_CARD_MAX and fieldCard != HAND_CARD_MAX:
			text = MSG_CATCH_FAIL
		elif handCard == HAND_CARD_MAX:
			return
		elif handCard < fieldCard:
			text = MSG_HELP1
		elif handCard == fieldCard:
			text = MSG_HELP2
		else:
			text = MSG_HELP3

		self.popupResult = Popup(self.popupResult, text)

	def ResultEffectHandCard(self, keepFieldCard, destroyHandCard, cardValue, isFiveNear):
		if self.pendingResult is None and not self.isLocked:
			return	# done already (the fallback below came first)
		self.pendingResult = None
		if self.IsCheckShowPopUp():
			self.ShowPopupDialog(self.handCardNumber, cardValue, isFiveNear)

		if destroyHandCard:
			if self.myHandCardImage:
				self.myHandCardImage.Hide()

			for card in self.handCardList:
				if card.GetCardCount() > 0:
					if self.handCardNumber == card.GetCardNumber():
						# the same number again: drawn at once, as Owsap's
						net.SendMiniGameCatchKing(CATCHKING_CG_CLICK_HAND, 0)
					else:
						card.ArrowImgShow()
					break

			self.handCardNumber = 0

		if self.selectedCardImage:
			self.selectedCardImage.Hide()

		if self.destroyCardEffect:
			self.destroyCardEffect.Hide()

		self.isLocked = False

	def ShowFiveNearEffect(self, cardPos, endFunc, rowType, cardValue, keepFieldCard, destroyHandCard, getReward, isFiveNear):
		checkPos = []
		row = cardPos / 5
		col = cardPos % 5
		for dr in (-1, 0, 1):
			for dc in (-1, 0, 1):
				if (dr or dc) and 0 <= row + dr <= 4 and 0 <= col + dc <= 4:
					checkPos.append((row + dr) * 5 + col + dc)

		self.pendingFiveNear = (rowType, cardPos, cardValue, keepFieldCard, destroyHandCard, getReward, isFiveNear)
		first = True
		for i in checkPos:
			# the end event once (Owsap's ran it for every neighbour)
			if first:
				self.gameCardList[i].ShowFiveNearEffect(ui.__mem_func__(endFunc), rowType, cardPos, cardValue, keepFieldCard, destroyHandCard, getReward, isFiveNear)
				first = False
			else:
				self.gameCardList[i].ShowFiveNearEffect(None)

	def ResultField(self, score, rowType, cardPos, cardValue, keepFieldCard, destroyHandCard, getReward, isFiveNear):
		if cardPos < 0 or cardPos >= FIELD_CARD_MAX:
			return

		if cardValue < 1 or cardValue > HAND_CARD_MAX:
			return

		self.isLocked = True
		self.lockTime = app.GetTime()
		self.pendingResult = None
		self.pendingFiveNear = None

		self.SetScore(score)

		self.gameCardList[cardPos].ArrowImgHide()
		# shown either way; a card not kept goes back under its explosion
		self.gameCardList[cardPos].SetCardNumber(cardValue, False)

		if self.selectedCardImage:
			self.selectedCardImage.LoadImage(card_img_path[cardValue])
			self.selectedCardImage.Show()

		if isFiveNear:
			self.ShowFiveNearEffect(cardPos, self.EndFiveNearEffect, rowType, cardValue, keepFieldCard, destroyHandCard, getReward, isFiveNear)
			chat.AppendChat(chat.CHAT_TYPE_INFO, MSG_SEARCH_NUMBER5)
		else:
			self.EndFiveNearEffect(rowType, cardPos, cardValue, keepFieldCard, destroyHandCard, getReward, isFiveNear)

	def SetRow(self, cardPos, rowType):
		if rowType == 0:
			return

		rowStart = 5 * (cardPos / 5)
		rowEnd = 4 + (5 * (cardPos / 5))
		colStart = cardPos - (5 * (cardPos / 5))
		colEnd = cardPos + 20 - (5 * (cardPos / 5))

		if rowType & 1:
			while rowStart <= rowEnd:
				self.gameCardList[rowStart].ShowRowBackground()
				rowStart += 1

		if rowType & 2:
			while colStart <= colEnd:
				self.gameCardList[colStart].ShowRowBackground()
				colStart += 5

	def EndFiveNearEffect(self, rowType, cardPos, cardValue, keepFieldCard, destroyHandCard, getReward, isFiveNear):
		self.pendingFiveNear = None
		if self.handCardNumber == HAND_CARD_MAX:
			self.gameCardList[cardPos].SetEndCardNumber(cardValue)

			if cardValue == HAND_CARD_MAX:
				self.__ClearScoreCompletionEffect()
				self.scoreEffect1.ResetFrame()
				self.scoreEffect1.Show()
		elif not keepFieldCard:
			self.gameCardList[cardPos].ShowCardDestroyEffect()

		self.SetEndEffectCardEvent(self.ResultEffectHandCard, keepFieldCard, destroyHandCard, cardValue, isFiveNear)
		self.ShowCardDestroyEffect()

		for card in self.gameCardList:
			card.HideFiveNearEffect()

		self.SetRow(cardPos, rowType)

	def HaveCardInHand(self):
		return self.handCardNumber

	LOCK_TIMEOUT = 3.0

	def OnUpdate(self):
		# MT2009: never leave the board locked. The effects end the lock
		# (five-near -> EndFiveNearEffect, the hand card's explosion ->
		# ResultEffectHandCard); if an end event does not come, the step it
		# owes runs here after LOCK_TIMEOUT seconds.
		if not self.isLocked:
			return
		# MT2009_PLUS_SIDEKICK_WARP_SAFE_V1: the owed step sends; not on the
		# way to another core (warpsafe.py).
		import warpsafe
		if not warpsafe.InGame():
			return
		now = app.GetTime()
		if now - self.lockTime < self.LOCK_TIMEOUT:
			return
		self.lockTime = now
		if self.pendingFiveNear:
			import dbg
			dbg.TraceError("CatchKing: the five-near effect did not end - going on")
			self.EndFiveNearEffect(*self.pendingFiveNear)
		elif self.pendingResult:
			import dbg
			dbg.TraceError("CatchKing: the card effect did not end - going on")
			self.ResultEffectHandCard(*self.pendingResult)
		else:
			self.isLocked = False

	def OverInToolTip(self, eventType, arg):
		if arg < 1 or arg > HAND_CARD_MAX:
			return

		(x, y) = self.GetGlobalPosition()
		self.infoTooltip.ClearToolTip()
		if hasattr(self.infoTooltip, "SetThinBoardSize"):
			self.infoTooltip.SetThinBoardSize(395)
		for line in MSG_TOOLTIP_CARD[arg]:
			self.infoTooltip.AppendTextLine(line)

		self.infoTooltip.SetToolTipPosition(x + (self.GetWidth() / 2), y + self.GetHeight() + self.infoTooltip.GetHeight() - 10)
		self.infoTooltip.Show()

	def OverInScoreToolTip(self):
		self.infoTooltip.ClearToolTip()
		if hasattr(self.infoTooltip, "SetThinBoardSize"):
			self.infoTooltip.SetThinBoardSize(395)
		self.infoTooltip.SetToolTipPosition(-1, -1)
		for text in self.scoreInfo:
			self.infoTooltip.AppendTextLine(text)
		self.infoTooltip.Show()

	def OverOutToolTip(self):
		if self.infoTooltip:
			self.infoTooltip.Hide()

	def SetBigScore(self, bigScore):
		if self.bigScoreText:
			self.bigScoreText.SetText(str(bigScore))

	def SetScore(self, score):
		if self.scoreText:
			if score < LOW_TOTAL_SCORE:
				self.scoreText.SetPackedFontColor(grp.GenerateColor(0.78, 0.78, 0.78, 1.0))
			elif score < MID_TOTAL_SCORE:
				self.scoreText.SetPackedFontColor(grp.GenerateColor(1.0, 0.85, 0.39, 1.0))
			else:
				self.scoreText.SetPackedFontColor(grp.GenerateColor(1.0, 0.0, 0.0, 1.0))

			self.scoreText.SetText(str(score))

	def SetReward(self, rewardCode):
		# The server has closed the game either way (a Loot, or too few points).
		if rewardCode == 1:
			self.popupResult = Popup(self.popupResult, MSG_REWARD_FAIL)

		self.rewardMode = 1
		if self.rewardButton:
			self.rewardButton.SetText(TEXT_RETRY)

	def __ClickRetryButton(self):
		if self.wndMiniGameCatchKing:
			self.wndMiniGameCatchKing.StartAgain()

	def OnPressEscapeKey(self):
		self.Close()
		return True

	def Close(self):
		if self.popupResult:
			self.popupResult.Close()
			self.popupResult = None

		if self.infoTooltip:
			self.infoTooltip.HideToolTip()

		self.Hide()

	def Clear(self):
		for (card, count) in zip(self.handCardList, (5, 2, 2, 1, 1, 1)):
			card.SetCardCount(count)
			card.ArrowImgHide()
		self.handCardList[0].ArrowImgShow()

		self.SetScore(0)

		for card in self.gameCardList:
			card.SetCardNumber(0, True)
			card.ArrowImgHide()

		self.handCardNumber = 0
		self.isLocked = False
		self.pendingFiveNear = None
		self.pendingResult = None
		if self.destroyCardEffect:
			self.destroyCardEffect.Hide()

		if self.myHandCardImage:
			self.myHandCardImage.Hide()
		if self.selectedCardImage:
			self.selectedCardImage.Hide()
		self.__ClearScoreCompletionEffect()

		self.rewardMode = 0
		if self.rewardButton:
			self.rewardButton.SetText(TEXT_REWARD)

	def Destroy(self):
		self.Close()
		for card in self.gameCardList + self.handCardList:
			card.Destroy()
		self.ClearDictionary()
		self.isLoaded = 0

		self.fieldBackground = None

		self.myHandCardImage = None
		self.selectedCardImage = None

		self.gameCardList = []
		self.handCardList = []
		self.scoreInfo = []

		self.handCardNumber = 0

		self.infoTooltip = None

		self.confirmWindowCheckButton = None
		self.checkImage = None

		self.bigScoreText = None
		self.scoreWnd = None
		self.scoreText = None

		self.rewardButton = None

		self.myHandCardBg = None
		self.selectedCardBg = None

		self.destroyCardEffect = None

		self.scoreTextEffect = None
		self.scoreEffect1 = None
		self.scoreEffect2 = None
		self.scoreEffect3 = None

		self.isLocked = False
		self.wndMiniGameCatchKing = None


class MiniGameCatchKing(ui.Window):
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
		self.Close()
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
			self.waiting_page = CatchKingWaitingPage()
			self.game_page = CatchKingGamePage(self)
		except:
			uiminigameutil.LoadError("MiniGameCatchKing.LoadWindow")

		self.Hide()

	def Open(self):
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

	def GameStart(self, bigScore):
		bet = 1
		if self.waiting_page:
			bet = self.waiting_page.GetChallengeCount()
			self.waiting_page.DecreaseMiniGameCatchKingCardCount(bet)
			self.waiting_page.Close()

		self.state = STATE_PLAY
		self.cur_page = self.game_page

		if self.game_page:
			self.game_page.Clear()
			self.game_page.SetBigScore(bigScore)
			self.game_page.Show()
			self.game_page.SetTop()

	def StartAgain(self):
		self.state = STATE_WAITING
		if self.game_page:
			self.game_page.Clear()
			self.game_page.Close()

		self.cur_page = self.waiting_page
		if self.waiting_page:
			self.waiting_page.Show()
			self.waiting_page.SetTop()

	def IsPlaying(self):
		return STATE_PLAY == self.state

	def CatchKingSetHandCard(self, cardNumber):
		if self.game_page:
			self.game_page.SetHandCard(cardNumber)

	def CatchKingResultField(self, score, rowType, cardPos, cardValue, keepFieldCard, destroyHandCard, getReward, isFiveNear):
		if self.game_page:
			self.game_page.ResultField(score, rowType, cardPos, cardValue, keepFieldCard, destroyHandCard, getReward, isFiveNear)

	def CatchKingSetEndCard(self, cardPos, cardNumber):
		if self.game_page:
			self.game_page.SetEndCard(cardPos, cardNumber)

	def CatchKingReward(self, rewardCode):
		if self.game_page:
			self.game_page.SetReward(rewardCode)

	def SetItemToolTip(self, tooltip):
		if self.waiting_page:
			self.waiting_page.SetItemToolTip(tooltip)

	def CatchKingFlagProcess(self, type, data):
		if self.waiting_page:
			self.waiting_page.CatchKingFlagProcess(type, data)


# ------------------------------------------------------------ the module's face
# game.py calls these (the exe calls the game window's Owsap-named methods).

_data = {'window': None, 'registered': False}


def Money(n):
	s = str(int(n))
	out = ""
	while len(s) > 3:
		out = " " + s[-3:] + out
		s = s[:-3]
	return s + out


def IsSupported():
	"""The exe has Catch the King's packets."""
	return hasattr(net, "SendMiniGameCatchKing") and hasattr(net, "SendMiniGameCatchKingRequestQuestFlag")


def RequestQuestFlag():
	if IsSupported():
		net.SendMiniGameCatchKingRequestQuestFlag()


def GetWindow(create = True):
	wnd = _data['window']
	if not wnd and create and IsSupported():
		wnd = uiminigameutil.SafeCreate(MiniGameCatchKing, "Z\xb3ap Kr\xf3la")
		if not wnd:
			return None
		try:
			wnd.SetItemToolTip(__import__("uiingameevent")._ItemToolTip())
		except:
			pass
		_data['window'] = wnd
	return wnd


def Open():
	"""The event hub's button (uiingameevent.RegisterOpener('catchking'))."""
	wnd = GetWindow()
	if wnd:
		wnd.Open()


def Register():
	"""From game.py, with the event hub's start: the hub's button opens the
	window, and the server learns this exe takes the game's packets."""
	if not IsSupported():
		return
	import uiingameevent
	uiingameevent.RegisterOpener("catchking", Open)
	if not _data['registered']:
		_data['registered'] = True
		RequestQuestFlag()


def Destroy():
	wnd = _data['window']
	if wnd:
		wnd.Destroy()
	_data['window'] = None
	_data['registered'] = False
	try:
		__import__("uiingameevent").UnregisterOpener("catchking")
	except:
		pass


# The game window's methods, as the exe calls them (game.py relays).
def EventStart(bigScore):
	wnd = GetWindow()
	if wnd:
		wnd.GameStart(bigScore)


def SetHandCard(cardNumber):
	wnd = GetWindow(False)
	if wnd:
		wnd.CatchKingSetHandCard(cardNumber)


def ResultField(score, rowType, cardPos, cardValue, keepFieldCard, destroyHandCard, getReward, isFiveNear):
	wnd = GetWindow(False)
	if wnd:
		wnd.CatchKingResultField(score, rowType, cardPos, cardValue, keepFieldCard, destroyHandCard, getReward, isFiveNear)


def SetEndCard(cardPos, cardNumber):
	wnd = GetWindow(False)
	if wnd:
		wnd.CatchKingSetEndCard(cardPos, cardNumber)


def Reward(rewardCode):
	wnd = GetWindow(False)
	if wnd:
		wnd.CatchKingReward(rewardCode)


def FlagProcess(type, data):
	if CATCHKING_GC_SET_CARD_PIECE_FLAG == type:
		chat.AppendChat(chat.CHAT_TYPE_INFO, MSG_CARD_PIECE_GAIN)
	elif CATCHKING_GC_SET_CARD_FLAG == type:
		chat.AppendChat(chat.CHAT_TYPE_INFO, MSG_CARD_GAIN)
	elif CATCHKING_GC_NO_MORE_GAIN == type:
		chat.AppendChat(chat.CHAT_TYPE_INFO, MSG_NO_MORE_GAIN)

	wnd = GetWindow(False)
	if wnd:
		wnd.CatchKingFlagProcess(type, data)
