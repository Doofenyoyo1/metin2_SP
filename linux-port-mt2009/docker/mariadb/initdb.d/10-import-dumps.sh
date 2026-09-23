#!/bin/bash
# =============================================================================
#  First-run database import for the mt2009 server files.
#
#  The official MariaDB image runs everything in /docker-entrypoint-initdb.d
#  exactly once -- when the data directory is empty -- so this runs on the very
#  first `docker compose up' and never again.
#
#  The package ships five per-database dumps (account, common, player, log,
#  world) made by `mariadb-dump 11.8' with no USE and no CREATE DATABASE, so
#  each is fed to a named database here. The db core also opens SQL_HOTBACKUP
#  at boot, so a sixth, empty database is created for it.
#
#  Three of the dumped tables use utf8mb3_uca1400_ai_ci, a collation MariaDB
#  only has from 11.4; the compose file pins mariadb:11.8 for that reason.
# =============================================================================
set -euo pipefail

DUMP_DIR=/docker-entrypoint-initdb.d/dumps
DATABASES="account common player log world hotbackup"

mysql_do() { mariadb --protocol=socket -uroot -p"${MARIADB_ROOT_PASSWORD}" "$@"; }

# Every input checked before anything durable is created -- initdb.d runs once
# per volume, and a half-built world behind a green healthcheck is the worst
# outcome. Reading one byte is the only honest readability test on a bind
# mount that carries the host's own modes.
[ -d "$DUMP_DIR" ] || { echo "[initdb] FATAL: $DUMP_DIR does not exist."; exit 1; }

_missing=""; _unreadable=""; _empty=""
for d in $DATABASES; do
  [ "$d" = "hotbackup" ] && continue
  f="$DUMP_DIR/$d.sql"
  if [ ! -f "$f" ]; then _missing="$_missing $d.sql"
  elif ! head -c 1 "$f" >/dev/null 2>&1; then _unreadable="$_unreadable $d.sql"
  elif [ ! -s "$f" ]; then _empty="$_empty $d.sql"
  fi
done
if [ -n "$_missing" ] || [ -n "$_unreadable" ] || [ -n "$_empty" ]; then
  echo "[initdb] FATAL: the SQL dumps cannot be imported; nothing has been created."
  [ -n "$_missing" ]    && echo "[initdb]   missing:    $_missing"
  [ -n "$_unreadable" ] && echo "[initdb]   unreadable: $_unreadable"
  [ -n "$_empty" ]      && echo "[initdb]   empty:      $_empty"
  echo "[initdb] They live in linux-port-mt2009/docker/mariadb/initdb.d/dumps on the host."
  echo "[initdb] Then remove this volume and start again: initdb runs once per volume."
  exit 1
fi

echo "[initdb] creating databases and the game's SQL user"

# The game speaks cp1250 (locale_service.cpp, __LocaleService_Init_Poland sets
# g_stLocale = "cp1250" and every connection does SET NAMES with it), and the
# dumped tables each carry their own charset. The database default only reaches
# tables created later -- the playerbot seed's, the panels' -- and cp1250 is
# what the text in them will be.
{
  for d in $DATABASES; do
    echo "CREATE DATABASE IF NOT EXISTS \`$d\` DEFAULT CHARACTER SET cp1250 COLLATE cp1250_general_ci;"
  done
  echo "CREATE USER IF NOT EXISTS '${M2_DB_USER}'@'%' IDENTIFIED BY '${M2_DB_PASSWORD}';"
  echo "ALTER USER '${M2_DB_USER}'@'%' IDENTIFIED BY '${M2_DB_PASSWORD}';"
  for d in $DATABASES; do
    echo "GRANT ALL PRIVILEGES ON \`$d\`.* TO '${M2_DB_USER}'@'%';"
  done
  echo "FLUSH PRIVILEGES;"
} | mysql_do

echo "[initdb] importing dumps from $DUMP_DIR"
for d in $DATABASES; do
  [ "$d" = "hotbackup" ] && continue
  f="$DUMP_DIR/$d.sql"
  echo "[initdb]   importing $d ($(du -h "$f" | cut -f1))"
  mysql_do "$d" < "$f"
done

# The panels, the seed's readiness probe and the tools were written for the
# r40250 layout, where the protos sat in `player'. Here they live in `world'
# (PROTO_FROM_DB), so `player' carries two views over them: same columns,
# same names, nothing to rewrite, and a REPLACE INTO from the db core can
# never hit them because with PROTO_FROM_DB it mirrors nothing.
echo "[initdb] creating the player.item_proto / player.mob_proto views over world"
mysql_do <<'SQL'
CREATE OR REPLACE VIEW player.item_proto AS SELECT * FROM world.item_proto;
CREATE OR REPLACE VIEW player.mob_proto  AS SELECT * FROM world.mob_proto;
SQL

