# -*- coding: utf-8 -*-
"""The three kingdoms, read out of the server's own map files.

    python tools/dump_world_catalog.py            # the twelve kingdom maps
    python tools/dump_world_catalog.py 1 3 4 5    # only these

Shinsoo and Jinno are supposed to get the same life Chunjo has, and every
number that takes a bot somewhere - a village's blacksmith, the gate to the
next map, the ground a hunting hub stands on - has to come from these files
rather than from a wiki or from Chunjo's numbers plus an offset. Each kingdom
lays its towns out differently, so an offset is always wrong.

What it reports per map, all of it measured:

  * `Setting.txt`: the base position and the size in sectors.
  * `Town.txt`: the respawn points, in world coordinates.
  * `npc.txt`: every service NPC this AI knows how to use, by vnum, and every
    warp NPC with the destination read out of its own name exactly as
    `FuncCheckWarp` does ("%s %ld %ld", cells, absolute).
  * `regen.txt` through `group.txt`/`group_group.txt`: how many spawn points,
    which levels, and the richest 64-cell squares as hub candidates.
  * `server_attr`: whether each of those points is standable at the cell
    centre the navigation grid samples, and whether it is inside a safe zone.

Nothing here writes to the project: it prints a report and, with --json, a
machine-readable dump beside it.
"""
import collections
import io
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import analyse_map_spawns as spawns_tool  # noqa: E402  (the regen/group parsers)

LOCALE = spawns_tool.LOCALE
SHARE = spawns_tool.SHARE
NAV_CELL = 50          # world units per server_attr cell
ATTR_BLOCK = 1 << 0
ATTR_BANPK = 1 << 2
ATTR_OBJECT = 1 << 7

# The kingdoms, by the map index file. M3 here is the guild map, which is what
# the existing Chunjo constant PLAYERBOT_MAP_CHUNJO_M3 = 24 already means.
KINGDOMS = [
    (1, 'Shinsoo', {'m1': 1, 'm2': 3, 'm3': 4, 'monkey_easy': 5}),
    (2, 'Chunjo', {'m1': 21, 'm2': 23, 'm3': 24, 'monkey_easy': 25}),
    (3, 'Jinno', {'m1': 41, 'm2': 43, 'm3': 44, 'monkey_easy': 45}),
]

# The NPCs the AI actually walks to, by the vnum it looks for.
SERVICES = {
    9003: 'misc merchant',
    9004: 'event helper',
    9002: 'armour merchant',
    9005: 'storekeeper',
    9006: 'skill reset (old woman)',
    9001: 'weapon merchant (Handlarz Bronia)',
    9012: 'teleporter',
    20016: 'blacksmith',
    20349: 'stable keeper',
    20353: 'stone monument',
    20356: 'ginseng collector',
}


def read(path):
    return io.open(path, encoding='cp949', errors='replace')


def map_index():
    out = {}
    for line in read(LOCALE + os.path.join('map', 'index')):
        parts = line.split()
        if len(parts) >= 2 and parts[0].isdigit():
            out[int(parts[0])] = parts[1]
    return out


def setting(folder):
    base, size = (0, 0), (0, 0)
    for line in read(LOCALE + os.path.join('map', folder, 'Setting.txt')):
        p = line.split()
        if len(p) >= 3 and p[0] == 'BasePosition':
            base = (int(p[1]), int(p[2]))
        elif len(p) >= 3 and p[0] == 'MapSize':
            size = (int(p[1]), int(p[2]))
    return base, size


def town_points(folder, base):
    out = []
    path = LOCALE + os.path.join('map', folder, 'Town.txt')
    if not os.path.exists(path):
        return out
    for line in read(path):
        p = line.split()
        if len(p) >= 2 and p[0].isdigit():
            point = (base[0] + int(p[0]) * 100, base[1] + int(p[1]) * 100)
            if point not in out:
                out.append(point)
    return out


def load_mob_names():
    """vnum -> the raw mob_proto name, which is where a warp keeps its target."""
    out = {}
    for n, line in enumerate(read(SHARE + os.path.join('conf', 'mob_proto.txt'))):
        if n == 0:
            continue
        c = line.rstrip('\n').split('\t')
        if len(c) < 2 or not c[0].isdigit():
            continue
        out[int(c[0])] = c[1]
    return out


def npcs(folder, base, mob_names):
    """Service NPCs and warp NPCs standing on this map, in world coordinates.

    A warp's destination lives in its own name as "<something> <x> <y>" in
    global cells - CHARACTER::FuncCheckWarp reads it with "%s %ld %ld" and
    multiplies by 100 - so it is already absolute and the map's base must not
    be added to it a second time.
    """
    services, warps = [], []
    path = LOCALE + os.path.join('map', folder, 'npc.txt')
    if not os.path.exists(path):
        return services, warps
    for line in read(path):
        t = line.strip()
        if not t or t.startswith('#'):
            continue
        p = t.split()
        if len(p) < 4:
            continue
        try:
            cx, cy, vnum = int(p[1]), int(p[2]), int(p[-1])
        except ValueError:
            continue
        world = (base[0] + cx * 100, base[1] + cy * 100)
        name = mob_names.get(vnum, '')
        m = re.match(r'^(.*?)\s+(\d+)\s+(\d+)\s*$', name)
        if m:
            warps.append({'vnum': vnum, 'name': name, 'local': (cx, cy), 'world': world,
                          'dest_world': (int(m.group(2)) * 100, int(m.group(3)) * 100)})
        elif vnum in SERVICES:
            services.append({'vnum': vnum, 'service': SERVICES[vnum],
                             'local': (cx, cy), 'world': world})
    return services, warps


