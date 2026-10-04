# MT2009_PLUS_COLLECTOR_STORAGE_V1: "Magazyn kolekcjonera", the collector's
# storage - one store for the whole account beside the classic safebox,
# opened with the "Kolekcjoner" button of the safebox window (uisafebox.py).
# The upstream window's features (server-patches/collector/README.md),
# rewritten so that a move is instant, as the companion's quick transfer is
# (the owner, 3 October: "to ma byc instant, tak jak przerzucanie miedzy
# towarzyszem a naszym eq").
#
# The window holds the whole store: the server sends it once when it opens
# and from then on only what a move changed (playerbot_collector.cpp):
#   COLL begin <tier> <capacity> <entries> <tiers ok>
#   COLL e <entry> <entry> ...      (several lines)
#   COLL end <entries>
#   COLL set <entry> | COLL del <id> | COLL res <op> <code> <units>
#   COLL tier <tier> <capacity> | COLL close <reason> | COLL err <code>
# an entry being "id,vnum,count,s0,s1,s2[,type:value;...x7]". The window
# lays the pages out, filters by category and searches by name itself, so a
# category, a page or a letter typed never asks the server anything.
#
# A move is one command ("/kolekcjoner put|putall|take ..."), sent at once -
# no queue, no pause, no "the last move is still going": the server keeps no
# flood guard for it but its own generous one. And the window does not wait
# for the answer either: the move shows the moment it is made (an overlay over
# what the server last said), and the server's "set"/"del" and "res" replace
# it - an item refused comes back by itself.
#
# How a player uses it, with the window open:
#   * bag -> store: a right click on an item in the bag, or a drag onto the
#     grid; Shift + right click asks how many; Ctrl + right click puts every
#     stack of that kind (the sort-locked cells stay, inventorysortlock.py);
#   * store -> bag: a right click or a double click on an entry takes a stack
#     (one stack of a stackable kind, topping up the bag's stacks of it);
#     Ctrl + right click takes all of it that fits; Shift asks how many; a
#     drag onto the bag puts it in that cell;
#   * stackable items without bonuses are one entry a kind, whatever the
#     count; the categories on the left and the search field filter; the
#     arrows under the grid turn the pages; "Rozbuduj" buys more entries.
#
# Images: mt2009_ui/collector/* in the root pack (category icons, the
# category buttons, the slot, the search button, the bar). A picture that
# cannot be found falls back to the client's own or to nothing - never an
# exception (the 2.0.46 crash over quest_checked.tga).
#
# Python 2.7 as the client has it; the texts are CP1250 escapes so the file
# stays ASCII, each a Polish and English pair (playerbot_lang.T).

from _weakref import proxy

import app
import chat
import item
import mouseModule
import net
import player
import snd
import ui
import wndMgr

from playerbot_lang import T

try:
	import pack
except ImportError:
	pack = None

COMMAND = '/kolekcjoner'
IMG = 'mt2009_ui/collector/'
SLOT_FALLBACK = 'd:/ymir work/ui/public/Slot_Base.sub'
BUTTON_SMALL = ('d:/ymir work/ui/public/small_button_01.sub', 'd:/ymir work/ui/public/small_button_02.sub',
	'd:/ymir work/ui/public/small_button_03.sub')
BUTTON_LARGE = ('d:/ymir work/ui/public/large_button_01.sub', 'd:/ymir work/ui/public/large_button_02.sub',
	'd:/ymir work/ui/public/large_button_03.sub')

# A slot type no stock script attaches (0-11 are the engine's; the companion's
# bag uses 101).
SLOT_TYPE_COLLECTOR = 102

COLS = 10
ROWS = 10
CELLS = COLS * ROWS
CELL = 32

PENDING_TIMEOUT = 6.0
MAX_DISTANCE = 1400
STACK_GUESS = 200

# EResult (playerbot_collector.h)
RESULT_DONE, RESULT_PARTIAL, RESULT_BUSY, RESULT_NO_SESSION, RESULT_NO_ITEM, RESULT_REFUSED, RESULT_FULL, \
	RESULT_NO_ROOM, RESULT_TOO_FAR, RESULT_BAD_REQUEST, RESULT_DEAD, RESULT_LEVEL, RESULT_GOLD, RESULT_MAX_TIER, \
	RESULT_COOLDOWN, RESULT_LOADING, RESULT_DISABLED = range(17)

# The tiers: (entries, level, yang) - the server's table.
TIERS = ((500, 0, 0), (1000, 20, 100000), (2000, 35, 500000), (3500, 50, 2000000), (5000, 65, 5000000),
	(7500, 80, 10000000), (10000, 90, 20000000))

COLOR_TEXT = 0xFFE8DCC8
COLOR_DIM = 0xFFA89A86
COLOR_COUNT = 0xFFC8B89A
COLOR_GOOD = 0xFF9CD49C
COLOR_ERROR = 0xFFFF8A80
COLOR_GOLD = 0xFFFFD27F
COLOR_HOVER = 0x30FFF0C8
COLOR_SELECTED_TEXT = 0xFFFFE6A0

MSG = {
	RESULT_PARTIAL: T('Przeniesiono cz\xea\x9c\xe6 - reszta si\xea nie zmie\x9cci\xb3a.', 'Part of it moved - the rest did not fit.'),
	RESULT_BUSY: T('Zamknij najpierw handel, sklep lub inne okno.', 'Close the trade, the shop or the other window first.'),
	RESULT_NO_SESSION: T('Magazyn nie jest otwarty.', 'The storage is not open.'),
	RESULT_NO_ITEM: T('Tego przedmiotu ju\xbf tam nie ma.', 'That item is not there any more.'),
	RESULT_REFUSED: T('Tego przedmiotu nie mo\xbfna tu schowa\xe6.', 'That item cannot be stored here.'),
	RESULT_FULL: T('Magazyn jest pe\xb3ny - rozbuduj go.', 'The storage is full - expand it.'),
	RESULT_NO_ROOM: T('Nie masz miejsca w ekwipunku.', 'Your inventory has no room.'),
	RESULT_TOO_FAR: T('Jeste\x9c za daleko od magazyniera.', 'You are too far from the storekeeper.'),
	RESULT_BAD_REQUEST: T('Tego nie da si\xea zrobi\xe6.', 'That cannot be done.'),
	RESULT_DEAD: T('Nie mo\xbfesz tego zrobi\xe6 po \x9cmierci.', 'You cannot do that while dead.'),
	RESULT_LEVEL: T('Masz za niski poziom na t\xea rozbudow\xea.', 'Your level is too low for this expansion.'),
	RESULT_GOLD: T('Masz za ma\xb3o Yang na t\xea rozbudow\xea.', 'You do not have enough Yang for this expansion.'),
	RESULT_MAX_TIER: T('Magazyn ma ju\xbf najwi\xeaksz\xb9 pojemno\x9c\xe6.', 'The storage is as large as it gets.'),
	RESULT_COOLDOWN: T('Za szybko - spr\xf3buj jeszcze raz.', 'Too fast - try again.'),
	RESULT_LOADING: T('Magazyn si\xea jeszcze wczytuje.', 'The storage is still loading.'),
	RESULT_DISABLED: T('Rozbudowa jest teraz niedost\xeapna.', 'Expanding is not available now.'),
}
MSG_FAILED = T('Nie uda\xb3o si\xea.', 'That did not work.')

