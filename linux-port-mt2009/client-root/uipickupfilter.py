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
# (PickupFilterSync, an updatable of game.py) and every change. The settings
# are the client's, for every character: autohunt/filtr.cfg. A client with no
# filtr.cfg takes, once, the kinds of the first character whose Auto Lowy
# file left some kinds out (AdoptAutoHuntKinds) - the per-character kinds of
# the clients before this one.
#
# Python 2.7 as the client has it; the texts are CP1250 escapes.

import chat
import net
import os
import player
import ui

import uiautohunt

CONFIG_PATH = os.path.join(uiautohunt.CONFIG_BASE_DIR, 'filtr.cfg')
ALL_KINDS = 0
for _key, _label, _bit in uiautohunt.LOOT_KINDS:
    ALL_KINDS |= _bit

_state = {'loaded': False, 'file': False, 'on': 0, 'kinds': ALL_KINDS, 'window': None}


def Load():
    if _state['loaded']:
        return
    _state['loaded'] = True
    try:
        with open(CONFIG_PATH, 'r') as handle:
            _state['file'] = True
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
    except (IOError, OSError):
        pass


def Save():
    if not os.path.exists(uiautohunt.CONFIG_BASE_DIR):
        try:
            os.makedirs(uiautohunt.CONFIG_BASE_DIR)
        except (IOError, OSError):
            pass
    try:
        with open(CONFIG_PATH, 'w') as handle:
            handle.write('on=%d\nkinds=%d\n' % (_state['on'], _state['kinds']))
        _state['file'] = True
    except (IOError, OSError):
        pass


def Send():
    Load()
    net.SendChatPacket('/pickup_filter %d %d' % (_state['on'], _state['kinds']))


def IsActive():
    Load()
    return _state['on'] != 0


def GetKinds():
    """The kinds the filter keeps, whether it is on or not."""
    Load()
    return _state['kinds']


def HasKind(bit):
    return (GetKinds() & bit) != 0


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
    too - whoever leaves a kind out means it to stay on the ground."""
    Load()
    _state['kinds'] ^= bit
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
    Hunter.LoadConfig), taken once by a client with no filtr.cfg: a file
    that left some kinds out becomes the filter, switched on. A file with
    every kind, or with none (the pick-up in all but name switched off),
    changes nothing. With filtr.cfg the filter's own settings win."""
    Load()
    if _state['file'] or os.path.exists(CONFIG_PATH):
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
        if player.GetMainCharacterName():
            self.sent = True
            Send()

    def Destroy(self):
        DestroyWindow()


class PickupFilterWindow(ui.BoardWithTitleBar):
    WIDTH = 300
    ROWS = (len(uiautohunt.LOOT_KINDS) + 2) // 3

    def __init__(self):
        ui.BoardWithTitleBar.__init__(self)
        self.widgets = []
        self.toggles = {}
        self.height = 32 + 30 + 28 + self.ROWS * 22 + 12 + 54
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
        y += 24 + self.ROWS * 22 + 8 + 6

        self._Label(self, BL + 4, y, 'Z, `, pet, Auto \xa3owy i Towarzysz. Yang zawsze.')
        self._Label(self, BL + 4, y + 14, 'To samo co Podnoszenie w Auto \xa3owach.')
        self._Label(self, BL + 4, y + 28, 'Wy\xb3\xb9czony filtr: podnosz\xea wszystko.')

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
            btn.SetText('%s: %s' % (label, 'Tak' if _state['kinds'] & bit else 'Nie'))

    def OnToggleFilter(self):
        ToggleOn()

    def OnToggleKind(self, bit):
        ToggleKind(bit)

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
