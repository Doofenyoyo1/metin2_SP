#!/bin/sh
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
echo "[playerbot-migrate] waiting for the complete r40250 schema"
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
            OR (table_schema='log'     AND table_name='speed_hack');
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
       AND p.map_index NOT IN (1, 3, 4, 5, 21, 23, 24, 25, 41, 43, 44, 45,
                               108, 109, 61, 63, 64, 104, 65, 71);
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
           AND p.map_index NOT IN (1, 3, 4, 5, 21, 23, 24, 25, 41, 43, 44, 45,
                                   108, 109, 61, 63, 64, 104, 65, 71);
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
if { printf 'SET @playerbot_seed_kingdoms = %s;
' "$kingdoms"; cat "$seed"; } |
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
