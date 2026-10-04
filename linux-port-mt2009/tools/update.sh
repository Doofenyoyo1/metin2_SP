#!/bin/sh
# =============================================================================
#  update.sh -- the 2.x (mt2009) line's update on Linux, without the launcher.
#
#  The 1.x line updates a Linux server by refreshing a git checkout of the
#  repository and re-running its installer (installer/install.sh, m2-updater).
#  That path knows nothing about this line: the repository's root VERSION is
#  the 1.x line's, install.sh stages the 1.x tree over whatever it finds, and
#  a 2.x server that ran it ended up with VERSION 1.33.3, the 1.x rates
#  script and a panel that could not talk to its game (l0st3k, 12 September:
#  "checkout z main melduje 1.33.3", rates stuck in state=running for 12 h,
#  no stalls, the panel's update stopping at 40%).
#
#  This line is delivered the way the Windows launcher delivers it: a package
#  zip published on GitHub and named by update-manifest-mt2009.json. This
#  script does exactly that on Linux --
#
#      sh linux-port/tools/update.sh            # from the server folder
#
#  1. reads the manifest (GitHub contents API, then raw as a fallback, then
#     the fallback server M2_UPDATE_MIRROR when GitHub does not answer),
#  2. downloads the server zip it names (or the same file name from the
#     fallback server), checks its SHA-256,
#  3. unpacks it over this folder (files the zip carries are replaced; .env,
#     docker-compose.override.yml and everything else stay as they are),
#  4. runs `docker compose up -d --build' in linux-port/docker.
#
#  Nothing here removes a volume: the database, characters, items and guilds
#  are in volumes, and `down' does not appear in this file in any form.
#
#  `sh update.sh watch' is the updater container's mode: it polls the panel's
#  spool for a request (the same request/update.status/update.log files the
#  1.x m2-updater uses, see linux-port/docker/updater/bin/m2-updater) and runs
#  the sequence above on each one. `sh update.sh check' only prints what is
#  installed and what is published.
#
#  A tool that has read the manifest and fetched and checked the package
#  itself (Tyrion's m2-vps-update) hands both over instead of letting this
#  script fetch them a second time, when the manifest may have moved on:
#
#      M2_UPDATE_MANIFEST_FILE=/path/manifest.json \
#      M2_UPDATE_ZIP=/path/metin2-server-update-X.Y.Z.zip \
#          sh linux-port/tools/update.sh run
#
#  The update then runs on exactly the bytes that tool checked. The SHA-256 is
#  still checked against the manifest, and the zip's own VERSION has to be the
#  manifest's. `run' only: `watch' always reads what is published.
#
#  Needs: docker with compose, and either python3 or curl + unzip + sha256sum.
# =============================================================================
set -u

# The server folder is two levels up from this file (Serwer/linux-port/tools).
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=${M2_UPDATE_STACK_DIR:-$(cd "$HERE/../.." && pwd)}

# An update unpacks a new copy of this very file over it, and sh reads a script
# as it runs it, so the old run went on reading the new file at the old offset
# and ended in "Syntax error: Unterminated quoted string" after a successful
# update (23 September). Run from a copy; the copy unlinks itself, which a
# shell that already has it open does not notice.
if [ -z "${M2_UPDATE_FROM_COPY:-}" ]; then
    case "${1:-run}" in
        run|watch)
            _self=${TMPDIR:-/tmp}/m2-update-self.$$
            if cp "$0" "$_self" 2>/dev/null; then
                M2_UPDATE_FROM_COPY=1 M2_UPDATE_STACK_DIR="$ROOT" exec sh "$_self" "$@"
            fi
            ;;
    esac
else
    rm -f "$0" 2>/dev/null
fi
COMPOSE_DIR="$ROOT/linux-port/docker"
# Written after an unpack, removed after a build that finished (build_and_start).
BUILD_PENDING="$ROOT/.update-build-pending"
REPO=${M2_UPDATE_REPO:-Doofenyoyo1/metin2_SP}
case "$REPO" in
    *TieruYT/metin2-playerbots*|*zaxerrrr-dot/mt2009-sp-plus*)
        echo "M2_UPDATE_REPO wskazuje repozytorium projektu zrodlowego; ta paczka aktualizuje sie tylko z repozytorium Metin2 SinglePlayer (Doofenyoyo1/metin2_SP)."
        exit 1 ;;
esac
BRANCH=${M2_UPDATE_BRANCH:-main}
MANIFEST_NAME=update-manifest-mt2009.json
SPOOL=${M2_UPDATE_SPOOL:-/opt/m2update}
POLL=${M2_UPDATE_POLL:-5}
# MT2009_PLUS_UPDATE_MIRROR_V1: the fallback update source. When GitHub does
# not answer (network error, non-200, a body that is not JSON) the manifest
# and the zip are read from here under the same file names; the zip passes
# the same SHA-256 check. Space-separated bases; empty turns it off. The
# manifest's optional "mirrors": [...] are tried before these. No default:
# MT2009 PLUS's mirror carries its packages, not this project's.
MIRROR_BASES=${M2_UPDATE_MIRROR-}
GITHUB_TIMEOUT=${M2_UPDATE_GITHUB_TIMEOUT:-15}
MIRROR_NOTICE='GitHub niedostępny - pobieram z serwera zapasowego'
MANIFEST_MIRRORS=''
FETCHED_FROM_MIRROR=''
WORK=${TMPDIR:-/tmp}/m2-update.$$

now() { date '+%Y-%m-%d %H:%M:%S'; }
say() { printf '%s [update] %s\n' "$(now)" "$*"; }
die() { say "ERROR: $*"; exit 1; }

have() { command -v "$1" >/dev/null 2>&1; }

# ---- the pieces that need a tool ---------------------------------------------
# GitHub's raw CDN can lag a release by minutes; the contents API does not.
# fetch_text URL [SECONDS]
fetch_text() {
    _url=$1; _secs=${2:-30}
    if have python3; then
        python3 - "$_url" "$_secs" <<'EOF'
import sys, urllib.request
req = urllib.request.Request(sys.argv[1], headers={
    'User-Agent': 'metin2-playerbots-update/2 (+https://github.com/Doofenyoyo1/metin2_SP)',
    'Accept': 'application/vnd.github.raw+json'})
sys.stdout.write(urllib.request.urlopen(req, timeout=int(sys.argv[2])).read().decode('utf-8', 'replace'))
EOF
    elif have curl; then
        # Bounded, like python's timeout above: a connection that stalls
        # without failing held "[1/4] reading what is published" for good,
        # and the fallbacks below only run once this one gives up.
        curl -fsSL --connect-timeout "$_secs" --max-time $((_secs * 2)) -A 'metin2-playerbots-update/2' -H 'Accept: application/vnd.github.raw+json' "$_url"
    else
        die "neither python3 nor curl is installed"
    fi
}