class Attr(object):
    """server_attr for one map, asked at the point the navigation grid samples."""

    def __init__(self, folder, base):
        self.base = base
        self.ok = False
        try:
            import lzo  # noqa: F401
        except ImportError:
            return
        try:
            import decode_server_attr as dsa
            self.w, self.h, self.cells = dsa.load(
                LOCALE + os.path.join('map', folder, 'server_attr'))
            self.dsa = dsa
            self.ok = True
        except Exception:
            self.ok = False

    def at(self, world_x, world_y):
        if not self.ok:
            return None
        cx = (world_x - self.base[0]) // NAV_CELL
        cy = (world_y - self.base[1]) // NAV_CELL
        return self.dsa.attribute_at(self.cells, self.w, self.h, cx, cy)

    def describe(self, world_x, world_y):
        a = self.at(world_x, world_y)
        if a is None:
            return 'attr?'
        bits = []
        if a & (ATTR_BLOCK | ATTR_OBJECT):
            bits.append('BLOCKED')
        else:
            bits.append('standable')
        if a & ATTR_BANPK:
            bits.append('safe-zone')
        return '+'.join(bits)


def hub_candidates(spawn_list, mobs, base, attr, cell=64, top=6):
    """The richest 64-cell squares, with the level band actually standing there."""
    buckets = collections.defaultdict(list)
    for _, cx, cy, vnum in spawn_list:
        buckets[(cx // cell, cy // cell)].append(vnum)
    out = []
    for (bx, by), vnums in sorted(buckets.items(), key=lambda kv: -len(kv[1]))[:top]:
        levels = sorted(mobs.get(v, {}).get('level', 0) for v in vnums)
        centre = (base[0] + (bx * cell + cell // 2) * 100,
                  base[1] + (by * cell + cell // 2) * 100)
        out.append({
            'cell': (bx * cell + cell // 2, by * cell + cell // 2),
            'world': centre,
            'spawns': len(vnums),
            'level_min': levels[0] if levels else 0,
            'level_median': levels[len(levels) // 2] if levels else 0,
            'level_max': levels[-1] if levels else 0,
            'ground': attr.describe(*centre),
        })
    return out


def dump_map(index, folder, mobs, mob_names):
    base, size = setting(folder)
    attr = Attr(folder, base)
    services, warps = npcs(folder, base, mob_names)
    spawn_list = spawns_tool.resolve(folder)
    levels = sorted(mobs.get(v, {}).get('level', 0) for _, _, _, v in spawn_list)
    stones = spawns_tool.resolve(folder, 'stone.txt')
    bosses = spawns_tool.resolve(folder, 'boss.txt')
    return {
        'index': index, 'folder': folder, 'base': base, 'sectors': size,
        'town': town_points(folder, base),
        'services': services, 'warps': warps,
        'spawn_points': len(spawn_list),
        'level_min': levels[0] if levels else 0,
        'level_median': levels[len(levels) // 2] if levels else 0,
        'level_max': levels[-1] if levels else 0,
        'stones': len(stones), 'bosses': len(bosses),
        'hubs': hub_candidates(spawn_list, mobs, base, attr),
        'attr_readable': attr.ok,
        'town_ground': [attr.describe(*p) for p in town_points(folder, base)],
        'service_ground': [attr.describe(*s['world']) for s in services],
    }


def main(argv):
    wanted = [int(a) for a in argv if a.isdigit()]
    as_json = '--json' in argv
    index = map_index()
    mobs = spawns_tool.load_mobs()
    mob_names = load_mob_names()
    result = {}
    for empire, kingdom, roles in KINGDOMS:
        for role, idx in roles.items():
            if wanted and idx not in wanted:
                continue
            folder = index.get(idx)
            if not folder:
                print('map %d is not in map/index' % idx)
                continue
            info = dump_map(idx, folder, mobs, mob_names)
            info['empire'] = empire
            info['kingdom'] = kingdom
            info['role'] = role
            result[idx] = info
            print('=== %-8s %-4s map %-3d %-28s base=%s sectors=%dx%d' % (
                kingdom, role, idx, folder, info['base'], info['sectors'][0], info['sectors'][1]))
            print('    spawns=%-6d levels %d/%d/%d   stones=%-3d bosses=%-3d attr=%s' % (
                info['spawn_points'], info['level_min'], info['level_median'],
                info['level_max'], info['stones'], info['bosses'],
                'read' if info['attr_readable'] else 'NOT READ (needs python-lzo)'))
            for p, g in zip(info['town'], info['town_ground']):
                print('    town   %-18s %s' % (str(p), g))
            for s, g in zip(info['services'], info['service_ground']):
                print('    npc    %-5d %-22s %-18s %s' % (
                    s['vnum'], s['service'], str(s['world']), g))
            for w in info['warps']:
                print('    warp   %-5d %-18s -> %-18s  "%s"' % (
                    w['vnum'], str(w['world']), str(w['dest_world']), w['name'].strip()))
            for h in info['hubs']:
                print('    hub    %-18s spawns=%-5d lvl %d..%d (med %d)  %s' % (
                    str(h['world']), h['spawns'], h['level_min'], h['level_max'],
                    h['level_median'], h['ground']))
            print()
    if as_json:
        out = os.path.join(HERE, 'world_catalog.json')
        io.open(out, 'w', encoding='utf-8').write(
            json.dumps(result, indent=1, ensure_ascii=False, default=list))
        print('written', out)


if __name__ == '__main__':
    main(sys.argv[1:])
