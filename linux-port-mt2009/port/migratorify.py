# -*- coding: utf-8 -*-
"""The playerbot migrator (mariadb/playerbot/apply.sh) for the mt2009 world.

Usage:  python migratorify.py

Copies linux-port/docker/mariadb/playerbot/{apply.sh,itemshop_schema.sql} into
linux-port-mt2009/docker/mariadb/playerbot/ and rewrites what the schema and
the map layout change:

  * the readiness probe: mt2009's log schema has hack_log, not speed_hack;
    player.item_proto is a view over world.item_proto (initdb creates it);
  * the list of maps a bot may be parked on: this stack's m2-render-config
    hosts the package's forty-six maps plus the guild villages and the high
    maps, so the list is that layout, not r40250's.

Idempotent: re-run after editing the r40250 original.
"""
import io
import os
import re
import shutil

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.normpath(os.path.join(HERE, '..', '..', 'linux-port', 'docker', 'mariadb', 'playerbot'))
DST = os.path.normpath(os.path.join(HERE, '..', 'docker', 'mariadb', 'playerbot'))

# Everything m2-render-config's MAPS_first / MAPS_game1 / MAPS_game2 host.
HOSTED = ('1, 3, 4, 5, 6, 107, 81, 110, 111, 112, 113, 181, 182, 183, 200, 250, 302, 304,\n'
          '                               21, 23, 24, 25, 26, 61, 63, 64, 65, 69, 70, 71, 104, 108, 109, 79, 216, 217, 73,\n'
          '                               41, 43, 44, 45, 46, 62, 66, 67, 68, 72, 90, 208, 301, 303, 351')

# What the package's player dump (initdb.d/dumps/player.sql) carries of the
# server it was taken from: its guild lands as (land_id, guild_id) and the
# buildings on them as (id, land_id, vnum) - and not one of those guilds.
PACKAGE_GUILD_LANDS = (
    (2, 408), (8, 78), (9, 108), (10, 69), (14, 3), (15, 395), (16, 2), (17, 52), (18, 18),
    (108, 5), (109, 6), (115, 92), (116, 93), (117, 20), (118, 13),
    (201, 212), (204, 712), (205, 57), (206, 9), (207, 58), (208, 25), (212, 19), (213, 14),
    (214, 15), (215, 344), (216, 47), (217, 33), (218, 7))
PACKAGE_GUILD_OBJECTS = (
    (1, 14, 14100), (2, 214, 14120), (3, 214, 14014), (4, 14, 14013), (5, 215, 14120),
    (6, 215, 14013), (7, 218, 14120), (8, 218, 14043), (9, 16, 14100), (10, 16, 14014),
    (11, 16, 14043), (12, 108, 14100), (13, 108, 14014), (14, 214, 14050), (15, 14, 14051),
    (16, 215, 14051), (17, 217, 14100), (18, 218, 14014), (19, 217, 14015), (20, 109, 14100),
    (21, 109, 14051), (22, 17, 14100), (23, 17, 14015), (24, 207, 14110), (25, 207, 14014),
    (26, 15, 14100), (27, 15, 14015), (28, 217, 14051), (29, 18, 14110), (30, 18, 14055),
    (31, 115, 14120), (32, 115, 14014), (33, 108, 14043), (34, 18, 14015), (35, 116, 14120),
    (36, 116, 14013), (37, 216, 14110), (38, 109, 14015), (39, 8, 14120), (40, 216, 14013),
    (41, 212, 14100), (42, 117, 14110), (43, 216, 14055), (44, 117, 14055), (45, 117, 14014),
    (46, 205, 14120), (47, 205, 14055), (48, 15, 14055), (49, 216, 14200), (50, 216, 14300),
    (51, 216, 14300), (52, 205, 14015), (53, 212, 14015), (54, 206, 14100), (55, 206, 14015),
    (56, 8, 14015), (57, 115, 14050), (58, 212, 14055), (59, 207, 14055), (60, 8, 14055),
    (61, 208, 14110), (62, 201, 14100))

# Three steps that were written into the rendered apply.sh by hand before this
# renderer knew them - the fishing pass and the teleport ring's flag, the bot
# guilds' tier table with the second channel's pins (6629944), and the world's
# difficulty (43ca602) - so a render dropped all three without a word (found
# rendering 2.0.81). They live here now, and a render reproduces the file.
FISHING_PASS_AND_RING = (
    '# And the pass the rod needs. Karta Wedkarska (27620), which CHARACTER::fishing()\n'
    "# wants worn, is sold in one place, the Fisherman's special shop (9009, opened\n"
    '# by fishing_pass_shop.quest), and the package asks level fifty for it - so a\n'
    '# player of thirty to forty-nine could wear the rod the line above allows and\n'
    '# never fish (17 September). The db core reads shop_special_proto at\n'
    '# boot, so this is live on the next start; idempotent, and only a fifty moves.\n'
    'db -e "UPDATE world.shop_special_proto SET limitvalue0 = 30 WHERE item_vnum = 27620 AND limittype0 = \'LEVEL\' AND limitvalue0 = 50; UPDATE world.shop_special_proto SET limitvalue1 = 30 WHERE item_vnum = 27620 AND limittype1 = \'LEVEL\' AND limitvalue1 = 50;"\n'
    '# Pierscien Teleportacji (70058) carries ITEM_FLAG_APPLICABLE (8192) in this\n'
    '# package, and under ENABLE_QUEST_DND_EVENT that flag makes UseItemEx treat an\n'
    '# ITEM_QUEST as "drop it onto another item": a plain use finds no target cell\n'
    '# and returns before the quest is asked, so teleport_ring.quest never ran for\n'
    '# a player ("caly czas nie dziala pierscien teleportu", 16 September).\n'
    '# The ring is dragged onto nothing; the flag comes off. Idempotent.\n'
    'db -e "UPDATE world.item_proto SET flag = flag & ~8192 WHERE vnum = 70058 AND (flag & 8192) <> 0;"\n'
    "# The Grotto of Exile's warp in Orc Valley's bottom-left corner (10077,\n"
    '# commented again since 2.2.22, when Koe-Pung took the way in; kept right for a\n'
    '# GM who puts it back) reads its target out of its own locale_name\n'
    "# (FuncCheckWarp), and the package's pointed at cell (9,46) of map 72 - a\n"
    '# blocked cell six kilometres from any open ground. The target is the\n'
    "# grotto's Town point (100,46), where the engine also stands up whoever dies\n"
    '# in there, 1.3 km from the way out (10078). The db core reads mob_proto at\n'
    '# boot (PROTO_FROM_DB); idempotent.\n'
    'db -e "UPDATE world.mob_proto SET name = \'????1? 100 12078\', locale_name = \'????1? 100 12078\' WHERE vnum = 10077 AND locale_name <> \'????1? 100 12078\';" || echo "[playerbot-migrate] WARNING: could not point the Grotto of Exile warp at its Town" >&2\n'
    "# Three doors of the Devil's Catacomb's fourth-floor maze (10814, 10817,\n"
    '# 10818) carry a locale_name with no space after the dot - ".233 780" - which\n'
    "# FuncCheckWarp's ' %s %ld %ld' cannot read, so the engine moved nobody\n"
    '# through them; their name column is whole, and its targets stand on the\n'
    "# maze's open ground (checked on map 216's server_attr, 26 September). The\n"
    '# stake at the end is reachable in every wiring without them. Idempotent.\n'
    'db -e "UPDATE world.mob_proto SET locale_name = name WHERE vnum IN (10814, 10817, 10818) AND locale_name <> name;" || echo "[playerbot-migrate] WARNING: could not mend the Catacomb maze doors" >&2\n'
    "# Three ItemShop lines stood behind time auctions the package's server ran\n"
    '# in December 2024 - 906 the Metin stone detector, 907 Kamien Duchowy, 908 -\n'
    '# and an ended auction is a line nobody sees and BuyItem refuses, a player\n'
    '# as much as a bot. Their auction rows go and the lines are ordinary ones;\n'
    '# an auction the operator makes is not touched. The db core reads both\n'
    '# tables at boot; idempotent. Until 2.2.22 the second DELETE was a\n'
    '# multi-table one, which MariaDB refuses with no default database, so the\n'
    "# players' buy counts of the three stayed and this warned at every start.\n"
    'db -e "DELETE FROM common.itemshop_time_auctions WHERE item_index IN (906, 907, 908) AND end_time < \'2025-01-01\'; DELETE FROM player.itemshop_time_auction WHERE item_index IN (906, 907, 908) AND item_index NOT IN (SELECT item_index FROM common.itemshop_time_auctions);" || echo "[playerbot-migrate] WARNING: could not end the ItemShop old time auctions" >&2\n'
    '# Pirate Tanaka (5001), the Tanaka event\'s treasure goblin\n'
    '# (playerbot_world_events.h): the package gives him 560 yang, which his fall\n'
    '# splits into thirty piles of twenty, and a flat thousand at each fifth of his\n'
    '# health (playerbotify apply_tanaka_goblin scales that to a fifth of a roll of\n'
    '# these). A world whose operator set his yang by hand keeps it: only the\n'
    '# stock 560 moves. The db core reads mob_proto at boot; idempotent.\n'
    'db -e "UPDATE world.mob_proto SET gold_min = 15000, gold_max = 25000 WHERE vnum = 5001 AND gold_min = 560 AND gold_max = 560;" || echo "[playerbot-migrate] WARNING: could not give Pirate Tanaka his yang" >&2\n'
    '# His ear (30202), which Yonah takes for a Purple Ebony Chest\n'
    '# (tanaka_ears.quest), stacks to the 200 its row already says: the package\n'
    '# left ITEM_FLAG_STACKABLE off, so every ear took a cell. Idempotent.\n'
    'db -e "UPDATE world.item_proto SET flag = flag | 4 WHERE vnum = 30202 AND (flag & 4) = 0;" || echo "[playerbot-migrate] WARNING: could not make Tanaka\'s ear stack" >&2\n'
)

