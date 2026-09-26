# -*- coding: utf-8 -*-
"""Bring upstream's changes in from its release packages, not from its git.

    python tools/sync-upstream-package.py --base-server <upstream server zip X>
        --new-server <upstream server zip Y>
        [--base-client <upstream client zip A> --new-client <upstream client zip B>]

From 2.2.17 the upstream project publishes no source: its repository holds the
releases, the changelog and the manifest, and a git diff between two of its
commits deletes this whole tree (tools/sync-upstream.sh refuses such a diff).
What a release does carry is the update package, and that package is the
source of everything a player runs: the playerbot overlay, the launcher, the
panels, the container scripts, the rendered migrator and seed, the staged
engine files. So a sync is a three-way merge per file: the base is the file in
the package this repository last synced to, ours is the file here, theirs is
the file in the new package.

Package paths are turned back into repository paths through
launcher/server-update-files.mt2009.txt and build_mt2009_server_update's
PathMap. What the script does not merge, and says so:

  * CHANGELOG.md and VERSION - a person writes those;
  * the staged engine tree (not in git): an engine file that differs needs a
    port-script edit (playerbotify.py, and ENGINE_EDITS in
    tools/build_mt2009_server_update.py), and the staged playerbot_* files are
    copies of the overlay, which is merged;
  * files only upstream publishes (its licence files).

Rendered files (apply.sh, the seed, the Dockerfile's share steps, the compose
deploy file) are merged like any other so the result can be read; the change
then belongs in their generator (migratorify, generate_seed + seedify,
shareify, composify) and the render must come out the same.

The client half extracts pack/root of both client packages with eterpack and
merges each changed script into linux-port-mt2009/client-root, and the COOP
files beside the exe into client-coop. Needs python-lzo.

Commits nothing. Conflicts are left with markers, as git merge-file leaves
them; the report lists them.
"""
import argparse
import fnmatch
import io
import os
import shutil
import subprocess
import sys
import tempfile
import zipfile

REPO = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
sys.path.insert(0, os.path.join(REPO, 'tools'))
from build_mt2009_server_update import published, STAGED  # noqa: E402

NOT_MERGED = {'CHANGELOG.md', 'linux-port-mt2009/VERSION'}
UPSTREAM_ONLY = {'LICENSE', 'LICENSE-MIT.txt', 'NOTICE.md'}


def unzip(path, dest):
    z = zipfile.ZipFile(path)
    for i in z.infolist():
        n = i.filename.replace('\\', '/')
        if n.endswith('/'):
            continue
        p = os.path.join(dest, n)
        os.makedirs(os.path.dirname(p), exist_ok=True)
        with open(p, 'wb') as f:
            f.write(z.read(i))


def walk(root):
    for dp, _, fs in os.walk(root):
        for f in fs:
            yield os.path.relpath(os.path.join(dp, f), root).replace(os.sep, '/')


def same(a, b):
    return os.path.isfile(a) and os.path.isfile(b) and open(a, 'rb').read() == open(b, 'rb').read()


def repo_paths(new_root):
    """Package path -> repository path, from the list the packager reads."""
    entries = [l.strip() for l in io.open(os.path.join(REPO, 'launcher', 'server-update-files.mt2009.txt'),
                                          encoding='utf-8-sig')
               if l.strip() and not l.strip().startswith('#')]
    rev = {}
    for e in entries:
        if any(c in e for c in '*?'):
            d, leaf = e.rsplit('/', 1)
            pre = published(d + '/')
            for p in walk(new_root):
                if not p.startswith(pre):
                    continue
                rest = p[len(pre):]
                if leaf == '*' or ('/' not in rest and fnmatch.fnmatch(rest, leaf)):
                    rev.setdefault(p, d + '/' + rest)
        else:
            rev.setdefault(published(e), e)
    return rev


