# MT2009_PLUS_DIGI_CLIENT_QOL_V1 - small client conveniences (Autor: Digi Rasta; nowy-system 0.19.0,
# paczki Biore). Kept here so the shared windows get only one-line hooks:
#   PLAYER.OnCommand    "PickupSound <vnum>" from the server (Pick-Up-Sound-Effect, game.py)
#   AnimateMoney        the inventory's Yang counter runs to the new amount in ~0.4 s
#                       (Refresh-Money-With-Sleep, uiinventory.py RefreshGold)
#   UpdateCompare/HideCompare   ALT over a weapon or armour shows the worn one next to it
#                       (Compare-Item-Tooltip, uitooltip.py ItemToolTip)
# Python 2.7 as the client has it; texts CP1250 as escapes.
import app
import item
import player
import snd
import ui
import wndMgr
import localeInfo
from _weakref import proxy

################################################################################
# Pick-up sound: one sound by the item's kind, at most once per 0.15 s (Z picks up many at once).
PICKUP_SOUND_GAP = 0.15
PICKUP_YANG_VNUM = 1


def PickupSoundFile(vnum):
	if vnum == PICKUP_YANG_VNUM:
		return "sound/ui/money.wav"
	item.SelectItem(vnum)
	itemType = item.GetItemType()
	itemSubType = item.GetItemSubType()
	if itemType == item.ITEM_TYPE_WEAPON:
		if itemSubType in (item.WEAPON_BOW, item.WEAPON_ARROW, getattr(item, "WEAPON_QUIVER", -99)):
			return "sound/ui/equip_bow.wav"
		return "sound/ui/equip_metal_weapon.wav"
	if itemType == item.ITEM_TYPE_ARMOR:
		if itemSubType in (item.ARMOR_NECK, item.ARMOR_EAR, item.ARMOR_WRIST):
			return "sound/ui/equip_ring_amulet.wav"
		return "sound/ui/equip_metal_armor.wav"
	return "sound/ui/pick.wav"


class PickupSoundPlayer(object):
	def __init__(self):
		self.next = 0.0

	def OnCommand(self, vnum="0", *rest):
		now = app.GetTime()
		if now < self.next:
			return
		try:
			vnum = int(vnum)
		except ValueError:
			return
		if vnum <= 0:
			return
		self.next = now + PICKUP_SOUND_GAP
		snd.PlaySound(PickupSoundFile(vnum))


PLAYER = PickupSoundPlayer()  # stringCommander keeps a weak reference to the bound method's object

################################################################################
# Yang counter: the text line gets a tiny child window whose OnUpdate counts (it runs only
# while the inventory is shown; opened later, the time is long past and it shows the amount).
MONEY_TIME = 0.4


class _MoneyCounter(ui.Window):
	def __init__(self, textLine):
		ui.Window.__init__(self)
		self.textLine = proxy(textLine)
		self.shown = None
		self.start = 0.0
		self.source = 0
		self.target = 0
		self.SetParent(textLine)
		self.AddFlag("not_pick")
		self.SetSize(0, 0)
		self.Show()

	def __del__(self):
		ui.Window.__del__(self)

	def __SetText(self, value):
		self.shown = value
		try:
			self.textLine.SetText(localeInfo.NumberToMoneyString(value))
		except ReferenceError:
			pass

	def SetTarget(self, gold):
		if self.shown is None or self.shown == gold:
			self.target = gold
			self.__SetText(gold)
			return
		self.source = self.shown
		self.target = gold
		self.start = app.GetTime()

	def OnUpdate(self):
		if self.shown is None or self.shown == self.target:
			return
		progress = (app.GetTime() - self.start) / MONEY_TIME
		if progress >= 1.0 or progress < 0.0:
			value = self.target
		else:
			value = self.source + long((self.target - self.source) * progress)
		self.__SetText(value)


