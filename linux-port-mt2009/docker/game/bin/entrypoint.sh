#!/bin/bash
# =============================================================================
#  Container entrypoint for the Metin2 mt2009 (r41023) game server -- the
#  r40250 entrypoint unchanged but for the guard on the optional helpers.
#
#  Runs as root, does the three things that need root or need doing exactly
#  once per container start, then hands over to the supervisor as an
#  unprivileged user:
#
#     1. wait for MariaDB (the db core asserts on a missing schema and, being
#        built without -DNDEBUG, aborts rather than warns)
#     2. materialise the core tree on the state volume and render every
#        CONFIG / conf.txt from the environment
#     3. drop privileges and exec the supervisor
#
#  Every knob is an environment variable; nothing is baked into the image.
# =============================================================================
set -euo pipefail

log() { printf '%s [entrypoint] %s\n' "$(date -u '+%Y-%m-%d %H:%M:%SZ')" "$*"; }
die() { printf '%s [entrypoint] FATAL: %s\n' "$(date -u '+%Y-%m-%d %H:%M:%SZ')" "$*" >&2; exit 1; }

SHARE_DIR="${M2_SHARE_DIR:-/opt/metin2/share}"
VAR_DIR="${M2_VAR_DIR:-/opt/metin2/var}"

# -----------------------------------------------------------------------------
# Configuration contract.  See .env.example for the operator-facing version.
# -----------------------------------------------------------------------------
M2_DB_HOST="${M2_DB_HOST:-db}"
M2_DB_PORT="${M2_DB_PORT:-3306}"
M2_DB_USER="${M2_DB_USER:-metin2}"
M2_DB_PASSWORD="${M2_DB_PASSWORD:-}"

M2_CHANNELS="${M2_CHANNELS:-1}"
M2_AUTH_PORT="${M2_AUTH_PORT:-11000}"
M2_AUTH_P2P_PORT="${M2_AUTH_P2P_PORT:-12000}"
M2_DB_CORE_PORT="${M2_DB_CORE_PORT:-15000}"

M2_PUBLIC_ADDRESS="${M2_PUBLIC_ADDRESS:-}"
M2_BIND_IP="${M2_BIND_IP:-0.0.0.0}"

M2_LOG_KEEP_DAYS="${M2_LOG_KEEP_DAYS:-7}"
M2_TABLE_POSTFIX="${M2_TABLE_POSTFIX:-}"
M2_MAX_LEVEL="${M2_MAX_LEVEL:-120}"
M2_PLAYER_DELETE_LEVEL_LIMIT="${M2_PLAYER_DELETE_LEVEL_LIMIT:-70}"
M2_TEST_SERVER="${M2_TEST_SERVER:-0}"
M2_MALL_URL="${M2_MALL_URL:-}"
M2_ADMINPAGE_PASSWORD="${M2_ADMINPAGE_PASSWORD:-}"
M2_ADMINPAGE_IP="${M2_ADMINPAGE_IP:-127.0.0.1}"

# Where the panel leaves a rate change and picks the answer up. A named volume
# mounted into this container and into the panel's; see docker-compose.yml.
M2_RATES_SPOOL="${M2_RATES_SPOOL:-/opt/m2spool}"

M2_RUN_AS_ROOT="${M2_RUN_AS_ROOT:-0}"
M2_CORE_DUMPS="${M2_CORE_DUMPS:-0}"
M2_DB_WAIT_TIMEOUT="${M2_DB_WAIT_TIMEOUT:-180}"

export M2_SHARE_DIR="$SHARE_DIR" M2_VAR_DIR="$VAR_DIR"
export M2_DB_HOST M2_DB_PORT M2_DB_USER M2_DB_PASSWORD
export M2_CHANNELS M2_AUTH_PORT M2_AUTH_P2P_PORT M2_DB_CORE_PORT
export M2_PUBLIC_ADDRESS M2_BIND_IP M2_LOG_KEEP_DAYS
export M2_TABLE_POSTFIX M2_MAX_LEVEL M2_PLAYER_DELETE_LEVEL_LIMIT
export M2_TEST_SERVER M2_MALL_URL M2_ADMINPAGE_PASSWORD M2_ADMINPAGE_IP
export M2_RATES_SPOOL

