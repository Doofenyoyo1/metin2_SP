# MT2009_PLUS_SHOP_AUTO_PRICE_V1: the Dom Towarowy's price hint and the
# automatic price for the price window of the shop builders (from the
# "Auto-cena" extension).
#
# A player putting an item on a new offline shop (offlineShopBuilder) or a
# private shop (uiPrivateShopBuilder) gets the "Ceny" button under the price,
# as the counter of an open shop has had it (offlineshopmanage.py). The Ceny
# window shows what the server's /flea_price says - the bots' asking price,
# their median and last sale, the market's range for such a stack - and lets
# the player pick an automatic price: the suggested one, the market's minimum
# or maximum, the median or the last sale, or none. The pick is one setting
# (shop_auto_price.cfg) shared with offlineshopmanage.py, which uses the mode
# helpers below too.
#
# /flea_price <request> <window> <cell> 3 answers with "FleaPriceRange",
# "FleaPriceSales" and "FleaPriceQuote" (game.py hands them to SetRange,
# SetSales and SetQuote). The server answers a cell of the inventory only and
# says nothing while the world has the Dom Towarowy off: a dragon soul's price
# window asks nothing, and an answer that does not come is said so after
# ANSWER_TIMEOUT rather than "Loading" for ever.
#
# Every reference this module keeps to a price window is a weak one: the
# builders drop their price window by its last reference (the private shop's
# accept never closes it), and a strong one here kept it alive and on the
# screen.
#
# Python 2.7 as the client has it.

import weakref

import app
import clientclock
import localeInfo
import net
import player
import snd
import ui
import uiCommon
import wndMgr
from playerbot_lang import T

AUTO_PRICE_FILE = "shop_auto_price.cfg"
AUTO_PRICE_KEY = "auto_price"
PRICE_WINDOW_FILE = "shop_price_window.cfg"
PRICE_WINDOW_OPEN_KEY = "prices_window_open"

AUTO_PRICE_SUGGESTED = "suggested"
AUTO_PRICE_MINIMUM = "minimum"
AUTO_PRICE_MAXIMUM = "maximum"
AUTO_PRICE_MEDIAN = "median"
AUTO_PRICE_LAST = "last"
AUTO_PRICE_INACTIVE = "inactive"
# The order of the Ceny window's two rows of three buttons.
AUTO_PRICE_MODES = (
	AUTO_PRICE_MINIMUM,
	AUTO_PRICE_INACTIVE,
	AUTO_PRICE_MAXIMUM,
	AUTO_PRICE_MEDIAN,
	AUTO_PRICE_SUGGESTED,
	AUTO_PRICE_LAST,
)

REQUEST_VERSION = 3
# offlineshopmanage.py counts its requests from 1; these start far above.
REQUEST_ID_FIRST = 1500000000
REQUEST_ID_MAX = 2000000000
# Seconds without a FleaPriceQuote before the Ceny window says none is coming.
ANSWER_TIMEOUT = 6.0

COLOR_HINT = 0xFFFFD56A
COLOR_TEXT = 0xFFE5E0D4

PRICE_WINDOW_LINE_X = 18
PRICE_WINDOW_LINE_Y = 36
PRICE_WINDOW_LINE_STEP = 19
PRICE_WINDOW_NOTE_Y = 112
PRICE_WINDOW_NOTE_STEP = 15
PRICE_WINDOW_BUTTON_Y = 151
PRICE_WINDOW_BUTTON_ROW_STEP = 24
PRICE_WINDOW_BUTTON_GAP = 8
PRICE_WINDOW_CLOSE_Y = 201
PRICE_WINDOW_HEIGHT = 235
PRICE_WINDOW_MIN_WIDTH = 320

_requestID = REQUEST_ID_FIRST
_requests = {}
_priceWindow = None
_activeDialog = None


# ---- the setting, shared with offlineshopmanage.py ----

def GetAutoPriceMode():
	try:
		f = open(AUTO_PRICE_FILE, "r")
		try:
			for line in f.readlines():
				key, sep, value = line.partition("=")
				if not sep or key.strip() != AUTO_PRICE_KEY:
					continue
				value = value.strip().lower()
				if value in AUTO_PRICE_MODES:
					return value
				if value == "1":
					return AUTO_PRICE_SUGGESTED
				if value == "0":
					return AUTO_PRICE_INACTIVE
		finally:
			f.close()
	except (IOError, OSError):
		pass
	return AUTO_PRICE_INACTIVE


