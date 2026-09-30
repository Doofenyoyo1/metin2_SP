#!/bin/sh
# Rendered for the mt2009 world by linux-port-mt2009/port/migratorify.py from
# linux-port/docker/mariadb/playerbot/apply.sh. DO NOT EDIT; edit the original.
# Apply the tracked Playerbot seed once per Compose start. The SQL itself is
# idempotent and conflict-safe, so this also handles existing databases.
set -eu

: "${M2_DB_HOST:?M2_DB_HOST is required}"
: "${M2_DB_PORT:?M2_DB_PORT is required}"
: "${M2_DB_USER:?M2_DB_USER is required}"
: "${M2_DB_PASSWORD:?M2_DB_PASSWORD is required}"

strict=${PLAYERBOT_SEED_STRICT:-0}
case "$strict" in
    0|1) ;;
    *)
        echo "[playerbot-migrate] FATAL: PLAYERBOT_SEED_STRICT must be 0 or 1" >&2
        exit 1
        ;;
esac

expected_existing_bots=${PLAYERBOT_EXPECT_MIN_EXISTING_BOTS:-0}
case "$expected_existing_bots" in
    ''|*[!0-9]*)
        echo "[playerbot-migrate] FATAL: PLAYERBOT_EXPECT_MIN_EXISTING_BOTS must be a non-negative integer" >&2
        exit 1
        ;;
esac

seed=/opt/playerbot/playerbots_seed.sql
[ -s "$seed" ] || {
    echo "[playerbot-migrate] FATAL: $seed is missing or empty" >&2
    exit 1
}

db() {
    # mariadb(1) inherits MYSQL_PWD; MARIADB_PWD is not a client variable.
    # Keeping it out of argv avoids exposing the secret in `docker top`/ps.
    MYSQL_PWD="$M2_DB_PASSWORD" mariadb \
        --protocol=tcp \
        --host="$M2_DB_HOST" \
        --port="$M2_DB_PORT" \
        --user="$M2_DB_USER" \
        --default-character-set=latin1 \
        --batch --skip-column-names "$@"
}

# The official image can answer its healthcheck while its temporary first-run
# server is still importing dumps. Wait for tables at the end of every shipped
# dump, then require the item prototypes used by the starter rows.
echo "[playerbot-migrate] waiting for the complete mt2009 schema"
attempt=0
# Consecutive probes refused for authentication; see the check inside the
# loop. Reset by any probe that fails for a different reason, so a login
# that starts working is not held against it.
auth_failures=0
while :; do
    attempt=$((attempt + 1))
    # Keep the error instead of discarding it. A refused login looks exactly
    # like a schema that has not finished importing, and silently waiting five
    # minutes for a permissions problem to fix itself helps nobody.
    probe_err=/tmp/playerbot-probe.err
    ready=$(db -e "
        SELECT COUNT(*)
          FROM information_schema.tables
         WHERE (table_schema='account' AND table_name='account')
            OR (table_schema='common'  AND table_name='gmlist')
            OR (table_schema='player'  AND table_name IN
                ('player','player_index','item','item_proto','string'))
            OR (table_schema='log'     AND table_name='hack_log');
    " 2>"$probe_err" || true)
    if [ -s "$probe_err" ] && [ "$attempt" -eq 3 ]; then
        echo "[playerbot-migrate] the database is not answering yet:" >&2
        head -3 "$probe_err" >&2
    fi

    # A refused login is not a slow import, and waiting thirty minutes for it
    # to fix itself tells the operator the wrong thing twice: once by the wait
    # and once by the message at the end, which blames a large world still
    # recovering and suggests starting again. It never recovers - the password
    # in .env and the one the volume was initialised with simply differ.
    #
    # MariaDB error 1045 is "access denied for user" and 1044 is "access denied
    # to database"; both are permanent until somebody changes the credentials.
    # Confirmed over a few attempts rather than on the first, because a server
    # in the middle of starting can refuse a connection once for other reasons,
    # and then given up on with a message about the thing that is actually
    # wrong. Everything else - a refused connection, a missing schema - keeps
    # the long budget, which is what it was for.
    if [ -s "$probe_err" ] && grep -qiE "1045|1044|access denied" "$probe_err"; then
        auth_failures=$((auth_failures + 1))
    else
        auth_failures=0
    fi
    if [ "$auth_failures" -ge 5 ]; then
        echo "[playerbot-migrate] FATAL: the database refuses this login." >&2
        head -1 "$probe_err" >&2
        echo "[playerbot-migrate] This is a credentials problem, not a slow import: the password in" >&2
        echo "[playerbot-migrate] linux-port/docker/.env and the one this database was created with" >&2
        echo "[playerbot-migrate] are not the same. Waiting will not change it." >&2
        echo "[playerbot-migrate] In the launcher: NAPRAW DOSTEP DO BAZY." >&2
        exit 1
    fi
    if [ "$ready" = "8" ]; then
        protos=$(db -e "SELECT COUNT(*) FROM player.item_proto;" 2>/dev/null || true)
        [ -n "$protos" ] && [ "$protos" -gt 0 ] 2>/dev/null && break
    fi
    # A large world that was not shut down cleanly can spend many minutes in
    # InnoDB recovery while the image's healthcheck already answers. Five
    # minutes was not enough for it, and because compose treats this container
    # as a hard dependency, the whole "up" failed and the launcher reported
    # that Docker had not built the server -- while starting it again by hand a
    # minute later worked. Wait far longer, and say what is happening.
    if [ "$attempt" -ge 900 ]; then
        echo "[playerbot-migrate] FATAL: database not ready after 30 minutes" >&2
        echo "[playerbot-migrate] the server is probably still recovering a large world; start it again" >&2
        exit 1
    fi
    if [ $((attempt % 30)) -eq 0 ]; then
        # Three different waits look the same from outside; say which this is.
        # "answering, with none of the eight tables" is not a slow import - it
        # is a MariaDB that initialised without the dumps, and no amount of
        # waiting changes it. One operator watched this for thirty minutes.
        if [ ! -s "$probe_err" ] && [ "${ready:-0}" = "0" ] && [ "$attempt" -ge 90 ]; then
            echo "[playerbot-migrate] MariaDB is answering but holds NONE of the r40250 tables ($((attempt * 2))s)." >&2
            echo "[playerbot-migrate] The database initialised without the SQL dumps: mariadb/initdb.d/dumps" >&2
            echo "[playerbot-migrate] was missing or empty on the very first start, and initdb.d never runs again." >&2
            echo "[playerbot-migrate] Waiting will not fix this. Stage the five dumps (the launcher and the" >&2
            echo "[playerbot-migrate] installer now check for them) and re-create the database volume." >&2
        elif [ ! -s "$probe_err" ] && [ "${ready:-0}" != "0" ]; then
            echo "[playerbot-migrate] still waiting for the schema ($((attempt * 2))s): ${ready}/8 tables so far - the first-run import is in progress"
        else
            echo "[playerbot-migrate] still waiting for the database ($((attempt * 2))s) - a large world can take a while to recover"
        fi
    fi
    sleep 2
done

# Repair anything MyISAM left marked as crashed.
#
# Seventy-three of this game's seventy-five tables are MyISAM, and one unclean
# stop marks a table crashed: every reader then fails until somebody repairs
# it. `myisam_recover_options = BACKUP,FORCE` in 99-metin2.cnf handles the
# common case, but only for a table the server itself opens AFTER that setting
# took effect - so a database that was already running when the option arrived
# keeps the old behaviour until it is restarted, and a data file damaged beyond
# what QUICK will touch stays broken either way. What the operator sees then is
# not a database error but a blank "Internal Server Error" from the advanced
# panel, which reads everything from these tables while the classic panel,
# which reads files, keeps working (archonek2137, 10 September: log.log marked
# as crashed).
#
# --fast only looks at tables that were not closed properly, so on a healthy
# world this is one open per table and repairs nothing. Failures are reported
# and never fatal: a world that starts with one damaged log table is far better
# than a world that refuses to start at all.
if [ -n "${M2_DB_ROOT_PASSWORD:-}" ]; then
    repair_log=/tmp/playerbot-repair.log
    if MYSQL_PWD="$M2_DB_ROOT_PASSWORD" mariadb-check \
            --protocol=tcp --host="$M2_DB_HOST" --port="$M2_DB_PORT" --user=root \
            --auto-repair --fast --silent \
            --databases account common player log >"$repair_log" 2>&1; then
        if [ -s "$repair_log" ]; then
            echo "[playerbot-migrate] repaired tables left crashed by an unclean stop:"
            head -20 "$repair_log"
        fi
    else
        echo "[playerbot-migrate] WARNING: table check failed; continuing" >&2
        head -5 "$repair_log" >&2
    fi
fi

# The ItemShop's own database, and the item_award table its purchases are
# delivered through. Created as root because the metin2 user cannot create a
# database, and only when the root password is in the environment (it is,
# from .env, on every install the launcher made); a world without it keeps
# running - the shop then answers with an empty page, not the game with an
# error. Idempotent: CREATE IF NOT EXISTS, and the seed only fills an empty
# shop, so an operator's own catalogue survives every restart.
social_len=$(db -e "
    SELECT CHARACTER_MAXIMUM_LENGTH FROM information_schema.columns
     WHERE table_schema='account' AND table_name='account' AND column_name='social_id';
")
if [ -n "$social_len" ] && [ "$social_len" -lt 18 ] 2>/dev/null; then
    echo "[playerbot-migrate] widening account.social_id from $social_len to 18 characters"
    db -e "ALTER TABLE account.account MODIFY social_id VARCHAR(18) NOT NULL DEFAULT '';"
fi
# The ItemShop reads mileage and jackpot off the account; this schema has
# cash alone. IF NOT EXISTS keeps it a no-op after the first time.
db -e "ALTER TABLE account.account ADD COLUMN IF NOT EXISTS mileage INT NOT NULL DEFAULT 0;"
db -e "ALTER TABLE account.account ADD COLUMN IF NOT EXISTS jackpot INT NOT NULL DEFAULT 0;"
# Fishing from thirty, which is what the wiki says and what the operator
# asked for. This line shipped fifty in three places and moving two was not
# enough: CHARACTER::fishing() (playerbotify.py lowers it), the AI gate, and
# the rod LIMIT_LEVEL - the one that refuses the equip, so a bot of thirty
# could neither wear a rod nor be drawn as an angler. item_proto is read out
# of world.item_proto here (PROTO_FROM_DB = 1), which is why this sticks;
# idempotent, and it touches only rods still carrying the old fifty.
db -e "UPDATE world.item_proto SET limitvalue0 = 30 WHERE type = 13 AND limittype0 = 1 AND limitvalue0 = 50;"
# And the pass the rod needs. Karta Wedkarska (27620), which CHARACTER::fishing()
# wants worn, is sold in one place, the Fisherman's special shop (9009, opened
# by fishing_pass_shop.quest), and the package asks level fifty for it - so a
# player of thirty to forty-nine could wear the rod the line above allows and
# never fish (17 September). The db core reads shop_special_proto at
# boot, so this is live on the next start; idempotent, and only a fifty moves.
db -e "UPDATE world.shop_special_proto SET limitvalue0 = 30 WHERE item_vnum = 27620 AND limittype0 = 'LEVEL' AND limitvalue0 = 50; UPDATE world.shop_special_proto SET limitvalue1 = 30 WHERE item_vnum = 27620 AND limittype1 = 'LEVEL' AND limitvalue1 = 50;"
# Pierscien Teleportacji (70058) carries ITEM_FLAG_APPLICABLE (8192) in this
# package, and under ENABLE_QUEST_DND_EVENT that flag makes UseItemEx treat an
# ITEM_QUEST as "drop it onto another item": a plain use finds no target cell
# and returns before the quest is asked, so teleport_ring.quest never ran for
# a player ("caly czas nie dziala pierscien teleportu", 16 September).
# The ring is dragged onto nothing; the flag comes off. Idempotent.
db -e "UPDATE world.item_proto SET flag = flag & ~8192 WHERE vnum = 70058 AND (flag & 8192) <> 0;"

# MT2009 Plus: every item the mod's world has (item_proto.mt2009plus.sql,
# the full package's world.item_proto), added where this world lacks it. A
# world made from another dump - Tieru's, or one the launcher made anew when
# it lost its identity - had no Amethyst (170000...), and every game core
# died at boot on special_item_group.txt ("there is no item 170000"), so a
# login got through the auth core and then nowhere. INSERT IGNORE: a row
# that is there is never changed. Every start; a failure never stops it.
if [ -s /opt/playerbot/item_proto.mt2009plus.sql ]; then
    ip_before=$(db -N -e "SELECT COUNT(*) FROM world.item_proto" 2>/dev/null || echo 0)
    if db < /opt/playerbot/item_proto.mt2009plus.sql; then
        ip_after=$(db -N -e "SELECT COUNT(*) FROM world.item_proto" 2>/dev/null || echo 0)
        echo "[playerbot-migrate] mod items: $((ip_after - ip_before)) missing item(s) added to world.item_proto ($ip_after in all)"
    else
        echo "[playerbot-migrate] WARNING: could not add the mod's items to world.item_proto" >&2
    fi
fi

# MT2009 Plus: the monsters the same way (mob_proto.mt2009plus.sql, the full
# package's world.mob_proto). A world from another dump had the costume
# pack's mount and pet seals from the file above and not their monsters, so
# a mount "failed to spawn (missing mob_proto row?)" and said "already on a
# horse" the next time, and a pet never came (a player's support bundle,
# 27 September). INSERT IGNORE: a row that is there is never changed.
if [ -s /opt/playerbot/mob_proto.mt2009plus.sql ]; then
    mp_before=$(db -N -e "SELECT COUNT(*) FROM world.mob_proto" 2>/dev/null || echo 0)
    if db < /opt/playerbot/mob_proto.mt2009plus.sql; then
        mp_after=$(db -N -e "SELECT COUNT(*) FROM world.mob_proto" 2>/dev/null || echo 0)
        echo "[playerbot-migrate] mod monsters: $((mp_after - mp_before)) missing monster(s) added to world.mob_proto ($mp_after in all)"
    else
        echo "[playerbot-migrate] WARNING: could not add the mod's monsters to world.mob_proto" >&2
    fi
fi

# MT2009 Plus: Cor Draconis and every sash may be handed to another player
# and put in a private/offline shop.  The engine checks GIVE (1 << 13) for an
# exchange and GIVE|MYSHOP (1 << 13, 1 << 16) for a shop, so clear precisely
# those two bits and preserve DROP, PKDROP and every unrelated restriction.
# Run this on every start rather than once: an upstream item_proto import may
# restore the old flags, and the UPDATE is idempotent.
trade_mask=$((8192 + 65536))
db -e "
    UPDATE world.item_proto
       SET antiflag = antiflag & ~$trade_mask
     WHERE (
            vnum IN (
                50252,50255,50256,50257,50258,50259,50260,
                51501,51502,51503,51504,51505,51506,51507,51508,51509,51510,
                51541,51548,51549,51562,51569,51576,51583,51590,51597,
                51604,51611,51618,51625,51632,76040
            )
            OR vnum BETWEEN 85001 AND 85024
            OR vnum BETWEEN 85101 AND 85104
            OR vnum BETWEEN 86061 AND 86064
       )
       AND (antiflag & $trade_mask) <> 0;
"
echo "[playerbot-migrate] Cor Draconis and sashes: player trade enabled"

# MT2009 Plus (26 September 2026): the Dragon Stones (110000-175499) trade and
# go on a shop at every grade - the lower three carried GIVE|MYSHOP, so a
# normal, brilliant or rare stone could be neither handed on nor sold - and
# the Alchemist's Time Elixir (D) costs 5 000 000 instead of 10 000 000. Every
# start, idempotent; the db core reads world.item_proto at boot.
db -e "
    UPDATE world.item_proto
       SET antiflag = antiflag & ~$trade_mask
     WHERE vnum BETWEEN 110000 AND 175499
       AND (antiflag & $trade_mask) <> 0;
    UPDATE world.item_proto SET gold = 5000000 WHERE vnum = 100002 AND gold <> 5000000;
" && echo "[playerbot-migrate] alchemy: Dragon Stones tradeable, Time Elixir (D) 5 000 000" \
  || echo "[playerbot-migrate] WARNING: alchemy item_proto changes failed" >&2

# The Grotto of Exile's warp in Orc Valley's bottom-left corner (10077,
# commented again since 2.2.22, when Koe-Pung took the way in; kept right for a
# GM who puts it back) reads its target out of its own locale_name
# (FuncCheckWarp), and the package's pointed at cell (9,46) of map 72 - a
# blocked cell six kilometres from any open ground. The target is the
# grotto's Town point (100,46), where the engine also stands up whoever dies
# in there, 1.3 km from the way out (10078). The db core reads mob_proto at
# boot (PROTO_FROM_DB); idempotent.
db -e "UPDATE world.mob_proto SET name = '????1? 100 12078', locale_name = '????1? 100 12078' WHERE vnum = 10077 AND locale_name <> '????1? 100 12078';" || echo "[playerbot-migrate] WARNING: could not point the Grotto of Exile warp at its Town" >&2
# Three doors of the Devil's Catacomb's fourth-floor maze (10814, 10817,
# 10818) carry a locale_name with no space after the dot - ".233 780" - which
# FuncCheckWarp's ' %s %ld %ld' cannot read, so the engine moved nobody
# through them; their name column is whole, and its targets stand on the
# maze's open ground (checked on map 216's server_attr, 26 September). The
# stake at the end is reachable in every wiring without them. Idempotent.
db -e "UPDATE world.mob_proto SET locale_name = name WHERE vnum IN (10814, 10817, 10818) AND locale_name <> name;" || echo "[playerbot-migrate] WARNING: could not mend the Catacomb maze doors" >&2
# Three ItemShop lines stood behind time auctions the package's server ran
# in December 2024 - 906 the Metin stone detector, 907 Kamien Duchowy, 908 -
# and an ended auction is a line nobody sees and BuyItem refuses, a player
# as much as a bot. Their auction rows go and the lines are ordinary ones;
# an auction the operator makes is not touched. The db core reads both
# tables at boot; idempotent. Until 2.2.22 the second DELETE was a
# multi-table one, which MariaDB refuses with no default database, so the
# players' buy counts of the three stayed and this warned at every start.
db -e "DELETE FROM common.itemshop_time_auctions WHERE item_index IN (906, 907, 908) AND end_time < '2025-01-01'; DELETE FROM player.itemshop_time_auction WHERE item_index IN (906, 907, 908) AND item_index NOT IN (SELECT item_index FROM common.itemshop_time_auctions);" || echo "[playerbot-migrate] WARNING: could not end the ItemShop old time auctions" >&2
# Pirate Tanaka (5001), the Tanaka event's treasure goblin
# (playerbot_world_events.h): the package gives him 560 yang, which his fall
# splits into thirty piles of twenty, and a flat thousand at each fifth of his
# health (playerbotify apply_tanaka_goblin scales that to a fifth of a roll of
# these). A world whose operator set his yang by hand keeps it: only the
# stock 560 moves. The db core reads mob_proto at boot; idempotent.
db -e "UPDATE world.mob_proto SET gold_min = 15000, gold_max = 25000 WHERE vnum = 5001 AND gold_min = 560 AND gold_max = 560;" || echo "[playerbot-migrate] WARNING: could not give Pirate Tanaka his yang" >&2
# His ear (30202), which Yonah takes for a Purple Ebony Chest
# (tanaka_ears.quest), stacks to the 200 its row already says: the package
# left ITEM_FLAG_STACKABLE off, so every ear took a cell. Idempotent.
db -e "UPDATE world.item_proto SET flag = flag | 4 WHERE vnum = 30202 AND (flag & 4) = 0;" || echo "[playerbot-migrate] WARNING: could not make Tanaka's ear stack" >&2
# The skill books (type 17), the Forgetting Book (22) and Kamien Duchowy
# (50513) stacked to the package's ten; the operator's two hundred (DUDU,
# 26 September). PROTO_FROM_DB: the db core reads it at boot, and books of
# two skills never merge, their socket differs. Never lowered again: the
# engine would cut every stack above the new ceiling at the next load.
db -e "UPDATE world.item_proto SET stack = 200 WHERE (type IN (17, 22) OR vnum = 50513) AND stack = 10;" || echo "[playerbot-migrate] WARNING: could not raise the books' stack" >&2
# The ItemShop's Auto Lowy ticket and anti-experience ring (the operator,
# 27 September): two quest items the package defines and nothing uses -
# "Opaska Posz. Zlota" (31073) and "Pierscien Levi" (40002) - renamed
# and bound (no sale, trade, drop or counter; the ticket stacks), their uses
# answered by autohunt_time.quest and antiexp_ring.quest; and the shop's
# first page gains them with the Teleport Ring (70058), which is never used
# up. ASCII names: db() speaks latin1 into the cp1250 columns. A line the
# operator changed by hand is kept (INSERT IGNORE). Idempotent.
db -e "UPDATE world.item_proto SET locale_name = 'Auto Lowy (8h)', flag = flag | 4, antiflag = 74112 WHERE vnum = 31073 AND locale_name <> 'Auto Lowy (8h)'; UPDATE world.item_proto SET locale_name = 'Pierscien Anty-Exp', flag = 0, antiflag = 41344 WHERE vnum = 40002 AND locale_name <> 'Pierscien Anty-Exp'; INSERT IGNORE INTO common.itemshop_items (\`index\`, vnum, count, price, currency, minLevel) VALUES (6, 31073, 1, 29, 'DRAGON_COIN', 0), (7, 40002, 1, 99, 'DRAGON_COIN', 0), (8, 70058, 1, 149, 'DRAGON_COIN', 30);" || echo "[playerbot-migrate] WARNING: could not add the ItemShop's Auto Lowy ticket and rings" >&2
# Three names in the shipped dumps end in a line break (Magiczny Kamien 25042,
# Gwiazda Nocy 50731, Sniezny Kwiat 50732: "\r\n" inside the quotes). A name
# goes into server commands a client splits on whitespace, and the GM panel's
# item list broke there ("__GMPanelItemListChunk() takes exactly 2 arguments
# (3 given)", a player's syserr, 27 September). Control characters are taken
# out of every name, on every start; a clean name is not touched.
db -e "UPDATE world.item_proto SET locale_name = REGEXP_REPLACE(locale_name, '[[:cntrl:]]+', '') WHERE locale_name REGEXP '[[:cntrl:]]';" \
    || echo "[playerbot-migrate] WARNING: could not clean the line breaks out of item names" >&2
# Maska Sabaha left the world with the Hwang curse (playerbotify
# apply_hwang_curse_removed, the share step of the game Dockerfile): the shop
# that sold one sells it no more. The db core reads the shops at boot, so this
# is live on the next start; idempotent.
db -e "DELETE FROM world.shop_item WHERE item_vnum IN (72731, 72735);"
# And nobody keeps one: every Maska Sabaha still in a bag, on a character, in a
# safebox or on a counter is removed (15 September, "usun" to the masks
# players already held). On every start, so a mask an old core still held while
# an update ran this beside it goes on the next one.
masks=$(db -e "DELETE FROM player.item WHERE vnum IN (72731, 72735); SELECT ROW_COUNT();" || echo x)
masks=$(printf '%s' "$masks" | tr -d '[:space:]')
if [ "$masks" = "x" ]; then
    echo "[playerbot-migrate] WARNING: could not remove the Maska Sabaha items" >&2
elif [ -n "$masks" ] && [ "$masks" != "0" ]; then
    echo "[playerbot-migrate] removed $masks Maska Sabaha item(s)"
fi
# The market of Shinsoo's and Jinno's villages moved onto the kingdom's guard
# in 2.0.52 (GetTownPitch, playerbot_empire_rules.h), and nothing would ever
# have moved the shops standing round the old pitch: an offline shop stands
# where its keeper stood when it was opened (OpenOfflineShop takes the
# character's position, a reopen included) and a keeper walks to its shop to
# serve it. So each bot's shop of the old ring is carried across by the
# distance between the two pitches, which keeps the ring's shape and spacing,
# and pulled in to 1650 of the guard where it stood further out - the ring of
# 400 to 1700 round each guard is open ground inside the safe zone on
# server_attr. A shop already inside the new ring and outside the old one
# belongs to the new pitch and stays. Once, marked in
# player.playerbot_migrations in the same transaction as the move; on every
# start after that only a bot's shop still within 2000 of an old pitch and more
# than 2000 from the new one moves - a keeper that reopened on the old spot
# while an update ran this beside the old game container (update.sh does not
# stop the game first). The db core writes a position only when a shop is
# opened or moved, so an old core cannot write the moved ones back. A player's
# own shop is left where its owner put it. Before the game container starts,
# because the db core reads the shops at boot.
db -e "CREATE TABLE IF NOT EXISTS player.playerbot_migrations (name VARCHAR(64) NOT NULL PRIMARY KEY, done_at DATETIME NOT NULL) ENGINE=InnoDB;"
# The bot guilds' tiers (playerbot_guild.h): a guild outlives every core
# restart, so its tier and kingdom live here; the core reads the table once
# and writes a row when it founds or adopts a guild.
db -e "CREATE TABLE IF NOT EXISTS player.playerbot_guild (guild_id INT UNSIGNED NOT NULL PRIMARY KEY, tier TINYINT UNSIGNED NOT NULL DEFAULT 3, empire TINYINT UNSIGNED NOT NULL DEFAULT 0, founder_pid INT UNSIGNED NOT NULL DEFAULT 0, founded_at DATETIME NOT NULL) ENGINE=InnoDB;"
# And when each last went to war (playerbot_guild_war.h), in unix seconds, so
# the pick that keeps a kingdom's last pair out of its next war survives the
# restart every update makes.
db -e "ALTER TABLE player.playerbot_guild ADD COLUMN IF NOT EXISTS last_war_at INT UNSIGNED NOT NULL DEFAULT 0;" \
    || echo "playerbot-migrate: could not add last_war_at to player.playerbot_guild" >&2
# A bot guild's land and buildings (playerbot_guild_land.h): the fund for the
# building materials its master holds, and every payment its members made to
# a collection ("zrzutka") - who, how much, for what - for the panel and for
# anybody who wants to check it was fair. Idempotent.
db -e "ALTER TABLE player.playerbot_guild ADD COLUMN IF NOT EXISTS build_fund BIGINT NOT NULL DEFAULT 0, ADD COLUMN IF NOT EXISTS fund_holder INT UNSIGNED NOT NULL DEFAULT 0;" \
    || echo "playerbot-migrate: could not add the building fund to player.playerbot_guild" >&2
db -e "CREATE TABLE IF NOT EXISTS player.playerbot_guild_contribution (id INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY, guild_id INT UNSIGNED NOT NULL, pid INT UNSIGNED NOT NULL, amount BIGINT NOT NULL, purpose VARCHAR(24) NOT NULL, at DATETIME NOT NULL, KEY guild_at (guild_id, at)) ENGINE=InnoDB;" \
    || echo "playerbot-migrate: could not create player.playerbot_guild_contribution" >&2
# The second channel's pins (playerbot_channel_rules.h): every bot that has
# ever kept an offline shop lives on the first channel for good, because the
# shops are the first channel's. The table only grows - each core adds the
# owners it sees before it reads it - and this adds them before any core has
# started, so the start that switches the second channel on finds every keeper
# of the last session already pinned. Written whatever the switch says.
db -e "CREATE TABLE IF NOT EXISTS player.playerbot_channel_pin (pid INT UNSIGNED NOT NULL PRIMARY KEY, pinned_at DATETIME NOT NULL) ENGINE=InnoDB;"
db -e "INSERT IGNORE INTO player.playerbot_channel_pin (pid, pinned_at) SELECT owner, NOW() FROM player.ikashop_offlineshop;" 2>/dev/null \
    || echo "playerbot-migrate: could not pin the shop keepers to the first channel" >&2
pitch_done=$(db -e "SELECT COUNT(*) FROM player.playerbot_migrations WHERE name = 'pitch_on_guard_2052';" 2>/dev/null || echo x)
case "$pitch_done" in
    0) pitch_near=1700; pitch_far=1700 ;;
    1) pitch_near=-1; pitch_far=2000 ;;
    *) pitch_near= ;;
