# Auto Hunt System
# Created by SIZOWSKI (Thank you for the original code! Go subscribe to him on YouTube! https://www.youtube.com/@metin2singleplayer a.k.a "ZAXEP - METIN2 SINGLE PLAYER")
# Modernized by Colide (Uriel).
#
# MT2009_PLUS_UPSTREAM_2_0_76: rebased on the upstream client 2.0.76 (from
# 2.0.49 + the crowd escape): mounted hunting and "Bojowiec", the wall
# escape and the minute's skip list, the server's way round walls
# (/autohunt_path), buff/potion affects, focus loss. The pick-up stays
# MT2009 PLUS's own filter (uipickupfilter.py, below); upstream's "Bez
# bonusu" and "Filtr dla Z i `" are that filter's job here.
#
# Auto Lowy 2.0 (Colide, 22 September). The official system's features for
# free (pl-wiki, "System - Auto Lowy"): twelve skills on their own clocks
# (one cast at an enemy waits for the fight - see SELF_SKILLS below),
# six potions each under its own share of health or mana, twelve items on a
# clock, stones, standing up after death and walking back, and a pick-up
# by kind in a window of its own. New in 2.0: three switches for what is
# fought - Moby, Metiny, Bossy - which the server ranks boss, stone,
# monster (do_autohunt_target, playerbotify apply_auto_hunt_categories);
# a corpse is let go the moment this client sees it dead
# (player.IsTargetDead), because the server keeps it two or three seconds
# and named it again; a bow in the hand reaches from afar
# (player.IsBowEquipped); the range is drawn on the ground
# (player.SetAutoHuntRangeCircle) - three functions of the client 2.0.25
# exe, each asked with hasattr or under AttributeError, so an older exe
# hunts without them; the windows keep their places in autohunt/config.cfg
# and every character its own settings in autohunt/postacie/<name>.cfg.
#
# The client cannot list the monsters or the items round its character, so
# it asks the server:
#   "/autohunt_target <range> <stones> <dx> <dy> <mobs> <bosses> [<skip> [<nearest> [<skips> [<order>]]]]"
#     -> "AutoHuntTarget <vid> [<blocked>]"
# (<nearest> 1: the nearest monster plain, for a hunter boxed in - see
# COMBAT_STUCK_SECONDS; MT2009_PLUS_AUTOHUNT_CROWD_V1 in do_autohunt_target, and a server
# without it reads seven arguments and ignores the eighth. <skips>: the
# targets left for a minute, "123,456", at most TARGET_BLOCK_CAPACITY -
# apply_auto_hunt_skip_list; a server before it ignores the ninth.
# <order>: MT2009_PLUS_AUTOHUNT_PRIORITY_V1, the "Priorytet" of the settings
# window, 0 the nearest plain, 1-6 the orders of PRIORITY_LABELS; the ninth
# is then always sent, "0" for no skips. <blocked>: 1 when a wall or a rock
# stands on the straight line to the target - MT2009_PLUS_AUTOHUNT_BLOCKED_V1,
# the way round is then asked at once by /autohunt_path below; a server
# before it sends the VID alone.)
#   "/autohunt_mount <off|on>" -> "AutoHuntMount <off|on> <mounted>"
# (MT2009_PLUS_AUTOHUNT_MOUNT_V1: off and back on a mount that holds the
# hunter in place; both idempotent - a retry never undoes a late success.)
#   "/autohunt_loot <range> <kinds> <dx> <dy>" -> "AutoHuntLoot <vid> <dx> <dy>"
#   "/autohunt_path <seq> <vid> <dx> <dy>"
#     -> "AutoHuntPath <seq> <ok|direct|none|wait|off> <kind> [<dx>,<dy>;...]"
# (the way round a wall from the bots' route planner, and "AutoHuntPath 0
# hello <kind>" after the first target question a VID - see PATH_KIND_NONE;
# playerbotify apply_auto_hunt_paths, playerbot_autohunt.h.)
# Every place goes both ways as an offset from the character: this client
# counts positions from its map's corner and the server from the world's.
# Every time is clientclock.Now(): app.GetTime() starts again from zero at
# every warp, and a skill's or an item's next moment taken on the map
# before would have held it for as long as the character played there.
#
# Autologin (autologin.py): a switch in the "Ustawienia Walki" grid, saved
# per character. With it on, a game that drops is logged in again with the
# same account and character, and a hunt that was running goes on. The
# Hunter's CanUpdate is the one hook every frame of the game passes, so the
# autologin learns there that the game is open again, and Destroy - the game
# window closing - is where it hears that the game has gone.
#
# The pick-up's kinds are the pick-up filter's (uipickupfilter.py, one
# filter for the Z key, Auto Lowy and the companion, the operator, 30
# September): the "Podnoszenie" board shows and switches the same kinds as
# the Ctrl+Z window, and its "Filtr" the same switch. "Podnies" stays Auto
# Lowy's own - whether the hunt picks up at all. With the filter off the hunt
# picks up every kind; the per-character loot_* keys of the older files are
# read once, for a client with no filtr.cfg (Hunter.LoadConfig).
#
# MT2009_PLUS_AUTOHUNT_PICKUP_TOGGLE_V1: that switch is a row of its own at
# the top of the "Podnoszenie" board, "Autopodnoszenie: Wlaczone /
# Wylaczone" (the owner, 2 October). Off, the hunt asks no /autohunt_loot
# and picks nothing up (LootMask); the choice goes into the character's
# file at once (Hunter.SavePickupChoice), without the rest of the unsaved
# window.
#
# "Bojowiec" (Setnil, 1 October) is the hunt of a rider on a battle horse, a
# switch in the "Ustawienia Walki" grid: it stands where it is and swings as a
# held space bar does, picks up round itself by the window's kinds, casts the
# horse's own skills from the saddle and climbs down for a moment for the
# class's buffs, which no saddle lets anybody cast - the section "The rider"
# below says how and why. With the switch off, mounted hunting shares only
# the dismount/cast/remount cycle and keeps its normal movement and range.
#
# game.py registers the Hunter with its updateables, K opens the windows.
# Python 2.7 as the client has it, and 3 for tests/uiautohunt_test.py.
# Player-visible strings are CP1250 escapes, so the file itself is ASCII, and
# each is a Polish and English pair (playerbot_lang.T): English for a client
# set to any language but Polish. The labels of the module's tables are read
# once, at import - the client's language changes only with a restart.

import app
import autologin
import chat
import chr
import clientclock
import item
import math
import mouseModule
import net
import os
import player
import skill
import sys
import ui
import wndMgr
from playerbot_lang import T

try:
    xrange
except NameError:
    xrange = range

SKILL_SLOTS = 12
USE_ITEM_SLOTS = 18
ITEM_SLOT_KEYS = tuple('item%d_vnum' % i for i in xrange(USE_ITEM_SLOTS))
ITEM_EDIT_KEYS = tuple('item%d_val' % i for i in xrange(USE_ITEM_SLOTS))

RANGES = (1000, 2000, 3000, 4000)
# What Auto Lowy picks up, a switch each. The bits are the server's
# (AutoHuntLootKind, playerbotify apply_auto_hunt_loot_kinds): 0-6 are the
# seven kinds of client 2.0.40, 7-12 what client 2.0.41 split off them - the
# helmet and the shield from the armour, the bracelet, the shoes, the
# necklace and the earrings from the jewellery, which keeps what is left
# (a pendant, gloves, rings, belts).
LOOT_KINDS = (
    ('loot_weapon',    T('Bro\xf1', 'Weapons'),         1 << 0),
    ('loot_armour',    T('Zbroje', 'Armour'),           1 << 1),
    ('loot_helmet',    T('He\xb3my', 'Helmets'),         1 << 7),
    ('loot_shield',    T('Tarcze', 'Shields'),          1 << 8),
    ('loot_bracelet',  T('Bransolety', 'Bracelets'),    1 << 9),
    ('loot_shoes',     T('Buty', 'Shoes'),              1 << 10),
    ('loot_necklace',  T('Naszyjniki', 'Necklaces'),    1 << 11),
    ('loot_earrings',  T('Kolczyki', 'Earrings'),       1 << 12),
    ('loot_jewellery', T('Ozdoby', 'Trinkets'),         1 << 2),
    ('loot_potion',    T('Mikstury', 'Potions'),        1 << 3),
    ('loot_book',      T('Ksi\xeagi', 'Books'),         1 << 4),
    ('loot_stone',     T('Kamienie', 'Stones'),         1 << 5),
    ('loot_other',     T('Inne', 'Other'),              1 << 6),
)
# Each split kind and the kind it came out of. A server before the split
# reads only the seven, so the second field of /autohunt_loot keeps them:
# "armour" while any of body, helmet or shield is taken, "jewellery" while
# any trinket is. A settings file from before the split gives each new
# switch the old one's value, so "Ozdoby: nie" stays a bag without shoes.
LOOT_SPLIT_FROM = (
    ('loot_helmet', 'loot_armour'), ('loot_shield', 'loot_armour'),
    ('loot_bracelet', 'loot_jewellery'), ('loot_shoes', 'loot_jewellery'),
    ('loot_necklace', 'loot_jewellery'), ('loot_earrings', 'loot_jewellery'),
)
# Asked with the filter on and no kind kept: no kind of AutoHuntLootKind has
# this bit, so the server (which answers nothing for no kinds at all) finds
# yang only - "Yang zawsze", as for the Z key.
LOOT_YANG_ONLY = 1 << 20

TARGET_REQUEST_INTERVAL = 0.8
LOOT_REQUEST_INTERVAL = 1.0
MOVE_INTERVAL = 0.35
RETURN_MOVE_INTERVAL = 1.0
FACE_INTERVAL = 0.5
POTION_INTERVAL = 1.0
COURAGE_CAPE_VNUMS = (39006, 70038, 70057, 70138, 76007)
LOOT_PICK_DISTANCE = 450
LOOT_FIRST_DISTANCE = 900
LOOT_PICK_INTERVAL = 0.6
LOOT_STUCK_SECONDS = 6.0
LOOT_STUCK_PAUSE = 10.0
# The queue's own patience: AskForLoot refreshes self.lootVid roughly once
# a second, so a sweep is declared over only after one full cycle of that
# clock has had the chance to say nothing more is owed.
LOOT_MAX_PICK_RETRIES = 3
LOOT_SKIP_DURATION = 30.0
LOOT_SWEEP_END_GRACE = 0.5
REVIVE_RETRY = 5.0
REVIVE_MIN_SECONDS = 10
SKILL_MIN_INTERVAL = 1.5
# A skill's blows are the client's own motion events: a walk begun while the
# motion runs ends it, and the skill hits nothing ("dmg ze skilli podczas
# grania na autolowach nie wchodzi", prodnathin, 26 September; Colide traced
# it to the walk to a drop that came right after the cast). After a cast the
# hunter takes no step - to a drop, to its target, back to its start - for
# this long; a drop at its feet is still picked up, which moves nothing.
SKILL_MOTION_HOLD = 1.3
ITEM_MIN_INTERVAL = 1
STATUS_INTERVAL = 0.3
MELEE_REACH = 200
ARCHER_REACH = 2400
STOP_SHORT_SHARE = 0.6
ANCHOR_LEASH = 600
STUCK_SECONDS = 8.0
# CHASE_SWITCH_MARGIN (Buby, 23 September) is gone: the target modes
# "Najblizszy" and "Fokus" (MT2009_PLUS_AUTOHUNT_PRIORITY_V1) say when the
# server's pick is taken.
STUCK_PAUSE = 2.0
# A walk that still gains ground is not stuck: the target's and the drop's
# clocks start again at every WALK_PROGRESS units closer. Counted from the
# first step, STUCK_SECONDS were 3600 units at a Ninja's pace and the drop's
# six seconds 2700, less than a range of 5000 asks for, so an archer gave up
# on the monsters at the edge and on every far drop (teivos, 27 September).
WALK_PROGRESS = 200
STUCK_SKIP_SECONDS = 60.0
# Boxed in on the way (Colide, 2 October): a blow knocked the target back, a
# pack closed round the character, and it ran at the first monster against
# the pack's bodies, hitting nothing, until STUCK_SECONDS let it go. While it
# walks to a target beyond its reach, a character that has neither moved
# COMBAT_MOVE_THRESHOLD units nor come that much nearer to the target in
# COMBAT_STUCK_SECONDS - a skill's motion apart - stops where it stands,
# leaves that target for COMBAT_SKIP_SECONDS and asks the server for the
# nearest monster plain (the eighth argument), which is one of the pack
# round it. Both measures, because either alone lies: the character's own
# place stands still while a monster comes to it, and the distance stands
# still while it follows one that runs or walks round a wall.
COMBAT_STUCK_SECONDS = 0.8
COMBAT_MOVE_THRESHOLD = 20.0
COMBAT_SKIP_SECONDS = 2.0
# Colide's second version (2 October), for the walls rather than the packs.
# A target that boxes the hunter in TARGET_BLOCK_LIMIT times, each within
# TARGET_BLOCK_WINDOW of the one before, is left for TARGET_BLOCK_DURATION;
# so is one given up after STUCK_SECONDS or an escape that failed. Up to
# TARGET_BLOCK_CAPACITY of them are refused here and named to the server
# together (the ninth argument), so it picks another monster rather than
# naming one of them again for as long as the minute runs.
TARGET_BLOCK_LIMIT = 3
TARGET_BLOCK_WINDOW = 30.0
TARGET_BLOCK_DURATION = 60.0
TARGET_BLOCK_CAPACITY = 32
# A box seen again within ESCAPE_REPEAT_SECONDS and ESCAPE_REPEAT_DISTANCE of
# the last one is a wall, not a pack: the hunter steps off it - back the way
# it came, then sideways, ESCAPE_STEP_DISTANCE a step, each step given
# ESCAPE_STEP_SECONDS to gain ground - for at most ESCAPE_MAX_SECONDS, and
# stands clear once ESCAPE_CLEAR_DISTANCE from where it was held. An escape
# that fails leaves its target for the minute, and the next escape waits
# ESCAPE_RETRY_SECONDS more after every failure, up to thirty. Colide's
# version stood the whole hunt still for that wait; here only the next
# escape waits, and the hunter fights on meanwhile - a hunter standing for
# thirty seconds among the monsters that boxed it in is a dead one.
ESCAPE_REPEAT_SECONDS = 6.0
ESCAPE_REPEAT_DISTANCE = 120.0
ESCAPE_STEP_DISTANCE = 225.0
ESCAPE_STEP_SECONDS = 0.9
ESCAPE_MAX_SECONDS = 5.5
ESCAPE_CLEAR_DISTANCE = 100.0
ESCAPE_RETRY_SECONDS = 5.0
ESCAPE_RETRY_MAX_SECONDS = 30.0








RETURN_TRAIL_LIMIT = 2048
RETURN_TRAIL_REACH = 60.0
RETURN_TRAIL_STEP = 50.0
RETURN_TRAIL_LOOP = 35.0
RETURN_TRAIL_JUMP = 600.0
# The way round a wall, from the server (Colide asked for it, 2 October:
# "pathing w zamknietych mapach z korytarzami"). The server keeps the bots'
# grid of the ground and says once a VID what a map's ground is: nothing
# (PATH_KIND_NONE - no grid there, or a server from before it, which never
# says), a grid (PATH_KIND_GRID: the way is asked when the straight walk
# fails - a box, or PATH_STALL_SECONDS of walking that gains no ground), or a
# map of rooms and corridors (PATH_KIND_CORRIDORS - the Monkey and Spider
# Dungeons, the grottoes, the Catacomb, the tower: asked before the walk to
# every target beyond reach, and for every walk back). The answer is the way
# as points, "direct" (no wall on the straight line - what holds the
# hunter is bodies, and the box goes on as before), "none" (the goal stands
# on ground this one does not join - the next room - and the target is left
# for the minute), or "wait"/"off" (the hunt goes on without). A target's
# way is asked once in PATH_RETRY_SECONDS, the next question no sooner than
# PATH_REQUEST_INTERVAL, an answer waited for PATH_ANSWER_SECONDS. A point
# within PATH_POINT_REACH is reached; a way held still for
# COMBAT_STUCK_SECONDS is given up; a target PATH_DRIFT from the way's end
# asks it again.
PATH_KIND_NONE = 0
PATH_KIND_GRID = 1
PATH_KIND_CORRIDORS = 2
PATH_REQUEST_INTERVAL = 0.5
PATH_ANSWER_SECONDS = 1.5
PATH_RETRY_SECONDS = 10.0
PATH_RETURN_RETRY_SECONDS = 10.0
PATH_STALL_SECONDS = 3.0
PATH_POINT_REACH = 70.0
PATH_DRIFT = 500.0
PATH_MAX_POINTS = 64
ESCAPE_TARGET_INTERVAL = 0.8
RETURN_MOUNTED_TRAIL_REACH = 150.0
RETURN_MOUNTED_STUCK_SECONDS = 2.0

# MT2009_PLUS_AUTOHUNT_PRIORITY_V1 (Autor: blaki): the target's mode and the
# order of what is attacked first, both saved for the character (the settings
# window, "Dodatkowe ustawienia"). "Najblizszy" asks the server every
# NEAREST_REQUEST_INTERVAL and takes a different VID it names at once;
# "Fokus" keeps its live target (the old CHASE_SWITCH_MARGIN is gone) and
# lets it go - for STUCK_SKIP_SECONDS - when its health has not fallen for
# FOCUS_IDLE_SECONDS while in reach, asking for the nearest one plain for
# FOCUS_FALLBACK_SECONDS. The order: the server takes the nearest within the
# first category that has a target (do_autohunt_target, the tenth argument).
TARGET_MODE_NEAREST = 0
TARGET_MODE_FOCUS = 1
NEAREST_REQUEST_INTERVAL = 0.25
FOCUS_IDLE_SECONDS = 3.0
FOCUS_FALLBACK_SECONDS = 1.0
PRIORITY_LABELS = (
    T('Bez priorytetu (najbli\xbfszy)', 'No priority (nearest)'),
    T('Bossy > Metiny > Moby', 'Bosses > Metins > Monsters'),
    T('Bossy > Moby > Metiny', 'Bosses > Monsters > Metins'),
    T('Metiny > Bossy > Moby', 'Metins > Bosses > Monsters'),
    T('Metiny > Moby > Bossy', 'Metins > Monsters > Bosses'),
    T('Moby > Bossy > Metiny', 'Monsters > Bosses > Metins'),
    T('Moby > Metiny > Bossy', 'Monsters > Metins > Bosses'),
)
# The server reads a command line of 256 characters (interpret_command): the
# skip list is cut so the order after it always arrives.
TARGET_COMMAND_MAX = 240

# MT2009_PLUS_AUTOHUNT_MOUNT_V1 (Autor: blaki): a mounted hunter that walks
# (to a target, a drop or back to its start) and gains no
# COMBAT_MOVE_THRESHOLD units in MOUNT_STUCK_SECONDS stops, gets off
# ("/autohunt_mount off"), waits MOUNT_RECOVERY_DELAY at least for the
# server's word and gets on again ("/autohunt_mount on", every
# MOUNT_RECOVERY_RETRY until the server and the client both say mounted).
# No remount in MOUNT_RECOVERY_REMOUNT_TIMEOUT stops the hunt with a word in
# the chat; one attempt in MOUNT_RECOVERY_COOLDOWN. A walk counts while a
# step was ordered within MOUNT_WALK_INTENT_SECONDS; a fight in place never.
MOUNT_STUCK_SECONDS = 3.0
MOUNT_WALK_INTENT_SECONDS = 1.5
MOUNT_RECOVERY_DELAY = 1.2
MOUNT_RECOVERY_RETRY = 1.5
MOUNT_RECOVERY_DISMOUNT_TIMEOUT = 8.0
MOUNT_RECOVERY_REMOUNT_TIMEOUT = 15.0
MOUNT_RECOVERY_COOLDOWN = 12.0

# A skill cast at an enemy goes only at the monster the hunter is fighting:
# alive, in the client's own hand (player.GetTargetVID) and within reach.
# Anything less the client settles by itself, and badly for a character
# nobody steers. With no live target __UseSkill takes whatever stands under
# the mouse cursor (PythonPlayerSkill.cpp, __ChangeTargetToPickedInstance);
# out of the skill's range it walks there in a straight line, fighting the
# hunter's own walk (__ReserveUseSkill, MODE_USE_SKILL); and Fast Attack then
# puts the character 270 units before that monster's middle, along the
# straight line in three dimensions, with one IsBlock test at the landing and
# no look at what lies between (ActorInstanceMotionEvent.cpp,
# ProcessMotionEventWarp) - over a cliff, through a wall, into a hillside.
# Clicked on its clock from anywhere, that is the likeliest way Buby's Ninja
# came to hang in a night sky with no world round it (23 September).
# What needs no enemy still goes on its own clock, fight or no fight: a
# standing skill and a toggle, which the client casts where the character
# stands, and SELF_SKILLS - the Ninja's Stealth and the Shaman's buffs,
# which the client turns on the caster when the target is a monster.
SELF_SKILLS = (34, 94, 95, 96, 109, 110, 111)
# What goes while the hunter waits for its health after standing up
# ('HP po wskrz. %'): the buffs and nothing that fights. A standing skill
# is an attack cast where the character stands, and the Shaman's Dragon's
# Roar (93) woke the monsters that had just killed her, the moment it came
# off its cooldown - she fell again, stood up and roared again, for good
# (prodnathin, 25 September). SELF_SKILLS and the standing buffs: the
# Warrior's Berserk, Aura and Strong Body, the Archer's Feather Walk, the
# weapon Sura's Enchanted Blade, Fear and Enchanted Armour, the black-magic
# Sura's Dark Protection. Not Flame Spirit (78), which strikes by itself.
BUFF_SKILLS = SELF_SKILLS + (3, 4, 19, 49, 63, 64, 65, 79)
# What skill.IsStandingSkill says of this client's own skills, for an exe or
# a stub that cannot be asked.
STANDING_SKILLS = (3, 4, 18, 19, 47, 49, 62, 63, 64, 65, 77, 78, 79, 93)
# Only a class's skills and the horse's are ever cast at an enemy; a guild
# or a support skill is nobody's.
TARGET_SKILL_RANGES = ((1, 111), (137, 140))
# Fast Attack moves the character by its distance to the target less 270.
# From melee reach that is a short step back onto the ground it has just
# walked over; from the skill's own range of 800 it was a jump of 530 over
# whatever stood between. So it waits for melee reach even when the hunter
# reaches further - an exe older than 2.0.25 takes an archery-school Ninja
# for a bow in the hand whatever it holds.
WARP_SKILLS = (32,)

CONFIG_BASE_DIR = 'autohunt'
CONFIG_CHAR_DIR = os.path.join(CONFIG_BASE_DIR, 'postacie')

DEFAULTS = [
    ('range', 2000), ('stones', 1), ('mobs', 1), ('bosses', 0), ('pickup', 1), 
    ('revive', 1), ('revive_after', 15), ('return', 1),
    ('attack', 1), ('use_potions', 1), ('use_buffs', 1), ('use_skills', 1),
    ('revive_hp_percent', 60), ('autologin', 0),
    # The bojowiec (the rider section below); a file without the key, every
    # file before it, reads it off.
    ('rider', 0),
    # MT2009_PLUS_AUTOHUNT_PRIORITY_V1 (Autor: blaki): a file without them
    # reads "Fokus" and no priority.
    ('target_mode', TARGET_MODE_FOCUS), ('priority_order', 0),
]
for i in xrange(USE_ITEM_SLOTS):
    DEFAULTS.append(('item%d_vnum' % i, 0))
    DEFAULTS.append(('item%d_val' % i, 60 if i < 6 else 30))
for _index in xrange(SKILL_SLOTS):
    DEFAULTS.append(('skill%d_slot' % _index, 0))
    DEFAULTS.append(('skill%d_interval' % _index, 0))
for _key, _label, _bit in LOOT_KINDS:
    DEFAULTS.append((_key, 1))

