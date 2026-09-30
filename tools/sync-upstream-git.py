# -*- coding: utf-8 -*-
"""Bring MT2009 PLUS's new commits in, three-way, from its git.

    python tools/sync-upstream-git.py <clone of zaxerrrr-dot/mt2009-sp-plus> <new commit>

The base is `synced_commit` in tools/upstream-sync.json. Both commits are
exported with `git archive` (without releases/ and client-patches/, which hold
the release zips and the client's sources), and every file that differs
between them is merged into this tree with `git merge-file`: base = the old
commit's file, ours = this tree's, theirs = the new commit's.

Its layout is a package's, so its linux-port/docker/{game,mariadb,ENGINE},
its .env.example, linux-port/tools/ and its compose file map onto
linux-port-mt2009/ (the compose file onto docker-compose.deploy.yml); the
shared panel, itemshop, seban-panel and updater contexts and the overlay map
onto themselves. Line endings follow the file here. A file that is new
upstream is copied as it is.

Not merged, and reported: its changelog and version files, its manifest,
its package lists and server-patches/, and its staged admin_panel.py
(files/admin_panel.py is ours and is merged from its files/ copy). What the
release needs besides - the engine package in tools/upstream-sync.json, the
dev compose file, .gitattributes for new data, the client rebase - is
described in CLAUDE.md under "Upstream sync and releases". Commits nothing.
"""
import json
import os
import shutil
import subprocess
import sys
import tempfile

REPO = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
SKIP = {'CHANGELOG.md', 'VERSION', 'CLIENT_VERSION', 'MOD_VERSION', 'update-manifest-mt2009.json',
        'client-files.json', 'launcher/server-update-files.mod.txt',
        'linux-port/docker/panel/app/admin_panel.py',
        # Gitignored here and carried by the package from the previous one.
        'linux-port/docker/seban-panel/VERSION'}
SKIP_PREFIX = ('server-patches/',)


def ours_path(p):
    if p == 'linux-port/docker/docker-compose.yml':
        return 'linux-port-mt2009/docker/docker-compose.deploy.yml'
    if p == 'linux-port/docker/.env.example':
        return 'linux-port-mt2009/docker/.env.example'
    for d in ('game', 'mariadb', 'ENGINE'):
        if p.startswith('linux-port/docker/' + d):
            return 'linux-port-mt2009/' + p[len('linux-port/'):]
    if p.startswith('linux-port/tools/'):
        return 'linux-port-mt2009/' + p[len('linux-port/'):]
    return p


def export(clone, commit, dest):
    os.makedirs(dest)
    arch = subprocess.check_output(['git', 'archive', commit, '--', '.', ':!releases', ':!client-patches'], cwd=clone)
    subprocess.run(['tar', '-x', '-C', dest], input=arch, check=True)


def walk(root):
    for dp, _, fs in os.walk(root):
        for f in fs:
            yield os.path.relpath(os.path.join(dp, f), root).replace(os.sep, '/')


def rd(p):
    with open(p, 'rb') as f:
        return f.read()


def lf(b):
    return b.replace(b'\r\n', b'\n')


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    clone, new = sys.argv[1], sys.argv[2]
    base = json.load(open(os.path.join(REPO, 'tools', 'upstream-sync.json')))['synced_commit']
    work = tempfile.mkdtemp(prefix='m2-upstream-git-')
    report = []
    try:
        b_root, t_root = os.path.join(work, 'base'), os.path.join(work, 'new')
        export(clone, base, b_root)
        export(clone, new, t_root)
        for p in sorted(set(walk(b_root)) | set(walk(t_root))):
            b, t = os.path.join(b_root, p), os.path.join(t_root, p)
            if os.path.exists(b) and os.path.exists(t) and rd(b) == rd(t):
                continue
            if p in SKIP or p.startswith(SKIP_PREFIX):
                report.append('by hand:  ' + p)
                continue
            op = ours_path(p)
            o = os.path.join(REPO, op)
            if not os.path.exists(t):
                report.append('DELETED upstream: ' + op)
                continue
            if not os.path.exists(b):
                if not os.path.exists(o):
                    os.makedirs(os.path.dirname(o), exist_ok=True)
                    shutil.copyfile(t, o)
                    report.append('added:    ' + op)
                elif lf(rd(o)) != lf(rd(t)):
                    report.append('CONFLICT (added on both sides): ' + op)
                continue
            if not os.path.exists(o):
                report.append('CONFLICT (gone here, changed upstream): ' + op)
                continue
            if lf(rd(t)) == lf(rd(b)):
                continue
            crlf = b'\r\n' in rd(o)
            if lf(rd(o)) == lf(rd(b)):
                data = rd(t)
                data = data.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n') if crlf else lf(data)
                with open(o, 'wb') as f:
                    f.write(data)
                report.append('taken:    ' + op)
                continue
            m = {}
            for k, src in (('o', o), ('b', b), ('t', t)):
                m[k] = os.path.join(work, 'm_' + k)
                with open(m[k], 'wb') as f:
                    f.write(lf(rd(src)))
            rc = subprocess.call(['git', 'merge-file', '-L', 'ours', '-L', 'base', '-L', 'upstream', m['o'], m['b'], m['t']])
            data = rd(m['o'])
            with open(o, 'wb') as f:
                f.write(data.replace(b'\n', b'\r\n') if crlf else data)
            report.append(('merged:   ' if rc == 0 else 'CONFLICT: ') + op)
    finally:
        shutil.rmtree(work, ignore_errors=True)
    print('\n'.join(report))
    print('\nnext: resolve conflicts; compare what was taken or added with its bytes upstream; '
          'engine package, dev compose, .gitattributes and the client rebase per CLAUDE.md; '
          'synced_commit in tools/upstream-sync.json; VERSION/CLIENT_VERSION above both lines; CHANGELOG.')


if __name__ == '__main__':
    main()