def merge(ours, base, theirs, report):
    rel = os.path.relpath(ours, REPO)
    if not os.path.exists(base):
        if os.path.exists(ours):
            if not same(ours, theirs):
                report.append('CONFLICT (added on both sides): ' + rel)
            return
        os.makedirs(os.path.dirname(ours), exist_ok=True)
        shutil.copyfile(theirs, ours)
        report.append('added:    ' + rel)
        return
    if not os.path.exists(ours):
        report.append('CONFLICT (gone here, changed upstream): ' + rel)
        return
    rc = subprocess.call(['git', 'merge-file', '-L', 'ours', '-L', 'base', '-L', 'theirs', ours, base, theirs])
    report.append(('merged:   ' if rc == 0 else 'CONFLICT: ') + rel)


def sync_server(base_zip, new_zip, work, report):
    b, n = os.path.join(work, 'sb'), os.path.join(work, 'sn')
    unzip(base_zip, b)
    unzip(new_zip, n)
    rev = repo_paths(n)
    tracked = set(subprocess.check_output(['git', 'ls-files'], cwd=REPO).decode().split('\n'))
    staged = published(STAGED + '/')
    for p in sorted(walk(n)):
        if same(os.path.join(b, p), os.path.join(n, p)):
            continue
        r = rev.get(p)
        if p in UPSTREAM_ONLY:
            report.append('skipped (upstream only): ' + p)
        elif r is None:
            report.append('UNMAPPED (not in our list): ' + p)
        elif r in NOT_MERGED:
            report.append('by hand:  ' + r)
        elif p.startswith(staged):
            if not os.path.basename(p).startswith('playerbot_'):  # those are the overlay's copies
                report.append('ENGINE (needs a port-script edit): ' + p)
        elif r not in tracked and os.path.exists(os.path.join(b, p)):
            report.append('staged copy, not merged: ' + r)
        else:
            merge(os.path.join(REPO, r), os.path.join(b, p), os.path.join(n, p), report)


def sync_client(base_zip, new_zip, work, report):
    b, n = os.path.join(work, 'cb'), os.path.join(work, 'cn')
    unzip(base_zip, b)
    unzip(new_zip, n)
    eterpack = [sys.executable, os.path.join(REPO, 'tools', 'eterpack.py'), '--profile', 'mt2009', 'extract']
    for side in (b, n):
        subprocess.check_call(eterpack + [os.path.join(side, 'pack', 'root'), os.path.join(side, 'root')],
                              stdout=subprocess.DEVNULL)
    for p in sorted(walk(os.path.join(n, 'root'))):
        if not same(os.path.join(b, 'root', p), os.path.join(n, 'root', p)):
            merge(os.path.join(REPO, 'linux-port-mt2009', 'client-root', p),
                  os.path.join(b, 'root', p), os.path.join(n, 'root', p), report)
    for p in sorted(walk(n)):
        if p.startswith(('pack/', 'root/')) or p.endswith('.exe'):
            continue
        if not same(os.path.join(b, p), os.path.join(n, p)):
            merge(os.path.join(REPO, 'linux-port-mt2009', 'client-coop', p), os.path.join(b, p),
                  os.path.join(n, p), report)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--base-server', required=True)
    ap.add_argument('--new-server', required=True)
    ap.add_argument('--base-client')
    ap.add_argument('--new-client')
    a = ap.parse_args()
    work = tempfile.mkdtemp(prefix='m2-upstream-')
    report = []
    try:
        sync_server(a.base_server, a.new_server, work, report)
        if a.base_client and a.new_client:
            sync_client(a.base_client, a.new_client, work, report)
    finally:
        shutil.rmtree(work, ignore_errors=True)
    for line in report:
        print(line)
    print('\nnext: resolve conflicts; move a change to a rendered file into its generator; '
          'write an engine file\'s change as a playerbotify edit; scrub attributions; '
          'set VERSION/CLIENT_VERSION above both lines; CHANGELOG; tools/upstream-sync.json.')


if __name__ == '__main__':
    main()