# The package declares account.social_id varchar(7); the engine's own limit
# (SOCIAL_ID_MAX_LEN) is 18 and the playerbot seed's ids are thirteen digits.
# Under NO_ENGINE_SUBSTITUTION a longer value is cut without a word, so every
# bot got '9000000' and the seed refused itself. Widened to what the engine
# reads; a player's seven-digit id is untouched.
echo "[initdb] widening account.social_id to the engine's 18 characters"
mysql_do -e "ALTER TABLE account.account MODIFY social_id VARCHAR(18) NOT NULL DEFAULT '';"

# The ItemShop's two currencies. r40250's account table carries mileage (and
# the shop reads jackpot beside it); this one has cash only, and the shop's
# first query dies on 'Unknown column a.mileage'.
echo "[initdb] adding the ItemShop's mileage / jackpot columns to account.account"
mysql_do <<'SQL'
ALTER TABLE account.account ADD COLUMN IF NOT EXISTS mileage INT NOT NULL DEFAULT 0;
ALTER TABLE account.account ADD COLUMN IF NOT EXISTS jackpot INT NOT NULL DEFAULT 0;
SQL

# 20-log-schema.sql (the log tables the engine writes that the dump lacks) is
# run by the image itself right after this script, in name order.

echo "[initdb] verifying"
for d in $DATABASES; do
  n=$(mysql_do -N -B -e "SELECT COUNT(*) FROM information_schema.tables WHERE table_schema='$d'")
  echo "[initdb]   $d: $n tables"
done

protos=$(mysql_do -N -B -e "SELECT COUNT(*) FROM world.item_proto" 2>/dev/null || echo 0)
mobs=$(mysql_do -N -B -e "SELECT COUNT(*) FROM world.mob_proto" 2>/dev/null || echo 0)
echo "[initdb]   world.item_proto: $protos rows, world.mob_proto: $mobs rows"
if [ "$protos" -lt 1 ] || [ "$mobs" -lt 1 ]; then
  echo "[initdb] FATAL: the protos are empty -- the import did not take."
  exit 1
fi

# -----------------------------------------------------------------------------
#  A first account. The package ships an empty account table, and the game has
#  no registration of its own, so without this nobody can log in at all.
#  The hash is what input_auth.cpp compares against:
#  CONCAT('*', UPPER(SHA1(UNHEX(SHA1(password))))) -- MySQL's native scheme.
#  Set M2_KEEP_DEMO_ACCOUNTS=0 to skip it on a machine with a public address.
# -----------------------------------------------------------------------------
if [ "${M2_KEEP_DEMO_ACCOUNTS:-1}" != "0" ]; then
  echo "[initdb] creating the tester accounts (admin / admin, test / test)"
  mysql_do <<'SQL'
INSERT INTO account.account (login, password, social_id, status)
VALUES ('admin', CONCAT('*', UPPER(SHA1(UNHEX(SHA1('admin'))))), '1234567', 'OK'),
       ('test',  CONCAT('*', UPPER(SHA1(UNHEX(SHA1('test'))))),  '1234567', 'OK')
ON DUPLICATE KEY UPDATE status = 'OK';
SQL

  # Game masters to go with the account. The r40250 package shipped
  # `[SA]Admin' on `admin' and a gmlist row for it; this one ships an empty
  # gmlist and no character at all, so the first player to log in as admin
  # found "no GM character" (archded, 11 September). GM rights here are the
  # pair (account, character name) - the engine checks the account and never
  # the host (gm.cpp, GERMAN_GM_NOT_CHECK_HOST) - so the rows need characters
  # to name. gm_characters.sql (mounted from mariadb/playerbot, the same file
  # apply.sh runs on every start) creates one of each class at ninety with
  # a full +9 set, a level-21 horse and its summon book; it does nothing on
  # an account that already has a character.
  if [ -s /opt/playerbot/gm_characters.sql ]; then
    echo "[initdb] creating the game-master characters on the admin account"
    mysql_do < /opt/playerbot/gm_characters.sql
  else
    echo "[initdb] WARNING: /opt/playerbot/gm_characters.sql not mounted; no GM character created"
  fi
fi

echo "[initdb] database initialisation complete"
