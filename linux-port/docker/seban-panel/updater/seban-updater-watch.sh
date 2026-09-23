#!/bin/sh
set -eu
SPOOL=${SEBAN_UPDATE_SPOOL:-/var/lib/docker/volumes/metin2_update-spool/_data}
OVR=${SEBAN_OVERRIDE_DIR:-$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)}
ROOT=${SEBAN_M2_ROOT:-/opt/metin2-mt2009/2}
PROJECT=${SEBAN_COMPOSE_PROJECT:-metin2}
ENV_FILE="$ROOT/linux-port/docker/.env"
mkdir -p "$SPOOL"
touch "$SPOOL/watcher"

status() {
  tmp="$SPOOL/update.status.new"
  ver=$(cat "$ROOT/VERSION" 2>/dev/null || echo unknown)
  printf 'state=%s\ntime=%s\nstep=%s\nsteps=%s\nmessage=%s\nversion=%s\n' "$1" "$(date +%s)" "$2" "$3" "$4" "$ver" > "$tmp"
  mv "$tmp" "$SPOOL/update.status"
}
read_env() {
  value=$(sed -n "s/^$1=//p" "$ENV_FILE" 2>/dev/null | tail -n 1 || true)
  printf '%s' "$value"
}
spawn_status() {
  tmp="$SPOOL/spawn-plan.status.new"
  printf 'state=%s\ntime=%s\nwindow=%s\nlate_joiners=%s\nlate_hours=%s\nmessage=%s\n' "$1" "$(date +%s)" "$2" "$3" "$4" "$5" > "$tmp"
  mv "$tmp" "$SPOOL/spawn-plan.status"
}
set_env() {
  key=$1 value=$2
  if grep -q "^$key=" "$ENV_FILE"; then
    sed -i "s|^$key=.*|$key=$value|" "$ENV_FILE"
  else
    printf '\n%s=%s\n' "$key" "$value" >> "$ENV_FILE"
  fi
}
valid_range() {
  value=$1 lower=$2 upper=$3
  case "$value" in ''|*[!0-9]*) return 1;; esac
  [ "$value" -ge "$lower" ] && [ "$value" -le "$upper" ]
}

window=$(read_env PLAYERBOT_SPAWN_WINDOW_MINUTES); [ -n "$window" ] || window=1
late=$(read_env PLAYERBOT_LATE_JOINERS); [ -n "$late" ] || late=0
hours=$(read_env PLAYERBOT_LATE_JOIN_HOURS); [ -n "$hours" ] || hours=24
spawn_status ready "$window" "$late" "$hours" 'Plan wejścia jest gotowy.'
status idle 0 5 'Seban updater is ready.'

while :; do
  touch "$SPOOL/watcher"

  if [ -f "$SPOOL/spawn-plan.request" ]; then
    id=$(sed -n 's/^id=//p' "$SPOOL/spawn-plan.request" | head -n 1 || true)
    last=$(cat "$SPOOL/spawn-plan.last-id" 2>/dev/null || true)
    if [ -n "$id" ] && [ "$id" != "$last" ]; then
      window=$(sed -n 's/^window=//p' "$SPOOL/spawn-plan.request" | head -n 1 || true)
      late=$(sed -n 's/^late_joiners=//p' "$SPOOL/spawn-plan.request" | head -n 1 || true)
      hours=$(sed -n 's/^late_hours=//p' "$SPOOL/spawn-plan.request" | head -n 1 || true)
      printf '%s\n' "$id" > "$SPOOL/spawn-plan.last-id"
      if ! valid_range "$window" 1 180 || ! valid_range "$late" 0 2500 || ! valid_range "$hours" 1 168; then
        spawn_status failed 1 0 24 'Odrzucono nieprawidłowy plan wejścia.'
      elif [ ! -f "$ENV_FILE" ]; then
        spawn_status failed "$window" "$late" "$hours" 'Nie odnaleziono .env aktywnej instalacji.'
      else
        spawn_status running "$window" "$late" "$hours" 'Zapisywanie planu i odtwarzanie kontenera gry.'
        set_env PLAYERBOT_SPAWN_WINDOW_MINUTES "$window"
        set_env PLAYERBOT_LATE_JOINERS "$late"
        set_env PLAYERBOT_LATE_JOIN_HOURS "$hours"
        if (cd "$ROOT/linux-port/docker" && docker compose -p "$PROJECT" up -d --force-recreate --no-deps game); then
          spawn_status ok "$window" "$late" "$hours" 'Plan wejścia aktywny; kontener gry został odtworzony.'
        else
          spawn_status failed "$window" "$late" "$hours" 'Plan zapisany, lecz odtworzenie kontenera gry nie powiodło się.'
        fi
      fi
    fi
  fi

  if [ -f "$SPOOL/request" ]; then
    id=$(sed -n 's/^id=//p' "$SPOOL/request" | head -n 1 || true)
    last=$(cat "$SPOOL/last-id" 2>/dev/null || true)
    if [ -n "$id" ] && [ "$id" != "$last" ]; then
      update_panel=$(sed -n 's/^update_seban_panel=//p' "$SPOOL/request" | head -n 1 || true)
      case "$update_panel" in 1) ;; *) update_panel=0;; esac
      printf '%s\n' "$id" > "$SPOOL/last-id"
      status running 1 5 'Creating a world backup before the update.'
      : > "$SPOOL/update.log"
      printf 'Request: update_seban_panel=%s\n' "$update_panel" >> "$SPOOL/update.log"
      if SEBAN_UPDATE_PANEL="$update_panel" "$OVR/update-with-backup.sh" >> "$SPOOL/update.log" 2>&1; then
        if [ "$update_panel" = 1 ]; then status ok 5 5 'Playerbots updated. Seban Panel option processed; see the log for its version decision.'
        else status ok 5 5 'Playerbots updated. Seban Panel kept unchanged. Backup created first.'; fi
      else
        status failed 5 5 'Update failed; see the updater log. Existing world was not removed.'
      fi
    fi
  fi
  sleep 5
done