def SetAutoPriceMode(mode):
	if mode not in AUTO_PRICE_MODES:
		mode = AUTO_PRICE_INACTIVE
	try:
		f = open(AUTO_PRICE_FILE, "w")
		try:
			f.write("%s=%s\n" % (AUTO_PRICE_KEY, mode))
		finally:
			f.close()
	except (IOError, OSError):
		pass


def GetAutoPriceText(mode):
	labels = {
		AUTO_PRICE_SUGGESTED: T("Sugerowana", "Suggested"),
		AUTO_PRICE_MINIMUM: T("Minimalna", "Minimum"),
		AUTO_PRICE_MAXIMUM: T("Maksymalna", "Maximum"),
		AUTO_PRICE_MEDIAN: T("Mediana", "Median"),
		AUTO_PRICE_LAST: T("Ostatnia", "Last"),
		AUTO_PRICE_INACTIVE: T("Nieaktywna", "Inactive"),
	}
	return labels.get(mode, labels[AUTO_PRICE_INACTIVE])


def GetModePrice(mode, quote, priceRange, sales):
	"""The price the mode asks for out of the server's three answers, each a
	tuple or None (quote: suggested, median, samples; range: min, max; sales:
	last, median, samples); 0 when the answer it needs is missing or empty."""
	if mode == AUTO_PRICE_SUGGESTED:
		data, index = quote, 0
	elif mode == AUTO_PRICE_MINIMUM:
		data, index = priceRange, 0
	elif mode == AUTO_PRICE_MAXIMUM:
		data, index = priceRange, 1
	elif mode == AUTO_PRICE_MEDIAN:
		data, index = sales, 1
	elif mode == AUTO_PRICE_LAST:
		data, index = sales, 0
	else:
		return 0
	if not data:
		return 0
	try:
		return max(0, long(data[index]))
	except (IndexError, TypeError, ValueError):
		return 0


def SetModeButtonState(button, selected):
	if selected:
		# The chosen one stays pressed, the cursor over it too.
		button.SetUpVisual("d:/ymir work/ui/public/middle_button_03.sub")
		button.SetOverVisual("d:/ymir work/ui/public/middle_button_03.sub")
		button.SetDownVisual("d:/ymir work/ui/public/middle_button_03.sub")
		button.Down()
		button.SetTextColor(COLOR_HINT)
	else:
		button.SetUpVisual("d:/ymir work/ui/public/middle_button_01.sub")
		button.SetOverVisual("d:/ymir work/ui/public/middle_button_02.sub")
		button.SetDownVisual("d:/ymir work/ui/public/middle_button_03.sub")
		button.SetUp()
		button.SetTextColor(COLOR_TEXT)


def FormatPriceLines(quote, priceRange, sales):
	"""The Ceny window's four lines for the server's answers."""
	if not quote:
		return (T("Pobieranie danych cenowych...", "Loading price data..."), "", "", "")

	suggestedPrice, observedPrice, sampleCount = quote
	if suggestedPrice > 0:
		primary = T("Sugestia botow: ", "The bots suggest: ") + localeInfo.NumberToMoneyString(suggestedPrice)
	else:
		primary = T("Boty nie maja jeszcze wyceny tego przedmiotu.", "The bots have no price for this item yet.")

	# No sales line at all is a server that sent none: the median is then
	# the quote's own and the last sale's line stays empty.
	lastSalePrice = 0
	if sales:
		lastSalePrice, observedPrice, sampleCount = sales
	if observedPrice > 0 and sampleCount == 1:
		secondary = T("Mediana cen botow: %s (1 probka)", "Median of the bots' prices: %s (1 sample)") % (
			localeInfo.NumberToMoneyString(observedPrice),)
	elif observedPrice > 0 and sampleCount > 0:
		secondary = T("Mediana cen botow: %s (probki: %d)", "Median of the bots' prices: %s (samples: %d)") % (
			localeInfo.NumberToMoneyString(observedPrice), sampleCount)
	elif lastSalePrice > 0:
		secondary = T("Za malo sprzedazy do wyliczenia mediany.", "Too few sales for a median.")
	else:
		secondary = T("Brak historii transakcji - pokazana cena bazowa.", "No sales yet - this is the base price.")

	if priceRange is None:
		marketRange = ""
	elif priceRange[0] > 0 and priceRange[1] >= priceRange[0]:
		marketRange = T("Rynek dla takiego stosu: %s - %s", "The market for such a stack: %s - %s") % (
			localeInfo.NumberToMoneyString(priceRange[0]), localeInfo.NumberToMoneyString(priceRange[1]))
	else:
		marketRange = T("Rynek: brak porownywalnych ofert.", "The market: no comparable offers.")

	if sales is None:
		lastSale = ""
	elif lastSalePrice > 0:
		lastSale = T("Ostatnia sprzedaz botow: %s", "Last sale by the bots: %s") % (
			localeInfo.NumberToMoneyString(lastSalePrice),)
	else:
		lastSale = T("Ostatnia sprzedaz botow: brak danych.", "Last sale by the bots: no data.")
	return (primary, secondary, marketRange, lastSale)


