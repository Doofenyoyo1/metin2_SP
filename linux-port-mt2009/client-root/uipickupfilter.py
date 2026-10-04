# Filtr podnoszenia - what the Z key and a loot pet pick up (the operator,
# 28 September: "okno dla autopeta i do recznego dzialania ... mozliwosc
# filtrowania zbieranego dropu, jak w auto lowach").
#
# One filter for everything (the operator, 30 September: "filtr dzialal na
# wszystko: czyli na nas, na autolowy i na towarzysza jednoczesnie. Tak aby
# mozna bylo ustawic tylko filtr pod ctrl + z lub tylko w autolowach"): this
# module owns it - a switch and the thirteen kinds of the auto-hunt's
# pick-up (uiautohunt.LOOT_KINDS, the server's AutoHuntLootKind bits). Two
# windows show and edit the same settings: this one (Ctrl+Z, "/filtr", the
# inventory's button) and the "Podnoszenie" board of Auto Lowy's settings
# window (uiautohunt.AutoHuntLootWindow). Every change is saved at once,
# sent to the server and shown in both.
#
# With the filter on it holds for:
#  - the Z and ` keys and a loot pet - the server picks up
#    (CHARACTER::PickupNearbyItems, server-patches/playerqol);
#  - Auto Lowy - uiautohunt.LootMask asks the server for these kinds only,
#    and the server checks the same filter (FAutoHuntLoot, "(auto hunt)");
#    Auto Lowy's own "Podnies" still says whether the hunt picks up at all;
#  - the companion (Towarzysz) - its own drops and the owner's drops it
#    lifts (playerbot_loot.h / playerbot_sidekick.h, the owner's filter).
# Off, everything is picked up as before the filter. Yang is always taken.
#
# "/pickup_filter <on> <kinds>" is sent after every login and warp
# (PickupFilterSync, an updatable of game.py) and every change.
# MT2009_PLUS_PICKUP_FILTER_PER_CHAR_V1 (the owner, 3 October): the settings
# are each character's own - autohunt/postacie/filtr_<nick>.cfg. A character
# with no file of its own starts from the old client-wide autohunt/filtr.cfg
# (left in place for the other characters), or, with none either, from the
# kinds its Auto Lowy file left out (AdoptAutoHuntKinds).
#
# MT2009_PLUS_PICKUP_BONUS_FILTER_V1 (the owner, 3 October: "podnos tylko z
# bonusem dla kategorii eq"): an equipment kind (BONUS_KINDS) has a third
# choice, "Bonus" - its items are picked up only with at least bonus_min of
# their five bonuses (1 by default, "Min. bonusow" in both windows). A click
# goes Tak -> Bonus -> Nie -> Tak; the other kinds stay Tak / Nie. The
# server decides, it knows the bonuses of an item on the ground: the command
# is "/pickup_filter <on> <kinds> <bonus kinds> <min bonuses>" (playerqol,
# Mt2009PlusPickupFilterAllows) - a server before it reads the first two and
# picks such items up whatever their bonuses. filtr.cfg keeps bonus= and
# bonus_min=, which an older client skips.
#
# Python 2.7 as the client has it; the texts are CP1250 escapes.

import chat
import net
import os
import player
import ui

import uiautohunt

# The client-wide file of the clients before MT2009_PLUS_PICKUP_FILTER_PER_CHAR_V1.
CONFIG_PATH = os.path.join(uiautohunt.CONFIG_BASE_DIR, 'filtr.cfg')


def CharacterName():
    try:
        return player.GetMainCharacterName() or ''
    except Exception:
        return ''


def CharConfigPath(name):
    safe = ''.join(c if c.isalnum() else '_' for c in (name or 'postac'))
    return os.path.join(uiautohunt.CONFIG_CHAR_DIR, 'filtr_%s.cfg' % safe)
ALL_KINDS = 0
for _key, _label, _bit in uiautohunt.LOOT_KINDS:
    ALL_KINDS |= _bit

# MT2009_PLUS_PICKUP_BONUS_FILTER_V1: the kinds that can be "Bonus" - the
# server's MT2009_PLUS_PICKUP_BONUS_KINDS.
BONUS_KEYS = ('loot_weapon', 'loot_armour', 'loot_helmet', 'loot_shield',
    'loot_bracelet', 'loot_shoes', 'loot_necklace', 'loot_earrings')
BONUS_KINDS = 0
for _key, _label, _bit in uiautohunt.LOOT_KINDS:
    if _key in BONUS_KEYS:
        BONUS_KINDS |= _bit
BONUS_MIN_MAX = 5

_state = {'loaded': False, 'file': False, 'on': 0, 'kinds': ALL_KINDS,
    'bonus': 0, 'bonus_min': 1, 'window': None, 'name': None}


