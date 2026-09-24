# Zmieniacz bonusow (bonus switcher): a window that takes one item from the
# bag and spends the bag's change stones on it until its bonuses are the ones
# asked for.
#
# The client does what a player's hand does - drags a change stone onto the
# item, net.SendItemUseToItemPacket - and nothing else: the server rolls
# every change (CHARACTER::UseItem, USE_CHANGE_ATTRIBUTE), so the switcher is
# only a patient hand. Four things on the server shape it:
#   * PulseManager admits five item uses in half a second per character, so
#     one change goes out at a time and the next waits for the answer (the
#     item's bonuses moved, or the stone count fell) - never a burst;
#   * a worn item is refused (item2->IsEquipped()), so only a bag item is
#     taken;
#   * the green stones (71151, 76023) change a weapon or a body armour of
#     level forty or less and refuse anything else without spending the stone,
#     which would be a change that never comes back - they are used only where
#     they work;
#   * an item with no bonus at all has nothing to change ("There is no upgrade
#     that you can change"): an add stone comes first, by hand.
# A bonus's number on this line is the engine's POINT number (item attrtype
# = POINT_*, 122 average damage, 121 skill damage), which is what
# player.GetItemAttribute answers.
#
# game.py opens it with U and registers the switcher with its updateables
# the first time. Settings are kept in bonusswitch.cfg beside the client.
# Python 2.7 as the client has it, and 3 for tests/uibonusswitch_test.py.
# Player-visible strings are CP1250 escapes, so the file itself is ASCII.

import chat
import clientclock
import item
import mouseModule
import net
import player
import ui

try:
    xrange
except NameError:
    xrange = range

CONFIG_FILE = 'bonusswitch.cfg'
WANTED_ROWS = 5
NORMAL_ATTRS = 5
MIN_DELAY_MS = 150
DEFAULT_DELAY_MS = 300
ANSWER_TIMEOUT = 3.0
MAX_SILENT_TRIES = 3
STATUS_INTERVAL = 0.3
# How often the wait for the server's answer looks at the item: the stone
# count is a walk over four pages of the bag.
ANSWER_CHECK_INTERVAL = 0.1

GREEN_CHANGE_STONES = (71151, 76023)
GREEN_MAX_LEVEL = 40

# (POINT number, name). The order is what a player looks for first.
BONUSES = (
    (0,   '- brak -'),
    (6,   'Max. P\xaf'),
    (8,   'Max. PE'),
    (13,  'Witalno\x9c\xe6'),
    (15,  'Inteligencja'),
    (12,  'Si\xb3a'),
    (14,  'Zr\xeaczno\x9c\xe6'),
    (122, '\x8crednie obra\xbfenia'),
    (121, 'Obra\xbfenia umiej\xeatno\x9cci'),
    (40,  'Szansa na krytyczne'),
    (41,  'Szansa na przeszywaj\xb9ce'),
    (17,  'Szybko\x9c\xe6 ataku'),
    (19,  'Szybko\x9c\xe6 ruchu'),
    (21,  'Szybko\x9c\xe6 zakl\xeacia'),
    (43,  'Silny na P\xf3\xb3ludzi'),
    (44,  'Silny na Zwierz\xeata'),
    (45,  'Silny na Orki'),
    (46,  'Silny na Esoteryczne'),
    (47,  'Silny na Nieumar\xb3e'),
    (48,  'Silny na Diab\xb3y'),
    (54,  'Silny na Wojownik\xf3w'),
    (55,  'Silny na Ninja'),
    (56,  'Silny na Sury'),
    (57,  'Silny na Szaman\xf3w'),
    (59,  'Odp. na Wojownika'),
    (60,  'Odp. na Ninj\xea'),
    (61,  'Odp. na Sur\xea'),
    (62,  'Odp. na Szamana'),
    (123, 'Odp. na umiej\xeatno\x9cci'),
    (124, 'Odp. na \x9crednie'),
    (37,  'Szansa na otrucie'),
    (38,  'Szansa na om\xb3ot'),
    (39,  'Szansa na spowolnienie'),
    (63,  'Kradzie\xbf P\xaf'),
    (64,  'Kradzie\xbf PE'),
    (65,  'Szansa na kradzie\xbf PE'),
    (67,  'Blok ataku fizycznego'),
    (68,  'Unik strza\xb3'),
    (69,  'Odp. na miecze'),
    (70,  'Odp. na dwur\xeaczne'),
    (71,  'Odp. na sztylety'),
    (72,  'Odp. na dzwony'),
    (73,  'Odp. na wachlarze'),
    (74,  'Odp. na strza\xb3y'),
    (75,  'Odp. na ogie\xf1'),
    (76,  'Odp. na b\xb3yskawice'),
    (77,  'Odp. na magi\xea'),
    (78,  'Odp. na wiatr'),
    (79,  'Odbicie ataku'),
    (81,  'Odp. na trucizn\xea'),
    (32,  'Regeneracja P\xaf'),
    (33,  'Regeneracja PE'),
    (82,  'PE przy zabiciu'),
    (87,  'P\xaf przy zabiciu'),
    (83,  'Dodatkowe PD'),
    (84,  'Wi\xeacej yang'),
    (85,  'Wi\xeacej drop\xf3w'),
    (88,  'Odporno\x9c\xe6 na om\xb3ot'),
    (89,  'Odporno\x9c\xe6 na spowolnienie'),
    (95,  'Warto\x9c\xe6 ataku'),
)
BONUS_TYPES = tuple(t for t, _ in BONUSES)
BONUS_NAMES = dict(BONUSES)


