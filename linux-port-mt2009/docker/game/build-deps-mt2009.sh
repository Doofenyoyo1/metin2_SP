#!/usr/bin/env bash
#
# build-deps-mt2009.sh -- the 32-bit Linux static dependency set for the
#                         mt2009 (martysama0134 r41023) server source.
#
# The package ships FreeBSD archives in Extern/lib (libIL, libcryptopp,
# libmysqlclient) which cannot link on Linux, and its Makefiles reach the rest
# through the system: -lz, -lmd, /usr/lib32/libssl.a, /usr/lib32/libcrypto.a.
# This script produces every one of them for i386 under one prefix:
#
#   $PREFIX/lib/libcryptopp.a        <- built from the shipped Extern/cryptopp
#                                       source (CRYPTOPP_VERSION 700, the same
#                                       one Extern/include/cryptopp describes)
#   $PREFIX/lib/libIL.a libILU.a ... <- DevIL 1.8.0 (the r40250 port's patched
#                                       drop; Extern/include/IL ships 1.7.8
#                                       headers, so the built ones are
#                                       overlaid on top -- see the Dockerfile)
#   $PREFIX/lib/libmysqlclient.a     <- MariaDB Connector/C, every auth plugin
#      + include/mysql/                 linked static; the headers replace the
#                                       shipped Extern/include/mysql, which
#                                       belong to a different connector build
#                                       and must not be mixed with this one
#   $PREFIX/lib/libz.a libssl.a libcrypto.a libmd.a
#                                    <- Ubuntu's own i386 -dev packages, copied
#
# Same shape as linux-port/docker/game/src/build-deps-40250.sh, which carries
# the reasoning behind each recipe; this one is only shorter because the mt2009
# tree builds its own Lua 5.0 in-tree and ships boost 1.83 as headers.
#
# Usage:
#   PREFIX=/opt/m2extern BUILD=/build/work TARBALLS=/build/extern-tarballs \
#   CRYPTOPP_SRC=/build/cryptopp ./build-deps-mt2009.sh [component...]
#
# Components: apt zlib openssl libmd cryptopp mysql devil   (default: all)

set -euo pipefail

PREFIX="${PREFIX:-/opt/m2extern}"
BUILD="${BUILD:-/build/work}"
TARBALLS="${TARBALLS:-/build/extern-tarballs}"
CRYPTOPP_SRC="${CRYPTOPP_SRC:-/build/cryptopp}"
SRC="$BUILD/src"
LOG="$BUILD/log"
STAGE="$BUILD/stage"
JOBS="${JOBS:-$(nproc)}"

DEVIL_TARBALL="devil_1_8_0_patched.tar.gz"
MARIADB_VER="3.3.10"
MARIADB_PLUGINDIR="${MARIADB_PLUGINDIR:-/usr/local/lib/mariadb/plugin}"

M32="-m32"
SYSLIB32="/usr/lib/i386-linux-gnu"

say()  { printf '=== %s\n' "$*"; }
info() { printf '    %s\n' "$*"; }
fail() { printf '!!! FAIL %s\n' "$*" >&2; }

fetch() { # url filename
  local url="$1" out="$SRC/$2"
  [ -s "$out" ] && return 0
  mkdir -p "$SRC"
  wget -q -O "$out.part" "$url" && mv "$out.part" "$out"
}

copy_syslib() { # libname.a
  [ -f "$SYSLIB32/$1" ] || { fail "$SYSLIB32/$1 missing -- run the 'apt' component first"; return 1; }
  install -d "$PREFIX/lib"
  install -m644 "$SYSLIB32/$1" "$PREFIX/lib/$1"
  info "$1  <- $SYSLIB32"
}

# ---------------------------------------------------------------------------
APT_PKGS="build-essential gcc-multilib g++-multilib libc6-dev-i386
linux-libc-dev:i386 cmake make git wget ca-certificates pkg-config file
binutils zlib1g-dev:i386 libssl-dev:i386 libmd-dev:i386"

c_apt() {
  say "apt prerequisites"
  dpkg --add-architecture i386
  apt-get update -qq
  # shellcheck disable=SC2086
  apt-get install -y --no-install-recommends $APT_PKGS
}

c_zlib()    { say "zlib (32-bit)";    copy_syslib libz.a; }
c_libmd()   { say "libmd (32-bit)";   copy_syslib libmd.a; }
c_openssl() { say "openssl (32-bit)"; copy_syslib libssl.a; copy_syslib libcrypto.a; }