# valid_manifest FILE: a JSON object (python3), or at least text that starts
# with '{' (without it).
valid_manifest() {
    [ -s "$1" ] || return 1
    if have python3; then
        python3 -c 'import json, sys; sys.exit(0 if isinstance(json.load(open(sys.argv[1], encoding="utf-8-sig")), dict) else 1)' "$1" 2>/dev/null
    else
        [ "$(tr -d ' \t\r\n' < "$1" | sed 's/^\xef\xbb\xbf//' | cut -c1)" = '{' ]
    fi
}

# The mirror bases, the manifest's "mirrors" first, each ending in '/'.
mirror_bases() {
    for _b in $MANIFEST_MIRRORS $MIRROR_BASES; do
        case "$_b" in
            http://*/|https://*/) printf '%s\n' "$_b" ;;
            http://*|https://*) printf '%s/\n' "$_b" ;;
        esac
    done | awk '!seen[$0]++'
}

# The manifest on stdout: GitHub (contents API, then raw) with a short
# timeout, then the mirrors. FETCHED_FROM_MIRROR names the mirror it came
# from (a function called with a redirection runs in this shell, so the
# variable survives `fetch_manifest > file').
fetch_manifest() {
    _fm=${TMPDIR:-/tmp}/m2-update-manifest.$$
    FETCHED_FROM_MIRROR=''
    if { fetch_text "https://api.github.com/repos/$REPO/contents/$MANIFEST_NAME?ref=$BRANCH" "$GITHUB_TIMEOUT" > "$_fm" 2>/dev/null && valid_manifest "$_fm"; } \
        || { fetch_text "https://raw.githubusercontent.com/$REPO/$BRANCH/$MANIFEST_NAME" "$GITHUB_TIMEOUT" > "$_fm" 2> "$_fm.err" && valid_manifest "$_fm"; }; then
        cat "$_fm"; rm -f "$_fm" "$_fm.err"; return 0
    fi
    for _base in $(mirror_bases); do
        if fetch_text "$_base$MANIFEST_NAME" 30 > "$_fm" 2>/dev/null && valid_manifest "$_fm"; then
            FETCHED_FROM_MIRROR=$_base
            printf '%s (%s)\n' "$MIRROR_NOTICE" "$_base" >&2
            cat "$_fm"; rm -f "$_fm" "$_fm.err"; return 0
        fi
    done
    # Nothing answered: GitHub's own error is the one worth showing.
    tail -n 3 "$_fm.err" >&2 2>/dev/null
    rm -f "$_fm" "$_fm.err"
    return 1
}

# manifest_mirrors FILE -> the manifest's "mirrors", space-separated (python3
# only; without it the built-in bases are used).
manifest_mirrors() {
    have python3 || return 0
    python3 - "$1" 2>/dev/null <<'EOF'
import json, sys
m = json.load(open(sys.argv[1], encoding='utf-8-sig'))
v = m.get('mirrors') if isinstance(m, dict) else None
if isinstance(v, list):
    print(' '.join(str(x) for x in v if isinstance(x, str) and x and ' ' not in x))
EOF
}

# manifest_field FILE KEY -> the server block's field (version, url, sha256).
manifest_field() {
    if have python3; then
        python3 - "$1" "$2" <<'EOF'
import json, sys
m = json.load(open(sys.argv[1], encoding='utf-8-sig'))
print(m.get('server', {}).get(sys.argv[2], ''))
EOF
    else
        # No JSON parser without python: take the server block by hand. The
        # manifest is written by us, two spaces of indentation, one key a line.
        sed -n '/"server"/,/}/p' "$1" | sed -n "s/.*\"$2\": *\"\([^\"]*\)\".*/\1/p" | head -n 1
    fi
}

download() {
    _url=$1; _out=$2
    if have curl; then
        # --speed-*: a transfer stalled below 1 kB/s for a minute is given up,
        # so the fallback server gets its turn instead of a hang.
        curl -fL --retry 3 --connect-timeout 20 --speed-time 60 --speed-limit 1024 -A 'metin2-playerbots-update/2' -o "$_out" "$_url"
    elif have python3; then
        python3 - "$_url" "$_out" <<'EOF'
import sys, urllib.request, shutil
req = urllib.request.Request(sys.argv[1], headers={'User-Agent': 'metin2-playerbots-update/2'})
with urllib.request.urlopen(req, timeout=120) as r, open(sys.argv[2], 'wb') as f:
    shutil.copyfileobj(r, f)
EOF
    else
        die "neither curl nor python3 is installed"
    fi
}

# download_checked URL OUT SHA256: the manifest's URL, then each mirror with
# the URL's file name, until one gives a file with this SHA-256.
download_checked() {
    _dl_url=$1; _dl_out=$2; _dl_want=$(printf '%s' "$3" | tr 'A-F' 'a-f')
    _dl_name=${_dl_url%%\?*}; _dl_name=${_dl_name##*/}
    _dl_sources=$_dl_url
    case "$_dl_name" in
        ''|*[!A-Za-z0-9._-]*) ;;
        *) for _b in $(mirror_bases); do _dl_sources="$_dl_sources $_b$_dl_name"; done ;;
    esac
    _dl_first=1; _dl_first_mirror=0
    for _src in $_dl_sources; do
        if [ "$_dl_first" = 0 ]; then
            [ "$_dl_first_mirror" = 1 ] || note "   $MIRROR_NOTICE"
            _dl_first_mirror=1
            note "   downloading $_src"
        fi
        _dl_first=0
        rm -f "$_dl_out"
        if download "$_src" "$_dl_out"; then
            [ "$(sha256_of "$_dl_out" | tr 'A-F' 'a-f')" = "$_dl_want" ] && return 0
            note "   $_src: the SHA-256 is not the manifest's"
        else
            note "   $_src: the download failed"
        fi
    done
    rm -f "$_dl_out"
    return 1
}