GUILD_TIERS_AND_CHANNEL_PINS = (
    "# The bot guilds' tiers (playerbot_guild.h): a guild outlives every core\n"
    '# restart, so its tier and kingdom live here; the core reads the table once\n'
    '# and writes a row when it founds or adopts a guild.\n'
    'db -e "CREATE TABLE IF NOT EXISTS player.playerbot_guild (guild_id INT UNSIGNED NOT NULL PRIMARY KEY, tier TINYINT UNSIGNED NOT NULL DEFAULT 3, empire TINYINT UNSIGNED NOT NULL DEFAULT 0, founder_pid INT UNSIGNED NOT NULL DEFAULT 0, founded_at DATETIME NOT NULL) ENGINE=InnoDB;"\n'
    '# And when each last went to war (playerbot_guild_war.h), in unix seconds, so\n'
    "# the pick that keeps a kingdom's last pair out of its next war survives the\n"
    '# restart every update makes.\n'
    'db -e "ALTER TABLE player.playerbot_guild ADD COLUMN IF NOT EXISTS last_war_at INT UNSIGNED NOT NULL DEFAULT 0;" \\\n'
    '    || echo "playerbot-migrate: could not add last_war_at to player.playerbot_guild" >&2\n'
    "# The second channel's pins (playerbot_channel_rules.h): every bot that has\n"
    '# ever kept an offline shop lives on the first channel for good, because the\n'
    "# shops are the first channel's. The table only grows - each core adds the\n"
    '# owners it sees before it reads it - and this adds them before any core has\n'
    '# started, so the start that switches the second channel on finds every keeper\n'
    '# of the last session already pinned. Written whatever the switch says.\n'
    'db -e "CREATE TABLE IF NOT EXISTS player.playerbot_channel_pin (pid INT UNSIGNED NOT NULL PRIMARY KEY, pinned_at DATETIME NOT NULL) ENGINE=InnoDB;"\n'
    'db -e "INSERT IGNORE INTO player.playerbot_channel_pin (pid, pinned_at) SELECT owner, NOW() FROM player.ikashop_offlineshop;" 2>/dev/null \\\n'
    '    || echo "playerbot-migrate: could not pin the shop keepers to the first channel" >&2\n'
)

