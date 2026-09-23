# -*- coding: utf-8 -*-
"""What actually stands on a map, read out of the server's own spawn files.

    python tools/analyse_map_spawns.py metin2_map_milgyo

A wiki says what a map was designed to hold; this says what this server will
put on it. The two disagree often enough that a hub table built from the wiki
sends bots to empty ground - so every level band, every hunting hub and every
mission home in the playerbot AI is supposed to come from here.

Reading the spawn files has one trap, documented at length in CLAUDE.md and
repeated in `blocks` below: `group.txt` and `group_group.txt` do not put the id
in the same field, and taking the last field of both reads a probability as a
group id. Every `r` line then resolves to nothing and the map looks empty.
"""
import collections
import io
import os
import re
import sys

# The Korean names in mob_proto are the fallback when mob_names_pl has no row,
# and a Windows console is cp1250: print what it cannot encode rather than
# dying on it.
try:
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')
except AttributeError:
    pass

HERE = os.path.dirname(os.path.abspath(__file__))
SHARE = os.environ.get('M2_SHARE') or os.path.join(
    HERE, '..', '..', 'm2src-cache', 'tree', 'server40250', 'share')
SHARE = os.path.abspath(SHARE) + os.sep
LOCALE = SHARE + os.path.join('locale', 'english') + os.sep


def read(path):
    return io.open(path, encoding='cp949', errors='replace')


def blocks(path, middle):
    """Vnum -> the numbers its block names.

    A group.txt member reads `<idx> "<name>" <mob vnum>`, so the mob is the
    last field; a group_group.txt member reads `<idx> <group vnum>
    <probability>`, so the group is the middle one.
    """
    out = collections.defaultdict(set)
    vnum, members = None, set()
    for line in read(path):
        t = line.strip()
        if not t or t == '{':
            continue
        if t == '}':
            if vnum is not None:
                out[vnum] |= members
            vnum, members = None, set()
            continue
        m = re.match(r'(?i)^vnum\s+(\d+)\s*$', t)
        if m:
            vnum = int(m.group(1))
            continue
        parts = t.split()
        if middle:
            if len(parts) >= 2 and parts[0].isdigit() and parts[1].isdigit():
                members.add(int(parts[1]))
        elif re.match(r'(?i)^(leader|\d+)\s', t):
            try:
                members.add(int(parts[-1]))
            except ValueError:
                pass
    return out


def load_mobs():
    """vnum -> level, rank, type, race, the etc-drop key and the Polish name.

    mob_proto's columns are VNUM, NAME, RANK, TYPE - in that order, so index 2
    is the rank (KNIGHT, S_PAWN, BOSS) and index 3 the type (MONSTER, STONE,
    NPC). They used to be stored the other way round and the spawn report
    printed "MONST" under a column headed "ranga".
    """
    names = {}
    try:
        for line in read(SHARE + os.path.join('conf', 'mob_names_pl.txt')):
            c = line.rstrip('\n').split('\t')
            if len(c) >= 2 and c[0].isdigit():
                names[int(c[0])] = c[1]
    except IOError:
        pass
    mobs = {}
    for n, line in enumerate(read(SHARE + os.path.join('conf', 'mob_proto.txt'))):
        if n == 0:
            continue
        c = line.rstrip('\n').split('\t')
        if len(c) < 33 or not c[0].isdigit():
            continue
        num = lambda i: int(c[i]) if c[i].lstrip('-').isdigit() else 0
        mobs[int(c[0])] = {
            'name': names.get(int(c[0]), c[1] if len(c) > 1 else ''),
            'rank': c[2], 'type': c[3], 'battle': c[4], 'level': num(5),
            'race': c[9], 'exp': num(20), 'drop': num(32),
            'hp': num(17), 'st': num(11),
        }
    return mobs


def resolve(folder, name='regen.txt'):
    """Every spawn line of a file as (kind, cell x, cell y, mob vnum).

    boss.txt and stone.txt have the same shape as regen.txt and use the same
    `g`/`r` indirection - the Hwang Temple's two bosses and its sixteen stones
    are all group lines - so they are read the same way rather than by taking
    the last field of the line for a mob.
    """
    groups = blocks(LOCALE + 'group.txt', False)
    supergroups = blocks(LOCALE + 'group_group.txt', True)
    out = []
    path = LOCALE + os.path.join('map', folder, name)
    if not os.path.exists(path):
        return out
    for line in read(path):
        t = line.strip()
        if not t or t.startswith('#'):
            continue
        parts = t.split()
        if len(parts) < 11:
            continue
        kind = parts[0].lower()[0]
        try:
            cx, cy, vid = int(parts[1]), int(parts[2]), int(parts[-1])
        except ValueError:
            continue
        if kind in ('m', 'b', 'e'):
            out.append((kind, cx, cy, vid))
        elif kind == 'g':
            for mob in groups.get(vid, ()):
                out.append((kind, cx, cy, mob))
        elif kind == 'r':
            for group in supergroups.get(vid, ()):
                for mob in groups.get(group, ()):
                    out.append((kind, cx, cy, mob))
    return out


def base_position(folder):
    for line in read(LOCALE + os.path.join('map', folder, 'Setting.txt')):
        parts = line.split()
        if len(parts) >= 3 and parts[0] == 'BasePosition':
            return int(parts[1]), int(parts[2])
    return 0, 0


def main(folder):
    mobs = load_mobs()
    spawns = resolve(folder)
    if not spawns:
        print('%s: zadnych spawnow - sprawdz nazwe folderu' % folder)
        return 1
    bx, by = base_position(folder)
    print('=== %s  base=(%d,%d)  punktow spawnu: %d ===' % (folder, bx, by, len(spawns)))

    per_mob = collections.Counter(v for _k, _x, _y, v in spawns)
    levels = [mobs.get(v, {}).get('level', 0) for v in per_mob]
    weighted = sorted((mobs.get(v, {}).get('level', 0), c) for v, c in per_mob.items())
    total = sum(c for _l, c in weighted)
    seen, median = 0, 0
    for lvl, count in weighted:
        seen += count
        if seen * 2 >= total:
            median = lvl
            break
    print('poziomy potworow: min %d, mediana (wg liczby spawnow) %d, max %d'
          % (min(levels), median, max(levels)))
    print()
    print('%-7s %-30s %5s %5s %8s %7s %s' % ('vnum', 'nazwa', 'lvl', 'ranga', 'spawnow', 'exp', 'drop'))
    for vnum, count in per_mob.most_common():
        m = mobs.get(vnum, {})
        print('%-7d %-30s %5d %5s %8d %7d %s' % (
            vnum, (m.get('name') or '?')[:30], m.get('level', 0),
            (m.get('rank') or '')[:5], count, m.get('exp', 0), m.get('drop', 0) or ''))

    for extra, label in (('boss.txt', 'BOSSY'), ('stone.txt', 'METINY')):
        rows = collections.Counter(v for _k, _x, _y, v in resolve(folder, extra))
        if not rows:
            print('\n%s: brak' % label)
            continue
        print('\n%s:' % label)
        for vnum, count in rows.most_common():
            m = mobs.get(vnum, {})
            print('  %-7d %-30s lvl %-4d ranga %-6s x%d' % (
                vnum, (m.get('name') or '?')[:30], m.get('level', 0),
                m.get('rank') or '?', count))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1] if len(sys.argv) > 1 else 'metin2_map_milgyo'))