sha256_of() {
    if have sha256sum; then sha256sum "$1" | cut -d' ' -f1
    elif have shasum; then shasum -a 256 "$1" | cut -d' ' -f1
    elif have python3; then python3 -c 'import hashlib,sys; print(hashlib.sha256(open(sys.argv[1],"rb").read()).hexdigest())' "$1"
    else die "no sha256sum, shasum or python3 to check the download with"
    fi
}

# The VERSION at the root of a package zip, or nothing when it cannot be read
# (no tool, or a zip without one): a manifest naming one version and a zip
# carrying another is refused before anything is unpacked.
zip_version() {
    if have python3; then
        python3 - "$1" 2>/dev/null <<'EOF'
import sys, zipfile
z = zipfile.ZipFile(sys.argv[1])
for info in z.infolist():
    if info.filename.replace('\\', '/') == 'VERSION':
        print(z.read(info).decode('ascii', 'replace').strip())
        break
EOF
    elif have unzip; then
        unzip -p "$1" VERSION 2>/dev/null | tr -d '\r\n '
    fi
}

# The zip is laid out relative to the server folder (VERSION, CHANGELOG.md,
# linux-port/..., launcher/..., tools/...), with backslashes in some entry
# names -- Windows made it. Python's zipfile takes both; unzip needs the
# names normalised, so python is preferred.
#
# Every file is written beside its place and renamed onto it, never written
# into it. The package carries this script, and sh reads a script as it runs
# it: overwritten in place, the running copy went on reading the new one from
# its old offset, a line cut in half, and ended "Unterminated quoted string"
# (code 2) after an update that had worked (Tyrion, 23 September, on a mock
# server). A rename leaves the running shell its own file. A file that was
# there keeps its mode; a new .sh is made executable.
unpack_over() {
    _zip=$1; _dst=$2
    if have python3; then
        python3 - "$_zip" "$_dst" <<'EOF'
import os, sys, zipfile
z = zipfile.ZipFile(sys.argv[1]); dst = sys.argv[2]; n = 0
for info in z.infolist():
    name = info.filename.replace('\\', '/')
    if name.endswith('/') or name.startswith('/') or '..' in name.split('/'):
        continue
    target = os.path.join(dst, *name.split('/'))
    os.makedirs(os.path.dirname(target), exist_ok=True)
    temp = '%s.m2-update.%d' % (target, os.getpid())
    try:
        with z.open(info) as src, open(temp, 'wb') as out:
            out.write(src.read())
        try:
            if os.path.exists(target):
                os.chmod(temp, os.stat(target).st_mode & 0o7777)
            if name.endswith('.sh'):
                os.chmod(temp, 0o755)
        except OSError:
            pass
        try:
            os.replace(temp, target)
        except OSError:
            # A file system that will not rename onto an open file (a Windows
            # folder shared into a container) gets it written in place, as
            # before; the dispatch at the bottom still keeps this script safe.
            with open(temp, 'rb') as src, open(target, 'wb') as out:
                out.write(src.read())
    finally:
        if os.path.exists(temp):
            os.remove(temp)
    n += 1
print('unpacked %d files' % n)
EOF
    elif have unzip; then
        _stage="$_dst/.m2-update-unpack.$$"
        rm -rf "$_stage"
        mkdir -p "$_stage" && unzip -o -q "$_zip" -d "$_stage" || { rm -rf "$_stage"; return 1; }
        ( cd "$_stage" && find . -type f ) | while IFS= read -r _f; do
            _f=${_f#./}
            [ -x "$_dst/$_f" ] && chmod +x "$_stage/$_f"
            mkdir -p "$(dirname "$_dst/$_f")" && mv -f "$_stage/$_f" "$_dst/$_f" || exit 1
        done
        _rc=$?
        rm -rf "$_stage"
        return $_rc
    else
        die "neither python3 nor unzip is installed"
    fi
}

installed_version() { tr -d ' \r\n' < "$ROOT/VERSION" 2>/dev/null || printf 'unknown'; }

# ---- sanity ------------------------------------------------------------------
check_tree() {
    [ -f "$ROOT/VERSION" ] || die "no VERSION in $ROOT -- run this from the server folder (the one with linux-port/ in it)"
    [ -f "$COMPOSE_DIR/docker-compose.yml" ] || die "no linux-port/docker/docker-compose.yml under $ROOT"
    if [ -f "$COMPOSE_DIR/ENGINE" ] && [ "$(tr -d ' \r\n' < "$COMPOSE_DIR/ENGINE")" != "mt2009" ]; then
        die "this folder is not the 2.x (mt2009) line -- its ENGINE marker says $(cat "$COMPOSE_DIR/ENGINE"); the 1.x line updates through installer/install.sh"
    fi
    if [ -d "$ROOT/installer" ] && [ ! -f "$COMPOSE_DIR/ENGINE" ]; then
        die "this looks like a 1.x checkout (installer/ present, no ENGINE marker); this script is for the 2.x package"
    fi
}

# ---- the sequence ------------------------------------------------------------
STEP=0; STEPS=4
STATUS="$SPOOL/update.status"; LOG="$SPOOL/update.log"
WATCHING=0
set_status() {
    [ "$WATCHING" = 1 ] || return 0
    ( umask 007
      { printf 'state=%s\n' "$1"; printf 'time=%s\n' "$(date +%s)"; printf 'step=%s\n' "$STEP"
        printf 'steps=%s\n' "$STEPS"; printf 'message=%s\n' "$2"; } > "$STATUS.new" ) && mv "$STATUS.new" "$STATUS"
    return 0
}
note() { say "$*"; [ "$WATCHING" = 1 ] && printf '%s %s\n' "$(now)" "$*" >> "$LOG" 2>/dev/null; return 0; }
step() { STEP=$((STEP + 1)); note "[$STEP/$STEPS] $*"; set_status running "$*"; }
fail() { note "FAILED: $*"; note "   nothing was removed; the server keeps running the version it had"; set_status failed "$1"; return 1; }

# The containers take their clock's zone from M2_TZ, and .env.example has
# always said UTC: a Polish player's panel two hours behind the clock of the
# machine it runs on (hunmar, 14 September). Run here on the host, the host's
# zone is known, so the example's UTC is replaced once and M2_TZ_DEFAULTED
# records it - a zone set afterwards, UTC included, is left as it is. Inside
# the updater container the host's zone cannot be seen, and nothing changes.
migrate_timezone() {
    _env="$COMPOSE_DIR/.env"
    [ -f "$_env" ] || return 0
    [ -f /.dockerenv ] && return 0
    grep -q '^M2_TZ_DEFAULTED=' "$_env" && return 0
    _cur=$(kv "$_env" M2_TZ | tr -d ' \r')
    _zone=""
    if [ -z "$_cur" ] || [ "$_cur" = UTC ]; then
        if have timedatectl; then _zone=$(timedatectl show -p Timezone --value 2>/dev/null); fi
        if [ -z "$_zone" ] && [ -f /etc/timezone ]; then _zone=$(head -n 1 /etc/timezone | tr -d ' \r'); fi
        if [ -z "$_zone" ] && [ -L /etc/localtime ]; then _zone=$(readlink /etc/localtime | sed 's|.*zoneinfo/||'); fi
        case "$_zone" in
            */*|UTC) ;;
            *) return 0 ;;
        esac
    fi
    [ -n "$(tail -c 1 "$_env")" ] && printf '\n' >> "$_env"
    if [ -n "$_zone" ]; then
        if grep -q '^M2_TZ=' "$_env"; then
            sed -i "s|^M2_TZ=.*|M2_TZ=$_zone|" "$_env"
        else
            printf 'M2_TZ=%s\n' "$_zone" >> "$_env"
        fi
        note "   the server's clock zone: M2_TZ=$_zone (this machine's own)"
    fi
    printf 'M2_TZ_DEFAULTED=1\n' >> "$_env"
}

# Split puts each kingdom on its own core and no bot can cross between them, so
# Shinsoo and Jinno stop at about thirty-six; unified has been the switch out of
# that since 2.0.30 and hardly anybody knew of it. Flipped exactly once, the way
# the three kingdoms were - unless this world asks for more bots than one core
# was measured to carry (9.4 s of every 60 at 1500), where split stays.
migrate_world_layout() {
    _env="$COMPOSE_DIR/.env"
    [ -f "$_env" ] || return 0
    grep -q '^M2_PLAYERBOT_WORLD_LAYOUT_DEFAULTED=' "$_env" && return 0
    # How big this world is. Since 2.0.83 an operator may ask per kingdom
    # instead of once, and then PLAYERBOT_AUTOSPAWN_COUNT says nothing about
    # the size - three times seven hundred is the world one core would carry.
    if [ "$(kv "$_env" PLAYERBOT_AUTOSPAWN_PER_KINGDOM | tr -d ' \r')" = 1 ]; then
        _bots=0
        for _k in PLAYERBOT_AUTOSPAWN_SHINSOO PLAYERBOT_AUTOSPAWN_CHUNJO PLAYERBOT_AUTOSPAWN_JINNO; do
            _one=$(kv "$_env" "$_k" | tr -d ' \r')
            case "$_one" in
                ''|*[!0-9]*) _one=0 ;;
            esac
            _bots=$((_bots + _one))
        done
    else
        _bots=$(kv "$_env" PLAYERBOT_AUTOSPAWN_COUNT | tr -d ' \r')
        case "$_bots" in
            ''|*[!0-9]*) _bots=0 ;;
        esac
    fi
    [ -n "$(tail -c 1 "$_env")" ] && printf '\n' >> "$_env"
    if [ "$_bots" -gt 1500 ]; then
        note "   the world layout stays split: this world asks for $_bots bots"
        printf 'M2_PLAYERBOT_WORLD_LAYOUT_DEFAULTED=1\n' >> "$_env"
        return 0
    fi
    if grep -q '^M2_PLAYERBOT_WORLD_LAYOUT=' "$_env"; then
        sed -i 's|^M2_PLAYERBOT_WORLD_LAYOUT=.*|M2_PLAYERBOT_WORLD_LAYOUT=unified|' "$_env"
    else
        printf 'M2_PLAYERBOT_WORLD_LAYOUT=unified\n' >> "$_env"
    fi
    note "   the world layout: M2_PLAYERBOT_WORLD_LAYOUT=unified (every kingdom reaches the frontier)"
    printf 'M2_PLAYERBOT_WORLD_LAYOUT_DEFAULTED=1\n' >> "$_env"
}

# Every 2.x world is meant to run all three kingdoms ("istotne, by tak bylo u
# kazdego"), and a .env is written once and kept: a Linux install made before
# 2.0.8 carries M2_PLAYERBOT_KINGDOMS=0 and stayed a Chunjo-only world for
# good, because only the Windows launcher ever flipped it
# (Assert-KingdomsDefault in start-server.ps1). Flipped here exactly once too;
# an operator who sets 0 again afterwards keeps 0.
migrate_kingdoms() {
    _env="$COMPOSE_DIR/.env"
    [ -f "$_env" ] || return 0
    grep -q '^M2_PLAYERBOT_KINGDOMS_DEFAULTED=' "$_env" && return 0
    [ -n "$(tail -c 1 "$_env")" ] && printf '\n' >> "$_env"
    if grep -q '^M2_PLAYERBOT_KINGDOMS=' "$_env"; then
        if [ "$(kv "$_env" M2_PLAYERBOT_KINGDOMS | tr -d ' \r')" != 1 ]; then
            sed -i 's|^M2_PLAYERBOT_KINGDOMS=.*|M2_PLAYERBOT_KINGDOMS=1|' "$_env"
            note "   three kingdoms: M2_PLAYERBOT_KINGDOMS=1 (Shinsoo, Chunjo and Jinno; the migrator seeds the two new ones on this start)"
        fi
    else
        printf 'M2_PLAYERBOT_KINGDOMS=1\n' >> "$_env"
    fi
    printf 'M2_PLAYERBOT_KINGDOMS_DEFAULTED=1\n' >> "$_env"
}

# 2.2.11 dropped a Blessing Scroll from one Metin stone in twenty, and
# Iwakura's answer the same evening was one in a hundred for the test. The
# 2.2.11 value is in every .env add_missing_env_keys gave the key to, where a
# new default never reaches, so 50 becomes 10 here, once; any other value is
# somebody's choice and stays.
migrate_blessing_scroll() {
    _env="$COMPOSE_DIR/.env"
    [ -f "$_env" ] || return 0
    grep -q '^M2_BLESSING_SCROLL_STONE_PERMILLE_DEFAULTED=' "$_env" && return 0
    [ -n "$(tail -c 1 "$_env")" ] && printf '\n' >> "$_env"
    if [ "$(kv "$_env" M2_BLESSING_SCROLL_STONE_PERMILLE | tr -d ' \r')" = 50 ]; then
        sed -i 's|^M2_BLESSING_SCROLL_STONE_PERMILLE=.*|M2_BLESSING_SCROLL_STONE_PERMILLE=10|' "$_env"
        note "   Blessing Scrolls from Metins: M2_BLESSING_SCROLL_STONE_PERMILLE=10 (1%; 2.2.11 had 5%)"
    fi
    printf 'M2_BLESSING_SCROLL_STONE_PERMILLE_DEFAULTED=1\n' >> "$_env"
}

# The server's logs were kept 3 days, and the owner wants a week (1 October,
# "Logi serwera niech usuwaja sie po 7 dniach"). The old default is in every
# .env, where a new one never reaches, so 3 becomes 7 here, once; any other
# value is somebody's choice and stays.
migrate_log_keep_days() {
    _env="$COMPOSE_DIR/.env"
    [ -f "$_env" ] || return 0
    grep -q '^M2_LOG_KEEP_DAYS_DEFAULTED=' "$_env" && return 0
    [ -n "$(tail -c 1 "$_env")" ] && printf '\n' >> "$_env"
    if [ "$(kv "$_env" M2_LOG_KEEP_DAYS | tr -d ' \r')" = 3 ]; then
        sed -i 's|^M2_LOG_KEEP_DAYS=.*|M2_LOG_KEEP_DAYS=7|' "$_env"
        note "   the server's logs are kept 7 days: M2_LOG_KEEP_DAYS=7 (it was 3)"
    fi
    printf 'M2_LOG_KEEP_DAYS_DEFAULTED=1\n' >> "$_env"
}

# Channel N listens on BASE+10*(N-1)..+2 inside the container (BASE is
# M2_GAME_PORT_BASE, 13000 unless a second stack on the host moved it -
# m2-render-config), and compose publishes M2_GAME_PORT_RANGE onto
# M2_GAME_CONTAINER_PORT_RANGE - so with the
# second channel on and the range left at 13000-13002 the cores are up, the
# bots play on CH2 and nobody outside the machine can reach it. Only the
# Windows launcher ever widened it, so a Linux host, or anyone who switched the
# channel on in the panel, had CH2 running and unreachable: "Boty graly na ch2
# lecz ja nie moglem sie logowac" (GoracyDelfin, 19 September), fixed by hand
# in .env. The wish the panel writes lives on a volume, so it is read from the
# running container when there is one; .env alone answers otherwise.
sync_channel_ports() {
    _env="$COMPOSE_DIR/.env"
    [ -f "$_env" ] || return 0
    _ch2=$(kv "$_env" M2_PLAYERBOT_CH2 | tr -d ' \r')
    _channels=$(kv "$_env" M2_CHANNELS | tr -d ' \r')
    _env_at=$(kv "$_env" M2_PLAYERBOT_CH2_SET_AT | tr -d ' \r')
    case "$_env_at" in
        ''|*[!0-9]*) _env_at=0 ;;
    esac
    # The panel's wish wins only when it is newer than .env's own moment
    # (M2_PLAYERBOT_CH2_SET_AT), as it does in the entrypoint; a world whose
    # operator set M2_CHANNELS=2 by hand keeps its ports.
    _wishes=$( (cd "$COMPOSE_DIR" && docker compose exec -T game cat /opt/m2spool/channels.wanted) 2>/dev/null |
        tr -d ' \r')
    _wish=$(printf '%s\n' "$_wishes" | sed -n 's/^CH2=//p' | head -n 1)
    _wish_at=$(printf '%s\n' "$_wishes" | sed -n 's/^SET_AT=//p' | head -n 1)
    case "$_wish_at" in
        ''|*[!0-9]*) _wish_at=0 ;;
    esac
    case "$_wish" in
        0|1)
            if [ "$_wish_at" -gt "$_env_at" ]; then
                _ch2="$_wish"
            fi
            ;;
    esac
    # Every channel the world runs keeps its ports: M2_CHANNELS (1-4), and
    # at least two while the second channel is on.
    _need=1
    case "$_channels" in
        2|3|4) _need="$_channels" ;;
    esac
    if [ "$_ch2" = 1 ] && [ "$_need" -lt 2 ]; then
        _need=2
    fi
    # The channels' first port inside the container: M2_GAME_PORT_BASE, which
    # the cores listen on (13000 when .env does not say). It used to be
    # 13000 here whatever the base was, and a world on 43000 got 13000-13012
    # at its first update with the second channel on (vps4, 2.10 -> 2.14).
    _base=$(kv "$_env" M2_GAME_PORT_BASE | tr -d ' \r')
    case "$_base" in
        ''|*[!0-9]*) _base=13000 ;;
    esac
    [ "$_base" -gt 0 ] && [ "$_base" -lt 65500 ] || _base=13000
    _span=$((10 * (_need - 1) + 2))
    # The published side keeps its distance from the container's: a host that
    # moved its ports (M2_GAME_PORT_RANGE 14000-..., container 13000-...)
    # keeps them, and one whose two ranges were the same stays so.
    _host_first=$(kv "$_env" M2_GAME_PORT_RANGE | tr -d ' \r' | sed -n 's/^\([0-9][0-9]*\).*/\1/p')
    _cont_first=$(kv "$_env" M2_GAME_CONTAINER_PORT_RANGE | tr -d ' \r' | sed -n 's/^\([0-9][0-9]*\).*/\1/p')
    [ -n "$_cont_first" ] || _cont_first=$_base
    if [ -n "$_host_first" ]; then
        _host_first=$((_host_first - _cont_first + _base))
    else
        _host_first=$_base
    fi
    if [ "$_host_first" -le 0 ] || [ "$_host_first" -ge 65500 ]; then
        _host_first=$_base
    fi
    _want_host=$_host_first-$((_host_first + _span))
    _want_cont=$_base-$((_base + _span))
    _want="$_want_host (container $_want_cont)"
    _changed=0
    for _key in M2_GAME_PORT_RANGE M2_GAME_CONTAINER_PORT_RANGE; do
        if [ "$_key" = M2_GAME_PORT_RANGE ]; then _want_one=$_want_host; else _want_one=$_want_cont; fi
        _cur=$(kv "$_env" "$_key" | tr -d ' \r')
        [ "$_cur" = "$_want_one" ] && continue
        [ -n "$(tail -c 1 "$_env")" ] && printf '\n' >> "$_env"
        if grep -q "^$_key=" "$_env"; then
            sed -i "s|^$_key=.*|$_key=$_want_one|" "$_env"
        else
            printf '%s=%s\n' "$_key" "$_want_one" >> "$_env"
        fi
        _changed=1
    done
    [ "$_changed" = 1 ] && note "   the channels' ports: $_want ($_need channel(s), second channel $([ "$_ch2" = 1 ] && echo on || echo off))"
    return 0
}

# A new key in .env.example reaches nobody who already installed: .env is
# written at install and never rewritten, and only the Windows launcher
# (Add-MissingDotEnvKeys) ever appended the keys a release added - a Linux
# host updated by this script had no M2_DIFFICULTY and no wait hours after
# 2.0.57 (GoracyDelfin, 17 September). Every KEY=value line of the example
# whose key .env does not carry is appended with the example's value - only
# the keys named below, whose example value is the compose default (an
# absent key already meant that), never a password, a port or an address;
# a key already there, empty included, is the operator's and is left alone.
ENV_KEYS_FROM_EXAMPLE="M2_DIFFICULTY M2_BIOLOGIST_WAIT_HOURS M2_HORSE_WAIT_HOURS M2_BOOK_WAIT_HOURS M2_BOT_BOOK_WAIT_HOURS M2_EXCHANGE_DUST_CHANCE M2_EXCHANGE_PARCHMENT_CHANCE M2_EXCHANGE_MATERIAL_CHANCE PLAYERBOT_SPAWN_WINDOW_MINUTES PLAYERBOT_LATE_JOINERS PLAYERBOT_LATE_JOIN_HOURS PLAYERBOT_MEDAL_DROPPERS PLAYERBOT_MEDAL_DROPPER_LEVEL M2_MOONLIGHT_CHEST_PERMILLE M2_MOONLIGHT_CHEST_STONE_PERMILLE M2_BLESSING_SCROLL_STONE_PERMILLE M2_PLAYERBOT_WORLD_LAYOUT PLAYERBOT_AUTOSPAWN_PER_KINGDOM PLAYERBOT_AUTOSPAWN_SHINSOO PLAYERBOT_AUTOSPAWN_CHUNJO PLAYERBOT_AUTOSPAWN_JINNO M2_PLAYERBOT_CH2 PLAYERBOT_CH2_SHARE M2_PLAYERBOT_CH2_SET_AT M2_RATE_EXP M2_RATE_DROP M2_RATE_YANG M2_PLAYERBOT_START_HELD M2_STARTER_CHEST M2_AUTOHUNT M2_SIDEKICK M2_ALCHEMY M2_SASHES M2_AUTOHUNT_ITEM M2_FLEA_MARKET M2_AREZZO M2_SEONHAE M2_MONSTER_HP M2_STARTER_KIT"
add_missing_env_keys() {
    _env="$COMPOSE_DIR/.env"
    _ex="$COMPOSE_DIR/.env.example"
    [ -f "$_env" ] && [ -f "$_ex" ] || return 0
    # MT2009_PLUS_UPDATE_ENV_NOW_V1: the list the update.sh on the disk names,
    # which after an unpack is the new release's. The shell read this file
    # whole before the package replaced it, so the list in this process is the
    # previous release's, and a key a release added reached .env only at the
    # update after it (2.10.0 -> 2.16.0 on Linux left out M2_AREZZO,
    # M2_FLEA_MARKET and M2_EXCHANGE_*). run_update normally hands the whole
    # .env step to the new script (migrate_env_from_release); this covers a
    # fallback to this process. Run alone (`env') it reads itself.
    _keys=$(tr -d '\r' < "$ROOT/linux-port/tools/update.sh" 2>/dev/null | sed -n 's/^ENV_KEYS_FROM_EXAMPLE="\([^"]*\)".*$/\1/p' | head -n 1)
    [ -n "$_keys" ] || _keys=$ENV_KEYS_FROM_EXAMPLE
    _added=""
    while IFS= read -r _line || [ -n "$_line" ]; do
        _line=$(printf '%s' "$_line" | tr -d '\r')
        case "$_line" in
            [A-Z_0-9]*=*) ;;
            *) continue ;;
        esac
        _key=${_line%%=*}
        case " $_keys " in
            *" $_key "*) ;;
            *) continue ;;
        esac
        grep -q "^$_key=" "$_env" && continue
        [ -n "$(tail -c 1 "$_env")" ] && printf '\n' >> "$_env"
        printf '%s\n' "$_line" >> "$_env"
        _added="$_added $_key"
    done < "$_ex"
    [ -n "$_added" ] && note "   new .env keys, at the example's defaults:$_added"
    add_other_missing_env_keys
    return 0
}