# ---------------------------------------------------------------------------
# cryptopp 7.0.0 from the shipped source directory.  IS_X86/IS_X64 must be
# forced: the GNUmakefile probes `g++ -dumpmachine`, which says x86_64 even
# under -m32, and would enable 64-bit-only SIMD translation units.
# ---------------------------------------------------------------------------
c_cryptopp() {
  say "cryptopp 7.0.0 (32-bit, from $CRYPTOPP_SRC)"
  [ -f "$CRYPTOPP_SRC/cryptlib.cpp" ] || { fail "no cryptlib.cpp in $CRYPTOPP_SRC"; return 1; }
  grep -q 'define CRYPTOPP_VERSION 700' "$CRYPTOPP_SRC/config.h" \
    || { fail "unexpected cryptopp version in $CRYPTOPP_SRC/config.h"; return 1; }
  mkdir -p "$LOG"
  # The package's GNUmakefile ends its libcryptopp.a rule with `mv ../lib/`:
  # it expects to sit at Extern/cryptopp beside Extern/lib.  Give it the
  # directory it wants and collect the archive from there.
  mkdir -p "$CRYPTOPP_SRC/../lib"
  ( cd "$CRYPTOPP_SRC" \
    && find . -name '*.o' -delete \
    && rm -f libcryptopp.a ../lib/libcryptopp.a \
    && make -j"$JOBS" static \
         CXX="g++" \
         CXXFLAGS="-DNDEBUG -O2 -fPIC $M32 -std=c++17 -w -march=i686" \
         IS_X86=1 IS_X64=0 ) > "$LOG/cryptopp.log" 2>&1 \
    || { fail "cryptopp build (see $LOG/cryptopp.log)"; tail -40 "$LOG/cryptopp.log"; return 1; }
  local a="$CRYPTOPP_SRC/../lib/libcryptopp.a"
  [ -f "$a" ] || a="$CRYPTOPP_SRC/libcryptopp.a"
  [ -f "$a" ] || { fail "libcryptopp.a was not produced"; return 1; }
  install -d "$PREFIX/lib"
  install -m644 "$a" "$PREFIX/lib/libcryptopp.a"
}

# ---------------------------------------------------------------------------
# MariaDB Connector/C -> lib/libmysqlclient.a (+ libmariadb.a) and
# include/mysql/.  Auth plugins static: a deployed server has no plugin dir.
# ---------------------------------------------------------------------------
c_mysql() {
  say "mariadb-connector-c $MARIADB_VER (32-bit)"
  fetch "https://codeload.github.com/mariadb-corporation/mariadb-connector-c/tar.gz/refs/tags/v$MARIADB_VER" \
        "mariadb-connector-c-$MARIADB_VER.tar.gz" || { fail "mariadb download"; return 1; }
  rm -rf "$SRC/mariadb-connector-c-$MARIADB_VER"
  tar xf "$SRC/mariadb-connector-c-$MARIADB_VER.tar.gz" -C "$SRC"
  mkdir -p "$LOG"
  ( cd "$SRC/mariadb-connector-c-$MARIADB_VER" \
    && rm -rf build && mkdir build && cd build \
    && cmake .. \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_FLAGS="$M32 -O2 -fPIC" \
      -DCMAKE_CXX_FLAGS="$M32 -O2 -fPIC" \
      -DCMAKE_EXE_LINKER_FLAGS="$M32" \
      -DCMAKE_SYSTEM_PROCESSOR=i686 \
      -DCMAKE_INSTALL_PREFIX="$STAGE/mariadb" \
      -DWITH_UNIT_TESTS=OFF -DWITH_SSL=OPENSSL -DWITH_EXTERNAL_ZLIB=ON \
      -DWITH_CURL=OFF \
      -DPLUGINDIR="$MARIADB_PLUGINDIR" \
      -DCLIENT_PLUGIN_DIALOG=STATIC \
      -DCLIENT_PLUGIN_MYSQL_CLEAR_PASSWORD=STATIC \
      -DCLIENT_PLUGIN_CACHING_SHA2_PASSWORD=STATIC \
      -DCLIENT_PLUGIN_SHA256_PASSWORD=STATIC \
      -DCLIENT_PLUGIN_AUTH_GSSAPI_CLIENT=OFF \
      -DCLIENT_PLUGIN_REMOTE_IO=OFF \
    && sed -i "s|^#define MARIADB_PLUGINDIR .*|#define MARIADB_PLUGINDIR \"$MARIADB_PLUGINDIR\"|" include/mariadb_version.h \
    && make -j"$JOBS" mariadbclient && make install ) > "$LOG/mariadb.log" 2>&1 \
    || { fail "mariadb build (see $LOG/mariadb.log)"; tail -40 "$LOG/mariadb.log"; return 1; }
  install -d "$PREFIX/lib" "$PREFIX/include/mysql"
  local a
  a=$(find "$STAGE/mariadb" -name 'libmariadbclient.a' | head -1)
  [ -n "$a" ] || { fail "libmariadbclient.a not produced"; return 1; }
  install -m644 "$a" "$PREFIX/lib/libmariadb.a"
  cp "$PREFIX/lib/libmariadb.a" "$PREFIX/lib/libmysqlclient.a"
  cp -a "$STAGE/mariadb/include/mariadb/." "$PREFIX/include/mysql/"
}