def NoAnswerLines():
	return (T("Serwer nie podal cen tego przedmiotu.", "The server sent no prices for this item."),
		T("Dom Towarowy moze byc wylaczony.", "The Dom Towarowy may be switched off."), "", "")


# ---- the Ceny window's place and whether it opens by itself ----

def _ReadPriceWindowSettings():
	settings = {}
	try:
		f = open(PRICE_WINDOW_FILE, "r")
		try:
			for line in f.readlines():
				key, sep, value = line.partition("=")
				if sep:
					settings[key.strip()] = value.strip()
		finally:
			f.close()
	except (IOError, OSError):
		pass
	return settings


def _WritePriceWindowSettings(settings):
	try:
		f = open(PRICE_WINDOW_FILE, "w")
		try:
			# The key offlineshopmanage.py reads first, then the place.
			f.write("%s=%s\n" % (PRICE_WINDOW_OPEN_KEY, settings.get(PRICE_WINDOW_OPEN_KEY, "0")))
			for key in ("x", "y"):
				if key in settings:
					f.write("%s=%s\n" % (key, settings[key]))
		finally:
			f.close()
	except (IOError, OSError):
		pass


def _GetPriceWindowPosition():
	settings = _ReadPriceWindowSettings()
	try:
		return (int(settings["x"]), int(settings["y"]))
	except (KeyError, ValueError):
		return None


def _GetPriceWindowOpen():
	return _ReadPriceWindowSettings().get(PRICE_WINDOW_OPEN_KEY, "0").lower() in ("1", "true", "yes")


def _SavePriceWindowState(window, isOpen):
	settings = _ReadPriceWindowSettings()
	if isOpen is not None:
		settings[PRICE_WINDOW_OPEN_KEY] = "1" if isOpen else "0"
	if window:
		try:
			x, y = window.GetGlobalPosition()
			settings["x"] = str(int(x))
			settings["y"] = str(int(y))
		except Exception:
			pass
	_WritePriceWindowSettings(settings)


# ---- the price window (the builder's MoneyInputDialog) ----

def _Alive(dialog):
	"""The price window, or None once it is gone or closed."""
	if dialog is None:
		return None
	try:
		if not dialog.IsShow() or not getattr(dialog, "inputValue", None):
			return None
	except ReferenceError:
		return None
	return dialog


def _ActiveDialog():
	if _activeDialog is None:
		return None
	return _Alive(_activeDialog())


class _DialogHandler:
	"""The Ceny button's event, holding its price window weakly (the button
	is the price window's own child)."""

	def __init__(self, dialog):
		self.dialogRef = weakref.ref(dialog)

	def OnClick(self):
		dialog = _Alive(self.dialogRef())
		if dialog:
			_SavePriceWindowState(None, True)
			OpenPriceWindow(dialog)


def _OnPriceWindowEscape():
	# Escape over the Ceny window closes the price window it shows as well,
	# by that window's own cancel, so the builder forgets it the way it
	# always does.
	dialog = _ActiveDialog()
	_ClosePriceWindow()
	if dialog:
		cancel = getattr(dialog.inputValue, "OnPressEscapeKey", None)
		if cancel:
			cancel()
		if _Alive(dialog):
			dialog.Close()
	return True