# Every other key of the example that .env lacks, so a reinstall or an update
# leaves an .env that names every setting the release knows (it used to keep
# only the list above). Added only where it changes nothing: the example's
# value is empty (compose reads an empty key as an absent one) or is the very
# default docker-compose.yml gives the key. Never a password, a secret or a
# token, never a once-only marker (*_DEFAULTED, *_SET_AT: their absence is
# what makes a migration run), and never a key .env already has, empty or not.
add_other_missing_env_keys() {
    _env="$COMPOSE_DIR/.env"
    _ex="$COMPOSE_DIR/.env.example"
    _compose="$COMPOSE_DIR/docker-compose.yml"
    [ -f "$_env" ] && [ -f "$_ex" ] || return 0
    _added=""
    while IFS= read -r _line || [ -n "$_line" ]; do
        _line=$(printf '%s' "$_line" | tr -d '\r')
        case "$_line" in
            [A-Z_0-9]*=*) ;;
            *) continue ;;
        esac
        _key=${_line%%=*}
        _val=${_line#*=}
        case "$_key" in
            *PASSWORD*|*SECRET*|*TOKEN*|*_DEFAULTED|*_SET_AT) continue ;;
        esac
        grep -q "^$_key=" "$_env" && continue
        if [ -n "$_val" ]; then
            [ -f "$_compose" ] || continue
            # The key's compose defaults, ${KEY:-value}; one, and the same.
            _defaults=$(grep -o "\${$_key:-[^}]*}" "$_compose" 2>/dev/null | sed "s/^\\\${$_key:-//; s/}\$//" | sort -u)
            [ -n "$_defaults" ] || continue
            [ "$(printf '%s\n' "$_defaults" | wc -l)" = 1 ] || continue
            [ "$_defaults" = "$_val" ] || continue
        fi
        [ -n "$(tail -c 1 "$_env")" ] && printf '\n' >> "$_env"
        printf '%s\n' "$_line" >> "$_env"
        _added="$_added $_key"
    done < "$_ex"
    [ -n "$_added" ] && note "   .env keys it did not name, at what they already meant:$_added"
    return 0
}

