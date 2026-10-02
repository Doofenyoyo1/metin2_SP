# Seon-Hae: the 6th and 7th bonus (MT2009_PLUS_SEONHAE_V1; the owner,
# 30 September: only the Seon-Hae version, NPC 20095; 1 October: no new exe).
# Owsap v6.2.6's root/uiattr67add.py on our chat-command protocol: no packet,
# no net.SendAttr67AddPacket, no NPC_STORAGE window, nothing the exe lacks.
# The window is uiscript/seonhaewindow.py (Owsap's attr67adddialog.py).
#
# The server (playerbot_seonhae.h, quest seonhae) sends one command, "SEONHAE",
# with a sub-command (game.py passes it here):
#   SEONHAE cfg <on>                       (at login: the item tooltip's hint)
#   SEONHAE open                           (talking to Seon-Hae)
#   SEONHAE state <on> <holding> <vnum> <seconds left> <wait minutes>
#   SEONHAE item <vnum> <s0> <s1> <s2> <t0> <v0> ... <t6> <v6>
#   SEONHAE close                          (the item was handed in)
#   SEONHAE done <1 added | 2 not> <vnum>
#   SEONHAE msg <id> <data>
# and the window answers "/seonhae open", "/seonhae add <cell> <shards>
# <additive cell> <additives>", "/seonhae collect". The window is only a form:
# the server checks everything again (the item, the shard by the item's level,
# the counts, the additive by its vnum, the distance to Seon-Hae).
#
# Python 2.7 as the client has it; the texts are CP1250 escapes.

import app
import chat
import item
import net
import player
import ui
import uiminigameutil
import uiCommon
import uiToolTip
import mouseModule

MATERIAL_MAX_COUNT = 10
SUPPORT_MAX_COUNT = 5
SUCCESS_PER_MATERIAL = 2
USE_LIMIT_RANGE = 1000

# the Additives by vnum (the server has the same table)
SUPPORT_PCT = {72064: 5, 72065: 10, 72066: 20, 72067: 50}

# the names when the client's item table does not have the items yet
OWN_ITEM_NAME = {
	39070: "Szary Od\xb3amek", 39071: "Bia\xb3y Od\xb3amek", 39072: "Zielony Od\xb3amek", 39073: "\xaf\xf3\xb3ty Od\xb3amek",
	39074: "Niebieski Od\xb3amek", 39075: "Fioletowy Od\xb3amek", 39076: "Czerwony Od\xb3amek", 39077: "T\xeaczowy Od\xb3amek",
	39081: "\x8cwi\xeaty Od\xb3amek",
	72064: "Ma\xb3y Suplement", 72065: "\x8credni Suplement", 72066: "Du\xbfy Suplement", 72067: "Silny Suplement",
}

TEXT_TITLE_ADD = "Dodaj bonus"
TEXT_TITLE_COLLECT = "Odbierz"
TEXT_CHANCE = "Szansa powodzenia: %d%%"
TEXT_OFF = "Seon-Hae nie przyjmuje teraz przedmiot\xf3w"
TEXT_WAIT = "Gotowe za: %s"
TEXT_READY = "Gotowe! Odbierz przedmiot"
TEXT_QUESTION1 = "Odda\xe6 przedmiot Seon-Hae (szansa %d%%)?"
TEXT_QUESTION2 = "Od\xb3amki i Suplementy przepadn\xb9, a przedmiot wr\xf3ci za %s."
TEXT_HINT_LINE = "Seon-Hae mo\xbfe doda\xe6 temu przedmiotowi dodatkowy bonus."
TEXT_RULES = (
	"Seon-Hae dodaje 6. lub 7. bonus do broni, zbroi i bi\xbfuterii,",
	"kt\xf3ra ma ju\xbf pi\xea\xe6 bonus\xf3w.",
	"Ka\xbfdy Od\xb3amek poziomu przedmiotu daje 2% szansy (do 10).",
	"Suplementy (do 5) zwi\xeakszaj\xb9 szans\xea - im wi\xeacej Od\xb3amk\xf3w, tym bardziej.",
	"Seon-Hae trzyma przedmiot przez jaki\x9c czas; po odbi\xf3r wr\xf3\xe6 do niego.",
	"Od\xb3amki i Suplementy przepadaj\xb9 tak\xbfe przy niepowodzeniu.",
)
TEXT_DONE_OK = "Seon-Hae odda\xb3 przedmiot z nowym bonusem!"
TEXT_DONE_FAIL = "Seon-Hae odda\xb3 przedmiot - tym razem bez nowego bonusu."
TEXT_UNKNOWN_ITEM = "Przedmiot %d"