def BonusName(bonusType):
    return BONUS_NAMES.get(bonusType, 'Bonus %d' % bonusType)


def BonusIndex(bonusType):
    try:
        return BONUS_TYPES.index(bonusType)
    except ValueError:
        return 0


def ReadAttrs(cell):
    """The item's normal bonus lines, (type, value) each; the two rare ones
    after them are not what a change stone rolls."""
    attrs = []
    for i in xrange(NORMAL_ATTRS):
        try:
            attrType, value = player.GetItemAttribute(cell, i)
        except Exception:
            attrType, value = 0, 0
        attrs.append((attrType, value))
    return attrs


def AttrsMatch(attrs, wanted):
    """Every wanted (type, minimum) present on the item at the minimum or
    above; a minimum of zero or less is any positive value. Nothing wanted is
    no match, or Start would stop at once and say it was done."""
    rows = [(t, m) for t, m in wanted if t]
    if not rows:
        return False
    for wantType, minimum in rows:
        found = False
        for attrType, value in attrs:
            if attrType == wantType and value > 0 and value >= minimum:
                found = True
                break
        if not found:
            return False
    return True


def BagCells():
    try:
        return player.INVENTORY_PAGE_SIZE * player.INVENTORY_PAGE_COUNT
    except AttributeError:
        return 45 * 4


def IsChangeStone(vnum):
    try:
        return item.GetUseType(vnum) == 'USE_CHANGE_ATTRIBUTE'
    except Exception:
        return False


def GreenStoneFits(targetVnum):
    """What the server asks of a green stone: a weapon or a body armour whose
    level limit is forty or less."""
    try:
        item.SelectItem(targetVnum)
        itemType = item.GetItemType()
        if itemType == item.ITEM_TYPE_WEAPON:
            pass
        elif itemType == item.ITEM_TYPE_ARMOR and item.GetItemSubType() == getattr(item, 'ARMOR_BODY', 0):
            pass
        else:
            return False
        limitLevel = getattr(item, 'LIMIT_LEVEL', 1)
        for i in xrange(getattr(item, 'LIMIT_MAX_NUM', 2)):
            limitType, limitValue = item.GetLimit(i)
            if limitType == limitLevel and limitValue > GREEN_MAX_LEVEL:
                return False
        return True
    except Exception:
        return False


def FindStone(targetCell, targetVnum):
    """The bag cell of a change stone that works on the item: a green one first
    where it fits (they are the cheap ones), then any ordinary one. -1 when
    there is none. Also the count of the stones that fit."""
    greenOk = GreenStoneFits(targetVnum)
    green, plain, count = -1, -1, 0
    for cell in xrange(BagCells()):
        if cell == targetCell:
            continue
        vnum = player.GetItemIndex(cell)
        if not vnum or not IsChangeStone(vnum):
            continue
        if vnum in GREEN_CHANGE_STONES:
            if not greenOk:
                continue
            if green < 0:
                green = cell
        elif plain < 0:
            plain = cell
        count += player.GetItemCount(cell)
    return (green if green >= 0 else plain), count