# The panel's build context is staged from files/ on the player's machine: the
# Windows launcher does it before every build (Sync-M2PlayerbotOverlay), and
# nothing did it here. The panel's Dockerfile COPYs schema/ and app/, so a
# Linux install built from the package alone stopped at "/schema: not found"
# and the whole compose build was cancelled with it (DUDU's VPS, 18 September).
# The same list as the launcher's; `sh update.sh stage' runs it alone.
restore_empty_context_dirs() {
    # share/package is empty on every install of both lines and the game
    # Dockerfile COPYs it all the same, so a tree whose unpacking dropped the
    # bare directory entry - git cannot carry one either - fails the build at
    # "failed to compute cache key" (dekri, 20 September, on Windows). Make it
    # rather than let the build die over a directory with nothing in it.
    # serverfiles/mark-default (the guild symbols' seed) is empty and COPYd
    # the same way ("/src/serverfiles/mark-default: not found", 27 September).
    for d in "$COMPOSE_DIR/game/src/serverfiles/share/package" \
             "$COMPOSE_DIR/game/src/serverfiles/mark-default"; do
        [ -d "$d" ] || mkdir -p "$d" 2>/dev/null || true
    done
}

stage_panel_context() {
    _panel="$COMPOSE_DIR/panel"
    [ -d "$_panel" ] || return 0
    mkdir -p "$_panel/app" "$_panel/schema" || return 1
    # The advanced panel's own build context wants the same thing, and until
    # 2.0.90 nothing on Linux put it there: its Dockerfile tolerates the file
    # being absent now, but a panel that says "dev" instead of a version is
    # still a panel nobody can tell is behind.
    if [ -d "$COMPOSE_DIR/seban-panel" ] && [ -f "$ROOT/VERSION" ]; then
        cp -a "$ROOT/VERSION" "$COMPOSE_DIR/seban-panel/VERSION" 2>/dev/null || true
    fi
    for _pair in "VERSION:app/VERSION" "CHANGELOG.md:app/CHANGELOG.md" \
            "files/admin_panel.py:app/admin_panel.py" "files/items.json:app/items.json" \
            "files/favicon.png:app/favicon.png" \
            "files/web_admin_schema.sql:schema/web_admin_schema.sql"; do
        _from="$ROOT/${_pair%%:*}"
        [ -f "$_from" ] || continue
        cp -f "$_from" "$_panel/${_pair#*:}" || return 1
    done
    if [ -d "$ROOT/files/static" ]; then
        mkdir -p "$_panel/app/static" || return 1
        cp -R "$ROOT/files/static/." "$_panel/app/static/" || return 1
    fi
    return 0
}