MESSAGES = {
	1: "Seon-Hae nie przyjmuje teraz przedmiot\xf3w.",
	2: "Jeste\x9c za daleko od Seon-Hae. Porozmawiaj z nim jeszcze raz.",
	3: "Zamknij najpierw inne okna (handel, sklep, magazyn...).",
	4: "Seon-Hae ma ju\xbf tw\xf3j przedmiot. Odbierz go najpierw.",
	5: "Seon-Hae przyjmuje tylko bro\xf1, zbroje i bi\xbfuteri\xea z ekwipunku.",
	6: "Przedmiot musi mie\xe6 pi\xea\xe6 bonus\xf3w.",
	7: "Ten przedmiot ma ju\xbf 6. i 7. bonus.",
	8: "Tego przedmiotu nie mo\xbfna teraz odda\xe6.",
	9: "Brakuje ci Od\xb3amk\xf3w: %s.",
	10: "Wybierz od 1 do 10 Od\xb3amk\xf3w.",
	11: "To nie jest Suplement albo nie mo\xbfna go teraz u\xbfy\xe6.",
	12: "Brakuje ci Suplement\xf3w: %s.",
	13: "Seon-Hae zabra\xb3 przedmiot do pracy. Wr\xf3\xe6 po niego za %s.",
	14: "Seon-Hae jeszcze pracuje. Wr\xf3\xe6 za %s.",
	15: "Zr\xf3b miejsce w ekwipunku.",
	18: "Seon-Hae nie ma \xbfadnego twojego przedmiotu.",
	19: "Co\x9c posz\xb3o nie tak. Przedmiot jest bezpieczny u Seon-Hae - spr\xf3buj p\xf3\x9fniej.",
	20: "Dla tego przedmiotu nie ma ju\xbf dodatkowego bonusu.",
}

_data = {
	'window': None,
	'game': None,
	'on': 0,
}


def Send(command):
	net.SendChatPacket(('/seonhae ' + command).strip())


def ToInt(value, default=0):
	try:
		return int(value)
	except (TypeError, ValueError):
		return default


def IsEnabled():
	return _data.get('on', 0) == 1


_known = {}


# Whether the client's item table has the item: an unknown vnum makes the exe
# select 60001 instead (uigoblin.py does the same).
def ItemKnown(vnum):
	if not vnum:
		return False	# item.SelectItem(0) logs "Cannot find item by 0"
	if vnum in _known:
		return _known[vnum]
	known = False
	try:
		result = item.SelectItem(vnum)
		if result is not None:
			known = bool(result)
		else:
			name = item.GetItemName()
			item.SelectItem(60001)
			known = vnum == 60001 or name != item.GetItemName()
	except Exception:
		known = False
	_known[vnum] = known
	return known


def ItemName(vnum):
	if not ItemKnown(vnum):
		return OWN_ITEM_NAME.get(vnum, TEXT_UNKNOWN_ITEM % vnum)
	try:
		item.SelectItem(vnum)
		return item.GetItemName()
	except Exception:
		return OWN_ITEM_NAME.get(vnum, TEXT_UNKNOWN_ITEM % vnum)


def TimeText(seconds):
	seconds = max(0, int(seconds))
	h = seconds // 3600
	m = (seconds % 3600) // 60
	s = seconds % 60
	if h > 0:
		return '%d:%02d:%02d' % (h, m, s)
	return '%02d:%02d' % (m, s)