esac
if [ -n "$pitch_near" ]; then
    if pitch_moved=$(db -e "
        CREATE TEMPORARY TABLE player.tmp_pitch_moves AS
        SELECT d.owner,
               d.nx + ROUND(d.dx * LEAST(1, 1650 / GREATEST(1, d.d_old))) AS tx,
               d.ny + ROUND(d.dy * LEAST(1, 1650 / GREATEST(1, d.d_old))) AS ty
          FROM (SELECT s.owner, m.nx, m.ny,
                       CAST(s.x AS SIGNED) - m.ox AS dx,
                       CAST(s.y AS SIGNED) - m.oy AS dy,
                       SQRT(POW(CAST(s.x AS SIGNED) - m.ox, 2) + POW(CAST(s.y AS SIGNED) - m.oy, 2)) AS d_old,
                       SQRT(POW(CAST(s.x AS SIGNED) - m.nx, 2) + POW(CAST(s.y AS SIGNED) - m.ny, 2)) AS d_new
                  FROM player.ikashop_offlineshop AS s
                  JOIN player.player AS p ON p.id = s.owner
                  JOIN account.account AS a ON a.id = p.account_id
                  JOIN (SELECT 1 AS map, 473625 AS ox, 954925 AS oy, 474325 AS nx, 954225 AS ny
                        UNION ALL SELECT 3, 353987, 880012, 353025, 882325
                        UNION ALL SELECT 41, 961212, 270162, 959925, 268825
                        UNION ALL SELECT 43, 865500, 244975, 863425, 246025) AS m ON m.map = s.map
                 WHERE a.login LIKE 'playerbot%') AS d
         WHERE d.d_old <= 2000 AND (d.d_old <= $pitch_near OR d.d_new > $pitch_far);
        START TRANSACTION;
        UPDATE player.ikashop_offlineshop AS s
          JOIN player.tmp_pitch_moves AS t ON t.owner = s.owner
           SET s.x = t.tx, s.y = t.ty;
        SELECT ROW_COUNT();
        INSERT IGNORE INTO player.playerbot_migrations (name, done_at) VALUES ('pitch_on_guard_2052', NOW());
        COMMIT;
        DROP TEMPORARY TABLE player.tmp_pitch_moves;
    "); then
        pitch_moved=$(printf '%s' "$pitch_moved" | tr -d '[:space:]')
        if [ "${pitch_moved:-0}" != "0" ]; then
            echo "[playerbot-migrate] $pitch_moved bot offline shop(s) in Yongan, Jayang, Pyongmoo and Bakra carried onto the guard's square"
        fi
    else
        echo "[playerbot-migrate] WARNING: could not move the bots' offline shops onto the new pitches" >&2
    fi
fi
# The package's player dump carries the guild lands and buildings of the
# server it was taken from - 28 player.guild_land rows and 62 player.object
# rows - and none of the guilds they belong to. The engine stands its land
# agent (NPC 20040) only on a land nobody owns (building::CManager, at boot),
# so those lands could never be bought and their buildings stood on ground
# nobody held, while a bot guild founded later under one of those numbers
# (2, 3, 5, ...) held a land and buildings it never paid for ("stoja juz
# budynki, pomimo ze teren nie jest zajety", Mat, 19 September; NerrVoVy
# cleared his by hand). Once, and the dump's own rows exactly: a land a
# player's guild has bought and the buildings it put up since (ids past the
# dump's last) are left alone. Before the game container starts, because the
# db core reads both at boot.
lands_done=$(db -e "SELECT COUNT(*) FROM player.playerbot_migrations WHERE name = 'package_guild_lands_2081';" 2>/dev/null || echo x)
if [ "$lands_done" = "0" ]; then
    if lands_out=$(db -e "
        START TRANSACTION;
        DELETE FROM player.object WHERE (id, land_id, vnum) IN (
            (1, 14, 14100), (2, 214, 14120), (3, 214, 14014), (4, 14, 14013), (5, 215, 14120), (6, 215, 14013), (7, 218, 14120), (8, 218, 14043),
            (9, 16, 14100), (10, 16, 14014), (11, 16, 14043), (12, 108, 14100), (13, 108, 14014), (14, 214, 14050), (15, 14, 14051), (16, 215, 14051),
            (17, 217, 14100), (18, 218, 14014), (19, 217, 14015), (20, 109, 14100), (21, 109, 14051), (22, 17, 14100), (23, 17, 14015), (24, 207, 14110),
            (25, 207, 14014), (26, 15, 14100), (27, 15, 14015), (28, 217, 14051), (29, 18, 14110), (30, 18, 14055), (31, 115, 14120), (32, 115, 14014),
            (33, 108, 14043), (34, 18, 14015), (35, 116, 14120), (36, 116, 14013), (37, 216, 14110), (38, 109, 14015), (39, 8, 14120), (40, 216, 14013),
            (41, 212, 14100), (42, 117, 14110), (43, 216, 14055), (44, 117, 14055), (45, 117, 14014), (46, 205, 14120), (47, 205, 14055), (48, 15, 14055),
            (49, 216, 14200), (50, 216, 14300), (51, 216, 14300), (52, 205, 14015), (53, 212, 14015), (54, 206, 14100), (55, 206, 14015), (56, 8, 14015),
            (57, 115, 14050), (58, 212, 14055), (59, 207, 14055), (60, 8, 14055), (61, 208, 14110), (62, 201, 14100));
        SELECT ROW_COUNT();
        DELETE FROM player.guild_land WHERE (land_id, guild_id) IN (
            (2, 408), (8, 78), (9, 108), (10, 69), (14, 3), (15, 395), (16, 2), (17, 52),
            (18, 18), (108, 5), (109, 6), (115, 92), (116, 93), (117, 20), (118, 13), (201, 212),
            (204, 712), (205, 57), (206, 9), (207, 58), (208, 25), (212, 19), (213, 14), (214, 15),
            (215, 344), (216, 47), (217, 33), (218, 7));
        SELECT ROW_COUNT();
        INSERT IGNORE INTO player.playerbot_migrations (name, done_at) VALUES ('package_guild_lands_2081', NOW());
        COMMIT;
    "); then
        lands_objects=$(printf '%s\n' "$lands_out" | awk 'NR == 1')
        lands_rows=$(printf '%s\n' "$lands_out" | awk 'NR == 2')
        echo "[playerbot-migrate] the package's guild lands cleared: ${lands_rows:-0} land(s), ${lands_objects:-0} building(s)"
    else
        echo "[playerbot-migrate] WARNING: could not clear the package's guild lands" >&2
    fi
fi
# 2.2.21 opened the Grotto of Exile (72, 73) and the Devil's Catacomb (216)
# and no client of that time could stand on any of them: the grotto's maps
# stood in the season2 pack without the maps/ the client looks under, and the
# Catacomb's map was in no pack at all (client 2.0.39 carries all three, its
# season2 pack taken whole from upstream's client). Entering one closed the
# client, and a character saved there could not log in again ("postac jest
# zbugowana", Iwakura, 26 September). Once: every character of a person
# saved on one of them or in an
# instance of one is put where the way out leads - by Koe-Pung in Orc Valley
# (284200, 810600, the target of the grotto's exit 10078) or before the
# Catacomb's Guardian in Hwang Temple (591400, 99200, the quest's own exit).
# A bot has no client and stays where it is. Before the game container starts
# (a character left there minutes before an update may still be written back
# by the old db core's cache; the new client can stand there anyway).
rescue_done=$(db -e "SELECT COUNT(*) FROM player.playerbot_migrations WHERE name = 'grotto_catacomb_client_2221';" 2>/dev/null || echo x)
if [ "$rescue_done" = "0" ]; then
    if rescue_out=$(db -e "
        START TRANSACTION;
        UPDATE player.player AS p JOIN account.account AS a ON a.id = p.account_id
           SET p.map_index = 64, p.x = 284200, p.y = 810600,
               p.exit_map_index = 64, p.exit_x = 284200, p.exit_y = 810600
         WHERE a.login NOT LIKE 'playerbot%'
           AND (p.map_index IN (72, 73) OR p.map_index BETWEEN 720000 AND 739999);
        SELECT ROW_COUNT();
        UPDATE player.player AS p JOIN account.account AS a ON a.id = p.account_id
           SET p.map_index = 65, p.x = 591400, p.y = 99200,
               p.exit_map_index = 65, p.exit_x = 591400, p.exit_y = 99200
         WHERE a.login NOT LIKE 'playerbot%'
           AND (p.map_index = 216 OR p.map_index BETWEEN 2160000 AND 2169999);
        SELECT ROW_COUNT();
        INSERT IGNORE INTO player.playerbot_migrations (name, done_at) VALUES ('grotto_catacomb_client_2221', NOW());
        COMMIT;
    "); then
        rescue_grotto=$(printf '%s\n' "$rescue_out" | awk 'NR == 1')
        rescue_catacomb=$(printf '%s\n' "$rescue_out" | awk 'NR == 2')
        echo "[playerbot-migrate] characters moved out of maps no old client could load: ${rescue_grotto:-0} from the Grotto of Exile, ${rescue_catacomb:-0} from the Devil's Catacomb"
    else
        echo "[playerbot-migrate] WARNING: could not move the characters out of the Grotto and the Catacomb" >&2
    fi
fi
# Smoczy Skowyt (93) cast at a target (server-patches/dragonroartarget) hurts a
# circle round that target of dwSplashRange: 500, the package's, left half a
# pack standing ("nie wszystkie trafiaja", the operator, 28 September). 900,
# only over the package's own 500; skill_proto is read at the cores' start.
db -e "UPDATE world.skill_proto SET dwSplashRange = 900 WHERE dwVnum = 93 AND dwSplashRange = 500;" || echo "[playerbot-migrate] WARNING: could not widen Smoczy Skowyt's splash" >&2
# Broszura Szermierki (70031), Seon-Pyeong's recipe material, stacks to the
# 200 its row already says: the package left ITEM_FLAG_STACKABLE off, so
# every brochure took a cell (NerrVoVy, 27 September), as Tanaka's ear did.
# PROTO_FROM_DB: the db core reads it at boot. Idempotent.
db -e "UPDATE world.item_proto SET flag = flag | 4 WHERE vnum = 70031 AND (flag & 4) = 0;" || echo "[playerbot-migrate] WARNING: could not make Broszura Szermierki stack" >&2
# The ItemShop's marriage page (indexes 201-299, which the client's
# ITEMSHOP_CATEGORY_MARRIAGE lists and client 2.0.47 shows) had no line at
# all: the engagement ring (the Old Lady's ring quest gives one too), the
# tuxedo, the wedding dress and the bouquet (the travelling peddler of a
# second village sells the three for yang too), and the Love Bird's Feather
# with the six harmony and love jewels that work on love points (xXxDaronxXx,
# 27 September). From level 25, the wedding's own level. A line the operator
# changed by hand is kept (INSERT IGNORE). Idempotent.
db -e "INSERT IGNORE INTO common.itemshop_items (\`index\`, vnum, count, price, currency, minLevel) VALUES (201, 70301, 1, 19, 'DRAGON_COIN', 25), (202, 11901, 1, 49, 'DRAGON_COIN', 25), (203, 11903, 1, 49, 'DRAGON_COIN', 25), (204, 50201, 1, 9, 'DRAGON_COIN', 25), (205, 71068, 1, 29, 'DRAGON_COIN', 25), (206, 71069, 1, 39, 'DRAGON_COIN', 25), (207, 71070, 1, 39, 'DRAGON_COIN', 25), (208, 71071, 1, 39, 'DRAGON_COIN', 25), (209, 71072, 1, 39, 'DRAGON_COIN', 25), (210, 71073, 1, 39, 'DRAGON_COIN', 25), (211, 71074, 1, 39, 'DRAGON_COIN', 25);" || echo "[playerbot-migrate] WARNING: could not fill the ItemShop's marriage page" >&2
# fish_log came from r40250's dump and has that engine's eight columns,
# while this one writes six - so every catch failed with errno 1136 and the
# table is empty on every 2.x world that ever ran. CREATE IF NOT EXISTS
# cannot repair a table that already exists with the wrong shape, so the
# old one is dropped here, before log_schema.sql below recreates it.
# Recognised by a column this engine never writes; a table already in the
# right shape, and whatever history it holds, is left alone.
fish_old=$(db -e "
    SELECT COUNT(*) FROM information_schema.columns
     WHERE table_schema='log' AND table_name='fish_log' AND column_name='map_index';
" 2>/dev/null || echo 0)
if [ "$fish_old" = "1" ]; then
    echo "[playerbot-migrate] fish_log has the r40250 shape and cannot be written; rebuilding it"
    db -e "DROP TABLE IF EXISTS log.fish_log;"
fi
# The log tables the engine writes and the package dump lacks (port/logschemify.py).
if [ -s /opt/playerbot/log_schema.sql ]; then
    if db < /opt/playerbot/log_schema.sql 2>/tmp/logschema.err; then
        echo "[playerbot-migrate] log schema checked"
    else
        echo "[playerbot-migrate] WARNING: log schema failed:" >&2
        head -3 /tmp/logschema.err >&2
    fi
fi

itemshop_schema=/opt/playerbot/itemshop_schema.sql
if [ -s "$itemshop_schema" ]; then
    if [ -n "${M2_DB_ROOT_PASSWORD:-}" ]; then
        if MYSQL_PWD="$M2_DB_ROOT_PASSWORD" mariadb --protocol=tcp --host="$M2_DB_HOST" \
                --port="$M2_DB_PORT" --user=root --default-character-set=utf8mb4 \
                < "$itemshop_schema" 2>/tmp/itemshop.err; then
            echo "[playerbot-migrate] itemshop schema applied"
        else
            echo "[playerbot-migrate] WARNING: itemshop schema failed:" >&2
            head -3 /tmp/itemshop.err >&2
        fi
    else
        echo "[playerbot-migrate] WARNING: M2_DB_ROOT_PASSWORD not set; itemshop schema skipped" >&2
    fi
fi

# A developer may keep more persistent bots than the public 350-row seed. When
# that world matters, make its minimum size explicit in .env. This catches the
# easy-to-miss case where Docker is pointed at another daemon or a fresh volume:
# fail before the canonical seed can make the empty world look legitimate.
existing_bot_count=$(db -e "
    SELECT COUNT(*)
      FROM player.player
     WHERE name LIKE 'bot%';
")
if [ "$expected_existing_bots" -gt 0 ] && [ "$existing_bot_count" -lt "$expected_existing_bots" ]; then
    echo "[playerbot-migrate] FATAL: persistent-world guard expected at least $expected_existing_bots bots, found $existing_bot_count" >&2
    echo "[playerbot-migrate] FATAL: check the Docker context/daemon and the db-data volume before starting the game" >&2
    exit 1
fi
if [ "$expected_existing_bots" -gt 0 ]; then
    echo "[playerbot-migrate] persistent-world guard satisfied: $existing_bot_count bots present (minimum $expected_existing_bots)"
fi

# A bot whose saved map is not one this server hosts can never be spawned: the
# character load asks the sectree manager for the position, gets nothing, and
# gives up - the same two bots failed on all seventeen starts of one day, with
# no way to recover because the AI tick only ever sees bots that did spawn.
# Put them back on Bokjung's arrival point before the game core starts.
echo "[playerbot-migrate] checking for bots parked on maps this server does not host"
stranded=$(db -e "
    SELECT COUNT(*)
      FROM player.player p
      JOIN account.account a ON a.id = p.account_id
     WHERE LEFT(a.login, 10) = 'playerbot_'
       AND p.map_index NOT IN (1, 3, 4, 5, 6, 107, 81, 110, 111, 112, 113, 181, 182, 183, 200, 250, 302, 304,
                               21, 23, 24, 25, 26, 61, 63, 64, 65, 69, 70, 71, 104, 108, 109, 79, 216, 217, 73,
                               41, 43, 44, 45, 46, 62, 66, 67, 68, 72, 90, 208, 301, 303, 351);
")
if [ -n "$stranded" ] && [ "$stranded" -gt 0 ] 2>/dev/null; then
    # Back to its OWN kingdom's second map, not always Chunjo's: a Jinno bot
    # dropped on Bokjung's arrival point is a bot in a foreign town with none
    # of its services in reach. The three points are the arrivals of each
    # kingdom's M1->M2 gate, read out of npc.txt (tools/dump_world_catalog.py);
    # Chunjo keeps the exact point this step has always used.
    db -e "
        UPDATE player.player p
          JOIN account.account a ON a.id = p.account_id
          LEFT JOIN player.player_index pi ON pi.id = a.id
           SET p.map_index = CASE pi.empire WHEN 1 THEN 3 WHEN 3 THEN 43 ELSE 23 END,
               p.x = CASE pi.empire WHEN 1 THEN 400200 WHEN 3 THEN 906400 ELSE 145500 END,
               p.y = CASE pi.empire WHEN 1 THEN 899500 WHEN 3 THEN 221400 ELSE 240000 END
         WHERE LEFT(a.login, 10) = 'playerbot_'
           AND p.map_index NOT IN (1, 3, 4, 5, 6, 107, 81, 110, 111, 112, 113, 181, 182, 183, 200, 250, 302, 304,
                               21, 23, 24, 25, 26, 61, 63, 64, 65, 69, 70, 71, 104, 108, 109, 79, 216, 217, 73,
                               41, 43, 44, 45, 46, 62, 66, 67, 68, 72, 90, 208, 301, 303, 351);
    "
    echo "[playerbot-migrate] moved $stranded bot(s) back to their own kingdom"
fi

# A negative alignment on a bot is a bug's footprint, not a history: a bot has
# no quarrel with its own kingdom. From 2.0.39 a duellist's blow went through
# CHARACTER::Damage without asking whether the engine would allow it, so a
# challenger struck before the other side had agreed and a winner went on
# striking the respawned loser. The engine counted each such kill as a murder -
# minus twenty thousand, shared over the killer's party - and bots of level
# nine walked about as "Zlosliwy" (98 on our own world, the lowest at -151002).
# Cleared here, before any core holds the bots in memory: a running core writes
# its cached alignment back over an UPDATE.
negative=$(db -e "
    SELECT COUNT(*)
      FROM player.player p
      JOIN account.account a ON a.id = p.account_id
     WHERE LEFT(a.login, 10) = 'playerbot_'
       AND p.alignment < 0;
")
if [ -n "$negative" ] && [ "$negative" -gt 0 ] 2>/dev/null; then
    db -e "
        UPDATE player.player p
          JOIN account.account a ON a.id = p.account_id
           SET p.alignment = 0
         WHERE LEFT(a.login, 10) = 'playerbot_'
           AND p.alignment < 0;
    "
    echo "[playerbot-migrate] cleared the negative alignment of $negative bot(s)"
fi

# There used to be a step here that pulled every bot outside Orc Valley's
# central island back onto it, from the days when the navigation refused
# water and the island was all a bot could reach. The bridges are crossings
# now and the hubs span the whole valley - the Fanatic islands in the north,
# the Black Orc camps in the south - so that step moved 207 bots off their
# hunting grounds at every start. Gone on purpose.

# The registry's own size is written into the seed, so the wrapper never has to
# be edited in step with it. Hardcoding 350 here survived the move to a
# 1000-character cohort only because the old range happened to be a prefix of
# the new one.
pid_range=$(grep -o 'registry is not exactly PID [0-9]*\.\.[0-9]*' "$seed" | head -1 | sed 's/.*PID //')
first_pid=${pid_range%%..*}
last_pid=${pid_range##*..}
case "${first_pid:-}${last_pid:-}" in
    ''|*[!0-9]*)
        echo "[playerbot-migrate] FATAL: cannot read the PID range from $seed" >&2
        exit 1
        ;;
esac

before=$(db -e "
    SELECT COUNT(*)
      FROM player.player
     WHERE id BETWEEN $first_pid AND $last_pid;
")

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

# The world's difficulty, as event flags in seconds (player.quest, dwPID 0 -
# what the db core loads at boot and pushes to every game core, the package's
# own idiom for a world-wide switch). quest/m2_difficulty.lua reads them: the
# Biologist's wait between two hand-ins and the stable keeper's four waits
# (the pony, each Horse Book, the medal trainings of 1-10 and of 11-19). The
# presets scale the package's own numbers - hard is what it shipped with,
# medium a third of it, easy none (what 2.0.55 and 2.0.56 gave everybody) -
# and custom takes the hour counts from .env, the horse's for every wait.
# The wait between two skill books is the engine's (m2_book_wait, playerbotify
# apply_book_wait) and the bots' own (m2_bot_book_wait), the package's 21 hours
# on hard (drip9660, 23 September).
# Written before the seed, which may leave early on a foreign cohort.
#
# The classic panel's difficulty card sets the same flags live, so .env is
# applied only when it changed since the last start (m2_difficulty_env holds
# what it said): a change made in the panel survives a restart until the
# launcher's difficulty is changed, and the one changed last is the one kept.
difficulty=$(printf '%s' "${M2_DIFFICULTY:-easy}" | tr 'A-Z' 'a-z' | tr -d ' \r')
hours_to_seconds() {
    printf '%s\n' "$1" | tr -d ' \r' | awk '{ h = $1 + 0; if (h < 0) h = 0; if (h > 8760) h = 8760; printf "%d", h * 3600 }'
}
case "$difficulty" in
    medium) dlevel=1; bio=28800; hbuy=14400; hup=14400; htr=21600; htr2=25200; book=25200; botbook=25200 ;;
    hard)   dlevel=2; bio=86400; hbuy=43200; hup=43200; htr=64800; htr2=75600; book=75600; botbook=75600 ;;
    custom)
        dlevel=3
        bio=$(hours_to_seconds "${M2_BIOLOGIST_WAIT_HOURS:-0}")
        hbuy=$(hours_to_seconds "${M2_HORSE_WAIT_HOURS:-0}")
        hup=$hbuy; htr=$hbuy; htr2=$hbuy
        book=$(hours_to_seconds "${M2_BOOK_WAIT_HOURS:-0}")
        botbook=$(hours_to_seconds "${M2_BOT_BOOK_WAIT_HOURS:-0}") ;;
    *)      difficulty=easy; dlevel=0; bio=0; hbuy=0; hup=0; htr=0; htr2=0; book=0; botbook=0 ;;
esac
dsig=$(printf '%s|%s|%s|%s|%s|%s|%s|%s|%s' "$difficulty" "$bio" "$hbuy" "$hup" "$htr" "$htr2" "$book" "$botbook" 1 | cksum | awk '{ print $1 % 2000000000 }')
dprev=$(db -N -e "SELECT lValue FROM player.quest WHERE dwPID = 0 AND szName = 'm2_difficulty_env' LIMIT 1" 2>/dev/null | tr -d ' \r')
if [ -n "$dprev" ] && [ "$dprev" = "$dsig" ]; then
    echo "[playerbot-migrate] difficulty: .env unchanged since the last start - the flags stay as the panel or the last start left them"
elif db -e "REPLACE INTO player.quest (dwPID, szName, szState, lValue) VALUES
        (0, 'm2_difficulty', '', $dlevel),
        (0, 'm2_biologist_wait', '', $bio),
        (0, 'm2_horse_buy_wait', '', $hbuy),
        (0, 'm2_horse_upgrade_wait', '', $hup),
        (0, 'm2_horse_train_wait', '', $htr),
        (0, 'm2_horse_train2_wait', '', $htr2),
        (0, 'm2_book_wait', '', $book),
        (0, 'm2_bot_book_wait', '', $botbook),
        (0, 'm2_difficulty_env', '', $dsig);"; then
    echo "[playerbot-migrate] difficulty: $difficulty (Biologist wait ${bio}s, horse: buy ${hbuy}s upgrade ${hup}s train ${htr}s/${htr2}s, books: players ${book}s bots ${botbook}s)"
else
    echo "[playerbot-migrate] WARNING: could not write the difficulty flags; the quests keep the last ones" >&2
fi

# The apprentice chest (Skrzynia Ucznia) is the world's choice, one switch
# for people and bots alike: the event flag m2_starter_chest_off, which
# starter_chest.quest asks at a person's first login, the seed below asks
# for every bot it creates, and the cores ask for the bots in the world
# (off, a bot keeps and opens no chest of the chain: playerbot_gear.h).
# .env's M2_STARTER_CHEST (the launcher's difficulty window and new-world
# dialog, seban latino's idea of 22 September; on unless it says 0) - or
# M2_PLAYERBOT_DISABLE_STUDENT_CHEST=1, the name Seban's own integration
# gives the same choice - is applied only when it changed since the last
# start (m2_starter_chest_env holds what it said): both panels set the flag
# live (web_admin.quest STARTER_CHEST), and a choice made there outlives a
# restart until the launcher's is changed, the difficulty's rule. Until 28
# September the flag was written from .env at every start and the seed
# gave every bot its chest whatever it said.
starter_off=0
case "$(printf '%s' "${M2_STARTER_CHEST:-1}" | tr 'A-Z' 'a-z' | tr -d ' \r')" in
    0|off|no|false) starter_off=1 ;;
esac
case "$(printf '%s' "${M2_PLAYERBOT_DISABLE_STUDENT_CHEST:-0}" | tr 'A-Z' 'a-z' | tr -d ' \r')" in
    1|on|yes|true) starter_off=1 ;;
esac
starter_env=$(db -N -e "SELECT lValue FROM player.quest WHERE dwPID = 0 AND szName = 'm2_starter_chest_env' LIMIT 1;" 2>/dev/null | tr -d ' \r')
if [ "$starter_env" != "$((starter_off + 1))" ]; then
    if db -e "REPLACE INTO player.quest (dwPID, szName, szState, lValue) VALUES
            (0, 'm2_starter_chest_off', '', $starter_off),
            (0, 'm2_starter_chest_env', '', $((starter_off + 1)));"; then
        echo "[playerbot-migrate] apprentice chest: $([ "$starter_off" = 1 ] && echo off || echo on) (from .env)"
    else
        echo "[playerbot-migrate] WARNING: could not write the apprentice chest flag; the quest keeps the last one" >&2
    fi
fi
# What the world says now: .env's, or a panel's made since.
case "$(db -N -e "SELECT lValue FROM player.quest WHERE dwPID = 0 AND szName = 'm2_starter_chest_off' LIMIT 1;" 2>/dev/null | tr -d ' \r')" in
    0) starter_off=0 ;;
    [1-9]*) starter_off=1 ;;
