# -*- coding: utf-8 -*-
"""Hunting hubs for a map, measured rather than guessed.

    python tools/generate_wander_hubs.py 1 --count 32 --band
    python tools/generate_wander_hubs.py 3 43 --count 12

Chunjo's three wander tables were placed by hand off regen.txt and each hub is
the centre of a real spawn rectangle. Shinsoo and Jinno need the same thing for
six more maps, and "Chunjo's numbers plus an offset" is wrong for every one of
them: the villages are mirrors of each other only in what they contain, never in
where they put it.

The method is the one the Spider Dungeon V2 hubs were built with:

  * bucket every spawn point into 6400-unit squares and rank the squares by how
    many points they hold;
  * take them richest first, refusing any square whose centre is within
    --spacing of a hub already taken, so the set covers the map instead of
    crowding one field;
  * put the hub on the actual spawn point nearest that centre which is standable
    on server_attr and outside a safe zone - a hub inside BANPK ground is a hub
    nothing may be killed on;
  * report the median monster level within 2500 units of it, which is what the
    level bands are read from.

Nothing is written: it prints the C++ rows to paste into playerbot_wandering.h.
"""
import argparse
import collections
import io
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import analyse_map_spawns as spawns_tool  # noqa: E402

LOCALE = spawns_tool.LOCALE
ATTR_BLOCK = 1 << 0
ATTR_BANPK = 1 << 2
ATTR_OBJECT = 1 << 7
NAV_CELL = 50
BUCKET = 64          # cells, i.e. 6400 world units


def read(path):
    return io.open(path, encoding='cp949', errors='replace')


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


class Attr(object):
    def __init__(self, folder, base):
        self.base = base
        self.ok = False
        try:
            import decode_server_attr as dsa
            self.w, self.h, self.cells = dsa.load(
                os.path.join(LOCALE, 'map', folder, 'server_attr'))
            self.dsa = dsa
            self.ok = True
        except Exception as exc:          # noqa: BLE001 - reported, not hidden
            self.why = str(exc)

    def at(self, wx, wy):
        if not self.ok:
            return None
        return self.dsa.attribute_at(self.cells, self.w, self.h,
                                     (wx - self.base[0]) // NAV_CELL,
                                     (wy - self.base[1]) // NAV_CELL)

    def free(self, wx, wy):
        """Standable, and not inside a safe zone."""
        a = self.at(wx, wy)
        if a is None:
            return False
        return not (a & (ATTR_BLOCK | ATTR_OBJECT)) and not (a & ATTR_BANPK)


def hubs_for(index, folder, count, spacing, mobs):
    base = base_of(folder)
    attr = Attr(folder, base)
    if not attr.ok:
        print('  %s: server_attr not read (%s)' % (folder, getattr(attr, 'why', '?')))
        return []
    points = spawns_tool.resolve(folder)
    if not points:
        return []
    world = [(base[0] + cx * 100, base[1] + cy * 100, v) for _, cx, cy, v in points]

    buckets = collections.defaultdict(list)
    for wx, wy, v in world:
        buckets[((wx - base[0]) // (BUCKET * 100), (wy - base[1]) // (BUCKET * 100))].append((wx, wy, v))

    chosen = []
    for (bx, by), members in sorted(buckets.items(), key=lambda kv: -len(kv[1])):
        if len(chosen) >= count:
            break
        cx = base[0] + (bx * BUCKET + BUCKET // 2) * 100
        cy = base[1] + (by * BUCKET + BUCKET // 2) * 100
        if any(abs(cx - h[0]) + abs(cy - h[1]) < spacing for h in chosen):
            continue
        # the real spawn point nearest that centre which a bot may stand on
        best = None
        for wx, wy, _ in sorted(members, key=lambda p: abs(p[0] - cx) + abs(p[1] - cy)):
            if attr.free(wx, wy):
                best = (wx, wy)
                break
        if best is None:
            continue
        levels = sorted(mobs.get(v, {}).get('level', 0)
                        for wx, wy, v in world
                        if abs(wx - best[0]) + abs(wy - best[1]) <= 2500)
        median = levels[len(levels) // 2] if levels else 0
        chosen.append((best[0], best[1], median, len(members)))
    return chosen


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument('maps', nargs='+', type=int)
    ap.add_argument('--count', type=int, default=12)
    ap.add_argument('--spacing', type=int, default=6000)
    ap.add_argument('--band', action='store_true',
                    help='emit the third column (median monster level)')
    args = ap.parse_args(argv)

    index = map_index()
    mobs = spawns_tool.load_mobs()
    for idx in args.maps:
        folder = index.get(idx)
        if not folder:
            print('map %d is not in map/index' % idx)
            continue
        rows = hubs_for(idx, folder, args.count, args.spacing, mobs)
        print('// map %d, %s: %d hubs from regen.txt, standable and outside the safe zone'
              % (idx, folder, len(rows)))
        for i in range(0, len(rows), 3):
            chunk = rows[i:i + 3]
            if args.band:
                print('\t\t\t\t\t' + ' '.join('{ %d, %d, %d },' % (x, y, lv)
                                              for x, y, lv, _ in chunk))
            else:
                print('\t\t\t\t' + ' '.join('{ %d, %d },' % (x, y)
                                            for x, y, _, _ in chunk))
        print('// spawn density per hub: %s' % ', '.join(str(n) for _, _, _, n in rows))
        print()


if __name__ == '__main__':
    main(sys.argv[1:])