# EOpenError
MSG_OPEN = {
	1: T('Najpierw otw\xf3rz zwyk\xb3y magazyn u magazyniera.', 'Open the safebox at the storekeeper first.'),
	2: T('Magazyn kolekcjonera ju\xbf si\xea otwiera...', "The collector's storage is opening already..."),
	3: T('Nie mo\xbfesz tego zrobi\xe6 po \x9cmierci.', 'You cannot do that while dead.'),
	4: T('Nie uda\xb3o si\xea wczyta\xe6 magazynu - spr\xf3buj jeszcze raz.', 'The storage could not be loaded - try again.'),
	5: T('Jeste\x9c za daleko od magazyniera.', 'You are too far from the storekeeper.'),
}
# ECloseReason
MSG_CLOSE = {
	1: T('Magazyn kolekcjonera zamkni\xeaty - odszed\xb3e\x9c od magazyniera.', "The collector's storage closed - you walked away."),
	2: T('Magazyn kolekcjonera zamkni\xeaty.', "The collector's storage closed."),
}

TITLE = T('Magazyn kolekcjonera', "Collector's storage")
TEXT_CATEGORIES = T('Kategorie', 'Categories')
TEXT_SEARCH = T('Szukaj przedmiotu...', 'Search for an item...')
TEXT_LOADING = T('Wczytywanie magazynu...', 'Loading the storage...')
TEXT_EMPTY = T('Nic tu nie ma.', 'Nothing here.')
TEXT_USED = T('Zaj\xeate: %d / %d', 'Used: %d / %d')
TEXT_UNITS = T('Sztuk: %s', 'Units: %s')
TEXT_HITS = T('Wynik\xf3w: %d', 'Found: %d')
TEXT_EXPAND = T('Rozbuduj', 'Expand')
TEXT_MAXED = T('Maksimum', 'Maximum')
TEXT_HINT = T('PPM: schowaj / wyjmij  Ctrl: wszystkie  Shift: ilo\x9c\xe6', 'RMB: store / take  Ctrl: all  Shift: amount')
TEXT_EXPANDED = T('Magazyn rozbudowany: %d miejsc.', 'The storage was expanded: %d entries.')
TEXT_EXPAND_ASK = T('Rozbudowa do %d miejsc za %s Yang (od %d poziomu). Rozbudowa\xe6?',
	'Expand to %d entries for %s Yang (level %d or higher)?')
TEXT_STORED = T('Schowano: %s szt.', 'Stored: %s')
TEXT_TAKEN = T('Wyj\xeato.', 'Taken out.')
TIP_TAKE = T('PPM / dwuklik: wyjmij stos', 'RMB / double click: take a stack')
TIP_TAKE_ALL = T('Ctrl + PPM: wyjmij wszystko, Shift: ilo\x9c\xe6', 'Ctrl + RMB: take all, Shift: amount')
TIP_COUNT = T('W magazynie: %s szt.', 'Stored: %s')
TIP_SEARCH = T('Wyczy\x9c\xe6 wyszukiwanie', 'Clear the search')
TIP_EXPAND = T('Kolejny poziom: %d miejsc, %s Yang, od %d poziomu postaci', 'Next tier: %d entries, %s Yang, level %d+')

# The categories: (key, Polish, English). The left column's order.
CATEGORIES = (
	('all', 'Wszystko', 'All'),
	('equipment', 'Wyposa\xbfenie', 'Equipment'),
	('materials', 'Ulepszacze', 'Refinement materials'),
	('upgrade', 'Zwoje i bonusy', 'Scrolls and bonuses'),
	('books', 'Ksi\xeagi', 'Books'),
	('stones', 'Kamienie dusz', 'Spirit stones'),
	('herbs', 'Zio\xb3a', 'Herbs'),
	('consumables', 'Zu\xbfywalne', 'Consumables'),
	('chests', 'Skrzynie i klucze', 'Chests and keys'),
	('gathering', '\xa3owienie i kopanie', 'Fishing and mining'),
	('quests', 'Zadania i eventy', 'Quests and events'),
	('appearance', 'Wygl\xb9d postaci', 'Appearance'),
	('other', 'Inne', 'Other'),
)
CATEGORY_INDEX = dict((c[0], i) for i, c in enumerate(CATEGORIES))

# The engine's item types and the USE subtypes the categories read (the
# client's item module names them where it has them).
def _Const(name, default):
	return getattr(item, name, default)

T_WEAPON = _Const('ITEM_TYPE_WEAPON', 1)
T_ARMOR = _Const('ITEM_TYPE_ARMOR', 2)
T_USE = _Const('ITEM_TYPE_USE', 3)
T_AUTOUSE = _Const('ITEM_TYPE_AUTOUSE', 4)
T_MATERIAL = _Const('ITEM_TYPE_MATERIAL', 5)
T_LOTTERY = 8
T_METIN = _Const('ITEM_TYPE_METIN', 10)
T_FISH = _Const('ITEM_TYPE_FISH', 12)
T_ROD = _Const('ITEM_TYPE_ROD', 13)
T_RESOURCE = _Const('ITEM_TYPE_RESOURCE', 14)
T_UNIQUE = _Const('ITEM_TYPE_UNIQUE', 16)
T_SKILLBOOK = _Const('ITEM_TYPE_SKILLBOOK', 17)
T_QUEST = _Const('ITEM_TYPE_QUEST', 18)
T_POLYMORPH = _Const('ITEM_TYPE_POLYMORPH', 19)
T_TREASURE_BOX = _Const('ITEM_TYPE_TREASURE_BOX', 20)
T_TREASURE_KEY = _Const('ITEM_TYPE_TREASURE_KEY', 21)
T_SKILLFORGET = _Const('ITEM_TYPE_SKILLFORGET', 22)
T_GIFTBOX = _Const('ITEM_TYPE_GIFTBOX', 23)
T_PICK = _Const('ITEM_TYPE_PICK', 24)
T_HAIR = _Const('ITEM_TYPE_HAIR', 25)
T_BLEND = _Const('ITEM_TYPE_BLEND', 27)
T_COSTUME = _Const('ITEM_TYPE_COSTUME', 28)
T_RING = _Const('ITEM_TYPE_RING', 33)
T_BELT = _Const('ITEM_TYPE_BELT', 34)

USE_POTION, USE_TALISMAN, USE_TUNING, USE_MOVE, USE_TREASURE_BOX, USE_MONEYBAG, USE_BAIT, USE_ABILITY_UP, \
	USE_AFFECT, USE_CREATE_STONE, USE_SPECIAL, USE_POTION_NODELAY, USE_CLEAR, USE_INVISIBILITY, USE_DETACHMENT, \
	USE_BUCKET, USE_POTION_CONTINUE, USE_CLEAN_SOCKET, USE_CHANGE_ATTRIBUTE, USE_ADD_ATTRIBUTE, \
	USE_ADD_ACCESSORY_SOCKET, USE_PUT_INTO_ACCESSORY_SOCKET, USE_ADD_ATTRIBUTE2, USE_RECIPE, \
	USE_CHANGE_ATTRIBUTE2, USE_BIND, USE_UNBIND = range(27)