# -----------------------------------------------------------------------------
# 0. Validate.  Fail loudly here rather than let a core abort ten seconds later
#    with a message only someone who has read config.cpp can interpret.
# -----------------------------------------------------------------------------
[ -n "$M2_DB_PASSWORD" ] || die "M2_DB_PASSWORD is empty. Set it in .env (see .env.example)."

# db/src/Config.cpp reads conf.txt words into a 256-byte buffer with GetWord(),
# which has no bounds check whatsoever -- an over-long value is a stack smash,
# not an error message.  The whole SQL_* line must fit, so the password is
# capped well below that.
[ "${#M2_DB_PASSWORD}" -le 120 ] \
  || die "M2_DB_PASSWORD is ${#M2_DB_PASSWORD} characters. Keep it under 120: the db core's config parser writes into a fixed 256-byte buffer without bounds checking."

# The password is written into CONFIG files as a whitespace-separated field.
case "$M2_DB_PASSWORD" in
  *[[:space:]]*|*'"'*) die 'M2_DB_PASSWORD must not contain whitespace or double quotes: the CONFIG and conf.txt parsers split on exactly those.' ;;
esac

case "$M2_CHANNELS" in
  1|2|3|4) : ;;
  *) die "M2_CHANNELS must be 1..4 (got '$M2_CHANNELS'). Channel N uses ports 13000+10*(N-1) .. +2." ;;
esac

# -----------------------------------------------------------------------------
# 0b. The second channel for bots and players (M2_PLAYERBOT_CH2; the cores split
#     the bots with playerbot_channel_rules.h). Two places may ask for it: .env,
#     which the launcher writes, and the web panel, which cannot write .env and
#     leaves its wish in the spool. The newer wins - each carries the moment it
#     was made (M2_PLAYERBOT_CH2_SET_AT, SET_AT=). Decided here, once for every
#     core: every core must give every bot the same channel, so nothing a core
#     reads for itself later may differ from its neighbours'.
# -----------------------------------------------------------------------------
M2_PLAYERBOT_CH2="${M2_PLAYERBOT_CH2:-0}"
PLAYERBOT_CH2_SHARE="${PLAYERBOT_CH2_SHARE:-40}"
ch2_env_at="${M2_PLAYERBOT_CH2_SET_AT:-0}"
case "$ch2_env_at" in ''|*[!0-9]*) ch2_env_at=0 ;; esac
ch2_source=env
ch2_at="$ch2_env_at"
ch2_wish="$M2_RATES_SPOOL/channels.wanted"
if [ -f "$ch2_wish" ]; then
  wish_on=$(sed -n 's/^CH2=\([01]\)\r\{0,1\}$/\1/p' "$ch2_wish" | head -n 1)
  wish_share=$(sed -n 's/^SHARE=\([0-9]\{1,3\}\)\r\{0,1\}$/\1/p' "$ch2_wish" | head -n 1)
  wish_at=$(sed -n 's/^SET_AT=\([0-9]\{1,12\}\)\r\{0,1\}$/\1/p' "$ch2_wish" | head -n 1)
  if [ -n "$wish_on" ] && [ -n "$wish_at" ] && [ "$wish_at" -gt "$ch2_env_at" ]; then
    M2_PLAYERBOT_CH2="$wish_on"
    [ -n "$wish_share" ] && PLAYERBOT_CH2_SHARE="$wish_share"
    ch2_source=panel
    ch2_at="$wish_at"
  fi