def CanTakeItem(cell):
    """Why an item cannot be switched, or '' when it can."""
    vnum = player.GetItemIndex(cell)
    if not vnum:
        return 'Pusty slot.'
    try:
        if player.IsEquipmentSlot(cell):
            return 'Zdejmij przedmiot - noszonego nie da si\xea zmienia\xe6.'
    except Exception:
        pass
    if cell >= BagCells():
        return 'Po\xb3\xf3\xbf przedmiot z plecaka.'
    try:
        item.SelectItem(vnum)
        if item.GetItemType() not in (item.ITEM_TYPE_WEAPON, item.ITEM_TYPE_ARMOR):
            return 'Tylko bro\xf1 i zbroje/ozdoby.'
    except Exception:
        pass
    return ''


class Switcher(object):
    """The loop. game.py registers it with the updateables, so it runs whether
    the window is open or not."""

    def __init__(self):
        self.window = None
        self.cell = -1
        self.vnum = 0
        self.wanted = [(0, 0)] * WANTED_ROWS
        self.delayMs = DEFAULT_DELAY_MS
        self.running = False
        self.waiting = False
        self.sentAt = 0.0
        self.nextSend = 0.0
        self.nextCheck = 0.0
        self.snapshot = None
        self.stonesBefore = 0
        self.silentTries = 0
        self.changes = 0
        self.stonesLeft = 0
        self.LoadConfig()

    # --- settings ----------------------------------------------------------

    def LoadConfig(self):
        try:
            f = open(CONFIG_FILE, 'r')
            try:
                lines = f.read().splitlines()
            finally:
                f.close()
        except Exception:
            return
        values = {}
        for line in lines:
            if '=' in line:
                key, value = line.split('=', 1)
                values[key.strip()] = value.strip()
        try:
            self.delayMs = max(MIN_DELAY_MS, int(values.get('delay', DEFAULT_DELAY_MS)))
        except ValueError:
            self.delayMs = DEFAULT_DELAY_MS
        wanted = []
        for i in xrange(WANTED_ROWS):
            try:
                wantType = int(values.get('row%d_type' % i, 0))
                minimum = int(values.get('row%d_min' % i, 0))
            except ValueError:
                wantType, minimum = 0, 0
            if wantType not in BONUS_NAMES:
                wantType = 0
            wanted.append((wantType, max(0, minimum)))
        self.wanted = wanted

    def SaveConfig(self):
        lines = ['delay=%d' % self.delayMs]
        for i, (wantType, minimum) in enumerate(self.wanted):
            lines.append('row%d_type=%d' % (i, wantType))
            lines.append('row%d_min=%d' % (i, minimum))
        try:
            f = open(CONFIG_FILE, 'w')
            f.write('\n'.join(lines) + '\n')
            f.close()
        except Exception:
            pass

    # --- the item ----------------------------------------------------------

    def SetItem(self, cell):
        if self.running:
            chat.AppendChat(chat.CHAT_TYPE_INFO, 'Zmieniacz: najpierw zatrzymaj.')
            return False
        why = CanTakeItem(cell)
        if why:
            chat.AppendChat(chat.CHAT_TYPE_INFO, 'Zmieniacz: ' + why)
            return False
        self.cell = cell
        self.vnum = player.GetItemIndex(cell)
        return True

    def ClearItem(self):
        if self.running:
            return
        self.cell = -1
        self.vnum = 0

    def ItemStillThere(self):
        return self.cell >= 0 and self.vnum and player.GetItemIndex(self.cell) == self.vnum

    # --- start and stop ----------------------------------------------------

    def Start(self):
        if self.running:
            return
        if not self.ItemStillThere():
            self.cell, self.vnum = -1, 0
            chat.AppendChat(chat.CHAT_TYPE_INFO, 'Zmieniacz: w\xb3\xf3\xbf przedmiot do okna.')
            return
        if not [t for t, _ in self.wanted if t]:
            chat.AppendChat(chat.CHAT_TYPE_INFO, 'Zmieniacz: wybierz przynajmniej jeden bonus.')
            return
        attrs = ReadAttrs(self.cell)
        if not [t for t, v in attrs if t]:
            chat.AppendChat(chat.CHAT_TYPE_INFO, 'Zmieniacz: przedmiot nie ma bonus\xf3w - najpierw dodaj je kamieniem.')
            return
        if AttrsMatch(attrs, self.wanted):
            chat.AppendChat(chat.CHAT_TYPE_INFO, 'Zmieniacz: przedmiot ju\xbf ma te bonusy.')
            return
        stone, count = FindStone(self.cell, self.vnum)
        if stone < 0:
            chat.AppendChat(chat.CHAT_TYPE_INFO, 'Zmieniacz: brak Zaczarowania Przedmiotu, kt\xf3re pasuje do tego przedmiotu.')
            return
        self.SaveConfig()
        self.running = True
        self.waiting = False
        self.silentTries = 0
        self.changes = 0
        self.stonesLeft = count
        self.nextSend = clientclock.Now()
        chat.AppendChat(chat.CHAT_TYPE_INFO, 'Zmieniacz: start, kamieni: %d.' % count)

    def Stop(self, message=None):
        wasRunning = self.running
        self.running = False
        self.waiting = False
        if message:
            chat.AppendChat(chat.CHAT_TYPE_INFO, 'Zmieniacz: ' + message)
        elif wasRunning:
            chat.AppendChat(chat.CHAT_TYPE_INFO, 'Zmieniacz: stop po %d zmianach.' % self.changes)

    # --- the updateable ----------------------------------------------------

    def CanUpdate(self):
        return self.running

    def OnUpdate(self):
        now = clientclock.Now()
        if not self.ItemStillThere():
            self.Stop('przedmiot zmieni\xb3 miejsce - zatrzymano.')
            return

        if self.waiting:
            if now < self.nextCheck:
                return
            self.nextCheck = now + ANSWER_CHECK_INTERVAL
            stones = FindStone(self.cell, self.vnum)[1]
            attrs = ReadAttrs(self.cell)
            if attrs != self.snapshot or stones < self.stonesBefore:
                self.waiting = False
                self.silentTries = 0
                self.changes += 1
                self.stonesLeft = stones
                self.nextSend = max(now, self.sentAt + self.delayMs / 1000.0)
            elif now - self.sentAt > ANSWER_TIMEOUT:
                self.waiting = False
                self.silentTries += 1
                if self.silentTries >= MAX_SILENT_TRIES:
                    self.Stop('serwer nie zmienia bonus\xf3w - zatrzymano.')
                    return
                self.nextSend = now
            else:
                return

        if now < self.nextSend:
            return

        attrs = ReadAttrs(self.cell)
        if AttrsMatch(attrs, self.wanted):
            self.Stop('gotowe po %d zmianach!' % self.changes)
            return

        stone, count = FindStone(self.cell, self.vnum)
        self.stonesLeft = count
        if stone < 0:
            self.Stop('sko\xf1czy\xb3y si\xea kamienie po %d zmianach.' % self.changes)
            return

        self.snapshot = attrs
        self.stonesBefore = count
        self.sentAt = now
        self.waiting = True
        net.SendItemUseToItemPacket(stone, self.cell)

    def Destroy(self):
        self.Stop()
        if self.window:
            self.window.Hide()
            self.window.Destroy()
            self.window = None

    # --- the window --------------------------------------------------------

    def ToggleWindow(self):
        if self.window is None:
            self.window = BonusSwitchWindow(self)
        try:
            shown = self.window.IsShow()
        except RuntimeError:
            self.window = BonusSwitchWindow(self)
            shown = False
        if shown:
            self.window.Close()
        else:
            self.window.Refresh()
            self.window.Show()
            self.window.SetTop()