UPGRADE_USES = (USE_TUNING, USE_DETACHMENT, USE_CLEAN_SOCKET, USE_CHANGE_ATTRIBUTE, USE_ADD_ATTRIBUTE,
	USE_ADD_ACCESSORY_SOCKET, USE_PUT_INTO_ACCESSORY_SOCKET, USE_ADD_ATTRIBUTE2, USE_CHANGE_ATTRIBUTE2,
	USE_BIND, USE_UNBIND, 29, 30)
CONSUMABLE_USES = (USE_POTION, USE_TALISMAN, USE_MOVE, USE_MONEYBAG, USE_ABILITY_UP, USE_AFFECT, USE_SPECIAL,
	USE_POTION_NODELAY, USE_CLEAR, USE_INVISIBILITY, USE_POTION_CONTINUE)

# Polish capitals to small ones in CP1250, for the search.
_LOWER = {'\xa5': '\xb9', '\xc6': '\xe6', '\xca': '\xea', '\xa3': '\xb3', '\xd1': '\xf1', '\xd3': '\xf3',
	'\x8c': '\x9c', '\x8f': '\x9f', '\xaf': '\xbf'}


def Lower(text):
	text = (text or '').lower()
	return ''.join(_LOWER.get(c, c) for c in text)


def Money(value):
	text = str(int(value))
	out = []
	while len(text) > 3:
		out.insert(0, text[-3:])
		text = text[:-3]
	out.insert(0, text)
	return ' '.join(out)


def _Int(value, default=0):
	try:
		return int(value)
	except (TypeError, ValueError):
		return default


def Exists(path):
	"""Whether the client's packs hold this file; False when it cannot say."""
	if not path:
		return False
	if path.startswith('d:/ymir work/ui/public/'):
		return True
	try:
		return bool(pack and pack.Exist(path))
	except Exception:
		return False


def Image(name):
	"""A picture of ours, or None when the player's packs lack it."""
	path = IMG + name
	return path if Exists(path) else None


def Say(text):
	if text:
		chat.AppendChat(chat.CHAT_TYPE_INFO, text)


def Pressed(*keys):
	for key in keys:
		try:
			if app.IsPressed(key):
				return True
		except Exception:
			pass
	return False


def ShiftPressed():
	return Pressed(app.DIK_LSHIFT, getattr(app, 'DIK_RSHIFT', app.DIK_LSHIFT))


def CtrlPressed():
	return Pressed(app.DIK_LCONTROL, getattr(app, 'DIK_RCONTROL', app.DIK_LCONTROL))


def Send(text):
	"""One command, now - only while the connection is in the game phase
	(warpsafe.py)."""
	try:
		import warpsafe
		if not warpsafe.InGame():
			return False
	except ImportError:
		pass
	net.SendChatPacket(text)
	return True


# ---------------------------------------------------------------- item facts

_facts = {}


def Facts(vnum):
	"""(category, height, stackable, lower-case name, name) of a vnum, read once."""
	got = _facts.get(vnum)
	if got:
		return got
	category, height, stacks, name = 'other', 1, False, ''
	try:
		item.SelectItem(vnum)
		name = item.GetItemName() or ''
		height = max(1, min(3, item.GetItemSize()[1]))
		stacks = bool(item.IsFlag(item.ITEM_FLAG_STACKABLE))
		anti = getattr(item, 'ITEM_ANTIFLAG_STACK', 0)
		if stacks and anti and item.IsAntiFlag(anti):
			stacks = False
		category = Classify(vnum, item.GetItemType(), item.GetItemSubType())
	except Exception:
		pass
	got = (category, height, stacks, Lower(name), name)
	_facts[vnum] = got
	return got


def Classify(vnum, itemType, subType):
	if 50701 <= vnum <= 50760:
		return 'herbs'
	if 27800 <= vnum <= 27999 or 50601 <= vnum <= 50640 or itemType in (T_FISH, T_ROD, T_PICK):
		return 'gathering'
	if itemType in (T_WEAPON, T_ARMOR, T_RING, T_BELT, T_UNIQUE):
		return 'equipment'
	if itemType in (T_SKILLBOOK, T_SKILLFORGET, T_POLYMORPH):
		return 'books'
	if itemType == T_METIN:
		return 'stones'
	if itemType in (T_GIFTBOX, T_TREASURE_BOX, T_TREASURE_KEY, T_LOTTERY):
		return 'chests'
	if itemType in (T_COSTUME, T_HAIR):
		return 'appearance'
	if itemType == T_QUEST:
		return 'quests'
	if itemType in (T_MATERIAL, T_RESOURCE) or 30000 <= vnum <= 30999:
		return 'materials'
	if itemType == T_USE:
		if subType in UPGRADE_USES:
			return 'upgrade'
		if subType == USE_TREASURE_BOX:
			return 'chests'
		if subType == USE_BAIT:
			return 'gathering'
		if subType in CONSUMABLE_USES:
			return 'consumables'
	if itemType in (T_AUTOUSE, T_BLEND):
		return 'consumables'
	return 'other'


# ---------------------------------------------------------------- the store

class Store(object):
	"""What the server last said, the moves still unanswered over it, and the
	picture the window draws from both."""

	def __init__(self):
		self.Reset()

	def Reset(self):
		self.entries = {}		# id -> [vnum, count, sockets, attrs or None]
		self.pending = {}		# op -> (kind, data, time)
		self.tier = 0
		self.capacity = TIERS[0][0]
		self.tiersOk = True
		self.loaded = False
		self.nextOp = 1

	@staticmethod
	def Parse(token):
		parts = token.split(',')
		if len(parts) < 6:
			return None
		try:
			entryId = int(parts[0])
			vnum = int(parts[1])
			count = int(parts[2])
			sockets = (int(parts[3]), int(parts[4]), int(parts[5]))
			attrs = None
			if len(parts) > 6 and parts[6]:
				attrs = []
				for pair in parts[6].split(';'):
					attrType, attrValue = pair.split(':', 1)
					attrs.append((int(attrType), int(attrValue)))
		except ValueError:
			return None
		return entryId, [vnum, count, sockets, attrs]

	def Op(self, kind, data):
		op = self.nextOp
		self.nextOp = op + 1 if op < 2000000000 else 1
		self.pending[op] = (kind, data, app.GetTime())
		return op

	def Expire(self):
		now = app.GetTime()
		gone = [op for op, (kind, data, t) in self.pending.items() if now - t > PENDING_TIMEOUT]
		for op in gone:
			del self.pending[op]
		return bool(gone)

	def Picture(self):
		"""The entries as the window shows them: the server's, with every move
		not answered yet already made."""
		shown = dict((entryId, list(e)) for entryId, e in self.entries.items())
		kinds = None
		for op in sorted(self.pending):
			kind, data, t = self.pending[op]
			if kind == 'take':
				entryId, n = data
				e = shown.get(entryId)
				if e:
					e[1] -= n
					if e[1] <= 0:
						del shown[entryId]
			elif kind in ('put', 'putall'):
				for one in (data if kind == 'putall' else (data,)):
					kinds = self.__Put(shown, kinds, op, one)
		return shown

	@staticmethod
	def __Put(shown, kinds, op, data):
		"""One bag stack put in, not answered yet: onto the entry of its kind,
		or as an entry of its own (a key below zero) until the answer."""
		vnum, n, sockets, attrs, stacks = data
		target = None
		if stacks and not attrs:
			if kinds is None:
				kinds = {}
				for entryId, e in shown.items():
					if not e[3]:
						kinds.setdefault((e[0], e[2]), entryId)
			target = kinds.get((vnum, sockets))
		if target is not None and target in shown:
			shown[target][1] += n
			return kinds
		key = -op
		while key in shown:
			key -= 1000000000	# a second stack of one "putall"
		shown[key] = [vnum, n, sockets, attrs]
		if stacks and not attrs and kinds is not None:
			kinds[(vnum, sockets)] = key
		return kinds


