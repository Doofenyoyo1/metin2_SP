# -*- coding: utf-8 -*-
"""Czy manifest wskazuje na wydanie, ktore wlasnie publikujemy?

Wydanie 2.0.38 wyszlo na GitHuba 13 wrzesnia o 21:08 i nie dotarlo do nikogo.
Manifest - jedyny plik, z ktorego launcher czyta, co zainstalowac - zostal
zmieniony lokalnie i nigdy nie zacommitowany, a nastepny commit (2.0.39) go
nadpisal. Historia manifestu idzie wiec 2.0.37 -> 2.0.39, bez 2.0.38 posrodku.

Gracz nie zobaczyl przy tym zadnego bledu. W logu SIZOWSKIEGO stoi siedem razy
"Sprawdzam aktualizacje (zainstalowana wersja: 2.0.37)..." i ani jednego
pobrania: launcher uczciwie porownal 2.0.37 z 2.0.37 i uznal, ze nie ma nic
nowego. Taka awaria jest cicha po obu stronach - u nas wydanie wyglada na
opublikowane, u gracza na aktualne - i dlatego potrzebuje wlasnej bramki.

Dlaczego nie lapie tego check-release-covers-changes.py: tam manifest stoi na
liscie NEVER_SHIPS ("manifest jest publikowany osobno"), i slusznie, bo nie
jedzie w paczce. Wylaczenie z paczki jest wlasnie powodem, dla ktorego nikt nie
zauwazyl, ze nie zostal przestawiony.

Uzycie:
    python tools/check-manifest-points-at-release.py 2.0.39

Wersja zaczynajaca sie od "2." jest sprawdzana w manifescie mt2009, kazda inna
w manifescie linii r40250.
"""
from __future__ import print_function

import io
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MT2009 = 'update-manifest-mt2009.json'
CLASSIC = 'update-manifest.json'


def load(name):
    path = os.path.join(ROOT, name)
    if not os.path.isfile(path):
        return None, 'nie ma pliku %s' % name
    # Manifesty bywaly zapisywane z BOM-em; utf-8-sig czyta oba warianty.
    with io.open(path, encoding='utf-8-sig') as handle:
        try:
            return json.load(handle), None
        except ValueError as exc:
            return None, '%s nie jest poprawnym JSON-em: %s' % (name, exc)


def check(version):
    name = MT2009 if version.startswith('2.') else CLASSIC
    manifest, problem = load(name)
    if problem:
        return [problem]

    server = manifest.get('server') or {}
    faults = []

    got = str(server.get('version', ''))
    if got != version:
        faults.append(
            'manifest %s mowi server.version=%s, a wydajemy %s - launcher '
            'zaproponuje graczowi %s albo nic' % (name, got or '(brak)', version, got or '(brak)'))

    url = str(server.get('url', ''))
    if version not in url:
        faults.append(
            'server.url nie zawiera %s: %s' % (version, url or '(brak)'))

    sha = str(server.get('sha256', ''))
    if not re.match(r'^[0-9A-Fa-f]{64}$', sha):
        faults.append(
            'server.sha256 nie wyglada na sume SHA-256: %s' % (sha or '(brak)'))

    return faults


def main(argv):
    if len(argv) != 2:
        print('uzycie: check-manifest-points-at-release.py <wersja>')
        return 2
    version = argv[1].lstrip('v')
    faults = check(version)
    if faults:
        print('MANIFEST NIE WSKAZUJE NA WYDANIE %s:' % version)
        for fault in faults:
            print('  - %s' % fault)
        print('')
        print('Popraw manifest, zacommituj go i wypchnij PRZED ogloszeniem')
        print('wydania. Bez tego paczka lezy na GitHubie, a launcher jej nie')
        print('widzi - dokladnie jak 2.0.38.')
        return 1
    print('manifest wskazuje na %s, url i suma sie zgadzaja' % version)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