def Load():
    # MT2009_PLUS_PICKUP_FILTER_PER_CHAR_V1: read again for another character.
    name = CharacterName()
    if _state['loaded'] and _state['name'] == name:
        return
    _state['loaded'] = True
    _state['name'] = name
    _state['file'] = False
    _state['on'] = 0
    _state['kinds'] = ALL_KINDS
    _state['bonus'] = 0
    _state['bonus_min'] = 1
    path = CharConfigPath(name) if name else CONFIG_PATH
    if name and not os.path.exists(path):
        path = CONFIG_PATH	# the old client-wide settings as a start
    try:
        with open(path, 'r') as handle:
            _state['file'] = path != CONFIG_PATH or not name
            for line in handle:
                if '=' not in line:
                    continue
                key, value = line.strip().split('=', 1)
                try:
                    value = int(value)
                except ValueError:
                    continue
                if key == 'on':
                    _state['on'] = 1 if value else 0
                elif key == 'kinds':
                    _state['kinds'] = value & ALL_KINDS
                elif key == 'bonus':
                    _state['bonus'] = value & BONUS_KINDS
                elif key == 'bonus_min':
                    _state['bonus_min'] = max(1, min(BONUS_MIN_MAX, value))
    except (IOError, OSError):
        pass


def Save():
    Load()
    name = _state['name']
    folder = uiautohunt.CONFIG_CHAR_DIR if name else uiautohunt.CONFIG_BASE_DIR
    if not os.path.exists(folder):
        try:
            os.makedirs(folder)
        except (IOError, OSError):
            pass
    try:
        with open(CharConfigPath(name) if name else CONFIG_PATH, 'w') as handle:
            handle.write('on=%d\nkinds=%d\nbonus=%d\nbonus_min=%d\n' % (
                _state['on'], _state['kinds'], _state['bonus'], _state['bonus_min']))
        _state['file'] = True
    except (IOError, OSError):
        pass


def Send():
    Load()
    net.SendChatPacket('/pickup_filter %d %d %d %d' % (
        _state['on'], _state['kinds'], BonusKinds(), _state['bonus_min']))


def IsActive():
    Load()
    return _state['on'] != 0


def GetKinds():
    """The kinds the filter keeps, whether it is on or not."""
    Load()
    return _state['kinds']


def HasKind(bit):
    return (GetKinds() & bit) != 0


def BonusKinds():
    """MT2009_PLUS_PICKUP_BONUS_FILTER_V1: the kinds kept only with bonuses."""
    Load()
    return _state['bonus'] & _state['kinds'] & BONUS_KINDS


def KindText(bit, yes='Tak', no='Nie', bonus='Bonus'):
    """A kind's choice as its button says it: yes, no or (equipment) bonus."""
    Load()
    if not _state['kinds'] & bit:
        return no
    if BonusKinds() & bit:
        return bonus
    return yes


def BonusMinText():
    Load()
    return 'Min. bonus\xf3w: %d' % _state['bonus_min']


def CycleBonusMin():
    """How many bonuses a "Bonus" kind's item needs: 1 to 5 and round."""
    Load()
    _state['bonus_min'] = _state['bonus_min'] % BONUS_MIN_MAX + 1
    Changed()


def EffectiveKinds():
    """What may be picked up: the kinds with the filter on, all of them off."""
    Load()
    return _state['kinds'] if _state['on'] else ALL_KINDS


def OnStateText():
    return 'W\xb3\xb9czony' if IsActive() else 'Wy\xb3\xb9czony'


def ToggleOn():
    Load()
    _state['on'] = 0 if _state['on'] else 1
    Changed()


def ToggleKind(bit):
    """A kind switched in either window. With the filter off it is turned on
    too - whoever leaves a kind out means it to stay on the ground.
    MT2009_PLUS_PICKUP_BONUS_FILTER_V1: an equipment kind goes Tak -> Bonus
    -> Nie -> Tak."""
    Load()
    if bit & BONUS_KINDS and _state['kinds'] & bit and not _state['bonus'] & bit:
        _state['bonus'] |= bit
    else:
        _state['kinds'] ^= bit
        _state['bonus'] &= ~bit
    if not _state['on']:
        _state['on'] = 1
        chat.AppendChat(chat.CHAT_TYPE_INFO, 'Filtr podnoszenia w\xb3\xb9czony (Z, Auto \xa3owy i Towarzysz).')
    Changed()


def Changed():
    Save()
    Send()
    RefreshWindows()


def AdoptAutoHuntKinds(mask):
    """The per-character kinds of Auto Lowy's older files (uiautohunt,
    Hunter.LoadConfig), taken once by a character with no filter file: a file
    that left some kinds out becomes the filter, switched on. A file with
    every kind, or with none (the pick-up in all but name switched off),
    changes nothing. With filtr.cfg the filter's own settings win."""
    Load()
    name = _state['name']
    if _state['file'] or os.path.exists(CONFIG_PATH) or (name and os.path.exists(CharConfigPath(name))):
        return
    mask &= ALL_KINDS
    if mask == 0 or mask == ALL_KINDS:
        return
    _state['on'] = 1
    _state['kinds'] = mask
    chat.AppendChat(chat.CHAT_TYPE_INFO, 'Filtr podnoszenia: rodzaje z Auto \xa3ow\xf3w tej postaci dzia\xb3aj\xb9 teraz dla Z, Auto \xa3ow\xf3w i Towarzysza (Ctrl+Z).')
    Changed()