# ---------------------------------------------------------------- the cursor

_state = {'icon': 0, 'window': None}


def _Controller():
	return getattr(mouseModule, 'mouseController', None)


def IsEntryAttached():
	c = _Controller()
	return bool(c) and c.isAttached() and c.GetAttachedType() == SLOT_TYPE_COLLECTOR


def AttachEntry(owner, entryId, vnum, count):
	"""An entry on the cursor, as the companion's bag does it
	(uisidekickinventory.AttachItem): the icon handed to AttachObject first."""
	c = _Controller()
	if not c or c.isAttached():
		return False
	ReleaseIcon()
	try:
		item.SelectItem(vnum)
		handle = item.GetIconInstance()
		(width, height) = item.GetItemSize()
	except Exception:
		return False
	if not handle:
		return False
	c.AttachedIconHandle = handle
	c.AttachObject(owner, SLOT_TYPE_COLLECTOR, entryId, vnum, count)
	if c.isAttached() and c.GetAttachedType() == SLOT_TYPE_COLLECTOR and c.AttachedIconHandle == handle:
		_state['icon'] = handle
		wndMgr.AttachIcon(SLOT_TYPE_COLLECTOR, vnum, entryId, width, height)
		return True
	if c.isAttached() and c.GetAttachedType() == SLOT_TYPE_COLLECTOR:
		c.DeattachObject()
	if c.AttachedIconHandle == handle:
		c.AttachedIconHandle = 0
	item.DeleteIconInstance(handle)
	return False


def ReleaseIcon(force=False):
	handle = _state['icon']
	if not handle:
		return
	c = _Controller()
	held = bool(c) and c.isAttached() and c.AttachedIconHandle == handle
	if held:
		if not force:
			return
		c.DeattachObject()
	try:
		item.DeleteIconInstance(handle)
	except Exception:
		pass
	_state['icon'] = 0


def Deattach():
	c = _Controller()
	if c and c.isAttached():
		c.DeattachObject()
	ReleaseIcon()


# ---------------------------------------------------------------- the window

class CategoryRow(ui.Window):
	"""A category: its round icon, the button with its name and its count."""

	def __init__(self, owner, index, key, name):
		ui.Window.__init__(self)
		self.owner = proxy(owner)
		self.index = index
		self.key = key
		self.selected = False
		self.over = False
		self.SetSize(178, 26)

		self.icons = [Image('cat_%s_%d.tga' % (key, i)) for i in (1, 2, 3)]
		self.icon = None
		if self.icons[0]:
			try:
				icon = ui.ImageBox()
				icon.SetParent(self)
				icon.AddFlag('not_pick')
				icon.LoadImage(self.icons[0])
				icon.SetPosition(0, 0)
				icon.Show()
				self.icon = icon
			except Exception:
				self.icon = None

		button = ui.Button()
		button.SetParent(self)
		images = [Image('catbtn_%02d.tga' % i) for i in (1, 2, 3)]
		x = 28 if self.icon else 4
		try:
			if all(images):
				button.SetUpVisual(images[0])
				button.SetOverVisual(images[1])
				button.SetDownVisual(images[2])
			else:
				raise RuntimeError('no category button')
		except Exception:
			button.SetUpVisual(BUTTON_LARGE[0])
			button.SetOverVisual(BUTTON_LARGE[1])
			button.SetDownVisual(BUTTON_LARGE[2])
		button.SetPosition(x, 3)
		button.SetEvent(ui.__mem_func__(self.OnClick))
		button.Show()
		self.button = button

		label = ui.TextLine()
		label.SetParent(button)
		label.AddFlag('not_pick')
		label.SetPosition(10, 3)
		label.SetText(name)
		label.SetPackedFontColor(COLOR_TEXT)
		label.Show()
		self.label = label

		count = ui.TextLine()
		count.SetParent(button)
		count.AddFlag('not_pick')
		count.SetPosition(button.GetWidth() - 8, 3)
		count.SetHorizontalAlignRight()
		count.SetPackedFontColor(COLOR_COUNT)
		count.Show()
		self.count = count

	def __del__(self):
		ui.Window.__del__(self)

	def Destroy(self):
		self.icon = None
		self.button = None
		self.label = None
		self.count = None

	def OnClick(self):
		try:
			self.owner.SelectCategory(self.key)
		except ReferenceError:
			pass

	def SetCount(self, n):
		self.count.SetText(str(n) if n else '')

	def SetSelected(self, selected):
		self.selected = selected
		if selected:
			self.button.Down()
		else:
			self.button.SetUp()
		self.label.SetPackedFontColor(COLOR_SELECTED_TEXT if selected else COLOR_TEXT)
		self.__Icon()

	def __Icon(self):
		if not self.icon:
			return
		state = 2 if self.selected else 0
		try:
			self.icon.LoadImage(self.icons[state] or self.icons[0])
		except Exception:
			pass


