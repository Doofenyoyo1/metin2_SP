# -*- coding: utf-8 -*-
"""The mt2009 server update package, built on Linux from the previous one.

    python tools/build_mt2009_server_update.py --previous <server-update-X.zip>
        [--engine-base <server.zip>] --out <dir>

tools/New-M2UpdatePackage.ps1 is the packager, and it needs Windows: it
builds from a working tree that holds the staged engine and the panel's build
context, neither of which git tracks. This produces the same package from a
clean `git archive HEAD`: the file list and the PathMap of
linux-port-mt2009/README.md, bytecode skipped under wildcards, VERSION at the
root, the overlay and staged playerbot_* sources identical. What git does not
track comes out of the previous release's package - the engine files this
repository never edits outside the port scripts, with the port-script edits
named in ENGINE_EDITS applied over them - and the staged playerbot_*
are HEAD's overlay, the panel files/admin_panel.py. With --engine-base (the
upstream full package's src/server, tools/upstream-sync.json "engine_base")
every engine file that neither git nor the previous package holds comes from
there, and the package carries the whole engine tree.

It then compares the file set with the previous package and fails if one was
dropped, because an update never deletes a file and a dropped one is almost
always a path that moved without the list noticing. Entries are written with
backslashes, as Compress-Archive on Windows PowerShell 5.1 writes them.
"""
import argparse
import fnmatch
import hashlib
import io
import os
import shutil
import subprocess
import sys
import tarfile
import tempfile
import zipfile

REPO = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
PATHMAP = {
    'linux-port-mt2009/docker/docker-compose.deploy.yml': 'linux-port/docker/docker-compose.yml',
    'linux-port-mt2009/VERSION': 'VERSION',
    'linux-port-mt2009/PACZKA_INFO.txt': 'PACZKA_INFO.txt',
    'linux-port-mt2009/': 'linux-port/',
}
# Gitignored and staged by the launcher, but carried by every package.
KEEP_FROM_PREVIOUS = ['linux-port/docker/seban-panel/VERSION']
OVERLAY = 'linux-port/overlays/playerbot/src/game/src'
STAGED = 'linux-port-mt2009/docker/game/src/server/game/src'
# Engine edits this repository made after the package it builds on. The engine
# files come out of that package verbatim, so an edit that lives only in
# port/playerbotify.py would never reach a player; each one named here is
# applied to the filled tree. They are idempotent: a package that already
# carries one finds it "already", and an anchor that moved stops the build.
# Since 2.16.0 the package is MT2009 PLUS's: its own mount and costume
# system replaced apply_costume_mount_allowed and apply_ride_seal_equip, and
# the project speaks Polish only, so the apply_person_language* edits went too.
ENGINE_EDITS = ['apply_sidekick_quest_kill_credit',
                'apply_quest_pc_is_playerbot', 'apply_drop_share_active', 'apply_tanaka_goblin',
                'apply_shaman_party_buff', 'apply_chest_mob_preview',
                'apply_auto_hunt_item_switch', 'apply_auto_hunt_loot_kinds', 'apply_refine_chance_shown',
                'apply_flea_market_fill_dispatch', 'apply_build_refusals_spoken',
                'apply_all_three_smiths', 'apply_build_refusal_reason', 'apply_boss_last_blow',
                'apply_guild_person_struck', 'apply_guild_bot_orders', 'apply_guild_war_answer_type',
                'apply_flea_price_range', 'apply_bot_shop_two_pages', 'apply_autospawn_bootstrap_once',
                'apply_quest_warp_channel', 'apply_mob_preview_stone_kinds', 'apply_peer_whisper_to_bot',
                'apply_spider_baroness_damage', 'apply_target_hp_values',
                'apply_guild_war_kills']
# Files an upstream package carries that this repository does not publish. An
# update never deletes a file, so a player who took the upstream package keeps
# it; the drop check below is for paths this repository's own list lost.
# MT2009 PLUS's notes on its own releases and packaging (its update channel,
# its mod list, its package list), which this project does not publish.
NOT_OURS = {'AKTUALIZACJE_MOD.md', 'MODS_PL.md', 'MOD_VERSION', 'SERWER_PL.md',
            'launcher/server-update-files.mod.txt',
            # Seban's host-side scripts and notes, which his own .gitignore
            # keeps out of the panel: nothing in our images runs them.
            'linux-port/docker/seban-panel/AKTUALIZATOR_PROSTO.md',
            'linux-port/docker/seban-panel/UPDATER_VPS.md',
            'linux-port/docker/seban-panel/deploy_map_names.sh',
            'linux-port/docker/seban-panel/deploy_new_item_icons.sh',
            'linux-port/docker/seban-panel/deploy_new_maps.sh',
            'linux-port/docker/seban-panel/deploy_new_maps_v2.sh',
            'linux-port/docker/seban-panel/patch_grant_quest.py',
            'linux-port/docker/seban-panel/rebuild_item_icons_from_item_list.sh',
            # The stable keeper's training missions, retired by its 2.19.0 (Digi
            # Rasta's horse to level 30): its git dropped them and its package
            # still carries the old copies, which nothing compiles any more.
            'linux-port/docker/game/quest/horse_levelup.quest',
            'linux-port/docker/game/quest/pony_levelup.quest'}


