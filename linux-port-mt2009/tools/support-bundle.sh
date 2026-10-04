#!/bin/sh
# =============================================================================
#  support-bundle.sh -- the launcher's "pakiet diagnostyczny" for a Linux/VPS
#  server (MT2009_PLUS_SUPPORT_BUNDLE_V1).
#
#  Run from the server folder (the one with VERSION in it):
#      sh linux-port/tools/support-bundle.sh
#  It writes /tmp/metin2-support-<date>.tar.gz: the version, the containers'
#  state and logs, the last lines of every game core's syserr/syslog, .env
#  WITHOUT passwords, memory and disk. No database, no accounts, no passwords.
#  Send that file to whoever helps you (WinSCP / FileZilla over SFTP).
# =============================================================================
set -u

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
DOCKER_DIR="$ROOT/linux-port/docker"
STAMP=$(date +%Y%m%d-%H%M%S)
NAME="metin2-support-$STAMP"
OUT="${TMPDIR:-/tmp}/$NAME"
mkdir -p "$OUT" || { echo "BLAD: nie moge utworzyc $OUT"; exit 1; }

say() { printf '%s\n' "$*"; }
say "Zbieram pakiet diagnostyczny do $OUT ..."

{
	say "Utworzono: $(date '+%Y-%m-%d %H:%M:%S %Z')"
	say "Folder serwera: $ROOT"
	say "Wersja: $(cat "$ROOT/VERSION" 2>/dev/null || echo '?')"
	say "Wersja moda: $(cat "$ROOT/MOD_VERSION" 2>/dev/null || echo '?')"
	say "System: $(. /etc/os-release 2>/dev/null; echo "${PRETTY_NAME:-?}")"
	say "Jadro: $(uname -r)"
	say "Docker: $(docker --version 2>/dev/null || echo 'brak')"
} > "$OUT/summary.txt"

free -m > "$OUT/memory.txt" 2>&1
uptime >> "$OUT/memory.txt" 2>&1
df -h > "$OUT/disk-space.txt" 2>&1
docker info > "$OUT/docker-info.txt" 2>&1

if [ -d "$DOCKER_DIR" ]; then
	cd "$DOCKER_DIR" || exit 1
	docker compose ps -a > "$OUT/compose-ps.txt" 2>&1
	docker compose logs --no-color --tail 3000 > "$OUT/compose-logs.txt" 2>&1
	# .env without anything that looks like a secret.
	if [ -f .env ]; then
		grep -v -i -E 'pass|secret|token|key|salt' .env > "$OUT/environment-redacted.txt" 2>/dev/null
	fi
	game=$(docker compose ps -q game 2>/dev/null | head -n 1)
	if [ -n "$game" ]; then
		docker logs --tail 3000 "$game" > "$OUT/game-container-logs.txt" 2>&1
	fi
fi

# Every core's syserr and syslog, from the game-var volume.
VOL=$(docker volume ls -q 2>/dev/null | grep 'game-var$' | head -n 1)
if [ -n "$VOL" ]; then
	DATA=$(docker volume inspect -f '{{.Mountpoint}}' "$VOL" 2>/dev/null)
	if [ -n "$DATA" ] && [ -d "$DATA" ]; then
		for core in "$DATA"/auth "$DATA"/db "$DATA"/channel*/*; do
			[ -d "$core" ] || continue
			tag=$(printf '%s' "${core#$DATA/}" | tr '/' '-')
			[ -f "$core/syserr" ] && tail -n 3000 "$core/syserr" > "$OUT/syserr-$tag.txt" 2>/dev/null
			[ -f "$core/syslog" ] && tail -n 5000 "$core/syslog" > "$OUT/syslog-$tag.txt" 2>/dev/null
			dumps=$(find "$core" -maxdepth 2 -type f -name 'core*' 2>/dev/null)
			[ -n "$dumps" ] && ls -la $dumps > "$OUT/crash-$tag.txt" 2>&1
		done
	fi
fi

cd "$(dirname "$OUT")" || exit 1
if tar czf "$OUT.tar.gz" "$NAME" 2>/dev/null; then
	rm -rf "$OUT"
	say ""
	say "GOTOWE: $OUT.tar.gz"
	say "Pobierz ten plik (WinSCP / FileZilla, SFTP) i przeslij go osobie, ktora pomaga."
	say "W srodku nie ma bazy danych ani hasel."
else
	say "BLAD: nie udalo sie spakowac $OUT"
	exit 1
fi