class CollectorWindow(ui.BoardWithTitleBar):

	WIDTH = 536
	HEIGHT = 474
	LEFT_X = 12
	GRID_X = 202
	GRID_Y = 62

	def __init__(self):
		ui.BoardWithTitleBar.__init__(self)
		self.store = Store()
		self.category = 'all'
		self.search = ''
		self.page = 0
		self.pages = [[]]
		self.cellOf = {}			# cell -> entry id (the page shown)
		self.dirty = True
		self.status = ''
		self.statusUntil = 0.0
		self.openPos = None
		self.nextDistanceCheck = 0.0
		self.tooltip = None
		self.shown = {}
		self.pickDialog = None
		self.pickTarget = None
		self.question = None
		self.rows = []
		self.widgets = []
		self.AddFlag('movable')
		self.AddFlag('float')
		self.SetSize(self.WIDTH, self.HEIGHT)
		self.SetTitleName(TITLE)
		self.SetCloseEvent(ui.__mem_func__(self.Close))
		self.__Build()
		self.Hide()

	def __del__(self):
		ui.BoardWithTitleBar.__del__(self)

	# ------------------------------------------------------------ building

	def __Text(self, parent, x, y, text='', color=COLOR_TEXT):
		line = ui.TextLine()
		line.SetParent(parent)
		line.AddFlag('not_pick')
		line.SetPosition(x, y)
		line.SetText(text)
		line.SetPackedFontColor(color)
		line.Show()
		self.widgets.append(line)
		return line

	def __Picture(self, parent, name, x, y, expanded=False):
		path = Image(name)
		if not path:
			return None
		try:
			image = ui.ExpandedImageBox() if expanded else ui.ImageBox()
			image.SetParent(parent)
			image.AddFlag('not_pick')
			image.LoadImage(path)
			image.SetPosition(x, y)
			image.Show()
		except Exception:
			return None
		self.widgets.append(image)
		return image

	def __Button(self, x, y, text, event, visuals=BUTTON_SMALL, tooltip=None):
		button = ui.Button()
		button.SetParent(self)
		button.SetPosition(x, y)
		button.SetUpVisual(visuals[0])
		button.SetOverVisual(visuals[1])
		button.SetDownVisual(visuals[2])
		if text:
			button.SetText(text)
		if tooltip:
			button.SetToolTipText(tooltip)
		button.SetEvent(ui.__mem_func__(event))
		button.Show()
		self.widgets.append(button)
		return button

	def __Build(self):
		left = self.LEFT_X
		header = self.__Picture(self, 'header.tga', left, 34)
		self.__Text(self, left + 80, 37, TEXT_CATEGORIES, COLOR_GOLD).SetHorizontalAlignCenter()
		english = T(False, True)
		for i, (key, pl, en) in enumerate(CATEGORIES):
			row = CategoryRow(self, i, key, en if english else pl)
			row.SetParent(self)
			row.SetPosition(left, 60 + i * 27)
			row.Show()
			self.rows.append(row)

		self.expandButton = self.__Button(left + 30, 418, TEXT_EXPAND, self.OnExpand, BUTTON_LARGE)

		# The search field.
		x = self.GRID_X
		if not self.__Picture(self, 'input.png', x, 35):
			bar = ui.Bar()
			bar.SetParent(self)
			bar.AddFlag('not_pick')
			bar.SetPosition(x, 35)
			bar.SetSize(156, 21)
			bar.SetColor(0xFF0A0A0A)
			bar.Show()
			self.widgets.append(bar)
		edit = ui.EditLine()
		edit.SetParent(self)
		edit.SetPosition(x + 6, 39)
		edit.SetSize(146, 16)
		edit.SetMax(24)
		edit.SetEscapeEvent(ui.__mem_func__(self.OnSearchEscape))
		edit.SetReturnEvent(ui.__mem_func__(self.OnSearchReturn))
		edit.Show()
		self.editSearch = edit
		self.searchHint = self.__Text(self, x + 7, 39, TEXT_SEARCH, COLOR_DIM)
		images = [Image('search_%02d.tga' % i) for i in (1, 2, 3)]
		if all(images):
			self.searchButton = self.__Button(x + 160, 34, '', self.OnClearSearch, images, TIP_SEARCH)
		else:
			self.searchButton = self.__Button(x + 160, 35, 'X', self.OnClearSearch, BUTTON_SMALL, TIP_SEARCH)
		self.hitsText = self.__Text(self, x + 320, 39, '', COLOR_DIM)
		self.hitsText.SetHorizontalAlignRight()

		# The grid.
		grid = ui.GridSlotWindow()
		grid.SetParent(self)
		grid.SetPosition(self.GRID_X, self.GRID_Y)
		grid.ArrangeSlot(0, COLS, ROWS, CELL, CELL, 0, 0)
		base = Image('slot.tga')
		try:
			grid.SetSlotBaseImage(base or SLOT_FALLBACK, 1.0, 1.0, 1.0, 1.0)
		except Exception:
			grid.SetSlotBaseImage(SLOT_FALLBACK, 1.0, 1.0, 1.0, 1.0)
		grid.SetSelectEmptySlotEvent(ui.__mem_func__(self.OnSelectEmptySlot))
		grid.SetSelectItemSlotEvent(ui.__mem_func__(self.OnSelectItemSlot))
		grid.SetUnselectItemSlotEvent(ui.__mem_func__(self.OnRightClickSlot))
		grid.SetUseSlotEvent(ui.__mem_func__(self.OnUseSlot))
		grid.SetOverInItemEvent(ui.__mem_func__(self.OnOverInItem))
		grid.SetOverOutItemEvent(ui.__mem_func__(self.OnOverOutItem))
		grid.RefreshSlot()
		grid.Show()
		self.grid = grid

		hover = ui.Bar()
		hover.SetParent(self)
		hover.AddFlag('not_pick')
		hover.SetColor(COLOR_HOVER)
		hover.Hide()
		self.hover = hover

		self.emptyText = self.__Text(self, self.GRID_X + COLS * CELL / 2, self.GRID_Y + ROWS * CELL / 2 - 6, '', COLOR_DIM)
		self.emptyText.SetHorizontalAlignCenter()

		# The pages.
		y = self.GRID_Y + ROWS * CELL + 6
		self.__Button(self.GRID_X, y, '<<', self.OnFirstPage)
		self.__Button(self.GRID_X + 46, y, '<', self.OnPreviousPage)
		self.__Button(self.GRID_X + COLS * CELL - 89, y, '>', self.OnNextPage)
		self.__Button(self.GRID_X + COLS * CELL - 43, y, '>>', self.OnLastPage)
		self.pageText = self.__Text(self, self.GRID_X + COLS * CELL / 2, y + 4, '1 / 1', COLOR_TEXT)
		self.pageText.SetHorizontalAlignCenter()

		# The fill bar and the counts.
		y += 28
		self.usedText = self.__Text(self, self.GRID_X, y, '', COLOR_TEXT)
		self.barEmpty = self.__Picture(self, 'bar_empty.tga', self.GRID_X + 128, y + 3)
		self.barFull = self.__Picture(self, 'bar_full.tga', self.GRID_X + 136, y + 5, True)
		self.unitsText = self.__Text(self, self.GRID_X + COLS * CELL, y, '', COLOR_DIM)
		self.unitsText.SetHorizontalAlignRight()
		self.statusText = self.__Text(self, self.GRID_X, y + 18, '', COLOR_GOOD)
		self.__Text(self, self.LEFT_X + 2, self.HEIGHT - 22, TEXT_HINT, COLOR_DIM)

		self.rows[0].SetSelected(True)

	def Destroy(self):
		self.Hide()
		if self.tooltip:
			self.tooltip.HideToolTip()
		self.tooltip = None
		if self.pickDialog:
			self.pickDialog.Destroy()
		self.pickDialog = None
		if self.question:
			self.question.Close()
		self.question = None
		for row in self.rows:
			row.Destroy()
		self.rows = []
		self.widgets = []
		self.grid = None
		self.hover = None
		self.editSearch = None

	# ------------------------------------------------------------ opening

	def OpenRequested(self):
		self.store.Reset()
		self.page = 0
		self.dirty = True
		self.__SetStatus(TEXT_LOADING)
		self.openPos = player.GetMainCharacterPosition()
		self.__Place()
		self.Show()
		self.SetTop()

	def __Place(self):
		if getattr(self, 'placed', False):
			return
		self.placed = True
		screenWidth = wndMgr.GetScreenWidth()
		x = max(0, screenWidth - 176 - self.WIDTH - 6)
		self.SetPosition(x, 20)

	def Close(self):
		if self.IsShow():
			Send(COMMAND + ' close')
		self.__CloseQuietly()

	def __CloseQuietly(self):
		Deattach()
		self.OnOverOutItem()
		if self.pickDialog:
			self.pickDialog.Close()
		if self.question:
			self.question.Close()
		if self.editSearch:
			self.editSearch.KillFocus()
		self.store.Reset()
		self.Hide()

	def OnPressEscapeKey(self):
		self.Close()
		return True

	# ------------------------------------------------------------ server

	def OnBegin(self, tier, capacity, entries, tiersOk=1, *rest):
		if not self.IsShow():
			self.OpenRequested()
		self.store.Reset()
		self.store.tier = _Int(tier)
		self.store.capacity = _Int(capacity, TIERS[0][0])
		self.store.tiersOk = _Int(tiersOk, 1) != 0
		self.dirty = True

	def OnEntries(self, *tokens):
		for token in tokens:
			got = Store.Parse(token)
			if got:
				self.store.entries[got[0]] = got[1]
		self.dirty = True

	def OnEnd(self, *rest):
		self.store.loaded = True
		self.__SetStatus('')
		self.dirty = True

	def OnSet(self, token='', *rest):
		got = Store.Parse(token)
		if got:
			self.store.entries[got[0]] = got[1]
			self.dirty = True

	def OnDel(self, entryId='0', *rest):
		self.store.entries.pop(_Int(entryId), None)
		self.dirty = True

	def OnResult(self, op, code, units='0', *rest):
		pending = self.store.pending.pop(_Int(op), None)
		code = _Int(code)
		self.dirty = True
		if code == RESULT_DONE:
			if pending and pending[0] == 'expand':
				self.__SetStatus(TEXT_EXPANDED % self.store.capacity)
			elif pending and pending[0] == 'putall':
				self.__SetStatus(TEXT_STORED % Money(_Int(units)))
			return
		if code in (RESULT_NO_SESSION, RESULT_TOO_FAR):
			return		# the close line follows
		self.__SetStatus(MSG.get(code, MSG_FAILED), code != RESULT_PARTIAL)

	def OnTier(self, tier, capacity, *rest):
		self.store.tier = _Int(tier)
		self.store.capacity = _Int(capacity, self.store.capacity)
		self.dirty = True

	def OnServerClose(self, reason='0', *rest):
		if self.IsShow():
			Say(MSG_CLOSE.get(_Int(reason)))
		self.__CloseQuietly()

	def OnError(self, code='0', *rest):
		Say(MSG_OPEN.get(_Int(code), MSG_FAILED))
		if _Int(code) != 2:
			self.__CloseQuietly()

	# ------------------------------------------------------------ moves

	def Ready(self):
		return self.IsShow() and self.store.loaded

	def Put(self, cell, count=0):
		"""The bag's stack in `cell` into the store, `count` of it (0: all)."""
		if not self.Ready():
			return False
		vnum = player.GetItemIndex(cell)
		if not vnum:
			return False
		if self.__Refused(vnum):
			self.__SetStatus(MSG[RESULT_REFUSED], True)
			return True
		have = player.GetItemCount(cell) or 1
		n = have if count <= 0 or count >= have else count
		category, height, stacks, low, name = Facts(vnum)
		sockets = tuple(player.GetItemMetinSocket(cell, j) for j in xrange(3))
		attrs = self.__BagAttrs(cell)
		if not stacks:
			n = have
		op = self.store.Op('put', (vnum, n, sockets, attrs, stacks))
		Send('%s put %d %d %d' % (COMMAND, op, cell, 0 if n == have else n))
		snd.PlaySound('sound/ui/drop.wav')
		self.dirty = True
		return True

	def PutAll(self, cell):
		"""Every stack of the kind in `cell` (the sort-locked cells stay)."""
		if not self.Ready():
			return False
		vnum = player.GetItemIndex(cell)
		if not vnum:
			return False
		if self.__Refused(vnum):
			self.__SetStatus(MSG[RESULT_REFUSED], True)
			return True
		keep = ''
		locked = set()
		try:
			import inventorysortlock
			keep = inventorysortlock.KeepArgument()
			locked = set(c for c in xrange(player.INVENTORY_PAGE_SIZE * 4) if inventorysortlock.IsLocked(c))
		except Exception:
			pass
		category, height, stacks, low, name = Facts(vnum)
		total = getattr(player, 'INVENTORY_DEFAULT_MAX_NUM', 180)
		stacksMoved = []
		for c in xrange(total):
			if player.GetItemIndex(c) != vnum or (c != cell and c in locked):
				continue
			sockets = tuple(player.GetItemMetinSocket(c, j) for j in xrange(3))
			attrs = self.__BagAttrs(c)
			stacksMoved.append((vnum, player.GetItemCount(c) or 1, sockets, attrs, stacks))
		if not stacksMoved:
			return False
		op = self.store.Op('putall', stacksMoved)
		Send(('%s putall %d %d %s' % (COMMAND, op, cell, keep)).strip())
		snd.PlaySound('sound/ui/drop.wav')
		self.dirty = True
		return True

	def Take(self, entryId, count=0, cell=-1):
		"""An entry into the bag: `count` of it, 0 for one stack."""
		if not self.Ready() or entryId <= 0:
			return False
		e = self.store.entries.get(entryId)
		if not e:
			return False
		category, height, stacks, low, name = Facts(e[0])
		if count < 0:
			n = e[1]
		elif count == 0:
			n = min(e[1], STACK_GUESS) if stacks and not e[3] else e[1]
		else:
			n = min(count, e[1])
		op = self.store.Op('take', (entryId, n))
		Send('%s take %d %d %d %d' % (COMMAND, op, entryId, n if count != 0 else 0, cell))
		snd.PlaySound('sound/ui/pick.wav')
		self.dirty = True
		return True

	def Expand(self):
		if not self.Ready():
			return
		op = self.store.Op('expand', None)
		Send('%s expand %d' % (COMMAND, op))

	@staticmethod
	def __Refused(vnum):
		"""What the server refuses whatever happens: ANTI_SAFEBOX."""
		try:
			item.SelectItem(vnum)
			return bool(item.IsAntiFlag(item.ITEM_ANTIFLAG_SAFEBOX))
		except Exception:
			return False

	@staticmethod
	def __BagAttrs(cell):
		attrs = []
		try:
			for i in xrange(player.ATTRIBUTE_SLOT_MAX_NUM):
				attrs.append(tuple(player.GetItemAttribute(cell, i)))
		except Exception:
			return None
		if not any(t or v for t, v in attrs):
			return None
		return attrs

	# ------------------------------------------------------------ the picture

	def __Layout(self, shown):
		"""The shown entries of the view, sorted, on pages of 10 x 10 - each at
		the first cell of its page where it fits."""
		query = Lower(self.search.strip())
		order = CATEGORY_INDEX
		counts = dict((key, 0) for key, pl, en in CATEGORIES)
		listed = []
		units = 0
		for entryId, e in shown.items():
			category, height, stacks, low, name = Facts(e[0])
			counts[category] = counts.get(category, 0) + 1
			counts['all'] += 1
			units += max(0, e[1])
			if self.category != 'all' and category != self.category:
				continue
			if query and query not in low:
				continue
			listed.append((order.get(category, 99), low, e[0], abs(entryId), entryId, height))
		listed.sort()
		pages = []
		occupied = None
		page = None
		for sortKey in listed:
			entryId, height = sortKey[4], sortKey[5]
			placed = -1
			if page is not None:
				placed = self.__Fit(occupied, height)
			if placed < 0:
				page = []
				pages.append(page)
				occupied = [False] * CELLS
				placed = self.__Fit(occupied, height)
				if placed < 0:
					continue
			for r in xrange(height):
				occupied[placed + r * COLS] = True
			page.append((placed, entryId))
		return pages or [[]], counts, len(listed), units

	@staticmethod
	def __Fit(occupied, height):
		for cell in xrange(CELLS):
			if occupied[cell] or cell // COLS + height > ROWS:
				continue
			if all(not occupied[cell + r * COLS] for r in xrange(1, height)):
				return cell
		return -1

	def Refresh(self):
		self.dirty = False
		shown = self.store.Picture()
		self.shown = shown
		self.pages, counts, hits, units = self.__Layout(shown)
		self.page = max(0, min(self.page, len(self.pages) - 1))
		for row in self.rows:
			row.SetCount(counts.get(row.key, 0))
		self.cellOf = {}
		grid = self.grid
		for cell in xrange(CELLS):
			grid.ClearSlot(cell)
		for cell, entryId in self.pages[self.page]:
			e = shown.get(entryId)
			if not e:
				continue
			self.cellOf[cell] = entryId
			try:
				grid.SetItemSlot(cell, e[0], e[1] if e[1] > 1 else 0, socket=e[2])
			except Exception:
				grid.SetItemSlot(cell, e[0], e[1] if e[1] > 1 else 0)
		grid.RefreshSlot()
		used = len(shown)
		capacity = max(1, self.store.capacity)
		self.usedText.SetText(TEXT_USED % (used, capacity))
		self.usedText.SetPackedFontColor(COLOR_ERROR if used >= capacity else COLOR_TEXT)
		self.unitsText.SetText(TEXT_UNITS % Money(units))
		if self.barFull:
			fill = min(1.0, float(used) / capacity)
			try:
				self.barFull.SetRenderingRect(0.0, 0.0, -(1.0 - fill), 0.0)
			except Exception:
				pass
		self.pageText.SetText('%d / %d' % (self.page + 1, len(self.pages)))
		self.hitsText.SetText(TEXT_HITS % hits if self.search.strip() else '')
		if not self.store.loaded:
			self.emptyText.SetText(TEXT_LOADING)
		elif not self.pages[self.page]:
			self.emptyText.SetText(TEXT_EMPTY)
		else:
			self.emptyText.SetText('')
		maxed = self.store.tier >= len(TIERS) - 1
		self.expandButton.SetText(TEXT_MAXED if maxed else TEXT_EXPAND)
		if not maxed:
			entries, level, price = TIERS[self.store.tier + 1]
			self.expandButton.SetToolTipText(TIP_EXPAND % (entries, Money(price), level))

	def __SetStatus(self, text, error=False):
		self.status = text or ''
		self.statusText.SetText(self.status)
		self.statusText.SetPackedFontColor(COLOR_ERROR if error else COLOR_GOOD)
		self.statusUntil = app.GetTime() + 6.0 if text else 0.0

	# ------------------------------------------------------------ the player

	def SelectCategory(self, key):
		self.category = key
		self.page = 0
		for row in self.rows:
			row.SetSelected(row.key == key)
		self.dirty = True

	def OnFirstPage(self):
		self.page = 0
		self.dirty = True

	def OnPreviousPage(self):
		self.page = max(0, self.page - 1)
		self.dirty = True

	def OnNextPage(self):
		self.page = min(len(self.pages) - 1, self.page + 1)
		self.dirty = True

	def OnLastPage(self):
		self.page = len(self.pages) - 1
		self.dirty = True

	def OnClearSearch(self):
		self.editSearch.SetText('')
		self.search = ''
		self.page = 0
		self.dirty = True

	def OnSearchEscape(self):
		if self.editSearch.GetText():
			self.OnClearSearch()
		else:
			self.editSearch.KillFocus()
		return True

	def OnSearchReturn(self):
		self.editSearch.KillFocus()
		return True

	def OnExpand(self):
		if self.store.tier >= len(TIERS) - 1:
			self.__SetStatus(MSG[RESULT_MAX_TIER], True)
			return
		entries, level, price = TIERS[self.store.tier + 1]
		try:
			import uiCommon
			question = uiCommon.QuestionDialog()
			question.SetText(TEXT_EXPAND_ASK % (entries, Money(price), level))
			question.SetAcceptEvent(ui.__mem_func__(self.OnExpandAccept))
			question.SetCancelEvent(ui.__mem_func__(self.OnExpandCancel))
			question.Open()
			self.question = question
		except Exception:
			self.Expand()

	def OnExpandAccept(self):
		self.OnExpandCancel()
		self.Expand()

	def OnExpandCancel(self):
		if self.question:
			self.question.Close()
		self.question = None

	def __EntryAt(self, cell):
		entryId = self.cellOf.get(cell)
		if entryId is None:
			return None, None
		return entryId, self.shown.get(entryId)

	def __DropFromBag(self):
		"""A bag item on the cursor let go over the grid: into the store."""
		c = _Controller()
		if not c or not c.isAttached():
			return False
		if c.GetAttachedType() == player.SLOT_TYPE_INVENTORY:
			cell = c.GetAttachedSlotNumber()
			count = c.GetAttachedItemCount()
			have = player.GetItemCount(cell)
			c.DeattachObject()
			self.Put(cell, 0 if count >= have else count)
			return True
		if c.GetAttachedType() == SLOT_TYPE_COLLECTOR:
			Deattach()
			return True
		c.DeattachObject()
		return True

	def OnSelectEmptySlot(self, cell):
		self.__DropFromBag()

	def OnSelectItemSlot(self, cell):
		if self.__DropFromBag():
			return
		entryId, e = self.__EntryAt(cell)
		if not e or entryId <= 0:
			return
		if ShiftPressed():
			self.__AskCount(entryId, e)
			return
		self.OnOverOutItem()
		AttachEntry(self, entryId, e[0], e[1])

	def OnRightClickSlot(self, cell):
		if IsEntryAttached():
			return
		entryId, e = self.__EntryAt(cell)
		if not e or entryId <= 0:
			return
		if ShiftPressed():
			self.__AskCount(entryId, e)
		elif CtrlPressed():
			self.Take(entryId, -1)
		else:
			self.Take(entryId, 0)
		self.OnOverOutItem()

	def OnUseSlot(self, cell):
		entryId, e = self.__EntryAt(cell)
		if e and entryId > 0:
			self.Take(entryId, 0)
			self.OnOverOutItem()

	def __AskCount(self, entryId, e):
		try:
			import uiPickEtc
			if not self.pickDialog:
				dialog = uiPickEtc.PickETCDialog()
				dialog.LoadDialog()
				dialog.SetAcceptEvent(ui.__mem_func__(self.OnPickCount))
				self.pickDialog = dialog
			self.pickTarget = entryId
			self.pickDialog.SetTitleName(Facts(e[0])[4])
			self.pickDialog.Open(e[1], 1)
		except Exception:
			self.Take(entryId, 0)

	def OnPickCount(self, count, *rest):
		entryId = self.pickTarget
		self.pickTarget = None
		if entryId and count > 0:
			self.Take(entryId, int(count))

	def AskBagCount(self, cell):
		"""Shift + right click in the bag: how many of the stack go in."""
		have = player.GetItemCount(cell)
		if have <= 1:
			return self.Put(cell, 0)
		try:
			import uiPickEtc
			if not getattr(self, 'bagPick', None):
				dialog = uiPickEtc.PickETCDialog()
				dialog.LoadDialog()
				dialog.SetAcceptEvent(ui.__mem_func__(self.OnBagPickCount))
				self.bagPick = dialog
			self.bagPickCell = cell
			self.bagPick.Open(have, 1)
			return True
		except Exception:
			return self.Put(cell, 0)

	def OnBagPickCount(self, count, *rest):
		cell = getattr(self, 'bagPickCell', -1)
		if cell >= 0 and count > 0:
			self.Put(cell, int(count))

	def OnOverInItem(self, cell):
		entryId, e = self.__EntryAt(cell)
		if not e or IsEntryAttached():
			return
		height = Facts(e[0])[1]
		self.hover.SetPosition(self.GRID_X + (cell % COLS) * CELL, self.GRID_Y + (cell // COLS) * CELL)
		self.hover.SetSize(CELL, CELL * height)
		self.hover.Show()
		if not self.tooltip:
			try:
				import uiToolTip
				self.tooltip = uiToolTip.ItemToolTip()
				self.tooltip.HideToolTip()
			except Exception:
				return
		tip = self.tooltip
		tip.ClearToolTip()
		attrs = e[3] or [(0, 0)] * 7
		try:
			tip.AddItemData(e[0], list(e[2]), attrs)
		except Exception:
			try:
				tip.SetItemToolTip(e[0])
			except Exception:
				return
		try:
			tip.AppendSpace(4)
			if e[1] > 1:
				tip.AppendTextLine(TIP_COUNT % Money(e[1]), 0xFFFFD27F)
			tip.AppendTextLine(TIP_TAKE, 0xFFA8A8A8)
			tip.AppendTextLine(TIP_TAKE_ALL, 0xFFA8A8A8)
		except Exception:
			pass
		tip.ShowToolTip()

	def OnOverOutItem(self):
		if self.hover:
			self.hover.Hide()
		if self.tooltip:
			self.tooltip.HideToolTip()

	def OnUpdate(self):
		now = app.GetTime()
		if self.store.Expire():
			self.dirty = True
		if self.editSearch:
			text = self.editSearch.GetText()
			if text != self.search:
				self.search = text
				self.page = 0
				self.dirty = True
			if text or self.editSearch.IsFocus():
				self.searchHint.Hide()
			else:
				self.searchHint.Show()
		if self.dirty:
			self.Refresh()
		if self.statusUntil and now > self.statusUntil:
			self.__SetStatus('')
		if now >= self.nextDistanceCheck and self.openPos:
			self.nextDistanceCheck = now + 0.5
			try:
				x, y, z = player.GetMainCharacterPosition()
				ox, oy, oz = self.openPos
				if abs(x - ox) > MAX_DISTANCE or abs(y - oy) > MAX_DISTANCE:
					Say(MSG_CLOSE[1])
					self.Close()
			except Exception:
				pass


# ---------------------------------------------------------------- module API

def GetWindow(create=True):
	window = _state['window']
	if window is None and create:
		window = CollectorWindow()
		_state['window'] = window
	return window


def IsOpen():
	window = GetWindow(False)
	return bool(window) and window.IsShow()


def RequestOpen():
	"""The safebox window's "Kolekcjoner" button."""
	window = GetWindow()
	if window.IsShow():
		window.SetTop()
		return
	if Send(COMMAND + ' open'):
		window.OpenRequested()


def QuickPut(cell):
	"""A right click on the bag with the window open (uiinventory.py): True
	when the click was the store's."""
	if not IsOpen() or not GetWindow().Ready():
		return False
	c = _Controller()
	if c and c.isAttached():
		return False
	if not (0 <= cell < getattr(player, 'INVENTORY_DEFAULT_MAX_NUM', 180)):
		return False
	if not player.GetItemIndex(cell):
		return False
	window = GetWindow()
	if CtrlPressed():
		return window.PutAll(cell)
	if ShiftPressed():
		return window.AskBagCount(cell)
	return window.Put(cell, 0)


def DropIntoPlayerBag(attachedType, attachedPos, cell):
	"""An entry from the cursor let go over the bag (uiinventory.py): True
	when it was ours and the order went."""
	if attachedType != SLOT_TYPE_COLLECTOR:
		return False
	window = GetWindow(False)
	if window and window.IsShow():
		count = 0
		c = _Controller()
		e = window.store.entries.get(attachedPos)
		if c and e:
			count = c.GetAttachedItemCount()
			if count >= e[1]:
				count = 0
		if not (0 <= cell < getattr(player, 'INVENTORY_DEFAULT_MAX_NUM', 180)):
			cell = -1
		window.Take(attachedPos, count, cell)
	ReleaseIcon(True)
	return True


def OnServer(word='', *args):
	"""Every "COLL ..." line of the server (game.py)."""
	window = GetWindow()
	handler = {
		'begin': window.OnBegin,
		'e': window.OnEntries,
		'end': window.OnEnd,
		'set': window.OnSet,
		'del': window.OnDel,
		'res': window.OnResult,
		'tier': window.OnTier,
		'close': window.OnServerClose,
		'err': window.OnError,
	}.get(word)
	if handler:
		try:
			handler(*args)
		except TypeError:
			pass


def Destroy():
	ReleaseIcon(True)
	window = _state['window']
	_state['window'] = None
	if window:
		window.Destroy()


class Keeper(object):
	"""One of the game's updateables: destroys the window with the game window
	(a warp, a logout), and lets go of an entry's icon once the cursor has."""

	def CanUpdate(self):
		return True

	def OnUpdate(self):
		if _state['icon'] and not IsEntryAttached():
			ReleaseIcon()

	def Destroy(self):
		Destroy()


def GetKeeper():
	return Keeper()
