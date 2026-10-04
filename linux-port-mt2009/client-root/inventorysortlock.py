# MT2009_PLUS_INVENTORY_SORT_LOCK_V1 - an item kept in its cell when the bag
# is sorted (the owner, 3 October: "skrot klawiszowy np. alt + LPM na
# przedmiot w ekwipunku dzieki czemu nie bedzie on nam uciekal na inny slot
# podczas uzywania sortowania ... maly wyznacznik wizualny np. mala gwiazdke").
#
# Alt + left click on an item in the four bag pages locks it, a second one
# unlocks it; a locked item has a small star in its slot's corner
# (mt2009_ui/sortlock/star.tga, uiinventory.py) and a line in its tooltip.
#
# The locks are the client's: one file per character, autohunt/sortowanie/
# <nick>.cfg, lines "cell=vnum". "Uloz i scal" and "Tylko scal stosy"
# (inventoryarrange.py) send the locked cells with the request,
# "/inventory_arrange [merge] keep=<hex>" - four cells to a hex digit, the
# lowest bit the lowest cell - and the server (playerbot_arrange.cpp) leaves
# those items where they stand and lays the rest out round them. A locked
# stack still takes in units of the same item when stacks are poured
# together, and never gives its own away.
#
# A lock is the cell and the item that stood in it when it was locked, so it
# holds only while that item is there:
# - moved whole to an empty cell of the bag by hand, the lock goes with it
#   (OnMove, from uiinventory's move packet); a part cut off a locked stack
#   leaves the lock where it was;
# - poured whole into a stack of the same item, the lock goes to that stack;
# - another item in the cell (the item used up, sold, dropped, equipped,
#   put into the safebox, and something else came there) ends the lock the
#   next time the bag is drawn; an empty cell ends it at the next sort.
# Refining keeps the lock: a weapon or armour whose vnum moved within its
# own ten (the same item one level up) is the same item.
#
# The texts are CP1250, the client's own, written as escapes so the file
# stays ASCII. Python 2.7 as the client has it.

import os
import chat
import item
import player

import uiautohunt

CONFIG_DIR = os.path.join(uiautohunt.CONFIG_BASE_DIR, 'sortowanie')

MSG_LOCKED = 'Przedmiot zablokowany na tym miejscu - sortowanie go nie przesunie (Alt+LPM zdejmuje blokad\xea).'
MSG_UNLOCKED = 'Zdj\xeato blokad\xea - sortowanie mo\xbfe znowu przesun\xb9\xe6 ten przedmiot.'
MSG_BAG_ONLY = 'Blokad\xea sortowania mo\xbfna za\xb3o\xbfy\xe6 tylko na przedmiot w plecaku.'
TOOLTIP_LOCKED = 'Zablokowany przy sortowaniu (Alt+LPM)'
TOOLTIP_COLOR = 0xffffd640

_state = {'name': None, 'locks': {}}


def _FileFor(name):
	if not name.isalnum():
		name = name.encode('hex')
	return os.path.join(CONFIG_DIR, name + '.cfg')


def _Load():
	name = player.GetName()
	if not name:
		return False
	if _state['name'] == name:
		return True
	_state['name'] = name
	_state['locks'] = {}
	try:
		with open(_FileFor(name), 'r') as handle:
			for line in handle:
				if '=' not in line:
					continue
				cell, vnum = line.strip().split('=', 1)
				try:
					cell, vnum = int(cell), int(vnum)
				except ValueError:
					continue
				if IsBagCell(cell) and vnum > 0:
					_state['locks'][cell] = vnum
	except (IOError, OSError):
		pass
	return True


def _Save():
	if not _state['name']:
		return
	try:
		if not os.path.exists(CONFIG_DIR):
			os.makedirs(CONFIG_DIR)
		with open(_FileFor(_state['name']), 'w') as handle:
			for cell, vnum in sorted(_state['locks'].items()):
				handle.write('%d=%d\n' % (cell, vnum))
	except (IOError, OSError):
		pass


def IsBagCell(cell):
	return 0 <= cell < player.INVENTORY_DEFAULT_MAX_NUM


def _IsRefinable(vnum):
	try:
		item.SelectItem(vnum)
		return item.GetItemType() in (item.ITEM_TYPE_WEAPON, item.ITEM_TYPE_ARMOR)
	except Exception:
		return False


def _SameItem(locked, vnum):
	if locked == vnum:
		return True
	return vnum > 0 and locked // 10 == vnum // 10 and _IsRefinable(locked) and _IsRefinable(vnum)


def IsLocked(cell):
	"""Whether the item standing in `cell` is the one locked there."""
	if not IsBagCell(cell) or not _Load():
		return False
	locked = _state['locks'].get(cell)
	return bool(locked) and _SameItem(locked, player.GetItemIndex(cell))


def Toggle(cell):
	"""Alt + left click: lock the item in `cell`, or unlock it."""
	if not IsBagCell(cell):
		chat.AppendChat(chat.CHAT_TYPE_INFO, MSG_BAG_ONLY)
		return False
	vnum = player.GetItemIndex(cell)
	if not vnum or not _Load():
		return False
	if IsLocked(cell):
		del _state['locks'][cell]
		chat.AppendChat(chat.CHAT_TYPE_INFO, MSG_UNLOCKED)
	else:
		_state['locks'][cell] = vnum
		chat.AppendChat(chat.CHAT_TYPE_INFO, MSG_LOCKED)
	_Save()
	return True


def OnMove(src, dst, count):
	"""A move packet from the bag's own window, before it is sent: a locked
	item moved whole takes its lock along."""
	if not IsBagCell(src) or src == dst or not IsLocked(src):
		return
	have = player.GetItemCount(src)
	if count and count < have:
		return  # a part cut off: the stack left behind keeps the lock
	if not IsBagCell(dst):
		return  # worn, the belt, the horse's page: the lock ends with the next item here
	vnum = player.GetItemIndex(src)
	there = player.GetItemIndex(dst)
	if there and there != vnum:
		return  # the engine moves nothing onto another item
	_state['locks'][dst] = _state['locks'][src]
	if not there:
		del _state['locks'][src]
	_Save()


def Prune(emptyToo=False):
	"""Ends the locks whose item is gone: another item in the cell always,
	an empty cell only when asked (a sort, which fills it)."""
	if not _Load():
		return
	changed = False
	for cell, locked in _state['locks'].items():
		vnum = player.GetItemIndex(cell)
		if vnum:
			if not _SameItem(locked, vnum):
				del _state['locks'][cell]
				changed = True
			elif vnum != locked:
				_state['locks'][cell] = vnum  # refined: the lock follows the new vnum
				changed = True
		elif emptyToo:
			del _state['locks'][cell]
			changed = True
	if changed:
		_Save()


def KeepArgument():
	"""The locked cells for /inventory_arrange: "keep=<hex>", or "" when
	nothing is locked."""
	Prune(True)
	cells = sorted(cell for cell in _state['locks'] if IsLocked(cell))
	if not cells:
		return ''
	digits = [0] * (cells[-1] // 4 + 1)
	for cell in cells:
		digits[cell // 4] |= 1 << (cell % 4)
	return 'keep=' + ''.join('%x' % d for d in digits)


def AppendToolTip(tooltip, cell):
	if tooltip and IsLocked(cell):
		tooltip.AppendSpace(5)
		tooltip.AppendTextLine(TOOLTIP_LOCKED, TOOLTIP_COLOR)
