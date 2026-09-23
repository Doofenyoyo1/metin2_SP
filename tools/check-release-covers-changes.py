# -*- coding: utf-8 -*-
"""Czy paczka serwera zawiera wszystko, co zmienilismy od ostatniego wydania?

Nie sprawdza, czy lista plikow pokrywa budowe (od tego jest
tools/check-update-covers-build.py), tylko czy KAZDA zmiana, ktora zrobilismy,
naprawde dojedzie do gracza. Te dwie rzeczy zawodza inaczej: pierwsza lapie
brakujacy plik budowy, ta lapie plik, ktory zmienilismy i o ktorym lista nie wie.
"""
from __future__ import print_function

import fnmatch
import io
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LIST = os.path.join(ROOT, 'launcher', 'server-update-files.txt')

# Co swiadomie nie jedzie do gracza i dlaczego.
NEVER_SHIPS = [
    ('CLAUDE.md', 'notatki projektu'),
    ('*.md', 'dokumentacja'),
    ('docs/*', 'dokumentacja i audyty'),
    ('installer/*', 'instalator pobiera sie z repozytorium przy kazdym uruchomieniu, nie z paczki'),
    ('tools/*', 'narzedzia pomiarowe, nie sa czescia serwera'),
    ('tests/*', 'testy'),
    ('builds/*', 'artefakty'),
    ('scratchpad/*', 'brudnopis'),
    ('update-manifest.json', 'manifest jest publikowany osobno'),
    ('linux-port/client-root/*', 'jedzie w paczce KLIENTA, nie serwera'),
    ('.gitignore', ''),
]


def patterns():
    out = []
    for line in io.open(LIST, encoding='utf-8'):
        line = line.strip()
        if line and not line.startswith('#'):
            out.append(line.replace('\\', '/'))
    return out


def covered(path, pats):
    for p in pats:
        if p == path:
            return p
        if fnmatch.fnmatch(path, p):
            return p
        # "dir/*" w tej liscie znaczy calosc katalogu, takze podkatalogi
        if p.endswith('/*') and path.startswith(p[:-1]):
            return p
    return None


def excused(path):
    for pat, why in NEVER_SHIPS:
        if fnmatch.fnmatch(path, pat) or (pat.endswith('/*') and path.startswith(pat[:-1])):
            return why or 'poza serwerem'
    return None


def main():
    base = sys.argv[1] if len(sys.argv) > 1 else 'v1.32.5'
    changed = subprocess.check_output(
        ['git', '-C', ROOT, 'diff', '--name-only', base + '..HEAD'],
        text=True).split()
    changed += subprocess.check_output(
        ['git', '-C', ROOT, 'status', '--porcelain'], text=True).split('\n')
    files = set()
    for entry in changed:
        entry = entry.strip()
        if not entry or entry in ('M', 'A', 'D', 'R', '??', 'RM'):
            continue
        if entry.startswith(('M ', 'A ', '?? ', 'RM ')):
            entry = entry.split(' ', 1)[1].strip()
        if '->' in entry:
            entry = entry.split('->')[-1].strip()
        if entry.endswith('/') or ' ' in entry:
            continue
        files.add(entry.replace('\\', '/'))

    pats = patterns()
    missing, shipped, skipped = [], [], []
    for path in sorted(files):
        if not os.path.exists(os.path.join(ROOT, path)):
            continue
        hit = covered(path, pats)
        if hit:
            shipped.append((path, hit))
            continue
        why = excused(path)
        if why:
            skipped.append((path, why))
            continue
        missing.append(path)

    print('zmienionych plikow: %d' % len(files))
    print('w paczce:           %d' % len(shipped))
    print('swiadomie poza:     %d' % len(skipped))
    for path, why in skipped:
        print('    %-58s %s' % (path, why))
    if missing:
        print('')
        print('NIE DOJADA DO GRACZA (%d):' % len(missing))
        for path in missing:
            print('    %s' % path)
        return 1
    print('')
    print('kazda zmiana ma swoja droge do gracza')
    return 0


if __name__ == '__main__':
    sys.exit(main())
