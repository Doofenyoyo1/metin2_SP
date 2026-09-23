# -*- coding: utf-8 -*-
"""Fishing stands on a map's own bank, measured off server_attr.

    python tools/generate_fishing_bank.py 21            # report Joan's banks
    python tools/generate_fishing_bank.py 1 --emit      # C++ rows for Yongan

A stand is a cell a bot may stand on with water in front of it. The engine's
`CHARACTER::fishing()` never reads the water - it computes a point four hundred
units ahead and ignores it - so what a stand has to be is somewhere the bot can
reach, close to the river, facing it, and not on top of the next angler.

  * `ATTR_WATER` is terrain and not a wall: a river carries BLOCK|WATER, a ford
    or a bridge deck carries WATER with the block bit clear. A stand is a dry
    standable cell (no BLOCK, no OBJECT, no WATER) within `--reach` of a wet one.
  * Points are generated on cell centres, because that is where the navigation
    grid samples: a stand on a cell corner is a point server_attr calls free and
    the route planner calls something else.
  * Stands are spaced by `--spacing` so two anglers do not end up on one tile.
  * The water point is the nearest wet cell centre, which is what the bot turns
    towards.

The bank is chosen as the largest connected stretch of shoreline near the
village, so the anglers stand where a player would rather than round some pond
on the far side of the map.
"""
import argparse
import collections
import io
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import analyse_map_spawns as spawns_tool   # noqa: E402
import decode_server_attr as dsa           # noqa: E402

LOCALE = spawns_tool.LOCALE
ATTR_BLOCK = 1 << 0
ATTR_WATER = 1 << 1
ATTR_BANPK = 1 << 2
ATTR_OBJECT = 1 << 7
CELL = 50


def read(p):
    return io.open(p, encoding='cp949', errors='replace')


def map_index():
    out = {}
    for line in read(os.path.join(LOCALE, 'map', 'index')):
        p = line.split()
        if len(p) >= 2 and p[0].isdigit():
            out[int(p[0])] = p[1]
    return out


def base_of(folder):
    for line in read(os.path.join(LOCALE, 'map', folder, 'Setting.txt')):
        p = line.split()
        if len(p) >= 3 and p[0] == 'BasePosition':
            return int(p[1]), int(p[2])
    return 0, 0


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument('maps', nargs='+', type=int)
    ap.add_argument('--near', type=int, nargs=2, default=None,
                    help='world x y to keep the bank close to (default: map centre)')
    ap.add_argument('--radius', type=int, default=20000)
    ap.add_argument('--reach', type=int, default=2, help='cells from dry to wet')
    ap.add_argument('--spacing', type=int, default=150)
    ap.add_argument('--limit', type=int, default=80)
    ap.add_argument('--emit', action='store_true')
    args = ap.parse_args(argv)

    index = map_index()
    for idx in args.maps:
        folder = index.get(idx)
        if not folder:
            print('map %d is not in map/index' % idx)
            continue
        base = base_of(folder)
        w, h, cells = dsa.load(os.path.join(LOCALE, 'map', folder, 'server_attr'))

        def at(cx, cy):
            return dsa.attribute_at(cells, w, h, cx, cy)

        # decode_server_attr reports SECTORS, not cells: each sector is 128x128
        # cells of fifty world units. Scanning range(w) x range(h) looks at the
        # first sixteen by twenty cells of the map and finds a river nowhere.
        cellsW, cellsH = w * 128, h * 128
        anchor = tuple(args.near) if args.near else (
            base[0] + cellsW * CELL // 2, base[1] + cellsH * CELL // 2)
        span = args.radius // CELL + 4
        ax = (anchor[0] - base[0]) // CELL
        ay = (anchor[1] - base[1]) // CELL

        wet = set()
        for cy in range(max(0, ay - span), min(cellsH, ay + span + 1)):
            for cx in range(max(0, ax - span), min(cellsW, ax + span + 1)):
                a = at(cx, cy)
                if a is not None and (a & ATTR_WATER):
                    wet.add((cx, cy))
        if not wet:
            print('// map %d %s: no water within %d of %s'
                  % (idx, folder, args.radius, anchor))
            continue

        stands = []
        seen = []
        # walk the wet cells nearest the anchor first, so the bank grows outward
        # from the village rather than from the map's corner
        def dist(c):
            return abs(base[0] + c[0] * CELL + CELL // 2 - anchor[0]) + \
                   abs(base[1] + c[1] * CELL + CELL // 2 - anchor[1])

        for wc in sorted(wet, key=dist):
            if len(stands) >= args.limit:
                break
            if dist(wc) > args.radius:
                break
            for dx in range(-args.reach, args.reach + 1):
                for dy in range(-args.reach, args.reach + 1):
                    cx, cy = wc[0] + dx, wc[1] + dy
                    a = at(cx, cy)
                    if a is None or (a & (ATTR_BLOCK | ATTR_OBJECT | ATTR_WATER)):
                        continue
                    sx = base[0] + cx * CELL + CELL // 2
                    sy = base[1] + cy * CELL + CELL // 2
                    if any(abs(sx - p[0]) + abs(sy - p[1]) < args.spacing for p in seen):
                        continue
                    wx = base[0] + wc[0] * CELL + CELL // 2
                    wy = base[1] + wc[1] * CELL + CELL // 2
                    stands.append((sx, sy, wx, wy))
                    seen.append((sx, sy))
                    break
                else:
                    continue
                break

        print('// map %d, %s: %d wet cells, %d stands within %d of %s'
              % (idx, folder, len(wet), len(stands), args.radius, anchor))
        if args.emit:
            for i in range(0, len(stands), 2):
                print('\t\t' + ' '.join('{ %6d, %6d, %6d, %6d },' % s
                                        for s in stands[i:i + 2]))
        else:
            spread = collections.Counter((s[0] // 10000, s[1] // 10000) for s in stands)
            print('   clusters (10k squares):', dict(spread))
            for s in stands[:6]:
                print('   stand %s water %s' % ((s[0], s[1]), (s[2], s[3])))


if __name__ == '__main__':
    main(sys.argv[1:])
