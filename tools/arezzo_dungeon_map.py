#!/usr/bin/env python3
"""arezzo_dungeon_map.py - the three Arezzo dungeons measured for the bots
(MT2009_PLUS_AREZZO_DUNGEON_BOTS_V1, playerbot_arezzo_dungeon_bots.h).

    arezzo_dungeon_map.py <griddir> [--ascii]

<griddir> holds each dungeon map's server_attr expanded by
`arezzo_bot_map.py decode <map>/server_attr <griddir>/<map>.grid` (python-lzo:
the m2pack-lzo image). For each dungeon it walks the 100-unit grid (a cell is
blocked when any of its four 50-unit cells carries ATTR_BLOCK or ATTR_OBJECT,
as the bots' navigation reads them) from the quest's entry point and checks
that every point the quest uses - each line of its regen files
(game/arezzo/data/dungeon/<quest>/*.txt) and the spawn_mob points of its
bosses - lies on that one walkable piece, and how many steps away.

Result of 1 October 2026: all three are one open arena round the guard -
Wukong 35 points within 24 steps of the entry, Skorpion 36 within 45,
Dzungla 37 within 30, none unreachable - so the bots need no corner tree
inside: the navigation's straight walk reaches every stage's objective, and
the plan of a run is the stage flag and its target list alone.
"""
import os, sys, struct, collections

HERE = os.path.dirname(os.path.abspath(__file__))
DATA = os.path.join(HERE, '..', 'linux-port', 'docker', 'game', 'arezzo', 'data', 'dungeon')
DUNGEONS = [
    # key, the map's folder, the quests' entry cell, the regen folder, spawn_mob points of the bosses
    ('wukong', 'plechito_wukong_dungeon', (264, 273), 'wzgorze_wukonga', [(264, 273), (264, 272)]),
    ('skorpion', 'plechito_scorpion_dungeon', (268, 228), 'ruiny_skorpiona', [(269, 226)]),
    ('dzungla', 'plechito_easter2023_dungeon', (384, 374), 'starozytna_dzungla', [(385, 348)]),
]

if len(sys.argv) < 2:
    sys.exit(__doc__)
griddir = sys.argv[1]
ascii_map = '--ascii' in sys.argv
bad_total = 0
for key, mapn, entry, folder, bosses in DUNGEONS:
    raw = open(os.path.join(griddir, mapn + '.grid'), 'rb').read()
    W, H = struct.unpack_from('<ii', raw, 0)
    g = raw[8:]
    N = W // 2

    def blocked(x, y):
        if x < 0 or y < 0 or x >= N or y >= N:
            return True
        for dy in (0, 1):
            for dx in (0, 1):
                if g[(2 * y + dy) * W + 2 * x + dx] & 0x81:
                    return True
        return False

    seen = {entry: 0}
    todo = collections.deque([entry])
    while todo:
        c = todo.popleft()
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1), (1, 1), (1, -1), (-1, 1), (-1, -1)):
            n = (c[0] + dx, c[1] + dy)
            if n in seen or blocked(*n):
                continue
            seen[n] = seen[c] + 1
            todo.append(n)
    pts = []
    for f in sorted(os.listdir(os.path.join(DATA, folder))):
        for line in open(os.path.join(DATA, folder, f)):
            p = line.split()
            if len(p) >= 3:
                pts.append((f, int(p[1]), int(p[2])))
    pts += [('boss', x, y) for x, y in bosses]
    bad = [(f, x, y) for f, x, y in pts if (x, y) not in seen]
    bad_total += len(bad)
    print('%-9s %-28s entry %s %s, walkable piece %d cells, points %d, unreachable %d, farthest %d steps' % (
        key, mapn, entry, 'blocked' if blocked(*entry) else 'open', len(seen), len(pts), len(bad),
        max(seen.get((x, y), 0) for f, x, y in pts)))
    for f, x, y in bad:
        near = min(seen, key=lambda c: (c[0] - x) ** 2 + (c[1] - y) ** 2)
        print('   UNREACHABLE %s (%d,%d), nearest open %s' % (f, x, y, near))
    if ascii_map:
        xs = [p[1] for p in pts]; ys = [p[2] for p in pts]
        marks = {}
        for f, x, y in pts:
            marks[(x, y)] = 'B' if f == 'boss' else ('w' if f.startswith('wave') else ('g' if f.startswith('guards') else 'S'))
        marks[entry] = 'E'
        for y in range(min(ys) - 4, max(ys) + 5):
            print('   ' + ''.join(marks.get((x, y), '#' if blocked(x, y) else ('.' if (x, y) in seen else ' '))
                                for x in range(min(xs) - 4, max(xs) + 5)))
sys.exit(1 if bad_total else 0)