def AttachPriceButton(dialog):
	if not dialog or not getattr(dialog, "board", None):
		return
	if not hasattr(dialog, "fleaPricesButton"):
		handler = _DialogHandler(dialog)
		button = ui.Button()
		button.SetParent(dialog.board)
		button.SetUpVisual("d:/ymir work/ui/public/middle_button_01.sub")
		button.SetOverVisual("d:/ymir work/ui/public/middle_button_02.sub")
		button.SetDownVisual("d:/ymir work/ui/public/middle_button_03.sub")
		button.SetText(T("Ceny", "Prices"))
		button.SetWindowHorizontalAlignCenter()
		button.SetEvent(ui.__mem_func__(handler.OnClick))
		button.Show()
		dialog.fleaPricesButtonHandler = handler
		dialog.fleaPricesButton = button

	# The layout offlineshopmanage.py gives its own price window.
	if app.ENABLE_CHEQUE_SYSTEM and hasattr(dialog, "GetTextCheque"):
		pricesY = 115
		buttonY = 140
		height = 174
	else:
		pricesY = 75
		buttonY = 100
		height = 134
		if getattr(dialog, "moneyText", None):
			dialog.moneyText.SetPosition(0, 57)
	dialog.fleaPricesButton.SetPosition(0, pricesY)
	dialog.SetSize(200, height)
	dialog.board.SetSize(200, height)
	if getattr(dialog, "acceptButton", None):
		dialog.acceptButton.SetPosition(-36, buttonY)
	if getattr(dialog, "cancelButton", None):
		dialog.cancelButton.SetPosition(35, buttonY)
	dialog.SetCenterPosition()


def _Prune():
	for requestID, request in _requests.items():
		if _Alive(request["dialog"]()) is None:
			del _requests[requestID]


def RequestDialog(dialog, windowType, slotIndex):
	"""Asks the server for the hint of the item at (windowType, slotIndex)
	for this price window, and gives the window its Ceny button."""
	global _requestID
	if not dialog:
		return 0
	_Prune()
	CancelDialog(dialog, keepWindow=True)
	AttachPriceButton(dialog)
	_requestID += 1
	if _requestID > REQUEST_ID_MAX:
		_requestID = REQUEST_ID_FIRST
	requestID = _requestID
	request = {
		"dialog": weakref.ref(dialog),
		"sentAt": clientclock.Now(),
		"originalText": dialog.GetText(),
		"lastAutoText": None,
		"quote": None,
		"range": None,
		"sales": None,
		"answered": False,
	}
	_requests[requestID] = request
	dialog.fleaPriceRequestID = requestID
	if windowType == player.INVENTORY:
		dialog.fleaPriceLines = FormatPriceLines(None, None, None)
		net.SendChatPacket("/flea_price %d %d %d %d" % (
			requestID, windowType, slotIndex, REQUEST_VERSION))
	else:
		# The server prices a cell of the inventory only (do_flea_price).
		request["answered"] = True
		dialog.fleaPriceLines = (T("Brak danych cenowych dla tego przedmiotu.", "No price data for this item."), "", "", "")
	if _GetPriceWindowOpen():
		OpenPriceWindow(dialog)
	return requestID


def CancelDialog(dialog, keepWindow=False):
	"""Forgets the price window's requests and closes its Ceny window (which
	opens again with the next one while the player keeps it open)."""
	if dialog is None:
		return
	for requestID, request in _requests.items():
		if request["dialog"]() is dialog:
			del _requests[requestID]
	if not keepWindow and _activeDialog is not None and _activeDialog() is dialog:
		_ClosePriceWindow()


class _PriceWindow(uiCommon.InputDialog):
	def OnUpdate(self):
		_OnPriceWindowUpdate()


