#!/bin/sh
set -eu

if [ "$(id -u)" -ne 0 ]; then
  echo "Uruchom instalator przez sudo: sudo $0 /sciezka/do/serwera [projekt-compose]" >&2
  exit 1
fi
SERVER_ROOT=${1:-}
PROJECT=${2:-metin2}
test -n "$SERVER_ROOT" && test -f "$SERVER_ROOT/VERSION" && test -d "$SERVER_ROOT/linux-port/docker" || {
  echo "Podaj katalog MT2009 zawierajacy VERSION i linux-port/docker." >&2; exit 1;
}
SOURCE_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
TARGET=/opt/seban-updater
DB_CONTAINER=${SEBAN_DB_CONTAINER:-${PROJECT}-db}
VOLUME=${SEBAN_UPDATE_VOLUME:-${PROJECT}_update-spool}
SPOOL=$(docker volume inspect "$VOLUME" --format '{{.Mountpoint}}')
test -d "$SPOOL"
install -d -m 0755 "$TARGET"
for file in apply-seban-overrides.sh update-mt2009.py update-with-backup.sh seban-updater-watch.sh create-log-itemshop-table.sh starter_chest.original.quest; do
  install -m 0755 "$SOURCE_DIR/$file" "$TARGET/$file"
done
"$TARGET/create-log-itemshop-table.sh" "$PROJECT"
cat > /etc/seban-updater.env <<EOF
SEBAN_M2_ROOT=$SERVER_ROOT
SEBAN_OVERRIDE_DIR=$TARGET
SEBAN_UPDATE_SPOOL=$SPOOL
SEBAN_BACKUP_ROOT=$(dirname "$SERVER_ROOT")/backups
SEBAN_DB_CONTAINER=$DB_CONTAINER
SEBAN_COMPOSE_PROJECT=$PROJECT
EOF
cat > /etc/systemd/system/seban-updater.service <<'EOF'
[Unit]
Description=Seban guarded MT2009 updater
After=docker.service
Requires=docker.service
[Service]
Type=simple
EnvironmentFile=/etc/seban-updater.env
ExecStart=/opt/seban-updater/seban-updater-watch.sh
Restart=always
RestartSec=3
[Install]
WantedBy=multi-user.target
EOF
systemctl daemon-reload
systemctl enable --now seban-updater
sleep 2
systemctl is-active --quiet seban-updater
echo "Aktualizator Seban jest aktywny. Odswiez /manage."
