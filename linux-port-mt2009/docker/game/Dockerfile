# syntax=docker/dockerfile:1.6
# =============================================================================
# mt2009 game image -- the martysama0134 r41023 server built for Linux/i386.
#
#  deps    : the 32-bit static dependency set (build-deps-mt2009.sh)
#  builder : the seven modules of the source tree, in the order the package's
#            own top-level Makefile uses; gcc -m32 -std=c++20 instead of clang
#  runtime : (later) the cores, share/, the rendered CONFIGs
#
# The source under src/server is the package's tree with linux-port-mt2009/
# port/linuxify.py applied (the launcher stages it that way); the build refuses
# a tree that has not been through it, because a kqueue libthecore compiles to
# nothing here and the failure would surface as a hundred link errors.
# =============================================================================
ARG BASE_IMAGE=ubuntu:24.04
# An apt mirror to use instead of archive.ubuntu.com, e.g.
# http://pl.archive.ubuntu.com/ubuntu/ -- the default archive crawls from some
# networks and this build installs ~600 MB of toolchain. Empty keeps the
# image's own sources.
ARG APT_MIRROR=

# -----------------------------------------------------------------------------
FROM ${BASE_IMAGE} AS deps

ARG APT_MIRROR
ENV DEBIAN_FRONTEND=noninteractive
SHELL ["/bin/bash", "-o", "pipefail", "-c"]
ENV M2_EXTERN=/opt/m2extern

# Both Canonical hosts, or apt-get update still hangs on the security pocket
# while the main one answers from the mirror.
RUN if [ -n "${APT_MIRROR}" ]; then \
      sed -i -e "s|http://archive.ubuntu.com/ubuntu/|${APT_MIRROR}|g" \
             -e "s|http://security.ubuntu.com/ubuntu/|${APT_MIRROR}|g" \
             /etc/apt/sources.list.d/ubuntu.sources; \
      grep -q "${APT_MIRROR}" /etc/apt/sources.list.d/ubuntu.sources; \
      ! grep -E '^URIs:.*ubuntu\.com' /etc/apt/sources.list.d/ubuntu.sources; \
    fi

COPY build-deps-mt2009.sh /build/build-deps-mt2009.sh
COPY src/extern-tarballs /build/extern-tarballs
COPY src/extern/cryptopp /build/cryptopp

# CRLF guard: this repository is edited on Windows.
RUN sed -i 's/\r$//' /build/build-deps-mt2009.sh && chmod +x /build/build-deps-mt2009.sh

RUN PREFIX="${M2_EXTERN}" BUILD=/build/work TARBALLS=/build/extern-tarballs \
    CRYPTOPP_SRC=/build/cryptopp \
    /build/build-deps-mt2009.sh apt zlib openssl libmd cryptopp mysql devil verify \
 && rm -rf /build/work/src /build/work/stage /build/cryptopp /var/lib/apt/lists/*

# -----------------------------------------------------------------------------
# libs: the support libraries and the db core. A stage of its own so that
# `docker build --target libs` gives an image to iterate the game core in
# (bind-mount game/, run make) while the overlay is being ported.
# -----------------------------------------------------------------------------
FROM deps AS libs

# A job per core ran out of memory on an 8 GB laptop: Docker Desktop gives
# the build VM half the host, and cmd_general.cpp or char.cpp with -O2 -g
# takes over a gigabyte per compiler, so four of them on four gigabytes
# thrashed until the build looked stuck at "game builder 2/3 67%"
# (.unright, 12 September). MAKE_JOBS (M2_MAKE_JOBS in .env) still wins;
# the default is the smaller of the cores and the memory divided by 1400 MB.
# The game core's own step, below, sizes itself by the memory that is free.
ARG MAKE_JOBS=

RUN apt-get update \
 && apt-get install -y --no-install-recommends ccache \
 && rm -rf /var/lib/apt/lists/*

ENV PATH="/usr/lib/ccache:${PATH}"
ENV CCACHE_DIR=/root/.ccache
# Every module Makefile hard-codes clang; the command line wins over the file.
# ENABLE_GCC_AUTODEPEND=0 keeps game/db off the `sed -i ''` BSD-ism in their
# autodepend recipe (a fresh build compiles every file anyway).
ENV M2_MAKE="CC=gcc CXX=g++ ENABLE_GCC_AUTODEPEND=0"

# The Makefiles reach the dependencies as ../../../Extern relative to */src,
# so the tree is laid out exactly like the package: /src/Server, /src/Extern.
WORKDIR /src/Server

COPY src/extern/include /src/Extern/include
RUN set -eux; \
    mkdir -p /src/Extern/lib; \
    cp -a "${M2_EXTERN}/lib/." /src/Extern/lib/; \
    # The shipped mysql headers belong to another connector build; ours must
    # match the archive we link.  IL ships 1.7.8 headers, the archive is 1.8.0.
    rm -rf /src/Extern/include/mysql; \
    cp -a "${M2_EXTERN}/include/mysql" /src/Extern/include/mysql; \
    cp -a "${M2_EXTERN}/include/IL/." /src/Extern/include/IL/; \
    # libsql includes <mysql/mysql.h> through -I/usr/local/include only.
    mkdir -p /usr/local/include; ln -sfn /src/Extern/include/mysql /usr/local/include/mysql; \
    # game/db link /usr/lib32/lib{ssl,crypto}.a by absolute path.
    mkdir -p /usr/lib32; \
    ln -sfn /usr/lib/i386-linux-gnu/libssl.a /usr/lib32/libssl.a; \
    ln -sfn /usr/lib/i386-linux-gnu/libcrypto.a /usr/lib32/libcrypto.a

# Layer 1: the support libraries, built once and cached.
COPY src/server/__REVISION__ __REVISION__
COPY src/server/common common
COPY src/server/libthecore libthecore
COPY src/server/libpoly libpoly
COPY src/server/libgame libgame
COPY src/server/liblua liblua
COPY src/server/libsql libsql

RUN grep -q 'fdwatch_sndbuf_left' libthecore/src/fdwatch.c \
 && grep -q 'defined(__linux__)' libthecore/src/signal.c \
 || { echo "FATAL: the tree has not been through port/linuxify.py"; exit 1; }

RUN --mount=type=cache,target=/root/.ccache set -eux; \
    find . -name '*.o' -delete; \
    find . \( -name Makefile -o -name config -o -name Depend -o -name __REVISION__ \) -exec sed -i 's/\r$//' {} +; \
    J="${MAKE_JOBS:-}"; if [ -z "$J" ]; then J="$(nproc)"; M="$(awk '/MemTotal/ { print int($2 / 1024 / 1400) }' /proc/meminfo)"; [ "$M" -ge 1 ] || M=1; [ "$M" -ge "$J" ] || J="$M"; fi; echo "make -j$J (cores $(nproc), memory $(awk '/MemTotal/ { print int($2 / 1024) }' /proc/meminfo) MB)"; \
    make -C liblua/5.0 CC=gcc MYCFLAGS="-O2 -g -fno-strict-aliasing"; \
    for m in libsql libgame/src libpoly libthecore/src; do \
      : > "$m/Depend"; \
      make -C "$m" dep ${M2_MAKE}; \
      make -C "$m" -j"$J" ${M2_MAKE}; \
    done; \
    for a in liblua/5.0/lib/liblua.a liblua/5.0/lib/liblualib.a libsql/libsql.a \
             libgame/lib/libgame.a libpoly/libpoly.a libthecore/lib/libthecore.a; do \
      [ -f "$a" ] || { echo "FATAL: $a was not produced"; exit 1; }; \
    done; \
    echo "support archives built"

# Layer 2: the db core. GuildManager.cpp includes one header from the game
# core (../../game/src/locale.hpp, self-contained); copying just that keeps
# this layer cached across game-source edits.
COPY src/server/db db
COPY src/server/game/src/locale.hpp game/src/locale.hpp
RUN --mount=type=cache,target=/root/.ccache set -eux; \
    sed -i 's/\r$//' db/src/Makefile; \
    rm -rf db/src/.obj db/src/Depend; \
    J="${MAKE_JOBS:-}"; if [ -z "$J" ]; then J="$(nproc)"; M="$(awk '/MemTotal/ { print int($2 / 1024 / 1400) }' /proc/meminfo)"; [ "$M" -ge 1 ] || M=1; [ "$M" -ge "$J" ] || J="$M"; fi; echo "make -j$J (cores $(nproc), memory $(awk '/MemTotal/ { print int($2 / 1024) }' /proc/meminfo) MB)"; \
    make -C db/src -j"$J" ${M2_MAKE}; \
    mkdir -p /out; cp "db/db_r$(cat __REVISION__)" /out/db

# -----------------------------------------------------------------------------
FROM libs AS builder

ARG MAKE_JOBS=
ARG STRIP_BINARIES=1

# Layer 3: the game core. Its job count is sized by what is free, not by the
# VM's total, for two reasons measured on 24 September. playerbot_manager.cpp
# is the whole bot AI in one translation unit and takes about 2.3 GB at
# -O2 -g, where an engine file takes 0.5-0.7 GB (the whole core from nothing:
# 98 s and 3 GB at -j4). And an update builds while the old server still runs
# - `up --build` recreates the containers only once the images are built - so
# the bots and the database hold gigabytes of the same VM while this compiles
# (4.5 GB at 1100 bots). Sized by MemTotal, a laptop swapped until the build
# looked stuck at "game builder 2/3 (67%)" again (Drip, 24 September). So:
# room for the heavy file and 700 MB a job after it, out of MemAvailable, and
# a line in the log when even the heavy file does not fit.
COPY src/server/game game
RUN --mount=type=cache,target=/root/.ccache set -eux; \
    sed -i 's/\r$//' game/src/Makefile; \
    rm -rf game/src/.obj game/src/Depend; \
    T="$(awk '/^MemTotal:/ { print int($2 / 1024) }' /proc/meminfo)"; \
    A="$(awk '/^MemAvailable:/ { print int($2 / 1024) }' /proc/meminfo)"; [ -n "$A" ] && [ "$A" -gt 0 ] || A="$T"; \
    J="${MAKE_JOBS:-}"; if [ -z "$J" ]; then J="$(nproc)"; M=$(( 1 + (A - 2600) / 700 )); [ "$M" -ge 1 ] || M=1; [ "$M" -ge "$J" ] || J="$M"; fi; \
    echo "make -j$J (cores $(nproc), memory $T MB, free $A MB)"; \
    if [ "$A" -lt 2600 ]; then echo "UWAGA: w maszynie Dockera wolne jest tylko $A MB pamieci RAM (to nie miejsce na dysku), a najciezszy plik rdzenia potrzebuje okolo 2300 MB. Kompilacja moze trwac bardzo dlugo. Zatrzymaj serwer przed aktualizacja (w launcherze: ZATRZYMAJ I ZAPISZ, potem GRAJ albo SPRAWDZ AKTUALIZACJE)."; fi; \
    make -C game/src -j"$J" ${M2_MAKE}; \
    mkdir -p /out; cp "game/game_r$(cat __REVISION__)" /out/game