WORLD_RATES = r"""
# The rates of a world that has never had any, before the cores start. On this
# engine a rate is not a rewritten table but three event flags (player.quest,
# dwPID 0) that CQuestManager::SetEventFlag maps onto CHARACTER_MANAGER's
# multipliers, and until somebody presses "Zastosuj" in the panel those rows do
# not exist - so a fresh world ran at 100% whatever the panel's own table said.
# It said 650% experience, seeded into web_admin_rates by the panel's schema
# for a test cycle long ago, and that number reached every player as a promise
# the game never kept: the panel showed it, the bots levelled at 100%, and the
# first press of the button - even without touching a field - was what made it
# real (NerrVoVy, 20 September).
#
# So the numbers the launcher asked for are written here, into both places at
# once, and only while the flags are absent: a world that has been set from the
# panel is never touched again, whatever this file says. That is also why the
# panel's schema no longer seeds the table.
rate_ok() {
    # A newline, because awk reads no record from an empty input and
    # the substitution would then be empty - not a number, so the SQL
    # below would be a syntax error rather than a default.
    printf '%s\n' "$1" | tr -d ' \r' | awk -v d="$2" '{ v = $1 + 0; if (v < 1 || v > 10000) v = d; printf "%d", v }'
}
r_exp=$(rate_ok "${M2_RATE_EXP:-100}" 100)
r_drop=$(rate_ok "${M2_RATE_DROP:-100}" 100)
r_yang=$(rate_ok "${M2_RATE_YANG:-100}" 100)
db -e "CREATE TABLE IF NOT EXISTS player.web_admin_rates (
        name VARCHAR(24) PRIMARY KEY, value INT NOT NULL DEFAULT 100);" >/dev/null 2>&1 \
    || echo "[playerbot-migrate] WARNING: could not make player.web_admin_rates" >&2
rates_set=$(db -e "SELECT COUNT(*) FROM player.quest WHERE dwPID = 0 AND szName = 'mob_exp';" 2>/dev/null || echo x)
if [ "$rates_set" = "x" ]; then
    echo "[playerbot-migrate] WARNING: could not read the rate flags; leaving them alone" >&2
elif [ "$rates_set" = "0" ]; then
    if db -e "REPLACE INTO player.quest (dwPID, szName, szState, lValue) VALUES
            (0, 'mob_exp', '', $r_exp),   (0, 'mob_exp_buyer', '', $r_exp),
            (0, 'mob_item', '', $r_drop), (0, 'mob_item_buyer', '', $r_drop),
            (0, 'mob_gold', '', $r_yang), (0, 'mob_gold_buyer', '', $r_yang);
        REPLACE INTO player.web_admin_rates (name, value) VALUES
            ('exp', $r_exp), ('drop', $r_drop), ('yang', $r_yang);"; then
        echo "[playerbot-migrate] fresh world: experience ${r_exp}%, item drops ${r_drop}%, yang ${r_yang}%"
    else
        echo "[playerbot-migrate] WARNING: could not write the fresh world's rates" >&2
    fi
    # And whether that world's bots wait at the door. The core reads this file
    # on the weights clock and, the first time it is asked, before its own
    # first tick - the bootstrap spawns a cohort before any tick runs, so a
    # file written afterwards would hold a door the crowd had already walked
    # through. Written only for a fresh world, because on any other one it is
    # the panel's button that owns it.
    if [ -d /opt/m2spool ]; then
        if [ "$(printf '%s' "${M2_PLAYERBOT_START_HELD:-0}" | tr -d ' \r')" = "1" ]; then
            printf '1\n' > /opt/m2spool/playerbot_hold 2>/dev/null \
                && echo "[playerbot-migrate] the bots will wait at the door until you let them in" \
                || echo "[playerbot-migrate] WARNING: could not hold the bots (/opt/m2spool not writable)" >&2
        else
            printf '0\n' > /opt/m2spool/playerbot_hold 2>/dev/null || true
        fi
        chmod 0664 /opt/m2spool/playerbot_hold 2>/dev/null || true
    fi
fi
"""

WORLD_DIFFICULTY = (
    '\n'
    "# The world's difficulty, as event flags in seconds (player.quest, dwPID 0 -\n"
    "# what the db core loads at boot and pushes to every game core, the package's\n"
    '# own idiom for a world-wide switch). quest/m2_difficulty.lua reads them: the\n'
    "# Biologist's wait between two hand-ins and the stable keeper's four waits\n"
    '# (the pony, each Horse Book, the medal trainings of 1-10 and of 11-19). The\n'
    "# presets scale the package's own numbers - hard is what it shipped with,\n"
    '# medium a third of it, easy none (what 2.0.55 and 2.0.56 gave everybody) -\n'
    "# and custom takes the hour counts from .env, the horse's for every wait.\n"
    "# The wait between two skill books is the engine's (m2_book_wait, playerbotify\n"
    "# apply_book_wait) and the bots' own (m2_bot_book_wait), the package's 21 hours\n"
    '# on hard (drip9660, 23 September).\n'
    '# Written before the seed, which may leave early on a foreign cohort.\n'
    '#\n'
    "# The classic panel's difficulty card sets the same flags live, so .env is\n"
    '# applied only when it changed since the last start (m2_difficulty_env holds\n'
    '# what it said): a change made in the panel survives a restart until the\n'
    "# launcher's difficulty is changed, and the one changed last is the one kept.\n"
    'difficulty=$(printf \'%s\' "${M2_DIFFICULTY:-easy}" | tr \'A-Z\' \'a-z\' | tr -d \' \\r\')\n'
    'hours_to_seconds() {\n'
    '    printf \'%s\\n\' "$1" | tr -d \' \\r\' | awk \'{ h = $1 + 0; if (h < 0) h = 0; if (h > 8760) h = 8760; printf "%d", h * 3600 }\'\n'
    '}\n'
    'case "$difficulty" in\n'
    '    medium) dlevel=1; bio=28800; hbuy=14400; hup=14400; htr=21600; htr2=25200; book=25200; botbook=25200 ;;\n'
    '    hard)   dlevel=2; bio=86400; hbuy=43200; hup=43200; htr=64800; htr2=75600; book=75600; botbook=75600 ;;\n'
    '    custom)\n'
    '        dlevel=3\n'
    '        bio=$(hours_to_seconds "${M2_BIOLOGIST_WAIT_HOURS:-0}")\n'
    '        hbuy=$(hours_to_seconds "${M2_HORSE_WAIT_HOURS:-0}")\n'
    '        hup=$hbuy; htr=$hbuy; htr2=$hbuy\n'
    '        book=$(hours_to_seconds "${M2_BOOK_WAIT_HOURS:-0}")\n'
    '        botbook=$(hours_to_seconds "${M2_BOT_BOOK_WAIT_HOURS:-0}") ;;\n'
    '    *)      difficulty=easy; dlevel=0; bio=0; hbuy=0; hup=0; htr=0; htr2=0; book=0; botbook=0 ;;\n'
    'esac\n'
    'dsig=$(printf \'%s|%s|%s|%s|%s|%s|%s|%s|%s\' "$difficulty" "$bio" "$hbuy" "$hup" "$htr" "$htr2" "$book" "$botbook" 1 | cksum | awk \'{ print $1 % 2000000000 }\')\n'
    'dprev=$(db -N -e "SELECT lValue FROM player.quest WHERE dwPID = 0 AND szName = \'m2_difficulty_env\' LIMIT 1" 2>/dev/null | tr -d \' \\r\')\n'
    'if [ -n "$dprev" ] && [ "$dprev" = "$dsig" ]; then\n'
    '    echo "[playerbot-migrate] difficulty: .env unchanged since the last start - the flags stay as the panel or the last start left them"\n'
    'elif db -e "REPLACE INTO player.quest (dwPID, szName, szState, lValue) VALUES\n'
    "        (0, 'm2_difficulty', '', $dlevel),\n"
    "        (0, 'm2_biologist_wait', '', $bio),\n"
    "        (0, 'm2_horse_buy_wait', '', $hbuy),\n"
    "        (0, 'm2_horse_upgrade_wait', '', $hup),\n"
    "        (0, 'm2_horse_train_wait', '', $htr),\n"
    "        (0, 'm2_horse_train2_wait', '', $htr2),\n"
    "        (0, 'm2_book_wait', '', $book),\n"
    "        (0, 'm2_bot_book_wait', '', $botbook),\n"
    '        (0, \'m2_difficulty_env\', \'\', $dsig);"; then\n'
    '    echo "[playerbot-migrate] difficulty: $difficulty (Biologist wait ${bio}s, horse: buy ${hbuy}s upgrade ${hup}s train ${htr}s/${htr2}s, books: players ${book}s bots ${botbook}s)"\n'
    'else\n'
    '    echo "[playerbot-migrate] WARNING: could not write the difficulty flags; the quests keep the last ones" >&2\n'
    'fi\n'
)