run_update() {
    STEP=0
    rm -rf "$WORK"; mkdir -p "$WORK" || { fail "cannot create $WORK"; return 1; }
    step "reading what is published"
    # What a tool hands over (the header): `run' only, never the panel's watch.
    _given_manifest=""; _given_zip=""
    if [ "$WATCHING" != 1 ]; then
        _given_manifest=${M2_UPDATE_MANIFEST_FILE:-}
        _given_zip=${M2_UPDATE_ZIP:-}
    fi
    if [ -n "$_given_manifest" ]; then
        cp "$_given_manifest" "$WORK/manifest.json" 2>"$WORK/fetch.err" || { fail "the manifest $_given_manifest could not be read: $(head -c 200 "$WORK/fetch.err")"; return 1; }
        note "   the manifest is $_given_manifest (M2_UPDATE_MANIFEST_FILE)"
    else
        fetch_manifest > "$WORK/manifest.json" 2>"$WORK/fetch.err" || { fail "the manifest could not be read (GitHub and the fallback server): $(head -c 200 "$WORK/fetch.err")"; return 1; }
        [ -n "$FETCHED_FROM_MIRROR" ] && note "   $MIRROR_NOTICE ($FETCHED_FROM_MIRROR)"
    fi
    MANIFEST_MIRRORS=$(manifest_mirrors "$WORK/manifest.json")
    _ver=$(manifest_field "$WORK/manifest.json" version)
    _url=$(manifest_field "$WORK/manifest.json" url)
    _sha=$(manifest_field "$WORK/manifest.json" sha256 | tr 'A-F' 'a-f')
    [ -n "$_ver" ] && [ -n "$_url" ] && [ -n "$_sha" ] || { fail "the manifest has no server version, url or sha256"; return 1; }
    note "   installed $(installed_version), published $_ver"
    if [ "$(installed_version)" = "$_ver" ] && [ "${FORCE:-0}" != 1 ]; then
        # The files are this version, but the last run stopped before its
        # build finished (BUILD_PENDING is written right after the unpack):
        # VERSION alone said "nothing to do" and the server kept the old
        # images for good ("already on 2.14.0", 29 September). Build now.
        if [ -f "$BUILD_PENDING" ]; then
            note "   the files are $_ver, but the last update did not finish building -- building now"
            build_and_start
            return $?
        fi
        note "   already on $_ver -- nothing to do (FORCE=1 to unpack it again)"
        set_status ok "the server is running version $_ver"
        return 0
    fi
    if [ -n "$_given_zip" ]; then
        step "taking the package $_given_zip (M2_UPDATE_ZIP)"
        cp "$_given_zip" "$WORK/update.zip" || { fail "the package $_given_zip could not be copied"; return 1; }
    else
        step "downloading $(basename "$_url")"
        download_checked "$_url" "$WORK/update.zip" "$_sha" || { fail "the download failed (GitHub and the fallback server)"; return 1; }
    fi
    _got=$(sha256_of "$WORK/update.zip" | tr 'A-F' 'a-f')
    [ "$_got" = "$_sha" ] || { fail "the package's SHA-256 ($_got) is not the manifest's ($_sha)"; return 1; }
    _zipver=$(zip_version "$WORK/update.zip")
    if [ -n "$_zipver" ] && [ "$_zipver" != "$_ver" ]; then
        fail "the package says version $_zipver and the manifest $_ver -- nothing was unpacked"
        return 1
    fi
    step "unpacking $_ver over $ROOT"
    unpack_over "$WORK/update.zip" "$ROOT" || { fail "the zip could not be unpacked"; return 1; }
    note "   the folder now says version $(installed_version)"
    # From here VERSION says the new version whatever happens to the build;
    # this is what tells the next run to finish it.
    : > "$BUILD_PENDING" 2>/dev/null || true
    migrate_env_from_release
    build_and_start
}