CONFIG_VERSION = 6
DEFAULTS.append(('config_version', CONFIG_VERSION))

GLOBAL_DEFAULTS = [
    ('win_main_x', -1), ('win_main_y', -1),
    ('win_loot_x', -1), ('win_loot_y', -1),
]

def DefaultConfig():
    return dict(DEFAULTS)

def ApplyConfigText(config, text):
    for line in text.splitlines():
        key, _, value = line.partition('=')
        key = key.strip()
        if key not in config:
            continue
        try:
            config[key] = max(0, int(value.strip()))
        except ValueError:
            pass
    return config

def ParseConfigText(text):
    """The "key=value" lines of a saved file as numbers, a bad line skipped."""
    values = {}
    for line in text.splitlines():
        key, _, value = line.partition('=')
        key = key.strip()
        if not key:
            continue
        try:
            values[key] = max(0, int(value.strip()))
        except ValueError:
            pass
    return values


def ConfigFromOldValues(values):
    """A file from the window before Colide's (no version, or 2) in this
    one's slots: the six skills where they were, the health and mana potions
    as the first two potion slots with their shares, the three items as the
    first three on a clock, and the pick-up back on (the toggle window saved
    it switched off by mistake)."""
    config = DefaultConfig()
    for key in ('range', 'stones', 'pickup', 'revive', 'revive_after', 'return'):
        if key in values:
            config[key] = values[key]
    for index in xrange(6):
        for key in ('skill%d_slot' % index, 'skill%d_interval' % index):
            if key in values:
                config[key] = values[key]
    for slot, (vnumKey, shareKey) in enumerate((('hp_vnum', 'hp_percent'), ('sp_vnum', 'sp_percent'))):
        config['item%d_vnum' % slot] = values.get(vnumKey, 0)
        if shareKey in values:
            config['item%d_val' % slot] = values[shareKey]
    for index in xrange(3):
        target = 6 + index
        config['item%d_vnum' % target] = values.get('item%d_vnum' % index, 0)
        if 'item%d_interval' % index in values:
            config['item%d_val' % target] = values['item%d_interval' % index]
    for key, label, bit in LOOT_KINDS:
        if key in values:
            config[key] = values[key]
    if 'config_version' not in values:
        config['pickup'] = 1
        for key, label, bit in LOOT_KINDS:
            config[key] = 1
    return config


def ConfigFromText(text):
    """Version 4 (2.0.19-2.0.24) and 5 (2.0 with Moby and Bossy) read as they
    are, the new keys taking their defaults; a file of the window before
    Colide's moves into these slots; version 3, his own first builds, starts
    from the defaults."""
    values = ParseConfigText(text)
    version = values.get('config_version', 0)
    if version >= 4:
        config = ApplyConfigText(DefaultConfig(), text)
    elif version <= 2:
        config = ConfigFromOldValues(values)
    else:
        config = DefaultConfig()
    if version < 6:
        for key, parent in LOOT_SPLIT_FROM:
            if key not in values:
                config[key] = config[parent]
    config['config_version'] = CONFIG_VERSION
    return config

def ConfigText(config):
    return ''.join('%s=%d\n' % (key, config[key]) for key, _ in DEFAULTS)

def GlobalConfigPath():
    return os.path.join(CONFIG_BASE_DIR, 'config.cfg')

def ConfigPath(name):
    safe = ''.join(c if c.isalnum() else '_' for c in (name or 'postac'))
    return os.path.join(CONFIG_CHAR_DIR, '%s.cfg' % safe)

def OldConfigPath(name):
    """Where 2.0.17-2.0.24 kept a character's settings."""
    safe = ''.join(c if c.isalnum() else '_' for c in (name or 'postac'))
    return os.path.join(CONFIG_BASE_DIR, '%s.cfg' % safe)


def OldestConfigPath(name):
    """Where the window before Colide's kept them, beside the client."""
    safe = ''.join(c if c.isalnum() else '_' for c in (name or 'postac'))
    return 'autohunt_%s.cfg' % safe

def ParseTargetVid(value):
    try:
        vid = int(value)
    except (TypeError, ValueError):
        return 0
    return vid if 0 < vid <= 0xffffffff else 0

def ParsePath(text):
    """The server's way, "dx,dy;dx,dy" (playerbot_autohunt_rules.h's
    EncodePath): a list of offsets, or None for anything else."""
    points = []
    try:
        for piece in str(text).split(';'):
            (x, y) = piece.split(',')
            points.append((int(x), int(y)))
    except (TypeError, ValueError):
        return None
    if not points or len(points) > PATH_MAX_POINTS:
        return None
    return points

def ParseLoot(vid, x, y):
    vid = ParseTargetVid(vid)
    if not vid:
        return (0, 0, 0)
    try:
        return (vid, int(x), int(y))
    except (TypeError, ValueError):
        return (0, 0, 0)

def PickupFilter():
    """uipickupfilter, imported when first asked: it imports this module."""
    import uipickupfilter
    return uipickupfilter

def ConfigLootKinds(config):
    """The kinds a character's file keeps (loot_*), from before the filter."""
    mask = 0
    for key, label, bit in LOOT_KINDS:
        if config.get(key):
            mask |= bit
    return mask

def LootMask(config):
    """What the hunt asks for: nothing without "Autopodnoszenie", else the pick-up
    filter's kinds (every kind with the filter off). MT2009 PLUS: the filter,
    not upstream's KindsMask (uipickupfilter.py)."""
    if not config.get('pickup'):
        return 0
    mask = PickupFilter().EffectiveKinds()
    return mask if mask else LOOT_YANG_ONLY

def LootCoarseMask(config):
    """LootMask in the seven kinds a server before the split reads."""
    bits = dict((key, bit) for key, label, bit in LOOT_KINDS)
    parents = dict(LOOT_SPLIT_FROM)
    fine = LootMask(config)
    mask = 0
    for key, label, bit in LOOT_KINDS:
        if fine & bit:
            mask |= bits[parents.get(key, key)]
    return mask

def FacingDegree(fromX, fromY, toX, toY):
    dx = toX - fromX
    dy = toY - fromY
    distance = math.sqrt(dx * dx + dy * dy)
    if distance <= 0:
        return 0.0
    degree = 180.0 * math.acos(max(-1.0, min(1.0, dy / distance))) / math.pi + 180.0
    if fromX >= toX:
        degree = 360.0 - degree
    return degree

def StopPoint(fromX, fromY, toX, toY, short):
    dx = fromX - toX
    dy = fromY - toY
    distance = math.sqrt(dx * dx + dy * dy)
    if distance <= short or distance <= 0:
        return (fromX, fromY)
    return (toX + dx * short / distance, toY + dy * short / distance)

def FindInventoryCell(vnum):
    if not vnum:
        return -1
    for cell in xrange(player.INVENTORY_MAX_NUM):
        if player.GetItemIndex(cell) == vnum:
            return cell
    return -1

MANA_ITEM_VNUMS = (
    27004, 27005, 27006, 27008,
    50815,
    27864, 27867, 27876,
    39012, 71019,
    50021,
)

_manaItems = {}


def IsManaItem(vnum):
    """Whether a potion slot holding ``vnum`` watches mana rather than health:
    a potion whose table restores mana and no health (value1 and value0 of
    USE_POTION and USE_POTION_NODELAY - the blue potions, the fish, the
    sushi, 27052), a blessing that restores a share of mana and none of
    health (value4 and value3), or one of MANA_ITEM_VNUMS. Read once per
    vnum."""
    if vnum in _manaItems:
        return _manaItems[vnum]
    mana = vnum in MANA_ITEM_VNUMS
    if not mana and vnum:
        try:
            item.SelectItem(vnum)
            if item.GetItemType() == item.ITEM_TYPE_USE and item.GetItemSubType() in (item.USE_POTION, item.USE_POTION_NODELAY):
                mana = ((item.GetValue(1) > 0 and item.GetValue(0) <= 0) or
                        (item.GetValue(4) > 0 and item.GetValue(3) <= 0))
        except Exception:
            mana = False
    _manaItems[vnum] = mana
    return mana

def InTargetSkillRange(skillIndex):
    """A skill of a class or of the horse: the only ones that fight. A guild's
    or a support skill is nobody's (TARGET_SKILL_RANGES)."""
    for low, high in TARGET_SKILL_RANGES:
        if low <= skillIndex <= high:
            return True
    return False

def NeedsTarget(skillIndex):
    """Whether a skill is cast at an enemy: a skill of a class or of the horse
    that is neither standing nor a toggle nor one of SELF_SKILLS. The client
    says what is standing and what is a toggle (skill.IsStandingSkill and
    skill.IsToggleSkill, both raising for a skill they do not know);
    STANDING_SKILLS answers where it cannot."""
    inRange = False
    for low, high in TARGET_SKILL_RANGES:
        if low <= skillIndex <= high:
            inRange = True
    if not inRange or skillIndex in SELF_SKILLS:
        return False
    # The exe's own answer first, STANDING_SKILLS only for an exe that has
    # none: 47 stands in the Polish skill table and is an aimed Arrow Shower
    # in the English one, where casting it on its clock shot whatever was
    # under the cursor, at any distance (teivos, 27 September).
    answered = False
    for name in ('IsToggleSkill', 'IsStandingSkill'):
        ask = getattr(skill, name, None)
        if ask is None:
            continue
        try:
            if ask(skillIndex):
                return False
            if name == 'IsStandingSkill':
                answered = True
        except Exception:
            pass
    return answered or skillIndex not in STANDING_SKILLS

def YesNo(value):
    return T('tak', 'yes') if value else T('nie', 'no')

def WlWyl(value):
    return T('W\xa3', 'ON') if value else T('WY\xa3', 'OFF')

# MT2009_PLUS_AUTOHUNT_PICKUP_TOGGLE_V1
def OnOff(value):
    return T('W\xb3\xb9czone', 'On') if value else T('Wy\xb3\xb9czone', 'Off')

# MT2009_PLUS_AUTOHUNT_PRIORITY_V1 (Autor: blaki)
def TargetModeText(mode):
    return T('Najbli\xbfszy', 'Nearest') if mode == TARGET_MODE_NEAREST else T('Fokus', 'Focus')


# ---------------------------------------------------------------------------
# The rider: "Bojowiec" (Setnil, 1 October: "aby mozna bylo ustawic na
# bojowca ze spacja w miejscu, zbiera itemy z filtra, odpala skille i
# odpalki"; the operator: "postac jesli skille ma uzywac to musi na chwile
# schodzic wlaczyc buffa ... podobnie jak boty to robia").
#
# A player plays a battle horse standing in one place: the space bar held, a
# Cape of Courage on a clock pulling the pack in, the ` key for the drop. In
# the saddle the client refuses every skill but the horse's own with a word
# over the head (NOT_HORSE_SKILL, CPythonPlayer::__CheckSkillUsable) and the
# server refuses it anyway (CHARACTER::UseSkill; combat.md, "No skill of a
# class is cast from a saddle"), so the rider climbs down for its buffs - the
# warrior's Aura, Berserk and Strong Body, a Sura's or a Shaman's own - and
# gets back on, as the bots do (ManagePlayerBotCombatBuffs, reason=buff).
#
# What a horse allows is the horse's level, and the client knows it only as
# the riding skill's (130): the server keeps that skill at the horse's level
# (CHARACTER::SetHorseLevel) and sends it whenever the horse levels up
# (horse.advance, horse.set_level). From level 11 - the combat horse of the
# armoured horse book, 20104-20106 - the rider swings from the saddle
# (CInstanceBase::SHORSE::CanAttack); from 21 - the military horse - it casts
# the four horse skills too (SHORSE::CanUseSkill, and the server's
# CanUseHorseSkill: grade 3). Below 11 the switch changes nothing.
#
# Everything sent is what a player sends: the attack key and SetRotation for
# the swing, SetTarget for the mark, ClickSkillSlot for a cast, "/ride" for
# Ctrl+G - the server's do_ride stops riding a rider and seats one on foot on
# the summoned horse, and StopRiding summons the horse as a follower, so the
# way back is always there - and the ` key's "/pickup_nearby <kinds>" for
# the drop: every wanted item within the server's own 600 (CItem::
# DistanceValid), nearest first, forty at most, in one command, so the rider
# never walks to an item. It is sent only when "/autohunt_loot" names a
# wanted one within reach, or a window that makes the character busy
# (CanHandleItem) would be told "busy" every second over nothing.
RIDER_HORSE_LEVEL = 11
RIDER_SKILL_HORSE_LEVEL = 21
RIDING_SKILL = 130
# Where playersettingmodule.py puts the riding skill (the support list's ninth),
# for an exe whose player.GetSkillSlotIndex cannot be asked.
RIDING_SLOT = 109
# The client keeps a skill's level from its grade's start
# (CPythonPlayer::SetSkillLevel_): a horse of 21 is grade 1, level 2.
GRADE_LEVEL_BASE = (0, 19, 29, 39)
# SKILL_HORSE_WILDATTACK, _CHARGE, _ESCAPE and _WILDATTACK_RANGE.
HORSE_SKILLS = (137, 138, 139, 140)
# What the rider climbs down for: the self-buffs of every class that fight
# with it. Not Feather Walk (49) or Swiftness (110), which a horse outruns
# (the bots leave them in the saddle too), not Stealth (34), which ends the
# fight, and not Cure (109), a heal. Flame Spirit (78) is a toggle that goes
# on striking from the saddle (ComputeSkill lets it), so it is worth the
# climb-down.
RIDER_BUFFS = (3, 4, 19, 63, 64, 65, 78, 79, 94, 95, 96, 111)
# How far a swing from the saddle reaches a monster that came to the rider,
# and how far round it the server is asked for one: a stone or a boss is
# ranked before every monster (apply_auto_hunt_stone_priority), and one beyond
# the swing would hold the rider's face while the pack hit it from behind. A
# bow reaches as on foot (ARCHER_REACH, inside the client's bow range of
# 2400 and more, so a shot never walks).
RIDER_REACH = 350
RIDER_RANGE = 700
# What the server names to pick up, and how near it has to lie: the server's
# 600 is DISTANCE_APPROX's, a few per cent off the straight line.
RIDER_LOOT_RANGE = 600
RIDER_PICK_DISTANCE = 550
# One batch a second: the server takes one every half second a character.
RIDER_PICK_INTERVAL = 1.0
# A target that stays beyond the swing is skipped (for STUCK_SKIP_SECONDS):
# a monster coming at the rider crosses the gap in a second or two.
RIDER_STALE_SECONDS = 4.0
# How often the monster in reach is marked again while the client has not
# taken the mark (RiderTarget).
RIDER_MARK_INTERVAL = 0.3
# How often the buffs are looked at in the saddle.
RIDER_BUFF_CHECK = 0.5



RIDER_RENEW_BEFORE = 10.0
RIDER_RENEW_ON_FOOT = 45.0
# When the rider may climb down: nothing at it (no monster in its reach in
# hand, and its health has not dropped for RIDER_QUIET_SECONDS), or health
# enough to take a few blows on foot. On foot under RIDER_DANGER_HP it gets
# back on at once, whatever is left to cast.
RIDER_SAFE_HP = 60
RIDER_DANGER_HP = 35
RIDER_QUIET_SECONDS = 2.0
# A cast that brought no buff within this long was refused - a weapon the
# buff is not for, a skill the client will not cast - and that buff takes
# nobody down for RIDER_REFUSED_SKIP. Without it a rider with Aura in a slot
# and a fan in the hand would climb down every few seconds for nothing.
RIDER_CONFIRM_SECONDS = 3.0
RIDER_REFUSED_SKIP = 60.0
# How long the client is given to show the character on foot or in the
# saddle after "/ride", and how many tries each way. A mount is refused
# within a second of the last (do_ride's HorseUse pulse), and the quick tries
# come RIDER_STEP_WAIT apart, past it; after them the hunt goes on on foot
# and tries every RIDER_MOUNT_SLOW_RETRY - a horse too tired to carry anybody
# gets its stamina back a point every twelve minutes - and stops trying
# after RIDER_MOUNT_MAX_TRIES.
RIDER_STEP_WAIT = 3.0


RIDER_SUMMON_WAIT = 1.0
RIDER_DISMOUNT_TRIES = 2
RIDER_MOUNT_QUICK_TRIES = 3
RIDER_MOUNT_SLOW_RETRY = 15.0
RIDER_MOUNT_MAX_TRIES = 20
# A climb-down that did not happen waits this long before the next; and two
# climb-downs are never closer than RIDER_CYCLE_GAP.
RIDER_CYCLE_BACKOFF = 20.0
RIDER_CYCLE_GAP = 5.0
# On foot no longer than this, whatever is left to cast.
RIDER_FOOT_LIMIT = 12.0
# Off the horse without the hunt's doing is decided only after this long: a
# rider who dies is taken off its horse a moment before its health reads 0.
RIDER_FOOT_GRACE = 1.5
RIDER_ATTACK_TRIES = 2
RIDER_ATTACK_RETRY = 5.0
RIDER_CAST_FALLBACK = 0.8
RIDER_CAST_MIN_GAP = 0.15




RIDER_PAUSE_DISMOUNT = (0.3, 0.7)
RIDER_PAUSE_ON_FOOT = (0.4, 0.8)
RIDER_PAUSE_CAST = (1.1, 1.5)
RIDER_PAUSE_MOUNT = (0.4, 0.8)
# The server drops a sixth command in half a second without a word
# (ENABLE_ANTI_CMD_FLOOD), so the rider's own commands - "/ride" and the
# pick-up - go no sooner than this after the hunt's last one.
COMMAND_GAP = 0.2
# How often the rider makes sure it hears of the affects (WatchAffects).
RIDER_WATCH_INTERVAL = 5.0
# A buff's length when the client has no list of the affects and its skill
# table names none.
RIDER_FALLBACK_BUFF_SECONDS = 60

RIDE = 'ride'
DISMOUNT = 'dismount'
TO_FOOT = 'to_foot'
CAST = 'cast'
MOUNT = 'mount'
TO_SADDLE = 'to_saddle'


def HumanPause(span):
    """A pause within span, as a hand takes one; the middle on an exe or a
    stub that cannot draw."""
    (low, high) = span
    try:
        return app.GetRandom(int(low * 1000), int(high * 1000)) / 1000.0
    except Exception:
        return (low + high) / 2.0


def SkillLevelOf(slot):
    """A skill's level as the server counts it, 0 for one not learned."""
    try:
        grade = player.GetSkillGrade(slot)
        level = player.GetSkillLevel(slot)
    except Exception:
        return 0
    if grade <= 0:
        return max(0, level)
    return GRADE_LEVEL_BASE[min(grade, len(GRADE_LEVEL_BASE) - 1)] + level


def HorseLevel():
    """The horse's level, from the riding skill; 0 when there is none."""
    slot = RIDING_SLOT
    try:
        slot = player.GetSkillSlotIndex(RIDING_SKILL)
    except Exception:
        pass
    try:
        if player.GetSkillIndex(slot) != RIDING_SKILL:
            return 0
    except Exception:
        return 0
    return SkillLevelOf(slot)


def IsMounted():
    ask = getattr(player, 'IsMountingHorse', None)
    if ask is None:
        return False
    try:
        return bool(ask())
    except Exception:
        return False


def IsToggle(skillIndex):
    try:
        return bool(skill.IsToggleSkill(skillIndex))
    except Exception:
        return False


def AffectDict():
    """The affects on this character as game.py keeps them for the root
    (BINARY_NEW_AddAffect: constInfo.AFFECT_DICT, {type: {point: (value,
    duration)}}, a skill's buff under the skill's number); None for a root
    without it."""
    for name in ('constInfo', 'constinfo'):
        module = sys.modules.get(name)
        affects = getattr(module, 'AFFECT_DICT', None) if module is not None else None
        if isinstance(affects, dict):
            return affects
    return None


def AffectDuration(entry):
    """The longest duration an affect's points were added with."""
    longest = 0
    try:
        for (value, duration) in entry.values():
            longest = max(longest, int(duration))
    except Exception:
        return 0
    return longest


def ItemAffectPresent(vnum, cell):
    """Use the same (affect, point) identity as the server and icon bar.
    Shared potion effects must match even when another item supplied them.
    HP/SP recovery potions and unrelated points are deliberately left alone.
    """
    affects = AffectDict()
    if not affects:
        return False
    try:
        item.SelectItem(vnum)
        kind = item.GetItemType()
        if kind == getattr(item, 'ITEM_TYPE_BLEND', -1):
            point = player.GetItemMetinSocket(cell, 0)
            return point in affects.get(chr.AFFECT_BLEND, {}) or player.POINT_RESIST_MAGIC in affects.get(chr.NEW_AFFECT_EXP_BONUS_EURO_FREE, {})
        if kind != item.ITEM_TYPE_USE:
            return False
        subtype = item.GetItemSubType()
        if subtype == getattr(item, 'USE_AFFECT', -1):
            affect, point = item.GetValue(0), item.GetValue(1)
            return point in affects.get(affect, {}) or (point != 0 and point in affects.get(chr.AFFECT_POTION_BOOST, {}))
        if subtype == getattr(item, 'USE_ABILITY_UP', -1):
            point = item.GetValue(0)
            
            affect = 204
            if point == player.POINT_MOV_SPEED:
                affect = chr.AFFECT_MOV_SPEED_POTION
            elif point == player.POINT_ATT_SPEED:
                affect = chr.AFFECT_ATT_SPEED_POTION
            return point in affects.get(affect, {})
    except (AttributeError, TypeError, ValueError, RuntimeError):
        pass
    return False


def BuffSeconds(slot, skillIndex):
    """How long a buff lasts by the client's skill table, for a client that
    keeps no list of the affects."""
    try:
        seconds = int(skill.GetDuration(skillIndex, player.GetSkillCurrentEfficientPercentage(slot)))
    except Exception:
        seconds = 0
    return seconds if seconds > 0 else RIDER_FALLBACK_BUFF_SECONDS


def CanCastNow(slot, skillIndex):
    """What the client asks before it sends a cast (__CheckSkillUsable): off
    its cooldown, learned, and the mana - or for a skill paid in health, the
    health - for it."""
    try:
        if player.IsSkillCoolTime(slot):
            return False
    except Exception:
        return False
    if SkillLevelOf(slot) <= 0:
        return False
    try:
        need = skill.GetSkillNeedSP(skillIndex, player.GetSkillCurrentEfficientPercentage(slot))
    except Exception:
        return True
    if need <= 0:
        return True
    try:
        if skill.IsUseHPSkill(skillIndex):
            return player.GetStatus(player.HP) >= need
    except Exception:
        pass
    return player.GetStatus(player.SP) >= need