STARTER_CHEST = (
    '\n'
    "# Whether a player's new character gets the apprentice chest at its first\n"
    "# login (starter_chest.quest reads m2_starter_chest_off). Asked with the\n"
    "# rates when a world is made (seban latino's idea, 22 September); on unless\n"
    "# .env says M2_STARTER_CHEST=0. An event flag like the difficulty, so a\n"
    "# change reaches the quests at the next start.\n"
    'starter=$(printf \'%s\' "${M2_STARTER_CHEST:-1}" | tr \'A-Z\' \'a-z\' | tr -d \' \\r\')\n'
    'case "$starter" in\n'
    '    0|off|no|false) starter_off=1 ;;\n'
    '    *)              starter_off=0 ;;\n'
    'esac\n'
    'if db -e "REPLACE INTO player.quest (dwPID, szName, szState, lValue) VALUES\n'
    "        (0, 'm2_starter_chest_off', '', $starter_off);\"; then\n"
    '    echo "[playerbot-migrate] apprentice chest for new characters: $([ "$starter_off" = 1 ] && echo off || echo on)"\n'
    'else\n'
    '    echo "[playerbot-migrate] WARNING: could not write the apprentice chest flag; the quest keeps the last one" >&2\n'
    'fi\n'
    "# A bot's apprentice chest is the seed's - Skrzynia Ucznia I lies in its\n"
    '# bag from the start - and the quest cannot tell a bot from a person, so a\n'
    '# bot still at level five or under at its first login got a second one: on\n'
    '# a new world, the whole cohort (Iwakura, 26 September). The seed marks the\n'
    '# bots it creates; this marks the ones seeded before it did, and changes\n'
    '# nothing on a start that finds them marked. A companion is one of these\n'
    '# identities, so a player gets no chest by making one either.\n'
    'if [ "$(db -e "SELECT COUNT(*) FROM information_schema.tables\n'
    '              WHERE table_schema=\'common\' AND table_name=\'playerbot_seed_state\';" 2>/dev/null)" = 1 ]; then\n'
    '    if db -e "INSERT INTO player.quest (dwPID, szName, szState, lValue)\n'
    "            SELECT l.pid, 'starter_chest', 'given', 1\n"
    '              FROM common.playerbot_seed_state AS l\n'
    "             WHERE l.state IN ('complete','adopted')\n"
    '            ON DUPLICATE KEY UPDATE lValue = GREATEST(lValue, 1);"; then\n'
    '        echo "[playerbot-migrate] apprentice chest: a bot\'s is the one the seed gave it"\n'
    '    else\n'
    '        echo "[playerbot-migrate] WARNING: could not mark the bots\' apprentice chest as given" >&2\n'
    '    fi\n'
    'fi\n'
)