# MT2009_PLUS_UPDATE_ENV_NOW_V1: the .env step of an update is the NEW
# release's, not this process's. The functions and the key list in memory are
# the version being replaced, so whatever the new version brings for .env (a
# key, a once-only flip) used to arrive only with the update after it. The
# freshly unpacked update.sh is run with `env' - the same thing
# `sh linux-port/tools/update.sh env' does by hand - on this folder, and
# under the panel its lines go to the spool's log like ours. A new script that
# is missing or fails (an older package without `env', say) leaves the step to
# this process, whose add_missing_env_keys still reads the list from the disk.
migrate_env_from_release() {
    _new="$ROOT/linux-port/tools/update.sh"
    if [ -f "$_new" ]; then
        note "   .env: the new version's settings (update.sh env)"
        if M2_UPDATE_STACK_DIR="$ROOT" M2_UPDATE_WATCHING="$WATCHING" sh "$_new" env; then
            return 0
        fi
        note "   .env: the new update.sh env did not finish -- this script does it instead"
    fi
    migrate_env
}

# The half of an update after the files: staged build inputs, then compose.
# Also what a run that finds BUILD_PENDING does, so a build that failed (or
# a VPS that rebooted halfway) is finished without downloading anything.
build_and_start() {
    # Before compose, because a published port range only changes at a recreate
    # (migrate_env did it already on a full update; a pending build redoes it).
    sync_channel_ports
    restore_empty_context_dirs
    stage_panel_context || { fail "the panel's build context could not be staged from files/"; return 1; }
    step "building and starting the new version (docker compose up -d --build)"
    # MT2009_PLUS_SLOW_BUILD_NOTICE_V1: said before the longest step, so nobody
    # takes a quiet terminal for a hang and breaks it off (the owner, 4 October).
    _slow="UWAGA: teraz kompiluje sie silnik gry i startuje serwer - to moze potrwac nawet do 15 minut (na slabszym VPS dluzej). Nie przerywaj i nie zamykaj tego okna, nawet jesli przez dluzszy czas nic sie nie wypisuje."
    say "$_slow"
    [ "$WATCHING" = 1 ] && printf '%s [update] %s\n' "$(now)" "$_slow" >> "$LOG" 2>/dev/null
    # By hand the build talks to the terminal; under the panel it goes to the
    # spool's log, which is what the panel's progress page tails.
    if [ "$WATCHING" = 1 ]; then
        ( cd "$COMPOSE_DIR" && docker compose up -d --build ) >> "$LOG" 2>&1
    else
        ( cd "$COMPOSE_DIR" && docker compose up -d --build )
    fi || { fail "the new version was not built or not started -- the log says where it stopped; run this again to retry the build"; return 1; }
    rm -f "$BUILD_PENDING" 2>/dev/null || true
    note "the server is now running version $(installed_version)"
    set_status ok "the server is running version $(installed_version)"
    rm -rf "$WORK"
    return 0
}