esac
echo "[playerbot-migrate] apprentice chest for people and bots: $([ "$starter_off" = 1 ] && echo off || echo on)"
# A bot's apprentice chest is the seed's - Skrzynia Ucznia I lies in its
# bag from the start - and the quest cannot tell a bot from a person, so a
# bot still at level five or under at its first login got a second one: on
# a new world, the whole cohort (Iwakura, 26 September). The seed marks the
# bots it creates; this marks the ones seeded before it did, and changes
# nothing on a start that finds them marked. A companion is one of these
# identities, so a player gets no chest by making one either. (The quest
# asks pc.is_playerbot() as well since 28 September.)
if [ "$(db -e "SELECT COUNT(*) FROM information_schema.tables
              WHERE table_schema='common' AND table_name='playerbot_seed_state';" 2>/dev/null)" = 1 ]; then
    if db -e "INSERT INTO player.quest (dwPID, szName, szState, lValue)
            SELECT l.pid, 'starter_chest', 'given', 1
              FROM common.playerbot_seed_state AS l
             WHERE l.state IN ('complete','adopted')
            ON DUPLICATE KEY UPDATE lValue = GREATEST(lValue, 1);"; then
        echo "[playerbot-migrate] apprentice chest: a bot's is the one the seed gave it"
    else
        echo "[playerbot-migrate] WARNING: could not mark the bots' apprentice chest as given" >&2
    fi
    # Off, no bot keeps a chest of the chain: every one in a registered
    # bot's bag goes before the cores start - the chest the seed gave each
    # identity that has never been in the world (1 955 on m2zip, every one of
    # them a bot that had never played: the rest had opened theirs), and the
    # chain the others carry. A core still running beside an update may
    # write back the few its online bots hold, and the cores take those out
    # themselves, through the engine (ManagePlayerBotProgressionChests): a
    # DELETE alone on a running world comes back from the db core's cache.
    # Never a person's, and never a companion's, whose bag is its owner's too.
    if [ "$starter_off" = 1 ]; then
        starter_keep=""
        if [ "$(db -e "SELECT COUNT(*) FROM information_schema.tables
                      WHERE table_schema='player' AND table_name='playerbot_sidekick';" 2>/dev/null)" = 1 ]; then
            starter_keep="AND l.pid NOT IN (SELECT k.sidekick_pid FROM player.playerbot_sidekick AS k)"
        fi
        if starter_gone=$(db -e "DELETE FROM player.item
                 WHERE window = 'INVENTORY'
                   AND (vnum BETWEEN 50187 AND 50196 OR vnum IN (50212, 50213))
                   AND owner_id IN (SELECT l.pid
                                      FROM common.playerbot_seed_state AS l
                                      JOIN player.player AS p ON p.id = l.pid
                                      JOIN account.account AS a ON a.id = p.account_id
                                     WHERE l.state IN ('complete', 'adopted')
                                       AND a.login LIKE 'playerbot%' $starter_keep);
                SELECT ROW_COUNT();"); then
            starter_gone=$(printf '%s' "$starter_gone" | tr -d '[:space:]')
            if [ -n "$starter_gone" ] && [ "$starter_gone" != 0 ]; then
                echo "[playerbot-migrate] apprentice chest off: $starter_gone chest(s) taken out of the bots' bags"
            fi
        else
            echo "[playerbot-migrate] WARNING: could not take the apprentice chests out of the bots' bags; the cores take them out as the bots come in" >&2
        fi
    fi
fi

# Whether the world is played with Auto Lowy and with the companion
# (Towarzysz): the launcher's difficulty window writes M2_AUTOHUNT and
# M2_SIDEKICK, both on unless .env says 0 (25 September, for Drip's
# COOP without the auto hunt). Off, the server refuses the hunt's target
# and drop (m2_autohunt_off, playerbotify apply_auto_hunt_switch) and
# sends no Towarzysz letter, refuses its command and keeps companions out
# of the world (m2_sidekick_off). Event flags like the difficulty, so a
# change reaches the cores at the next start. The Dom Towarowy (M2_FLEA_MARKET,
# the same window, 27 September) is a third: off, flea_market.quest offers
# nothing at the merchant and every /flea_ command refuses
# (m2_flea_market_off, playerbotify apply_flea_market).
feature_off() {
    case "$(printf '%s' "$1" | tr 'A-Z' 'a-z' | tr -d ' \r')" in
        0|off|no|false) echo 1 ;;
        *)              echo 0 ;;
    esac
}
autohunt_off=$(feature_off "${M2_AUTOHUNT:-1}")
sidekick_off=$(feature_off "${M2_SIDEKICK:-1}")
flea_off=$(feature_off "${M2_FLEA_MARKET:-1}")
if db -e "REPLACE INTO player.quest (dwPID, szName, szState, lValue) VALUES
        (0, 'm2_autohunt_off', '', $autohunt_off),
        (0, 'm2_sidekick_off', '', $sidekick_off),
        (0, 'm2_flea_market_off', '', $flea_off);"; then
    echo "[playerbot-migrate] Auto Lowy: $([ "$autohunt_off" = 1 ] && echo off || echo on), companions: $([ "$sidekick_off" = 1 ] && echo off || echo on), Dom Towarowy: $([ "$flea_off" = 1 ] && echo off || echo on)"