def published(rel):
    for k in sorted(PATHMAP, key=len, reverse=True):
        if rel.lower().startswith(k.lower()):
            return PATHMAP[k] + rel[len(k):]
    return rel


def bytecode(rel):
    return '__pycache__' in rel.split('/') or rel.endswith(('.pyc', '.pyo'))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--previous', required=True, help='the previous release\'s server update zip')
    ap.add_argument('--engine-base', help='the upstream full package\'s engine tree (server/...), '
                                          'tools/upstream-sync.json "engine_base"')
    ap.add_argument('--out', required=True)
    a = ap.parse_args()

    version = open(os.path.join(REPO, 'linux-port-mt2009', 'VERSION')).read().strip()
    src = tempfile.mkdtemp(prefix='m2-server-src-')
    tar = subprocess.check_output(['git', 'archive', 'HEAD'], cwd=REPO)
    tarfile.open(fileobj=io.BytesIO(tar)).extractall(src)

    old = zipfile.ZipFile(a.previous)
    old_raw = {n.replace('\\', '/'): n for n in old.namelist() if not n.endswith(('/', '\\'))}
    entries = [l.strip() for l in io.open(os.path.join(REPO, 'launcher', 'server-update-files.mt2009.txt'), encoding='utf-8-sig')
               if l.strip() and not l.strip().startswith('#')]

    filled = 0
    from_base = []
    for e in entries:
        if any(c in e for c in '*?'):
            d, leaf = e.rsplit('/', 1)
            if os.path.isdir(os.path.join(src, d)):
                continue
            prefix = published(d + '/')
            for n in old_raw:
                if not n.startswith(prefix):
                    continue
                rest = n[len(prefix):]
                if leaf == '*' or ('/' not in rest and fnmatch.fnmatch(rest, leaf)):
                    dst = os.path.join(src, d, rest)
                    os.makedirs(os.path.dirname(dst), exist_ok=True)
                    open(dst, 'wb').write(old.read(old_raw[n]))
                    filled += 1
        elif not os.path.isfile(os.path.join(src, e)):
            pub = published(e)
            if pub not in old_raw:
                # An engine file upstream's update package left out (2.18.0:
                # guild_manager.cpp, which an ENGINE_EDITS edit changes) comes
                # from the engine base, laid down below.
                if a.engine_base:
                    from_base.append(e)
                    continue
                sys.exit('listed, not in git and not in the previous package: ' + e)
            os.makedirs(os.path.dirname(os.path.join(src, e)), exist_ok=True)
            open(os.path.join(src, e), 'wb').write(old.read(old_raw[pub]))
            filled += 1
    for keep in KEEP_FROM_PREVIOUS:
        if not os.path.isfile(os.path.join(src, keep)) and keep in old_raw:
            open(os.path.join(src, keep), 'wb').write(old.read(old_raw[keep]))
            filled += 1

    # MT2009 PLUS's update packages are made for its own full package and
    # carry only the engine files it changed lately: what it changed earlier
    # and never shipped (CSafeboxCache::EraseOwner, DSManager::
    # RepairZeroAttributeItem) is in that full package alone, and on a tree
    # that came from Tieru's full package - every one of ours - 2.17.0 stopped
    # in the db core. The full package's whole engine tree is laid under what
    # git and the update package already put there and ships with every
    # release, so a player's tree is the one this code was written against
    # whatever it started from. Its playerbot_* are left out (the overlay is
    # ours) and so is the runtime data it keeps beside the sources.
    base_files = []
    if a.engine_base:
        engine = STAGED.rsplit('/game/src', 1)[0] + '/'
        bz = zipfile.ZipFile(a.engine_base)
        for n in bz.namelist():
            nn = n.replace('\\', '/')
            if nn.endswith('/') or not nn.startswith('server/'):
                continue
            rel = nn[len('server/'):]
            if rel.startswith('serverfiles/') or (
                    rel.startswith('game/src/') and os.path.basename(rel).startswith('playerbot_')):
                continue
            base_files.append(engine + rel)
            dst = os.path.join(src, engine + rel)
            if not os.path.isfile(dst):
                os.makedirs(os.path.dirname(dst), exist_ok=True)
                open(dst, 'wb').write(bz.read(n))
        if not base_files:
            sys.exit('the engine base holds no server/ tree: ' + a.engine_base)
    for e in from_base:
        if not os.path.isfile(os.path.join(src, e)):
            sys.exit('listed, not in git, the previous package or the engine base: ' + e)

    sys.path.insert(0, os.path.join(REPO, 'linux-port-mt2009', 'port'))
    import playerbotify
    for name in ENGINE_EDITS:
        getattr(playerbotify, name)(os.path.join(src, STAGED))

    shutil.copyfile(os.path.join(src, 'files', 'admin_panel.py'),
                    os.path.join(src, 'linux-port', 'docker', 'panel', 'app', 'admin_panel.py'))
    staged = os.path.join(src, STAGED)
    for f in os.listdir(staged):
        if f.startswith('playerbot_'):
            os.remove(os.path.join(staged, f))
    for f in os.listdir(os.path.join(src, OVERLAY)):
        if f.startswith('playerbot_'):
            shutil.copyfile(os.path.join(src, OVERLAY, f), os.path.join(staged, f))

    files = []
    for e in entries:
        if not any(c in e for c in '*?'):
            files.append(e)
            continue
        d, leaf = e.rsplit('/', 1)
        root = os.path.join(src, d)
        if not os.path.isdir(root):
            sys.exit('pattern directory missing: ' + d)
        if leaf == '*':
            m = sorted(os.path.relpath(os.path.join(dp, f), src).replace(os.sep, '/')
                       for dp, _, fs in os.walk(root) for f in fs)
        else:
            m = sorted(d + '/' + f for f in os.listdir(root)
                       if os.path.isfile(os.path.join(root, f)) and fnmatch.fnmatch(f, leaf))
        kept = [x for x in m if not bytecode(x)]
        if not kept:
            sys.exit('pattern matched nothing: ' + e)
        files += kept
    files += [k for k in KEEP_FROM_PREVIOUS if os.path.isfile(os.path.join(src, k))]
    files += base_files

    pub = {}
    for f in files:
        if not os.path.isfile(os.path.join(src, f)):
            sys.exit('listed file does not exist: ' + f)
        pub.setdefault(published(f), f)
    if 'VERSION' not in pub:
        sys.exit('no VERSION at the root of the package')
    if 'linux-port/docker/.env' in pub:
        sys.exit('.env is never published')
    ovp, stp = published(OVERLAY + '/'), published(STAGED + '/')
    ov = {p[len(ovp):] for p in pub if p.startswith(ovp)}
    st = {p[len(stp):] for p in pub if p.startswith(stp) and p[len(stp):].startswith('playerbot_')}
    if ov != st:
        sys.exit('overlay and staged playerbot sources differ in names: %s' % sorted(ov ^ st))
    for n in ov:
        if open(os.path.join(src, pub[ovp + n]), 'rb').read() != open(os.path.join(src, pub[stp + n]), 'rb').read():
            sys.exit('overlay and staged copy differ: ' + n)

    dropped = sorted(set(old_raw) - set(pub) - NOT_OURS)
    if dropped:
        sys.exit('files the previous package carried and this one does not: %s' % dropped)
    changed = sorted(p for p in set(pub) & set(old_raw)
                     if open(os.path.join(src, pub[p]), 'rb').read() != old.read(old_raw[p]))

    os.makedirs(a.out, exist_ok=True)
    zp = os.path.join(a.out, 'metin2-server-update-%s.zip' % version)
    with zipfile.ZipFile(zp, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for p in sorted(pub):
            z.write(os.path.join(src, pub[p]), p.replace('/', '\\'))
    shutil.rmtree(src, ignore_errors=True)
    digest = hashlib.sha256(open(zp, 'rb').read()).hexdigest().upper()
    print('server %s: %d files, %d from the previous package, %d changed, %d new' % (
        version, len(pub), filled, len(changed), len(set(pub) - set(old_raw))))
    for p in changed:
        print('  changed', p)
    print(zp)
    print('sha256', digest)


if __name__ == '__main__':
    main()