def RefreshWindows():
    window = _state['window']
    if window is not None:
        try:
            if window.IsShow():
                window.Refresh()
        except RuntimeError:
            pass
    hunter = uiautohunt._hunter
    if hunter is not None and hunter.lootWindow is not None:
        try:
            if hunter.lootWindow.IsShow():
                hunter.lootWindow.Refresh()
        except RuntimeError:
            pass


def OnAck(on='0', kinds='0', *rest):
    pass


class PickupFilterSync(object):
    """An updatable: the filter to the server once a game phase has a named
    character - after the login and after every warp, when the core may be
    another one."""

    def __init__(self):
        self.sent = False

    def CanUpdate(self):
        return not self.sent

    def OnUpdate(self):
        # MT2009_PLUS_SIDEKICK_WARP_SAFE_V1: only in the game phase (warpsafe.py).
        import warpsafe
        if player.GetMainCharacterName() and warpsafe.InGame():
            self.sent = True
            Send()

    def Destroy(self):
        DestroyWindow()


class PickupFilterWindow(ui.BoardWithTitleBar):
    WIDTH = 300
    # The kinds and "Min. bonusow" (MT2009_PLUS_PICKUP_BONUS_FILTER_V1).
    ROWS = (len(uiautohunt.LOOT_KINDS) + 1 + 2) // 3

    def __init__(self):
        ui.BoardWithTitleBar.__init__(self)
        self.widgets = []
        self.toggles = {}
        self.bonusMinButton = None
        self.height = 32 + 30 + 28 + self.ROWS * 22 + 12 + 68
        self.AddFlag('movable')
        self.AddFlag('float')
        self.SetSize(self.WIDTH, self.height)
        self.SetTitleName('Filtr podnoszenia')
        self.SetCloseEvent(ui.__mem_func__(self.Close))
        self.Build()
        self.SetCenterPosition()

    def Build(self):
        BL = 10
        BW = self.WIDTH - 2 * BL
        y = 32
        self.onButton = self._Btn(self, 'xlarge', 0, y, '', self.OnToggleFilter)
        self.onButton.SetWindowHorizontalAlignCenter()
        y += 30

        board = ui.ThinBoard()
        board.SetParent(self)
        board.SetPosition(BL, y)
        board.SetSize(BW, 24 + self.ROWS * 22 + 8)
        board.Show()
        self.widgets.append(board)
        self._Label(board, 14, 4, 'Podnosz\xea:')
        for idx, (key, label, bit) in enumerate(uiautohunt.LOOT_KINDS):
            btn = self._Btn(board, 'large', 4 + (idx % 3) * 92, 24 + (idx // 3) * 22, '', self.OnToggleKind, bit)
            self.toggles[bit] = (btn, label)
        pos = len(uiautohunt.LOOT_KINDS)
        self.bonusMinButton = self._Btn(board, 'large', 4 + (pos % 3) * 92, 24 + (pos // 3) * 22, '', self.OnCycleBonusMin)
        y += 24 + self.ROWS * 22 + 8 + 6

        self._Label(self, BL + 4, y, 'Z, `, pet, Auto \xa3owy i Towarzysz. Yang zawsze.')
        self._Label(self, BL + 4, y + 14, 'To samo co Podnoszenie w Auto \xa3owach.')
        self._Label(self, BL + 4, y + 28, 'Wy\xb3\xb9czony filtr: podnosz\xea wszystko.')
        self._Label(self, BL + 4, y + 42, 'Bonus: eq tylko z bonusami (min. bonus\xf3w).')

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

    def Refresh(self):
        Load()
        self.onButton.SetText('Filtr: %s' % OnStateText())
        for bit, (btn, label) in self.toggles.items():
            btn.SetText('%s: %s' % (label, KindText(bit)))
        self.bonusMinButton.SetText(BonusMinText())

    def OnToggleFilter(self):
        ToggleOn()

    def OnToggleKind(self, bit):
        ToggleKind(bit)

    def OnCycleBonusMin(self):
        CycleBonusMin()

    def Open(self):
        self.Refresh()
        self.Show()
        self.SetTop()

    def Close(self):
        self.Hide()

    def OnPressEscapeKey(self):
        self.Close()
        return True

    def Destroy(self):
        self.Hide()
        self.widgets = []
        self.toggles = {}
        self.bonusMinButton = None


def ToggleWindow():
    window = _state['window']
    if window is None:
        window = PickupFilterWindow()
        _state['window'] = window
    if window.IsShow():
        window.Close()
    else:
        window.Open()


def OpenWindow():
    window = _state['window']
    if window is None or not window.IsShow():
        ToggleWindow()


def DestroyWindow():
    window = _state['window']
    _state['window'] = None
    if window is not None:
        window.Destroy()