def AnimateMoney(textLine, gold):
	counter = getattr(textLine, "digiMoneyCounter", None)
	if counter is None:
		try:
			counter = _MoneyCounter(textLine)
		except Exception:
			textLine.SetText(localeInfo.NumberToMoneyString(gold))
			return
		textLine.digiMoneyCounter = counter
	counter.SetTarget(gold)

################################################################################
# Compare tooltip: the worn item's description next to the hovered weapon/armour while ALT is held.
COMPARE_LABEL = "[ Za\xb3o\xbfony ]"


def CompareEquipSlot(itemVnum):
	item.SelectItem(itemVnum)
	itemType = item.GetItemType()
	itemSubType = item.GetItemSubType()
	if itemType == item.ITEM_TYPE_WEAPON:
		if itemSubType in (item.WEAPON_ARROW, getattr(item, "WEAPON_QUIVER", -99)):
			return -1
		return item.EQUIPMENT_WEAPON
	if itemType == item.ITEM_TYPE_ARMOR:
		return {
			item.ARMOR_BODY: item.EQUIPMENT_BODY,
			item.ARMOR_HEAD: item.EQUIPMENT_HEAD,
			item.ARMOR_SHIELD: item.EQUIPMENT_SHIELD,
			item.ARMOR_WRIST: item.EQUIPMENT_WRIST,
			item.ARMOR_FOOTS: item.EQUIPMENT_SHOES,
			item.ARMOR_NECK: item.EQUIPMENT_NECK,
			item.ARMOR_EAR: item.EQUIPMENT_EAR,
		}.get(itemSubType, -1)
	return -1


def _AltPressed():
	return app.IsPressed(app.DIK_LALT) or app.IsPressed(getattr(app, "DIK_RALT", app.DIK_LALT))


def _ItemToolTipClass(tooltip):
	# The plain ItemToolTip even for a subclass (HyperlinkItemToolTip has its own board and size).
	for cls in type(tooltip).__mro__:
		if cls.__name__ == "ItemToolTip":
			return cls
	return type(tooltip)


def HideCompare(tooltip):
	tip = getattr(tooltip, "compareToolTip", None)
	if tip and tip.IsShow():
		tip.Hide()


def UpdateCompare(tooltip):
	owner = getattr(tooltip, "compareOwner", None)
	if owner is not None:
		# This is the compare tooltip itself: it leaves when its owner is gone or ALT is let go.
		try:
			if not owner.IsShow() or not _AltPressed():
				tooltip.Hide()
		except ReferenceError:
			tooltip.Hide()
		return

	tip = getattr(tooltip, "compareToolTip", None)
	itemVnum = getattr(tooltip, "itemVnum", 0)
	show = False
	if itemVnum and tooltip.IsShow() and _AltPressed():
		shown = getattr(tooltip, "compareShown", None)
		if shown and shown[0] == itemVnum:
			show = True
		else:
			slot = CompareEquipSlot(itemVnum)
			if slot >= 0 and slot != getattr(tooltip, "compareSourceSlot", -1) and player.GetItemIndex(slot):
				if not tip:
					tip = _ItemToolTipClass(tooltip)()
					tip.compareOwner = proxy(tooltip)
					tip.SetFollow(False)
					tooltip.compareToolTip = tip
				tip.SetInventoryItem(slot)
				tip.AppendTextLine(COMPARE_LABEL, tooltip.POSITIVE_COLOR)
				tip.ResizeToolTip()
				tooltip.compareShown = (itemVnum, slot)
				show = True
	if not tip:
		return
	if not show:
		if tip.IsShow():
			tip.Hide()
		return
	(x, y) = tooltip.GetGlobalPosition()
	width = tip.GetWidth()
	if x - width - 2 >= 0:
		x = x - width - 2
	else:
		x = x + tooltip.GetWidth() + 2
	y = max(0, min(y, wndMgr.GetScreenHeight() - tip.GetHeight()))
	tip.SetPosition(x, y)
	if not tip.IsShow():
		tip.Show()
	tip.SetTop()
