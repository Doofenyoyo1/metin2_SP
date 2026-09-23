#!/bin/sh
set -eu
ROOT=${1:?usage: apply-seban-overrides.sh /path/to/server}
SELF=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ENV="$ROOT/linux-port/docker/.env"
QUEST="$ROOT/linux-port/docker/game/quest/starter_chest.quest"
ORIGINAL_QUEST="$SELF/starter_chest.original.quest"
DB_CONTAINER=${SEBAN_DB_CONTAINER:-metin2-db}
set_kv() {
  key=$1 value=$2 file=$3
  if grep -q "^$key=" "$file"; then sed -i "s|^$key=.*|$key=$value|" "$file"; else printf '\n%s=%s\n' "$key" "$value" >> "$file"; fi
}
PW=$(docker inspect "$DB_CONTAINER" --format '{{range .Config.Env}}{{println .}}{{end}}' 2>/dev/null | sed -n 's/^MARIADB_ROOT_PASSWORD=//p' | head -1)
setting() {
  value=""
  if [ -n "$PW" ]; then value=$(docker exec -e MYSQL_PWD="$PW" "$DB_CONTAINER" mariadb -N -uroot player -e "SELECT value FROM web_seban_settings WHERE name='$1' LIMIT 1" 2>/dev/null | tail -1 || true); fi
  case "$value" in 0|1) printf '%s' "$value";; *) printf '%s' "$2";; esac
}
ALLOW_STUDENT_CHEST=$(setting allow_student_chest 0)
ALLOW_MOONLIGHT_CHEST=$(setting allow_moonlight_chest 0)
KEEP_DEMO_CHARACTERS=$(setting keep_demo_characters 0)
# The freshly unpacked Tieru package is the canonical enabled quest. Keep its
# newest copy before replacing it with the disabled Seban variant.
if [ -s "$QUEST" ] && ! grep -q 'Seban local override' "$QUEST"; then cp "$QUEST" "$ORIGINAL_QUEST"; fi
if [ "$ALLOW_MOONLIGHT_CHEST" = 1 ]; then
  set_kv M2_MOONLIGHT_CHEST_PERMILLE 10 "$ENV"; set_kv M2_MOONLIGHT_CHEST_STONE_PERMILLE 300 "$ENV"
else
  set_kv M2_MOONLIGHT_CHEST_PERMILLE 0 "$ENV"; set_kv M2_MOONLIGHT_CHEST_STONE_PERMILLE 0 "$ENV"
fi
if [ "$ALLOW_STUDENT_CHEST" = 1 ]; then
  set_kv M2_PLAYERBOT_DISABLE_STUDENT_CHEST 0 "$ENV"; test -s "$ORIGINAL_QUEST"; cp "$ORIGINAL_QUEST" "$QUEST"
else
  set_kv M2_PLAYERBOT_DISABLE_STUDENT_CHEST 1 "$ENV"
  cat > "$QUEST" <<'QUEST'
-- Seban local override: no Apprentice Chest for new characters.
quest starter_chest begin
  state start begin
    when login begin
      return
    end
  end
end
QUEST
fi
if [ "$KEEP_DEMO_CHARACTERS" != 1 ] && [ -n "$PW" ]; then
  docker exec -e MYSQL_PWD="$PW" "$DB_CONTAINER" mariadb -uroot player -e "DELETE i FROM item i JOIN player p ON p.id=i.owner_id WHERE p.name IN ('Admin','AdminNinja','AdminSura','AdminSzaman'); DELETE FROM player WHERE name IN ('Admin','AdminNinja','AdminSura','AdminSzaman');"
fi
printf '%s seban-overrides: student=%s moonlight=%s keep-demo=%s\n' "$(date '+%F %T')" "$ALLOW_STUDENT_CHEST" "$ALLOW_MOONLIGHT_CHEST" "$KEEP_DEMO_CHARACTERS" >> "$ROOT/seban-overrides.log"