class Hunter(object):
    def __init__(self):
        self.config = DefaultConfig()
        self.config_global = dict(GLOBAL_DEFAULTS)
        self.configName = None
        self.running = False
        self.mainWindow = None
        self.lootWindow = None
        self.isLoaded = False
        # The first frame of a game phase waits here until the character has
        # a name (OnGameSession): True after an autologin, False otherwise.
        self.pendingSession = None
        # The rider's knowledge of its buffs: when each was last put up and
        # for how long ({skill: (time, seconds)}), kept across hunts - it is
        # the character's, not the hunt's - and forgotten with the character.
        self.riderBuffStamp = {}
        # Set when the rider's code fails: the rest of the game runs it no more.
        self.riderBroken = False
        self.lastCommandAt = -1000.0
        
        
        self.pathKind = PATH_KIND_NONE
        self.pathSeq = 0
        self.LoadGlobalConfig()
        self.ResetState()

    def OnServerTargetHP(self, vid, hp, maxHp):
        """The target's health from the server (game.py "TargetHP"). At 0
        the monster is dead, whatever the exe can tell: this one has no
        player.IsTargetDead, and a corpse stays two or three seconds."""
        # MT2009_PLUS_AUTOHUNT_PRIORITY_V1 (Autor: blaki): a fall of the
        # target's health is "Fokus"'s proof that the blows land.
        if vid and vid == self.targetVid:
            if self.targetLastHP is None or hp < self.targetLastHP:
                self.focusLastProgress = clientclock.Now()
            self.targetLastHP = hp
        if hp <= 0 and vid:
            if not hasattr(self, 'deadVids'):
                self.deadVids = {}
            now = clientclock.Now()
            self.deadVids[vid] = now + 10.0
            if len(self.deadVids) > 64:
                for key in [k for k, until in self.deadVids.items() if until < now]:
                    del self.deadVids[key]

    def IsKnownDead(self, vid):
        if hasattr(player, 'IsTargetDead') and player.IsTargetDead(vid):
            return True
        until = getattr(self, 'deadVids', {}).get(vid, 0.0)
        return until and clientclock.Now() < until

    def ResetState(self):
        self.targetVid = 0
        # MT2009_PLUS_AUTOHUNT_PRIORITY_V1 (Autor: blaki)
        self.targetLastHP = None
        self.focusLastProgress = 0.0
        self.forceNearestUntil = 0.0
        # MT2009_PLUS_AUTOHUNT_MOUNT_V1 (Autor: blaki): a recovery under way
        # outlives a new start (it ends by itself).
        self.mountRecoveryPhase = getattr(self, 'mountRecoveryPhase', 0)
        if not self.mountRecoveryPhase:
            self.mountRecoveryAt = 0.0
            self.mountRecoveryStarted = 0.0
            self.mountRecoveryRemountStarted = 0.0
            self.mountRecoveryOffConfirmed = False
            self.mountRecoveryOnConfirmed = False
        self.mountRecoveryCooldownUntil = getattr(self, 'mountRecoveryCooldownUntil', 0.0)
        self.ResetMountWatch()
        self.skipVid = 0
        self.skipUntil = 0.0
        # The target that boxed the hunter in, on a slot of its own so the
        # minute's skip (and a corpse's) is not forgotten for its two seconds;
        # while it runs the server is asked for the nearest monster plain.
        self.blockedVid = 0
        self.blockedUntil = 0.0
        self.blockedWaiting = False
        self.lastTargetRequestAt = -1.0
        self.ResetChaseMovement()
        self.ResetEscape()
        self.ResetNavigation()
        self.attacking = False
        self.anchor = (0, 0)
        self.nextRequest = 0.0
        self.nextMove = 0.0
        self.nextFace = 0.0
        self.nextPotion = 0.0
        self.nextRevive = 0.0
        self.deadSince = 0.0
        self.justRevived = False
        self.approachSince = 0.0
        self.approachBest = 0.0
        self.lootBest = 0.0
        self.skillNext = [0.0] * SKILL_SLOTS
        self.itemNext = [0.0] * USE_ITEM_SLOTS
        self.lootVid = 0
        self.lootPos = (0, 0)
        self.lootSince = 0.0
        self.lootPausedUntil = 0.0
        self.nextLootRequest = 0.0
        self.nextLootPick = 0.0
        self.nextBuffGlobal = 0.0
        self.skillHoldUntil = 0.0
        self.lootSweeping = False
        self.lootSweepIdleSince = 0.0
        self.targetMissFrames = 0
        self.targetSetSince = 0.0
        self.lootPickAttempts = {}
        self.lootSkippedVids = {}
        self.ResetRider()

    def ResetRider(self):
        # RIDE in the saddle (or on foot, with the hunt on foot), or a step of
        # a climb-down for buffs or of the way back on, until riderPhaseUntil.
        self.riderPhase = RIDE
        self.riderPhaseUntil = 0.0
        self.mountedSkillCycle = False
        
        
        
        self.riderSaddle = False
        self.riderNeedsSummon = False
        self.riderWasMounted = False
        self.riderFootSince = 0.0
        self.riderFootAt = 0.0
        self.riderNextMount = 0.0
        self.riderMountTries = 0
        self.riderMountGroupTries = 0
        self.riderMountGroupLimit = RIDER_MOUNT_QUICK_TRIES
        self.riderDismountTries = 0
        self.riderNextCycle = 0.0
        self.riderNextBuffCheck = 0.0
        self.riderNextWatch = 0.0
        # The slots cast in this climb-down, the casts waiting for their buff
        # ({skill: (slot index, slot, time)}), and the slots refused lately.
        self.riderTried = set()
        self.riderAttackPending = None
        self.riderAttackAttempts = {}
        self.riderTargetGraceUntil = 0.0
        self.riderNextSkillTarget = 0.0
        self.riderNextCast = 0.0
        self.riderNextTargetRequest = 0.0
        self.riderTargetSearchUntil = None
        self.riderPending = {}
        self.riderRefused = {}
        self.riderStaleSince = 0.0
        self.riderNextMark = 0.0
        self.riderLastHp = None
        self.riderHpDropAt = -1000.0
        self.riderSaidRetry = False

    def CanUpdate(self):
        # Asked on every frame of the game, running or not: the autologin
        # learns here that the game is open again and says when a hunt it
        # paused is due (autologin.GameFrame).
        event = autologin.GameFrame()
        if event == autologin.NEW_SESSION:
            self.pendingSession = False
        elif event == autologin.RECONNECTED:
            self.pendingSession = True
        elif event == autologin.RESUME:
            self.ResumeAfterAutoLogin()
        if self.pendingSession is not None:
            self.OnGameSession(self.pendingSession)
        # MT2009_PLUS_AUTOHUNT_MOUNT_V1: a remount begun is finished even
        # when the hunt is stopped while the character stands on foot.
        return self.running or self.mountRecoveryPhase != 0

    def OnGameSession(self, reconnected):
        """The first frame of a game phase with a named character. Back after
        a dropped connection with the same character, the settings in memory
        are the ones it hunted with, saved or not; any other entry reads the
        character's file, and with it the autologin switch."""
        name = player.GetMainCharacterName()
        if not name:
            return
        self.pendingSession = None
        if reconnected and name == self.configName:
            self.isLoaded = True
            autologin.SetArmed(self.config.get('autologin', 0))
            chat.AppendChat(chat.CHAT_TYPE_INFO, T('Auto \xa3owy: zalogowano ponownie po zerwaniu po\xb3\xb9czenia.',
                'Auto Hunt: logged in again after the connection dropped.'))
            return
        if name != self.configName:
            self.riderBuffStamp = {}
        self.configName = name
        self.LoadConfig()
        self.isLoaded = True

    def ResumeAfterAutoLogin(self):
        if self.running:
            return
        if not self.isLoaded:
            autologin.DelayResume(2.0)
            return
        self.Start()
        chat.AppendChat(chat.CHAT_TYPE_INFO, T('Auto \xa3owy: wznowione po ponownym zalogowaniu.',
            'Auto Hunt: resumed after logging in again.'))

    def OnUpdate(self):
        # MT2009_PLUS_SIDEKICK_WARP_SAFE_V1: no packet (target, loot, potion,
        # restart) on the way to another core (warpsafe.py).
        import warpsafe
        if not warpsafe.InGame():
            self.mountRecoveryPhase = 0
            return
        now = clientclock.Now()

        # MT2009_PLUS_AUTOHUNT_MOUNT_V1: the remount takes the whole frame.
        if self.HandleMountRecovery(now):
            return
        if not self.running:
            return

        if player.GetStatus(player.HP) <= 0:
            
            
            self.ResetEscape()
            self.ResetNavigation()
            self.WhileDead(now)
            return
            
        if self.deadSince > 0.0:
            self.justRevived = True
            self.deadSince = 0.0

        if self.justRevived:
            maxHP = player.GetStatus(player.MAX_HP)
            curHP = player.GetStatus(player.HP)
            threshold = min(100, self.config.get('revive_hp_percent', 60))
            if maxHP > 0 and curHP * 100 < maxHP * threshold:
                self.HandleItems(now)
                self.CastSkills(now, buffsOnly=True)
                self.ResetChaseMovement()
                return
            self.justRevived = False

        self.HandleItems(now)
        
        
        
        
        if not self.RiderFrame(now):
            self.CheckPathPending(now)
            if self.WatchMountStuck(now):
                return
            if not self.EscapeFrame(now):
                self.RememberWalkPosition()
                self.CastSkills(now)
                self.AskForLoot(now)
                self.Chase(now)
        else:
            # A walk's measure taken before the saddle is no measure after it.
            self.ResetChaseMovement()
            self.ResetEscape()
            self.ResetReturnMovement()
            self.ResetPath()
            self.returning = False

        if self.lootWindow and self.lootWindow.IsShow():
            try:
                base_range = self.config.get('range', 2000)
                if self.RiderActive():
                    player.SetAutoHuntRangeCircle(self.RiderRange(), 0.0, 0.0, 0)
                elif self.running and self.config.get('return', 0):
                    (ax, ay) = self.anchor
                    player.SetAutoHuntRangeCircle(base_range, float(ax), float(ay), 1)
                else:
                    player.SetAutoHuntRangeCircle(base_range, 0.0, 0.0, 0)
            except AttributeError:
                pass

    def Destroy(self):
        # The game window is closing: whether the hunt ran is what the
        # autologin resumes after a drop, so it is told before the Stop.
        # Nothing is sent into a closing game (the rider's way back on).
        autologin.NoteGameClosed(self.running)
        self.Stop(quiet=True, remount=False)
        if self.mainWindow:
            self.mainWindow.Hide()
            self.mainWindow.Destroy()
            self.mainWindow = None
            
        if self.lootWindow:
            self.lootWindow.Hide()
            self.lootWindow.Destroy()
            self.lootWindow = None
            
        self.isLoaded = False

    def Start(self, startText=None):
        if self.running:
            return
        self.ResetState()
        (x, y, z) = player.GetMainCharacterPosition()
        self.anchor = (int(x), int(y))
        self.running = True
        # MT2009_PLUS_AUTOHUNT_QUICK_V1: the quick start says its own line.
        chat.AppendChat(chat.CHAT_TYPE_INFO, startText or (T('Auto \xa3owy: start, zasi\xeag %d.',
            'Auto Hunt: started, range %d.') % self.config['range']))
        if not LootMask(self.config):
            chat.AppendChat(chat.CHAT_TYPE_INFO, T('Auto \xa3owy: podnoszenie jest wy\xb3\xb9czone.',
                'Auto Hunt: picking up is switched off.'))
        elif PickupFilter().IsActive():
            chat.AppendChat(chat.CHAT_TYPE_INFO, T('Auto \xa3owy: podnosz\xea wed\xb3ug filtra podnoszenia (Ctrl+Z).',
                'Auto Hunt: picking up by the pick-up filter (Ctrl+Z).'))
        if self.config.get('rider', 0):
            self.RiderSwitched()

    def Stop(self, quiet=False, remount=True):
        if not self.running:
            return
        # Stopped on the ground of a climb-down the hunt made: the character
        # is put back in the saddle it was taken out of.
        if remount:
            self.RiderRemountOnStop()
        self.ResetRider()
        self.running = False
        if self.escapeUntil:
            
            (px, py, pz) = player.GetMainCharacterPosition()
            self.WalkTo(px, py)
        self.ResetEscape()
        self.ReleaseAttack()
        self.targetVid = 0
        self.lootVid = 0
        self.lootSweeping = False
        self.lootSweepIdleSince = 0.0
        if not quiet:
            chat.AppendChat(chat.CHAT_TYPE_INFO, T('Auto \xa3owy: stop.', 'Auto Hunt: stopped.'))

    def OnServerOff(self, reason=''):
        # The world is played without Auto Lowy (M2_AUTOHUNT=0 on the server,
        # the launcher's difficulty window), or with it only for a character
        # with time from the ItemShop's "Auto Lowy (8h)" and this one has none
        # (M2_AUTOHUNT_ITEM=1; the reason "item", server 2.2.26): the target
        # and the drop are refused with "AutoHuntOff", and the hunt stops and
        # says why - once a start, however many refusals were already on
        # their way. A server before 2.2.26 sends no reason.
        if not self.running:
            return
        self.Stop(quiet=True)
        if reason == 'item':
            chat.AppendChat(chat.CHAT_TYPE_INFO, T('Auto \xa3owy: brak czasu. Kup "Auto \xa3owy (8h)" w ItemShopie i u\xbfyj go z ekwipunku - czas leci tylko w grze.',
                'Auto Hunt: no time left. Buy its 8-hour ticket in the ItemShop and use it from your inventory - the time only runs while you play.'))
            return
        chat.AppendChat(chat.CHAT_TYPE_INFO, T('Auto \xa3owy s\xb9 wy\xb3\xb9czone na tym serwerze (ustawienia \x9cwiata w launcherze).',
            'Auto Hunt is switched off on this server (the world settings in the launcher).'))

    def OnServerTarget(self, value, blocked='0'):
        if not self.running or not self.config.get('attack', 1):
            return
        if self.mountRecoveryPhase:
            return
        if self.RiderActive():
            self.RiderOnServerTarget(value)
            return
        if self.lootSweeping:
            return
        new_vid = ParseTargetVid(value)
        if not new_vid:
            return
        # The server is told one target to leave out; a second one left is
        # refused here (the minute's skip while the box's two seconds are
        # the one sent, and an answer already on its way when either began).
        now = clientclock.Now()
        if self.escapeUntil or now < self.escapeWaitUntil:
            if self.justRevived:
                return
            
            distance = player.GetCharacterDistance(new_vid)
            if distance < 0 or distance > min(MELEE_REACH, self.Reach()):
                return
            if not hasattr(player, 'IsTargetDead') or player.IsTargetDead(new_vid):
                return
            px, py, pz = player.GetMainCharacterPosition()
            self.WalkTo(px, py)
            self.ResetEscape()
            self.ResetChaseMovement()
            self.blockedTargets.pop(new_vid, None)
            self.targetBlockAttempts.pop(new_vid, None)
            if self.skipVid == new_vid:
                self.skipVid = 0
                self.skipUntil = 0.0
            self.targetVid = 0
            self.combatRecoveryUntil = 0.0
            self.nextRequest = now + TARGET_REQUEST_INTERVAL
        if new_vid == self.blockedVid and now < self.blockedUntil:
            return
        if new_vid == self.skipVid and now < self.skipUntil:
            return
        if self.IsTargetBlocked(new_vid, now):
            return

        # MT2009_PLUS_AUTOHUNT_PRIORITY_V1 (Autor: blaki): "Fokus" keeps a
        # live target whatever the server names; "Najblizszy" takes the new
        # one at once.
        if self.targetVid != 0 and new_vid != self.targetVid:
            if self.TargetMode() == TARGET_MODE_FOCUS:
                if player.GetCharacterDistance(self.targetVid) >= 0 and not self.IsKnownDead(self.targetVid):
                    return

        if new_vid != self.targetVid:
            self.returning = False
            self.ResetReturnMovement()
            if self.pathPoints and self.pathVid != new_vid:
                self.ResetPath()
            self.ReleaseAttack()
            # A walk already measured goes on being measured from where it
            # was held, against the new target's distance from now on: a
            # server that names one target and then another behind the same
            # wall would otherwise start the measure again every time, and
            # the hunter would stand there for good (Colide).
            if self.chasePosition is not None:
                self.chaseVid = new_vid
                self.chaseDistance = None
            else:
                self.ResetChaseMovement()
            self.blockedWaiting = False
            self.targetVid = new_vid
            self.targetLastHP = None
            self.focusLastProgress = now
            self.forceNearestUntil = 0.0
            self.approachSince = 0.0
            self.nextMove = 0.0
            self.nextFace = 0.0
            self.targetMissFrames = 0
            self.targetSetSince = 0.0
        # MT2009_PLUS_AUTOHUNT_BLOCKED_V1 (Autor: blaki): a wall or a rock on
        # the straight line - the way round is asked at once from the
        # existing /autohunt_path (not upstream's /autohunt_route).
        if str(blocked) == '1' and new_vid == self.targetVid:
            self.AskBlockedPath(now, new_vid)

    # MT2009_PLUS_AUTOHUNT_BLOCKED_V1 (Autor: blaki)
    def AskBlockedPath(self, now, vid):
        if self.pathPoints and self.pathPurpose == 'target' and self.pathVid == vid:
            return False
        if not self.PathAskable(vid, now):
            return False
        distance = player.GetCharacterDistance(vid)
        if 0 <= distance <= self.Reach():
            return False
        (tx, ty, tz) = chr.GetPixelPosition(vid)
        return self.AskPath(now, 'target', vid, tx, ty)

    # MT2009_PLUS_AUTOHUNT_PRIORITY_V1 (Autor: blaki)
    def TargetMode(self):
        if self.config.get('target_mode', TARGET_MODE_FOCUS) == TARGET_MODE_NEAREST:
            return TARGET_MODE_NEAREST
        return TARGET_MODE_FOCUS

    def PriorityOrder(self):
        return max(0, min(len(PRIORITY_LABELS) - 1, self.config.get('priority_order', 0)))

    def FocusIdle(self, now, vid):
        """The target in reach whose health has not fallen for
        FOCUS_IDLE_SECONDS: let go for the minute, the nearest asked for."""
        if now - self.focusLastProgress < FOCUS_IDLE_SECONDS:
            return False
        self.ReleaseAttack()
        if player.GetTargetVID() != 0:
            player.ClearTarget()
        self.BlockTarget(vid, now)
        self.skipVid = vid
        self.skipUntil = now + STUCK_SKIP_SECONDS
        self.targetVid = 0
        self.targetLastHP = None
        self.approachSince = 0.0
        self.targetSetSince = 0.0
        self.targetMissFrames = 0
        self.forceNearestUntil = now + FOCUS_FALLBACK_SECONDS
        self.nextRequest = 0.0
        return True

    # MT2009_PLUS_AUTOHUNT_MOUNT_V1 (Autor: blaki) ---------------------------
    def ResetMountWatch(self):
        self.mountWatchPosition = None
        self.mountWatchSince = 0.0
        self.walkIntentAt = 0.0

    def OnServerMount(self, action, mounted):
        try:
            mounted = int(mounted) != 0
        except (TypeError, ValueError):
            return
        if action == 'off' and self.mountRecoveryPhase == 1 and not mounted:
            self.mountRecoveryOffConfirmed = True
        elif action == 'on' and self.mountRecoveryPhase == 2:
            self.mountRecoveryOnConfirmed = mounted

    def WatchMountStuck(self, now):
        """A mounted walk that gains nothing for MOUNT_STUCK_SECONDS starts
        the remount; True when it did (the frame is taken)."""
        if (not IsMounted() or self.riderPhase != RIDE or
                now - self.walkIntentAt > MOUNT_WALK_INTENT_SECONDS):
            self.mountWatchPosition = None
            return False
        (px, py, pz) = player.GetMainCharacterPosition()
        if (self.mountWatchPosition is None or
                (px - self.mountWatchPosition[0]) ** 2 +
                (py - self.mountWatchPosition[1]) ** 2 >= COMBAT_MOVE_THRESHOLD ** 2):
            self.mountWatchPosition = (px, py)
            self.mountWatchSince = now
            return False
        if now - self.mountWatchSince < MOUNT_STUCK_SECONDS:
            return False
        return self.TryMountRecovery(now)

    def TryMountRecovery(self, now):
        if (self.mountRecoveryPhase or now < self.mountRecoveryCooldownUntil or
                not IsMounted()):
            return False
        self.ReleaseAttack()
        (px, py, pz) = player.GetMainCharacterPosition()
        self.WalkTo(px, py)
        self.ResetChaseMovement()
        self.ResetMountWatch()
        self.nextMove = 0.0
        self.mountRecoveryPhase = 1
        self.mountRecoveryStarted = now
        self.mountRecoveryAt = now + MOUNT_RECOVERY_RETRY
        self.mountRecoveryOffConfirmed = False
        self.mountRecoveryOnConfirmed = False
        self.mountRecoveryCooldownUntil = now + MOUNT_RECOVERY_COOLDOWN
        chat.AppendChat(chat.CHAT_TYPE_INFO, T('Auto \xa3owy: wierzchowiec utkn\xb9\xb3 - zsiadam i wsiadam ponownie.',
            'Auto Hunt: the mount is stuck - getting off and on again.'))
        self.Command('/autohunt_mount off', now)
        return True

    def HandleMountRecovery(self, now):
        if not self.mountRecoveryPhase:
            return False
        if player.GetStatus(player.HP) <= 0:
            self.mountRecoveryPhase = 0
            return False
        if self.mountRecoveryPhase == 1:
            if (self.mountRecoveryOffConfirmed and
                    now - self.mountRecoveryStarted >= MOUNT_RECOVERY_DELAY):
                self.mountRecoveryPhase = 2
                self.mountRecoveryRemountStarted = now
                self.mountRecoveryAt = now + MOUNT_RECOVERY_RETRY
                self.Command('/autohunt_mount on', now)
                return True
            if now - self.mountRecoveryStarted >= MOUNT_RECOVERY_DISMOUNT_TIMEOUT:
                # Still in the saddle: nothing to restore. Off without the
                # server's word: go on to the remount.
                if IsMounted():
                    self.mountRecoveryPhase = 0
                    return False
                self.mountRecoveryOffConfirmed = True
                return True
            if now >= self.mountRecoveryAt and not self.mountRecoveryOffConfirmed:
                self.Command('/autohunt_mount off', now)
                self.mountRecoveryAt = now + MOUNT_RECOVERY_RETRY
            return True
        if self.mountRecoveryOnConfirmed and IsMounted():
            self.mountRecoveryPhase = 0
            # The target is kept; a way round it is asked again.
            self.ResetChaseMovement()
            self.ResetMountWatch()
            if self.pathPoints:
                self.ResetPath()
            self.pathTriedAt.pop(self.targetVid, None)
            self.nextMove = 0.0
            self.nextRequest = 0.0
            return False
        if now - self.mountRecoveryRemountStarted >= MOUNT_RECOVERY_REMOUNT_TIMEOUT:
            self.mountRecoveryPhase = 0
            wasRunning = self.running
            self.Stop(quiet=True, remount=False)
            chat.AppendChat(chat.CHAT_TYPE_INFO, T('Auto \xa3owy: nie uda\xb3o si\xea ponownie wsi\xb9\x9c\xe6 na wierzchowca po utkni\xeaciu - \xb3owy zatrzymane.' if wasRunning else 'Auto \xa3owy: nie uda\xb3o si\xea ponownie wsi\xb9\x9c\xe6 na wierzchowca po utkni\xeaciu.',
                'Auto Hunt: could not get back on the mount after it got stuck - hunting stopped.'))
            return True
        if now >= self.mountRecoveryAt:
            # Idempotent on the server: a late success stays mounted.
            self.Command('/autohunt_mount on', now)
            self.mountRecoveryAt = now + MOUNT_RECOVERY_RETRY
        return True

    def OnServerLoot(self, vid, x, y):
        if not self.running or clientclock.Now() < self.lootPausedUntil or not LootMask(self.config):
            return
        (new_vid, dx, dy) = ParseLoot(vid, x, y)
        if new_vid:
            now = clientclock.Now()
            skip_until = self.lootSkippedVids.get(new_vid, 0.0)
            if now < skip_until:
                return
            elif skip_until:
                del self.lootSkippedVids[new_vid]
        if new_vid != self.lootVid:
            self.lootSince = 0.0
            self.nextMove = 0.0
        self.lootVid = new_vid
        (px, py, pz) = player.GetMainCharacterPosition()
        self.lootPos = (int(px) + dx, int(py) + dy)

    def WhileDead(self, now):
        self.ReleaseAttack()
        self.ResetChaseMovement()
        self.targetVid = 0
        self.lootVid = 0
        self.lootSweeping = False
        self.lootSweepIdleSince = 0.0
        self.RiderOnDeath()
        if not self.deadSince:
            self.deadSince = now
            return
        wait = max(REVIVE_MIN_SECONDS, self.config['revive_after'])
        if self.config['revive'] and now - self.deadSince >= wait and now >= self.nextRevive:
            self.nextRevive = now + REVIVE_RETRY
            self.justRevived = True
            self.Command('/restart_here', now)

    def HandleItems(self, now):
        if self.config['use_potions'] and now >= self.nextPotion:
            wanted = False
            for i in xrange(6):
                vnum = self.config['item%d_vnum' % i]
                val = self.config['item%d_val' % i]
                if not vnum or val <= 0:
                    continue
                if self.justRevived and vnum in COURAGE_CAPE_VNUMS:
                    continue

                if IsManaItem(vnum):
                    curPoint = player.GetStatus(player.SP)
                    maxPoint = player.GetStatus(player.MAX_SP)
                else:
                    curPoint = player.GetStatus(player.HP)
                    maxPoint = player.GetStatus(player.MAX_HP)

                if maxPoint > 0 and (curPoint * 100) <= (maxPoint * val):
                    # A second before the next look whether or not the bag
                    # still has one: the search walks every cell of four
                    # pages, and on every frame it would cost the frame.
                    wanted = True
                    cell = FindInventoryCell(vnum)
                    if cell >= 0 and not ItemAffectPresent(vnum, cell):
                        net.SendItemUsePacket(cell)

            if wanted:
                self.nextPotion = now + POTION_INTERVAL

        if self.config['use_buffs']:
            if now >= self.nextBuffGlobal:
                for i in xrange(6, 18):
                    vnum = self.config['item%d_vnum' % i]
                    interval = self.config['item%d_val' % i]
                    if not vnum or interval <= 0 or now < self.itemNext[i]:
                        continue
                    if self.justRevived and vnum in COURAGE_CAPE_VNUMS:
                        continue

                    
                    
                    self.itemNext[i] = now + max(ITEM_MIN_INTERVAL, interval)
                    cell = FindInventoryCell(vnum)
                    if cell >= 0 and not ItemAffectPresent(vnum, cell):
                        net.SendItemUsePacket(cell)
                        self.nextBuffGlobal = now + 0.1
                        break

    def AskForLoot(self, now):
        mask = LootMask(self.config)
        if not mask:
            self.lootVid = 0
            return
        if now < self.nextLootRequest or now < self.lootPausedUntil:
            return
        self.nextLootRequest = now + LOOT_REQUEST_INTERVAL
        (dx, dy) = self.AnchorOffset()
        self.Command('/autohunt_loot %d %d %d %d %d' % (
            self.config['range'], LootCoarseMask(self.config), dx, dy, mask), now)

    def RequestTarget(self, now):
        # MT2009_PLUS_AUTOHUNT_PRIORITY_V1 (Autor: blaki): "Najblizszy" asks
        # four times a second.
        if self.TargetMode() == TARGET_MODE_NEAREST:
            self.nextRequest = now + NEAREST_REQUEST_INTERVAL
        else:
            self.nextRequest = now + TARGET_REQUEST_INTERVAL
        self.lastTargetRequestAt = now
        (dx, dy) = self.AnchorOffset()
        command = '/autohunt_target %d %d %d %d %d %d' % (
            self.config['range'],
            1 if self.config.get('stones', 0) else 0,
            dx, dy,
            1 if self.config.get('mobs', 1) else 0,
            1 if self.config.get('bosses', 0) else 0)
        boxed = self.blockedVid and now < self.blockedUntil
        if boxed:
            skip = self.blockedVid
        elif self.skipVid and now < self.skipUntil:
            skip = self.skipVid
        else:
            skip = 0
        
        left = [vid for vid in self.BlockedTargetVids(now) if vid != skip]
        # MT2009_PLUS_AUTOHUNT_PRIORITY_V1 (Autor: blaki): every argument up
        # to the tenth, the order; the skip list ("0" for none) cut to what
        # the server's line holds. Boxed in, or "Fokus" falling back, the
        # nearest plain.
        nearest = boxed or now < self.forceNearestUntil
        order = 0 if nearest else self.PriorityOrder()
        command += ' %d %d' % (skip, 1 if nearest else 0)
        tail = ' %d' % order
        skips = ''
        for vid in left:
            piece = (',' if skips else '') + str(vid)
            if len(command) + 1 + len(skips) + len(piece) + len(tail) > TARGET_COMMAND_MAX:
                break
            skips += piece
        command += ' ' + (skips or '0') + tail
        self.Command(command, now)

    def ResetChaseMovement(self):
        self.chasePosition = None
        self.chaseDistance = 0
        self.chaseStillSince = 0.0
        self.chaseVid = 0

    def ResetEscape(self):
        
        
        self.walkHistory = []
        self.blockPosition = None
        self.blockAt = 0.0
        self.escapeUntil = 0.0
        self.escapeWaitUntil = 0.0
        self.escapeOrigin = None
        self.escapePoints = []
        self.escapeGoal = None
        self.escapeStepUntil = 0.0
        self.escapeProgressPosition = None
        self.escapeFailures = 0
        self.escapeHeading = (1.0, 0.0)
        self.escapeDetouring = False
        self.escapePurpose = 'combat'
        self.escapeTargetVid = 0
        self.nextEscapeTargetRequest = 0.0

    def RequestEscapeTarget(self, now):
        if not self.config.get('attack', 1) or self.justRevived or now < self.skillHoldUntil:
            return
        if now < self.nextEscapeTargetRequest or not self.CommandGapOk(now):
            return
        self.nextEscapeTargetRequest = now + ESCAPE_TARGET_INTERVAL
        self.nextRequest = max(self.nextRequest, now + TARGET_REQUEST_INTERVAL)
        
        self.Command('/autohunt_target 300 0 0 0 %d %d 0 1' % (
            1 if self.config.get('mobs', 1) else 0,
            1 if self.config.get('bosses', 0) else 0), now)

    def ResetNavigation(self):
        
        
        self.blockedTargets = {}
        self.targetBlockAttempts = {}
        self.returnTrail = []
        self.returning = False
        self.ResetReturnMovement()
        
        
        self.pathTriedAt = {}
        self.pathReturnRetryAt = 0.0
        self.nextPathRequest = 0.0
        self.ResetPath()

    def ResetPath(self):
        
        
        self.pathPoints = []
        self.pathPurpose = None
        self.pathVid = 0
        self.pathEnd = None
        self.pathMovePosition = None
        self.pathMoveSince = 0.0
        self.pathPending = None

    def ResetReturnMovement(self):
        self.returnGoal = None
        self.returnPosition = None
        self.returnStillSince = None

    def BlockedTargetVids(self, now):
        for vid, until in list(self.blockedTargets.items()):
            if now >= until:
                del self.blockedTargets[vid]
        return sorted(self.blockedTargets)

    def IsTargetBlocked(self, vid, now):
        until = self.blockedTargets.get(vid, 0.0)
        if now < until:
            return True
        if until:
            del self.blockedTargets[vid]
        return False

    def BlockTarget(self, vid, now):
        """Leaves a target for TARGET_BLOCK_DURATION; a full list lets go of
        the one nearest its end."""
        if not vid:
            return
        if vid not in self.blockedTargets and len(self.blockedTargets) >= TARGET_BLOCK_CAPACITY:
            oldest = min(self.blockedTargets, key=self.blockedTargets.get)
            del self.blockedTargets[oldest]
        self.blockedTargets[vid] = now + TARGET_BLOCK_DURATION
        self.targetBlockAttempts.pop(vid, None)

    def CountTargetBlock(self, vid, now):
        for oldVid, (count, stamp) in list(self.targetBlockAttempts.items()):
            if now - stamp > TARGET_BLOCK_WINDOW:
                del self.targetBlockAttempts[oldVid]
        (count, stamp) = self.targetBlockAttempts.get(vid, (0, now))
        count += 1
        self.targetBlockAttempts[vid] = (count, now)
        if count >= TARGET_BLOCK_LIMIT:
            self.BlockTarget(vid, now)

    def RecordReturnPosition(self, px, py):
        """One more point of the trail "Wracaj" walks back by; none while
        it is being walked back."""
        if self.returning:
            return
        if not self.returnTrail:
            (ax, ay) = self.anchor
            if (px - ax) ** 2 + (py - ay) ** 2 <= RETURN_TRAIL_REACH ** 2:
                self.returnTrail.append((ax, ay))
            else:
                self.returnTrail.append((px, py))
        (sx, sy) = self.returnTrail[-1]
        if (px - sx) ** 2 + (py - sy) ** 2 < RETURN_TRAIL_STEP ** 2:
            return
        
        
        for index in xrange(len(self.returnTrail) - 3, -1, -1):
            (sx, sy) = self.returnTrail[index]
            if (px - sx) ** 2 + (py - sy) ** 2 <= RETURN_TRAIL_LOOP ** 2:
                self.returnTrail = self.returnTrail[:index + 1]
                return
        if len(self.returnTrail) >= RETURN_TRAIL_LIMIT:
            self.returnTrail = [(px, py)]
        else:
            self.returnTrail.append((px, py))

    def RememberWalkPosition(self):
        (px, py, pz) = player.GetMainCharacterPosition()
        if self.walkHistory:
            (sx, sy) = self.walkHistory[-1]
            distance = (px - sx) ** 2 + (py - sy) ** 2
            if distance > RETURN_TRAIL_JUMP ** 2:
                self.ResetEscape()
                self.ResetNavigation()
            elif distance < RETURN_TRAIL_STEP ** 2:
                return
        self.walkHistory.append((px, py))
        self.walkHistory = self.walkHistory[-8:]
        self.RecordReturnPosition(px, py)

    def EscapeStepAllowed(self, x, y):
        
        if not self.config.get('return', 0):
            return True
        ax, ay = abs(x - self.anchor[0]), abs(y - self.anchor[1])
        return ax + ay - min(ax, ay) / 2.0 <= self.config['range']

    def BeginEscape(self, now, px, py, tx, ty, purpose='combat', vid=0):
        """Steps off what holds the hunter on its way to (tx, ty): back the
        way it came where it came from far enough, then half a turn, then
        to either side and back to either side, the side first changing
        with every failure."""
        self.escapePurpose = purpose
        self.escapeTargetVid = vid
        dx, dy = tx - px, ty - py
        length = math.sqrt(dx * dx + dy * dy)
        if length < 1.0:
            dx, dy, length = 1.0, 0.0, 1.0
        dx, dy = dx / length, dy / length
        self.escapeHeading = (dx, dy)
        self.escapeDetouring = False
        vectors = []
        for hx, hy in reversed(self.walkHistory):
            bx, by = hx - px, hy - py
            back = math.sqrt(bx * bx + by * by)
            if ESCAPE_CLEAR_DISTANCE <= back <= 400:
                vectors.append((bx / back, by / back, min(back, ESCAPE_STEP_DISTANCE)))
                break
        side = 1 if self.escapeFailures % 2 == 0 else -1
        for angle in (180, 90 * side, -90 * side, 135 * side, -135 * side):
            radians = math.radians(angle)
            vx = dx * math.cos(radians) - dy * math.sin(radians)
            vy = dx * math.sin(radians) + dy * math.cos(radians)
            vectors.append((vx, vy, ESCAPE_STEP_DISTANCE))
        self.escapePoints = []
        for vx, vy, length in vectors:
            x, y = int(px + vx * length), int(py + vy * length)
            if not self.EscapeStepAllowed(x, y):
                continue
            if (x - px) ** 2 + (y - py) ** 2 < ESCAPE_CLEAR_DISTANCE ** 2:
                continue
            if any((x - sx) ** 2 + (y - sy) ** 2 < 40 ** 2 for sx, sy in self.escapePoints):
                continue
            self.escapePoints.append((x, y))
        self.escapeOrigin = (px, py)
        self.escapeGoal = None
        self.escapeUntil = now + ESCAPE_MAX_SECONDS
        self.ResetChaseMovement()
        self.ResetPath()
        self.EscapeFrame(now)

    def FinishEscape(self, now, success):
        (px, py, pz) = player.GetMainCharacterPosition()
        self.WalkTo(px, py)
        self.escapeUntil = 0.0
        self.escapeGoal = None
        self.escapePoints = []
        self.ResetChaseMovement()
        self.approachSince = 0.0
        self.nextMove = 0.0
        self.nextRequest = 0.0
        self.ResetReturnMovement()
        if success:
            self.escapeFailures = 0
            self.blockPosition = None
            self.walkHistory = [(px, py)]
            if self.escapePurpose == 'combat':
                
                
                self.blockedVid = self.escapeTargetVid
                self.blockedUntil = now + COMBAT_SKIP_SECONDS
                self.blockedWaiting = True
        else:
            if self.escapePurpose == 'combat':
                self.BlockTarget(self.escapeTargetVid, now)
            self.escapeFailures += 1
            self.escapeWaitUntil = now + min(ESCAPE_RETRY_MAX_SECONDS,
                ESCAPE_RETRY_SECONDS * self.escapeFailures)

    def BeginEscapeDetour(self, now, px, py):
        """Off the wall: one more step across the way it was going, so the
        next walk does not run straight back into the same corner."""
        dx, dy = self.escapeHeading
        side = 1 if self.escapeFailures % 2 == 0 else -1
        self.escapePoints = []
        for sign in (side, -side):
            x = int(px - dy * sign * ESCAPE_STEP_DISTANCE)
            y = int(py + dx * sign * ESCAPE_STEP_DISTANCE)
            if not self.EscapeStepAllowed(x, y):
                continue
            (ox, oy) = self.escapeOrigin
            if (x - ox) ** 2 + (y - oy) ** 2 < ESCAPE_CLEAR_DISTANCE ** 2:
                continue
            self.escapePoints.append((x, y))
        self.escapeDetouring = True
        self.escapeGoal = None
        if not self.escapePoints:
            self.FinishEscape(now, True)

    def EscapeFrame(self, now):
        """True while a step off a wall takes the frame."""
        if self.escapePurpose == 'return':
            enabled = self.config.get('return', 0)
        else:
            enabled = self.config.get('attack', 1)
        if self.escapeUntil and not enabled:
            
            (px, py, pz) = player.GetMainCharacterPosition()
            self.WalkTo(px, py)
            self.ResetEscape()
            self.ResetReturnMovement()
            self.returning = False
            return False
        if self.escapeUntil or now < self.escapeWaitUntil:
            self.RequestEscapeTarget(now)
        if now < self.escapeWaitUntil:
            return True
        if not self.escapeUntil:
            return False
        self.ReleaseAttack()
        (px, py, pz) = player.GetMainCharacterPosition()
        (ox, oy) = self.escapeOrigin
        if (px - ox) ** 2 + (py - oy) ** 2 > RETURN_TRAIL_JUMP ** 2:
            
            self.ResetEscape()
            self.ResetNavigation()
            self.ResetChaseMovement()
            self.nextRequest = 0.0
            return True
        self.RecordReturnPosition(px, py)
        if now >= self.escapeUntil:
            self.FinishEscape(now, False)
            return True
        if self.escapeGoal is not None:
            (gx, gy) = self.escapeGoal
            if ((px - gx) ** 2 + (py - gy) ** 2 <= 40 ** 2 and
                    (px - ox) ** 2 + (py - oy) ** 2 >= ESCAPE_CLEAR_DISTANCE ** 2):
                if self.escapeDetouring:
                    self.FinishEscape(now, True)
                    return True
                self.BeginEscapeDetour(now, px, py)
                if not self.escapeUntil:
                    return True
                return self.EscapeFrame(now)
            (sx, sy) = self.escapeProgressPosition
            if (px - sx) ** 2 + (py - sy) ** 2 >= COMBAT_MOVE_THRESHOLD ** 2:
                self.escapeProgressPosition = (px, py)
                self.escapeStepUntil = now + ESCAPE_STEP_SECONDS
            if now < self.escapeStepUntil:
                return True
        if not self.escapePoints:
            self.FinishEscape(now, False)
            return True
        self.escapeGoal = self.escapePoints.pop(0)
        self.escapeProgressPosition = (px, py)
        self.escapeStepUntil = now + ESCAPE_STEP_SECONDS
        self.WalkTo(self.escapeGoal[0], self.escapeGoal[1])
        return True

    def NoteChaseWalk(self, now, vid, px, py, distance):
        """The walk's measure starts at the first step ordered to this
        target, not when it was named: a hunter that has not been told to
        walk yet has not been boxed in."""
        if self.chasePosition is None or self.chaseVid != vid:
            self.chasePosition = (px, py)
            self.chaseDistance = distance
            self.chaseStillSince = now
            self.chaseVid = vid

    def OnServerPath(self, seq, answer='', kind='0', points=''):
        """The server's "AutoHuntPath <seq> <answer> <kind> [<points>]"; seq
        0 only says the ground's kind (after the first target question a
        VID)."""
        try:
            seq = int(seq)
            kind = int(kind)
        except (TypeError, ValueError):
            return
        self.pathKind = kind if kind in (PATH_KIND_GRID, PATH_KIND_CORRIDORS) else PATH_KIND_NONE
        pending = self.pathPending
        if not seq or not pending or pending['seq'] != seq:
            return
        self.pathPending = None
        if not self.running:
            return
        self.ApplyPath(clientclock.Now(), pending, answer, points)

    def CheckPathPending(self, now):
        
        pending = self.pathPending
        if pending and now - pending['at'] >= PATH_ANSWER_SECONDS:
            self.pathPending = None
            self.ApplyPath(now, pending, 'wait', '')

    def PathAskable(self, vid, now):
        """Whether a target's way may be asked now: a map with a grid, no
        question in flight, the client's own interval, and not asked for this
        target within PATH_RETRY_SECONDS."""
        if self.pathKind == PATH_KIND_NONE or self.pathPending or now < self.nextPathRequest:
            return False
        asked = self.pathTriedAt.get(vid)
        return asked is None or now - asked >= PATH_RETRY_SECONDS

    def AskPath(self, now, purpose, vid, gx, gy, box=None):
        """/autohunt_path for the way to (gx, gy) - a monster's place, which
        the server reads itself by its VID, or the hunt's start."""
        if self.pathKind == PATH_KIND_NONE or self.pathPending or now < self.nextPathRequest:
            return False
        (px, py, pz) = player.GetMainCharacterPosition()
        self.pathSeq = self.pathSeq % 65535 + 1
        self.pathPending = {'seq': self.pathSeq, 'purpose': purpose, 'vid': vid,
            'origin': (int(px), int(py)), 'at': now, 'box': box}
        self.nextPathRequest = now + PATH_REQUEST_INTERVAL
        if purpose == 'target':
            for oldVid, asked in list(self.pathTriedAt.items()):
                if now - asked >= PATH_RETRY_SECONDS:
                    del self.pathTriedAt[oldVid]
            self.pathTriedAt[vid] = now
        self.Command('/autohunt_path %d %d %d %d' % (self.pathSeq, vid, int(gx - px), int(gy - py)), now)
        return True

    def ApplyPath(self, now, pending, answer, points):
        purpose = pending['purpose']
        vid = pending['vid']
        box = pending['box']
        if answer == 'ok':
            offsets = ParsePath(points)
            if offsets is None:
                answer = 'wait'
            elif purpose == 'target' and self.targetVid != vid:
                return
            elif purpose == 'return' and not self.returning:
                return
            else:
                (ox, oy) = pending['origin']
                self.pathPoints = [(ox + dx, oy + dy) for (dx, dy) in offsets]
                self.pathPurpose = purpose
                self.pathVid = vid
                self.pathEnd = self.pathPoints[-1]
                self.pathMovePosition = None
                self.ResetChaseMovement()
                self.ResetReturnMovement()
                self.approachSince = 0.0
                self.nextMove = 0.0
                return
        if purpose == 'target':
            if answer == 'none':
                
                self.BlockTarget(vid, now)
                if self.targetVid == vid:
                    self.ReleaseAttack()
                    if player.GetTargetVID() != 0:
                        player.ClearTarget()
                    self.targetVid = 0
                    self.approachSince = 0.0
                    self.ResetChaseMovement()
                    self.nextRequest = 0.0
            elif box and self.targetVid == vid:
                
                self.LetGoBoxed(now, vid, box)
            return
        
        
        self.pathReturnRetryAt = now + PATH_RETURN_RETRY_SECONDS
        if answer == 'direct':
            self.returnTrail = []
        if box and self.returning and now >= self.escapeWaitUntil:
            (px, py, gx, gy, repeated) = box
            self.BeginEscape(now, px, py, gx, gy, purpose='return')

    def WalkPath(self, now, distance=None):
        """One frame of the way: True while it is walked, False once it is
        done or held (and forgotten)."""
        (px, py, pz) = player.GetMainCharacterPosition()
        while self.pathPoints:
            (gx, gy) = self.pathPoints[0]
            if (px - gx) ** 2 + (py - gy) ** 2 > PATH_POINT_REACH ** 2:
                break
            self.pathPoints.pop(0)
        if not self.pathPoints:
            self.ResetPath()
            return False
        if now < self.skillHoldUntil:
            
            self.pathMovePosition = None
            return True
        if self.pathMovePosition is None:
            self.pathMovePosition = (px, py)
            self.pathMoveSince = now
        else:
            (sx, sy) = self.pathMovePosition
            if (px - sx) ** 2 + (py - sy) ** 2 >= COMBAT_MOVE_THRESHOLD ** 2:
                self.pathMovePosition = (px, py)
                self.pathMoveSince = now
            elif now - self.pathMoveSince >= COMBAT_STUCK_SECONDS:
                
                self.ResetPath()
                return False
        if now >= self.nextMove:
            self.nextMove = now + MOVE_INTERVAL
            self.WalkTo(self.pathPoints[0][0], self.pathPoints[0][1])
        
        self.approachSince = now
        if distance is not None:
            self.approachBest = distance
        return True

    def FollowTargetPath(self, now, vid, distance):
        if not self.pathPoints or self.pathPurpose != 'target' or self.pathVid != vid:
            return False
        if self.pathEnd is not None:
            (tx, ty, tz) = chr.GetPixelPosition(vid)
            (ex, ey) = self.pathEnd
            if (tx - ex) ** 2 + (ty - ey) ** 2 > PATH_DRIFT ** 2 and self.PathAskable(vid, now):
                
                self.AskPath(now, 'target', vid, tx, ty)
        return self.WalkPath(now, distance)

    def ChaseBlocked(self, now, vid, distance):
        """Whether the walk to a target beyond reach is boxed in (see
        COMBAT_STUCK_SECONDS); if it is, the target is left and the nearest
        asked for at once."""
        if now < self.skillHoldUntil:
            # The motion holds the step; the measure starts again after it.
            self.ResetChaseMovement()
            return False
        if self.chasePosition is None or self.chaseVid != vid:
            return False
        (px, py, pz) = player.GetMainCharacterPosition()
        if self.chaseDistance is None:
            
            
            self.chaseDistance = distance
        (sx, sy) = self.chasePosition
        moved = (px - sx) ** 2 + (py - sy) ** 2 >= COMBAT_MOVE_THRESHOLD ** 2
        if moved or distance <= self.chaseDistance - COMBAT_MOVE_THRESHOLD:
            self.chasePosition = (px, py)
            self.chaseDistance = distance
            self.chaseStillSince = now
            return False
        if now - self.chaseStillSince < COMBAT_STUCK_SECONDS:
            return False
        
        repeated = False
        if self.blockPosition is not None and now - self.blockAt <= ESCAPE_REPEAT_SECONDS:
            (bx, by) = self.blockPosition
            repeated = (px - bx) ** 2 + (py - by) ** 2 <= ESCAPE_REPEAT_DISTANCE ** 2
        self.blockPosition = (px, py)
        self.blockAt = now
        (tx, ty, tz) = chr.GetPixelPosition(vid)
        self.ReleaseAttack()
        # A stop where it stands: the walk ordered at the target would go on
        # into the pack the moment it gave way. No step back to the start.
        self.WalkTo(px, py)
        self.ResetChaseMovement()
        box = (px, py, tx, ty, repeated)
        if self.PathAskable(vid, now) and self.AskPath(now, 'target', vid, tx, ty, box):
            
            
            return True
        self.LetGoBoxed(now, vid, box)
        return True

    def LetGoBoxed(self, now, vid, box):
        """The box's answer when no way round is known: the target left for
        COMBAT_SKIP_SECONDS and the nearest monster asked for, or a step
        off the wall when it held the hunter where it held it last."""
        (px, py, tx, ty, repeated) = box
        self.CountTargetBlock(vid, now)
        self.ReleaseAttack()
        if player.GetTargetVID() != 0:
            player.ClearTarget()
        self.blockedVid = vid
        self.blockedUntil = now + COMBAT_SKIP_SECONDS
        self.blockedWaiting = True
        self.targetVid = 0
        self.approachSince = 0.0
        self.targetSetSince = 0.0
        self.targetMissFrames = 0
        self.nextMove = 0.0
        self.nextFace = 0.0
        self.ResetChaseMovement()
        if repeated and now >= self.escapeWaitUntil:
            
            self.BeginEscape(now, px, py, tx, ty, vid=vid)
        elif self.lastTargetRequestAt == now:
            
            
            self.nextRequest = 0.0
        else:
            self.RequestTarget(now)
        return True

    def Chase(self, now):
        self.PickNearLoot(now)
        if self.lootSweeping:
            self.returning = False
            self.ResetReturnMovement()
            self.ResetChaseMovement()
            if self.pathPoints:
                self.ResetPath()
            self.HandleLootSweep(now)
            return
        if self.config['attack']:
            if now >= self.nextRequest:
                self.RequestTarget(now)
        else:
            self.targetVid = 0
        vid = self.targetVid
        if vid and self.IsKnownDead(vid):
            if self.attacking:
                self.ReleaseAttack()
            if player.GetTargetVID() != 0:
                player.ClearTarget()
                
            self.skipVid = vid
            self.skipUntil = now + 5.0
            
            self.targetVid = 0
            self.nextRequest = 0
            # Allow immediate loot walking - the old skill's animation
            # hold must not delay the sweep after the target is gone.
            self.skillHoldUntil = 0.0
            self.targetMissFrames = 0
            self.targetSetSince = 0.0
            # The queue: whatever is down is picked up before the next
            # target is even asked for.
            self.TryStartLootSweep(now, force=True)
            return
        distance = player.GetCharacterDistance(vid) if vid else -1
        if distance < 0:
            self.ResetChaseMovement()
            if not vid and self.blockedWaiting and now < self.blockedUntil:
                # Boxed in and waiting for the nearest: the answer is a
                # monster at hand, so no walk back to the start meanwhile.
                self.nextRequest = min(self.nextRequest, now + 0.3)
                return
            # Grace period: the client may need a few frames to load a
            # mob the server just picked.  Clear only after five misses
            # so a mob on the edge of view is not thrown away at once.
            if vid and self.targetMissFrames < 5:
                self.targetMissFrames += 1
                return
            self.targetMissFrames = 0
            self.targetSetSince = 0.0
            self.targetVid = 0
            if self.pathPurpose == 'target':
                self.ResetPath()
            self.ReleaseAttack()
            if player.GetTargetVID() != 0:
                player.ClearTarget()
            # Quick retry instead of waiting for the full request interval.
            self.nextRequest = min(self.nextRequest, now + 0.3)
            if not self.TryStartLootSweep(now, force=True):
                self.ReturnToAnchor(now)
            return
        self.targetMissFrames = 0
        self.returning = False
        self.ResetReturnMovement()

        if self.pathPurpose == 'return':
            self.ResetPath()

        reach = self.Reach()
        if distance > reach:
            # MT2009_PLUS_AUTOHUNT_PRIORITY_V1: "Fokus" measures in reach only.
            self.focusLastProgress = now
            self.targetSetSince = 0.0
            self.ReleaseAttack()
            pending = self.pathPending
            if pending and pending['box'] and pending['purpose'] == 'target' and pending['vid'] == vid:
                
                return
            if self.FollowTargetPath(now, vid, distance):
                return
            if self.ChaseBlocked(now, vid, distance):
                return
            if not self.approachSince or distance < self.approachBest - WALK_PROGRESS:
                self.approachSince = now
                self.approachBest = distance
                if self.pathKind == PATH_KIND_CORRIDORS and self.PathAskable(vid, now):
                    
                    
                    (tx, ty, tz) = chr.GetPixelPosition(vid)
                    self.AskPath(now, 'target', vid, tx, ty)
            elif now - self.approachSince > PATH_STALL_SECONDS and self.PathAskable(vid, now):
                
                (tx, ty, tz) = chr.GetPixelPosition(vid)
                self.AskPath(now, 'target', vid, tx, ty)
            elif now - self.approachSince > STUCK_SECONDS:
                self.BlockTarget(vid, now)
                self.skipVid = vid
                self.skipUntil = now + STUCK_SKIP_SECONDS
                self.targetVid = 0
                self.approachSince = 0.0
                self.nextRequest = now + STUCK_PAUSE
                # Back to the start only when "Wracaj" asks for it.
                if self.config.get('return', 0):
                    self.ReturnToAnchor(now)
                return
            if now >= self.nextMove and now >= self.skillHoldUntil:
                self.nextMove = now + MOVE_INTERVAL
                (px, py, pz) = player.GetMainCharacterPosition()
                (tx, ty, tz) = chr.GetPixelPosition(vid)
                (sx, sy) = StopPoint(px, py, tx, ty, reach * STOP_SHORT_SHARE)
                self.WalkTo(sx, sy)
                self.NoteChaseWalk(now, vid, px, py, distance)
            return
            
        self.ResetChaseMovement()
        if self.pathPurpose == 'target':
            self.ResetPath()
        
        self.targetBlockAttempts.pop(vid, None)
        self.approachSince = 0.0
        # MT2009_PLUS_AUTOHUNT_PRIORITY_V1 (Autor: blaki): no fall of its
        # health for FOCUS_IDLE_SECONDS in reach - another target.
        if self.FocusIdle(now, vid):
            return
        if now >= self.nextFace:
            self.nextFace = now + FACE_INTERVAL
            self.Face(vid)
            
        current_target = player.GetTargetVID()
        if current_target != vid:
            if self.attacking:
                self.ReleaseAttack()
            if current_target != 0:
                player.ClearTarget()
            if vid != 0:
                player.SetTarget(vid)
                # For a bow character start auto-attack on the same frame
                # as SetTarget.  The melee path walks first which naturally
                # sets the target, but a bow stands still and the one-frame
                # gap between SetTarget and SetAttackKeyState can let a
                # stale client animation swallow the target selection.
                if self.Reach() > MELEE_REACH and not self.attacking:
                    player.SetAttackKeyState(True)
                    self.attacking = True
                # Track how long SetTarget has not registered.
                if not self.targetSetSince:
                    self.targetSetSince = now
                elif now - self.targetSetSince > 1.0:
                    # SetTarget still ignored after a second - walk a short
                    # step toward the mob to nudge the client out of any
                    # lingering skill-reserved mode.
                    if now >= self.nextMove:
                        self.nextMove = now + MOVE_INTERVAL
                        (px, py, pz) = player.GetMainCharacterPosition()
                        (tx, ty, tz) = chr.GetPixelPosition(vid)
                        dx = tx - px
                        dy = ty - py
                        d = math.sqrt(dx * dx + dy * dy)
                        if d > 0:
                            step = min(300.0, d)
                            self.WalkTo(px + dx * step / d, py + dy * step / d)
                        self.targetSetSince = now
        else:
            self.targetSetSince = 0.0
            if not self.attacking:
                player.SetAttackKeyState(True)
                self.attacking = True

    def TryStartLootSweep(self, now, force=False):
        """Enters the queue's second half: whatever the ground still owes is
        collected before the next target is even asked for. Only a target
        just settled - dead, or gone from the map entirely - opens it
        (force=True); a target still being fought, or merely out of reach
        and being walked to, is never detoured from."""
        if self.lootSweeping:
            return True
        if not LootMask(self.config):
            return False
        if not self.lootVid:
            return False
        if not force and self.LootDistance() > LOOT_FIRST_DISTANCE:
            return False

        self.ReleaseAttack()
        if player.GetTargetVID() != 0:
            player.ClearTarget()
        self.targetVid = 0
        self.lootSweeping = True
        self.lootSweepIdleSince = 0.0
        self.lootSince = 0.0
        self.lootBest = 0.0
        self.nextMove = 0.0
        return True

    def HandleLootSweep(self, now):
        """The queue stays on loot until AskForLoot's own clock has had a
        clear turn to say nothing more is owed (LOOT_SWEEP_END_GRACE) - not
        the instant a single pick-up empties self.lootVid, which a request
        already on its way back would otherwise refill a moment later."""
        if not LootMask(self.config):
            self.lootSweeping = False
            self.lootSweepIdleSince = 0.0
            self.nextRequest = 0
            return

        if self.lootVid:
            self.lootSweepIdleSince = 0.0
            self.GoForLoot(now)
            return

        if not self.lootSweepIdleSince:
            self.lootSweepIdleSince = now
            return

        if now - self.lootSweepIdleSince >= LOOT_SWEEP_END_GRACE:
            self.lootSweeping = False
            self.lootSweepIdleSince = 0.0
            self.lootPickAttempts = {}
            self.nextRequest = 0

    def CastSkills(self, now, buffsOnly=False):
        for index, slot, skillIndex in self.SkillCandidates(now, buffsOnly):
            
            if IsMounted() and skillIndex not in HORSE_SKILLS:
                continue
            player.ClickSkillSlot(slot)
            self.skillNext[index] = now + max(SKILL_MIN_INTERVAL, float(self.config['skill%d_interval' % index]))
            if NeedsTarget(skillIndex):
                self.skillHoldUntil = now + SKILL_MOTION_HOLD
            return

    def SkillCandidates(self, now, buffsOnly=False):
        if not self.config['use_skills']:
            return

        fightDistance = None
        for index in xrange(SKILL_SLOTS):
            slot = self.config['skill%d_slot' % index]
            if not slot or now < self.skillNext[index]:
                continue
            skillIndex = player.GetSkillIndex(slot)
            if not skillIndex or player.IsSkillCoolTime(slot):
                continue
            if skill.IsToggleSkill(skillIndex) and player.IsSkillActive(slot):
                continue
            if skillIndex in BUFF_SKILLS and self.BuffPresent(slot, skillIndex):
                continue
            if buffsOnly and skillIndex not in BUFF_SKILLS:
                continue
            needsTarget = NeedsTarget(skillIndex)
            if needsTarget:
                # The slot's clock is left alone, so the skill goes on the
                # first frame the fight is there, and a buff further down
                # still goes on this one.
                if fightDistance is None:
                    fightDistance = self.FightDistance()
                limit = MELEE_REACH if skillIndex in WARP_SKILLS else self.Reach()
                if fightDistance < 0 or fightDistance > limit:
                    continue
            elif skillIndex not in BUFF_SKILLS and InTargetSkillRange(skillIndex):
                # A standing combat skill (Arrow Shower, Dragon's Roar and
                # the like): cast only while the hunter's own target is alive
                # and in the client's hand.  Without this the skill fires
                # between groups, its animation blocks player.SetTarget for
                # the next mob, and a bow Ninja freezes until a targeted
                # skill's cooldown expires and ClickSkillSlot selects the
                # target through the client's own mechanism.  A guild's buff
                # is no class's combat skill and goes between fights as
                # before.
                isToggle = False
                try:
                    isToggle = skill.IsToggleSkill(skillIndex)
                except Exception:
                    pass
                if not isToggle:
                    if fightDistance is None:
                        fightDistance = self.FightDistance()
                    if fightDistance < 0:
                        continue
            yield (index, slot, skillIndex)

    def FightDistance(self):
        """How far stands the monster a skill would be cast at, or -1 when the
        client would have to find one itself: the hunter's own target, alive,
        and in the client's hand - Chase marks it only within reach, and a
        mark not taken yet or held by a corpse sends the client to the mouse
        cursor. With the attack switched off nothing is fought (and Chase
        clears whatever the client holds), so nothing is cast at an enemy."""
        vid = self.targetVid if self.config['attack'] else 0
        if not vid or player.GetTargetVID() != vid:
            return -1
        if self.IsKnownDead(vid):
            return -1
        return player.GetCharacterDistance(vid)

    def PickNearLoot(self, now):
        if not self.lootVid or now < self.nextLootPick:
            return False
        if self.LootDistance() > LOOT_PICK_DISTANCE:
            return False
        vid = self.lootVid
        attempts = self.lootPickAttempts.get(vid, 0) + 1
        self.lootPickAttempts[vid] = attempts
        if attempts > LOOT_MAX_PICK_RETRIES:
            self.lootSkippedVids[vid] = now + LOOT_SKIP_DURATION
            self.lootPickAttempts.pop(vid, None)
            self.lootVid = 0
            self.lootSince = 0.0
            return False
        self.nextLootPick = now + LOOT_PICK_INTERVAL
        net.SendItemPickUpPacket(vid)
        self.lootVid = 0
        self.lootSince = 0.0
        self.nextLootRequest = min(self.nextLootRequest, now + 0.3)
        return True

    def GoForLoot(self, now):
        if not self.lootVid:
            return False
        dist = self.LootDistance()
        if dist <= LOOT_PICK_DISTANCE:
            return True
        if not self.lootSince or dist < self.lootBest - WALK_PROGRESS:
            self.lootSince = now
            self.lootBest = dist
        elif now - self.lootSince > LOOT_STUCK_SECONDS:
            self.lootVid = 0
            self.lootSince = 0.0
            self.lootPausedUntil = now + LOOT_STUCK_PAUSE
            return False
        if now >= self.nextMove and now >= self.skillHoldUntil:
            self.nextMove = now + MOVE_INTERVAL
            self.WalkTo(self.lootPos[0], self.lootPos[1])
        return True

    def Reach(self):
        if hasattr(player, 'IsBowEquipped'):
            if player.IsBowEquipped():
                return ARCHER_REACH
            return MELEE_REACH
        # An exe older than client 2.0.25 cannot say what is in the hand: a
        # Ninja of the archery school is taken to hold its bow.
        if net.GetMainActorRace() % 4 == 1 and net.GetMainActorSkillGroup() == 2:
            return ARCHER_REACH
        return MELEE_REACH

    def AnchorOffset(self):
        if self.config.get('return', 0):
            (px, py, pz) = player.GetMainCharacterPosition()
            return (self.anchor[0] - int(px), self.anchor[1] - int(py))
        else:
            return (0, 0)

    def LootDistance(self):
        (px, py, pz) = player.GetMainCharacterPosition()
        (lx, ly) = self.lootPos
        return math.sqrt((px - lx) * (px - lx) + (py - ly) * (py - ly))

    def ReturnToAnchor(self, now):
        """The walk back to the start with "Wracaj": the trail's points in
        reverse (see RETURN_TRAIL_LIMIT), the start itself once the trail is
        walked, and a step off what holds the walk (Colide)."""
        if not self.config['return']:
            self.returning = False
            self.ResetReturnMovement()
            return
        if now < self.skillHoldUntil:
            self.ResetReturnMovement()
            return
        (px, py, pz) = player.GetMainCharacterPosition()
        (ax, ay) = self.anchor
        if (px - ax) * (px - ax) + (py - ay) * (py - ay) <= ANCHOR_LEASH * ANCHOR_LEASH:
            self.returning = False
            self.ResetReturnMovement()
            if self.pathPurpose == 'return':
                self.ResetPath()
            return
        self.returning = True
        if self.pathPurpose == 'return' and self.pathPoints:
            if self.WalkPath(now):
                return
            
            self.pathReturnRetryAt = now + PATH_RETURN_RETRY_SECONDS
            self.ResetReturnMovement()
        pending = self.pathPending
        if pending and pending['purpose'] == 'return' and pending['box']:
            
            return
        if (self.pathKind == PATH_KIND_CORRIDORS and not pending and
                now >= self.pathReturnRetryAt and self.returnPosition is None):
            
            
            self.pathReturnRetryAt = now + PATH_RETURN_RETRY_SECONDS
            self.AskPath(now, 'return', 0, ax, ay)
        mounted = IsMounted()
        trailReach = RETURN_MOUNTED_TRAIL_REACH if mounted else RETURN_TRAIL_REACH
        stuckSeconds = RETURN_MOUNTED_STUCK_SECONDS if mounted else COMBAT_STUCK_SECONDS
        while len(self.returnTrail) > 1:
            gx, gy = self.returnTrail[-1]
            if (px - gx) ** 2 + (py - gy) ** 2 > trailReach ** 2:
                break
            self.returnTrail.pop()
        goal = self.returnTrail[-1] if len(self.returnTrail) > 1 else (ax, ay)
        if self.returnGoal != goal:
            self.ResetReturnMovement()
            self.returnGoal = goal
            self.nextMove = 0.0
        if self.returnPosition is not None:
            (sx, sy) = self.returnPosition
            if (px - sx) ** 2 + (py - sy) ** 2 >= COMBAT_MOVE_THRESHOLD ** 2:
                self.returnPosition = (px, py)
                self.returnStillSince = now
            elif now - self.returnStillSince >= stuckSeconds and now >= self.escapeWaitUntil:
                self.ReleaseAttack()
                self.WalkTo(px, py)
                if player.GetTargetVID() != 0:
                    player.ClearTarget()
                if (self.pathKind != PATH_KIND_NONE and now >= self.pathReturnRetryAt and
                        self.AskPath(now, 'return', 0, ax, ay, (px, py, goal[0], goal[1], False))):
                    self.pathReturnRetryAt = now + PATH_RETURN_RETRY_SECONDS
                    self.ResetReturnMovement()
                    return
                self.BeginEscape(now, px, py, goal[0], goal[1], purpose='return')
                return
        if now >= self.nextMove:
            self.nextMove = now + RETURN_MOVE_INTERVAL
            self.WalkTo(goal[0], goal[1])
            if self.returnPosition is None:
                self.returnPosition = (px, py)
                self.returnStillSince = now

    def WalkTo(self, x, y):
        # MT2009_PLUS_AUTOHUNT_MOUNT_V1: a step ordered somewhere (not a stop
        # where it stands) is a walk WatchMountStuck measures.
        (px, py, pz) = player.GetMainCharacterPosition()
        if (px - x) ** 2 + (py - y) ** 2 >= (2 * COMBAT_MOVE_THRESHOLD) ** 2:
            self.walkIntentAt = clientclock.Now()
        chr.MoveToDestPosition(player.GetMainCharacterIndex(), int(x), int(y))

    def Face(self, vid):
        (px, py, pz) = player.GetMainCharacterPosition()
        (tx, ty, tz) = chr.GetPixelPosition(vid)
        chr.SelectInstance(player.GetMainCharacterIndex())
        chr.SetRotation(FacingDegree(px, py, tx, ty))

    def ReleaseAttack(self):
        if self.attacking:
            player.SetAttackKeyState(False)
            self.attacking = False

    def Command(self, text, now=None):
        """A chat command to the server, on the hunt's command clock."""
        net.SendChatPacket(text)
        self.lastCommandAt = clientclock.Now() if now is None else now

    def CommandGapOk(self, now):
        return now - self.lastCommandAt >= COMMAND_GAP

    # -- The rider (the section above Hunter says what and why) ------------

    def RiderReach(self):
        """How near a monster the rider strikes: a bow's reach, or a swing
        from the saddle."""
        if self.Reach() > MELEE_REACH:
            return ARCHER_REACH
        return RIDER_REACH

    def RiderRange(self):
        """How far round the rider the server is asked for a monster, never
        more than the window's own range."""
        reach = ARCHER_REACH if self.Reach() > MELEE_REACH else RIDER_RANGE
        return max(300, min(self.config.get('range', 2000), reach))

    def RiderActive(self):
        """Whether the rider has the hunt: in the saddle of a battle horse
        with the switch on, or in a step of its own off the horse."""
        if not self.running or self.riderBroken:
            return False
        if self.riderPhase != RIDE:
            return True
        return bool(self.config.get('rider', 0)) and IsMounted() and HorseLevel() >= RIDER_HORSE_LEVEL

    def RiderFrame(self, now):
        """The rider's part of the frame; False leaves it to the hunt on foot.
        Whatever goes wrong in it, the game window's frame must not: one line
        in syserr.txt, the character back on its horse when the rider had
        taken it down, and the hunt goes on on foot until the client restarts."""
        if self.riderBroken:
            return False
        try:
            return self.RiderUpdate(now)
        except Exception:
            self.RiderBreak(now)
            return False

    def RiderBreak(self, now):
        self.riderBroken = True
        try:
            import dbg
            import traceback
            dbg.TraceError('uiautohunt rider: %s' % traceback.format_exc())
        except Exception:
            pass
        try:
            self.ReleaseAttack()
            if self.riderPhase in (CAST, MOUNT) and not IsMounted():
                self.Command('/ride', now)
        except Exception:
            pass
        self.riderPhase = RIDE

    def RiderSwitched(self):
        """The switch was set, or a hunt starts with it: the character is to
        ride its battle horse, and the chat says where that stands."""
        on = bool(self.config.get('rider', 0))
        self.riderSaddle = on
        self.riderNeedsSummon = on and not IsMounted()
        self.riderNextMount = 0.0
        self.riderMountTries = 0
        self.riderSaidRetry = False
        if not self.running:
            return
        if not on:
            chat.AppendChat(chat.CHAT_TYPE_INFO, T('Auto \xa3owy: bojowiec wy\xb3\xb9czony.',
                'Auto Hunt: battle horse mode off.'))
            return
        self.WatchAffects()
        if HorseLevel() < RIDER_HORSE_LEVEL:
            chat.AppendChat(chat.CHAT_TYPE_INFO, T('Auto \xa3owy: bojowiec potrzebuje konia od %d poziomu - na razie \xb3owy pieszo.',
                'Auto Hunt: the battle horse mode needs a horse of level %d or more - hunting on foot for now.') % RIDER_HORSE_LEVEL)
        elif IsMounted():
            chat.AppendChat(chat.CHAT_TYPE_INFO, T('Auto \xa3owy: bojowiec - walka w miejscu z siod\xb3a.',
                'Auto Hunt: battle horse mode - fighting in place from the saddle.'))
        else:
            chat.AppendChat(chat.CHAT_TYPE_INFO, T('Auto \xa3owy: bojowiec - wsiadam na konia.',
                'Auto Hunt: battle horse mode - getting on the horse.'))

    def RiderUpdate(self, now):
        rider = bool(self.config.get('rider', 0))
        if not rider and self.riderPhase == RIDE:
            self.riderWasMounted = False
            self.riderFootSince = 0.0
            self.RiderConfirmCasts(now)
            if IsMounted():
                self.mountedSkillCycle = False
                self.riderSaddle = False
                self.riderMountTries = 0
                if now >= self.riderNextWatch:
                    self.riderNextWatch = now + RIDER_WATCH_INTERVAL
                    self.WatchAffects()
                self.RiderTrackHealth(now)
                if self.RiderMaybeClimbDown(now, ordinary=True):
                    self.mountedSkillCycle = True
                    self.riderSaddle = True
                    return True
            elif self.mountedSkillCycle and self.riderSaddle and now >= self.riderNextMount:
                self.RiderBeginMount(now, quick=self.riderMountTries == 0)
                return True
            return False
        if now >= self.riderNextWatch:
            self.riderNextWatch = now + RIDER_WATCH_INTERVAL
            self.WatchAffects()
        self.RiderTrackHealth(now)
        self.RiderConfirmCasts(now)
        if self.riderPhase != RIDE:
            self.RiderStep(now, rider or self.mountedSkillCycle)
            return True
        if IsMounted():
            self.riderFootSince = 0.0
            if HorseLevel() < RIDER_HORSE_LEVEL:
                self.riderWasMounted = False
                return False
            if not self.riderWasMounted:
                # Into the saddle from the hunt on foot: its sweep of the
                # drops is its own. A target it was walking to is let go by
                # the rider's own rule if it stays out of reach.
                self.lootSweeping = False
                self.lootSweepIdleSince = 0.0
            self.riderWasMounted = True
            self.riderSaddle = True
            self.riderMountTries = 0
            self.RiderFight(now)
            return True
        if self.riderWasMounted:
            # Off the horse, and not by the rider's own climb-down: the
            # player's Ctrl+G, a horse too tired to carry anybody, or a death
            # whose health reads 0 a moment later - which WhileDead tells
            # apart from the other two.
            if not self.riderFootSince:
                self.riderFootSince = now
                self.ReleaseAttack()
            if now - self.riderFootSince < RIDER_FOOT_GRACE:
                return True
            self.riderWasMounted = False
            self.riderSaddle = False
            self.riderFootSince = 0.0
            chat.AppendChat(chat.CHAT_TYPE_INFO, T('Auto \xa3owy: posta\xe6 zesz\xb3a z konia - \xb3owy dalej pieszo. Wsi\xb9d\x9f (Ctrl+G), a bojowiec wr\xf3ci.',
                'Auto Hunt: the character got off the horse - hunting on foot. Get back on (Ctrl+G) and the battle horse mode comes back.'))
            return False
        if self.riderSaddle and now >= self.riderNextMount and HorseLevel() >= RIDER_HORSE_LEVEL:
            self.RiderBeginMount(now, quick=self.riderMountTries == 0)
            return True
        return False

    def RiderTrackHealth(self, now):
        hp = player.GetStatus(player.HP)
        if self.riderLastHp is not None and hp < self.riderLastHp:
            self.riderHpDropAt = now
        self.riderLastHp = hp

    def RiderHealthShare(self):
        maxHP = player.GetStatus(player.MAX_HP)
        if maxHP <= 0:
            return 100
        return player.GetStatus(player.HP) * 100 // maxHP

    def RiderQuiet(self, now):
        """Nothing is at the rider: no monster in its reach in hand, and its
        health has not dropped for RIDER_QUIET_SECONDS."""
        if now - self.riderHpDropAt < RIDER_QUIET_SECONDS:
            return False
        vid = self.targetVid
        if not vid or (hasattr(player, 'IsTargetDead') and player.IsTargetDead(vid)):
            return True
        distance = player.GetCharacterDistance(vid)
        return distance < 0 or distance > self.RiderReach()

    def RiderFight(self, now):
        self.RiderHorseSkills(now)
        self.RiderLoot(now)
        if self.RiderMaybeClimbDown(now):
            return
        self.RiderTarget(now)

    def RiderTarget(self, now):
        """The space bar held, in place: the server names what to face - what
        hits the rider first, then the nearest - within the rider's range,
        and the swing goes once it is within reach. Nothing is walked to."""
        if not self.config['attack']:
            self.targetVid = 0
            self.ReleaseAttack()
            return
        if now >= self.nextRequest:
            self.nextRequest = now + TARGET_REQUEST_INTERVAL
            command = '/autohunt_target %d %d 0 0 %d %d' % (
                self.RiderRange(),
                1 if self.config.get('stones', 0) else 0,
                1 if self.config.get('mobs', 1) else 0,
                1 if self.config.get('bosses', 0) else 0)
            if self.skipVid and now < self.skipUntil:
                command += ' %d' % self.skipVid
            self.Command(command, now)
        vid = self.targetVid
        if not vid:
            self.ReleaseAttack()
            self.nextRequest = min(self.nextRequest, now + 0.3)
            return
        if self.IsKnownDead(vid):
            self.RiderDropTarget(now, vid, 5.0)
            return
        distance = player.GetCharacterDistance(vid)
        if distance < 0:
            # The client may need a few frames to show what the server named.
            if self.targetMissFrames < 5:
                self.targetMissFrames += 1
                return
            self.RiderDropTarget(now, 0, 0.0)
            return
        self.targetMissFrames = 0
        if now >= self.nextFace:
            self.nextFace = now + FACE_INTERVAL
            self.Face(vid)
        if distance > self.RiderReach():
            # Coming, or not: a monster crosses the gap in a second or two,
            # and a stone or one that stays away is let go for another.
            self.ReleaseAttack()
            if not self.riderStaleSince:
                self.riderStaleSince = now
            elif now - self.riderStaleSince > RIDER_STALE_SECONDS:
                self.RiderDropTarget(now, vid, STUCK_SKIP_SECONDS)
            return
        self.riderStaleSince = 0.0
        # The mark is tried a few times a second, not every frame: the client
        # refuses one off the screen (CanPickInstance) by sending the server
        # a target of 0, which on every frame would be sixty packets a second.
        if player.GetTargetVID() != vid and now >= self.riderNextMark:
            self.riderNextMark = now + RIDER_MARK_INTERVAL
            player.SetTarget(vid)
        if not self.attacking:
            player.SetAttackKeyState(True)
            self.attacking = True

    def RiderDropTarget(self, now, skipVid, seconds):
        self.ReleaseAttack()
        if self.targetVid and player.GetTargetVID() == self.targetVid:
            player.ClearTarget()
        if skipVid:
            self.skipVid = skipVid
            self.skipUntil = now + seconds
        self.targetVid = 0
        self.riderStaleSince = 0.0
        self.targetMissFrames = 0
        self.nextRequest = 0.0

    def RiderOnServerTarget(self, value):
        """Accept a nearby replacement during an ordinary skill cycle."""
        if self.mountedSkillCycle and self.riderPhase == CAST:
            vid = ParseTargetVid(value)
            if not vid or not self.config['attack'] or self.justRevived:
                return
            if not hasattr(player, 'IsTargetDead') or player.IsTargetDead(vid):
                return
            distance = player.GetCharacterDistance(vid)
            if 0 <= distance <= min(MELEE_REACH, self.Reach()):
                self.targetVid = vid
                self.riderNextSkillTarget = 0.0
                self.riderTargetSearchUntil = None
            return
        if self.riderPhase != RIDE:
            return
        newVid = ParseTargetVid(value)
        if not newVid or newVid == self.targetVid:
            return
        vid = self.targetVid
        if vid and not (hasattr(player, 'IsTargetDead') and player.IsTargetDead(vid)):
            distance = player.GetCharacterDistance(vid)
            if 0 <= distance <= self.RiderReach():
                return
        self.targetVid = newVid
        self.riderStaleSince = 0.0
        self.riderNextMark = 0.0
        self.targetMissFrames = 0
        self.nextFace = 0.0

    def RiderHorseSkills(self, now):
        """The horse's own skills, the only ones a saddle casts, and only a
        military horse's: at the monster in hand and within reach, as a
        skill of a class is cast on foot (NeedsTarget)."""
        if not self.config['use_skills'] or HorseLevel() < RIDER_SKILL_HORSE_LEVEL:
            return
        fightDistance = None
        for index in xrange(SKILL_SLOTS):
            slot = self.config['skill%d_slot' % index]
            if not slot or now < self.skillNext[index]:
                continue
            skillIndex = player.GetSkillIndex(slot)
            if skillIndex not in HORSE_SKILLS or player.IsSkillCoolTime(slot):
                continue
            if SkillLevelOf(slot) <= 0:
                continue
            if fightDistance is None:
                fightDistance = self.FightDistance()
            if fightDistance < 0 or fightDistance > self.RiderReach():
                continue
            player.ClickSkillSlot(slot)
            self.skillNext[index] = now + max(SKILL_MIN_INTERVAL, float(self.config['skill%d_interval' % index]))
            return

    def RiderLoot(self, now):
        """The drop round the rider, by the window's kinds, without a step:
        "/autohunt_loot" names the nearest wanted item within the server's
        pick-up reach, and the ` key's batch takes every one there."""
        mask = LootMask(self.config)
        if not mask:
            self.lootVid = 0
            return
        self.RiderPick(now, mask)
        if now >= self.nextLootRequest:
            self.nextLootRequest = now + LOOT_REQUEST_INTERVAL
            self.Command('/autohunt_loot %d %d 0 0 %d' % (
                RIDER_LOOT_RANGE, LootCoarseMask(self.config), mask), now)

    def RiderPick(self, now, mask):
        """The batch, when the server has named a wanted item within reach;
        the next question comes a moment after it, not in the same frame."""
        vid = self.lootVid
        if not vid or now < self.nextLootPick or self.LootDistance() > RIDER_PICK_DISTANCE:
            return
        if not self.CommandGapOk(now):
            return
        # A drop it cannot take - a full bag - is named again and again: three
        # tries, and it is let be for LOOT_SKIP_DURATION, as on foot.
        if len(self.lootPickAttempts) > 32:
            self.lootPickAttempts = {}
        attempts = self.lootPickAttempts.get(vid, 0) + 1
        self.lootPickAttempts[vid] = attempts
        if attempts > LOOT_MAX_PICK_RETRIES:
            self.lootSkippedVids[vid] = now + LOOT_SKIP_DURATION
            self.lootPickAttempts.pop(vid, None)
            self.lootVid = 0
            return
        self.nextLootPick = now + RIDER_PICK_INTERVAL
        self.Command('/pickup_nearby %d' % mask, now)
        self.lootVid = 0
        self.nextLootRequest = now + 0.3

    def RiderMaybeClimbDown(self, now, ordinary=False):
        """A buff of the window's slots is off or nearly spent, can be cast
        now, and the moment allows it: the climb-down begins."""
        if not self.config['use_skills'] or now < self.riderNextBuffCheck or now < self.riderNextCycle:
            return False
        self.riderNextBuffCheck = now + RIDER_BUFF_CHECK
        due = self.MountedFootSkills(now, RIDER_RENEW_BEFORE) if ordinary else self.RiderBuffs(now, RIDER_RENEW_BEFORE)
        if not due:
            return False
        if self.RiderHealthShare() < RIDER_SAFE_HP and not self.RiderQuiet(now):
            return False
        self.ReleaseAttack()
        if ordinary:
            px, py, pz = player.GetMainCharacterPosition()
            self.WalkTo(px, py)
        self.riderTried = set()
        self.riderDismountTries = 0
        self.riderAttackPending = None
        self.riderAttackAttempts = {}
        self.riderPhase = DISMOUNT
        self.riderPhaseUntil = now + HumanPause(RIDER_PAUSE_DISMOUNT)
        return True

    def MountedFootSkills(self, now, margin):
        """Normal hunting skills, with the rider's buff renewal safeguards."""
        buffs = dict((index, (index, slot, skillIndex)) for index, slot, skillIndex in self.RiderBuffs(now, margin))
        found = []
        for index, slot, skillIndex in self.SkillCandidates(now):
            if skillIndex in HORSE_SKILLS or index in self.riderTried:
                continue
            if self.riderRefused.get(index, 0.0) > now or not CanCastNow(slot, skillIndex):
                continue
            if skillIndex in RIDER_BUFFS and index not in buffs:
                continue
            found.append((index, slot, skillIndex))
        return found

    def RiderBuffs(self, now, margin):
        """The rider's buffs in the window's slots that want casting now, in
        the slots' order: one of RIDER_BUFFS, off its slot's own clock, not
        cast in this climb-down nor waiting for its buff nor refused lately,
        absent from the server's affect list and castable (CanCastNow).
        The duration margin is only a fallback for older roots without it.
        """
        found = []
        for index in xrange(SKILL_SLOTS):
            slot = self.config['skill%d_slot' % index]
            if not slot or now < self.skillNext[index] or index in self.riderTried:
                continue
            if self.riderRefused.get(index, 0.0) > now:
                continue
            try:
                skillIndex = player.GetSkillIndex(slot)
            except Exception:
                continue
            if skillIndex not in RIDER_BUFFS or skillIndex in self.riderPending:
                continue
            if self.BuffPresent(slot, skillIndex):
                continue
            left = self.BuffRemaining(slot, skillIndex, now)
            if left is None or left > margin:
                continue
            if not CanCastNow(slot, skillIndex):
                continue
            found.append((index, slot, skillIndex))
        return found

    def BuffRemaining(self, slot, skillIndex, now):
        """Seconds left of a buff: 0 when it is off, None when it is on and
        the client cannot say for how long - one put up before this client
        heard of it - and a toggle has no clock."""
        if IsToggle(skillIndex):
            return None if self.BuffPresent(slot, skillIndex) else 0.0
        affects = AffectDict()
        if affects is not None and skillIndex not in affects:
            return 0.0
        stamp = self.riderBuffStamp.get(skillIndex)
        if stamp is None:
            # With no list of the affects, one the rider never cast is off.
            return None if affects is not None else 0.0
        (at, seconds) = stamp
        if seconds <= 0:
            return None
        return max(0.0, seconds - (now - at))

    def BuffPresent(self, slot, skillIndex):
        if IsToggle(skillIndex) and self.SkillActive(slot):
            return True
        affects = AffectDict()
        return affects is not None and skillIndex in affects

    def SkillActive(self, slot):
        try:
            return bool(player.IsSkillActive(slot))
        except Exception:
            return False

    def SkillCooling(self, slot):
        try:
            return bool(player.IsSkillCoolTime(slot))
        except Exception:
            return False

    def WatchAffects(self):
        """Hears of every affect the server puts on the character (game.py's
        BINARY_NEW_AddAffect tells the root's eventManager), so a buff's time
        is known whoever cast it. The game window's close drops every
        observer (unregister_all_events), so this is asked again now and
        then; the same observer added twice is one."""
        manager = sys.modules.get('eventManager') or sys.modules.get('eventmanager')
        if manager is None:
            return
        try:
            manager.EventManager().add_observer(manager.ADD_AFFECT_EVENT, self.OnAffectAdded)
        except Exception:
            pass

    def OnAffectAdded(self, affectType, pointIdx=0, value=0, duration=0, *rest):
        # Called from the game window's packet handler: nothing may escape.
        try:
            seconds = int(duration)
            if seconds > 0:
                self.riderBuffStamp[int(affectType)] = (clientclock.Now(), seconds)
        except Exception:
            pass

    def RiderConfirmCasts(self, now):
        """A cast is a buff once the buff is there: put up after the cast
        (the server's word, WatchAffects), on after being off, a toggle
        switched on - or, renewing one that was on, once the client sent it
        (its cooldown runs). Nothing within RIDER_CONFIRM_SECONDS was refused."""
        if not self.riderPending:
            return
        affects = AffectDict()
        for skillIndex in list(self.riderPending.keys()):
            (index, slot, at, before) = self.riderPending[skillIndex]
            stamp = self.riderBuffStamp.get(skillIndex)
            confirmed = False
            if stamp is not None and stamp[0] >= at:
                confirmed = True
            elif IsToggle(skillIndex):
                confirmed = self.SkillActive(slot)
            elif affects is not None and not before:
                if skillIndex in affects:
                    confirmed = True
                    self.riderBuffStamp[skillIndex] = (at, AffectDuration(affects[skillIndex]))
            elif self.SkillCooling(slot):
                confirmed = True
                if affects is not None and skillIndex in affects:
                    seconds = AffectDuration(affects[skillIndex])
                else:
                    seconds = BuffSeconds(slot, skillIndex)
                self.riderBuffStamp[skillIndex] = (at, seconds)
            if confirmed:
                del self.riderPending[skillIndex]
            elif now - at >= RIDER_CONFIRM_SECONDS:
                del self.riderPending[skillIndex]
                self.riderRefused[index] = now + RIDER_REFUSED_SKIP

    def RiderCast(self, now, index, slot, skillIndex):
        self.riderNextCast = now + RIDER_CAST_FALLBACK
        offensive = skillIndex not in BUFF_SKILLS and InTargetSkillRange(skillIndex) and not IsToggle(skillIndex)
        if self.mountedSkillCycle and offensive:
            
            self.riderAttackAttempts[index] = self.riderAttackAttempts.get(index, 0) + 1
            player.ClickSkillSlot(slot)
            self.riderAttackPending = (index, slot, now)
            self.skillHoldUntil = now + SKILL_MOTION_HOLD
            return
        before = self.BuffPresent(slot, skillIndex)
        self.riderTried.add(index)
        player.ClickSkillSlot(slot)
        self.skillNext[index] = now + max(SKILL_MIN_INTERVAL, float(self.config['skill%d_interval' % index]))
        if skillIndex in RIDER_BUFFS:
            self.riderPending[skillIndex] = (index, slot, now, before)
        self.skillHoldUntil = now + SKILL_MOTION_HOLD

    def RiderConfirmAttack(self, now):
        if self.riderAttackPending is None:
            return False
        index, slot, at = self.riderAttackPending
        if self.SkillCooling(slot):
            self.skillNext[index] = at + max(SKILL_MIN_INTERVAL, float(self.config['skill%d_interval' % index]))
            self.riderTried.add(index)
            self.riderAttackPending = None
        elif now < at + SKILL_MOTION_HOLD:
            return True
        else:
            self.riderAttackPending = None
            if self.riderAttackAttempts[index] >= RIDER_ATTACK_TRIES:
                self.riderTried.add(index)
                self.riderRefused[index] = now + RIDER_ATTACK_RETRY
        return False

    def RiderRestoreSkillTarget(self, now):
        if not self.mountedSkillCycle or not self.config['attack']:
            return
        if not self.RiderNeedsSkillTarget(now):
            return
        vid = self.targetVid
        distance = player.GetCharacterDistance(vid) if vid else -1
        alive = vid and hasattr(player, 'IsTargetDead') and not player.IsTargetDead(vid)
        if not alive or distance < 0 or distance > min(MELEE_REACH, self.Reach()):
            if self.riderTargetSearchUntil is None:
                self.riderTargetSearchUntil = now + RIDER_FOOT_GRACE
                self.riderTargetGraceUntil = max(self.riderTargetGraceUntil, self.riderTargetSearchUntil)
            if now < self.riderTargetSearchUntil and now >= self.riderNextTargetRequest and self.CommandGapOk(now):
                self.riderNextTargetRequest = now + TARGET_REQUEST_INTERVAL
                self.Command('/autohunt_target 300 0 0 0 %d %d 0 1' % (
                    1 if self.config.get('mobs', 1) else 0,
                    1 if self.config.get('bosses', 0) else 0), now)
            return
        if player.GetTargetVID() == vid or now < self.riderNextSkillTarget:
            return
        self.riderNextSkillTarget = now + RIDER_MARK_INTERVAL
        player.SetTarget(vid)

    def RiderNeedsSkillTarget(self, now):
        if not self.config['use_skills']:
            return False
        for index in xrange(SKILL_SLOTS):
            slot = self.config['skill%d_slot' % index]
            if not slot or index in self.riderTried or now < self.skillNext[index] or now < self.riderRefused.get(index, 0.0):
                continue
            skillIndex = player.GetSkillIndex(slot)
            if skillIndex not in BUFF_SKILLS and skillIndex not in HORSE_SKILLS and InTargetSkillRange(skillIndex) and not IsToggle(skillIndex) and not self.SkillCooling(slot):
                return True
        return False

    def RiderSkillBusy(self, now):
        ask = getattr(player, 'IsUsingSkill', None)
        if ask is not None:
            try:
                return bool(ask())
            except Exception:
                pass
        return now < self.riderNextCast

    def RiderStep(self, now, rider):
        """One step of a climb-down - the pause, "/ride", the wait for the
        client to show the character on foot, the casts - or of the way back
        on. The character is never left on foot by a step that failed: every
        way out of the ground leads to MOUNT."""
        mounted = IsMounted()
        phase = self.riderPhase
        if phase in (DISMOUNT, TO_FOOT):
            if not mounted:
                self.RiderOnFoot(now)
                return
            if phase == DISMOUNT:
                if not rider:
                    # Switched off before the climb-down: it stays in the saddle.
                    self.riderPhase = RIDE
                    return
                if now < self.riderPhaseUntil or not self.CommandGapOk(now):
                    return
                self.riderDismountTries += 1
                self.Command('/ride', now)
                self.riderPhase = TO_FOOT
                self.riderPhaseUntil = now + RIDER_STEP_WAIT
                return
            # TO_FOOT: a "/ride" is on its way, switch or no switch - the
            # ground it lands on leads back to the saddle (CAST, then MOUNT).
            if now < self.riderPhaseUntil:
                return
            if rider and self.riderDismountTries < RIDER_DISMOUNT_TRIES:
                self.riderPhase = DISMOUNT
                self.riderPhaseUntil = now
                return
            # The server would not take it down (a busy character, say): it
            # fights on from the saddle and tries again later.
            self.riderPhase = RIDE
            self.riderNextCycle = now + RIDER_CYCLE_BACKOFF
            return
        if phase == CAST:
            if self.mountedSkillCycle and self.RiderConfirmAttack(now):
                return
            if mounted:
                self.RiderInSaddle(now)
                return
            if self.RiderHealthShare() < RIDER_DANGER_HP:
                # Too hurt to stand there: back on at once, whatever is left.
                self.RiderBeginMount(now, pause=False)
                return
            if not rider or now - self.riderFootAt >= RIDER_FOOT_LIMIT:
                self.RiderBeginMount(now)
                return
            if now < self.riderPhaseUntil or self.RiderSkillBusy(now):
                return
            self.RiderRestoreSkillTarget(now)
            due = self.MountedFootSkills(now, RIDER_RENEW_ON_FOOT) if self.mountedSkillCycle else self.RiderBuffs(now, RIDER_RENEW_ON_FOOT)
            if not due:
                if self.mountedSkillCycle and now < self.riderTargetGraceUntil:
                    return
                self.RiderBeginMount(now)
                return
            (index, slot, skillIndex) = due[0]
            self.RiderCast(now, index, slot, skillIndex)
            self.riderPhaseUntil = now + RIDER_CAST_MIN_GAP
            return
        # MOUNT and TO_SADDLE.
        if mounted:
            self.RiderInSaddle(now)
            return
        if phase == MOUNT:
            if now < self.riderPhaseUntil or now < self.skillHoldUntil or not self.CommandGapOk(now):
                return
            if self.RiderSummon(now):
                return
            self.riderMountTries += 1
            self.riderMountGroupTries += 1
            self.Command('/ride', now)
            self.riderPhase = TO_SADDLE
            self.riderPhaseUntil = now + RIDER_STEP_WAIT
            return
        if now < self.riderPhaseUntil:
            return
        
        
        self.riderNeedsSummon = True
        if self.riderMountTries >= RIDER_MOUNT_MAX_TRIES:
            self.riderPhase = RIDE
            self.riderWasMounted = False
            self.riderSaddle = False
            chat.AppendChat(chat.CHAT_TYPE_INFO, T('Auto \xa3owy: nie da si\xea wsi\xb9\x9c\xe6 na konia - \xb3owy dalej pieszo. Wsi\xb9d\x9f sam (Ctrl+G), a bojowiec wr\xf3ci.',
                'Auto Hunt: the horse cannot be mounted - hunting on foot. Get on yourself (Ctrl+G) and the battle horse mode comes back.'))
            return
        if self.riderMountGroupTries < self.riderMountGroupLimit:
            self.riderPhase = MOUNT
            self.riderPhaseUntil = now + HumanPause(RIDER_PAUSE_MOUNT)
            return
        # Not in the saddle after the quick tries - no horse called, or one
        # too tired: the hunt goes on on foot and tries again later, one try
        # at a time, each of which the server answers with its reason.
        self.riderPhase = RIDE
        self.riderWasMounted = False
        self.riderNextMount = now + RIDER_MOUNT_SLOW_RETRY
        if not self.riderSaidRetry:
            self.riderSaidRetry = True
            chat.AppendChat(chat.CHAT_TYPE_INFO, T('Auto \xa3owy: nie mog\xea wsi\xb9\x9c\xe6 na konia - pr\xf3buj\xea co %d s, a do tego czasu walcz\xea pieszo.',
                'Auto Hunt: cannot get on the horse - trying every %d s, fighting on foot meanwhile.') % int(RIDER_MOUNT_SLOW_RETRY))

    def RiderOnFoot(self, now):
        self.riderPhase = CAST
        self.riderFootAt = now
        self.riderPhaseUntil = now + HumanPause(RIDER_PAUSE_ON_FOOT)
        self.riderTargetGraceUntil = now + RIDER_FOOT_GRACE
        self.riderNextSkillTarget = 0.0
        self.riderNextCast = 0.0
        self.riderNextTargetRequest = 0.0
        self.riderTargetSearchUntil = None

    def RiderBeginMount(self, now, pause=True, quick=True):
        """The way back on: RIDER_MOUNT_QUICK_TRIES tries, or one when the
        quick ones have already failed and this is a later try."""
        self.ReleaseAttack()
        self.riderPhase = MOUNT
        self.riderMountGroupTries = 0
        self.riderMountGroupLimit = RIDER_MOUNT_QUICK_TRIES if quick else 1
        self.riderPhaseUntil = now + HumanPause(RIDER_PAUSE_MOUNT) if pause else now

    def RiderSummon(self, now):
        """Use the native book after a death or a start on foot. /ride does
        not call a horse, and the horse destroyed at death is no follower.
        A horse already out makes the book's quest return without a dialog;
        the correct book keeps the quest's grade, SP and chance checks."""
        if not self.riderNeedsSummon:
            return False
        self.riderNeedsSummon = False
        book = 50053 if HorseLevel() >= RIDER_SKILL_HORSE_LEVEL else 50052
        cell = FindInventoryCell(book)
        if cell < 0:
            return False
        net.SendItemUsePacket(cell)
        self.riderPhaseUntil = now + RIDER_SUMMON_WAIT
        return True

    def RiderInSaddle(self, now):
        self.riderPhase = RIDE
        self.mountedSkillCycle = False
        self.riderAttackPending = None
        self.riderWasMounted = True
        self.riderSaddle = True
        self.riderNeedsSummon = False
        self.riderMountTries = 0
        self.riderMountGroupTries = 0
        self.riderFootSince = 0.0
        self.riderTried = set()
        self.riderSaidRetry = False
        self.riderNextCycle = max(self.riderNextCycle, now + RIDER_CYCLE_GAP)
        self.nextRequest = 0.0

    def RiderOnDeath(self):
        """Dead: a rider in the saddle, or on its way down or back up, gets
        on its horse again once it stands up and its health is back
        ('HP po wskrz. %'); the steps of a climb-down end here."""
        self.riderSaddle = bool(self.config.get('rider', 0))
        self.riderNeedsSummon = self.riderSaddle
        self.riderPhase = RIDE
        self.mountedSkillCycle = False
        self.riderAttackPending = None
        self.riderWasMounted = False
        self.riderFootSince = 0.0
        self.riderPending = {}
        self.riderTried = set()
        self.riderNextMount = 0.0
        self.riderMountTries = 0
        self.riderLastHp = None

    def RiderRemountOnStop(self):
        """A hunt stopped on the ground of a climb-down puts the character back
        in the saddle - unless a "/ride" is already on its way (TO_FOOT,
        TO_SADDLE), which one more would undo."""
        if self.riderPhase not in (CAST, MOUNT) or IsMounted():
            return
        if player.GetStatus(player.HP) <= 0:
            return
        self.Command('/ride')

    def LoadGlobalConfig(self):
        path = GlobalConfigPath()
        if os.path.exists(path):
            try:
                with open(path, 'r') as handle:
                    for line in handle.read().splitlines():
                        key, _, value = line.partition('=')
                        key = key.strip()
                        if key in self.config_global:
                            try:
                                self.config_global[key] = int(value.strip())
                            except ValueError:
                                pass
            except (IOError, OSError):
                pass

    def ReadSavedConfig(self):
        """The character's file as settings, the defaults with no file."""
        path = ConfigPath(self.configName)
        for older in (OldConfigPath(self.configName), OldestConfigPath(self.configName)):
            if os.path.exists(path):
                break
            path = older

        try:
            with open(path, 'r') as handle:
                return ConfigFromText(handle.read())
        except (IOError, OSError):
            return DefaultConfig()

    def LoadConfig(self):
        self.config = self.ReadSavedConfig()
        # The kinds this character's file kept become the pick-up filter's,
        # once, when the client has no filtr.cfg yet (a file there only).
        for path in (ConfigPath(self.configName), OldConfigPath(self.configName), OldestConfigPath(self.configName)):
            if os.path.exists(path):
                PickupFilter().AdoptAutoHuntKinds(ConfigLootKinds(self.config))
                break
        autologin.SetArmed(self.config.get('autologin', 0))

    def EnsureLoaded(self):
        if not self.isLoaded:
            self.configName = player.GetMainCharacterName()
            self.LoadConfig()
            self.isLoaded = True

    def SaveKeys(self, keys):
        """These keys of the settings in memory into the character's file,
        every other line as the file has it: a pick-up switch is kept the
        moment it is clicked, and whatever else was changed and not saved
        stays unsaved."""
        if not self.configName:
            return False
        saved = self.ReadSavedConfig()
        for key in keys:
            saved[key] = self.config[key]
        if not os.path.exists(CONFIG_CHAR_DIR):
            try:
                os.makedirs(CONFIG_CHAR_DIR)
            except (IOError, OSError):
                pass
        try:
            with open(ConfigPath(self.configName), 'w') as handle:
                handle.write(ConfigText(saved))
            return True
        except (IOError, OSError):
            return False

    # MT2009_PLUS_AUTOHUNT_PICKUP_TOGGLE_V1: only the pick-up switch into
    # the character's file - what it saved before stays as it was.
    def SavePickupChoice(self):
        return self.SaveKeys(('pickup',))

    def SaveGlobalConfig(self):
        if not os.path.exists(CONFIG_BASE_DIR):
            try:
                os.makedirs(CONFIG_BASE_DIR)
            except (IOError, OSError):
                pass
                
        if self.mainWindow and self.lootWindow:
            try:
                mx, my = self.mainWindow.GetLocalPosition()
                lx, ly = self.lootWindow.GetLocalPosition()
                self.config_global['win_main_x'] = int(mx)
                self.config_global['win_main_y'] = int(my)
                self.config_global['win_loot_x'] = int(lx)
                self.config_global['win_loot_y'] = int(ly)
            except RuntimeError:
                pass
                
        try:
            with open(GlobalConfigPath(), 'w') as handle:
                for k, v in self.config_global.items():
                    handle.write('%s=%d\n' % (k, v))
        except (IOError, OSError):
            pass

    def SaveConfig(self):
        if not os.path.exists(CONFIG_CHAR_DIR):
            try:
                os.makedirs(CONFIG_CHAR_DIR)
            except (IOError, OSError):
                pass
                
        self.SaveGlobalConfig()
        
        try:
            with open(ConfigPath(self.configName), 'w') as handle:
                handle.write(ConfigText(self.config))
            return True
        except (IOError, OSError):
            return False

    def CreateWindows(self):
        self.mainWindow = AutoHuntWindow(self)
        self.lootWindow = AutoHuntLootWindow(self)
        
        sw = wndMgr.GetScreenWidth()
        sh = wndMgr.GetScreenHeight()
        
        w1 = self.mainWindow.WIDTH
        h1 = self.mainWindow.HEIGHT
        w2 = self.lootWindow.WIDTH
        
        mx = self.config_global.get('win_main_x', -1)
        my = self.config_global.get('win_main_y', -1)
        lx = self.config_global.get('win_loot_x', -1)
        ly = self.config_global.get('win_loot_y', -1)
        
        if mx < 0 or my < 0 or mx + w1 > sw or my + h1 > sh:
            mx = max(0, (sw - (w1 + 10 + w2)) / 2)
            my = max(0, (sh - h1) / 2)
            lx = mx + w1 + 10
            ly = my
            
        if lx < 0 or ly < 0 or lx + w2 > sw or ly + self.lootWindow.HEIGHT > sh:
            lx = mx + w1 + 10
            ly = my
            
        self.mainWindow.SetPosition(int(mx), int(my))
        self.lootWindow.SetPosition(int(lx), int(ly))

    def ShowLootWindow(self):
        """The settings window alone, from the game options' "Ustaw": its
        first panel is the pick-up filter."""
        self.EnsureLoaded()
        if self.lootWindow is None:
            self.CreateWindows()
        try:
            self.lootWindow.IsShow()
        except RuntimeError:
            self.mainWindow = None
            self.lootWindow = None
            self.CreateWindows()
        self.lootWindow.Refresh()
        self.lootWindow.Show()
        self.lootWindow.SetTop()
        if self.mainWindow:
            self.mainWindow.RefreshSettingsButton()

    def ToggleWindow(self):
        self.EnsureLoaded()

        if self.mainWindow is None:
            self.CreateWindows()

        try:
            is_show = self.mainWindow.IsShow()
        except RuntimeError:
            self.mainWindow = None
            self.lootWindow = None
            self.CreateWindows()
            is_show = False

        # MT2009_PLUS_AUTOHUNT_WINDOWS_V1 (Autor: blaki): K opens the fight
        # window alone; its "Dodatkowe ustawienia" row opens the settings
        # window, and closing K closes both.
        if is_show:
            self.mainWindow.Close()
        else:
            self.mainWindow.Refresh()
            self.lootWindow.Refresh()
            self.mainWindow.Show()
            self.mainWindow.SetTop()
            self.mainWindow.RefreshSettingsButton()
            self.SaveGlobalConfig()

    # MT2009_PLUS_AUTOHUNT_QUICK_V1: the quick start/stop (Shift+K unless
    # rebound - keybind.py "autohunt_quick" - and the inventory sidebar's
    # button), no window opened. Started, the hunt takes this character's
    # settings as K's window and its "Start" take them: the file read once
    # (EnsureLoaded, also when the window was never opened), the window's
    # fields first when it is open - with the attack on. Pressed again, it
    # stops, as the window's "Stop". A server that refuses the hunt (the
    # world without Auto Lowy, or no "Auto Lowy (8h)" time) answers the
    # first target asked with "AutoHuntOff" and OnServerOff stops it and
    # says why, as for a start from the window.
    def OpenMainWindow(self):
        """K's window when it is built and shown, else None."""
        try:
            if self.mainWindow and self.mainWindow.IsShow():
                return self.mainWindow
        except RuntimeError:
            pass
        return None

    def QuickToggle(self):
        import warpsafe
        window = self.OpenMainWindow()
        if self.running:
            if window:
                window.OnStop()
            else:
                self.Stop()
            return
        if not warpsafe.InGame():
            return
        self.EnsureLoaded()
        if window:
            window.ReadEdits()
        attackSwitched = not self.config.get('attack', 1)
        if attackSwitched:
            self.config['attack'] = 1
        text = T('Auto \xa3owy: szybki start, zasi\xeag %d', 'Auto Hunt: quick start, range %d') % self.config['range']
        if attackSwitched:
            text += T(', atak w\xb3\xb9czony', ', attack switched on')
        keys = ''
        try:
            import keybind
            keys = keybind.GetText('autohunt_quick')
        except Exception:
            pass
        if keys:
            text += T(' (%s - stop).', ' (%s - stop).') % keys
        else:
            text += '.'
        self.Start(text)
        if window:
            if attackSwitched:
                window.Refresh()
            else:
                window.RefreshStatus()


