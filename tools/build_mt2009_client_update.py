# -*- coding: utf-8 -*-
"""The mt2009 client update package, built on Linux from the previous one.

    python tools/build_mt2009_client_update.py --previous <client-update-X.zip> --since <commit> --out <dir>

The packs (pack/root, pack/locale) are repacked with tools/eterpack.py from
the previous package's, replacing only what changed in client-root/ and
client-locale/ since <commit> - the commit the previous client was published
from. Before that it checks that every file of those two directories at
<commit> is in the previous packs byte for byte, so the baseline is the one
the diff assumes. The COOP files beside the exe are taken from HEAD when they
changed; metin2client.exe stays the previous one (it is built on Windows by
linux-port-mt2009/tools/build-client.ps1). Needs python-lzo.
"""
import argparse
import hashlib
import os
import shutil
import subprocess
import sys
import tempfile
import zipfile

REPO = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
PACKS = [('root', 'linux-port-mt2009/client-root'), ('locale', 'linux-port-mt2009/client-locale')]
BESIDE = 'linux-port-mt2009/client-coop'


def git(*args):
    return subprocess.check_output(['git'] + list(args), cwd=REPO)


def eterpack(*args):
    subprocess.check_call([sys.executable, os.path.join(REPO, 'tools', 'eterpack.py'), '--profile', 'mt2009'] + list(args),
                          stdout=subprocess.DEVNULL)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--previous', required=True, help='the previous release\'s client update zip')
    ap.add_argument('--since', required=True, help='the commit the previous client was published from')
    ap.add_argument('--out', required=True)
    a = ap.parse_args()

    version = open(os.path.join(REPO, 'linux-port-mt2009', 'CLIENT_VERSION')).read().strip()
    work = tempfile.mkdtemp(prefix='m2-client-')
    old_dir, new_dir = os.path.join(work, 'old'), os.path.join(work, 'new')
    old = zipfile.ZipFile(a.previous)
    names = [i.filename for i in old.infolist() if not i.filename.endswith(('/', '\\'))]
    for n in names:
        p = os.path.join(old_dir, n.replace('\\', '/'))
        os.makedirs(os.path.dirname(p), exist_ok=True)
        open(p, 'wb').write(old.read(n))
    shutil.copytree(old_dir, new_dir)

    for pack, src_dir in PACKS:
        extracted = os.path.join(work, 'x-' + pack)
        eterpack('extract', os.path.join(old_dir, 'pack', pack), extracted)
        index = {}
        for dp, _, fs in os.walk(extracted):
            for f in fs:
                full = os.path.join(dp, f)
                index[os.path.relpath(full, extracted).replace(os.sep, '/').lower()] = full
        for f in git('ls-tree', '-r', '--name-only', a.since, src_dir).decode().split():
            rel = f[len(src_dir) + 1:].lower()
            if rel not in index:
                sys.exit('%s at %s is not in the previous %s pack: %s' % (src_dir, a.since, pack, rel))
            if open(index[rel], 'rb').read() != git('show', a.since + ':' + f):
                sys.exit('%s at %s differs from the previous %s pack: %s - wrong --since?' % (src_dir, a.since, pack, rel))

        changed = git('diff', '--name-only', a.since, 'HEAD', '--', src_dir).decode().split()
        if not changed:
            continue
        repl = os.path.join(work, 'repl-' + pack)
        for f in changed:
            if not os.path.isfile(os.path.join(REPO, f)):
                sys.exit('a pack file was deleted, which a repack cannot express: ' + f)
            dst = os.path.join(repl, f[len(src_dir) + 1:])
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            shutil.copyfile(os.path.join(REPO, f), dst)
            print('  %s: %s' % (pack, f[len(src_dir) + 1:]))
        eterpack('repack', os.path.join(old_dir, 'pack', pack), os.path.join(new_dir, 'pack', pack), repl)

        # Round trip: only the replaced files may differ.
        check = os.path.join(work, 'y-' + pack)
        eterpack('extract', os.path.join(new_dir, 'pack', pack), check)
        want = {f[len(src_dir) + 1:].lower() for f in changed}
        for dp, _, fs in os.walk(check):
            for f in fs:
                full = os.path.join(dp, f)
                rel = os.path.relpath(full, check).replace(os.sep, '/').lower()
                same = rel in index and open(full, 'rb').read() == open(index[rel], 'rb').read()
                if not same and rel not in want:
                    sys.exit('repacked %s pack changed a file it was not asked to: %s' % (pack, rel))

    top = {n.replace('\\', '/') for n in names if '\\' not in n and '/' not in n}
    for f in git('diff', '--name-only', a.since, 'HEAD', '--', BESIDE).decode().split():
        base = os.path.basename(f)
        if base in top:
            shutil.copyfile(os.path.join(REPO, f), os.path.join(new_dir, base))
            print('  beside the exe:', base)

    os.makedirs(a.out, exist_ok=True)
    zp = os.path.join(a.out, 'metin2-client-update-%s.zip' % version)
    with zipfile.ZipFile(zp, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for n in names:
            z.write(os.path.join(new_dir, n.replace('\\', '/')), n)
    shutil.rmtree(work, ignore_errors=True)
    print(zp)
    print('sha256', hashlib.sha256(open(zp, 'rb').read()).hexdigest().upper())


if __name__ == '__main__':
    main()