# Both cores must be 32-bit with a 32-bit time_t: the wire and DB structs
# depend on it.
RUN set -eux; \
    for b in /out/game /out/db; do \
      file -b "$b" | grep -q 'ELF 32-bit LSB .* Intel 80386' \
        || { echo "FATAL: $b is not a 32-bit i386 ELF"; file "$b"; exit 1; }; \
    done; \
    if strings -a /out/game | grep -q '_TIME_BITS=64'; then \
      echo "FATAL: _TIME_BITS=64 leaked into the build"; exit 1; \
    fi; \
    echo "unstripped: $(du -sh /out | cut -f1)"; \
    if [ "${STRIP_BINARIES}" = "1" ]; then strip --strip-unneeded /out/game /out/db; fi; \
    ls -la /out

# -----------------------------------------------------------------------------
# quests: the panel's in-game helper (web_admin.quest), the movement-speed
# bonus and the starter chest, compiled by the tree's own qc into object files
# the cores load at boot. High Risk is NOT on the list on purpose: an update
# never deletes a file, so every 2.0.0-2.0.5 tree still carries
# quest/high_risk.quest, and 2.0.6 - which only removed the file from the
# repository - kept compiling it on every player's machine ("po wgraniu update
# risk mode nadal jest"). The loop is the gate, not the directory. qc ships as a FreeBSD binary in the package's quest
# directory, so it is built here from game/src/quest against the in-tree Lua
# 5.0. The 300-odd stock quests keep the object files they came with; this
# stage produces only its own three and they are copied over as an addition.
# -----------------------------------------------------------------------------
FROM builder AS quests

COPY quest/ /quest-add/
COPY src/serverfiles/share/locale/poland/quest/quest_functions /quest-src/quest_functions