ITEMSHOP_MOUNTS = r"""
# The world's mounts, in the in-game ItemShop (CItemShopManager, read by the
# db core out of common.itemshop_items at boot), in the range 801-899 that the
# client's shop window shows as "Wierzchowce" (uiitemshop.py).
#
# 2.1.2 put them at 701-799 and listed only ITEM_COSTUME / COSTUME_MOUNT (28/2).
# Both were wrong for this package, measured on a player's world (23
# September): its own Dragon Mark goods already stand at 701-713, so the tab
# showed them a second time beside "Smocze znaki", and world.item_proto holds
# no mount costume at all - costumes are subtype 0 (307) and 1 (395) only. Its
# mounts are the ride seals, ITEM_UNIQUE / UNIQUE_SPECIAL_RIDE (16/2): worn in a
# unique slot, EquipItem hands one to the quest as sig_use and mount_seals.quest
# puts the rider on the animal. The four war seals are the ones whose animals
# the world's mob_proto names (20115-20118); the other seals wait for theirs.
#
# A ride seal's value0 is its time in minutes, counted by unique_expire_event
# only while it is worn, and ITEM_MANAGER::CreateItem copies it into the new
# seal; M2_ITEMSHOP_MOUNT_HOURS sets it for the four (30 by default, the
# package's 28800 minutes being twenty days). A seal already made keeps the
# time it was made with. A costume mount, on a world that has one, is listed
# as before. A mount already in the shop, at any index, is left where it is -
# except a costume mount 2.1.2 put in the Dragon Mark range, which moves; one
# the operator deleted comes back at the next start, unless .env says
# M2_ITEMSHOP_MOUNTS=0. The price is M2_ITEMSHOP_MOUNT_PRICE Dragon Coins.
#
# The table's columns are not in any file this project carries (the db core's
# loader ships only as a binary), so they are read from information_schema and
# a new row is a copy of the shop's first hairstyle row - the kind the bots buy
# and wear every day - with the index, the item, the count and the price put in
# and any promotion or auction number cleared. A table this cannot read, or a
# shop with no hairstyle to copy, is left untouched and says so.
ishop_on=$(printf '%s' "${M2_ITEMSHOP_MOUNTS:-1}" | tr 'A-Z' 'a-z' | tr -d ' \r')
ishop_price=$(printf '%s\n' "${M2_ITEMSHOP_MOUNT_PRICE:-500}" | tr -d ' \r' | awk '{ v = $1 + 0; if (v < 1 || v > 100000) v = 500; printf "%d", v }')
ishop_hours=$(printf '%s\n' "${M2_ITEMSHOP_MOUNT_HOURS:-30}" | tr -d ' \r' | awk '{ v = $1 + 0; if (v < 1 || v > 8760) v = 30; printf "%d", v }')
# The seals mount_seals.quest can put a rider on; keep the two lists together.
ishop_seals='71125, 71126, 71127, 71128'
# What the tab lists: a mount costume whose apply names a mount, or a ride seal
# the quest knows.
ishop_mount_items="((p.type = 28 AND p.subtype = 2
                      AND EXISTS (SELECT 1 FROM world.mob_proto AS m
                                   WHERE m.vnum >= 20000 AND m.vnum IN (p.applyvalue0, p.applyvalue1, p.applyvalue2)))
                  OR (p.type = 16 AND p.subtype = 2 AND p.vnum IN ($ishop_seals)))"
case "$ishop_on" in
    0|off|no|false)
        echo "[playerbot-migrate] ItemShop mounts: left to the operator (M2_ITEMSHOP_MOUNTS=0)"
        ;;
    *)
        bq='`'
        # One line a column, '|' between the fields: a tab is IFS whitespace, and
        # read would fold the empty extra of an ordinary column away.
        ishop_cols=$(db -e "SELECT CONCAT(column_name, '|', extra, '|', data_type) FROM information_schema.columns
                             WHERE table_schema = 'common' AND table_name = 'itemshop_items'
                             ORDER BY ordinal_position;" 2>/dev/null || true)
        ishop_col() {
            for want in "$@"; do
                hit=$(printf '%s\n' "$ishop_cols" | awk -F'|' -v w="$want" 'tolower($1) == w { print $1; exit }')
                if [ -n "$hit" ]; then
                    printf '%s' "$hit"
                    return 0
                fi
            done
            return 1
        }
        c_idx=$(ishop_col index item_index idx id || true)
        c_vnum=$(ishop_col vnum item_vnum || true)
        c_count=$(ishop_col count item_count amount || true)
        c_price=$(ishop_col price item_price || true)
        if [ -z "$ishop_cols" ]; then
            echo "[playerbot-migrate] ItemShop mounts: no common.itemshop_items on this world; nothing listed"
        elif [ -z "$c_idx" ] || [ -z "$c_vnum" ] || [ -z "$c_count" ] || [ -z "$c_price" ]; then
            echo "[playerbot-migrate] WARNING: ItemShop mounts: common.itemshop_items has columns this step does not know ($(printf '%s\n' "$ishop_cols" | awk -F'|' '{ printf "%s%s", s, $1; s = " " }')); nothing listed" >&2
        else
            ins=''
            sel=''
            while IFS='|' read -r col extra dtype; do
                [ -n "$col" ] || continue
                lc=$(printf '%s' "$col" | tr 'A-Z' 'a-z')
                if [ "$col" = "$c_idx" ]; then
                    v='n.idx'
                elif [ "$col" = "$c_vnum" ]; then
                    v='n.vnum'
                elif [ "$col" = "$c_count" ]; then
                    v='1'
                elif [ "$col" = "$c_price" ]; then
                    v="$ishop_price"
                else
                    case "$extra" in
                        *auto_increment*) v='NULL' ;;
                        *)
                            case "$lc:$dtype" in
                                *promo*:*int|*auction*:*int|*promo*:decimal|*auction*:decimal) v='0' ;;
                                *) v="t.$bq$col$bq" ;;
                            esac
                            ;;
                    esac
                fi
                ins="$ins${ins:+, }$bq$col$bq"
                sel="$sel${sel:+, }$v"
            done <<EOF
$ishop_cols
EOF
            I="$bq$c_idx$bq"
            V="$bq$c_vnum$bq"
            # The seals' worn time, in minutes. Only while it is worn does it
            # count (value2 = 0), and only a seal made from now on takes it.
            db -e "UPDATE world.item_proto SET value0 = $ishop_hours * 60
                    WHERE type = 16 AND subtype = 2 AND value2 = 0 AND vnum IN ($ishop_seals)
                      AND value0 <> $ishop_hours * 60;" 2>/dev/null \
                || echo "[playerbot-migrate] WARNING: ItemShop mounts: the seals' time could not be set" >&2
            # 2.1.2 listed mount costumes at 701-799, where the package keeps its
            # Dragon Mark goods; a row of ours there goes, to come back at 801.
            # (A multi-table DELETE with an alias wants a default database, and
            # the migrator runs with none.)
            db -e "DELETE FROM common.itemshop_items
                    WHERE $I BETWEEN 701 AND 799
                      AND $V IN (SELECT vnum FROM world.item_proto WHERE type = 28 AND subtype = 2);" 2>/dev/null || true
            mounts_in_world=$(db -e "SELECT COUNT(*) FROM world.item_proto AS p
                 WHERE $ishop_mount_items;" 2>/dev/null || echo x)
            ishop_template=$(db -e "SELECT MIN(i.$I) FROM common.itemshop_items AS i
                                      JOIN world.item_proto AS h ON h.vnum = i.$V
                                     WHERE h.type = 28 AND h.subtype = 1;" 2>/dev/null | tr -d '[:space:]')
            case "$ishop_template" in
                ''|NULL|*[!0-9]*) ishop_template= ;;
            esac
            if [ -z "$ishop_template" ]; then
                echo "[playerbot-migrate] WARNING: ItemShop mounts: the shop has no hairstyle row to copy; nothing listed" >&2
            elif ishop_added=$(db -e "
                INSERT INTO common.itemshop_items ($ins)
                SELECT $sel
                  FROM (SELECT c.vnum, b.base + ROW_NUMBER() OVER (ORDER BY c.vnum) AS idx
                          FROM (SELECT p.vnum FROM world.item_proto AS p
                                 WHERE $ishop_mount_items
                                   AND p.vnum NOT IN (SELECT $V FROM common.itemshop_items)) AS c
                         CROSS JOIN (SELECT COALESCE(MAX($I), 800) AS base FROM common.itemshop_items
                                      WHERE $I BETWEEN 801 AND 899) AS b) AS n
                  JOIN common.itemshop_items AS t ON t.$I = $ishop_template
                 WHERE n.idx <= 899;
                SELECT ROW_COUNT();" 2>/tmp/ishop_mounts.err); then
                ishop_added=$(printf '%s' "$ishop_added" | tr -d '[:space:]')
                listed=$(db -e "SELECT COUNT(*) FROM common.itemshop_items WHERE $I BETWEEN 801 AND 899;" 2>/dev/null || echo '?')
                echo "[playerbot-migrate] ItemShop mounts: ${mounts_in_world} in the world, ${ishop_added:-0} added, ${listed} in the Wierzchowce tab (${ishop_price} Dragon Coins each, ${ishop_hours} h worn)"
            else
                echo "[playerbot-migrate] WARNING: ItemShop mounts could not be listed:" >&2
                head -3 /tmp/ishop_mounts.err >&2
            fi
        fi
        ;;
esac
"""
GAME_FEATURES = (
    '\n'
    "# Whether the world is played with Auto Lowy and with the companion\n"
    "# (Towarzysz): the launcher's difficulty window writes M2_AUTOHUNT and\n"
    "# M2_SIDEKICK, both on unless .env says 0 (25 September, for Drip's\n"
    "# COOP without the auto hunt). Off, the server refuses the hunt's target\n"
    "# and drop (m2_autohunt_off, playerbotify apply_auto_hunt_switch) and\n"
    "# sends no Towarzysz letter, refuses its command and keeps companions out\n"
    "# of the world (m2_sidekick_off). Event flags like the difficulty, so a\n"
    "# change reaches the cores at the next start.\n"
    'feature_off() {\n'
    '    case "$(printf \'%s\' "$1" | tr \'A-Z\' \'a-z\' | tr -d \' \\r\')" in\n'
    '        0|off|no|false) echo 1 ;;\n'
    '        *)              echo 0 ;;\n'
    '    esac\n'
    '}\n'
    'autohunt_off=$(feature_off "${M2_AUTOHUNT:-1}")\n'
    'sidekick_off=$(feature_off "${M2_SIDEKICK:-1}")\n'
    'if db -e "REPLACE INTO player.quest (dwPID, szName, szState, lValue) VALUES\n'
    "        (0, 'm2_autohunt_off', '', $autohunt_off),\n"
    "        (0, 'm2_sidekick_off', '', $sidekick_off);\"; then\n"
    '    echo "[playerbot-migrate] Auto Lowy: $([ "$autohunt_off" = 1 ] && echo off || echo on), companions: $([ "$sidekick_off" = 1 ] && echo off || echo on)"\n'
    'else\n'
    '    echo "[playerbot-migrate] WARNING: could not write the Auto Lowy and companion flags; the cores keep the last ones" >&2\n'
    'fi\n'
)