class BonusSwitchWindow(ui.BoardWithTitleBar):
    WIDTH = 330
    HEIGHT = 470
    ROW_H = 22

    def __init__(self, switcher):
        ui.BoardWithTitleBar.__init__(self)
        self.switcher = switcher
        self.widgets = []
        self.rowNames = []
        self.rowEdits = []
        self.attrLines = []
        self.nextStatus = 0.0
        self.AddFlag('movable')
        self.AddFlag('float')
        self.SetSize(self.WIDTH, self.HEIGHT)
        self.SetTitleName('Zmieniacz bonus\xf3w')
        self.SetCloseEvent(ui.__mem_func__(self.Close))
        self.Build()
        self.SetCenterPosition()

    def Build(self):
        BL = 10
        BW = self.WIDTH - 2 * BL
        y = 32

        itemBoard = self._Board(BL, y, BW, 60)
        self.itemSlot = ui.SlotWindow()
        self.itemSlot.SetParent(itemBoard)
        self.itemSlot.SetPosition(10, 12)
        self.itemSlot.SetSize(32, 32)
        self.itemSlot.AppendSlot(0, 0, 0, 32, 32)
        self.itemSlot.SetSlotBaseImage('d:/ymir work/ui/public/slot_base.sub', 1.0, 1.0, 1.0, 1.0)
        self.itemSlot.SetSelectEmptySlotEvent(ui.__mem_func__(self.OnItemSlot))
        self.itemSlot.SetSelectItemSlotEvent(ui.__mem_func__(self.OnItemSlot))
        self.itemSlot.SetUnselectItemSlotEvent(ui.__mem_func__(self.OnClearItemSlot))
        self.itemSlot.Show()
        self.widgets.append(self.itemSlot)
        self.itemName = self._Label(itemBoard, 52, 12, 'Przeci\xb9gnij tu przedmiot z plecaka')
        self.stoneText = self._Label(itemBoard, 52, 30, '')
        y += 60 + 4

        attrBoard = self._Board(BL, y, BW, 22 + NORMAL_ATTRS * 16 + 4)
        self._Label(attrBoard, 10, 4, 'Obecne bonusy')
        for i in xrange(NORMAL_ATTRS):
            self.attrLines.append(self._Label(attrBoard, 14, 22 + i * 16, ''))
        y += 22 + NORMAL_ATTRS * 16 + 4 + 4

        wantH = 22 + WANTED_ROWS * self.ROW_H + 4
        wantBoard = self._Board(BL, y, BW, wantH)
        self._Label(wantBoard, 10, 4, 'Szukane bonusy (min. warto\x9c\xe6)')
        for i in xrange(WANTED_ROWS):
            ry = 22 + i * self.ROW_H
            self._Btn(wantBoard, 'small', 6, ry, '<', self.OnPrevBonus, i)
            name = self._Label(wantBoard, 56 + 80, ry + 3, '')
            name.SetHorizontalAlignCenter()
            self.rowNames.append(name)
            self._Btn(wantBoard, 'small', 222, ry, '>', self.OnNextBonus, i)
            self.rowEdits.append(self._Edit(wantBoard, 270, ry + 1, 36, 5))
        y += wantH + 4

        setBoard = self._Board(BL, y, BW, 26)
        self._Label(setBoard, 10, 6, 'Odst\xeap mi\xeadzy zmianami (ms)')
        self.delayEdit = self._Edit(setBoard, 250, 4, 56, 5)
        y += 26 + 6

        self.statusText = self._Label(self, BL + 6, y, '')
        y += 20

        bw, gap = 88, 8
        bx = (self.WIDTH - (3 * bw + 2 * gap)) // 2
        self._Btn(self, 'large', bx, y, 'Start', self.OnStart)
        self._Btn(self, 'large', bx + bw + gap, y, 'Zatrzymaj', self.OnStop)
        self._Btn(self, 'large', bx + 2 * (bw + gap), y, 'Wyczy\x9c\xe6', self.OnClearRows)

    def _Board(self, x, y, w, h):
        board = ui.ThinBoard()
        board.SetParent(self)
        board.SetPosition(x, y)
        board.SetSize(w, h)
        board.Show()
        self.widgets.append(board)
        return board

    def _Label(self, parent, x, y, text):
        line = ui.TextLine()
        line.SetParent(parent)
        line.SetPosition(x, y)
        line.SetText(text)
        line.Show()
        self.widgets.append(line)
        return line

    def _Btn(self, parent, size, x, y, text, event, *args):
        button = ui.Button()
        button.SetParent(parent)
        button.SetPosition(x, y)
        button.SetUpVisual('d:/ymir work/ui/public/%s_button_01.sub' % size)
        button.SetOverVisual('d:/ymir work/ui/public/%s_button_02.sub' % size)
        button.SetDownVisual('d:/ymir work/ui/public/%s_button_03.sub' % size)
        button.SetText(text)
        button.SAFE_SetEvent(event, *args)
        button.Show()
        self.widgets.append(button)
        return button

    def _Edit(self, parent, x, y, w, maxLen):
        bar = ui.SlotBar()
        bar.SetParent(parent)
        bar.SetPosition(x, y)
        bar.SetSize(w, 18)
        bar.Show()
        self.widgets.append(bar)
        edit = ui.EditLine()
        edit.SetParent(bar)
        edit.SetPosition(4, 2)
        edit.SetSize(w - 6, 16)
        edit.SetMax(maxLen)
        edit.SetNumberMode()
        edit.Show()
        self.widgets.append(edit)
        return edit

    # --- events ------------------------------------------------------------

    def OnItemSlot(self, slotIndex):
        controller = mouseModule.mouseController
        if not controller.isAttached():
            return
        attachedType = controller.GetAttachedType()
        cell = controller.GetAttachedSlotNumber()
        controller.DeattachObject()
        if attachedType != player.SLOT_TYPE_INVENTORY:
            chat.AppendChat(chat.CHAT_TYPE_INFO, 'Zmieniacz: tylko przedmiot z plecaka.')
            return
        if self.switcher.SetItem(cell):
            self.Refresh()

    def OnClearItemSlot(self, slotIndex):
        self.switcher.ClearItem()
        self.Refresh()

    def OnPrevBonus(self, row):
        self._StepBonus(row, -1)

    def OnNextBonus(self, row):
        self._StepBonus(row, 1)

    def _StepBonus(self, row, step):
        if self.switcher.running:
            return
        self.ReadEdits()
        wantType, minimum = self.switcher.wanted[row]
        index = (BonusIndex(wantType) + step) % len(BONUS_TYPES)
        self.switcher.wanted[row] = (BONUS_TYPES[index], minimum)
        self.Refresh()

    def OnClearRows(self):
        if self.switcher.running:
            return
        self.switcher.wanted = [(0, 0)] * WANTED_ROWS
        self.Refresh()

    def OnStart(self):
        self.ReadEdits()
        self.switcher.Start()
        self.RefreshStatus()

    def OnStop(self):
        self.switcher.Stop()
        self.RefreshStatus()

    def ReadEdits(self):
        if self.switcher.running:
            return
        wanted = []
        for i in xrange(WANTED_ROWS):
            wantType = self.switcher.wanted[i][0]
            try:
                minimum = int(self.rowEdits[i].GetText() or 0)
            except ValueError:
                minimum = 0
            wanted.append((wantType, max(0, minimum)))
        self.switcher.wanted = wanted
        try:
            self.switcher.delayMs = max(MIN_DELAY_MS, int(self.delayEdit.GetText() or DEFAULT_DELAY_MS))
        except ValueError:
            self.switcher.delayMs = DEFAULT_DELAY_MS

    # --- drawing -----------------------------------------------------------

    def Refresh(self):
        switcher = self.switcher
        for i in xrange(WANTED_ROWS):
            wantType, minimum = switcher.wanted[i]
            self.rowNames[i].SetText(BonusName(wantType))
            self.rowEdits[i].SetText(str(minimum) if minimum else '')
        self.delayEdit.SetText(str(switcher.delayMs))
        self.RefreshStatus()

    def RefreshStatus(self):
        switcher = self.switcher
        if switcher.ItemStillThere():
            self.itemSlot.SetItemSlot(0, switcher.vnum, 0)
            try:
                item.SelectItem(switcher.vnum)
                self.itemName.SetText(item.GetItemName())
            except Exception:
                self.itemName.SetText('Przedmiot %d' % switcher.vnum)
            attrs = ReadAttrs(switcher.cell)
            stones = FindStone(switcher.cell, switcher.vnum)[1]
            self.stoneText.SetText('Kamieni zmiany w plecaku: %d' % stones)
        else:
            self.itemSlot.ClearSlot(0)
            self.itemName.SetText('Przeci\xb9gnij tu przedmiot z plecaka')
            self.stoneText.SetText('')
            attrs = [(0, 0)] * NORMAL_ATTRS
        for i in xrange(NORMAL_ATTRS):
            attrType, value = attrs[i]
            self.attrLines[i].SetText('%s +%d' % (BonusName(attrType), value) if attrType else '-')
        self.itemSlot.RefreshSlot()
        if switcher.running:
            self.statusText.SetText('Zmieniam... zmian: %d, kamieni: %d' % (switcher.changes, switcher.stonesLeft))
        else:
            self.statusText.SetText('Zatrzymany. Zmian: %d' % switcher.changes)

    def OnUpdate(self):
        now = clientclock.Now()
        if now < self.nextStatus:
            return
        self.nextStatus = now + STATUS_INTERVAL
        self.RefreshStatus()

    def Close(self):
        self.ReadEdits()
        self.switcher.SaveConfig()
        self.Hide()

    def OnPressEscapeKey(self):
        self.Close()
        return True

    def Destroy(self):
        self.Hide()
        self.switcher = None
        self.widgets = []
        self.rowNames = []
        self.rowEdits = []
        self.attrLines = []


_switcher = None


def GetSwitcher():
    global _switcher
    if _switcher is None:
        _switcher = Switcher()
    return _switcher


def ToggleWindow():
    GetSwitcher().ToggleWindow()