kv() { sed -n "s/^$2=//p" "$1" 2>/dev/null | head -n 1; }

# What an update does to .env once the new files are in place, in this order.
# `sh update.sh env' runs it alone: vps-install.sh does when it installs over
# a world that already has an .env (the launcher's "install" over an older
# world on a VPS), so a new key, the channels' ports and the once-only flips
# reach that world as they reach an updated one. Existing values stay.
migrate_env() {
    migrate_timezone
    add_missing_env_keys
    # Before the layout, which reads the kingdoms' own counts.
    migrate_kingdoms
    # After the keys, so a world that had no layout line at all gets the
    # example's and then this.
    migrate_world_layout
    # After the keys too: a world that never had the line gets the example's.
    migrate_blessing_scroll
    migrate_log_keep_days
    # Before compose, because a published port range only changes at a recreate.
    sync_channel_ports
}

watch() {
    WATCHING=1
    mkdir -p "$SPOOL" 2>/dev/null
    say "watching $SPOOL/request for the panel (server folder $ROOT, $(installed_version))"
    _done=""
    while :; do
        touch "$SPOOL/watcher" 2>/dev/null
        if [ -f "$SPOOL/request" ]; then
            _id=$(kv "$SPOOL/request" id)
            if [ -n "$_id" ] && [ "$_id" != "$_done" ]; then
                _done=$_id
                note "update requested (the panel was told the published version is $(kv "$SPOOL/request" version))"
                run_update || true
            fi
        fi
        sleep "$POLL"
    done
}

# One command, read to its closing brace before any of it runs, and it ends in
# exit: whatever replaces this file while it runs - the unpack above, or
# anybody copying a tree over it - the shell never reads another line of it.
{
case "${1:-run}" in
    run)   check_tree; run_update ;;
    check) check_tree; fetch_manifest > "$WORK.m" && printf 'installed %s, published %s\n' "$(installed_version)" "$(manifest_field "$WORK.m" version)"; rm -f "$WORK.m" ;;
    watch) check_tree; watch ;;
    stage) check_tree; stage_panel_context && say "the panel's build context is staged from files/" || die "staging the panel's build context failed" ;;
    # MT2009_PLUS_UPDATE_ENV_NOW_V1: run by an older update.sh right after it
    # unpacked this one (migrate_env_from_release); under the panel its lines
    # go to the spool's log as well.
    env)   [ "${M2_UPDATE_WATCHING:-0}" = 1 ] && WATCHING=1; check_tree; migrate_env ;;
    *) printf 'usage: sh %s [run|check|watch|stage|env]\n' "$0"; exit 2 ;;
esac
exit $?
}