def sql_rows(rows, per_line=8):
    """A tuple of tuples as the SQL list of row constructors, a few to a line."""
    parts = ['(' + ', '.join(str(v) for v in r) + ')' for r in rows]
    lines = [', '.join(parts[i:i + per_line]) for i in range(0, len(parts), per_line)]
    return (',\n' + ' ' * 12).join(lines)


GROTTO_CATACOMB_RESCUE = (
    "# 2.2.21 opened the Grotto of Exile (72, 73) and the Devil's Catacomb (216)\n"
    "# and no client of that time could stand on any of them: the grotto's maps\n"
    '# stood in the season2 pack without the maps/ the client looks under, and the\n'
    "# Catacomb's map was in no pack at all (client 2.0.39 carries all three, its\n"
    '# season2 pack taken whole from upstream\'s client). Entering one closed the\n'
    '# client, and a character saved there could not log in again ("postac jest\n'
    '# zbugowana", Iwakura, 26 September). Once: every character of a person\n'
    '# saved on one of them or in an\n'
    '# instance of one is put where the way out leads - by Koe-Pung in Orc Valley\n'
    "# (284200, 810600, the target of the grotto's exit 10078) or before the\n"
    "# Catacomb's Guardian in Hwang Temple (591400, 99200, the quest's own exit).\n"
    '# A bot has no client and stays where it is. Before the game container starts\n'
    '# (a character left there minutes before an update may still be written back\n'
    "# by the old db core's cache; the new client can stand there anyway).\n"
    'rescue_done=$(db -e "SELECT COUNT(*) FROM player.playerbot_migrations WHERE name = \'grotto_catacomb_client_2221\';" 2>/dev/null || echo x)\n'
    'if [ "$rescue_done" = "0" ]; then\n'
    '    if rescue_out=$(db -e "\n'
    '        START TRANSACTION;\n'
    '        UPDATE player.player AS p JOIN account.account AS a ON a.id = p.account_id\n'
    '           SET p.map_index = 64, p.x = 284200, p.y = 810600,\n'
    '               p.exit_map_index = 64, p.exit_x = 284200, p.exit_y = 810600\n'
    "         WHERE a.login NOT LIKE 'playerbot%'\n"
    '           AND (p.map_index IN (72, 73) OR p.map_index BETWEEN 720000 AND 739999);\n'
    '        SELECT ROW_COUNT();\n'
    '        UPDATE player.player AS p JOIN account.account AS a ON a.id = p.account_id\n'
    '           SET p.map_index = 65, p.x = 591400, p.y = 99200,\n'
    '               p.exit_map_index = 65, p.exit_x = 591400, p.exit_y = 99200\n'
    "         WHERE a.login NOT LIKE 'playerbot%'\n"
    '           AND (p.map_index = 216 OR p.map_index BETWEEN 2160000 AND 2169999);\n'
    '        SELECT ROW_COUNT();\n'
    "        INSERT IGNORE INTO player.playerbot_migrations (name, done_at) VALUES ('grotto_catacomb_client_2221', NOW());\n"
    '        COMMIT;\n'
    '    "); then\n'
    '        rescue_grotto=$(printf \'%s\\n\' "$rescue_out" | awk \'NR == 1\')\n'
    '        rescue_catacomb=$(printf \'%s\\n\' "$rescue_out" | awk \'NR == 2\')\n'
    '        echo "[playerbot-migrate] characters moved out of maps no old client could load: ${rescue_grotto:-0} from the Grotto of Exile, ${rescue_catacomb:-0} from the Devil\'s Catacomb"\n'
    '    else\n'
    '        echo "[playerbot-migrate] WARNING: could not move the characters out of the Grotto and the Catacomb" >&2\n'
    '    fi\n'
    'fi\n'
)


def guild_lands_block():
    """The migrator's step that takes the package's guild lands off the world."""
    return (
        '# The package\'s player dump carries the guild lands and buildings of the\n'
        '# server it was taken from - %d player.guild_land rows and %d player.object\n'
        '# rows - and none of the guilds they belong to. The engine stands its land\n'
        '# agent (NPC 20040) only on a land nobody owns (building::CManager, at boot),\n'
        '# so those lands could never be bought and their buildings stood on ground\n'
        '# nobody held, while a bot guild founded later under one of those numbers\n'
        '# (2, 3, 5, ...) held a land and buildings it never paid for ("stoja juz\n'
        '# budynki, pomimo ze teren nie jest zajety", Mat, 19 September; NerrVoVy\n'
        '# cleared his by hand). Once, and the dump\'s own rows exactly: a land a\n'
        '# player\'s guild has bought and the buildings it put up since (ids past the\n'
        '# dump\'s last) are left alone. Before the game container starts, because the\n'
        '# db core reads both at boot.\n'
        'lands_done=$(db -e "SELECT COUNT(*) FROM player.playerbot_migrations WHERE name = \'package_guild_lands_2081\';" 2>/dev/null || echo x)\n'
        'if [ "$lands_done" = "0" ]; then\n'
        '    if lands_out=$(db -e "\n'
        '        START TRANSACTION;\n'
        '        DELETE FROM player.object WHERE (id, land_id, vnum) IN (\n'
        '            %s);\n'
        '        SELECT ROW_COUNT();\n'
        '        DELETE FROM player.guild_land WHERE (land_id, guild_id) IN (\n'
        '            %s);\n'
        '        SELECT ROW_COUNT();\n'
        '        INSERT IGNORE INTO player.playerbot_migrations (name, done_at) VALUES (\'package_guild_lands_2081\', NOW());\n'
        '        COMMIT;\n'
        '    "); then\n'
        '        lands_objects=$(printf \'%%s\\n\' "$lands_out" | awk \'NR == 1\')\n'
        '        lands_rows=$(printf \'%%s\\n\' "$lands_out" | awk \'NR == 2\')\n'
        '        echo "[playerbot-migrate] the package\'s guild lands cleared: ${lands_rows:-0} land(s), ${lands_objects:-0} building(s)"\n'
        '    else\n'
        '        echo "[playerbot-migrate] WARNING: could not clear the package\'s guild lands" >&2\n'
        '    fi\n'
        'fi\n'
    ) % (len(PACKAGE_GUILD_LANDS), len(PACKAGE_GUILD_OBJECTS),
         sql_rows(PACKAGE_GUILD_OBJECTS), sql_rows(PACKAGE_GUILD_LANDS))