fi
case "$M2_PLAYERBOT_CH2" in 1) : ;; *) M2_PLAYERBOT_CH2=0 ;; esac
case "$PLAYERBOT_CH2_SHARE" in ''|*[!0-9]*) PLAYERBOT_CH2_SHARE=40 ;; esac
if [ "$M2_PLAYERBOT_CH2" = 1 ] && [ "$M2_CHANNELS" -lt 2 ]; then
  M2_CHANNELS=2
fi
[ "$M2_PLAYERBOT_CH2" = 1 ] \
  && log "second channel ON (from $ch2_source): ${PLAYERBOT_CH2_SHARE}% of the bots on CH2, shops on CH1 only"
export M2_PLAYERBOT_CH2 PLAYERBOT_CH2_SHARE M2_CHANNELS
# What the server runs with, for the panel (it reads this volume, not .env).
# The published range says whether players can reach CH2: the launcher opens
# BASE..BASE+12 (13000-13012 by default) when it switches the channel on; a wish from the panel alone
# brings the bots over at once and the players with the launcher's next start.
# SET_AT is the moment of the choice that won, so the panel can tell its own
# wish still waiting from one a later choice in the launcher has overtaken.
mkdir -p "$VAR_DIR"
# BASE is the channels' first port (M2_GAME_PORT_BASE, m2-render-config): the
# panels tell the second channel's ports from it (BASE+10..BASE+12), and a
# range that does not reach BASE+12 means CH2 is not published yet.
ch_port_base="${M2_GAME_PORT_BASE:-13000}"
case "$ch_port_base" in ''|*[!0-9]*) ch_port_base=13000 ;; esac
printf 'CH2=%s\nSHARE=%s\nCHANNELS=%s\nSOURCE=%s\nPORTS=%s\nSET_AT=%s\nBASE=%s\n' \
  "$M2_PLAYERBOT_CH2" "$PLAYERBOT_CH2_SHARE" "$M2_CHANNELS" "$ch2_source" \
  "${M2_GAME_CONTAINER_PORT_RANGE:-$ch_port_base-$((ch_port_base + 2))}" "$ch2_at" "$ch_port_base" \
  > "$VAR_DIR/channels.effective" 2>/dev/null || true

if [ -z "$M2_PUBLIC_ADDRESS" ]; then
  log "WARNING: M2_PUBLIC_ADDRESS is not set."
  log "         The cores will advertise their container-internal address, which"
  log "         no client outside this host can reach. Players will reach the"
  log "         login server and then fail to enter the world. Set it in .env."
fi

# -----------------------------------------------------------------------------
# 1. Wait for MariaDB.
#
#    Not a nicety: every core opens its SQL handles during boot, and db/ is
#    compiled without -DNDEBUG, so a missing table is an assert() and an abort,
#    not a warning.  Starting into a half-initialised database produces a crash
#    that looks like a code fault.  bash's /dev/tcp is used so the runtime image
#    needs no netcat and no mysql client.
# -----------------------------------------------------------------------------
log "waiting for MariaDB at ${M2_DB_HOST}:${M2_DB_PORT} (timeout ${M2_DB_WAIT_TIMEOUT}s)"
deadline=$(( SECONDS + M2_DB_WAIT_TIMEOUT ))
until (exec 3<>"/dev/tcp/${M2_DB_HOST}/${M2_DB_PORT}") 2>/dev/null; do
  [ "$SECONDS" -lt "$deadline" ] || die "MariaDB at ${M2_DB_HOST}:${M2_DB_PORT} did not become reachable in ${M2_DB_WAIT_TIMEOUT}s"
  sleep 2
done
log "MariaDB is reachable"

# -----------------------------------------------------------------------------
# 2. Materialise the core tree and render the configuration.
# -----------------------------------------------------------------------------
m2-render-config