RUN set -eux; \
    mkdir -p /out-quest/poland/quest; \
    if [ ! -f /quest-add/web_admin.quest ]; then \
      echo "note: no web_admin.quest in the build context -- the panel will use"; \
      echo "      its database fallback and Teleport/Running speed will not work."; \
      exit 0; \
    fi; \
    sed -i 's/\r$//' game/src/quest/Makefile; \
    rm -f game/src/quest/qc game/src/quest/*.o; \
    make -C game/src/quest qc CXX="g++ -m32" \
         CPPFLAGS="-g -w -std=c++20 -include cstring -include cstdio -include cstdlib"; \
    test -x game/src/quest/qc; \
    file -b game/src/quest/qc | grep -q 'ELF 32-bit'; \
    out=/out-quest/poland/quest; \
    rm -rf /tmp/qc-work; mkdir -p /tmp/qc-work/object; \
    sed 's/\r$//' /quest-src/quest_functions > /tmp/qc-work/quest_functions; \
    # Functions the engine has and the package's list forgot, which qc would
    # otherwise refuse (gm_profile.quest: equipping, bonus lines, stack size,
    # the item's use path).
    # pc.open_special_shop for fishing_pass_shop (2.0.68); affect.add_new and
    # affect.remove_new for the panel's speed in web_admin (2.0.76).
    # pc.is_playerbot for the Devil's Catacomb's bot branches (2.2.22).
    # affect.get_duration and pc.has_affect for the ItemShop's Auto Lowy ticket
    # and anti-experience ring (autohunt_time, antiexp_ring; 2.2.27).
    # pc.warp_channel for the panels' teleport to a bot on another channel
    # (web_admin; 2.2.42).
    for fn in mysql_direct_query pc.equip_slot pc.unequip_slot item.set_attr item.get_max_stack item.use pc.open_special_shop affect.add_new affect.remove_new pc.is_playerbot affect.get_duration pc.has_affect pc.warp_channel; do \
      grep -qx "$fn" /tmp/qc-work/quest_functions || echo "$fn" >> /tmp/qc-work/quest_functions; \
    done; \
    # pony_levelup is the package's own stable-keeper quest with the 18-hour
    # wait between two medals taken out (2.0.12); compiled here under its own
    # name, its object files land over the stock ones in the runtime copy.
    # gm_profile brings the admin account's game masters to one profile
    # through the engine (2.0.13).
    # new_quest_lv19 is the package's level-19 quest with its bear group
    # completed: the stock group names 140 and 141 only, so the Cursed Bear
    # (139) and the Cursed Brown Bear (142) never counted towards the five
    # bear skins (Pabloo, from Dixdros' report, 2.0.47).
    # teleport_ring answers the use of Pierscien Teleportacji (70058), which the
    # package drops and no stock quest handled (2.0.48).
    # pony_buy, horse_upgrade, horse_upgrade2 and horse_levelup are the
    # package's stable-keeper quests with the wait for the horse taken out:
    # twelve hours for the pony and each upgrade, twenty-one between two
    # trainings (16 September, 2.0.56).
    # fishing_pass_shop opens the Fisherman's special shop, the only seller
    # of Karta Wedkarska, which no stock quest ever opened (2.0.68).
    # collect_quest_lv85 is the Biologist's red-branch row with its hand-in
    # asking for the right item: the stock quest counted 30166, the level-80
    # row's, so nobody could hand in a single 30167 (2.0.95).
    # towarzysz is the letter that makes and commands a player's companion
    # (playerbot_sidekick.h): class, path and name, then call, free, dismiss.
    # devilcatacomb_zone is the Devil's Catacomb, which the package kept in
    # quest/_unused and never compiled (26 September).
    # check_trans_ticket is the package's own with Koe-Pung's branch put back:
    # the Grotto of Exile for a Bloodstone from 75, in place of the free warp
    # 2.2.21 opened; heavens_cave_keyquest is Seon-Hae's daily quest for the
    # Bloodstone, from quest/_unused (2.2.22).
    # tanaka_ears is Yonah's exchange of Pirate Tanaka's ear for a Purple Ebony
    # Chest, the Tanaka event's reward (playerbot_world_events.h).
    # seon_pyeong opens Seon-Pyeong's cube window by the Grotto of Exile: a +9
    # piece of level 65-66 into its level-80 successor (cube.seon_pyeong.txt,
    # appended to the share's cube.txt below; NerrVoVy, 27 September).
    # flea_market is the Dom Towarowy's two options at the miscellaneous
    # merchant in M1, offered only while M2_FLEA_MARKET is on (Uxie [DSO]).
    # temple_of_the_ochao is the Temple of Ochao's entrance (Straznik Swiatyni,
    # 20426), its portal (20415) and the Bodyguard's reward (MT2009_PLUS_OCHAO_V1).
    # biblioteka_wiedzy is the Arezzo library dungeon (363) and map_warp the package's
    # Teleporter with Dolina Cyklopow (360) on its first page (MT2009_PLUS_AREZZO_V1).
    # dungeon_panel is the dungeon panel's part in the quests: the instances' start and the
    # bosses of the open maps (MT2009_PLUS_DUNGEON_PANEL_V1, playerbot_dungeon_panel.h).
    # wzgorze_wukonga, ruiny_skorpiona and starozytna_dzungla are the Arezzo dungeons 364-366
    # (MT2009_PLUS_AREZZO_V1, phases 5-7).
    for q in web_admin speed_boost starter_chest pony_levelup gm_profile new_quest_lv19 teleport_ring pony_buy horse_upgrade horse_upgrade2 horse_levelup deviltower_zone fishing_pass_shop collect_quest_lv85 towarzysz devilcatacomb_zone check_trans_ticket heavens_cave_keyquest tanaka_ears acce_costume_uriel ah_yu_shop costume_bonus_transfer dragon_soul dragon_soul_refine dragon_soul_shop autohunt_time antiexp_ring seon_pyeong flea_market temple_of_the_ochao razador_dungeon nemere_dungeon goblin_skarbow biblioteka_wiedzy map_warp dungeon_panel wzgorze_wukonga ruiny_skorpiona starozytna_dzungla; do \
      [ -f "/quest-add/$q.quest" ] || continue; \
      sed 's/\r$//' "/quest-add/$q.quest" > "/tmp/qc-work/$q.quest"; \
      ( cd /tmp/qc-work && /src/Server/game/src/quest/qc "$q.quest" ); \
      test -s "/tmp/qc-work/object/state/$q" \
        || { echo "FATAL: qc produced no state file for $q"; find /tmp/qc-work/object | head -50; exit 1; }; \
      cp -a "/tmp/qc-work/$q.quest" "$out/$q.quest"; \
    done; \
    test -d /tmp/qc-work/object/notarget/login \
      || { echo "FATAL: qc produced no login handler for web_admin"; exit 1; }; \
    cp -a /tmp/qc-work/object "$out/object"; \
    chmod -R a+rX "$out"; \
    echo "quest objects:"; find "$out/object" -type f | wc -l

# -----------------------------------------------------------------------------
FROM ${BASE_IMAGE} AS runtime

ARG APT_MIRROR
ENV DEBIAN_FRONTEND=noninteractive

RUN if [ -n "${APT_MIRROR}" ]; then \
      sed -i -e "s|http://archive.ubuntu.com/ubuntu/|${APT_MIRROR}|g" \
             -e "s|http://security.ubuntu.com/ubuntu/|${APT_MIRROR}|g" \
             /etc/apt/sources.list.d/ubuntu.sources; \
    fi

# What `ldd` resolves on the two binaries: libstdc++, libgcc_s, libc, and -
# because the package's Makefiles link them dynamically (-lz, -lmd) - zlib and
# libmd. tzdata so the logs carry the operator's clock.
# Check-Date off, as in the panel: a Windows clock running behind makes apt
# refuse every Release file as "not valid yet" (Xewi, 19 September). Only this
# stage and the panel carry it - the deps stage is cached on every install that
# has built once, and a changed line there would rebuild every library.
RUN dpkg --add-architecture i386 \
 && apt-get -o Acquire::Check-Date=false update \
 && apt-get install -y --no-install-recommends \
        libc6:i386 libstdc++6:i386 libgcc-s1:i386 zlib1g:i386 libmd0:i386 \
        tzdata ca-certificates \
        mariadb-client \
 && rm -rf /var/lib/apt/lists/*

# Non-root: the game parses untrusted network input and needs no privilege
# beyond binding ports above 1024. The entrypoint starts as root only long
# enough to fix ownership of the state volume, then drops to this account.
RUN groupadd -r -g 2000 metin2 && useradd -r -u 2000 -g metin2 -d /opt/metin2 -s /usr/sbin/nologin metin2
# The rate spool is shared with the panel container; a FIXED gid in both
# images is what makes one volume usable from both.
RUN groupadd -r -g 2050 m2spool && usermod -a -G m2spool metin2

# ---- static half: never written at runtime, replaced by rebuilding the image
#      share/bin      the two binaries this build produced
#      share/CMD      the command permission table (a file, not a directory)
#      share/data     mob spawns, dungeons, pc tables
#      share/locale   quest objects, maps, translations (locale/poland)
#      share/package  (empty, kept because every core symlinks it)
#  There is no share/conf: the protos live in the database (PROTO_FROM_DB).
COPY --from=builder /out/game /opt/metin2/share/bin/game
COPY --from=builder /out/db   /opt/metin2/share/bin/db
COPY src/serverfiles/share/CMD     /opt/metin2/share/CMD
COPY src/serverfiles/share/data    /opt/metin2/share/data
COPY src/serverfiles/share/locale  /opt/metin2/share/locale
COPY src/serverfiles/share/package /opt/metin2/share/package

# What the r40250 image adds to its share, for the poland locale here
# (port/shareify.py renders the three files and this step):
#  * mob_drop_item.m3.append.txt - the level-30 weapon ladder's kill groups on
#    the guild map's cursed wolves;
#  * special_item_group.moonlight.txt - the Moonlight chest (50011) the bots
#    open, replacing the stock block (the reader keeps the first group per
#    vnum, so the stock one is cut out first);
#  * special_item_group.moonlight.custom.txt, when the operator made one -
#    their Moonlight chest (50011) instead of ours. No package, update or
#    launcher writes it, so an edit there survives them all (Piciu713,
#    26 September: an edit of the shipped file came back as the 16.09 one);
#  * special_item_group.starter.txt - the starter chest chain the playerbot
#    seed hands out and the package has no group for.
#  * special_item_group.cors.txt - the higher Cor Draconis grades (50256-50259)
#    open into dragon stones of their own grade (MT2009_PLUS_COR_GRADES_V1), and
#    so does the newer series 51501-51646 (its grade ranges, colours, "+" and
#    chests), which the stock file has no group for;
#  * special_item_group.goblin.txt - the Goblin board's money pouch (50023),
#    1 000 000 - 10 000 000 yang instead of the stock 10 000 - 50 000.
#  * special_item_group.dungeons.txt - the Razador (50270) and Nemere (50271)
#    boss chests the two dungeons' quests hand out (MT2009_PLUS_BOSS_CHESTS_V1).
COPY special_item_group.moonlight.custom*.txt mob_drop_item.m3.append.txt special_item_group.moonlight.txt special_item_group.starter.txt special_item_group.cors.txt special_item_group.dungeons.txt special_item_group.goblin.txt /tmp/share-add/
RUN set -eu; L=/opt/metin2/share/locale/poland \
 && for f in /tmp/share-add/*.txt; do sed -i '1s/^\xEF\xBB\xBF//; s/\r$//; s/$/\r/' "$f"; done \
 && m=/tmp/share-add/special_item_group.moonlight.txt; c=/tmp/share-add/special_item_group.moonlight.custom.txt \
 && if [ -s "$c" ]; then grep -q -E '^[[:blank:]]*Vnum[[:blank:]]+50011[[:space:]]*$' "$c" || { echo "FATAL: $c defines no Vnum 50011" >&2; exit 1; }; m="$c"; echo "share: the operator's Moonlight chest"; fi \
 && cat /tmp/share-add/mob_drop_item.m3.append.txt >> "$L/mob_drop_item.txt" \
 && f="$L/special_item_group.txt" \
 && awk 'BEGIN{keep=1} /^Group/{blk=""; keep=1} {blk=blk $0 "\n"} /Vnum[ \t]+(50011|50187|50212|50213|50188|50189|50190|50191|50192|50193|50194|50195|50196|50186|50254|50256|50257|50258|50259|50270|50271|50023|515[0-9][0-9]|516[0-4][0-9])([^0-9]|$)/{keep=0} /^}/{ if (keep) printf "%s", blk; blk=""; keep=1 }' "$f" > "$f.new" \
 && cat "$m" /tmp/share-add/special_item_group.starter.txt /tmp/share-add/special_item_group.cors.txt /tmp/share-add/special_item_group.dungeons.txt /tmp/share-add/special_item_group.goblin.txt >> "$f.new" \
 && test "$(grep -c -E '^[[:blank:]]*Vnum[[:blank:]]+(50270|50271)([^0-9]|$)' "$f.new")" = 2 \
 && test "$(grep -c -E '^[[:blank:]]*Vnum[[:blank:]]+(515[0-9][0-9]|516[0-4][0-9])([^0-9]|$)' "$f.new")" = 146 \
 && test "$(grep -c -E '^[[:blank:]]*Vnum[[:blank:]]+50023([^0-9]|$)' "$f.new")" = 1 \
 && mv "$f.new" "$f" \
 && rm -rf /tmp/share-add \
 && echo "share: moonlight + starter + boss chests, M3 drops appended"

# The alchemy bonuses (server-patches/dragonsoulbalance): the package's
# BasicApplys and AdditionalApplys groups of dragon_soul_table.txt give way to
# the MT2009 Plus balance - no elemental resistance, a race bonus instead of
# the elemental one, the Amethyst's Sungma bonuses gone, the strong ones
# smaller. The engine half (the apply names the table may use) is
# dragon_soul_table.cpp, MT2009_PLUS_DS_APPLYS_V1.
COPY dragon_soul_applys.mt2009plus.txt /tmp/dragon_soul_applys.txt
RUN set -eu; f=/opt/metin2/share/locale/poland/dragon_soul_table.txt \
 && sed -i 's/\r$//; s/$/\r/' /tmp/dragon_soul_applys.txt \
 && awk -v add=/tmp/dragon_soul_applys.txt 'BEGIN{skip=0} /^Group[ \t]+BasicApplys/{ while ((getline l < add) > 0) print l; skip=1; next } skip && /^Group[ \t]+ApplyNumSettings/{ skip=0 } !skip{ print }' "$f" > "$f.new" \
 && grep -q 'ATTBONUS_MILGYO' "$f.new" && grep -q '^Group[[:blank:]]*ApplyNumSettings' "$f.new" \
 && ! grep -Eq '^[^#]*(RESIST_(ICE|FIRE|WIND|EARTH|ELEC|DARK)|ENCHANT_|SUNGMA_)' "$f.new" \
 && mv "$f.new" "$f" && rm -f /tmp/dragon_soul_applys.txt \
 && echo "share: MT2009 Plus alchemy bonuses"

# Maska Sabaha goes with the Hwang curse (playerbotify apply_hwang_curse_removed,
# port/shareify.py renders this step): its drop lines and the Hwang loot box roll
# it at nothing - a group stops reading at the first index it lacks, so a line is
# zeroed, not deleted - and the temple's introduction hands none out.
RUN set -eu; L=/opt/metin2/share/locale/poland \
 && sed -i -E 's/^([[:blank:]]*[0-9]+[[:blank:]]+7273[15][[:blank:]]+[0-9]+[[:blank:]]+)[0-9.]+/\10/' "$L/mob_drop_item.txt" \
 && sed -i -E '/^Group[[:blank:]]+LootBox_Hwang/,/^}/ s/^([[:blank:]]*[0-9]+[[:blank:]]+7273[15][[:blank:]]+[0-9]+[[:blank:]]+)[0-9.]+/\10/' "$L/special_item_group.txt" \
 && sed -i '/^reward_data\.hwang_introduction/,/^}/ { /{72731, 1},/d }' "$L/quest/libs/other/reward_data.lua" \
 && ! grep -E '^[[:blank:]]*[0-9]+[[:blank:]]+7273[15][[:blank:]]+[0-9]+[[:blank:]]+[1-9]' "$L/mob_drop_item.txt" \
 && ! grep -q '{72731, 1}' "$L/quest/libs/other/reward_data.lua" \
 && echo "share: Maska Sabaha removed"

# Costumes, mounts, pets and weapon skins come from the ItemShop only
# (operator, 26 September 2026: players found mounts in Metin stones and
# chests). The world's item table gave two vnums of the old drop lines to
# ItemShop mounts - 71282 (Ekstraktor Skaz, gone) is the Niebieski Oldtimer
# and 71294 (the ItemShop's Rada Pustelnika) the Feniks Zmierzchu - so every
# kill, boss chest, mission book and fishing catch that rolled those rolled a
# mount. 71294 becomes the Rada Pustelnika that exists (71094); 71282 and the
# Pieczec Dzika Wojennego (71125) of the Tajemnicza Skrzynia roll at nothing -
# a group stops reading at the first index it lacks, so a line is zeroed, not
# deleted; the fishing table's row is commented out.
RUN set -eu; L=/opt/metin2/share/locale/poland \
 && for f in "$L/mob_drop_item.txt" "$L/special_item_group.txt"; do \
      sed -i -E 's/^([[:blank:]]*[0-9]+[[:blank:]]+)71294([[:blank:]])/\171094\2/' "$f"; \
      sed -i -E 's/^([[:blank:]]*[0-9]+[[:blank:]]+(71282|71125)[[:blank:]]+[0-9]+[[:blank:]]+)[0-9.]+/\10/' "$f"; \
      if grep -Eq '^[[:blank:]]*[0-9]+[[:blank:]]+71294[[:blank:]]' "$f" || \
         grep -Eq '^[[:blank:]]*[0-9]+[[:blank:]]+(71282|71125)[[:blank:]]+[0-9]+[[:blank:]]+[0-9.]*[1-9]' "$f"; then \
        echo "share: a mount still drops in $f" >&2; exit 1; \
      fi; \
    done \
 && F="$L/quest/libs/fishing/fishing_drop.lua" \
 && sed -i -E 's/^([[:blank:]]*)(71282,[[:blank:]]*1,)/\1-- \2/' "$F" \
 && ! grep -Eq '^[[:blank:]]*71282,' "$F" \
 && echo "share: costumes and mounts from the ItemShop only"

# MT2009_PLUS_GOBLIN_V1: Treasure Island (map 419) of the Treasure Hunt event
# (playerbot_goblin.h) - the map from the "Official Treasure Hunt System"
# archive, its line in the map index, and the Treasure Goblin's and the
# Giant Treasure Chest's motions (treasure_hunt/README.txt). Which core hosts
# it is m2-render-config's (MAPS_game2).
COPY treasure_hunt/ /tmp/treasure_hunt/
RUN set -eu; L=/opt/metin2/share/locale/poland; D=/opt/metin2/share/data/monster \
 && rm -rf "$L/map/metin2_map_treasure_hunt" \
 && cp -a /tmp/treasure_hunt/map/metin2_map_treasure_hunt "$L/map/metin2_map_treasure_hunt" \
 && LC_ALL=C sed -i '/^419[[:blank:]]/d' "$L/map/index" \
 && if [ -n "$(tail -c 1 "$L/map/index")" ]; then printf '\r\n' >> "$L/map/index"; fi \
 && printf '419\tmetin2_map_treasure_hunt\r\n' >> "$L/map/index" \
 && for m in treasure_hunt_goblin treasure_hunt_box; do rm -rf "$D/$m"; cp -a "/tmp/treasure_hunt/monster/$m" "$D/$m"; done \
 && chmod -R a+rX "$L/map/metin2_map_treasure_hunt" "$D/treasure_hunt_goblin" "$D/treasure_hunt_box" \
 && rm -rf /tmp/treasure_hunt \
 && test -s "$L/map/metin2_map_treasure_hunt/server_attr" \
 && echo "share: Treasure Island (419) and the goblin's motions"

# The quest texts name this world, not Metin2009 (port/shareify.py renders this
# step): the substitution l0st3k's translate.lua of 15 September makes, on both
# copies of the file.
RUN set -eu; L=/opt/metin2/share/locale/poland \
 && for f in "$L/translate.lua" "$L/quest/libs/translate/translate.lua"; do \
      [ -f "$f" ] || continue; \
      LC_ALL=C sed -i 's/Metin2009/Metin2 SinglePlayer/g' "$f"; \
      if LC_ALL=C grep -q 'Metin2009' "$f"; then echo "share: Metin2009 left in $f" >&2; exit 1; fi; \
    done \
 && echo "share: Metin2 SinglePlayer in the quest texts"

# The world's difficulty (port/shareify.py renders this step): m2_difficulty.lua
# is dofile'd after the quest libraries and puts the Biologist's wait and the
# stable keeper's waits on event flags the migrator writes from M2_DIFFICULTY.
# It replaces the 2.0.55 sed that made collect_data.is_research_in_progress
# answer false for everybody (namiot_, 15 September): that is what "easy" is.
COPY quest/m2_difficulty.lua /opt/metin2/share/locale/poland/quest/libs/other/m2_difficulty.lua
RUN set -eu; L=/opt/metin2/share/locale/poland/quest/libs/other/_otherModuleLoader.lua \
 && LC_ALL=C grep -q 'other/collect_data.lua' "$L" \
 && LC_ALL=C sed -i 's/\r$//' "$L" \
 && printf '\ndofile( LIBDIR .. "other/m2_difficulty.lua")\n' >> "$L" \
 && LC_ALL=C grep -q 'other/m2_difficulty.lua' "$L" \
 && test -s /opt/metin2/share/locale/poland/quest/libs/other/m2_difficulty.lua \
 && echo "share: difficulty hooked into the quest libraries"

# MT2009_PLUS_EXCHANGE_CHANCE_V1: the NPC exchanges' chances follow the
# world's difficulty - soul stones to Magiczny Pyl, skill books to Pergamin,
# upgrade items to Materialy Rzemieslnicze. The
# package keeps its chances in a table local to item_exchange.lua and reads
# it twice: the window's chance field (item_exchange.open's cmdchat, which the
# client prints as it is) and the roll of each piece (item_exchange.exchange).
# Both reads go through m2_difficulty.exchange_chance (m2_difficulty.lua,
# loaded after the crafting libraries and asked only at run time), so the
# window shows the chance that is rolled; the package's number is what it
# answers when nothing else applies.
RUN set -eu; F=/opt/metin2/share/locale/poland/quest/libs/crafting/item_exchange.lua \
 && LC_ALL=C grep -q 'if number(1,100) <= exchange_data.chance then' "$F" \
 && LC_ALL=C grep -q 'exchange_data.price, exchange_data.chance, vnum))' "$F" \
 && LC_ALL=C sed -i \
      -e 's/^\([[:blank:]]*\)local success_count = 0/\1local m2_exchange_percent = m2_difficulty.exchange_chance(exchange_vnum, exchange_data.chance)\n\1local success_count = 0/' \
      -e 's/if number(1,100) <= exchange_data\.chance then/if number(1,100) <= m2_exchange_percent then/' \
      -e 's/exchange_data\.price, exchange_data\.chance, vnum))/exchange_data.price, m2_difficulty.exchange_chance(vnum, exchange_data.chance), vnum))/' \
      "$F" \
 && test "$(LC_ALL=C grep -c 'm2_difficulty.exchange_chance(' "$F")" = 2 \
 && LC_ALL=C grep -q 'if number(1,100) <= m2_exchange_percent then' "$F" \
 && ! LC_ALL=C grep -q '<= exchange_data.chance' "$F" \
 && LC_ALL=C grep -q 'm2_difficulty.exchange_chance = function' /opt/metin2/share/locale/poland/quest/libs/other/m2_difficulty.lua \
 && echo "share: the NPC exchanges' chances follow the difficulty"

# The monkey curse (port/shareify.py renders this step): the package's
# monkey_curse quest turned anybody 55 minutes into a Monkey Dungeon (35 on
# 108, 25 on 109) into a monkey and warped them out, bots included. Its event
# handlers go; its state table stays for the flags characters already carry.
RUN set -eu; O=/opt/metin2/share/locale/poland/quest/object \
 && n=$(find "$O" -path "$O/state" -prune -o -type f -name 'monkey_curse.*' -print | wc -l) \
 && find "$O" -path "$O/state" -prune -o -type f -name 'monkey_curse.*' -exec rm -f {} + \
 && rm -rf "$O/monkey_curse" \
 && ! find "$O" -path "$O/state" -prune -o -name 'monkey_curse*' -print | grep -q . \
 && echo "share: monkey curse removed ($n handlers)"

# MT2009_PLUS_EASTER_METIN_CAP_V1: the package compiles three generations of
# the Easter event - event_easter, event_easter_2012 and event_easter_2013 -
# and all three answer the same easter_drop/easter_rabbit flags and the same
# kill of an Easter metin (8041-8050): one kill put two or three new metins
# down (_2013 on the killer's map, the other two on a map of the killer's
# kingdom and level), paid out a basket twice and an egg besides, and _2013
# put twelve more on a map at every login or map change while the Bunny was
# on - "Metin na Metinie" on every map (a player, 29 September). event_easter
# is the event the panel switches; the two older ones lose their handlers and
# keep their state tables for the flags characters already carry. The
# engine caps what is left per map (regen.cpp, EasterRegenMaySpawn).
RUN set -eu; O=/opt/metin2/share/locale/poland/quest/object \
 && n=$(find "$O" -path "$O/state" -prune -o -type f \( -name 'event_easter_2012.*' -o -name 'event_easter_2013.*' \) -print | wc -l) \
 && find "$O" -path "$O/state" -prune -o -type f \( -name 'event_easter_2012.*' -o -name 'event_easter_2013.*' \) -exec rm -f {} + \
 && ! find "$O" -path "$O/state" -prune -o -type f \( -name 'event_easter_2012.*' -o -name 'event_easter_2013.*' \) -print | grep -q . \
 && test -f "$O/8043/kill/event_easter.start" \
 && echo "share: old Easter event copies removed ($n handlers)"

# The Grotto of Exile's entrance (port/shareify.py renders this step): the
# package commented out Koe-Pung in Orc Valley's bottom-left corner (20093,
# cell 282,1455) - the Dragon Order's guard, who lets a character of 75
# through for a Bloodstone (quest/check_trans_ticket.quest, compiled above) -
# with a free warp on the same cell (10077) and Seon-Pyeong beside them
# (20091). Koe-Pung and Seon-Pyeong come back and the warp stays commented,
# as the game has it: "wejscie do groty chyba powinno wymagac jakiegos
# klucza" (the operator, 26 September; 2.2.21 had opened the warp). The Bloodstone
# is Seon-Hae's daily quest (quest/heavens_cave_keyquest.quest), and the
# package stands Seon-Hae (20095) in Jinno's first village alone: Yongan and
# Joan get him beside their herbalist (20018), on cells measured free and on
# the main ground of their server_attr, far outside the market's ring. A bot
# is moved into the grotto server-side and asks nobody.
RUN set -eu; L=/opt/metin2/share/locale/poland/map; N=$L/map_n_threeway/npc.txt \
 && sed -i -E 's#^(m[[:blank:]]+282[[:blank:]]+1455[[:blank:]].*[[:blank:]]10077[[:space:]]*)$#//\1#; s#^//(m[[:blank:]]+282[[:blank:]]+1455[[:blank:]].*[[:blank:]]20093[[:space:]]*)$#\1#; s#^//(m[[:blank:]]+295[[:blank:]]+1442[[:blank:]].*[[:blank:]]20091[[:space:]]*)$#\1#' "$N" \
 && ! grep -q -E '^m[[:blank:]]+282[[:blank:]]+1455[[:blank:]].*[[:blank:]]10077' "$N" \
 && grep -q -E '^m[[:blank:]]+282[[:blank:]]+1455[[:blank:]].*[[:blank:]]20093' "$N" \
 && grep -q -E '^m[[:blank:]]+295[[:blank:]]+1442[[:blank:]].*[[:blank:]]20091' "$N" \
 && for p in "metin2_map_a1 698 652" "metin2_map_b1 677 585"; do \
      set -- $p; F="$L/$1/npc.txt"; \
      [ -z "$(tail -c 1 "$F")" ] || printf '\n' >> "$F"; \
      grep -q -E '[[:blank:]]20095[[:space:]]*$' "$F" || printf 'm\t%s\t%s\t0\t0\t0\t5\t1m\t100\t1\t20095\n' "$2" "$3" >> "$F"; \
      grep -q -E "^m[[:blank:]]+$2[[:blank:]]+$3[[:blank:]].*[[:blank:]]20095" "$F"; \
    done \
 && echo "share: Grotto of Exile entrance restored in Orc Valley"

# The Devil's Catacomb (port/shareify.py renders this step; the quest itself is
# compiled from quest/devilcatacomb_zone.quest above): its Guardian (20367)
# comes back to Hwang Temple, and the Reaper (1093) drops the Reaper's token -
# the Dried Head (30319) the third floor takes one of from every member - as
# r40250's Reaper does. It goes into his existing limit group, flat and certain
# like his casket, because a second limit group for one monster is ignored.
#
# MT2009 Plus (26 September 2026): the Guardian stays commented out. The
# MT2009 Plus client has no metin2_map_devilcatacomb (only the 7x7
# metin2_map_devilscatacomb, and this server's map is 8x8), and a player
# warped onto a map the client cannot load is thrown out at every login
# after. The token drop stays; the Guardian comes back with the map.
RUN set -eu; L=/opt/metin2/share/locale/poland \
 && sed -i -E '/^Group[[:blank:]]+RiperLimit/,/^}/ s/^([[:blank:]]+1[[:blank:]]+50082[[:blank:]]+1[[:blank:]]+100.*)$/&\n\t2\t30319\t1\t100\r/' "$L/mob_drop_item.txt" \
 && awk '/^Group[[:blank:]]+RiperLimit/{f=1} f&&/^}/{f=0} f&&/[[:blank:]]30319[[:blank:]]/{n++} END{exit n==1?0:1}' "$L/mob_drop_item.txt" \
 && echo "share: the Reaper's token (Devil's Catacomb Guardian left out: no client map)"

# MT2009 Plus (27 September 2026): the guild building materials - Kamien
# Wegielny 90010, Pien 90011, Dykta 90012 - drop ten times as often from the
# ordinary monsters that carry them (Mount Sohan's ice, the Fireland's fire,
# the Hwang Temple's frogs: 0.09-0.4 in mob_drop_item.txt, one piece in
# roughly a thousand to four thousand kills); the bosses' chances (40-300)
# stay. The guilds build with them (playerbot_guild_land.h) and the guild
# materials droppers farm them. Exactly fourteen lines, or the build stops.
RUN set -eu; L=/opt/metin2/share/locale/poland \
 && awk -F '\t' 'BEGIN{OFS="\t"} ($3 ~ /^9001[012]$/ && $5+0 > 0 && $5+0 < 1) {$5 = $5 * 10; n++} {print} END{print n > "/tmp/guildmat.count"}' "$L/mob_drop_item.txt" > /tmp/mob_drop_item.txt \
 && [ "$(cat /tmp/guildmat.count)" = 14 ] \
 && mv /tmp/mob_drop_item.txt "$L/mob_drop_item.txt" && rm -f /tmp/guildmat.count \
 && echo "share: guild building materials x10 from ordinary monsters (14 lines)"

# The costume sets (server-patches/playerqol, MT2009_PLUS_COSTUME_SET_V1): a
# body and a hair of one line give +800 HP and +15 attack value. The game
# reads it from its locale at the first equip; the client's tooltip has the
# same lists in costume_sets.py.
COPY costume_sets.txt /opt/metin2/share/locale/poland/costume_sets.txt

# Seon-Pyeong's recipes (port/shareify.py renders cube.seon_pyeong.txt and this
# step; the quest that opens his window is quest/seon_pyeong.quest above): a +9
# piece of level 65-66 into its level-80 successor. The package's cube.txt has
# no recipe for him, and Cube_open refuses an NPC with none.
COPY cube.seon_pyeong.txt /tmp/cube-add/
RUN set -eu; C=/opt/metin2/share/locale/poland/cube.txt; A=/tmp/cube-add/cube.seon_pyeong.txt \
 && ! grep -q -E '^npc[[:blank:]]+20091[[:space:]]*$' "$C" \
 && n=$(grep -c -E '^npc[[:blank:]]+20091[[:space:]]*$' "$A") \
 && sed -i 's/\r$//; s/$/\r/' "$A" \
 && { [ -z "$(tail -c 1 "$C")" ] || printf '\r\n' >> "$C"; } \
 && cat "$A" >> "$C" \
 && rm -rf /tmp/cube-add \
 && test "$(grep -c -E '^npc[[:blank:]]+20091[[:space:]]*$' "$C")" -eq "$n" \
 && echo "share: Seon-Pyeong's recipes appended to cube.txt ($n)"

# MT2009_PLUS_OCHAO_V1 (share): Swiatynia Ochao, the Temple of Ochao of the
# "Version 16.2" package - map 209, metin2_map_mt_th_dungeon_01, an open map
# for level 95 and more, hosted on "first" (m2-render-config). Its server
# files as the package ships them, its groups (group.ochao.txt) and drops
# (mob_drop_item.ochao.txt: the package's vnums mapped to this world's), the
# En-Tai Guardian's motions (data/monster/trent_officer) and the entrance NPC,
# Straznik Swiatyni (20426): beside Koe-Pung in Orc Valley, on a cell measured
# free on its server_attr, and where the package stands him in Dawnmistwood.
# The Guardian's eleven rooms are playerbot_ochao.h; the NPCs' talk is
# quest/temple_of_the_ochao.quest (compiled above).
COPY ochao/ /tmp/ochao-add/
RUN set -eu; L=/opt/metin2/share/locale/poland; A=/tmp/ochao-add \
 && rm -rf "$L/map/metin2_map_mt_th_dungeon_01" && cp -a "$A/map/metin2_map_mt_th_dungeon_01" "$L/map/" \
 && { [ -z "$(tail -c 1 "$L/map/index")" ] || printf '\n' >> "$L/map/index"; } \
 && sed -i -E '/^209[[:blank:]]/d' "$L/map/index" && printf '209 metin2_map_mt_th_dungeon_01\n' >> "$L/map/index" \
 && test "$(grep -c -E '^209[[:blank:]]+metin2_map_mt_th_dungeon_01' "$L/map/index")" -eq 1 \
 && mkdir -p /opt/metin2/share/data/monster && rm -rf /opt/metin2/share/data/monster/trent_officer \
 && cp -a "$A/data/monster/trent_officer" /opt/metin2/share/data/monster/ \
 && ! grep -q -E '^Group[[:blank:]]+OchaoMonsters1[[:space:]]*$' "$L/group.txt" \
 && { [ -z "$(tail -c 1 "$L/group.txt")" ] || printf '\n' >> "$L/group.txt"; } \
 && sed 's/\r$//' "$A/group.ochao.txt" >> "$L/group.txt" \
 && sed -i 's/\r$//; s/$/\r/' "$A/mob_drop_item.ochao.txt" \
 && { [ -z "$(tail -c 1 "$L/mob_drop_item.txt")" ] || printf '\r\n' >> "$L/mob_drop_item.txt"; } \
 && cat "$A/mob_drop_item.ochao.txt" >> "$L/mob_drop_item.txt" \
 && for p in "map_n_threeway 289 1448 5" "metin2_map_dawnmistwood 1004 700 1"; do \
      set -- $p; F="$L/map/$1/npc.txt"; \
      [ -z "$(tail -c 1 "$F")" ] || printf '\n' >> "$F"; \
      grep -q -E '[[:blank:]]20426[[:space:]]*$' "$F" || printf 'm\t%s\t%s\t0\t0\t0\t%s\t1m\t100\t1\t20426\n' "$2" "$3" "$4" >> "$F"; \
      grep -q -E "^m[[:blank:]]+$2[[:blank:]]+$3[[:blank:]].*[[:blank:]]20426" "$F"; \
    done \
 && S="$L/settings.lua" \
 && { LC_ALL=C grep -a -q 'add_goto_info("ochao"' "$S" || printf 'add_goto_info("ochao", 0, 209, 8537, 14164)\r\n' >> "$S"; } \
 && rm -rf "$A" \
 && echo "share: Swiatynia Ochao (map 209, groups, drops, Straznik Swiatyni, /goto ochao)"

# MT2009_PLUS_DUNGEONS_V1: the Razador and Nemere dungeons (quest/razador_dungeon.quest and
# quest/nemere_dungeon.quest, compiled above; their monsters, keys and names are apply.sh's
# MT2009_PLUS_DUNGEONS_V1). Everything they need from the share goes in here:
#  * metin2_map_n_snow_dungeon_01 (index 352, 4x3 at 512000 153600 - where the client's
#    atlasinfo puts it) with its Setting/Town and a server_attr made by us (every cell
#    walkable: the package never shipped the map, and the client's own attr decides where
#    a player walks);
#  * MT2009_PLUS_DUNGEON_DROP_V1: metin2_map_n_flame_dungeon_01's server_attr (351) made from the
#    GF client's attr.atr (the one the 2.0.28 client walks on) with the package's no-PK cells
#    kept - the package's own blocked the bridges' middles, so a player could not cross them
#    (the owner, 29 September); 352's already is the client's;
#  * the regen files of the two dungeons (data/dungeon/razador_mt: every fourth line of the
#    package's flame_dungeon files, a party of level 55 and not the 100+ the package planned;
#    data/dungeon/nemere_mt: the ice rooms, the package's ice_dungeon groups of groups);
#  * the public copies of 351 and 352 are the lobbies (both maps on game2, see
#    m2-render-config): nothing spawns there but the NPC that opens the instance - the
#    Am-heh statue (20385) on 351, the stone lion (20397) on 352; the package's own
#    npc.txt/regen.txt filled 351 with Razador's hourly group;
#  * the two guards: the Fire Land's (20394) on map 62 at the archive's exit cell (91,916),
#    the Ice Land's (20395) on Mount Sohan (61) at (738,119); both cells measured free;
#  * the Nemere dungeon's drops, mirrored from the package's Razador groups;
#  * MT2009_PLUS_NEMERE_GOLEM_MAG_V1: the Lodowy Golem Mag's (6107, icegolem_magician) run
#    motion - the package's run.msa moves it BACKWARDS (Accumulation y +15.84, every other
#    mob's is negative), so the server's move speed for it was negative and every chase got
#    a move duration of ~4e9 ms (a negative int in a DWORD): its server position no longer
#    matched the one the client drew, the player's hits landed out of range (no HP taken)
#    and the player was pulled across the room. The client's run.msa
#    (gf_nemere, the same run.GR2) has -310.21; the server takes the same number.
# Older packages' shares (a VPS on 2.14.0, 29 September) list no 351 in map/index and
# already carry a Mob 6191 group: the index line is added like 352's, and the awk below
# drops any 6091/6191 group but the chest previews, so only our own groups guard against
# a second append.
COPY dungeons/ /tmp/dungeons-add/
RUN set -eu; L=/opt/metin2/share/locale/poland; A=/tmp/dungeons-add \
 && find "$A" -type f -name '*.txt' -exec sed -i '1s/^\xEF\xBB\xBF//; s/\r$//; s/$/\r/' {} + \
 && cp -a "$A/map/metin2_map_n_snow_dungeon_01" "$L/map/" \
 && cp "$A/map/metin2_map_n_flame_dungeon_01/server_attr" "$L/map/metin2_map_n_flame_dungeon_01/server_attr" \
 && { [ -z "$(tail -c 1 "$L/map/index")" ] || printf '\n' >> "$L/map/index"; } \
 && { grep -q -E '^352[[:blank:]]' "$L/map/index" || printf '352 metin2_map_n_snow_dungeon_01\n' >> "$L/map/index"; } \
 && { grep -q -E '^351[[:blank:]]' "$L/map/index" || printf '351 metin2_map_n_flame_dungeon_01\n' >> "$L/map/index"; } \
 && test "$(grep -c -E '^352[[:blank:]]+metin2_map_n_snow_dungeon_01[[:space:]]*$' "$L/map/index")" = 1 \
 && test "$(grep -c -E '^351[[:blank:]]+metin2_map_n_flame_dungeon_01[[:space:]]*$' "$L/map/index")" = 1 \
 && mkdir -p /opt/metin2/share/data/dungeon \
 && cp -a "$A/data/dungeon/razador_mt" "$A/data/dungeon/nemere_mt" /opt/metin2/share/data/dungeon/ \
 && M=/opt/metin2/share/data/monster/icegolem_magician/run.msa \
 && sed -i -E 's/^(Accumulation[[:blank:]]+)0\.00[[:blank:]]+15\.84[[:blank:]]+0\.00/\10.00\t-310.21\t0.00/' "$M" \
 && grep -q -E '^Accumulation[[:blank:]]+0\.00[[:blank:]]+-310\.21[[:blank:]]+0\.00' "$M" \
 && for f in regen.txt npc.txt boss.txt stone.txt; do : > "$L/map/metin2_map_n_flame_dungeon_01/$f"; done \
 && printf 'm\t348\t578\t0\t0\t0\t0\t60s\t100\t1\t20385\r\n' > "$L/map/metin2_map_n_flame_dungeon_01/npc.txt" \
 && for p in "metin2_map_n_flame_01 95 910 20394" "map_n_snowm_01 742 114 20395"; do \
      set -- $p; F="$L/map/$1/npc.txt"; \
      [ -z "$(tail -c 1 "$F")" ] || printf '\n' >> "$F"; \
      grep -q -E "[[:blank:]]$4[[:space:]]*\$" "$F" || printf 'm\t%s\t%s\t0\t0\t0\t0\t60s\t100\t1\t%s\n' "$2" "$3" "$4" >> "$F"; \
      grep -q -E "^m[[:blank:]]+$2[[:blank:]]+$3[[:blank:]].*[[:blank:]]$4" "$F"; \
    done \
 && ! grep -q -E '^Group[[:blank:]]+MT2009_nemere_' "$L/mob_drop_item.txt" \
 && { [ -z "$(tail -c 1 "$L/mob_drop_item.txt")" ] || printf '\r\n' >> "$L/mob_drop_item.txt"; } \
 && cat "$A/mob_drop_item.dungeons.append.txt" >> "$L/mob_drop_item.txt" \
 && awk '/^Group/{inb=1; blk=""; cut=0; name=$2} \
      inb { blk = blk $0 "\n"; if ($0 ~ /^[[:blank:]]*Mob[[:blank:]]+(6091|6191)([^0-9]|$)/ && name !~ /^MT2009_chest_preview_/) cut = 1; \
            if ($0 ~ /^}/) { inb = 0; if (!cut) printf "%s", blk }; next } \
      { print }' "$L/mob_drop_item.txt" > "$L/mob_drop_item.txt.new" && mv "$L/mob_drop_item.txt.new" "$L/mob_drop_item.txt" \
 && test "$(grep -c -E '^[[:blank:]]*Mob[[:blank:]]+(6091|6191)([^0-9]|$)' "$L/mob_drop_item.txt")" = 2 \
 && test "$(grep -c -E '^Group[[:blank:]]+MT2009_chest_preview_(6091|6191)' "$L/mob_drop_item.txt")" = 2 \
 && test "$(grep -c -E '^Group[[:blank:]]+MT2009_nemere_' "$L/mob_drop_item.txt")" = 9 \
 && test "$(od -An -tx1 -j4 -N4 "$L/map/metin2_map_n_snow_dungeon_01/server_attr" | tr -d ' ')" = 0c000000 \
 && test "$(od -An -tx1 -N8 "$L/map/metin2_map_n_flame_dungeon_01/server_attr" | tr -d ' ')" = 0c0000000c000000 \
 && rm -rf "$A" \
 && echo "share: Razador (351) and Nemere (352) dungeons, their guards and the Nemere drops"

# MT2009_PLUS_AREZZO_V1 (share): the first three places from the Arezzo files (the owner has the
# rights to them), for players - the bots come later:
#  * Dolina Cyklopow: metin2_map_exp, index 360, 4x4 at 230400 281600, level 45, on game1
#    (m2-render-config), entered by the Teleporter (quest/map_warp.quest, first page);
#  * Zaczarowany Las: natural_map, 362, 4x4 at 332800 384000, level 95+, game1, entered by the
#    portal the En-Tai Guardian leaves in the Temple of Ochao (quest/temple_of_the_ochao.quest);
#  * Biblioteka Wiedzy: plechito_chamber_of_wisdom, 363, 2x2 at 947200 614400, the level-30 spider
#    dungeon on game2 (quest/biblioteka_wiedzy.quest, rooms in data/dungeon/biblioteka_wiedzy); its
#    guard, Straznik Biblioteki (20430), stands in Orc Valley beside the Temple guard (a cell measured
#    free) and in the public copy of 363 (the fallback lobby, as Razador's 351).
# MT2009_PLUS_AREZZO_MODULE_V1: the guards on the old maps (20430 here, 20423 and 20424 below) are no
# npc.txt lines any more - playerbot_arezzo.h spawns them while the module is on (M2_AREZZO, the panel)
# and removes them while it is off; these steps only make sure no npc.txt line is left behind.
# Arezzo's server_attr (cell for cell the client's attr.atr, checked) and Setting.txt; the regen is
# every other of Arezzo's spawn points with our monsters (the level-45 cyclopes 9601-9605, the
# stronger lemures 9611-9615) in groups 36001-36203 (group.arezzo.txt); Arezzo's C++ timed bosses and
# stones are boss.txt / stone.txt (Arges 9606, Polifem 9607, Metin Cienia 8009; Lemur Hrabia 3390,
# the Bodyguard 3391, 8052, 8053); a Teleporter (9012) at each open map's landing point. The drops
# (mob_drop_item.arezzo.txt), the library's chest 30773 (special_item_group.arezzo.txt), the names
# the Teleporter shows (translate.lua) and /goto cyklopy, /goto las for game masters.
COPY arezzo/ /tmp/arezzo-add/
RUN set -eu; L=/opt/metin2/share/locale/poland; A=/tmp/arezzo-add \
 && find "$A" -type f -name '*.txt' -exec sed -i '1s/^\xEF\xBB\xBF//; s/\r$//; s/$/\r/' {} + \
 && for m in metin2_map_exp natural_map plechito_chamber_of_wisdom; do rm -rf "$L/map/$m"; cp -a "$A/map/$m" "$L/map/"; done \
 && { [ -z "$(tail -c 1 "$L/map/index")" ] || printf '\n' >> "$L/map/index"; } \
 && sed -i -E '/^36[0-6][[:blank:]]/d' "$L/map/index" \
 && printf '360 metin2_map_exp\n362 natural_map\n363 plechito_chamber_of_wisdom\n' >> "$L/map/index" \
 && test "$(grep -c -E '^(360[[:blank:]]+metin2_map_exp|362[[:blank:]]+natural_map|363[[:blank:]]+plechito_chamber_of_wisdom)[[:space:]]*$' "$L/map/index")" = 3 \
 && cp "$A/dungeon_info.txt" "$L/dungeon_info.txt" \
 && test "$(grep -c -E '^dungeon[[:blank:]]' "$L/dungeon_info.txt")" -ge 8 \
    # MT2009_PLUS_DUNGEON_PANEL_V1 (share): the dungeon panel's list, locale/poland/dungeon_info.txt.
 && mkdir -p /opt/metin2/share/data/dungeon && rm -rf /opt/metin2/share/data/dungeon/biblioteka_wiedzy \
 && cp -a "$A/data/dungeon/biblioteka_wiedzy" /opt/metin2/share/data/dungeon/ \
 && ! grep -q -E '^Group[[:blank:]]+MT2009_arezzo_' "$L/group.txt" \
 && { [ -z "$(tail -c 1 "$L/group.txt")" ] || printf '\n' >> "$L/group.txt"; } \
 && sed 's/\r$//' "$A/group.arezzo.txt" >> "$L/group.txt" \
 && test "$(grep -c -E '^[[:blank:]]*Vnum[[:blank:]]+36(00[123]|20[123])[[:space:]]*$' "$L/group.txt")" = 6 \
 && ! grep -q -E '^Group[[:blank:]]+MT2009_arezzo_' "$L/mob_drop_item.txt" \
 && { [ -z "$(tail -c 1 "$L/mob_drop_item.txt")" ] || printf '\r\n' >> "$L/mob_drop_item.txt"; } \
 && cat "$A/mob_drop_item.arezzo.txt" >> "$L/mob_drop_item.txt" \
 && test "$(grep -c -E '^Group[[:blank:]]+MT2009_arezzo_' "$L/mob_drop_item.txt")" = 7 \
 && f="$L/special_item_group.txt" \
 && ! grep -q -E '^[[:blank:]]*Vnum[[:blank:]]+(30773|95073[123])([^0-9]|$)' "$f" \
 && { [ -z "$(tail -c 1 "$f")" ] || printf '\r\n' >> "$f"; } \
 && cat "$A/special_item_group.arezzo.txt" >> "$f" \
 && test "$(grep -c -E '^[[:blank:]]*Vnum[[:blank:]]+(30773|95073[123])([^0-9]|$)' "$f")" = 4 \
 && F="$L/map/map_n_threeway/npc.txt" \
 && sed -i -E '/[[:blank:]]20430[[:space:]]*$/d' "$F" \
 && ! grep -q -E '[[:blank:]]20430[[:space:]]*$' "$F" \
 && for f in "$L/translate.lua" "$L/quest/libs/translate/translate.lua"; do \
      [ -f "$f" ] || continue; \
      LC_ALL=C grep -a -q 'map_names\[360\]' "$f" && continue; \
      { [ -z "$(tail -c 1 "$f")" ] || printf '\r\n' >> "$f"; }; \
      printf '\r\n-- MT2009_PLUS_AREZZO_V1: the Arezzo places (the Teleporter lists 360)\r\ntranslate.general.map_names[360] = "Dolina Cyklop\363w"\r\ntranslate.general.map_names[362] = "Zaczarowany Las"\r\ntranslate.general.map_names[363] = "Biblioteka Wiedzy"\r\n' >> "$f"; \
    done \
 && LC_ALL=C grep -a -q 'map_names\[360\]' "$L/quest/libs/translate/translate.lua" \
 && S="$L/settings.lua" \
 && { LC_ALL=C grep -a -q 'add_goto_info("cyklopy"' "$S" || printf 'add_goto_info("cyklopy", 0, 360, 2650, 3051)\r\n' >> "$S"; } \
 && { LC_ALL=C grep -a -q 'add_goto_info("las"' "$S" || printf 'add_goto_info("las", 0, 362, 3787, 3946)\r\n' >> "$S"; } \
 && test "$(od -An -tx1 -N8 "$L/map/metin2_map_exp/server_attr" | tr -d ' ')" = 1000000010000000 \
 && test "$(od -An -tx1 -N8 "$L/map/natural_map/server_attr" | tr -d ' ')" = 1000000010000000 \
 && test "$(od -An -tx1 -N8 "$L/map/plechito_chamber_of_wisdom/server_attr" | tr -d ' ')" = 0800000008000000 \
 && rm -rf "$A" \
 && echo "share: Arezzo places - Dolina Cyklopow (360), Zaczarowany Las (362), Biblioteka Wiedzy (363)"

# MT2009_PLUS_AREZZO_V1 (share, phases 5-8): the three Arezzo dungeons and the fourth map, players only:
#  * Wzgorze Wukonga - plechito_wukong_dungeon, 364, 2x2 at 844800 537600, level 45, game2
#    (quest/wzgorze_wukonga.quest; its guard Straznik Wzgorza, 20423, in the Hwang Temple at 165,941);
#  * Ruiny Skorpiona - plechito_scorpion_dungeon, 365, 2x2 at 844800 588800, level 65 - harder than Razador
#    (quest/ruiny_skorpiona.quest; Straznik Ruin, 20424, on the Fire Land beside Razador's guard, 101,910);
#  * Starozytna Dzungla - plechito_easter2023_dungeon, 366, 3x3 at 768000 537600, level 95, the hardest
#    (quest/starozytna_dzungla.quest; Straznik Dzungli, 20425, in the Enchanted Forest - arezzo/map/natural_map);
#  * Pustkowie Faraona - metin2_map_pustynia, 361, 4x4 at 230400 384000, level 55, game1, by the Teleporter.
# The dungeons' public copies are the fallback lobbies (their guard only); their rooms are
# data/dungeon/<quest>. The Plechito monsters' motions (share/data/monster/<folder>, the .msa/.msm/motlist
# of the Arezzo share) - the server times attacks and moves by them. The desert's bosses' drops
# (mob_drop_item.arezzo2.txt), the three chests (special_item_group.arezzo2.txt), the Teleporter's names and
# /goto pustkowie for game masters. The groups are in group.arezzo.txt (the step above).
COPY arezzo/ /tmp/arezzo-p58/
RUN set -eu; L=/opt/metin2/share/locale/poland; A=/tmp/arezzo-p58 \
 && find "$A" -type f -name '*.txt' -exec sed -i '1s/^\xEF\xBB\xBF//; s/\r$//; s/$/\r/' {} + \
 && for m in metin2_map_pustynia plechito_wukong_dungeon plechito_scorpion_dungeon plechito_easter2023_dungeon; do rm -rf "$L/map/$m"; cp -a "$A/map/$m" "$L/map/"; done \
 && { [ -z "$(tail -c 1 "$L/map/index")" ] || printf '\n' >> "$L/map/index"; } \
 && sed -i -E '/^36[1456][[:blank:]]/d' "$L/map/index" \
 && printf '361 metin2_map_pustynia\n364 plechito_wukong_dungeon\n365 plechito_scorpion_dungeon\n366 plechito_easter2023_dungeon\n' >> "$L/map/index" \
 && test "$(grep -c -E '^(361[[:blank:]]+metin2_map_pustynia|364[[:blank:]]+plechito_wukong_dungeon|365[[:blank:]]+plechito_scorpion_dungeon|366[[:blank:]]+plechito_easter2023_dungeon)[[:space:]]*$' "$L/map/index")" = 4 \
 && for q in wzgorze_wukonga ruiny_skorpiona starozytna_dzungla; do rm -rf "/opt/metin2/share/data/dungeon/$q"; cp -a "$A/data/dungeon/$q" /opt/metin2/share/data/dungeon/; done \
 && mkdir -p /opt/metin2/share/data/monster \
 && for m in "$A"/data/monster/*; do rm -rf "/opt/metin2/share/data/monster/$(basename "$m")"; cp -a "$m" /opt/metin2/share/data/monster/; done \
 && test -s /opt/metin2/share/data/monster/plechi_wukong_mainboss/motlist.txt \
 && test -s /opt/metin2/share/data/monster/plechi_easter2023_bossmain/motlist.txt \
 && ! grep -q -E '^Group[[:blank:]]+MT2009_arezzo_(9675|9681)' "$L/mob_drop_item.txt" \
 && { [ -z "$(tail -c 1 "$L/mob_drop_item.txt")" ] || printf '\r\n' >> "$L/mob_drop_item.txt"; } \
 && cat "$A/mob_drop_item.arezzo2.txt" >> "$L/mob_drop_item.txt" \
 && test "$(grep -c -E '^Group[[:blank:]]+MT2009_arezzo_' "$L/mob_drop_item.txt")" = 9 \
 && f="$L/special_item_group.txt" \
 && ! grep -q -E '^[[:blank:]]*Vnum[[:blank:]]+(3077[456]|9507[456][123])([^0-9]|$)' "$f" \
 && { [ -z "$(tail -c 1 "$f")" ] || printf '\r\n' >> "$f"; } \
 && cat "$A/special_item_group.arezzo2.txt" >> "$f" \
 && test "$(grep -c -E '^[[:blank:]]*Vnum[[:blank:]]+(3077[456]|9507[456][123])([^0-9]|$)' "$f")" = 12 \
 && for p in "metin2_map_milgyo 20423" "metin2_map_n_flame_01 20424"; do \
      set -- $p; F="$L/map/$1/npc.txt"; \
      sed -i -E "/[[:blank:]]$2[[:space:]]*\$/d" "$F"; \
      ! grep -q -E "[[:blank:]]$2[[:space:]]*\$" "$F" || exit 1; \
    done \
 && grep -q -E '[[:blank:]]20425[[:space:]]*$' "$L/map/natural_map/npc.txt" \
 && for f in "$L/translate.lua" "$L/quest/libs/translate/translate.lua"; do \
      [ -f "$f" ] || continue; \
      LC_ALL=C grep -a -q 'map_names\[361\]' "$f" && continue; \
      { [ -z "$(tail -c 1 "$f")" ] || printf '\r\n' >> "$f"; }; \
      printf '\r\n-- MT2009_PLUS_AREZZO_V1 (phases 5-8)\r\ntranslate.general.map_names[361] = "Pustkowie Faraona"\r\ntranslate.general.map_names[364] = "Wzg\363rze Wukonga"\r\ntranslate.general.map_names[365] = "Ruiny Skorpiona"\r\ntranslate.general.map_names[366] = "Staro\277ytna D\277ungla"\r\n' >> "$f"; \
    done \
 && LC_ALL=C grep -a -q 'map_names\[361\]' "$L/quest/libs/translate/translate.lua" \
 && S="$L/settings.lua" \
 && { LC_ALL=C grep -a -q 'add_goto_info("pustkowie"' "$S" || printf 'add_goto_info("pustkowie", 0, 361, 2409, 3940)\r\n' >> "$S"; } \
 && test "$(od -An -tx1 -N8 "$L/map/metin2_map_pustynia/server_attr" | tr -d ' ')" = 1000000010000000 \
 && test "$(od -An -tx1 -N8 "$L/map/plechito_wukong_dungeon/server_attr" | tr -d ' ')" = 0800000008000000 \
 && test "$(od -An -tx1 -N8 "$L/map/plechito_scorpion_dungeon/server_attr" | tr -d ' ')" = 0800000008000000 \
 && test "$(od -An -tx1 -N8 "$L/map/plechito_easter2023_dungeon/server_attr" | tr -d ' ')" = 0c0000000c000000 \
 && rm -rf "$A" \
 && echo "share: Arezzo phases 5-8 - Pustkowie Faraona (361), Wzgorze Wukonga (364), Ruiny Skorpiona (365), Starozytna Dzungla (366)"
# (MT2009_PLUS_DUNGEON_DROP_V1: Razador (6091) and Nemere (6191) drop only yang - the owner,
# 29 September; their loot is the 2-5 chests per player of the quests - so every drop group
# of theirs is cut whole above but the 0% chest previews; item_manager.cpp drops nothing
# for them either.)
# The level-75 weapons, the Reaper's and Azrael's, not the Spider Baroness's
# (MT2009_PLUS_LEVEL75_DROP_V1; upstream 2.2.39, the operator's decision of
# 29 September). Her limit group (mob 2092) loses its 12 lines of
# level 75 - six families at +3 and +4 - and is numbered again: the reader
# stops at the first index a group lacks, and the drop preview lists every
# line whatever its chance. The Reaper (1093) and Azrael (2598) get the 7
# families of 75, the fan of 75 among them, in their own limit groups (a second
# one for a monster is ignored), numbered on from the last line - the Reaper's
# Dried Head above included: the Reaper +3 at 10%, +4 at 5%; Azrael
# +3 at 20%, +4 at 10%. The drop preview reads the loaded groups, so it follows.
RUN set -eu; F=/opt/metin2/share/locale/poland/mob_drop_item.txt \
 ; C='183 184 193 194 3163 3164 2173 2174 1133 1134 5123 5124' \
 ; A='1093:183:10:Zatruty_miecz+3 1093:184:5:Zatruty_miecz+4 \
      1093:193:10:Lwi_Miecz+3 1093:194:5:Lwi_Miecz+4 \
      1093:3163:10:Miecz_Zalu+3 1093:3164:5:Miecz_Zalu+4 \
      1093:2173:10:Stalowy_Luk_Kruka+3 1093:2174:5:Stalowy_Luk_Kruka+4 \
      1093:1133:10:Skrzydla_Demona_Chakr.+3 1093:1134:5:Skrzydla_Demona_Chakr.+4 \
      1093:7183:10:Wachlarz_8_Trigramow+3 1093:7184:5:Wachlarz_8_Trigramow+4 \
      1093:5123:10:Bambusowy_Dzwon+3 1093:5124:5:Bambusowy_Dzwon+4 \
      2598:183:20:Zatruty_miecz+3 2598:184:10:Zatruty_miecz+4 \
      2598:193:20:Lwi_Miecz+3 2598:194:10:Lwi_Miecz+4 \
      2598:3163:20:Miecz_Zalu+3 2598:3164:10:Miecz_Zalu+4 \
      2598:2173:20:Stalowy_Luk_Kruka+3 2598:2174:10:Stalowy_Luk_Kruka+4 \
      2598:1133:20:Skrzydla_Demona_Chakr.+3 2598:1134:10:Skrzydla_Demona_Chakr.+4 \
      2598:7183:20:Wachlarz_8_Trigramow+3 2598:7184:10:Wachlarz_8_Trigramow+4 \
      2598:5123:20:Bambusowy_Dzwon+3 2598:5124:10:Bambusowy_Dzwon+4' \
 && awk -v from=2092 -v cut="$C" -v add="$A" ' \
      function flush(   i, t, f, j, lead, mx, k, nm) { \
        if (typ == "limit" && mob == from) { \
          hits++; j = 0; \
          for (i = 1; i <= nb; i++) { \
            t = B[i]; sub(/\r$/, "", t); \
            if (split(t, f) >= 2 && f[1] ~ /^[0-9]+$/) { \
              if (f[2] in C) { gone++; continue; } \
              match(B[i], /^[ \t]*[0-9]+/); lead = substr(B[i], 1, RLENGTH); sub(/[0-9]+$/, "", lead); \
              print lead (++j) substr(B[i], RLENGTH + 1); \
            } else print B[i]; \
          } \
        } else if (typ == "limit" && (mob in NA)) { \
          got[mob]++; mx = 0; \
          for (i = 1; i <= nb; i++) { \
            t = B[i]; sub(/\r$/, "", t); \
            if (split(t, f) >= 2 && f[1] ~ /^[0-9]+$/) { \
              if (f[1] + 0 > mx) mx = f[1] + 0; \
              for (k = 1; k <= NA[mob]; k++) if (AV[mob, k] == f[2]) twice++; \
            } \
            if (i < nb) print B[i]; \
          } \
          for (k = 1; k <= NA[mob]; k++) { nm = AN[mob, k]; gsub(/_/, " ", nm); printf "\t%d\t%s\t1\t%s\t-- %s\r\n", mx + k, AV[mob, k], AP[mob, k], nm; } \
          print B[nb]; \
        } else for (i = 1; i <= nb; i++) print B[i]; \
      } \
      BEGIN { nc = split(cut, c, " "); for (i = 1; i <= nc; i++) C[c[i]] = 1; n = split(add, a, " "); for (i = 1; i <= n; i++) { split(a[i], p, ":"); k = ++NA[p[1]]; AV[p[1], k] = p[2]; AP[p[1], k] = p[3]; AN[p[1], k] = p[4]; } } \
      /^Group/ { inb = 1; nb = 0; mob = ""; typ = ""; } \
      !inb { print; next; } \
      { B[++nb] = $0; t = $0; sub(/\r$/, "", t); k = split(t, f); if (k >= 2 && tolower(f[1]) == "mob") mob = f[2]; if (k >= 2 && tolower(f[1]) == "type") typ = tolower(f[2]); } \
      /^}/ { flush(); inb = 0; } \
      END { if (inb) flush(); if (hits != 1 || gone != nc) { printf "share: %d limit groups of %s, %d of its %d lines of 75 cut\n", hits, from, gone, nc > "/dev/stderr"; bad = 1; } for (m in NA) if (got[m] != 1) { printf "share: %d limit groups of %s\n", got[m], m > "/dev/stderr"; bad = 1; } if (twice) { printf "share: %d lines of 75 were there already\n", twice > "/dev/stderr"; bad = 1; } exit bad; } \
      ' "$F" > "$F.new" \
 && mv "$F.new" "$F" \
 && awk -v from=2092 -v cut="$C" -v add="$A" ' \
      function check(   i, n, key, q) { \
        seen[mob]++; \
        for (i = 1; i <= nb; i++) if (I[i] != i) { printf "share: line %d of the limit group of %s is numbered %d\n", i, mob, I[i] > "/dev/stderr"; bad = 1; } \
        if (mob == from) { for (i = 1; i <= nb; i++) if (V[i] in C) { printf "share: %s is still in the limit group of %s\n", V[i], mob > "/dev/stderr"; bad = 1; } return; } \
        for (key in W) { split(key, q, SUBSEP); if (q[1] != mob) continue; n = 0; for (i = 1; i <= nb; i++) if (V[i] == q[2]) n++; if (n != 1) { printf "share: %s is %d times in the limit group of %s\n", q[2], n, mob > "/dev/stderr"; bad = 1; } } \
      } \
      BEGIN { nc = split(cut, c, " "); for (i = 1; i <= nc; i++) C[c[i]] = 1; n = split(add, a, " "); for (i = 1; i <= n; i++) { split(a[i], p, ":"); W[p[1], p[2]] = 1; M[p[1]] = 1; } } \
      /^Group/ { mob = ""; typ = ""; nb = 0; } \
      { t = $0; sub(/\r$/, "", t); k = split(t, f); if (k >= 2 && tolower(f[1]) == "mob") mob = f[2]; if (k >= 2 && tolower(f[1]) == "type") typ = tolower(f[2]); if (k >= 2 && f[1] ~ /^[0-9]+$/) { nb++; I[nb] = f[1] + 0; V[nb] = f[2]; } } \
      /^}/ && typ == "limit" && (mob == from || (mob in M)) { check(); } \
      END { if (seen[from] != 1) { printf "share: %d limit groups of %s\n", seen[from], from > "/dev/stderr"; bad = 1; } for (m in M) if (seen[m] != 1) { printf "share: %d limit groups of %s\n", seen[m], m > "/dev/stderr"; bad = 1; } exit bad; } \
      ' "$F" \
 && echo "share: the level-75 weapons are the Reaper's and Azrael's, not the Spider Baroness's"

# A herbal recipe is read without a wait (port/shareify.py renders this step;
# Iwakura for Derpsonkowy95, 29 September): crafting.learn_recipe wrote a wait
# of SKILLBOOK_LEARN_DELAY - twenty-one hours - on every recipe a read took and
# refused the next read until it ran out or an Exorcism Scroll was spent. It is
# no longer written, and one written before is no longer asked.
RUN set -eu; F=/opt/metin2/share/locale/poland/quest/libs/crafting/crafting.lua \
 && LC_ALL=C sed -i -E 's/^([[:blank:]]*local is_delay = )delta_delay > 0([[:space:]]*)$/\1false\2/; s/^([[:blank:]]*set_flag\("learn_delay"\.\.vnum, )get_time\(\) \+ SKILLBOOK_LEARN_DELAY\)([[:space:]]*)$/\10)\2/' "$F" \
 && test "$(LC_ALL=C grep -c -E '^[[:blank:]]*local is_delay = false[[:space:]]*$' "$F")" -eq 1 \
 && test "$(LC_ALL=C grep -c -E 'set_flag\("learn_delay"\.\.vnum, 0\)' "$F")" -eq 1 \
 && ! LC_ALL=C grep -q 'SKILLBOOK_LEARN_DELAY' "$F" \
 && echo "share: a recipe is read without a wait"

# The Spider Baroness (port/shareify.py renders special_spawns.baroness.txt and
# this step): the level-75 boss who drops the Stalki, stood in the second
# Spider Dungeon's last room every four to five hours (the operator's decision
# of 28 September). The package spawns her nowhere; the damage multiplier her
# lair would give her is the engine's (playerbotify apply_spider_baroness_damage).
COPY special_spawns.baroness.txt /tmp/spawn-add/
RUN set -eu; S=/opt/metin2/share/locale/poland/special_spawns.txt; A=/tmp/spawn-add/special_spawns.baroness.txt \
 && ! grep -q -E '^[[:blank:]]*vnum[[:blank:]]+2092[[:space:]]*$' "$S" \
 && sed -i 's/\r$//; s/$/\r/' "$A" \
 && { [ -z "$(tail -c 1 "$S")" ] || printf '\r\n' >> "$S"; } \
 && cat "$A" >> "$S" \
 && rm -rf /tmp/spawn-add \
 && test "$(grep -c -E '^[[:blank:]]*spawn_vnum[[:blank:]]+2092[[:space:]]*$' "$S")" -eq 1 \
 && echo "share: the Spider Baroness spawns in the second Spider Dungeon"

# The cores run unprivileged and read this tree by path; a directory only root
# can enter costs every quest below it, silently. Repair, then assert.
RUN set -eu; \
    find /opt/metin2/share -type d ! -perm -005 -exec chmod a+rx {} + ; \
    find /opt/metin2/share -type f ! -perm -004 -exec chmod a+r  {} + ; \
    bad="$(find /opt/metin2/share \( -type d ! -perm -005 \) -o \( -type f ! -perm -004 \) | head -5)"; \
    if [ -n "$bad" ]; then \
      echo "FATAL: still not readable by the server account:"; echo "$bad"; exit 1; \
    fi; \
    echo "share tree is readable by the server account"

# The three quests compiled in the quests stage, as a pure addition on top of
# the package's own object tree (new files under quest/object/, the sources
# beside the stock ones). Empty when web_admin.quest was left out.
COPY --from=quests /out-quest/ /opt/metin2/share/locale/

# Seed for a core's mark/ directory (guild symbols): copied into the state
# volume on first run only.
COPY src/serverfiles/mark-default /opt/metin2/share/mark-default

COPY bin/ /usr/local/bin/
RUN sed -i 's/\r$//' /usr/local/bin/entrypoint.sh /usr/local/bin/m2-* /usr/local/bin/m2ctl \
 && chmod 0755 /usr/local/bin/entrypoint.sh /usr/local/bin/m2-* /usr/local/bin/m2ctl \
 && chmod 0755 /opt/metin2/share/bin/game /opt/metin2/share/bin/db

# ---- mutable half: everything the cores write lives here, and only here.
RUN mkdir -p /opt/metin2/var && chown metin2:metin2 /opt/metin2/var
VOLUME ["/opt/metin2/var"]

ENV M2_SHARE_DIR=/opt/metin2/share \
    M2_VAR_DIR=/opt/metin2/var \
    TZ=UTC

EXPOSE 11000/tcp 13000/tcp 13001/tcp 13002/tcp

HEALTHCHECK --interval=15s --timeout=5s --start-period=180s --retries=4 \
    CMD ["/usr/local/bin/m2-healthcheck"]

ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]
CMD ["m2-supervise"]