# ---------------------------------------------------------------------------
# DevIL 1.8.0 (the r40250 port's patched drop) -> libIL.a libILU.a libILUT.a
# and include/IL/.  Every optional codec off: the game reads guild marks
# (TGA) and nothing else.
# ---------------------------------------------------------------------------
c_devil() {
  say "DevIL 1.8.0 patched (32-bit, from $TARBALLS/$DEVIL_TARBALL)"
  [ -f "$TARBALLS/$DEVIL_TARBALL" ] || { fail "missing $TARBALLS/$DEVIL_TARBALL"; return 1; }
  rm -rf "$SRC/devil"; mkdir -p "$SRC/devil"
  tar xf "$TARBALLS/$DEVIL_TARBALL" -C "$SRC/devil"
  local d
  d=$(find "$SRC/devil" -maxdepth 4 -type d -name DevIL -exec test -f '{}/CMakeLists.txt' \; -print | head -1)
  [ -n "$d" ] || { fail "DevIL cmake root not found"; return 1; }
  mkdir -p "$LOG"
  ( cd "$d" && rm -rf build && mkdir build && cd build \
    && cmake .. \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_FLAGS="$M32 -O2 -fPIC -w" \
      -DCMAKE_CXX_FLAGS="$M32 -O2 -fPIC -w" \
      -DCMAKE_EXE_LINKER_FLAGS="$M32" \
      -DCMAKE_INSTALL_PREFIX="$STAGE/devil" \
      -DBUILD_SHARED_LIBS=OFF \
      -DIL_NO_PNG=ON -DIL_NO_JPG=ON -DIL_NO_TIF=ON -DIL_NO_LCMS=ON \
      -DIL_NO_MNG=ON -DIL_NO_JP2=ON -DIL_NO_EXR=ON -DIL_NO_WDP=ON \
      -DIL_USE_DXTC_NVIDIA=OFF -DIL_USE_DXTC_SQUISH=OFF \
    && make -j"$JOBS" && make install ) > "$LOG/devil.log" 2>&1 \
    || { fail "DevIL build (see $LOG/devil.log)"; tail -40 "$LOG/devil.log"; return 1; }
  install -d "$PREFIX/lib" "$PREFIX/include/IL"
  find "$STAGE/devil" -name 'libIL*.a' -exec install -m644 '{}' "$PREFIX/lib/" \;
  cp -a "$STAGE/devil/include/IL/." "$PREFIX/include/IL/"
}

c_verify() {
  say "verify"
  local f
  for f in libz.a libmd.a libssl.a libcrypto.a libcryptopp.a libmysqlclient.a libIL.a; do
    [ -f "$PREFIX/lib/$f" ] || { fail "$PREFIX/lib/$f missing"; return 1; }
    file -b "$PREFIX/lib/$f" | grep -q 'ar archive' || { fail "$f is not an archive"; return 1; }
    # every member must be i386
    if ar p "$PREFIX/lib/$f" 2>/dev/null | file -b - | grep -q 'x86-64'; then
      fail "$f carries x86-64 objects"; return 1
    fi
    info "OK $f"
  done
  [ -f "$PREFIX/include/mysql/mysql.h" ] || { fail "mysql headers missing"; return 1; }
  [ -f "$PREFIX/include/IL/il.h" ] || { fail "IL headers missing"; return 1; }
}

main() {
  local comps=("$@")
  [ ${#comps[@]} -eq 0 ] && comps=(apt zlib openssl libmd cryptopp mysql devil verify)
  mkdir -p "$BUILD" "$PREFIX"
  local c
  for c in "${comps[@]}"; do
    "c_$c" || { fail "component $c failed"; exit 1; }
  done
  say "done: $PREFIX"
  ls -la "$PREFIX/lib"
}

main "$@"
