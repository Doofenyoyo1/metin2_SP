#!/bin/sh
set -eu
OVR=${SEBAN_OVERRIDE_DIR:-$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)}
SERVER_ROOT=${SEBAN_M2_ROOT:-/opt/metin2-mt2009/mt2009-r41023-base}
BACKUP_ROOT=${SEBAN_BACKUP_ROOT:-$(dirname "$SERVER_ROOT")/backups}
if [ "$#" -gt 0 ] && [ "$1" = "--check" ]; then
  exec "$OVR/update-mt2009.py" --check
fi
stamp=$(date -u +%Y%m%dT%H%M%SZ)
dir="$BACKUP_ROOT/gui-update-$stamp"
mkdir -p "$dir"
db_container=${SEBAN_DB_CONTAINER:-metin2-db}
pw=$(docker inspect "$db_container" --format '{{range .Config.Env}}{{println .}}{{end}}' | sed -n 's/^MARIADB_ROOT_PASSWORD=//p')
test -n "$pw"
docker exec -e MYSQL_PWD="$pw" "$db_container" mariadb-dump -uroot --databases account common player log > "$dir/world.sql"
test $(stat -c%s "$dir/world.sql") -gt 10000000
gzip -9 "$dir/world.sql"
gzip -t "$dir/world.sql.gz"
printf '%s\n' "$dir/world.sql.gz" > "$OVR/latest-backup-path"
"$OVR/update-mt2009.py"