else
    echo "[playerbot-migrate] WARNING: could not write the Auto Lowy, companion and Dom Towarowy flags; the cores keep the last ones" >&2
fi
# Auto Lowy for everybody (0) or only with the ItemShop's ticket (1): .env
# M2_AUTOHUNT_ITEM, which the launcher's difficulty window writes; the
# classic panel sets the flag live (web_admin.quest AUTOHUNT). The .env value
# is applied only when it changed since the last start (m2_autohunt_item_env),
# so a choice made in the panel outlives a restart.
case "$(printf '%s' "${M2_AUTOHUNT_ITEM:-0}" | tr 'A-Z' 'a-z' | tr -d ' \r')" in 1|on|yes|true) autohunt_item=1 ;; *) autohunt_item=0 ;; esac
autohunt_item_env=$(db -N -e "SELECT lValue FROM player.quest WHERE dwPID = 0 AND szName = 'm2_autohunt_item_env' LIMIT 1;" 2>/dev/null | tr -d ' \r')
if [ "$autohunt_item_env" != "$((autohunt_item + 1))" ]; then
    if db -e "REPLACE INTO player.quest (dwPID, szName, szState, lValue) VALUES (0, 'm2_autohunt_item', '', $autohunt_item), (0, 'm2_autohunt_item_env', '', $((autohunt_item + 1)));"; then
        echo "[playerbot-migrate] Auto Lowy: $([ "$autohunt_item" = 1 ] && echo 'only with the ItemShop ticket' || echo 'for everybody') (from .env)"
    else
        echo "[playerbot-migrate] WARNING: could not write the Auto Lowy ticket flag" >&2
    fi
fi

# The rare goods' two world switches (server-patches/raretoggle and
# dragon_soul.quest read them): m2_alchemy_off stops every new Cor Draconis,
# m2_sash_off every new sash. The admin panel sets them live, so, as with the
# difficulty, .env is applied only when it changed since the last start
# (m2_rare_env holds what it said): a switch made in the panel survives a
# restart until .env is changed, and the one changed last is kept.
alchemy_off=$(feature_off "${M2_ALCHEMY:-1}")
sash_off=$(feature_off "${M2_SASHES:-1}")
rsig=$((alchemy_off * 2 + sash_off + 1))
rprev=$(db -N -e "SELECT lValue FROM player.quest WHERE dwPID = 0 AND szName = 'm2_rare_env' LIMIT 1" 2>/dev/null | tr -d ' \r')
if [ -n "$rprev" ] && [ "$rprev" = "$rsig" ]; then
    echo "[playerbot-migrate] alchemy and sashes: .env unchanged since the last start - the switches stay as the panel or the last start left them"
elif db -e "REPLACE INTO player.quest (dwPID, szName, szState, lValue) VALUES
        (0, 'm2_alchemy_off', '', $alchemy_off),
        (0, 'm2_sash_off', '', $sash_off),
        (0, 'm2_rare_env', '', $rsig);"; then
    echo "[playerbot-migrate] alchemy: $([ "$alchemy_off" = 1 ] && echo off || echo on), sashes: $([ "$sash_off" = 1 ] && echo off || echo on)"
else
    echo "[playerbot-migrate] WARNING: could not write the alchemy and sash switches; they stay as they were" >&2
fi

echo "[playerbot-migrate] applying deterministic Playerbot seed (PID $first_pid..$last_pid)"
result=/tmp/playerbot-seed.out
trap 'rm -f "$result"' EXIT HUP INT TERM
# Shinsoo and Jinno are opt-in: M2_PLAYERBOT_KINGDOMS=1 lets the seed create
# their cohorts, anything else keeps the file to the Chunjo cohort it has
# always been. The variable goes in ahead of the file, in the same session,
# because a SET is per-connection.
kingdoms=0
case "${M2_PLAYERBOT_KINGDOMS:-0}" in
    1|true|TRUE|yes|YES) kingdoms=1 ;;