def MinutesText(minutes):
	minutes = max(0, int(minutes))
	if minutes >= 60 and minutes % 60 == 0:
		return '%d h' % (minutes // 60)
	if minutes >= 60:
		return '%d h %d min' % (minutes // 60, minutes % 60)
	return '%d min' % minutes


def AppendChat(message):
	chat.AppendChat(chat.CHAT_TYPE_INFO, message)


# Owsap's 67AttrMaterial by the item's level (the server has the same table).
def MaterialVnum(itemVnum):
	level = 0
	try:
		if not itemVnum:
			raise ValueError
		item.SelectItem(itemVnum)
		for i in xrange(item.LIMIT_MAX_NUM):
			(limitType, limitValue) = item.GetLimit(i)
			if limitType == item.LIMIT_LEVEL:
				level = limitValue
	except Exception:
		level = 0
	if level < 30:
		return 39070
	if level < 40:
		return 39071
	if level < 50:
		return 39072
	if level < 60:
		return 39073
	if level < 75:
		return 39074
	if level < 90:
		return 39075
	if level < 105:
		return 39076
	if level < 120:
		return 39077
	return 39081


def ChancePct(materials, supportPct, supports):
	pct = materials * SUCCESS_PER_MATERIAL
	if supports > 0 and supportPct > 0:
		pct += supportPct * supports * materials / (MATERIAL_MAX_COUNT * SUPPORT_MAX_COUNT)
	return max(0, min(100, pct))


def AttrCounts(window_type, slotIndex):
	normal = 0
	rare = 0
	for i in xrange(player.ATTRIBUTE_SLOT_MAX_NUM):
		try:
			(attrType, attrValue) = player.GetItemAttribute(window_type, slotIndex, i)
		except Exception:
			attrType = 0
		if attrType == 0:
			continue
		if i < player.ATTRIBUTE_SLOT_NORM_NUM:
			normal += 1
		else:
			rare += 1
	return (normal, rare)


def CanTakeExtraBonus(itemVnum, attrSlot):
	"""For the item tooltip (uitooltip.py): a weapon or armour with five
	bonuses and fewer than two extra ones, while Seon-Hae is on."""
	if not IsEnabled() or not attrSlot or not itemVnum:
		return False
	try:
		item.SelectItem(itemVnum)
		itemType = item.GetItemType()
		if itemType not in (item.ITEM_TYPE_WEAPON, item.ITEM_TYPE_ARMOR):
			return False
		if itemType == item.ITEM_TYPE_WEAPON and item.GetItemSubType() == item.WEAPON_ARROW:
			return False
		normal = 0
		rare = 0
		for i in xrange(min(len(attrSlot), player.ATTRIBUTE_SLOT_MAX_NUM)):
			if attrSlot[i][0] == 0:
				continue
			if i < player.ATTRIBUTE_SLOT_NORM_NUM:
				normal += 1
			else:
				rare += 1
		return normal >= player.ATTRIBUTE_SLOT_NORM_NUM and rare < player.ATTRIBUTE_SLOT_RARE_NUM
	except Exception:
		return False


class SeonHaeWindow(ui.ScriptWindow):
	def __init__(self):
		ui.ScriptWindow.__init__(self)
		self.isLoaded = False

		self.registSlot = None
		self.registPos = -1
		self.registVnum = 0

		self.materialSlot = None
		self.materialUp = None
		self.materialDown = None
		self.materialCountText = None
		self.materialVnum = 0
		self.materialCount = 0

		self.supportSlot = None
		self.supportUp = None
		self.supportDown = None
		self.supportCountText = None
		self.supportPos = -1
		self.supportVnum = 0
		self.supportCount = 0

		self.addButton = None
		self.totalText = None
		self.questionButton = None
		self.rulesToolTip = None
		self.textToolTip = None

		self.on = 0
		self.holding = 0
		self.heldVnum = 0
		self.heldSockets = [0, 0, 0]
		self.heldAttrs = [(0, 0)] * 7
		self.readyAt = 0.0
		self.waitMinutes = 1440
		self.lastShownSecond = -1

		self.startX = 0
		self.startY = 0

		self.questionDialog = None
		self.itemToolTip = None

		self.__LoadWindow()

	def __del__(self):
		ui.ScriptWindow.__del__(self)

	def __LoadWindow(self):
		try:
			ui.PythonScriptLoader().LoadScriptFile(self, 'UIScript/seonhaewindow.py')
		except Exception:
			uiminigameutil.LoadError('SeonHaeWindow.LoadScript')

		try:
			self.registSlot = self.GetChild('regist_slot')
			self.materialSlot = self.GetChild('material_slot')
			self.materialUp = self.GetChild('material_slot_arrow_up_button')
			self.materialDown = self.GetChild('material_slot_arrow_down_button')
			self.materialCountText = self.GetChild('material_slot_count_text')
			self.supportSlot = self.GetChild('support_slot')
			self.supportUp = self.GetChild('support_slot_arrow_up_button')
			self.supportDown = self.GetChild('support_slot_arrow_down_button')
			self.supportCountText = self.GetChild('support_slot_count_text')
			self.addButton = self.GetChild('attr_add_button')
			self.totalText = self.GetChild('TotalSuccessText')
			self.questionButton = self.GetChild('question_button')
		except Exception:
			uiminigameutil.LoadError('SeonHaeWindow.BindObject')

		self.GetChild('board').SetCloseEvent(ui.__mem_func__(self.Close))

		self.materialUp.SetEvent(ui.__mem_func__(self.__ClickMaterial), True)
		self.materialDown.SetEvent(ui.__mem_func__(self.__ClickMaterial), False)
		self.supportUp.SetEvent(ui.__mem_func__(self.__ClickSupport), True)
		self.supportDown.SetEvent(ui.__mem_func__(self.__ClickSupport), False)

		self.registSlot.SetSelectEmptySlotEvent(ui.__mem_func__(self.__SelectEmptyRegistSlot))
		self.registSlot.SetSelectItemSlotEvent(ui.__mem_func__(self.__SelectItemRegistSlot))
		self.registSlot.SetOverInItemEvent(ui.__mem_func__(self.__OverInRegistSlot))
		self.registSlot.SetOverOutItemEvent(ui.__mem_func__(self.__OverOutItem))

		self.materialSlot.SetOverInItemEvent(ui.__mem_func__(self.__OverInMaterialSlot))
		self.materialSlot.SetOverOutItemEvent(ui.__mem_func__(self.__OverOutItem))

		self.supportSlot.SetSelectEmptySlotEvent(ui.__mem_func__(self.__SelectEmptySupportSlot))
		self.supportSlot.SetSelectItemSlotEvent(ui.__mem_func__(self.__SelectItemSupportSlot))
		self.supportSlot.SetOverInItemEvent(ui.__mem_func__(self.__OverInSupportSlot))
		self.supportSlot.SetOverOutItemEvent(ui.__mem_func__(self.__OverOutItem))

		self.addButton.SetEvent(ui.__mem_func__(self.__ClickAddButton))

		self.rulesToolTip = uiToolTip.ToolTip()
		self.rulesToolTip.AppendSpace(5)
		for line in TEXT_RULES:
			self.rulesToolTip.AppendTextLine(line)
		self.rulesToolTip.SetTop()
		self.questionButton.SetToolTipWindow(self.rulesToolTip)

		self.textToolTip = uiToolTip.ToolTip()
		self.textToolTip.HideToolTip()

		self.itemToolTip = uiToolTip.ItemToolTip()
		self.itemToolTip.HideToolTip()

		self.questionDialog = uiCommon.QuestionDialog2()
		self.questionDialog.Close()

		self.isLoaded = True
		self.__Refresh()

	def Destroy(self):
		self.Hide()
		if self.questionDialog:
			self.questionDialog.Close()
		if self.itemToolTip:
			self.itemToolTip.HideToolTip()
		if self.textToolTip:
			self.textToolTip.HideToolTip()
		if self.rulesToolTip:
			self.rulesToolTip.HideToolTip()
		self.ClearDictionary()
		self.registSlot = None
		self.materialSlot = None
		self.materialUp = None
		self.materialDown = None
		self.materialCountText = None
		self.supportSlot = None
		self.supportUp = None
		self.supportDown = None
		self.supportCountText = None
		self.addButton = None
		self.totalText = None
		self.questionButton = None
		self.rulesToolTip = None
		self.textToolTip = None
		self.itemToolTip = None
		self.questionDialog = None
		self.isLoaded = False

	## the server

	def Open(self):
		(self.startX, self.startY, z) = player.GetMainCharacterPosition()
		self.__ClearForm()
		self.__Refresh()
		self.SetCenterPosition()
		self.SetTop()
		self.Show()
		Send('open')

	def Close(self):
		if self.questionDialog:
			self.questionDialog.Close()
		self.__OverOutItem()
		self.__ClearForm()
		self.Hide()

	def OnPressEscapeKey(self):
		self.Close()
		return True

	def SetState(self, on, holding, vnum, left, waitMinutes):
		self.on = on
		self.holding = holding
		self.heldVnum = vnum if holding else 0
		self.readyAt = app.GetTime() + max(0, left)
		self.waitMinutes = waitMinutes if waitMinutes > 0 else 1440
		if not holding:
			self.heldSockets = [0, 0, 0]
			self.heldAttrs = [(0, 0)] * 7
		else:
			self.__ClearForm()
		self.lastShownSecond = -1
		self.__Refresh()

	def SetHeldItem(self, vnum, sockets, attrs):
		self.heldVnum = vnum
		self.heldSockets = sockets
		self.heldAttrs = attrs
		self.__Refresh()

	## the form

	def __ClearForm(self):
		self.registPos = -1
		self.registVnum = 0
		self.materialVnum = 0
		self.materialCount = 0
		self.supportPos = -1
		self.supportVnum = 0
		self.supportCount = 0

	def __SetSlot(self, slot, vnum, count=0):
		if not slot:
			return
		for i in xrange(slot.GetSlotCount()):
			slot.ClearSlot(i)
		if vnum and ItemKnown(vnum):
			slot.SetItemSlot(0, vnum, count)
		slot.RefreshSlot()

	def __Chance(self):
		return ChancePct(self.materialCount, SUPPORT_PCT.get(self.supportVnum, 0), self.supportCount)

	def __Refresh(self):
		if not self.isLoaded:
			return
		if self.holding:
			self.__SetSlot(self.registSlot, self.heldVnum)
			self.__SetSlot(self.materialSlot, 0)
			self.__SetSlot(self.supportSlot, 0)
			self.materialCountText.SetText('-')
			self.supportCountText.SetText('-')
			self.addButton.SetText(TEXT_TITLE_COLLECT)
			self.__RefreshTime(True)
			return
		self.__SetSlot(self.registSlot, self.registVnum if self.registPos >= 0 else 0)
		self.__SetSlot(self.materialSlot, self.materialVnum, self.materialCount)
		self.__SetSlot(self.supportSlot, self.supportVnum if self.supportPos >= 0 else 0, self.supportCount)
		self.materialCountText.SetText(str(self.materialCount))
		self.supportCountText.SetText(str(self.supportCount))
		self.addButton.SetText(TEXT_TITLE_ADD)
		if not self.on:
			self.totalText.SetText(TEXT_OFF)
		else:
			self.totalText.SetText(TEXT_CHANCE % self.__Chance())

	def __RefreshTime(self, force=False):
		left = int(self.readyAt - app.GetTime() + 0.999)
		if left < 0:
			left = 0
		if not force and left == self.lastShownSecond:
			return
		self.lastShownSecond = left
		if left > 0:
			self.totalText.SetText(TEXT_WAIT % TimeText(left))
		else:
			self.totalText.SetText(TEXT_READY)

	def __SelectEmptyRegistSlot(self, slotIndex):
		if self.holding or not mouseModule.mouseController.isAttached():
			return
		attachedType = mouseModule.mouseController.GetAttachedType()
		attachedPos = mouseModule.mouseController.GetAttachedSlotNumber()
		if player.SLOT_TYPE_INVENTORY != attachedType:
			return
		vnum = player.GetItemIndex(attachedPos)
		if not vnum:
			return
		item.SelectItem(vnum)
		itemType = item.GetItemType()
		if itemType not in (item.ITEM_TYPE_WEAPON, item.ITEM_TYPE_ARMOR):
			AppendChat(MESSAGES[5])
			return
		if itemType == item.ITEM_TYPE_WEAPON and item.GetItemSubType() == item.WEAPON_ARROW:
			AppendChat(MESSAGES[5])
			return
		(normal, rare) = AttrCounts(player.INVENTORY, attachedPos)
		if normal < player.ATTRIBUTE_SLOT_NORM_NUM:
			AppendChat(MESSAGES[6])
			return
		if rare >= player.ATTRIBUTE_SLOT_RARE_NUM:
			AppendChat(MESSAGES[7])
			return
		mouseModule.mouseController.DeattachObject()
		self.registPos = attachedPos
		self.registVnum = vnum
		self.materialVnum = MaterialVnum(vnum)
		self.materialCount = min(MATERIAL_MAX_COUNT, player.GetItemCountByVnum(self.materialVnum))
		if self.supportPos == attachedPos:
			self.__ClearSupport()
		self.__Refresh()

	def __SelectItemRegistSlot(self, slotIndex):
		if self.holding or mouseModule.mouseController.isAttached():
			return
		self.__OverOutItem()
		self.registPos = -1
		self.registVnum = 0
		self.materialVnum = 0
		self.materialCount = 0
		self.__Refresh()

	def __SelectEmptySupportSlot(self, slotIndex):
		if self.holding or not mouseModule.mouseController.isAttached():
			return
		attachedType = mouseModule.mouseController.GetAttachedType()
		attachedPos = mouseModule.mouseController.GetAttachedSlotNumber()
		if player.SLOT_TYPE_INVENTORY != attachedType:
			return
		vnum = player.GetItemIndex(attachedPos)
		if vnum not in SUPPORT_PCT:
			AppendChat(MESSAGES[11])
			return
		if attachedPos == self.registPos:
			return
		mouseModule.mouseController.DeattachObject()
		self.supportPos = attachedPos
		self.supportVnum = vnum
		self.supportCount = min(1, player.GetItemCountByVnum(vnum))
		self.__Refresh()

	def __SelectItemSupportSlot(self, slotIndex):
		if self.holding or mouseModule.mouseController.isAttached():
			return
		self.__OverOutItem()
		self.__ClearSupport()
		self.__Refresh()

	def __ClearSupport(self):
		self.supportPos = -1
		self.supportVnum = 0
		self.supportCount = 0

	def __ClickMaterial(self, up):
		if self.holding or not self.materialVnum:
			return
		count = self.materialCount
		if up:
			if count < MATERIAL_MAX_COUNT and count < player.GetItemCountByVnum(self.materialVnum):
				count += 1
		else:
			count -= 1
		self.materialCount = max(0, count)
		self.__Refresh()

	def __ClickSupport(self, up):
		if self.holding or self.supportPos < 0 or not self.supportVnum:
			return
		count = self.supportCount
		if up:
			if count < SUPPORT_MAX_COUNT and count < player.GetItemCountByVnum(self.supportVnum):
				count += 1
		else:
			count -= 1
		self.supportCount = max(0, count)
		self.__Refresh()

	def __ClickAddButton(self):
		if self.holding:
			Send('collect')
			return
		if not self.on:
			AppendChat(MESSAGES[1])
			return
		if self.registPos < 0:
			return
		if self.materialCount <= 0:
			AppendChat(MESSAGES[9] % ItemName(self.materialVnum))
			return
		self.questionDialog.SetText1(TEXT_QUESTION1 % self.__Chance())
		self.questionDialog.SetText2(TEXT_QUESTION2 % MinutesText(self.waitMinutes))
		self.questionDialog.SetAcceptEvent(ui.__mem_func__(self.__Accept))
		self.questionDialog.SetCancelEvent(ui.__mem_func__(self.questionDialog.Close))
		self.questionDialog.Open()

	def __Accept(self):
		self.questionDialog.Close()
		if self.holding or self.registPos < 0 or self.materialCount <= 0:
			return
		supportPos = self.supportPos if (self.supportPos >= 0 and self.supportCount > 0) else -1
		supportCount = self.supportCount if supportPos >= 0 else 0
		Send('add %d %d %d %d' % (self.registPos, self.materialCount, supportPos, supportCount))

	## tooltips

	def __ShowText(self, text):
		if not self.textToolTip:
			return
		self.textToolTip.ClearToolTip()
		self.textToolTip.AppendTextLine(text)
		self.textToolTip.ShowToolTip()

	def __OverInRegistSlot(self, slotIndex):
		if not self.itemToolTip:
			return
		if self.holding and self.heldVnum:
			self.itemToolTip.ClearToolTip()
			self.itemToolTip.AddItemData(self.heldVnum, self.heldSockets, self.heldAttrs)
			self.itemToolTip.ShowToolTip()
		elif self.registPos >= 0:
			self.itemToolTip.SetInventoryItem(self.registPos)

	def __OverInMaterialSlot(self, slotIndex):
		if not self.materialVnum:
			return
		if ItemKnown(self.materialVnum) and self.itemToolTip:
			self.itemToolTip.SetItemToolTip(self.materialVnum)
		else:
			self.__ShowText(ItemName(self.materialVnum))

	def __OverInSupportSlot(self, slotIndex):
		if self.supportPos >= 0 and self.itemToolTip:
			self.itemToolTip.SetInventoryItem(self.supportPos)

	def __OverOutItem(self):
		if self.itemToolTip:
			self.itemToolTip.HideToolTip()
		if self.textToolTip:
			self.textToolTip.HideToolTip()

	## every frame

	def OnUpdate(self):
		(x, y, z) = player.GetMainCharacterPosition()
		if abs(x - self.startX) > USE_LIMIT_RANGE or abs(y - self.startY) > USE_LIMIT_RANGE:
			self.Close()
			return
		if self.holding:
			self.__RefreshTime()
			return
		# an item moved, sold or used since it was put in the form
		changed = False
		if self.registPos >= 0 and player.GetItemIndex(self.registPos) != self.registVnum:
			self.registPos = -1
			self.registVnum = 0
			self.materialVnum = 0
			self.materialCount = 0
			changed = True
		if self.supportPos >= 0 and player.GetItemIndex(self.supportPos) != self.supportVnum:
			self.__ClearSupport()
			changed = True
		if self.materialVnum and self.materialCount > player.GetItemCountByVnum(self.materialVnum):
			self.materialCount = player.GetItemCountByVnum(self.materialVnum)
			changed = True
		if self.supportVnum and self.supportCount > player.GetItemCountByVnum(self.supportVnum):
			self.supportCount = player.GetItemCountByVnum(self.supportVnum)
			changed = True
		if changed:
			self.__Refresh()


def GetWindow():
	wnd = _data['window']
	if wnd is None:
		wnd = uiminigameutil.SafeCreate(SeonHaeWindow, "Seon-Hae")
		_data['window'] = wnd
	return wnd


def OnMessage(msgId, data=0):
	text = MESSAGES.get(msgId)
	if not text:
		return
	if msgId in (9, 12):
		text = text % ItemName(data)
	elif msgId == 13:
		text = text % MinutesText(data)
	elif msgId == 14:
		text = text % TimeText(data)
	AppendChat(text)


def __OnState(on='0', holding='0', vnum='0', left='0', waitMinutes='0', *rest):
	_data['on'] = 1 if ToInt(on) == 1 else 0
	wnd = _data['window']
	if wnd:
		wnd.SetState(ToInt(on), ToInt(holding), ToInt(vnum), ToInt(left), ToInt(waitMinutes))


def __OnItem(*args):
	if not args:
		return
	vnum = ToInt(args[0])
	values = [ToInt(a) for a in args[1:]]
	sockets = (values[:3] + [0, 0, 0])[:3]
	rest = values[3:]
	attrs = []
	for i in xrange(7):
		if 2 * i + 1 < len(rest):
			attrs.append((rest[2 * i], rest[2 * i + 1]))
		else:
			attrs.append((0, 0))
	wnd = _data['window']
	if wnd:
		wnd.SetHeldItem(vnum, sockets, attrs)


def __OnDone(result='0', vnum='0', *rest):
	if ToInt(result) == 1:
		AppendChat(TEXT_DONE_OK)
	else:
		AppendChat(TEXT_DONE_FAIL)


def OnCommand(game, *args):
	_data['game'] = game
	if not args:
		return
	sub, rest = args[0], args[1:]
	try:
		if sub == 'cfg':
			_data['on'] = 1 if (rest and ToInt(rest[0]) == 1) else 0
		elif sub == 'open':
			wnd = GetWindow()
			if wnd:
				wnd.Open()
		elif sub == 'state':
			__OnState(*rest)
		elif sub == 'item':
			__OnItem(*rest)
		elif sub == 'close':
			wnd = _data['window']
			if wnd:
				wnd.Close()
		elif sub == 'done':
			__OnDone(*rest)
		elif sub == 'msg':
			OnMessage(ToInt(rest[0]) if rest else -1, ToInt(rest[1]) if len(rest) > 1 else 0)
	except Exception:
		import dbg
		import sys
		dbg.TraceError('uiseonhae.OnCommand %s: %s' % (str(args), str(sys.exc_info()[1])))


def DestroyWindow():
	wnd = _data['window']
	if wnd:
		wnd.Destroy()
	_data['window'] = None
	_data['game'] = None
