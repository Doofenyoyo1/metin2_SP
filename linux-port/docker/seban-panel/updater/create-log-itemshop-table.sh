#!/bin/sh
set -eu
PROJECT=${1:-metin2}
DB_CONTAINER=${SEBAN_DB_CONTAINER:-${PROJECT}-db}
PW=$(docker inspect "$DB_CONTAINER" --format '{{range .Config.Env}}{{println .}}{{end}}' 2>/dev/null | sed -n 's/^MARIADB_ROOT_PASSWORD=//p' | head -n 1)
if [ -z "$PW" ]; then
  echo "Could not read MARIADB_ROOT_PASSWORD from $DB_CONTAINER." >&2
  exit 1
fi
docker exec -e MYSQL_PWD="$PW" "$DB_CONTAINER" mariadb -uroot -e "
CREATE TABLE IF NOT EXISTS log.itemshop (
  pid INT UNSIGNED NOT NULL DEFAULT 0,
  aid INT UNSIGNED NOT NULL DEFAULT 0,
  item_index INT NOT NULL DEFAULT 0,
  vnum INT UNSIGNED NOT NULL DEFAULT 0,
  quantity INT NOT NULL DEFAULT 0,
  price BIGINT NOT NULL DEFAULT 0,
  currency TINYINT NOT NULL DEFAULT 0,
  item_id INT UNSIGNED DEFAULT NULL,
  time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  money_before INT UNSIGNED NOT NULL DEFAULT 0
) ENGINE=InnoDB;"
echo "log.itemshop is ready."
