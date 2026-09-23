#!/usr/bin/env bash
# =============================================================================
#  game-compile.sh -- compile the mt2009 game core against the staged tree
#  without rebuilding the image: the `libs' stage (dependencies, support
#  archives, db core) is the container, the game sources are a bind mount.
#
#    linux-port-mt2009/tools/game-compile.sh              # full make
#    linux-port-mt2009/tools/game-compile.sh playerbot_manager.cpp
#                                                         # one file, -fsyntax-only
#
#  Needs `docker build --target libs -t m2mt2009-libs:dev linux-port-mt2009/docker/game'
#  once (a cached no-op after a full build). Objects land in game/src/.obj on
#  the host, so a second run only recompiles what changed.
# =============================================================================
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
GAME="$HERE/../docker/game/src/server/game"
IMAGE="${M2_LIBS_IMAGE:-m2mt2009-libs:dev}"
JOBS="${JOBS:-$(nproc 2>/dev/null || echo 4)}"

# Git Bash hands docker a Windows path; docker wants C:/... with forward
# slashes and no MSYS rewriting of the container side.
hostpath() { ( cd "$1" && pwd -W 2>/dev/null || pwd ); }
GAME_HOST="$(hostpath "$GAME")"

if [ $# -eq 0 ]; then
  MSYS_NO_PATHCONV=1 docker run --rm -v "$GAME_HOST:/src/Server/game" -w /src/Server \
    "$IMAGE" bash -c "sed -i 's/\r\$//' game/src/Makefile && rm -f game/src/Depend && make -C game/src -j$JOBS CC=gcc CXX=g++ ENABLE_GCC_AUTODEPEND=0 2>&1 | grep -vE '^compiling ' | head -200"
else
  # `make -n' prints the exact compile line the Makefile would use; the file
  # is swapped in and -fsyntax-only added, so the check is the build's own
  # flags and nothing invented.
  for f in "$@"; do
    MSYS_NO_PATHCONV=1 docker run --rm -v "$GAME_HOST:/src/Server/game" -w /src/Server/game/src \
      "$IMAGE" bash -c "sed -i 's/\r\$//' Makefile; rm -f Depend; cmd=\$(make -n CC=gcc CXX=g++ ENABLE_GCC_AUTODEPEND=0 .obj/${f%.cpp}.o | grep -E '^g\+\+|^gcc' | head -1); [ -n \"\$cmd\" ] || { echo 'no compile line for $f'; exit 1; }; cmd=\${cmd/ -c / -fsyntax-only -c }; echo \"\$cmd\" | cut -c1-160; eval \"\$cmd\" 2>&1 | head -${M2_ERR_LINES:-120}"
  done
fi