def OpenPriceWindow(dialog):
	global _priceWindow, _activeDialog
	if not _Alive(dialog):
		return
	_activeDialog = weakref.ref(dialog)
	if _priceWindow:
		_RefreshPriceWindow()
		_priceWindow.SetTop()
		return
	window = _PriceWindow()
	window.SetTitle(T("Ceny", "Prices"))
	window.inputSlot.Hide()
	window.inputValue.Hide()
	window.acceptButton.Hide()
	window.cancelButton.SetText(T("Zamknij", "Close"))
	window.cancelButton.SetWindowHorizontalAlignCenter()
	window.SetCancelEvent(ui.__mem_func__(window.Close))
	window.closeEvent = _OnPriceWindowClosedByPlayer
	window.OnPressEscapeKey = _OnPriceWindowEscape
	window.fleaPriceText = []
	for index in xrange(4):
		line = ui.TextLine()
		line.SetParent(window.board)
		line.AddFlag("not_pick")
		line.SetPackedFontColor(COLOR_HINT if index == 0 else COLOR_TEXT)
		line.SetPosition(PRICE_WINDOW_LINE_X, PRICE_WINDOW_LINE_Y + index * PRICE_WINDOW_LINE_STEP)
		line.Show()
		window.fleaPriceText.append(line)
	window.fleaPriceNote = []
	for noteText in (
		T("Wybierz tryb automatycznego ustalania ceny.", "Select an automatic pricing mode."),
		T("Cena zostanie uzupelniona zgodnie z wybrana opcja.", "The price will be set according to your selection."),
	):
		line = ui.TextLine()
		line.SetParent(window.board)
		line.AddFlag("not_pick")
		line.SetWindowHorizontalAlignCenter()
		line.SetHorizontalAlignCenter()
		line.SetPackedFontColor(COLOR_TEXT)
		line.SetText(noteText)
		line.Show()
		window.fleaPriceNote.append(line)
	window.fleaAutoPriceButtons = {}
	for mode in AUTO_PRICE_MODES:
		button = ui.Button()
		button.SetParent(window)
		button.SetUpVisual("d:/ymir work/ui/public/middle_button_01.sub")
		button.SetOverVisual("d:/ymir work/ui/public/middle_button_02.sub")
		button.SetDownVisual("d:/ymir work/ui/public/middle_button_03.sub")
		button.SetWindowHorizontalAlignCenter()
		button.SetText(GetAutoPriceText(mode))
		button.SetEvent(_OnSelectAutoPriceMode, mode)
		button.Show()
		window.fleaAutoPriceButtons[mode] = button
	_priceWindow = window
	window.Open()
	window.inputValue.KillFocus()
	_RefreshPriceWindow()
	_PlacePriceWindow(window, dialog)
	window.SetTop()
	dialog.SetFocus()


def _PlacePriceWindow(window, dialog):
	position = _GetPriceWindowPosition()
	if position:
		x, y = position
	else:
		x, y = dialog.GetGlobalPosition()
		y -= window.GetHeight()
	window.SetPosition(
		max(0, min(x, wndMgr.GetScreenWidth() - window.GetWidth())),
		max(0, min(y, wndMgr.GetScreenHeight() - window.GetHeight())))


def _RefreshPriceWindow():
	window = _priceWindow
	dialog = _ActiveDialog()
	if not window or not dialog or not getattr(window, "board", None):
		return
	lines = getattr(dialog, "fleaPriceLines", None) or ()
	width = PRICE_WINDOW_MIN_WIDTH
	for i, textLine in enumerate(window.fleaPriceText):
		textLine.SetText(lines[i] if i < len(lines) else "")
		width = max(width, textLine.GetTextSize()[0] + PRICE_WINDOW_LINE_X + 32)
	for line in window.fleaPriceNote:
		width = max(width, line.GetTextSize()[0] + 32)
	buttonWidth = window.fleaAutoPriceButtons[AUTO_PRICE_MINIMUM].GetWidth()
	width = max(width, 3 * buttonWidth + 2 * PRICE_WINDOW_BUTTON_GAP + 32)
	(x, y) = window.GetGlobalPosition()
	window.SetSize(width, PRICE_WINDOW_HEIGHT)
	window.board.SetSize(width, PRICE_WINDOW_HEIGHT)
	for index, line in enumerate(window.fleaPriceNote):
		line.SetPosition(0, PRICE_WINDOW_NOTE_Y + index * PRICE_WINDOW_NOTE_STEP)
	current = GetAutoPriceMode()
	for index, mode in enumerate(AUTO_PRICE_MODES):
		button = window.fleaAutoPriceButtons[mode]
		column = index % 3
		row = index // 3
		button.SetPosition((column - 1) * (button.GetWidth() + PRICE_WINDOW_BUTTON_GAP),
			PRICE_WINDOW_BUTTON_Y + row * PRICE_WINDOW_BUTTON_ROW_STEP)
		SetModeButtonState(button, mode == current)
	window.cancelButton.SetPosition(0, PRICE_WINDOW_CLOSE_Y)
	# A window grown wider keeps its right edge on the screen.
	window.SetPosition(
		max(0, min(x, wndMgr.GetScreenWidth() - width)),
		max(0, min(y, wndMgr.GetScreenHeight() - PRICE_WINDOW_HEIGHT)))