class AutoHuntWindow(ui.BoardWithTitleBar):
    WIDTH = 300
    # MT2009_PLUS_AUTOHUNT_WINDOWS_V1: 25 more for "Dodatkowe ustawienia".
    HEIGHT = 580
    GRID_ROWS = 4
    SLOT_STEP = 40
    SLOTS_PER_ROW = 6
    EDIT_W = 34
    EDIT_H = 18

    def __init__(self, hunter):
        ui.BoardWithTitleBar.__init__(self)
        self.hunter = hunter
        self.widgets = []
        self.edits = {}
        self.toggles = {}
        self.nextStatus = 0.0
        self.AddFlag('movable')
        self.AddFlag('float')
        self.SetSize(self.WIDTH, self.HEIGHT)
        self.SetTitleName(T('Auto \xa3owy - Walka', 'Auto Hunt - Combat'))
        self.SetCloseEvent(ui.__mem_func__(self.Close))
        self.Build()

    def Build(self):
        BL = 10
        BW = self.WIDTH - 2 * BL
        SL = 20
        SECTION_GAP = 5
        y = 32

        sk_r1_y = 22
        sk_e1_y = sk_r1_y + 34
        sk_r2_y = sk_e1_y + 18 + 4
        sk_e2_y = sk_r2_y + 34
        sk_h = sk_e2_y + 18 + 7

        skBoard = self._Board(BL, y, BW, sk_h)
        self._Label(skBoard, 14, 4, T('Umiej\xeatno\x9cci', 'Skills'))

        self.skillSlots1 = self._Slots(skBoard, SL, sk_r1_y, self.SLOTS_PER_ROW)
        self.skillSlots1.SetSelectEmptySlotEvent(ui.__mem_func__(self._EvSkill1))
        self.skillSlots1.SetSelectItemSlotEvent(ui.__mem_func__(self._EvSkill1))
        self.skillSlots1.SetUnselectItemSlotEvent(ui.__mem_func__(self._EvClrSkill1))
        for i in xrange(self.SLOTS_PER_ROW):
            self._EditField(skBoard, SL + i * self.SLOT_STEP, sk_e1_y, self.EDIT_W, 'skill%d_interval' % i)

        self.skillSlots2 = self._Slots(skBoard, SL, sk_r2_y, self.SLOTS_PER_ROW)
        self.skillSlots2.SetSelectEmptySlotEvent(ui.__mem_func__(self._EvSkill2))
        self.skillSlots2.SetSelectItemSlotEvent(ui.__mem_func__(self._EvSkill2))
        self.skillSlots2.SetUnselectItemSlotEvent(ui.__mem_func__(self._EvClrSkill2))
        for i in xrange(self.SLOTS_PER_ROW):
            self._EditField(skBoard, SL + i * self.SLOT_STEP, sk_e2_y, self.EDIT_W, 'skill%d_interval' % (i + self.SLOTS_PER_ROW))

        y += sk_h + SECTION_GAP

        mk_r1_y = 22
        mk_e1_y = mk_r1_y + 34
        mk_h = mk_e1_y + 18 + 7

        mkBoard = self._Board(BL, y, BW, mk_h)
        self._Label(mkBoard, 14, 4, T('Mikstury (% HP / PE)', 'Potions (% HP / SP)'))

        self.itemSlots1 = self._Slots(mkBoard, SL, mk_r1_y, self.SLOTS_PER_ROW)
        self.itemSlots1.SetSelectEmptySlotEvent(ui.__mem_func__(self._EvItem1))
        self.itemSlots1.SetSelectItemSlotEvent(ui.__mem_func__(self._EvItem1))
        self.itemSlots1.SetUnselectItemSlotEvent(ui.__mem_func__(self._EvClrItem1))
        for i in xrange(self.SLOTS_PER_ROW):
            self._EditField(mkBoard, SL + i * self.SLOT_STEP, mk_e1_y, self.EDIT_W, ITEM_EDIT_KEYS[i])

        y += mk_h + SECTION_GAP

        od_r1_y = 22
        od_e1_y = od_r1_y + 34
        od_r2_y = od_e1_y + 18 + 4
        od_e2_y = od_r2_y + 34
        od_h = od_e2_y + 18 + 7

        odBoard = self._Board(BL, y, BW, od_h)
        self._Label(odBoard, 14, 4, T('Odpa\xb3y (Sekundy)', 'Timed items (seconds)'))

        self.itemSlots2 = self._Slots(odBoard, SL, od_r1_y, self.SLOTS_PER_ROW)
        self.itemSlots2.SetSelectEmptySlotEvent(ui.__mem_func__(self._EvItem2))
        self.itemSlots2.SetSelectItemSlotEvent(ui.__mem_func__(self._EvItem2))
        self.itemSlots2.SetUnselectItemSlotEvent(ui.__mem_func__(self._EvClrItem2))
        for i in xrange(self.SLOTS_PER_ROW):
            self._EditField(odBoard, SL + i * self.SLOT_STEP, od_e1_y, self.EDIT_W, ITEM_EDIT_KEYS[i + self.SLOTS_PER_ROW])

        self.itemSlots3 = self._Slots(odBoard, SL, od_r2_y, self.SLOTS_PER_ROW)
        self.itemSlots3.SetSelectEmptySlotEvent(ui.__mem_func__(self._EvItem3))
        self.itemSlots3.SetSelectItemSlotEvent(ui.__mem_func__(self._EvItem3))
        self.itemSlots3.SetUnselectItemSlotEvent(ui.__mem_func__(self._EvClrItem3))
        for i in xrange(self.SLOTS_PER_ROW):
            self._EditField(odBoard, SL + i * self.SLOT_STEP, od_e2_y, self.EDIT_W, ITEM_EDIT_KEYS[i + self.SLOTS_PER_ROW * 2])

        y += od_h + SECTION_GAP

        ROW_H = 19
        st_row_start = 22
        # A label has the eighty pixels left of its switch: the English ones
        # are cut to what the Polish ones take.
        settings_rows = [
            (T('Atak', 'Attack'),                  'attack',            'toggle'),
            (T('Umiej\xeatno\x9cci', 'Skills'),    'use_skills',        'toggle'),
            (T('Wskrzeszenie', 'Revive'),          'revive',            'toggle'),
            (T('HP po wskrz. %', 'Revive HP %'),   'revive_hp_percent', 'edit'),
            (T('Mikstury', 'Potions'),             'use_potions',       'toggle'),
            (T('Odpa\xb3y', 'Timed items'),        'use_buffs',         'toggle'),
            (T('Wracaj', 'Return'),                'return',            'toggle'),
            # The grid's eighth place, empty until then: the window keeps its
            # size and the switch looks like its neighbours (autologin.py).
            ('Autologin',                          'autologin',         'toggle'),
            # The bojowiec (Setnil, 1 October): the right half of the board's
            # title row, empty until then, so the eight keep the places their
            # players know and the window its 555 pixels, which an 800x600
            # screen holds.
            (T('Bojowiec', 'Battle horse'),        'rider',             'toggle'),
        ]
        grid = 2 * self.GRID_ROWS
        st_h = st_row_start + self.GRID_ROWS * ROW_H + 4

        stBoard = self._Board(BL, y, BW, st_h)
        self._Label(stBoard, 14, 4, T('Ustawienia Walki', 'Combat settings'))

        col_w = (BW - 8) // 2
        for idx, (lbl, key, kind) in enumerate(settings_rows):
            if idx < grid:
                col = idx // self.GRID_ROWS
                row = idx % self.GRID_ROWS
            else:
                col = 1
                row = -1
            x = 4 + col * col_w
            ry = st_row_start + row * ROW_H
            if kind == 'toggle':
                self._SettingRow(stBoard, x, ry, col_w - 2, ROW_H - 1, lbl, key)
            else:
                self._EditSettingRow(stBoard, x, ry, col_w - 2, ROW_H - 1, lbl, key)
        self.toggles['rider'][0].SetToolTipText(T(
            'Ko\xf1 bojowy: atak w miejscu, tylko to, co blisko - po buffy zsiada',
            'Battle horse: fights in place, only what is near - off only for buffs'))

        y += st_h + 4

        bw = 88
        gap = 8
        total = 3 * bw + 2 * gap
        bx = (self.WIDTH - total) // 2

        self.statusText = self._Label(self, bx + 6, y, T('Wy\xb3\xb9czone', 'Off'))
        y += 18

        self._Btn(self, 'large', bx, y, T('Zapisz', 'Save'), self.OnSave)
        bx += bw + gap
        self.startButton = self._Btn(self, 'large', bx, y, 'Start', self.OnStart)
        bx += bw + gap
        self.stopButton = self._Btn(self, 'large', bx, y, T('Zatrzymaj', 'Stop'), self.OnStop)

        # MT2009_PLUS_AUTOHUNT_WINDOWS_V1 (Autor: blaki): the settings window
        # (pick-up, targets, mode, priority) opens only from here.
        self._Label(self, 22, y + 29, T('Dodatkowe ustawienia', 'More settings'))
        self.settingsButton = self._Btn(self, 'large', 202, y + 25, T('Otw\xf3rz', 'Open'),
            self.ToggleExtraWindow)

    # MT2009_PLUS_AUTOHUNT_WINDOWS_V1 (Autor: blaki)
    def RefreshSettingsButton(self):
        loot = self.hunter.lootWindow if self.hunter else None
        try:
            shown = bool(loot and loot.IsShow())
        except RuntimeError:
            shown = False
        self.settingsButton.SetText(T('Zamknij', 'Close') if shown else T('Otw\xf3rz', 'Open'))

    def ToggleExtraWindow(self):
        loot = self.hunter.lootWindow if self.hunter else None
        if not loot:
            return
        if loot.IsShow():
            loot.Close()
        else:
            self.ReadEdits()
            loot.Refresh()
            loot.Show()
            loot.SetTop()
        self.RefreshSettingsButton()
        self.hunter.SaveGlobalConfig()

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

    def _Slots(self, parent, x, y, count):
        slots = ui.SlotWindow()
        slots.SetParent(parent)
        slots.SetPosition(x, y)
        slots.SetSize(count * self.SLOT_STEP, 32)
        for i in xrange(count):
            slots.AppendSlot(i, i * self.SLOT_STEP, 0, 32, 32)
        slots.SetSlotBaseImage('d:/ymir work/ui/public/slot_base.sub', 1.0, 1.0, 1.0, 1.0)
        slots.Show()
        self.widgets.append(slots)
        return slots

    def _EditField(self, parent, x, y, w, key):
        bar = ui.SlotBar()
        bar.SetParent(parent)
        bar.SetPosition(x, y)
        bar.SetSize(w, self.EDIT_H)
        bar.Show()
        self.widgets.append(bar)

        edit = ui.EditLine()
        edit.SetParent(bar)
        edit.SetPosition(4, 1)
        edit.SetSize(w - 6, self.EDIT_H - 2)
        edit.SetMax(4)
        edit.SetNumberMode()
        edit.Show()
        self.widgets.append(edit)
        self.edits[key] = edit

    def _SettingRow(self, board, x, y, w, h, label, key):
        bar = ui.Bar()
        bar.SetParent(board)
        bar.SetPosition(x, y)
        bar.SetSize(w, h)
        bar.SetColor(0xC0000000)
        bar.Show()
        self.widgets.append(bar)

        btn_w = 50
        text_area = w - btn_w - 4
        
        tx = x + (text_area // 2)
        lbl_line = self._Label(board, tx, y + (h - 12) // 2, label)
        lbl_line.SetHorizontalAlignCenter()

        btn = ui.Button()
        btn.SetParent(board)
        btn.SetPosition(x + w - btn_w, y)
        btn.SetUpVisual('d:/ymir work/ui/public/small_button_01.sub')
        btn.SetOverVisual('d:/ymir work/ui/public/small_button_02.sub')
        btn.SetDownVisual('d:/ymir work/ui/public/small_button_03.sub')
        btn.SetText(WlWyl(0))
        btn.SAFE_SetEvent(self.OnToggle, key)
        btn.Show()
        self.widgets.append(btn)
        self.toggles[key] = (btn, label, True)

    def _EditSettingRow(self, board, x, y, w, h, label, key):
        bar = ui.Bar()
        bar.SetParent(board)
        bar.SetPosition(x, y)
        bar.SetSize(w, h)
        bar.SetColor(0xC0000000)
        bar.Show()
        self.widgets.append(bar)

        edit_w = 34 
        text_area = w - 50 - 4
        
        tx = x + (text_area // 2)
        lbl_line = self._Label(board, tx, y + (h - 12) // 2, label)
        lbl_line.SetHorizontalAlignCenter()

        slotbar = ui.SlotBar()
        slotbar.SetParent(board)
        slotbar.SetPosition(x + w - 46, y + 1)
        slotbar.SetSize(edit_w, h - 2)
        slotbar.Show()
        self.widgets.append(slotbar)

        edit = ui.EditLine()
        edit.SetParent(slotbar)
        edit.SetPosition(4, (h - 2 - 14) // 2)
        edit.SetSize(edit_w - 8, 14)
        edit.SetMax(3)
        edit.SetNumberMode()
        edit.Show()
        self.widgets.append(edit)
        self.edits[key] = edit

    def _EvSkill1(self, idx):
        self.OnSkillSlot(idx)

    def _EvClrSkill1(self, idx):
        self.OnClearSkillSlot(idx)

    def _EvSkill2(self, idx):
        self.OnSkillSlot(idx + self.SLOTS_PER_ROW)

    def _EvClrSkill2(self, idx):
        self.OnClearSkillSlot(idx + self.SLOTS_PER_ROW)

    def _EvItem1(self, idx):
        self.OnItemSlot(idx)

    def _EvClrItem1(self, idx):
        self.OnClearItemSlot(idx)

    def _EvItem2(self, idx):
        self.OnItemSlot(idx + self.SLOTS_PER_ROW)

    def _EvClrItem2(self, idx):
        self.OnClearItemSlot(idx + self.SLOTS_PER_ROW)

    def _EvItem3(self, idx):
        self.OnItemSlot(idx + self.SLOTS_PER_ROW * 2)

    def _EvClrItem3(self, idx):
        self.OnClearItemSlot(idx + self.SLOTS_PER_ROW * 2)

    def Refresh(self):
        config = self.hunter.config
        for key, (btn, label, wyl) in self.toggles.items():
            if wyl:
                btn.SetText(WlWyl(config[key]))
            else:
                btn.SetText('%s: %s' % (label, YesNo(config[key])))
        for key, edit in self.edits.items():
            edit.SetText(str(config[key]))
        self.RefreshSlots()
        self.RefreshStatus()

    def RefreshSlots(self):
        config = self.hunter.config
        for i in xrange(self.SLOTS_PER_ROW):
            slot = config['skill%d_slot' % i]
            si = player.GetSkillIndex(slot) if slot else 0
            if si:
                self.skillSlots1.SetSkillSlotNew(i, si, player.GetSkillGrade(slot), player.GetSkillLevel(slot))
            else:
                self.skillSlots1.ClearSlot(i)
        self.skillSlots1.RefreshSlot()

        for i in xrange(self.SLOTS_PER_ROW):
            gi = i + self.SLOTS_PER_ROW
            slot = config['skill%d_slot' % gi]
            si = player.GetSkillIndex(slot) if slot else 0
            if si:
                self.skillSlots2.SetSkillSlotNew(i, si, player.GetSkillGrade(slot), player.GetSkillLevel(slot))
            else:
                self.skillSlots2.ClearSlot(i)
        self.skillSlots2.RefreshSlot()

        for i in xrange(self.SLOTS_PER_ROW):
            vnum = config[ITEM_SLOT_KEYS[i]]
            if vnum:
                self.itemSlots1.SetItemSlot(i, vnum, 0)
            else:
                self.itemSlots1.ClearSlot(i)
        self.itemSlots1.RefreshSlot()

        for i in xrange(self.SLOTS_PER_ROW):
            vnum = config[ITEM_SLOT_KEYS[i + self.SLOTS_PER_ROW]]
            if vnum:
                self.itemSlots2.SetItemSlot(i, vnum, 0)
            else:
                self.itemSlots2.ClearSlot(i)
        self.itemSlots2.RefreshSlot()

        for i in xrange(self.SLOTS_PER_ROW):
            vnum = config[ITEM_SLOT_KEYS[i + self.SLOTS_PER_ROW * 2]]
            if vnum:
                self.itemSlots3.SetItemSlot(i, vnum, 0)
            else:
                self.itemSlots3.ClearSlot(i)
        self.itemSlots3.RefreshSlot()

    def RefreshStatus(self):
        hunter = self.hunter
        if not hunter.running:
            self.statusText.SetText(T('Wy\xb3\xb9czone', 'Off'))
            return
        if hunter.justRevived:
            pct = min(100, hunter.config.get('revive_hp_percent', 60))
            self.statusText.SetText(T('Czekam na HP (%d%%)', 'Waiting for HP (%d%%)') % pct)
            return
        if hunter.escapeUntil:
            self.statusText.SetText(T('Omijam przeszkod\xea', 'Getting round an obstacle'))
            return
        if hunter.pathPoints:
            self.statusText.SetText(T('Id\xea drog\xb9 dooko\xb3a', 'Taking the way round'))
            return
        if hunter.mountRecoveryPhase:
            self.statusText.SetText(T('Wierzchowiec utkn\xb9\xb3 - wsiadam ponownie', 'Mount stuck - getting on again'))
            return
        phase = hunter.riderPhase
        if phase in (DISMOUNT, TO_FOOT):
            self.statusText.SetText(T('Zsiadam po skille', 'Dismounting for skills') if hunter.mountedSkillCycle else T('Bojowiec: zsiadam po buffy', 'Rider: getting off for buffs'))
            return
        if phase == CAST:
            self.statusText.SetText(T('Rzucam skille', 'Casting skills') if hunter.mountedSkillCycle else T('Bojowiec: rzucam buffy', 'Rider: casting buffs'))
            return
        if phase in (MOUNT, TO_SADDLE):
            self.statusText.SetText(T('Wsiadam na konia', 'Mounting the horse') if hunter.mountedSkillCycle else T('Bojowiec: wsiadam na konia', 'Rider: getting on the horse'))
            return
        if hunter.RiderActive():
            if hunter.targetVid:
                self.statusText.SetText(T('Bojowiec - cel: %s', 'Rider - target: %s') % chr.GetNameByVID(hunter.targetVid))
            else:
                self.statusText.SetText(T('Bojowiec: czekam na potwory', 'Rider: waiting for monsters'))
            return
        if hunter.targetVid:
            self.statusText.SetText(T('Cel: %s', 'Target: %s') % chr.GetNameByVID(hunter.targetVid))
        elif hunter.lootVid:
            self.statusText.SetText(T('Podnosz\xea przedmiot', 'Picking up an item'))
        else:
            self.statusText.SetText(T('Szukam potwork\xf3w', 'Looking for monsters'))

    def ReadEdits(self):
        for key, edit in self.edits.items():
            try:
                self.hunter.config[key] = max(0, int(edit.GetText() or 0))
            except ValueError:
                pass

    def OnUpdate(self):
        now = clientclock.Now()
        if now < self.nextStatus:
            return
        self.nextStatus = now + STATUS_INTERVAL
        self.RefreshStatus()

    def OnStart(self):
        self.ReadEdits()
        if not self.hunter.running:
            self.hunter.Start()
        self.RefreshStatus()

    def OnStop(self):
        self.ReadEdits()
        if self.hunter.running:
            self.hunter.Stop()
        self.RefreshStatus()

    def OnToggle(self, key):
        self.ReadEdits()
        self.hunter.config[key] = 0 if self.hunter.config[key] else 1
        if key == 'autologin':
            autologin.SetArmed(self.hunter.config[key])
            if self.hunter.config[key]:
                chat.AppendChat(chat.CHAT_TYPE_INFO, T('Auto \xa3owy: autologin w\xb3\xb9czony - po zerwaniu po\xb3\xb9czenia gra sama zaloguje si\xea ponownie i wznowi \xb3owy. Zapisz, \xbfeby zapami\xeata\xe6 to na nast\xeapny raz.',
                    'Auto Hunt: autologin on - if the connection drops, the game logs in again by itself and the hunt goes on. Save to keep it for next time.'))
            else:
                chat.AppendChat(chat.CHAT_TYPE_INFO, T('Auto \xa3owy: autologin wy\xb3\xb9czony.', 'Auto Hunt: autologin off.'))
        elif key == 'rider':
            self.hunter.RiderSwitched()
            try:
                if self.hunter.lootWindow:
                    self.hunter.lootWindow.Refresh()
            except Exception:
                pass
        self.Refresh()

    def OnSave(self):
        self.ReadEdits()
        if self.hunter.SaveConfig():
            chat.AppendChat(chat.CHAT_TYPE_INFO, T('Auto \xa3owy: ustawienia zapisane.', 'Auto Hunt: settings saved.'))
        else:
            chat.AppendChat(chat.CHAT_TYPE_INFO, T('Auto \xa3owy: nie uda\xb3o si\xea zapisa\xe6 ustawie\xf1.',
                'Auto Hunt: the settings could not be saved.'))

    def OnSkillSlot(self, index):
        attached = self.TakeAttached()
        if attached and attached[0] == player.SLOT_TYPE_SKILL:
            self.hunter.config['skill%d_slot' % index] = attached[1]
            self.RefreshSlots()

    def OnClearSkillSlot(self, index):
        self.hunter.config['skill%d_slot' % index] = 0
        self.RefreshSlots()

    def OnItemSlot(self, index):
        attached = self.TakeAttached()
        if attached and attached[0] == player.SLOT_TYPE_INVENTORY:
            self.hunter.config[ITEM_SLOT_KEYS[index]] = attached[2]
            self.RefreshSlots()

    def OnClearItemSlot(self, index):
        self.hunter.config[ITEM_SLOT_KEYS[index]] = 0
        self.RefreshSlots()

    def TakeAttached(self):
        controller = mouseModule.mouseController
        if not controller.isAttached():
            return None
        attached = (
            controller.GetAttachedType(),
            controller.GetAttachedSlotNumber(),
            controller.GetAttachedItemIndex(),
        )
        controller.DeattachObject()
        return attached

    def Close(self):
        self.ReadEdits()
        if self.hunter:
            self.hunter.SaveGlobalConfig()
            # MT2009_PLUS_AUTOHUNT_WINDOWS_V1: closing K closes both.
            if self.hunter.lootWindow:
                self.hunter.lootWindow.Close()
        self.Hide()

    def OnPressEscapeKey(self):
        self.Close()
        return True

    def Destroy(self):
        self.Hide()
        self.hunter = None
        self.widgets = []
        self.edits = {}
        self.toggles = {}
        self.settingsButton = None


# MT2009_PLUS_AUTOHUNT_PICKUP_TOGGLE_V1: "Autopodnoszenie" has a row of its
# own above the grid.
# MT2009_PLUS_PICKUP_BONUS_FILTER_V1: and "Min. bonusow" after "Filtr" - the
# pick-up filter's (uipickupfilter.CycleBonusMin).
LOOT_ROWS = (len(LOOT_KINDS) + 2 + 2) // 3   # the kinds, "Filtr" and "Min. bonusow", three a row
LOOT_PICKUP_ROW_H = 22


class AutoHuntLootWindow(ui.BoardWithTitleBar):
    WIDTH = 300
    # MT2009_PLUS_AUTOHUNT_PRIORITY_V1: 88 more for the target's mode and
    # the priority.
    HEIGHT = 267 + LOOT_ROWS * 22 + LOOT_PICKUP_ROW_H

    def __init__(self, hunter):
        ui.BoardWithTitleBar.__init__(self)
        self.hunter = hunter
        self.widgets = []
        self.toggles = {}
        self.kindToggles = {}
        self.filterBtn = None
        self.bonusMinBtn = None
        self.pickupBtn = None
        self.targetModeBtn = None
        self.priorityBtn = None
        self.priorityText = None
        self.AddFlag('movable')
        self.AddFlag('float')
        self.SetSize(self.WIDTH, self.HEIGHT)
        self.SetTitleName(T('Auto \xa3owy - Ustawienia', 'Auto Hunt - Settings'))
        self.SetCloseEvent(ui.__mem_func__(self.Close))
        self.Build()

    def Build(self):
        BL = 10
        BW = self.WIDTH - 2 * BL
        y = 32

        pd_btn_start = 24 + LOOT_PICKUP_ROW_H
        pd_h = pd_btn_start + LOOT_ROWS * 22 + 8
        pdBoard = self._Board(BL, y, BW, pd_h)
        self._Label(pdBoard, 14, 4, T('Podnoszenie (filtr jak pod Ctrl+Z)', 'Pick-up (the Ctrl+Z filter)'))

        # MT2009_PLUS_AUTOHUNT_PICKUP_TOGGLE_V1: "Autopodnoszenie" is Auto
        # Lowy's own, a row like the "Ustawienia Walki" ones; the kinds and
        # "Filtr" (the last cell of the last row) are the pick-up filter's
        # (uipickupfilter.py).
        self._PickupRow(pdBoard, 4, 24, BW - 8, LOOT_PICKUP_ROW_H - 2)
        pdy = pd_btn_start
        for idx, (key, label, bit) in enumerate(LOOT_KINDS):
            pos = idx
            col = pos % 3
            row = pos // 3
            self._KindBtn(pdBoard, 4 + col * 92, pdy + row * 22, label, bit)
        pos = len(LOOT_KINDS)
        self.filterBtn = self._Btn(pdBoard, 'large', 4 + (pos % 3) * 92, pdy + (pos // 3) * 22,
            '', self.OnToggleFilter)
        pos += 1
        self.bonusMinBtn = self._Btn(pdBoard, 'large', 4 + (pos % 3) * 92, pdy + (pos // 3) * 22,
            '', self.OnCycleBonusMin)

        y += pd_h + 5

        tg_btn_start = 24
        tg_h = tg_btn_start + 5 * 22 + 8
        tgBoard = self._Board(BL, y, BW, tg_h)
        self._Label(tgBoard, 14, 4, T('Cele do atakowania', 'What to attack'))

        self._FlagBtn(tgBoard, 4 + 0 * 92, tg_btn_start, T('Moby', 'Monsters'), 'mobs')
        self._FlagBtn(tgBoard, 4 + 1 * 92, tg_btn_start, T('Metiny', 'Metins'), 'stones')
        self._FlagBtn(tgBoard, 4 + 2 * 92, tg_btn_start, T('Bossy', 'Bosses'), 'bosses')
        # MT2009_PLUS_AUTOHUNT_PRIORITY_V1 (Autor: blaki): the target's mode
        # and the order, saved for the character at once.
        self._Label(tgBoard, 14, tg_btn_start + 26, T('Tryb celu:', 'Target mode:'))
        self.targetModeBtn = self._Btn(tgBoard, 'large', BW - 92,
            tg_btn_start + 22, '', self.OnToggleTargetMode)
        self._Label(tgBoard, 14, tg_btn_start + 48, T('Priorytet:', 'Priority:'))
        self.priorityBtn = self._Btn(tgBoard, 'large', BW - 92,
            tg_btn_start + 44, T('Zmie\xf1', 'Change'), self.OnCyclePriority)
        self.priorityText = self._Label(tgBoard, 14, tg_btn_start + 70, '')
        self._Label(tgBoard, 14, tg_btn_start + 92,
            T('Fokus: brak obra\xbfe\xf1 3 s -> nowy cel', 'Focus: no damage for 3 s -> new target'))

        y += tg_h + 10

        self.rangeText = self._Label(self, self.WIDTH // 2, y, T('Zasi\xeag: %d', 'Range: %d') % 2000)
        self.rangeText.SetHorizontalAlignCenter()

        self.rangeSlider = ui.SliderBar()
        self.rangeSlider.SetParent(self)
        self.rangeSlider.SetWindowHorizontalAlignCenter()
        self.rangeSlider.SetPosition(0, y + 18)
        self.rangeSlider.SetEvent(ui.__mem_func__(self.OnChangeRange))
        self.rangeSlider.Show()
        self.widgets.append(self.rangeSlider)

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

    def _FlagBtn(self, parent, x, y, label, key):
        btn = self._Btn(parent, 'large', x, y, '', self.OnToggle, key)
        self.toggles[key] = (btn, label, False)
        return btn

    # MT2009_PLUS_AUTOHUNT_PICKUP_TOGGLE_V1
    def _PickupRow(self, board, x, y, w, h):
        bar = ui.Bar()
        bar.SetParent(board)
        bar.SetPosition(x, y)
        bar.SetSize(w, h)
        bar.SetColor(0xC0000000)
        bar.Show()
        self.widgets.append(bar)

        btn_w = 88
        self.pickupLabel = self._Label(board, x + (w - btn_w - 4) // 2, y + (h - 12) // 2, T('Autopodnoszenie:', 'Auto pick-up:'))
        self.pickupLabel.SetHorizontalAlignCenter()
        self.pickupBtn = self._Btn(board, 'large', x + w - btn_w, y, '', self.OnTogglePickup)

    def _KindBtn(self, parent, x, y, label, bit):
        btn = self._Btn(parent, 'large', x, y, '', self.OnToggleKind, bit)
        self.kindToggles[bit] = (btn, label)
        return btn

    def Refresh(self):
        config = self.hunter.config

        current_range = max(300, min(5000, config['range']))
        slider_pos = float(current_range - 300) / 4700.0
        self.rangeSlider.SetSliderPos(slider_pos)
        self.rangeText.SetText(self.RangeText(current_range))

        for key, (btn, label, wyl) in self.toggles.items():
            btn.SetText('%s: %s' % (label, YesNo(config[key])))
        self.pickupBtn.SetText(OnOff(config['pickup']))
        # MT2009_PLUS_AUTOHUNT_PRIORITY_V1
        self.targetModeBtn.SetText(TargetModeText(config.get('target_mode', TARGET_MODE_FOCUS)))
        order = max(0, min(len(PRIORITY_LABELS) - 1, config.get('priority_order', 0)))
        self.priorityText.SetText(PRIORITY_LABELS[order])

        pickupFilter = PickupFilter()
        for bit, (btn, label) in self.kindToggles.items():
            # MT2009_PLUS_PICKUP_BONUS_FILTER_V1: tak / nie / bonus.
            btn.SetText('%s: %s' % (label, pickupFilter.KindText(bit, YesNo(1), YesNo(0), T('bonus', 'bonus'))))
        self.filterBtn.SetText(T('Filtr: %s', 'Filter: %s') % YesNo(pickupFilter.IsActive()))
        self.bonusMinBtn.SetText(pickupFilter.BonusMinText())

    def RangeText(self, value):
        # MT2009_PLUS_UPSTREAM_2_0_76: the bojowiec fights in place, so in the
        # saddle the range is what a swing (or a bow) reaches
        # (Hunter.RiderRange); said beside the slider, or a slider that stops
        # at 700 reads as broken (Charlie, 2 October).
        text = T('Zasi\xeag: %d', 'Range: %d') % value
        try:
            if self.hunter.config.get('rider', 0):
                text += T(' (na koniu: %d)', ' (mounted: %d)') % self.hunter.RiderRange()
        except Exception:
            pass
        return text

    def OnChangeRange(self):
        if self.hunter.mainWindow:
            self.hunter.mainWindow.ReadEdits()

        pos = self.rangeSlider.GetSliderPos()
        new_range = int(300 + (pos * 4700))
        self.hunter.config['range'] = new_range
        self.rangeText.SetText(self.RangeText(new_range))

        try:
            if self.hunter.running and self.hunter.config.get('return', 0):
                (ax, ay) = self.hunter.anchor
                player.SetAutoHuntRangeCircle(new_range, float(ax), float(ay), 1)
            else:
                player.SetAutoHuntRangeCircle(new_range, 0.0, 0.0, 0)
        except AttributeError:
            pass

    def OnToggle(self, key):
        if self.hunter.mainWindow:
            self.hunter.mainWindow.ReadEdits()
        self.hunter.config[key] = 0 if self.hunter.config[key] else 1
        self.Refresh()

    # MT2009_PLUS_AUTOHUNT_PRIORITY_V1 (Autor: blaki): saved for the
    # character at once, the mode said in the chat.
    def OnToggleTargetMode(self):
        if self.hunter.mainWindow:
            self.hunter.mainWindow.ReadEdits()
        hunter = self.hunter
        mode = hunter.config.get('target_mode', TARGET_MODE_FOCUS)
        mode = TARGET_MODE_NEAREST if mode == TARGET_MODE_FOCUS else TARGET_MODE_FOCUS
        hunter.config['target_mode'] = mode
        hunter.nextRequest = 0.0
        hunter.SaveKeys(('target_mode',))
        chat.AppendChat(chat.CHAT_TYPE_INFO, T('Auto \xa3owy: tryb celu - %s.', 'Auto Hunt: target mode - %s.') % TargetModeText(mode))
        self.Refresh()

    def OnCyclePriority(self):
        if self.hunter.mainWindow:
            self.hunter.mainWindow.ReadEdits()
        hunter = self.hunter
        order = (hunter.config.get('priority_order', 0) + 1) % len(PRIORITY_LABELS)
        hunter.config['priority_order'] = order
        hunter.nextRequest = 0.0
        hunter.SaveKeys(('priority_order',))
        chat.AppendChat(chat.CHAT_TYPE_INFO, T('Auto \xa3owy: priorytet - %s.', 'Auto Hunt: priority - %s.') % PRIORITY_LABELS[order])
        self.Refresh()

    # MT2009_PLUS_AUTOHUNT_PICKUP_TOGGLE_V1: saved for the character at once.
    def OnTogglePickup(self):
        if self.hunter.mainWindow:
            self.hunter.mainWindow.ReadEdits()
        hunter = self.hunter
        # Off, AskForLoot and HandleLootSweep let the loot go next frame.
        hunter.config['pickup'] = 0 if hunter.config['pickup'] else 1
        hunter.SavePickupChoice()
        chat.AppendChat(chat.CHAT_TYPE_INFO, T('Auto \xa3owy: autopodnoszenie %s.', 'Auto Hunt: auto pick-up %s.') % OnOff(hunter.config['pickup']).lower())
        self.Refresh()

    # The pick-up filter's kinds and switch: saved, sent to the server and
    # shown in the Ctrl+Z window at once (uipickupfilter.Changed, which
    # refreshes this window too).
    def OnToggleKind(self, bit):
        if self.hunter.mainWindow:
            self.hunter.mainWindow.ReadEdits()
        PickupFilter().ToggleKind(bit)
        self.Refresh()

    def OnToggleFilter(self):
        if self.hunter.mainWindow:
            self.hunter.mainWindow.ReadEdits()
        PickupFilter().ToggleOn()
        self.Refresh()

    # MT2009_PLUS_PICKUP_BONUS_FILTER_V1
    def OnCycleBonusMin(self):
        if self.hunter.mainWindow:
            self.hunter.mainWindow.ReadEdits()
        PickupFilter().CycleBonusMin()
        self.Refresh()

    def Close(self):
        try:
            player.SetAutoHuntRangeCircle(0)
        except AttributeError:
            pass

        if self.hunter:
            self.hunter.SaveGlobalConfig()
        self.Hide()
        # MT2009_PLUS_AUTOHUNT_WINDOWS_V1: "Zamknij" back to "Otworz".
        if self.hunter and self.hunter.mainWindow:
            self.hunter.mainWindow.RefreshSettingsButton()

    def OnPressEscapeKey(self):
        self.Close()
        return True

    def Destroy(self):
        self.Hide()
        self.hunter = None
        self.widgets = []
        self.toggles = {}
        self.kindToggles = {}
        self.filterBtn = None
        self.bonusMinBtn = None
        self.pickupBtn = None
        self.targetModeBtn = None
        self.priorityBtn = None
        self.priorityText = None


_hunter = None

def GetHunter():
    global _hunter
    if _hunter is None:
        _hunter = Hunter()
    return _hunter

def ToggleWindow():
    GetHunter().ToggleWindow()

def ShowLootWindow():
    GetHunter().ShowLootWindow()

# MT2009_PLUS_AUTOHUNT_QUICK_V1: the quick start/stop and whether a hunt runs
# (the inventory sidebar's button shows it).
def QuickToggle():
    GetHunter().QuickToggle()

def IsRunning():
    return _hunter is not None and bool(_hunter.running)

def OnServerTarget(value, blocked='0'):
    GetHunter().OnServerTarget(value, blocked)

# MT2009_PLUS_AUTOHUNT_MOUNT_V1 (Autor: blaki)
def OnServerMount(action, mounted):
    GetHunter().OnServerMount(action, mounted)

def OnServerLoot(vid, x, y):
    GetHunter().OnServerLoot(vid, x, y)

def OnServerPath(seq='0', answer='', kind='0', points='', *rest):
    GetHunter().OnServerPath(seq, answer, kind, points)

def OnServerOff(reason=''):
    GetHunter().OnServerOff(reason)

def OnServerTargetHP(vid, hp, maxHp):
    GetHunter().OnServerTargetHP(vid, hp, maxHp)


def OnFocusLost():
    """The exe cleared its pressed keys: forget our held attack too.
    Background hunting resumes on the next frame with its own live target.
    """
    if _hunter is not None:
        _hunter.ReleaseAttack()
        _hunter.ResetChaseMovement()