def main():
    os.makedirs(DST, exist_ok=True)
    s = io.open(os.path.join(SRC, 'apply.sh'), encoding='utf-8', newline='').read()
    assert '\r' not in s

    n = s.count("table_name='speed_hack'")
    assert n == 1, n
    s = s.replace("table_name='speed_hack'", "table_name='hack_log'")

    pat = re.compile(r"\(1, 3, 4, 5, 21, 23, 24, 25, 41, 43, 44, 45,\s+108, 109, 61, 63, 64, 104, 65, 71\)")
    s, n = pat.subn('(' + HOSTED + ')', s)
    assert n == 2, n

    s = s.replace('echo "[playerbot-migrate] waiting for the complete r40250 schema"',
                  'echo "[playerbot-migrate] waiting for the complete mt2009 schema"')

    # A world initialised before initdb widened account.social_id gets the
    # same ALTER here, once; see 10-import-dumps.sh for why.
    anchor = 'itemshop_schema=/opt/playerbot/itemshop_schema.sql\n'
    assert s.count(anchor) == 1
    s = s.replace(anchor,
                  'social_len=$(db -e "\n'
                  '    SELECT CHARACTER_MAXIMUM_LENGTH FROM information_schema.columns\n'
                  '     WHERE table_schema=\'account\' AND table_name=\'account\' AND column_name=\'social_id\';\n'
                  '")\n'
                  'if [ -n "$social_len" ] && [ "$social_len" -lt 18 ] 2>/dev/null; then\n'
                  '    echo "[playerbot-migrate] widening account.social_id from $social_len to 18 characters"\n'
                  '    db -e "ALTER TABLE account.account MODIFY social_id VARCHAR(18) NOT NULL DEFAULT \'\';"\n'
                  'fi\n'
                  '# The ItemShop reads mileage and jackpot off the account; this schema has\n'
                  '# cash alone. IF NOT EXISTS keeps it a no-op after the first time.\n'
                  'db -e "ALTER TABLE account.account ADD COLUMN IF NOT EXISTS mileage INT NOT NULL DEFAULT 0;"\n'
                  'db -e "ALTER TABLE account.account ADD COLUMN IF NOT EXISTS jackpot INT NOT NULL DEFAULT 0;"\n'
                  '# Fishing from thirty, which is what the wiki says and what the operator\n'
                  '# asked for. This line shipped fifty in three places and moving two was not\n'
                  '# enough: CHARACTER::fishing() (playerbotify.py lowers it), the AI gate, and\n'
                  '# the rod LIMIT_LEVEL - the one that refuses the equip, so a bot of thirty\n'
                  '# could neither wear a rod nor be drawn as an angler. item_proto is read out\n'
                  '# of world.item_proto here (PROTO_FROM_DB = 1), which is why this sticks;\n'
                  '# idempotent, and it touches only rods still carrying the old fifty.\n'
                  'db -e "UPDATE world.item_proto SET limitvalue0 = 30 WHERE type = 13 AND limittype0 = 1 AND limitvalue0 = 50;"\n'
                  '# Maska Sabaha left the world with the Hwang curse (playerbotify\n'
                  '# apply_hwang_curse_removed, the share step of the game Dockerfile): the shop\n'
                  '# that sold one sells it no more. The db core reads the shops at boot, so this\n'
                  '# is live on the next start; idempotent.\n'
                  'db -e "DELETE FROM world.shop_item WHERE item_vnum IN (72731, 72735);"\n'
                  '# And nobody keeps one: every Maska Sabaha still in a bag, on a character, in a\n'
                  '# safebox or on a counter is removed (15 September, "usun" to the masks\n'
                  '# players already held). On every start, so a mask an old core still held while\n'
                  '# an update ran this beside it goes on the next one.\n'
                  'masks=$(db -e "DELETE FROM player.item WHERE vnum IN (72731, 72735); SELECT ROW_COUNT();" || echo x)\n'
                  'masks=$(printf \'%s\' "$masks" | tr -d \'[:space:]\')\n'
                  'if [ "$masks" = "x" ]; then\n'
                  '    echo "[playerbot-migrate] WARNING: could not remove the Maska Sabaha items" >&2\n'
                  'elif [ -n "$masks" ] && [ "$masks" != "0" ]; then\n'
                  '    echo "[playerbot-migrate] removed $masks Maska Sabaha item(s)"\n'
                  'fi\n'
                  '# The market of Shinsoo\'s and Jinno\'s villages moved onto the kingdom\'s guard\n'
                  '# in 2.0.52 (GetTownPitch, playerbot_empire_rules.h), and nothing would ever\n'
                  '# have moved the shops standing round the old pitch: an offline shop stands\n'
                  '# where its keeper stood when it was opened (OpenOfflineShop takes the\n'
                  '# character\'s position, a reopen included) and a keeper walks to its shop to\n'
                  '# serve it. So each bot\'s shop of the old ring is carried across by the\n'
                  '# distance between the two pitches, which keeps the ring\'s shape and spacing,\n'
                  '# and pulled in to 1650 of the guard where it stood further out - the ring of\n'
                  '# 400 to 1700 round each guard is open ground inside the safe zone on\n'
                  '# server_attr. A shop already inside the new ring and outside the old one\n'
                  '# belongs to the new pitch and stays. Once, marked in\n'
                  '# player.playerbot_migrations in the same transaction as the move; on every\n'
                  '# start after that only a bot\'s shop still within 2000 of an old pitch and more\n'
                  '# than 2000 from the new one moves - a keeper that reopened on the old spot\n'
                  '# while an update ran this beside the old game container (update.sh does not\n'
                  '# stop the game first). The db core writes a position only when a shop is\n'
                  '# opened or moved, so an old core cannot write the moved ones back. A player\'s\n'
                  '# own shop is left where its owner put it. Before the game container starts,\n'
                  '# because the db core reads the shops at boot.\n'
                  'db -e "CREATE TABLE IF NOT EXISTS player.playerbot_migrations (name VARCHAR(64) NOT NULL PRIMARY KEY, done_at DATETIME NOT NULL) ENGINE=InnoDB;"\n'
                  'pitch_done=$(db -e "SELECT COUNT(*) FROM player.playerbot_migrations WHERE name = \'pitch_on_guard_2052\';" 2>/dev/null || echo x)\n'
                  'case "$pitch_done" in\n'
                  '    0) pitch_near=1700; pitch_far=1700 ;;\n'
                  '    1) pitch_near=-1; pitch_far=2000 ;;\n'
                  '    *) pitch_near= ;;\n'
                  'esac\n'
                  'if [ -n "$pitch_near" ]; then\n'
                  '    if pitch_moved=$(db -e "\n'
                  '        CREATE TEMPORARY TABLE player.tmp_pitch_moves AS\n'
                  '        SELECT d.owner,\n'
                  '               d.nx + ROUND(d.dx * LEAST(1, 1650 / GREATEST(1, d.d_old))) AS tx,\n'
                  '               d.ny + ROUND(d.dy * LEAST(1, 1650 / GREATEST(1, d.d_old))) AS ty\n'
                  '          FROM (SELECT s.owner, m.nx, m.ny,\n'
                  '                       CAST(s.x AS SIGNED) - m.ox AS dx,\n'
                  '                       CAST(s.y AS SIGNED) - m.oy AS dy,\n'
                  '                       SQRT(POW(CAST(s.x AS SIGNED) - m.ox, 2) + POW(CAST(s.y AS SIGNED) - m.oy, 2)) AS d_old,\n'
                  '                       SQRT(POW(CAST(s.x AS SIGNED) - m.nx, 2) + POW(CAST(s.y AS SIGNED) - m.ny, 2)) AS d_new\n'
                  '                  FROM player.ikashop_offlineshop AS s\n'
                  '                  JOIN player.player AS p ON p.id = s.owner\n'
                  '                  JOIN account.account AS a ON a.id = p.account_id\n'
                  '                  JOIN (SELECT 1 AS map, 473625 AS ox, 954925 AS oy, 474325 AS nx, 954225 AS ny\n'
                  '                        UNION ALL SELECT 3, 353987, 880012, 353025, 882325\n'
                  '                        UNION ALL SELECT 41, 961212, 270162, 959925, 268825\n'
                  '                        UNION ALL SELECT 43, 865500, 244975, 863425, 246025) AS m ON m.map = s.map\n'
                  '                 WHERE a.login LIKE \'playerbot%\') AS d\n'
                  '         WHERE d.d_old <= 2000 AND (d.d_old <= $pitch_near OR d.d_new > $pitch_far);\n'
                  '        START TRANSACTION;\n'
                  '        UPDATE player.ikashop_offlineshop AS s\n'
                  '          JOIN player.tmp_pitch_moves AS t ON t.owner = s.owner\n'
                  '           SET s.x = t.tx, s.y = t.ty;\n'
                  '        SELECT ROW_COUNT();\n'
                  '        INSERT IGNORE INTO player.playerbot_migrations (name, done_at) VALUES (\'pitch_on_guard_2052\', NOW());\n'
                  '        COMMIT;\n'
                  '        DROP TEMPORARY TABLE player.tmp_pitch_moves;\n'
                  '    "); then\n'
                  '        pitch_moved=$(printf \'%s\' "$pitch_moved" | tr -d \'[:space:]\')\n'
                  '        if [ "${pitch_moved:-0}" != "0" ]; then\n'
                  '            echo "[playerbot-migrate] $pitch_moved bot offline shop(s) in Yongan, Jayang, Pyongmoo and Bakra carried onto the guard\'s square"\n'
                  '        fi\n'
                  '    else\n'
                  '        echo "[playerbot-migrate] WARNING: could not move the bots\' offline shops onto the new pitches" >&2\n'
                  '    fi\n'
                  'fi\n'
                  '# fish_log came from r40250\'s dump and has that engine\'s eight columns,\n'
                  '# while this one writes six - so every catch failed with errno 1136 and the\n'
                  '# table is empty on every 2.x world that ever ran. CREATE IF NOT EXISTS\n'
                  '# cannot repair a table that already exists with the wrong shape, so the\n'
                  '# old one is dropped here, before log_schema.sql below recreates it.\n'
                  '# Recognised by a column this engine never writes; a table already in the\n'
                  '# right shape, and whatever history it holds, is left alone.\n'
                  'fish_old=$(db -e "\n'
                  '    SELECT COUNT(*) FROM information_schema.columns\n'
                  '     WHERE table_schema=\'log\' AND table_name=\'fish_log\' AND column_name=\'map_index\';\n'
                  '" 2>/dev/null || echo 0)\n'
                  'if [ "$fish_old" = "1" ]; then\n'
                  '    echo "[playerbot-migrate] fish_log has the r40250 shape and cannot be written; rebuilding it"\n'
                  '    db -e "DROP TABLE IF EXISTS log.fish_log;"\n'
                  'fi\n'
                  '# The log tables the engine writes and the package dump lacks (port/logschemify.py).\n'
                  'if [ -s /opt/playerbot/log_schema.sql ]; then\n'
                  '    if db < /opt/playerbot/log_schema.sql 2>/tmp/logschema.err; then\n'
                  '        echo "[playerbot-migrate] log schema checked"\n'
                  '    else\n'
                  '        echo "[playerbot-migrate] WARNING: log schema failed:" >&2\n'
                  '        head -3 /tmp/logschema.err >&2\n'
                  '    fi\n'
                  'fi\n'
                  '\n' + anchor)
    # The package's guild lands, after the pitch step has made sure
    # player.playerbot_migrations exists.
    anchor = '# fish_log came from r40250\'s dump and has that engine\'s eight columns,\n'
    assert s.count(anchor) == 1
    s = s.replace(anchor, guild_lands_block() + GROTTO_CATACOMB_RESCUE + anchor)
    # The three steps that used to live only in the rendered file.
    for text, anchor in (
            (FISHING_PASS_AND_RING, '# Maska Sabaha left the world with the Hwang curse (playerbotify\n'),
            (GUILD_TIERS_AND_CHANNEL_PINS,
             'pitch_done=$(db -e "SELECT COUNT(*) FROM player.playerbot_migrations WHERE name = '
             '\'pitch_on_guard_2052\';" 2>/dev/null || echo x)\n'),
            (WORLD_RATES,
             '\necho "[playerbot-migrate] applying deterministic Playerbot seed (PID $first_pid..$last_pid)"\n'),
            (WORLD_DIFFICULTY,
             '\necho "[playerbot-migrate] applying deterministic Playerbot seed (PID $first_pid..$last_pid)"\n'),
            (STARTER_CHEST,
             '\necho "[playerbot-migrate] applying deterministic Playerbot seed (PID $first_pid..$last_pid)"\n'),
            (ITEMSHOP_MOUNTS,
             '\necho "[playerbot-migrate] applying deterministic Playerbot seed (PID $first_pid..$last_pid)"\n'),
            (GAME_FEATURES,
             '\necho "[playerbot-migrate] applying deterministic Playerbot seed (PID $first_pid..$last_pid)"\n')):
        assert s.count(anchor) == 1, anchor
        s = s.replace(anchor, text + anchor)
    # The four game masters of the tester account (gm_characters.sql), before
    # the generic "grant the oldest character" step: on a world whose admin
    # account is still empty they are created with their gmlist rows, on a
    # world where somebody plays on admin the file does nothing and the
    # generic step grants that character.
    anchor = ('# ---------------------------------------------------------------------------\n'
              '# A game master for the tester account.\n')
    assert s.count(anchor) == 1
    s = s.replace(anchor,
                  '# The tester account\'s own game masters, mt2009 only (see the file).\n'
                  'if [ -s /opt/playerbot/gm_characters.sql ]; then\n'
                  '    if gm_out=$(db < /opt/playerbot/gm_characters.sql 2>&1); then\n'
                  '        echo "[playerbot-migrate] $gm_out"\n'
                  '    else\n'
                  '        echo "[playerbot-migrate] WARNING: gm_characters.sql failed:" >&2\n'
                  '        echo "$gm_out" | head -3 >&2\n'
                  '    fi\n'
                  'fi\n'
                  '\n' + anchor)
    head = ('#!/bin/sh\n'
            '# Rendered for the mt2009 world by linux-port-mt2009/port/migratorify.py from\n'
            '# linux-port/docker/mariadb/playerbot/apply.sh. DO NOT EDIT; edit the original.\n')
    assert s.startswith('#!/bin/sh\n'), s[:40]
    s = head + s[len('#!/bin/sh\n'):]
    io.open(os.path.join(DST, 'apply.sh'), 'w', encoding='utf-8', newline='').write(s)
    print('migratorify: apply.sh rendered')

    shutil.copyfile(os.path.join(SRC, 'itemshop_schema.sql'), os.path.join(DST, 'itemshop_schema.sql'))
    print('migratorify: itemshop_schema.sql copied')


if __name__ == '__main__':
    main()