def _ClosePriceWindow():
	"""Closes the Ceny window with its price window; the player's choice to
	keep it open stays, its place is kept."""
	global _priceWindow, _activeDialog
	window = _priceWindow
	_priceWindow = None
	_activeDialog = None
	if window:
		_SavePriceWindowState(window, None)
		window.Close()


def _OnPriceWindowClosedByPlayer():
	# The window's own Close (its X or Zamknij): it stays closed from now on.
	global _priceWindow, _activeDialog
	window = _priceWindow
	if not window:
		return
	dialog = _ActiveDialog()
	_SavePriceWindowState(window, False)
	_priceWindow = None
	_activeDialog = None
	if dialog:
		dialog.SetFocus()


def _OnPriceWindowUpdate():
	dialog = _ActiveDialog()
	if not dialog:
		# The price window went without telling (a builder closed, a warp).
		_ClosePriceWindow()
		return
	request = _requests.get(getattr(dialog, "fleaPriceRequestID", 0))
	if request and not request["answered"] and clientclock.Now() - request["sentAt"] >= ANSWER_TIMEOUT:
		request["answered"] = True
		dialog.fleaPriceLines = NoAnswerLines()
		_RefreshPriceWindow()


def _OnSelectAutoPriceMode(mode):
	if mode not in AUTO_PRICE_MODES:
		return
	SetAutoPriceMode(mode)
	snd.PlaySound("sound/ui/click.wav")
	_RefreshPriceWindow()
	dialog = _ActiveDialog()
	if dialog:
		_TryApply(getattr(dialog, "fleaPriceRequestID", 0))


def _TryApply(requestID):
	request = _requests.get(requestID)
	if not request:
		return
	dialog = _Alive(request["dialog"]())
	if not dialog:
		return
	price = GetModePrice(GetAutoPriceMode(), request["quote"], request["range"], request["sales"])
	if price <= 0:
		return
	# Never over a price the player typed: only the one the window opened
	# with, or the one this filled in before.
	text = dialog.GetText()
	if text != request["originalText"] and text != request["lastAutoText"]:
		return
	# The server sells for less than GOLD_MAX only, and a price window may
	# take fewer digits than that (fleaPriceMax).
	limit = getattr(dialog, "fleaPriceMax", player.GOLD_MAX - 1)
	dialog.SetValue(min(price, limit))
	request["lastAutoText"] = dialog.GetText()


def _OnAnswer(requestID, key, value):
	request = _requests.get(requestID)
	if not request:
		return
	request[key] = value
	if key == "quote":
		# The server sends the range and the sales first, the quote last.
		request["answered"] = True
	dialog = _Alive(request["dialog"]())
	if not dialog:
		del _requests[requestID]
		return
	if request["quote"]:
		dialog.fleaPriceLines = FormatPriceLines(request["quote"], request["range"], request["sales"])
	if _ActiveDialog() is dialog:
		_RefreshPriceWindow()
	_TryApply(requestID)


def SetQuote(requestID, suggestedPrice, observedPrice, sampleCount):
	_OnAnswer(requestID, "quote", (suggestedPrice, observedPrice, sampleCount))


def SetRange(requestID, minPrice, maxPrice):
	_OnAnswer(requestID, "range", (minPrice, maxPrice))


def SetSales(requestID, lastSalePrice, medianPrice, medianUnits):
	_OnAnswer(requestID, "sales", (lastSalePrice, medianPrice, medianUnits))