# -----------------------------------------------------------------------------
# 2b. Server-wide rates: the two things that need root.
#
#     The spool directory shared with the panel container has to be usable by
#     both service accounts, and the handful of data tables the rates rewrite
#     have to be writable by the unprivileged account the supervisor runs as.
#     Everything else about the rates happens later, in m2-supervise.
# -----------------------------------------------------------------------------
command -v m2-rates >/dev/null 2>&1 && { m2-rates prepare || log "could not prepare the rate spool (the rates page will say so)"; }
command -v m2-world-reset >/dev/null 2>&1 && { m2-world-reset prepare || log "could not prepare the world reset's backup folder"; }
# The same, for the language files: this runs as root, and the four files the
# switch replaces belong to the image and have to be writable by the service
# account afterwards. Absent on an image built before this existed.
command -v m2-lang >/dev/null 2>&1 \
  && { m2-lang prepare || log "could not prepare the language files (the language page will say so)"; }
# MT2009_PLUS_CHEST_EDITOR_V1: the chests the Seban panel edits - the image's
# special_item_group.txt kept aside, the live one writable by the service
# account, the spool folder both containers write (m2-chests).
command -v m2-chests >/dev/null 2>&1 \
  && { m2-chests prepare || log "could not prepare the chest files (the panel's Szkatułki page will say so)"; }
# MT2009_PLUS_DROP_EDITOR_V1: the same for the monsters' drops the Seban panel
# edits - the image's mob_drop_item.txt kept aside, the live one writable by
# the service account, the spool folder (m2-drops).
command -v m2-drops >/dev/null 2>&1 \
  && { m2-drops prepare || log "could not prepare the drop files (the panel's Drop z potworów page will say so)"; }
# MT2009_PLUS_DB_EDITOR_V1: the same for the fishing table (quest/libs/fishing/
# fishing_drop.lua) the database editor's "Łowienie ryb" page edits (m2-fishing).
command -v m2-fishing >/dev/null 2>&1 \
  && { m2-fishing prepare || log "could not prepare the fishing table (the panel's Łowienie ryb page will say so)"; }
# MT2009_PLUS_DB_EDITOR_V1: the same for the maps' spawns (regen.txt) the
# Seban panel's database editor edits - the image's files kept as
# regen.image.txt, the live ones writable by the service account (m2-regen).
command -v m2-regen >/dev/null 2>&1 \
  && { m2-regen prepare || log "could not prepare the map spawn files (the panel's Spawny potworów page will say so)"; }
# MT2009_PLUS_DB_EDITOR_V1: the same for the respawn files the Seban panel's
# "Respawn bossów i metinów" edits - special_spawns.txt and every map's
# boss.txt / stone.txt kept aside, the live ones writable (m2-spawns).
command -v m2-spawns >/dev/null 2>&1 \
  && { m2-spawns prepare || log "could not prepare the respawn files (the panel's Respawn page will say so)"; }

# -----------------------------------------------------------------------------
# 3. Resource limits.
#
#    Core dumps are off by default: a game core is 250-320 MB resident, so a
#    crash loop fills a VPS disk in minutes.  Turn them on deliberately when
#    chasing a crash -- cores land in <core>/cores/ on the state volume.
# -----------------------------------------------------------------------------
if [ "$M2_CORE_DUMPS" = "1" ]; then
  ulimit -c unlimited || log "could not raise core dump limit"
  log "core dumps ENABLED (<core>/cores/)"
else
  ulimit -c 0 || true
fi

# -----------------------------------------------------------------------------
# 4. Ownership, then drop privileges.
# -----------------------------------------------------------------------------
if [ "$(id -u)" = "0" ]; then
  chown -R metin2:metin2 "$VAR_DIR" 2>/dev/null || log "could not chown $VAR_DIR (continuing)"

  if [ "$M2_RUN_AS_ROOT" = "1" ]; then
    log "running as root (M2_RUN_AS_ROOT=1)"
    exec "$@"
  fi

  log "dropping privileges to metin2 and starting: $*"
  exec setpriv --reuid=metin2 --regid=metin2 --init-groups --inh-caps=-all -- "$@"
fi

log "starting: $*"
exec "$@"