esac
echo "[playerbot-migrate] kingdoms (Shinsoo/Jinno) cohorts: $kingdoms"
# And whether a bot the seed makes now starts with its apprentice chest: the
# world's switch as the step above left it (off gives none).
if { printf 'SET @playerbot_seed_kingdoms = %s;
SET @playerbot_seed_starter_chest = %s;
' "$kingdoms" "$((1 - ${starter_off:-0}))"; cat "$seed"; } |
        db --show-warnings >"$result" 2>&1; then
    [ ! -s "$result" ] || cat "$result"
else
    rc=$?
    cat "$result" >&2
    if grep -Fq 'playerbot seed conflict:' "$result"; then
        if [ "$strict" = "1" ]; then
            echo "[playerbot-migrate] FATAL: canonical cohort conflict (strict mode)" >&2
            exit "$rc"
        fi
        echo "[playerbot-migrate] WARNING: existing non-canonical Playerbot cohort detected" >&2
        echo "[playerbot-migrate] WARNING: preserving it unchanged; canonical seed skipped" >&2
        echo "[playerbot-migrate] WARNING: set PLAYERBOT_SEED_STRICT=1 to make this fatal" >&2
        exit 0
    fi
    echo "[playerbot-migrate] FATAL: seed failed for a non-conflict reason" >&2
    exit "$rc"
fi

count=$(db -e "
    SELECT COUNT(*)
      FROM player.player
     WHERE id BETWEEN $first_pid AND $last_pid;
")
if [ -z "$count" ] || [ "$count" -eq 0 ] 2>/dev/null; then
    echo "[playerbot-migrate] FATAL: post-check found no bot characters at all" >&2
    exit 1
fi
added=$((count - before))
if [ "$added" -gt 0 ]; then
    echo "[playerbot-migrate] created $added new bot character(s)"
fi
echo "[playerbot-migrate] seed complete: $count bot character(s) in PID $first_pid..$last_pid"

# ---------------------------------------------------------------------------
# Human nicknames.
#
# "Pozdrawiam pana botarek7 jest kotem ale brzmi jak bot" - a world of botX7
# reads as a world of bots however well they behave. The pool and the rules are
# in playerbot_names.sql; this only chooses which of its three modes to run and
# reports what it did.
#
# It runs after the seed on purpose: a bot created a minute ago is renamed on
# the same start, and a bot the seed decided to preserve is left with whatever
# name it has, because the SQL only touches characters still called bot*.
#
# A failure here is not fatal. Names are the one part of a bot's identity
# nothing depends on - the core matches on the account login - so a server that
# could not rename its bots is a server that works with the old names.
# ---------------------------------------------------------------------------
names=/opt/playerbot/playerbot_names.sql
if [ -s "$names" ]; then
    human=1
    case "${M2_PLAYERBOT_HUMAN_NAMES:-1}" in
        0|false|FALSE|no|NO) human=0 ;;
        restore|RESTORE) human=restore ;;
    esac
    echo "[playerbot-migrate] human nicknames: $human"
    names_out=/tmp/playerbot-names.out
    if { printf 'SET @playerbot_human_names = %s;
' "'$human'"; cat "$names"; } |
            db --show-warnings >"$names_out" 2>&1; then
        [ ! -s "$names_out" ] || cat "$names_out"
    else
        cat "$names_out" >&2
        echo "[playerbot-migrate] WARNING: nicknames not applied; bots keep their seed names" >&2
    fi
    rm -f "$names_out"
else
    echo "[playerbot-migrate] no playerbot_names.sql; bots keep their seed names"
fi

# The tester account's own game masters, mt2009 only (see the file).
if [ -s /opt/playerbot/gm_characters.sql ]; then
    if gm_out=$(db < /opt/playerbot/gm_characters.sql 2>&1); then
        echo "[playerbot-migrate] $gm_out"
    else
        echo "[playerbot-migrate] WARNING: gm_characters.sql failed:" >&2
        echo "$gm_out" | head -3 >&2
    fi
fi

# ---------------------------------------------------------------------------
# A game master for the tester account.
#
# GM rights are one row in common.gmlist naming an account AND a character,
# and the mt2009 package ships neither a row nor a character on `admin' - so
# a world initialised before initdb created one (2.0.0, 2.0.1) has an admin
# who can log in and command nothing, and the first report was exactly that:
# "loguje sie admin admin, a tam nie ma postaci gm". The character the player
# made in the meantime is the one they want the rights on, so this grants
# IMPLEMENTOR to the tester account's oldest character, once, and only while
# the list is empty: a world that has ever named a GM is left as it is, and a
# world whose tester account was removed (M2_KEEP_DEMO_ACCOUNTS=0) has nothing
# to grant to. A world that has no character on the account yet gets the row
# on the first start after one is created. The db core reads the list at
# boot, and this runs before the game container starts.
# ---------------------------------------------------------------------------
gm_rows=$(db -e "SELECT COUNT(*) FROM common.gmlist;" 2>/dev/null || echo x)
if [ "$gm_rows" = "0" ]; then
    gm_name=$(db -e "
        SELECT p.name
          FROM player.player AS p
          JOIN account.account AS a ON a.id = p.account_id
         WHERE a.login = 'admin'
         ORDER BY p.id
         LIMIT 1;
    " 2>/dev/null || true)
    if [ -n "$gm_name" ]; then
        if db -e "
            INSERT INTO common.gmlist (mAccount, mName, mContactIP, mServerIP, mAuthority)
            VALUES ('admin', '$gm_name', '', 'ALL', 'IMPLEMENTOR');
        "; then
            echo "[playerbot-migrate] gmlist was empty: '$gm_name' on the admin account is IMPLEMENTOR now"
        else
            echo "[playerbot-migrate] WARNING: could not grant GM rights to '$gm_name'" >&2
        fi
    else
        echo "[playerbot-migrate] gmlist is empty and the admin account has no character yet; the first one it gets becomes GM on the next start"
    fi
fi

# ---------------------------------------------------------------------------
# MT2009 Plus: our item-shop data (mod/*.sql, made by
# custom-patches/package/build_release.sh). Each file runs ONCE per install --
# the marker in player.playerbot_migrations keeps a player's own later shop
# edits. As root: the web shop's database is created by the root-only schema
# above. A failure is reported and never stops the server from starting.
# ---------------------------------------------------------------------------
for mod_sql in /opt/playerbot/mod/*.sql; do
    [ -s "$mod_sql" ] || continue
    mod_name="mod:$(basename "$mod_sql")"
    mod_done=$(db -e "SELECT COUNT(*) FROM player.playerbot_migrations WHERE name = '$mod_name';" 2>/dev/null || echo x)
    [ "$mod_done" = "0" ] || continue
    if [ -z "${M2_DB_ROOT_PASSWORD:-}" ]; then
        echo "[playerbot-migrate] WARNING: M2_DB_ROOT_PASSWORD not set; $mod_name skipped" >&2
        continue
    fi
    if MYSQL_PWD="$M2_DB_ROOT_PASSWORD" mariadb --protocol=tcp --host="$M2_DB_HOST" \
            --port="$M2_DB_PORT" --user=root --default-character-set=utf8mb4 \
            < "$mod_sql" 2>/tmp/mod.err; then
        db -e "INSERT IGNORE INTO player.playerbot_migrations (name, done_at) VALUES ('$mod_name', NOW());" || true
        echo "[playerbot-migrate] $mod_name applied"
    else
        echo "[playerbot-migrate] WARNING: $mod_name failed:" >&2
        head -3 /tmp/mod.err >&2 || true
    fi
done
# The ended time auctions again, after the item-shop data: on a new install
# mod/10_ingame_itemshop.sql runs after the cleanup further up and writes
# 906-908 back into common.itemshop_time_auctions, whose player rows that
# cleanup had just removed - and the db core then refused to start ("item_index
# 906 not found in itemshop_time_auction in player database", 26 September,
# every CH1 OFF on a fresh 2.8.0). Idempotent.
db -e "DELETE FROM common.itemshop_time_auctions WHERE item_index IN (906, 907, 908) AND end_time < '2025-01-01'; DELETE FROM player.itemshop_time_auction WHERE item_index IN (906, 907, 908) AND item_index NOT IN (SELECT item_index FROM common.itemshop_time_auctions);" || echo "[playerbot-migrate] WARNING: could not end the ItemShop old time auctions" >&2
# The ItemShop's Auto Lowy ticket and the two rings once more, after the
# item-shop data: on a new install mod/10_ingame_itemshop.sql runs after the
# lines further up, empties common.itemshop_items and writes it back without
# them. INSERT IGNORE: a line the operator changed by hand is kept.
db -e "INSERT IGNORE INTO common.itemshop_items (\`index\`, vnum, count, price, currency, minLevel) VALUES (6, 31073, 1, 29, 'DRAGON_COIN', 0), (7, 40002, 1, 99, 'DRAGON_COIN', 0), (8, 70058, 1, 149, 'DRAGON_COIN', 30);" || echo "[playerbot-migrate] WARNING: could not add the ItemShop's Auto Lowy ticket and rings" >&2
# MT2009 PLUS New Pet System (playerbot_newpet.h, MT2009_PLUS_NEW_PET_V1):
# the second pet, hatched from an egg and levelled by its owner's kills. Its
# items (55001-55118, 55401-55411; type ITEM_PET = 37, handled by the game's
# NewPetUseItem - the ItemShop seals keep PET_UPBRINGING/PET_PAY), the pet
# mobs (34036-34083, clones of 34001 as the ItemShop pets are; the client's
# npclist.txt has their models), the ItemShop's pet page lines 40901-40927
# (eggs, supplies, the pet transporter). MT2009_PLUS_NEW_PET_SHOP_V1: no NPC
# shop sells a new-pet item - they come from the ItemShop and the Metin drops
# only (the owner, 29 September); 2.14.0 and older put the Proteinowa
# Przekaska and the Transporter Peta on the General Store (shop 3, Handlarka),
# the DELETE below takes them off on every existing install.
# Last, after the item-shop data (mod/10_ingame_itemshop.sql rewrites
# common.itemshop_items once per install). INSERT IGNORE: a row the operator
# changed by hand is kept. The names are UTF-8 here, SET NAMES converts them
# to the tables' CP1250. The db core reads the protos at boot. Idempotent.
db -e "SET NAMES utf8mb4;
DROP TEMPORARY TABLE IF EXISTS world.np_item;
CREATE TEMPORARY TABLE world.np_item AS SELECT * FROM world.item_proto WHERE vnum = 50513 LIMIT 1;
UPDATE world.np_item SET vnum = 55401, name = 'Jajo Małpki', locale_name = 'Jajo Małpki', type = 37, subtype = 0, stack = 200, size = 1, antiflag = 0, flag = 4, wearflag = 0, gold = 5000, shop_buy_price = 5000, value0 = 0, value1 = 0, value2 = 0, value3 = 0, value4 = 0, value5 = 0;
INSERT IGNORE INTO world.item_proto SELECT * FROM world.np_item;
UPDATE world.np_item SET vnum = 55402, name = 'Jajo Pajączka', locale_name = 'Jajo Pajączka', type = 37, subtype = 0, stack = 200, size = 1, antiflag = 0, flag = 4, wearflag = 0, gold = 5000, shop_buy_price = 5000, value0 = 0, value1 = 0, value2 = 0, value3 = 0, value4 = 0, value5 = 0;
INSERT IGNORE INTO world.item_proto SELECT * FROM world.np_item;
UPDATE world.np_item SET vnum = 55403, name = 'Jajo Mini Razadora', locale_name = 'Jajo Mini Razadora', type = 37, subtype = 0, stack = 200, size = 1, antiflag = 0, flag = 4, wearflag = 0, gold = 5000, shop_buy_price = 5000, value0 = 0, value1 = 0, value2 = 0, value3 = 0, value4 = 0, value5 = 0;
INSERT IGNORE INTO world.item_proto SELECT * FROM world.np_item;
UPDATE world.np_item SET vnum = 55404, name = 'Jajo Mini Nemere', locale_name = 'Jajo Mini Nemere', type = 37, subtype = 0, stack = 200, size = 1, antiflag = 0, flag = 4, wearflag = 0, gold = 5000, shop_buy_price = 5000, value0 = 0, value1 = 0, value2 = 0, value3 = 0, value4 = 0, value5 = 0;
INSERT IGNORE INTO world.item_proto SELECT * FROM world.np_item;
UPDATE world.np_item SET vnum = 55405, name = 'Jajo Smoczka', locale_name = 'Jajo Smoczka', type = 37, subtype = 0, stack = 200, size = 1, antiflag = 0, flag = 4, wearflag = 0, gold = 5000, shop_buy_price = 5000, value0 = 0, value1 = 0, value2 = 0, value3 = 0, value4 = 0, value5 = 0;
INSERT IGNORE INTO world.item_proto SELECT * FROM world.np_item;
UPDATE world.np_item SET vnum = 55406, name = 'Jajo Czerwonego Smoczka', locale_name = 'Jajo Czerwonego Smoczka', type = 37, subtype = 0, stack = 200, size = 1, antiflag = 0, flag = 4, wearflag = 0, gold = 5000, shop_buy_price = 5000, value0 = 0, value1 = 0, value2 = 0, value3 = 0, value4 = 0, value5 = 0;
INSERT IGNORE INTO world.item_proto SELECT * FROM world.np_item;
UPDATE world.np_item SET vnum = 55409, name = 'Jajo Baashido', locale_name = 'Jajo Baashido', type = 37, subtype = 0, stack = 200, size = 1, antiflag = 0, flag = 4, wearflag = 0, gold = 5000, shop_buy_price = 5000, value0 = 0, value1 = 0, value2 = 0, value3 = 0, value4 = 0, value5 = 0;
INSERT IGNORE INTO world.item_proto SELECT * FROM world.np_item;
UPDATE world.np_item SET vnum = 55410, name = 'Jajo Nessie', locale_name = 'Jajo Nessie', type = 37, subtype = 0, stack = 200, size = 1, antiflag = 0, flag = 4, wearflag = 0, gold = 5000, shop_buy_price = 5000, value0 = 0, value1 = 0, value2 = 0, value3 = 0, value4 = 0, value5 = 0;
INSERT IGNORE INTO world.item_proto SELECT * FROM world.np_item;
UPDATE world.np_item SET vnum = 55411, name = 'Jajo Exedyara', locale_name = 'Jajo Exedyara', type = 37, subtype = 0, stack = 200, size = 1, antiflag = 0, flag = 4, wearflag = 0, gold = 5000, shop_buy_price = 5000, value0 = 0, value1 = 0, value2 = 0, value3 = 0, value4 = 0, value5 = 0;
INSERT IGNORE INTO world.item_proto SELECT * FROM world.np_item;
UPDATE world.np_item SET vnum = 55001, name = 'Proteinowa Przekąska', locale_name = 'Proteinowa Przekąska', type = 37, subtype = 3, stack = 200, size = 1, antiflag = 0, flag = 4, wearflag = 0, gold = 50000, shop_buy_price = 500, value0 = 0, value1 = 0, value2 = 0, value3 = 0, value4 = 0, value5 = 0;
INSERT IGNORE INTO world.item_proto SELECT * FROM world.np_item;
UPDATE world.np_item SET vnum = 55002, name = 'Transporter Peta', locale_name = 'Transporter Peta', type = 37, subtype = 2, stack = 200, size = 1, antiflag = 0, flag = 4, wearflag = 0, gold = 500000, shop_buy_price = 5000, value0 = 0, value1 = 0, value2 = 0, value3 = 0, value4 = 0, value5 = 0;
INSERT IGNORE INTO world.item_proto SELECT * FROM world.np_item;
UPDATE world.np_item SET vnum = 55007, name = 'Transporter z Petem', locale_name = 'Transporter z Petem', type = 37, subtype = 2, stack = 1, size = 1, antiflag = 33024, flag = 0, wearflag = 0, gold = 0, shop_buy_price = 0, value0 = 0, value1 = 0, value2 = 0, value3 = 0, value4 = 0, value5 = 0;
INSERT IGNORE INTO world.item_proto SELECT * FROM world.np_item;
UPDATE world.np_item SET vnum = 55008, name = 'Zwój Imienia Peta', locale_name = 'Zwój Imienia Peta', type = 37, subtype = 6, stack = 200, size = 1, antiflag = 0, flag = 4, wearflag = 0, gold = 500, shop_buy_price = 500, value0 = 0, value1 = 0, value2 = 0, value3 = 0, value4 = 0, value5 = 0;
INSERT IGNORE INTO world.item_proto SELECT * FROM world.np_item;
UPDATE world.np_item SET vnum = 55009, name = 'Skrzynia Ksiąg Peta', locale_name = 'Skrzynia Ksiąg Peta', type = 37, subtype = 2, stack = 200, size = 1, antiflag = 0, flag = 4, wearflag = 0, gold = 1000, shop_buy_price = 1000, value0 = 0, value1 = 0, value2 = 0, value3 = 0, value4 = 0, value5 = 0;
INSERT IGNORE INTO world.item_proto SELECT * FROM world.np_item;
UPDATE world.np_item SET vnum = 55010, name = 'Sztuka Łowcy Metinów', locale_name = 'Sztuka Łowcy Metinów', type = 37, subtype = 4, stack = 200, size = 1, antiflag = 0, flag = 4, wearflag = 0, gold = 1000, shop_buy_price = 1000, value0 = 1, value1 = 0, value2 = 0, value3 = 0, value4 = 0, value5 = 0;
INSERT IGNORE INTO world.item_proto SELECT * FROM world.np_item;
UPDATE world.np_item SET vnum = 55011, name = 'Sztuka Łowcy Bossów', locale_name = 'Sztuka Łowcy Bossów', type = 37, subtype = 4, stack = 200, size = 1, antiflag = 0, flag = 4, wearflag = 0, gold = 1000, shop_buy_price = 1000, value0 = 2, value1 = 0, value2 = 0, value3 = 0, value4 = 0, value5 = 0;
INSERT IGNORE INTO world.item_proto SELECT * FROM world.np_item;
UPDATE world.np_item SET vnum = 55012, name = 'Sztuka Łowcy Nieumarłych', locale_name = 'Sztuka Łowcy Nieumarłych', type = 37, subtype = 4, stack = 200, size = 1, antiflag = 0, flag = 4, wearflag = 0, gold = 1000, shop_buy_price = 1000, value0 = 3, value1 = 0, value2 = 0, value3 = 0, value4 = 0, value5 = 0;
INSERT IGNORE INTO world.item_proto SELECT * FROM world.np_item;
UPDATE world.np_item SET vnum = 55013, name = 'Sztuka Bogobójcy', locale_name = 'Sztuka Bogobójcy', type = 37, subtype = 4, stack = 200, size = 1, antiflag = 0, flag = 4, wearflag = 0, gold = 1000, shop_buy_price = 1000, value0 = 4, value1 = 0, value2 = 0, value3 = 0, value4 = 0, value5 = 0;
INSERT IGNORE INTO world.item_proto SELECT * FROM world.np_item;
UPDATE world.np_item SET vnum = 55014, name = 'Sztuka Najwyższego Mędrca', locale_name = 'Sztuka Najwyższego Mędrca', type = 37, subtype = 4, stack = 200, size = 1, antiflag = 0, flag = 4, wearflag = 0, gold = 1000, shop_buy_price = 1000, value0 = 5, value1 = 0, value2 = 0, value3 = 0, value4 = 0, value5 = 0;
INSERT IGNORE INTO world.item_proto SELECT * FROM world.np_item;
UPDATE world.np_item SET vnum = 55015, name = 'Sztuka Łowcy Potworów', locale_name = 'Sztuka Łowcy Potworów', type = 37, subtype = 4, stack = 200, size = 1, antiflag = 0, flag = 4, wearflag = 0, gold = 1000, shop_buy_price = 1000, value0 = 6, value1 = 0, value2 = 0, value3 = 0, value4 = 0, value5 = 0;
INSERT IGNORE INTO world.item_proto SELECT * FROM world.np_item;
UPDATE world.np_item SET vnum = 55016, name = 'Sztuka Siły', locale_name = 'Sztuka Siły', type = 37, subtype = 4, stack = 200, size = 1, antiflag = 0, flag = 4, wearflag = 0, gold = 1000, shop_buy_price = 1000, value0 = 7, value1 = 0, value2 = 0, value3 = 0, value4 = 0, value5 = 0;
INSERT IGNORE INTO world.item_proto SELECT * FROM world.np_item;
UPDATE world.np_item SET vnum = 55017, name = 'Sztuka Wiedzy', locale_name = 'Sztuka Wiedzy', type = 37, subtype = 4, stack = 200, size = 1, antiflag = 0, flag = 4, wearflag = 0, gold = 1000, shop_buy_price = 1000, value0 = 8, value1 = 0, value2 = 0, value3 = 0, value4 = 0, value5 = 0;
INSERT IGNORE INTO world.item_proto SELECT * FROM world.np_item;
UPDATE world.np_item SET vnum = 55018, name = 'Sztuka Cienia', locale_name = 'Sztuka Cienia', type = 37, subtype = 4, stack = 200, size = 1, antiflag = 0, flag = 4, wearflag = 0, gold = 1000, shop_buy_price = 1000, value0 = 9, value1 = 0, value2 = 0, value3 = 0, value4 = 0, value5 = 0;
INSERT IGNORE INTO world.item_proto SELECT * FROM world.np_item;
UPDATE world.np_item SET vnum = 55019, name = 'Sztuka Łowcy Magii', locale_name = 'Sztuka Łowcy Magii', type = 37, subtype = 4, stack = 200, size = 1, antiflag = 0, flag = 4, wearflag = 0, gold = 1000, shop_buy_price = 1000, value0 = 10, value1 = 0, value2 = 0, value3 = 0, value4 = 0, value5 = 0;
INSERT IGNORE INTO world.item_proto SELECT * FROM world.np_item;
UPDATE world.np_item SET vnum = 55020, name = 'Sztuka Łowcy Broni', locale_name = 'Sztuka Łowcy Broni', type = 37, subtype = 4, stack = 200, size = 1, antiflag = 0, flag = 4, wearflag = 0, gold = 1000, shop_buy_price = 1000, value0 = 11, value1 = 0, value2 = 0, value3 = 0, value4 = 0, value5 = 0;
INSERT IGNORE INTO world.item_proto SELECT * FROM world.np_item;
UPDATE world.np_item SET vnum = 55021, name = 'Sztuka Świętej Ochrony', locale_name = 'Sztuka Świętej Ochrony', type = 37, subtype = 4, stack = 200, size = 1, antiflag = 0, flag = 4, wearflag = 0, gold = 1000, shop_buy_price = 1000, value0 = 12, value1 = 0, value2 = 0, value3 = 0, value4 = 0, value5 = 0;
INSERT IGNORE INTO world.item_proto SELECT * FROM world.np_item;
UPDATE world.np_item SET vnum = 55022, name = 'Sztuka Świętej Zbroi', locale_name = 'Sztuka Świętej Zbroi', type = 37, subtype = 4, stack = 200, size = 1, antiflag = 0, flag = 4, wearflag = 0, gold = 1000, shop_buy_price = 1000, value0 = 13, value1 = 0, value2 = 0, value3 = 0, value4 = 0, value5 = 0;
INSERT IGNORE INTO world.item_proto SELECT * FROM world.np_item;
UPDATE world.np_item SET vnum = 55023, name = 'Sztuka Bożego Błogosławieństwa', locale_name = 'Sztuka Bożego Błogosławieństwa', type = 37, subtype = 4, stack = 200, size = 1, antiflag = 0, flag = 4, wearflag = 0, gold = 1000, shop_buy_price = 1000, value0 = 14, value1 = 0, value2 = 0, value3 = 0, value4 = 0, value5 = 0;
INSERT IGNORE INTO world.item_proto SELECT * FROM world.np_item;
UPDATE world.np_item SET vnum = 55024, name = 'Sztuka Najwyższego Wojownika', locale_name = 'Sztuka Najwyższego Wojownika', type = 37, subtype = 4, stack = 200, size = 1, antiflag = 0, flag = 4, wearflag = 0, gold = 1000, shop_buy_price = 1000, value0 = 15, value1 = 0, value2 = 0, value3 = 0, value4 = 0, value5 = 0;
INSERT IGNORE INTO world.item_proto SELECT * FROM world.np_item;
UPDATE world.np_item SET vnum = 55025, name = 'Sztuka Mistrza Bossów', locale_name = 'Sztuka Mistrza Bossów', type = 37, subtype = 4, stack = 200, size = 1, antiflag = 0, flag = 4, wearflag = 0, gold = 1000, shop_buy_price = 1000, value0 = 16, value1 = 0, value2 = 0, value3 = 0, value4 = 0, value5 = 0;
INSERT IGNORE INTO world.item_proto SELECT * FROM world.np_item;
UPDATE world.np_item SET vnum = 55026, name = 'Sztuka Świętego Żywiołu', locale_name = 'Sztuka Świętego Żywiołu', type = 37, subtype = 4, stack = 200, size = 1, antiflag = 0, flag = 4, wearflag = 0, gold = 1000, shop_buy_price = 1000, value0 = 17, value1 = 0, value2 = 0, value3 = 0, value4 = 0, value5 = 0;
INSERT IGNORE INTO world.item_proto SELECT * FROM world.np_item;
UPDATE world.np_item SET vnum = 55027, name = 'Sztuka Mistrza Broni', locale_name = 'Sztuka Mistrza Broni', type = 37, subtype = 4, stack = 200, size = 1, antiflag = 0, flag = 4, wearflag = 0, gold = 1000, shop_buy_price = 1000, value0 = 18, value1 = 0, value2 = 0, value3 = 0, value4 = 0, value5 = 0;
INSERT IGNORE INTO world.item_proto SELECT * FROM world.np_item;
UPDATE world.np_item SET vnum = 55028, name = 'Sztuka Mistrza Magii', locale_name = 'Sztuka Mistrza Magii', type = 37, subtype = 4, stack = 200, size = 1, antiflag = 0, flag = 4, wearflag = 0, gold = 1000, shop_buy_price = 1000, value0 = 19, value1 = 0, value2 = 0, value3 = 0, value4 = 0, value5 = 0;
INSERT IGNORE INTO world.item_proto SELECT * FROM world.np_item;
UPDATE world.np_item SET vnum = 55029, name = 'Sztuka Mistrza Łowów', locale_name = 'Sztuka Mistrza Łowów', type = 37, subtype = 4, stack = 200, size = 1, antiflag = 0, flag = 4, wearflag = 0, gold = 1000, shop_buy_price = 1000, value0 = 20, value1 = 0, value2 = 0, value3 = 0, value4 = 0, value5 = 0;
INSERT IGNORE INTO world.item_proto SELECT * FROM world.np_item;
UPDATE world.np_item SET vnum = 55030, name = 'Sztuka Świętej Wody', locale_name = 'Sztuka Świętej Wody', type = 37, subtype = 4, stack = 200, size = 1, antiflag = 0, flag = 4, wearflag = 0, gold = 1000, shop_buy_price = 1000, value0 = 21, value1 = 0, value2 = 0, value3 = 0, value4 = 0, value5 = 0;
INSERT IGNORE INTO world.item_proto SELECT * FROM world.np_item;
UPDATE world.np_item SET vnum = 55031, name = 'Sztuka Życia', locale_name = 'Sztuka Życia', type = 37, subtype = 4, stack = 200, size = 1, antiflag = 0, flag = 4, wearflag = 0, gold = 1000, shop_buy_price = 1000, value0 = 22, value1 = 0, value2 = 0, value3 = 0, value4 = 0, value5 = 0;
INSERT IGNORE INTO world.item_proto SELECT * FROM world.np_item;
UPDATE world.np_item SET vnum = 55032, name = 'Smakołyk', locale_name = 'Smakołyk', type = 37, subtype = 7, stack = 200, size = 1, antiflag = 0, flag = 4, wearflag = 0, gold = 500, shop_buy_price = 500, value0 = 800000, value1 = 0, value2 = 0, value3 = 0, value4 = 0, value5 = 0;
INSERT IGNORE INTO world.item_proto SELECT * FROM world.np_item;
UPDATE world.np_item SET vnum = 55033, name = 'Pet Reverti', locale_name = 'Pet Reverti', type = 37, subtype = 8, stack = 200, size = 1, antiflag = 0, flag = 516, wearflag = 0, gold = 500, shop_buy_price = 500, value0 = 0, value1 = 0, value2 = 0, value3 = 0, value4 = 0, value5 = 0;
INSERT IGNORE INTO world.item_proto SELECT * FROM world.np_item;
UPDATE world.np_item SET vnum = 55034, name = 'Pet Revertus', locale_name = 'Pet Revertus', type = 37, subtype = 5, stack = 200, size = 1, antiflag = 0, flag = 4, wearflag = 0, gold = 500, shop_buy_price = 500, value0 = 0, value1 = 0, value2 = 0, value3 = 0, value4 = 0, value5 = 0;
INSERT IGNORE INTO world.item_proto SELECT * FROM world.np_item;
UPDATE world.np_item SET vnum = 55035, name = 'Smakołyk+', locale_name = 'Smakołyk+', type = 37, subtype = 9, stack = 200, size = 1, antiflag = 0, flag = 4, wearflag = 0, gold = 500, shop_buy_price = 500, value0 = 5, value1 = 0, value2 = 0, value3 = 0, value4 = 0, value5 = 0;
INSERT IGNORE INTO world.item_proto SELECT * FROM world.np_item;
UPDATE world.np_item SET vnum = 55036, name = 'Klucz Miejsca Umiejętności', locale_name = 'Klucz Miejsca Umiejętności', type = 37, subtype = 10, stack = 200, size = 1, antiflag = 0, flag = 4, wearflag = 0, gold = 1000, shop_buy_price = 1000, value0 = 0, value1 = 0, value2 = 0, value3 = 0, value4 = 0, value5 = 0;
INSERT IGNORE INTO world.item_proto SELECT * FROM world.np_item;
UPDATE world.np_item SET vnum = 55101, name = 'Eliksir Witalności (S)', locale_name = 'Eliksir Witalności (S)', type = 37, subtype = 11, stack = 200, size = 1, antiflag = 0, flag = 516, wearflag = 0, gold = 1000, shop_buy_price = 1000, value0 = 0, value1 = 1, value2 = 0, value3 = 0, value4 = 0, value5 = 0;
INSERT IGNORE INTO world.item_proto SELECT * FROM world.np_item;
UPDATE world.np_item SET vnum = 55102, name = 'Eliksir Witalności (M)', locale_name = 'Eliksir Witalności (M)', type = 37, subtype = 11, stack = 200, size = 1, antiflag = 0, flag = 516, wearflag = 0, gold = 1000, shop_buy_price = 1000, value0 = 0, value1 = 2, value2 = 0, value3 = 0, value4 = 0, value5 = 0;
INSERT IGNORE INTO world.item_proto SELECT * FROM world.np_item;
UPDATE world.np_item SET vnum = 55103, name = 'Eliksir Witalności (L)', locale_name = 'Eliksir Witalności (L)', type = 37, subtype = 11, stack = 200, size = 1, antiflag = 0, flag = 516, wearflag = 0, gold = 1000, shop_buy_price = 1000, value0 = 0, value1 = 3, value2 = 0, value3 = 0, value4 = 0, value5 = 0;
INSERT IGNORE INTO world.item_proto SELECT * FROM world.np_item;
UPDATE world.np_item SET vnum = 55104, name = 'Eliksir Witalności (XL)', locale_name = 'Eliksir Witalności (XL)', type = 37, subtype = 11, stack = 200, size = 1, antiflag = 0, flag = 516, wearflag = 0, gold = 1000, shop_buy_price = 1000, value0 = 0, value1 = 4, value2 = 0, value3 = 0, value4 = 0, value5 = 0;
INSERT IGNORE INTO world.item_proto SELECT * FROM world.np_item;
UPDATE world.np_item SET vnum = 55108, name = 'Eliksir Walki (S)', locale_name = 'Eliksir Walki (S)', type = 37, subtype = 11, stack = 200, size = 1, antiflag = 0, flag = 516, wearflag = 0, gold = 1000, shop_buy_price = 1000, value0 = 1, value1 = 1, value2 = 0, value3 = 0, value4 = 0, value5 = 0;
INSERT IGNORE INTO world.item_proto SELECT * FROM world.np_item;
UPDATE world.np_item SET vnum = 55109, name = 'Eliksir Walki (M)', locale_name = 'Eliksir Walki (M)', type = 37, subtype = 11, stack = 200, size = 1, antiflag = 0, flag = 516, wearflag = 0, gold = 1000, shop_buy_price = 1000, value0 = 1, value1 = 2, value2 = 0, value3 = 0, value4 = 0, value5 = 0;
INSERT IGNORE INTO world.item_proto SELECT * FROM world.np_item;
UPDATE world.np_item SET vnum = 55110, name = 'Eliksir Walki (L)', locale_name = 'Eliksir Walki (L)', type = 37, subtype = 11, stack = 200, size = 1, antiflag = 0, flag = 516, wearflag = 0, gold = 1000, shop_buy_price = 1000, value0 = 1, value1 = 3, value2 = 0, value3 = 0, value4 = 0, value5 = 0;
INSERT IGNORE INTO world.item_proto SELECT * FROM world.np_item;
UPDATE world.np_item SET vnum = 55111, name = 'Eliksir Walki (XL)', locale_name = 'Eliksir Walki (XL)', type = 37, subtype = 11, stack = 200, size = 1, antiflag = 0, flag = 516, wearflag = 0, gold = 1000, shop_buy_price = 1000, value0 = 1, value1 = 4, value2 = 0, value3 = 0, value4 = 0, value5 = 0;
INSERT IGNORE INTO world.item_proto SELECT * FROM world.np_item;
UPDATE world.np_item SET vnum = 55115, name = 'Eliksir Mocy (S)', locale_name = 'Eliksir Mocy (S)', type = 37, subtype = 11, stack = 200, size = 1, antiflag = 0, flag = 516, wearflag = 0, gold = 1000, shop_buy_price = 1000, value0 = 2, value1 = 1, value2 = 0, value3 = 0, value4 = 0, value5 = 0;
INSERT IGNORE INTO world.item_proto SELECT * FROM world.np_item;
UPDATE world.np_item SET vnum = 55116, name = 'Eliksir Mocy (M)', locale_name = 'Eliksir Mocy (M)', type = 37, subtype = 11, stack = 200, size = 1, antiflag = 0, flag = 516, wearflag = 0, gold = 1000, shop_buy_price = 1000, value0 = 2, value1 = 2, value2 = 0, value3 = 0, value4 = 0, value5 = 0;
INSERT IGNORE INTO world.item_proto SELECT * FROM world.np_item;
UPDATE world.np_item SET vnum = 55117, name = 'Eliksir Mocy (L)', locale_name = 'Eliksir Mocy (L)', type = 37, subtype = 11, stack = 200, size = 1, antiflag = 0, flag = 516, wearflag = 0, gold = 1000, shop_buy_price = 1000, value0 = 2, value1 = 3, value2 = 0, value3 = 0, value4 = 0, value5 = 0;
INSERT IGNORE INTO world.item_proto SELECT * FROM world.np_item;
UPDATE world.np_item SET vnum = 55118, name = 'Eliksir Mocy (XL)', locale_name = 'Eliksir Mocy (XL)', type = 37, subtype = 11, stack = 200, size = 1, antiflag = 0, flag = 516, wearflag = 0, gold = 1000, shop_buy_price = 1000, value0 = 2, value1 = 4, value2 = 0, value3 = 0, value4 = 0, value5 = 0;
INSERT IGNORE INTO world.item_proto SELECT * FROM world.np_item;
UPDATE world.item_proto SET gold = shop_buy_price WHERE vnum IN (55401, 55402, 55403, 55404, 55405, 55406, 55409, 55410, 55411, 55001, 55002, 55007, 55008, 55009, 55010, 55011, 55012, 55013, 55014, 55015, 55016, 55017, 55018, 55019, 55020, 55021, 55022, 55023, 55024, 55025, 55026, 55027, 55028, 55029, 55030, 55031, 55032, 55033, 55034, 55035, 55036, 55101, 55102, 55103, 55104, 55108, 55109, 55110, 55111, 55115, 55116, 55117, 55118) AND gold < shop_buy_price;
DROP TEMPORARY TABLE IF EXISTS world.np_mob;
CREATE TEMPORARY TABLE world.np_mob AS SELECT * FROM world.mob_proto WHERE vnum = 34001 LIMIT 1;
UPDATE world.np_mob SET vnum = 34041, name = 'Małpka', locale_name = 'Małpka';
INSERT IGNORE INTO world.mob_proto SELECT * FROM world.np_mob;
UPDATE world.np_mob SET vnum = 34042, name = 'Małpka (Heroiczna)', locale_name = 'Małpka (Heroiczna)';
INSERT IGNORE INTO world.mob_proto SELECT * FROM world.np_mob;
UPDATE world.np_mob SET vnum = 34045, name = 'Pajączek', locale_name = 'Pajączek';
INSERT IGNORE INTO world.mob_proto SELECT * FROM world.np_mob;
UPDATE world.np_mob SET vnum = 34046, name = 'Pajączek (Heroiczny)', locale_name = 'Pajączek (Heroiczny)';
INSERT IGNORE INTO world.mob_proto SELECT * FROM world.np_mob;
UPDATE world.np_mob SET vnum = 34049, name = 'Mini Razador', locale_name = 'Mini Razador';
INSERT IGNORE INTO world.mob_proto SELECT * FROM world.np_mob;
UPDATE world.np_mob SET vnum = 34050, name = 'Mini Razador (Heroiczny)', locale_name = 'Mini Razador (Heroiczny)';
INSERT IGNORE INTO world.mob_proto SELECT * FROM world.np_mob;
UPDATE world.np_mob SET vnum = 34053, name = 'Mini Nemere', locale_name = 'Mini Nemere';
INSERT IGNORE INTO world.mob_proto SELECT * FROM world.np_mob;
UPDATE world.np_mob SET vnum = 34054, name = 'Mini Nemere (Heroiczny)', locale_name = 'Mini Nemere (Heroiczny)';
INSERT IGNORE INTO world.mob_proto SELECT * FROM world.np_mob;
UPDATE world.np_mob SET vnum = 34036, name = 'Smoczek', locale_name = 'Smoczek';
INSERT IGNORE INTO world.mob_proto SELECT * FROM world.np_mob;
UPDATE world.np_mob SET vnum = 34037, name = 'Smoczek (Heroiczny)', locale_name = 'Smoczek (Heroiczny)';
INSERT IGNORE INTO world.mob_proto SELECT * FROM world.np_mob;
UPDATE world.np_mob SET vnum = 34064, name = 'Czerwony Smoczek', locale_name = 'Czerwony Smoczek';
INSERT IGNORE INTO world.mob_proto SELECT * FROM world.np_mob;
UPDATE world.np_mob SET vnum = 34065, name = 'Czerwony Smoczek (Hero)', locale_name = 'Czerwony Smoczek (Hero)';
INSERT IGNORE INTO world.mob_proto SELECT * FROM world.np_mob;
UPDATE world.np_mob SET vnum = 34080, name = 'Mały Baashido', locale_name = 'Mały Baashido';
INSERT IGNORE INTO world.mob_proto SELECT * FROM world.np_mob;
UPDATE world.np_mob SET vnum = 34081, name = 'Mały Baashido (Hero)', locale_name = 'Mały Baashido (Hero)';
INSERT IGNORE INTO world.mob_proto SELECT * FROM world.np_mob;
UPDATE world.np_mob SET vnum = 34082, name = 'Nessie', locale_name = 'Nessie';
INSERT IGNORE INTO world.mob_proto SELECT * FROM world.np_mob;
UPDATE world.np_mob SET vnum = 34083, name = 'Nessie (Heroiczna)', locale_name = 'Nessie (Heroiczna)';
INSERT IGNORE INTO world.mob_proto SELECT * FROM world.np_mob;
UPDATE world.np_mob SET vnum = 34047, name = 'Pisklę Exedyara', locale_name = 'Pisklę Exedyara';
INSERT IGNORE INTO world.mob_proto SELECT * FROM world.np_mob;
UPDATE world.np_mob SET vnum = 34048, name = 'Pisklę Exedyara (Hero)', locale_name = 'Pisklę Exedyara (Hero)';
INSERT IGNORE INTO world.mob_proto SELECT * FROM world.np_mob;
INSERT IGNORE INTO common.itemshop_items (\`index\`, vnum, count, price, currency, minLevel) VALUES (40901, 55401, 1, 29, 'DRAGON_COIN', 0), (40902, 55402, 1, 29, 'DRAGON_COIN', 0), (40903, 55403, 1, 29, 'DRAGON_COIN', 0), (40904, 55404, 1, 29, 'DRAGON_COIN', 0), (40905, 55405, 1, 29, 'DRAGON_COIN', 0), (40906, 55406, 1, 29, 'DRAGON_COIN', 0), (40907, 55409, 1, 29, 'DRAGON_COIN', 0), (40908, 55410, 1, 29, 'DRAGON_COIN', 0), (40909, 55411, 1, 29, 'DRAGON_COIN', 0), (40920, 55001, 10, 9, 'DRAGON_COIN', 0), (40921, 55032, 10, 19, 'DRAGON_COIN', 0), (40922, 55035, 5, 19, 'DRAGON_COIN', 0), (40923, 55009, 1, 15, 'DRAGON_COIN', 0), (40924, 55008, 1, 9, 'DRAGON_COIN', 0), (40925, 55033, 1, 9, 'DRAGON_COIN', 0), (40926, 55034, 1, 5, 'DRAGON_COIN', 0), (40927, 55036, 1, 49, 'DRAGON_COIN', 0), (40928, 55002, 1, 19, 'DRAGON_COIN', 0);
DELETE FROM world.shop_item WHERE item_vnum BETWEEN 55001 AND 55999;" || echo "[playerbot-migrate] WARNING: could not add the New Pet System's items and mobs" >&2
# MT2009_PLUS_WHEEL_V1: Bilet Kola Fortuny (80030), the Kolo Fortuny's ticket (playerbot_wheel.h,
# "/kolo"): quest type, stacks to 200, tradeable, no drop/NPC sale (the SM coupon's antiflags); the
# ItemShop's first page sells it for 25 Smocze Monety. PROTO_FROM_DB: read at the db core's boot. Idempotent.
db -e "INSERT IGNORE INTO world.item_proto (vnum, name, locale_name, type, subtype, stack, weight, size, antiflag, flag, wearflag, immuneflag, gold, shop_buy_price, refined_vnum, refine_set, magic_pct, specular, socket_pct, addon_type, limittype0, limitvalue0, limittype1, limitvalue1, applytype0, applyvalue0, applytype1, applyvalue1, applytype2, applyvalue2, value0, value1, value2, value3, value4, value5, socket0, socket1, socket2, socket3, socket4, socket5) VALUES (80030, 'Bilet Kola Fortuny', _cp1250 X'42696C6574204B6FB36120466F7274756E79', 18, 0, 200, 0, 1, 384, 8196, 0, '', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, -1, -1, -1, -1, -1);" || echo "[playerbot-migrate] WARNING: could not add Bilet Kola Fortuny" >&2
db -e "INSERT IGNORE INTO common.itemshop_items (\`index\`, vnum, count, price, currency, minLevel) VALUES (9, 80030, 1, 25, 'DRAGON_COIN', 0);" || echo "[playerbot-migrate] WARNING: could not add the wheel ticket to the ItemShop" >&2
# MT2009_PLUS_OCHAO_V1 (db): Swiatynia Ochao (map 209, playerbot_ochao.h,
# quest/temple_of_the_ochao.quest). The package's monsters on this world's
# ladder - after the Grotto of Exile (81-97), level 98-105 - cloned from the
# Mt Thunder lemurs they share their models with (3301-3305, 3390, 3391; the
# En-Tai Guardian from 3304 with the tree beings' motions), the package's two
# NPCs (Portal 20415; Straznik Swiatyni 20426 - the package's 20408 is this
# world's Friendly Monkey) cloned from the Teleporter (9012), and the package's
# mob skill 263 (SLOW4000) the three bosses use. INSERT IGNORE: a row the
# operator changed by hand is kept. The db core reads the protos at boot.
db -e "SET NAMES utf8mb4;
INSERT IGNORE INTO world.skill_proto (dwVnum, szName, bType, bLevelStep, bMaxLevel, bLevelLimit, szPointOn, szPointPoly, szSPCostPoly, szDurationPoly, szDurationSPCostPoly, szCooldownPoly, szMasterBonusPoly, szAttackGradePoly, setFlag, setAffectFlag, szPointOn2, szPointPoly2, szDurationPoly2, setAffectFlag2, szPointOn3, szPointPoly3, szDurationPoly3, szGrandMasterAddSPCostPoly, prerequisiteSkillVnum, prerequisiteSkillLevel, eSkillType, iMaxHit, szSplashAroundDamageAdjustPoly, dwTargetRange, dwSplashRange) VALUES (263, 'SLOW4000', 0, 1, 1, 0, 'HP', '-5*k*atk', '', '', '', '12', '', '', 'ATTACK,USE_MELEE_DAMAGE,SPLASH', 'NONE', 'MOV_SPEED', '-70', '3', 'NONE', 'NONE', '', '', '', 0, 0, 'MELEE', 0, '1', 2000, 6000);
DROP TEMPORARY TABLE IF EXISTS world.ochao_mob;
CREATE TEMPORARY TABLE world.ochao_mob AS SELECT * FROM world.mob_proto WHERE vnum = 3301 LIMIT 1;
UPDATE world.ochao_mob SET vnum = 6301, name = 'Wojownik Ochao', locale_name = 'Wojownik Ochao', level = 98, max_hp = 18500, damage_min = 190, damage_max = 232, exp = 21000, gold_min = 1200, gold_max = 1800, def = 105, dam_multiply = 2.2, drop_item = 0, resurrection_vnum = 0;
INSERT IGNORE INTO world.mob_proto SELECT * FROM world.ochao_mob;
DELETE FROM world.ochao_mob; INSERT INTO world.ochao_mob SELECT * FROM world.mob_proto WHERE vnum = 3302 LIMIT 1;
UPDATE world.ochao_mob SET vnum = 6302, name = 'Żołnierz Ochao', locale_name = 'Żołnierz Ochao', level = 99, max_hp = 23000, damage_min = 192, damage_max = 236, exp = 25000, gold_min = 1300, gold_max = 1950, def = 120, dam_multiply = 2.4, drop_item = 0, resurrection_vnum = 0;
INSERT IGNORE INTO world.mob_proto SELECT * FROM world.ochao_mob;
DELETE FROM world.ochao_mob; INSERT INTO world.ochao_mob SELECT * FROM world.mob_proto WHERE vnum = 3303 LIMIT 1;
UPDATE world.ochao_mob SET vnum = 6303, name = 'Mag Ochao', locale_name = 'Mag Ochao', level = 100, max_hp = 22000, damage_min = 194, damage_max = 238, exp = 25500, gold_min = 1300, gold_max = 1950, def = 120, dam_multiply = 2.4, drop_item = 0, resurrection_vnum = 0;
INSERT IGNORE INTO world.mob_proto SELECT * FROM world.ochao_mob;
DELETE FROM world.ochao_mob; INSERT INTO world.ochao_mob SELECT * FROM world.mob_proto WHERE vnum = 3304 LIMIT 1;
UPDATE world.ochao_mob SET vnum = 6304, name = 'Kat Ochao', locale_name = 'Kat Ochao', level = 101, max_hp = 52000, damage_min = 176, damage_max = 262, exp = 52000, gold_min = 2000, gold_max = 3000, def = 142, dam_multiply = 2.8, drop_item = 0, resurrection_vnum = 0;
INSERT IGNORE INTO world.mob_proto SELECT * FROM world.ochao_mob;
DELETE FROM world.ochao_mob; INSERT INTO world.ochao_mob SELECT * FROM world.mob_proto WHERE vnum = 3305 LIMIT 1;
UPDATE world.ochao_mob SET vnum = 6305, name = 'Generał Ochao', locale_name = 'Generał Ochao', level = 102, max_hp = 95000, damage_min = 178, damage_max = 266, exp = 95000, gold_min = 2600, gold_max = 3900, def = 172, dam_multiply = 3.2, drop_item = 0, resurrection_vnum = 0;
INSERT IGNORE INTO world.mob_proto SELECT * FROM world.ochao_mob;
DELETE FROM world.ochao_mob; INSERT INTO world.ochao_mob SELECT * FROM world.mob_proto WHERE vnum = 3391 LIMIT 1;
UPDATE world.ochao_mob SET vnum = 6311, name = 'Ochroniarz Ochao', locale_name = 'Ochroniarz Ochao', rank = 4, level = 103, max_hp = 500000, damage_min = 155, damage_max = 285, exp = 220000, gold_min = 30000, gold_max = 45000, def = 210, dam_multiply = 3.4, summon = 6304, skill_vnum0 = 263, skill_level0 = 20, skill_vnum1 = 0, skill_level1 = 0, drop_item = 0, resurrection_vnum = 0;
INSERT IGNORE INTO world.mob_proto SELECT * FROM world.ochao_mob;
DELETE FROM world.ochao_mob; INSERT INTO world.ochao_mob SELECT * FROM world.mob_proto WHERE vnum = 3390 LIMIT 1;
UPDATE world.ochao_mob SET vnum = 6390, name = 'Władca Ochao', locale_name = 'Władca Ochao', rank = 5, level = 105, max_hp = 1100000, damage_min = 160, damage_max = 290, exp = 520000, gold_min = 50000, gold_max = 75000, def = 240, dam_multiply = 3.8, summon = 6304, skill_vnum0 = 263, skill_level0 = 20, drop_item = 0, resurrection_vnum = 0;
INSERT IGNORE INTO world.mob_proto SELECT * FROM world.ochao_mob;
DELETE FROM world.ochao_mob; INSERT INTO world.ochao_mob SELECT * FROM world.mob_proto WHERE vnum = 3304 LIMIT 1;
UPDATE world.ochao_mob SET vnum = 6400, name = 'Strażnik En-Tai', locale_name = 'Strażnik En-Tai', rank = 4, level = 105, folder = 'trent_officer', max_hp = 600000, damage_min = 155, damage_max = 285, exp = 260000, gold_min = 20000, gold_max = 30000, def = 206, dam_multiply = 3.4, summon = 6302, skill_vnum0 = 263, skill_level0 = 20, regen_cycle = 10, regen_percent = 10, drop_item = 0, resurrection_vnum = 0;
INSERT IGNORE INTO world.mob_proto SELECT * FROM world.ochao_mob;
DELETE FROM world.ochao_mob; INSERT INTO world.ochao_mob SELECT * FROM world.mob_proto WHERE vnum = 9012 LIMIT 1;
UPDATE world.ochao_mob SET vnum = 20415, name = 'Portal', locale_name = 'Portal', folder = '';
INSERT IGNORE INTO world.mob_proto SELECT * FROM world.ochao_mob;
UPDATE world.ochao_mob SET vnum = 20426, name = 'Strażnik Świątyni', locale_name = 'Strażnik Świątyni';
INSERT IGNORE INTO world.mob_proto SELECT * FROM world.ochao_mob;
DROP TEMPORARY TABLE IF EXISTS world.ochao_mob;" || echo "[playerbot-migrate] WARNING: could not add the Temple of Ochao's monsters, NPCs and mob skill" >&2
# MT2009_PLUS_DUNGEONS_V1: the Razador (Czysciec Ognia, map 351, level 55) and Nemere (Lodowa
# Kraina, map 352, level 75) dungeons (quest/razador_dungeon.quest, quest/nemere_dungeon.quest,
# game/dungeons/). Their monsters get Polish names and the stats of their level band - harder
# than the Demon Tower, Razador below and Nemere above Azrael's Catacomb (DT -> Razador ->
# Azrael -> Nemere); the Metin of Frost (8058), the Ice Pillar (20399), the stone lion (20397)
# and the ice seals (20398) are added as copies of the flame dungeon's rows, the three Nemere
# keys (30760-30762) as copies of the Golden Cog Wheel (30329). PROTO_FROM_DB: read at the db
# core's boot. Idempotent: the same values every start, rows added once.
# MT2009_PLUS_DUNGEON_DROP_V1: Szel (6151) hits twice as hard, dam_multiply 2.6 -> 5.2 (the
# owner, 29 September: +100% attack).
db -e "UPDATE world.mob_proto SET name = _cp1250 X'447563682050B36F6D69656E6961', locale_name = _cp1250 X'447563682050B36F6D69656E6961', level = 56, st = 75, dx = 80, ht = 70, iq = 30, damage_min = 150, damage_max = 180, max_hp = 5500, def = 70, exp = 1100, dam_multiply = 1.5, sp_berserk = 0, sp_stoneskin = 0, sp_deathblow = 0, sp_revive = 0 WHERE vnum = 6001;
UPDATE world.mob_proto SET name = _cp1250 X'50B36F6D69656E6E7920547967727973', locale_name = _cp1250 X'50B36F6D69656E6E7920547967727973', level = 57, st = 78, dx = 80, ht = 72, iq = 30, damage_min = 155, damage_max = 185, max_hp = 6500, def = 72, exp = 1200, dam_multiply = 1.5, sp_berserk = 0, sp_stoneskin = 0, sp_deathblow = 0, sp_revive = 0 WHERE vnum = 6002;
UPDATE world.mob_proto SET name = _cp1250 X'50B36F6D69656E6E7920576F6A6F776E696B', locale_name = _cp1250 X'50B36F6D69656E6E7920576F6A6F776E696B', level = 58, st = 80, dx = 80, ht = 74, iq = 30, damage_min = 160, damage_max = 195, max_hp = 7500, def = 74, exp = 1350, dam_multiply = 1.6, sp_berserk = 0, sp_stoneskin = 0, sp_deathblow = 0, sp_revive = 0 WHERE vnum = 6003;
UPDATE world.mob_proto SET name = _cp1250 X'50B36F6D69656E6E792052796365727A', locale_name = _cp1250 X'50B36F6D69656E6E792052796365727A', level = 59, st = 82, dx = 80, ht = 76, iq = 30, damage_min = 165, damage_max = 200, max_hp = 8500, def = 76, exp = 1450, dam_multiply = 1.6, sp_berserk = 0, sp_stoneskin = 0, sp_deathblow = 0, sp_revive = 0 WHERE vnum = 6004;
UPDATE world.mob_proto SET name = _cp1250 X'4B72F36C2050B36F6D69656E69', locale_name = _cp1250 X'4B72F36C2050B36F6D69656E69', level = 60, st = 85, dx = 85, ht = 80, iq = 30, damage_min = 170, damage_max = 210, max_hp = 10000, def = 78, exp = 2000, dam_multiply = 1.8, sp_berserk = 0, sp_stoneskin = 0, sp_deathblow = 0, sp_revive = 0 WHERE vnum = 6005;
UPDATE world.mob_proto SET name = _cp1250 X'4F676E6973747920476F6C656D', locale_name = _cp1250 X'4F676E6973747920476F6C656D', level = 61, st = 88, dx = 85, ht = 84, iq = 30, damage_min = 175, damage_max = 220, max_hp = 11000, def = 80, exp = 2150, dam_multiply = 1.8, sp_berserk = 0, sp_stoneskin = 0, sp_deathblow = 0, sp_revive = 0 WHERE vnum = 6006;
UPDATE world.mob_proto SET name = _cp1250 X'4F676E6973747920476F6C656D204D6167', locale_name = _cp1250 X'4F676E6973747920476F6C656D204D6167', level = 62, st = 90, dx = 85, ht = 88, iq = 35, damage_min = 180, damage_max = 230, max_hp = 12000, def = 82, exp = 2300, dam_multiply = 1.8, sp_berserk = 0, sp_stoneskin = 0, sp_deathblow = 0, sp_revive = 0 WHERE vnum = 6007;
UPDATE world.mob_proto SET name = _cp1250 X'47656E657261B320476F6C656DF377', locale_name = _cp1250 X'47656E657261B320476F6C656DF377', level = 63, st = 95, dx = 90, ht = 94, iq = 35, damage_min = 190, damage_max = 240, max_hp = 14000, def = 85, exp = 3800, dam_multiply = 2.0, sp_berserk = 0, sp_stoneskin = 0, sp_deathblow = 0, sp_revive = 0 WHERE vnum = 6008;
UPDATE world.mob_proto SET name = _cp1250 X'57F3647A20476F6C656DF377', locale_name = _cp1250 X'57F3647A20476F6C656DF377', level = 64, st = 100, dx = 90, ht = 100, iq = 35, damage_min = 195, damage_max = 250, max_hp = 15500, def = 88, exp = 4200, dam_multiply = 2.0, sp_berserk = 0, sp_stoneskin = 0, sp_deathblow = 0, sp_revive = 0 WHERE vnum = 6009;
UPDATE world.mob_proto SET name = _cp1250 X'49676E69746F72', locale_name = _cp1250 X'49676E69746F72', level = 65, st = 110, dx = 95, ht = 110, iq = 40, damage_min = 210, damage_max = 270, max_hp = 120000, def = 90, exp = 30000, dam_multiply = 2.4, sp_berserk = 10, sp_stoneskin = 10, sp_deathblow = 10, sp_revive = 0 WHERE vnum = 6051;
UPDATE world.mob_proto SET name = _cp1250 X'52617A61646F72', locale_name = _cp1250 X'52617A61646F72', level = 68, st = 125, dx = 100, ht = 130, iq = 45, damage_min = 230, damage_max = 310, max_hp = 700000, def = 95, exp = 180000, dam_multiply = 2.6, sp_berserk = 15, sp_stoneskin = 15, sp_deathblow = 10, sp_revive = 0 WHERE vnum = 6091;
UPDATE world.mob_proto SET name = _cp1250 X'4D726F9F6E79204B7279737A7461B3', locale_name = _cp1250 X'4D726F9F6E79204B7279737A7461B3', level = 78, st = 115, dx = 100, ht = 100, iq = 40, damage_min = 240, damage_max = 290, max_hp = 9000, def = 90, exp = 950, dam_multiply = 1.6, sp_berserk = 0, sp_stoneskin = 0, sp_deathblow = 0, sp_revive = 0 WHERE vnum = 6101;
UPDATE world.mob_proto SET name = _cp1250 X'4D726F9F6E79204F776164', locale_name = _cp1250 X'4D726F9F6E79204F776164', level = 79, st = 118, dx = 100, ht = 105, iq = 40, damage_min = 245, damage_max = 300, max_hp = 10500, def = 92, exp = 1050, dam_multiply = 1.6, sp_berserk = 0, sp_stoneskin = 0, sp_deathblow = 0, sp_revive = 0 WHERE vnum = 6102;
UPDATE world.mob_proto SET name = _cp1250 X'4D726F9F6E7920437AB36F7769656B', locale_name = _cp1250 X'4D726F9F6E7920437AB36F7769656B', level = 80, st = 120, dx = 100, ht = 110, iq = 40, damage_min = 250, damage_max = 310, max_hp = 12000, def = 94, exp = 1200, dam_multiply = 1.7, sp_berserk = 0, sp_stoneskin = 0, sp_deathblow = 0, sp_revive = 0 WHERE vnum = 6103;
UPDATE world.mob_proto SET name = _cp1250 X'4D726F9F6E792059657469', locale_name = _cp1250 X'4D726F9F6E792059657469', level = 81, st = 122, dx = 100, ht = 115, iq = 40, damage_min = 255, damage_max = 320, max_hp = 13500, def = 96, exp = 1300, dam_multiply = 1.7, sp_berserk = 0, sp_stoneskin = 0, sp_deathblow = 0, sp_revive = 0 WHERE vnum = 6104;
UPDATE world.mob_proto SET name = _cp1250 X'4D726F9F6E7920476F6C656D', locale_name = _cp1250 X'4D726F9F6E7920476F6C656D', level = 82, st = 125, dx = 105, ht = 120, iq = 40, damage_min = 260, damage_max = 335, max_hp = 15000, def = 98, exp = 1750, dam_multiply = 1.9, sp_berserk = 0, sp_stoneskin = 0, sp_deathblow = 0, sp_revive = 0 WHERE vnum = 6105;
UPDATE world.mob_proto SET name = _cp1250 X'4D726F9F6E792054726F6C6C', locale_name = _cp1250 X'4D726F9F6E792054726F6C6C', level = 83, st = 128, dx = 105, ht = 125, iq = 40, damage_min = 265, damage_max = 345, max_hp = 17000, def = 100, exp = 1850, dam_multiply = 1.9, sp_berserk = 0, sp_stoneskin = 0, sp_deathblow = 0, sp_revive = 0 WHERE vnum = 6106;
UPDATE world.mob_proto SET name = _cp1250 X'4C6F646F777920476F6C656D204D6167', locale_name = _cp1250 X'4C6F646F777920476F6C656D204D6167', level = 84, st = 130, dx = 105, ht = 130, iq = 45, damage_min = 270, damage_max = 355, max_hp = 18500, def = 102, exp = 1950, dam_multiply = 1.9, sp_berserk = 0, sp_stoneskin = 0, sp_deathblow = 0, sp_revive = 0 WHERE vnum = 6107;
UPDATE world.mob_proto SET name = _cp1250 X'4D726F9F6E792047656E657261B3', locale_name = _cp1250 X'4D726F9F6E792047656E657261B3', level = 86, st = 135, dx = 110, ht = 135, iq = 45, damage_min = 280, damage_max = 375, max_hp = 22000, def = 105, exp = 3400, dam_multiply = 2.1, sp_berserk = 0, sp_stoneskin = 0, sp_deathblow = 0, sp_revive = 0 WHERE vnum = 6108;
UPDATE world.mob_proto SET name = _cp1250 X'57B361646361204D726F7A75', locale_name = _cp1250 X'57B361646361204D726F7A75', level = 88, st = 140, dx = 110, ht = 140, iq = 45, damage_min = 290, damage_max = 390, max_hp = 26000, def = 108, exp = 3700, dam_multiply = 2.1, sp_berserk = 0, sp_stoneskin = 0, sp_deathblow = 0, sp_revive = 0 WHERE vnum = 6109;
UPDATE world.mob_proto SET name = _cp1250 X'537A656C', locale_name = _cp1250 X'537A656C', level = 88, st = 145, dx = 115, ht = 150, iq = 50, damage_min = 300, damage_max = 400, max_hp = 220000, def = 110, exp = 40000, dam_multiply = 5.2, sp_berserk = 15, sp_stoneskin = 15, sp_deathblow = 15, sp_revive = 0 WHERE vnum = 6151;
UPDATE world.mob_proto SET name = _cp1250 X'4E656D657265', locale_name = _cp1250 X'4E656D657265', level = 95, st = 160, dx = 125, ht = 170, iq = 60, damage_min = 320, damage_max = 450, max_hp = 1500000, def = 115, exp = 420000, dam_multiply = 3.2, sp_berserk = 20, sp_stoneskin = 20, sp_deathblow = 15, sp_revive = 0 WHERE vnum = 6191;
UPDATE world.mob_proto SET name = _cp1250 X'4D6574696E20437A799CE66361', locale_name = _cp1250 X'4D6574696E20437A799CE66361', level = 60, max_hp = 280000, def = 70, exp = 40 WHERE vnum = 8057;
UPDATE world.mob_proto SET name = _cp1250 X'506F73B96720416D2D686568', locale_name = _cp1250 X'506F73B96720416D2D686568' WHERE vnum = 20385;
UPDATE world.mob_proto SET name = _cp1250 X'5374656C61204973666574', locale_name = _cp1250 X'5374656C61204973666574' WHERE vnum = 20386;
UPDATE world.mob_proto SET name = _cp1250 X'5A616D656B2050727A657A6E61637A656E6961', locale_name = _cp1250 X'5A616D656B2050727A657A6E61637A656E6961' WHERE vnum = 20387;
UPDATE world.mob_proto SET name = _cp1250 X'536D6F637A61204272616D61', locale_name = _cp1250 X'536D6F637A61204272616D61' WHERE vnum = 20388;
UPDATE world.mob_proto SET name = _cp1250 X'53747261BF6E696B204F676E697374656A205A69656D69', locale_name = _cp1250 X'53747261BF6E696B204F676E697374656A205A69656D69' WHERE vnum = 20394;
UPDATE world.mob_proto SET name = _cp1250 X'53747261BF6E696B204C6F646F77656A204B7261696E79', locale_name = _cp1250 X'53747261BF6E696B204C6F646F77656A204B7261696E79' WHERE vnum = 20395;
DROP TEMPORARY TABLE IF EXISTS world.dg_mob;
CREATE TEMPORARY TABLE world.dg_mob AS SELECT * FROM world.mob_proto WHERE vnum = 8057 LIMIT 1;
UPDATE world.dg_mob SET vnum = 8058, name = _cp1250 X'4D6574696E204D726F7A75', locale_name = _cp1250 X'4D6574696E204D726F7A75', level = 85, max_hp = 450000, def = 85, exp = 55, ht = 100;
INSERT IGNORE INTO world.mob_proto SELECT * FROM world.dg_mob;
DROP TEMPORARY TABLE world.dg_mob;
CREATE TEMPORARY TABLE world.dg_mob AS SELECT * FROM world.mob_proto WHERE vnum = 8057 LIMIT 1;
UPDATE world.dg_mob SET vnum = 20399, name = _cp1250 X'4C6F646F77792046696C6172', locale_name = _cp1250 X'4C6F646F77792046696C6172', level = 85, max_hp = 350000, def = 85, exp = 55, ht = 100, folder = 'ice_stonepillar';
INSERT IGNORE INTO world.mob_proto SELECT * FROM world.dg_mob;
DROP TEMPORARY TABLE world.dg_mob;
CREATE TEMPORARY TABLE world.dg_mob AS SELECT * FROM world.mob_proto WHERE vnum = 20385 LIMIT 1;
UPDATE world.dg_mob SET vnum = 20397, name = _cp1250 X'4B616D69656E6E79204C6577', locale_name = _cp1250 X'4B616D69656E6E79204C6577', folder = 'ICE_lionstone';
INSERT IGNORE INTO world.mob_proto SELECT * FROM world.dg_mob;
DROP TEMPORARY TABLE world.dg_mob;
CREATE TEMPORARY TABLE world.dg_mob AS SELECT * FROM world.mob_proto WHERE vnum = 20386 LIMIT 1;
UPDATE world.dg_mob SET vnum = 20398, name = _cp1250 X'4C6F646F776120506965637AEAE6', locale_name = _cp1250 X'4C6F646F776120506965637AEAE6', folder = 'ice_keybox';
INSERT IGNORE INTO world.mob_proto SELECT * FROM world.dg_mob;
DROP TEMPORARY TABLE world.dg_mob;
DROP TEMPORARY TABLE IF EXISTS world.dg_item;
CREATE TEMPORARY TABLE world.dg_item AS SELECT * FROM world.item_proto WHERE vnum = 30329 LIMIT 1;
UPDATE world.dg_item SET vnum = 30760, name = _cp1250 X'4C6F646F7779204B6C75637A', locale_name = _cp1250 X'4C6F646F7779204B6C75637A', stack = 200, antiflag = 0, flag = 4;
INSERT IGNORE INTO world.item_proto SELECT * FROM world.dg_item;
DROP TEMPORARY TABLE world.dg_item;
CREATE TEMPORARY TABLE world.dg_item AS SELECT * FROM world.item_proto WHERE vnum = 30329 LIMIT 1;
UPDATE world.dg_item SET vnum = 30761, name = _cp1250 X'4B7279737A7461B3204C6F6475', locale_name = _cp1250 X'4B7279737A7461B3204C6F6475', stack = 200, antiflag = 0, flag = 4;
INSERT IGNORE INTO world.item_proto SELECT * FROM world.dg_item;
DROP TEMPORARY TABLE world.dg_item;
CREATE TEMPORARY TABLE world.dg_item AS SELECT * FROM world.item_proto WHERE vnum = 30329 LIMIT 1;
UPDATE world.dg_item SET vnum = 30762, name = _cp1250 X'4B6C75637A204D726F7A75', locale_name = _cp1250 X'4B6C75637A204D726F7A75', stack = 200, antiflag = 0, flag = 4;
INSERT IGNORE INTO world.item_proto SELECT * FROM world.dg_item;
DROP TEMPORARY TABLE world.dg_item;" || echo "[playerbot-migrate] WARNING: could not set up the Razador and Nemere dungeons' monsters and keys" >&2

# MT2009_PLUS_GOBLIN_V1: the Treasure Hunt event (playerbot_goblin.h, the events
# file's kind "goblin"): the Treasure Ticket (70617, from chests while the
# event runs - it takes a player of level 70 to Treasure Island), the Goblin
# Key (70618, a reward of the Doubloon board after its first) and the Goblin
# Key Box (70619, eight keys, the fourth round's reward); the Treasure Goblin
# (20856, a NPC the island's monsters strike) and the Giant Treasure Chest
# (20857). Bound to the character, as the archive's. Idempotent: added once.
db -e "INSERT IGNORE INTO world.item_proto (vnum, name, locale_name, type, subtype, stack, weight, size, antiflag, flag, wearflag, immuneflag, gold, shop_buy_price, refined_vnum, refine_set, magic_pct, specular, socket_pct, addon_type, limittype0, limitvalue0, limittype1, limitvalue1, applytype0, applyvalue0, applytype1, applyvalue1, applytype2, applyvalue2, value0, value1, value2, value3, value4, value5, socket0, socket1, socket2, socket3, socket4, socket5) VALUES
(70617, 'Treasure Ticket', _cp1250 X'42696C657420536B617262F377', 3, 10, 200, 0, 1, 221312, 8196, 0, '', 0, 0, 0, 0, 0, 0, 0, 0, 1, 70, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, -1, -1, -1, -1, -1),
(70618, 'Goblin Key', _cp1250 X'4B6C75637A20476F626C696E61', 3, 10, 200, 0, 1, 221312, 8196, 0, '', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, -1, -1, -1, -1, -1),
(70619, 'Goblin Key Box', _cp1250 X'537A6B617475B36B61207A204B6C75637A616D6920476F626C696E61', 3, 10, 200, 0, 1, 221312, 8196, 0, '', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, -1, -1, -1, -1, -1);
DROP TEMPORARY TABLE IF EXISTS world.gob_mob;
CREATE TEMPORARY TABLE world.gob_mob AS SELECT * FROM world.mob_proto WHERE vnum = 20005 LIMIT 1;
UPDATE world.gob_mob SET vnum = 20856, name = _cp1250 X'476F626C696E20536B617262F377', locale_name = _cp1250 X'476F626C696E20536B617262F377', rank = 0, type = 1, level = 1, ai_flag = '', setImmuneFlag = 'SLOW,TERROR', folder = 'treasure_hunt_goblin', on_click = 0, max_hp = 180000, regen_cycle = 0, regen_percent = 0, exp = 0, def = 0, move_speed = 100, attack_speed = 100;
INSERT IGNORE INTO world.mob_proto SELECT * FROM world.gob_mob;
DROP TEMPORARY TABLE world.gob_mob;
CREATE TEMPORARY TABLE world.gob_mob AS SELECT * FROM world.mob_proto WHERE vnum = 20005 LIMIT 1;
UPDATE world.gob_mob SET vnum = 20857, name = _cp1250 X'5769656C6B6120536B727A796E696120536B617262F377', locale_name = _cp1250 X'5769656C6B6120536B727A796E696120536B617262F377', rank = 0, type = 1, level = 1, ai_flag = 'NOMOVE', setImmuneFlag = 'STUN,SLOW,TERROR', folder = 'treasure_hunt_box', on_click = 0, exp = 0;
INSERT IGNORE INTO world.mob_proto SELECT * FROM world.gob_mob;
DROP TEMPORARY TABLE world.gob_mob;" || echo "[playerbot-migrate] WARNING: could not add the Treasure Hunt's items and its goblin" >&2

# MT2009_PLUS_COSTUME_BONUS_V1: costume bonuses. The engine rolls a costume's
# bonuses from item_attr's costume_body / costume_hair / costume_weapon sets
# (ENABLE_ITEM_ATTR_COSTUME), and the base dump ships all three at zero: an
# ItemShop costume "(bonus)" / "+" (magic_pct 100) came without a bonus and
# Transformuj kostium (70063) rolled nothing ("PutAttributeWithLevel: Cannot put
# item attribute 8 1" in syserr). Seeded once, only while every costume column
# is still zero, from the sets costumes used before (body / head / weapon) -
# the operator's own values are never overwritten. And the General Store
# (shop 3, NPC 9003) sells the three costume items: 70063 Transformuj kostium,
# 70064 Zaczaruj kostium, 70065 Transfer bonusow. Idempotent.
db -e "SET @m2_costume_sets := (SELECT COALESCE(SUM(costume_body + costume_hair + costume_weapon), 0) FROM world.item_attr);
UPDATE world.item_attr SET costume_body = body, costume_hair = head, costume_weapon = weapon WHERE @m2_costume_sets = 0;
INSERT IGNORE INTO world.shop_item (shop_vnum, item_vnum, count) VALUES (3, 70065, 1), (3, 70064, 20), (3, 70063, 20);" || echo "[playerbot-migrate] WARNING: could not set up the costume bonus sets and the General Store's costume items" >&2

# MT2009_PLUS_BOSS_CHESTS_V1: the Razador and Nemere dungeons' boss chests, at the
# official vnums - Skrzynia Razadora (50270) and Skrzynia Nemere (50271), gift boxes
# (type 23) that stack, opened by their special_item_group groups
# (game/special_item_group.dungeons.txt); razador_dungeon.quest and nemere_dungeon.quest
# hand them out when the boss dies. Idempotent: added once, never changed after.
db -e "INSERT IGNORE INTO world.item_proto (vnum, name, locale_name, type, subtype, stack, weight, size, antiflag, flag, wearflag, immuneflag, gold, shop_buy_price, refined_vnum, refine_set, magic_pct, specular, socket_pct, addon_type, limittype0, limitvalue0, limittype1, limitvalue1, applytype0, applyvalue0, applytype1, applyvalue1, applytype2, applyvalue2, value0, value1, value2, value3, value4, value5, socket0, socket1, socket2, socket3, socket4, socket5) VALUES
(50270, 'Skrzynia Razadora', 'Skrzynia Razadora', 23, 0, 200, 0, 1, 0, 4, 0, '', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, -1, -1, -1, -1, -1),
(50271, 'Skrzynia Nemere', 'Skrzynia Nemere', 23, 0, 200, 0, 1, 0, 4, 0, '', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, -1, -1, -1, -1, -1);" || echo "[playerbot-migrate] WARNING: could not add the Razador and Nemere boss chests" >&2
