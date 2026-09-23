# -*- coding: utf-8 -*-
"""Renderuje playerbot_weapon_atlas.h: kazda bron tego swiata i skad sie bierze.

"Opracuj wszystkie bronie dostepne na serwerze, aby boty wiedzialy po co graja"
(Tieru, 15 wrzesnia). Atlas mowi AI, jaka bron jest dla danej klasy na danym
poziomie do zdobycia i gdzie: u kupca w wiosce, ze wspolnego dropu potworow
danej rangi i poziomu, z konkretnego potwora albo skrzyni na mapie, na ktorej
boty poluja - albo tylko na mapie, na ktora boty nie chodza.

Wejscie to trzy tabele swiata mt2009 zrzucone z bazy (nazwy w cp1250, stad
HEX) i katalog locale serwera:

    docker exec -i <db> sh -c 'exec mariadb -uroot -p"$MARIADB_ROOT_PASSWORD" -N' > item_proto_hex.tsv
      SELECT vnum, HEX(locale_name), type, subtype, COALESCE(limitvalue0,0), antiflag,
             value1, value2, value3, value4, value5 FROM world.item_proto ORDER BY vnum;
    ... > mob_proto_hex.tsv
      SELECT vnum, HEX(locale_name), level, rank, type FROM world.mob_proto ORDER BY vnum;
    ... > shop_items.tsv
      SELECT s.npc_vnum, si.item_vnum FROM world.shop s
        JOIN world.shop_item si ON si.shop_vnum = s.vnum ORDER BY 1, 2;

Uzycie (z katalogu linux-port/overlays/playerbot):
    python tools/generate_weapon_atlas.py item_proto_hex.tsv mob_proto_hex.tsv shop_items.tsv \\
        ../../../linux-port-mt2009/docker/game/src/serverfiles/share/locale/poland \\
        src/game/src/playerbot_weapon_atlas.h

Pulapki pliku spawnu sa te same co w tools/analyse_map_spawns.py: w group.txt
potwor jest ostatnim polem, w group_group.txt grupa jest polem srodkowym, a
wlasny group.txt mapy ma pierwszenstwo przed globalnym.
"""
import collections
import io
import os
import re
import sys

# Mapy, na ktorych AI poluje: wioski, mapy gildii, Lochy Malp i pogranicze
# (playerbot_types.h: PLAYERBOT_MAP_* i GetPlayerBotFrontierMapForLevel).
# Pozostale hostowane mapy (69, 70, 73, 79, 216, 217) istnieja, ale bot
# nie ma do nich drogi, wiec bron stamtad jest dla atlasu "gdzie indziej".
BOT_MAPS = [1, 3, 4, 5, 21, 23, 24, 25, 41, 43, 44, 45, 108, 109,
            61, 63, 64, 65, 66, 67, 68, 71, 104]
# Kolejnosc bitow klasy w atlasie i antyflagi item_proto, ktore je wykluczaja.
CLASS_BITS = [(0x01, 4), (0x02, 8), (0x04, 16), (0x08, 32)]   # wojownik, ninja, sura, szaman
WEAPON_ARROW = 6
MOB_TYPE_MONSTER, MOB_TYPE_STONE = 0, 2

SOURCE_MERCHANT = 0x01   # sprzedaje kupiec stojacy na mapie botow
SOURCE_COMMON = 0x02     # wspolny drop rangi i przedzialu poziomu, a takie potwory sa na mapach botow
SOURCE_DROP = 0x04       # konkretny potwor na mapie botow
SOURCE_CHEST = 0x08      # skrzynia, ktora wypada z potwora na mapie botow
SOURCE_STONE = 0x10      # ten potwor to kamien Metin
SOURCE_ELSEWHERE = 0x20  # wypada tylko tam, gdzie boty nie chodza

SOURCE_NAMES = [(SOURCE_MERCHANT, 'kupiec'), (SOURCE_COMMON, 'wspolny'), (SOURCE_DROP, 'potwor'),
                (SOURCE_CHEST, 'skrzynia'), (SOURCE_STONE, 'metin'), (SOURCE_ELSEWHERE, 'poza')]


def rd(path, enc='cp1250'):
    return io.open(path, encoding=enc, errors='replace')


def ascii_name(hexname):
    raw = bytes.fromhex(hexname).decode('cp1250', 'replace')
    table = str.maketrans('ąćęłńóśźżĄĆĘŁŃÓŚŹŻ', 'acelnoszzACELNOSZZ')
    return re.sub(r'[^\x20-\x7e]', '?', raw.translate(table)).replace('"', "'")


def load_items(path):
    items = {}
    for line in rd(path, 'utf-8'):
        c = line.rstrip('\r\n').split('\t')
        if len(c) < 11 or not c[0].isdigit():
            continue
        items[int(c[0])] = dict(name=ascii_name(c[1]), type=int(c[2]), sub=int(c[3]), level=int(c[4]),
                                anti=int(c[5]), magic=(int(c[6]), int(c[7])), atk=(int(c[8]), int(c[9])),
                                plus=int(c[10]))
    return items


def load_mobs(path):
    mobs = {}
    for line in rd(path, 'utf-8'):
        c = line.rstrip('\r\n').split('\t')
        if len(c) < 5 or not c[0].isdigit():
            continue
        try:
            mobs[int(c[0])] = dict(name=ascii_name(c[1]), level=int(c[2]), rank=int(c[3]), type=int(c[4]))
        except ValueError:
            continue
    return mobs


def groups(path, key):
    """Bloki Group { Mob|Vnum N; Type T; idx vnum count prob } jako slowniki."""
    out, cur = [], None
    for line in rd(path):
        t = line.split('--')[0].strip()
        if not t:
            continue
        if t.lower().startswith('group'):
            cur = dict(id=None, type='', rows=[])
            continue
        if t == '{':
            continue
        if t == '}':
            if cur is not None and cur['id'] is not None:
                out.append(cur)
            cur = None
            continue
        if cur is None:
            continue
        p = t.split()
        if p[0].lower() == key.lower() and len(p) > 1 and p[1].isdigit():
            cur['id'] = int(p[1])
        elif p[0].lower() == 'type' and len(p) > 1:
            cur['type'] = p[1].lower()
        elif len(p) >= 4 and p[0].isdigit() and p[1].isdigit():
            try:
                cur['rows'].append((int(p[1]), int(p[2]), float(p[3])))
            except ValueError:
                pass
    return out


def blocks(path, middle):
    out = collections.defaultdict(set)
    if not os.path.exists(path):
        return out
    vnum, members = None, set()
    for line in rd(path, 'cp949'):
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


def map_spawns(locale, index, maps):
    """mob -> Counter(map -> punkty spawnu), npc -> set(map) dla podanych map."""
    g_groups = blocks(os.path.join(locale, 'group.txt'), False)
    g_super = blocks(os.path.join(locale, 'group_group.txt'), True)
    spawns = collections.defaultdict(collections.Counter)
    npcs = collections.defaultdict(set)
    for mi in maps:
        folder = index.get(mi)
        d = os.path.join(locale, 'map', folder) if folder else None
        if not d or not os.path.isdir(d):
            continue
        local_groups = dict(g_groups)
        local_groups.update(blocks(os.path.join(d, 'group.txt'), False))
        local_super = dict(g_super)
        local_super.update(blocks(os.path.join(d, 'group_group.txt'), True))
        for fn in sorted(os.listdir(d)):
            is_npc = fn == 'npc.txt'
            if not (is_npc or fn in ('regen.txt', 'boss.txt', 'stone.txt') or re.match(r'base.*regen\.txt$', fn)):
                continue
            for line in rd(os.path.join(d, fn), 'cp949'):
                t = line.strip()
                if not t or t.startswith('#') or t.startswith('//'):
                    continue
                parts = t.split()
                if len(parts) < 11:
                    continue
                kind = parts[0].lower()[0]
                try:
                    vid = int(parts[-1])
                except ValueError:
                    continue
                if is_npc:
                    if kind == 'm':
                        npcs[vid].add(mi)
                    continue
                if kind in ('m', 'b', 'e', 's'):
                    spawns[vid][mi] += 1
                elif kind == 'g':
                    for mob in local_groups.get(vid, ()):
                        spawns[mob][mi] += 1
                elif kind == 'r':
                    for grp in local_super.get(vid, ()):
                        for mob in local_groups.get(grp, ()):
                            spawns[mob][mi] += 1
    return spawns, npcs


def main(argv):
    if len(argv) != 6:
        print(__doc__)
        return 2
    items = load_items(argv[1])
    mobs = load_mobs(argv[2])
    shop_rows = []
    for line in rd(argv[3], 'utf-8'):
        c = line.split()
        if len(c) >= 2 and c[0].isdigit() and c[1].isdigit():
            shop_rows.append((int(c[0]), int(c[1])))
    locale = argv[4]
    out_path = argv[5]

    index = {}
    for line in rd(os.path.join(locale, 'map', 'index')):
        p = line.split()
        if len(p) >= 2 and p[0].isdigit():
            index[int(p[0])] = p[1]
    all_maps = sorted(index)
    spawns, npcs = map_spawns(locale, index, all_maps)
    bot_maps = set(BOT_MAPS)

    def on_bot_maps(mob):
        return {m: n for m, n in spawns.get(mob, {}).items() if m in bot_maps}

    def anywhere(mob):
        return bool(spawns.get(mob))

    merchant_items = set(v for npc, v in shop_rows if npcs.get(npc, set()) & bot_maps)
    # Rangi i poziomy potworow stojacych na mapach botow, dla wspolnego dropu.
    bot_ranks = collections.defaultdict(set)
    for mob, where in spawns.items():
        m = mobs.get(mob)
        if m and m['type'] == MOB_TYPE_MONSTER and any(x in bot_maps for x in where):
            bot_ranks[m['rank']].add(m['level'])

    mob_groups = groups(os.path.join(locale, 'mob_drop_item.txt'), 'Mob')
    chest_groups = groups(os.path.join(locale, 'special_item_group.txt'), 'Vnum')
    dropped_by = collections.defaultdict(list)   # vnum -> [mob]
    for g in mob_groups:
        for vnum, _cnt, _prob in g['rows']:
            dropped_by[vnum].append(g['id'])
    in_chest = collections.defaultdict(list)     # vnum -> [chest vnum]
    for g in chest_groups:
        for vnum, _cnt, _prob in g['rows']:
            in_chest[vnum].append(g['id'])
    common = collections.defaultdict(list)       # vnum -> [(rank, from, to)]
    for n, line in enumerate(rd(os.path.join(locale, 'common_drop_item.txt'))):
        if n == 0:
            continue
        cols = line.rstrip('\r\n').split('\t')
        for rank in range(4):
            c = cols[rank * 6: rank * 6 + 6]
            if len(c) < 5:
                continue
            try:
                common[int(c[4])].append((rank, int(c[1]), int(c[2])))
            except ValueError:
                continue

    rows = []
    for base in sorted(v for v, it in items.items() if it['type'] == 1 and v % 10 == 0):
        it = items[base]
        if it['sub'] == WEAPON_ARROW or (it['atk'] == (0, 0) and it['magic'] == (0, 0)):
            continue
        mask = 0
        for bit, anti in CLASS_BITS:
            if not it['anti'] & anti:
                mask |= bit
        if mask == 0:
            continue
        sources = 0
        best = None   # (score, mob, map)
        for r in range(10):
            v = base + r
            if v not in items:
                continue
            if v in merchant_items:
                sources |= SOURCE_MERCHANT
            for rank, lf, lt in common.get(v, ()):
                if any(lf <= lvl <= lt for lvl in bot_ranks.get(rank, ())):
                    sources |= SOURCE_COMMON
            for mob in dropped_by.get(v, ()):
                where = on_bot_maps(mob)
                m = mobs.get(mob, {})
                if where:
                    sources |= SOURCE_DROP
                    if m.get('type') == MOB_TYPE_STONE:
                        sources |= SOURCE_STONE
                    mp, pts = max(where.items(), key=lambda kv: kv[1])
                    score = (pts, -abs(m.get('level', 0) - it['level']))
                    if best is None or score > best[0]:
                        best = (score, mob, mp)
                elif anywhere(mob):
                    sources |= SOURCE_ELSEWHERE
            for chest in in_chest.get(v, ()):
                for mob in dropped_by.get(chest, ()):
                    if on_bot_maps(mob):
                        sources |= SOURCE_CHEST
                        if mobs.get(mob, {}).get('type') == MOB_TYPE_STONE:
                            sources |= SOURCE_STONE
                    elif anywhere(mob):
                        sources |= SOURCE_ELSEWHERE
        rows.append((base, it, mask, sources, best))

    lines = []
    lines.append('// playerbot_weapon_atlas.h - generated by tools/generate_weapon_atlas.py.')
    lines.append('// Every weapon family of this world (world.item_proto, +0 vnums), who may')
    lines.append('// carry it (the anti-flags), and where a bot can get one: a village merchant')
    lines.append('// on a map the bots walk, the common drop of a monster rank and level band')
    lines.append('// that stands on such a map, a monster or a chest there, or only on maps no')
    lines.append('// bot goes to. Read with world.mob_proto, world.shop and the spawn files of')
    lines.append('// every map. Do not edit by hand; run the generator.')
    lines.append('#ifndef PLAYERBOT_WEAPON_ATLAS_H')
    lines.append('#define PLAYERBOT_WEAPON_ATLAS_H')
    lines.append('')
    lines.append('namespace')
    lines.append('{')
    for flag, name in SOURCE_NAMES:
        const = {'kupiec': 'MERCHANT', 'wspolny': 'COMMON', 'potwor': 'DROP', 'skrzynia': 'CHEST',
                 'metin': 'STONE', 'poza': 'ELSEWHERE'}[name]
        lines.append('\tconst WORD PLAYERBOT_WEAPON_SOURCE_%s = 0x%02x;' % (const, flag))
    lines.append('\tconst BYTE PLAYERBOT_WEAPON_CLASS_WARRIOR = 0x01;')
    lines.append('\tconst BYTE PLAYERBOT_WEAPON_CLASS_ASSASSIN = 0x02;')
    lines.append('\tconst BYTE PLAYERBOT_WEAPON_CLASS_SURA = 0x04;')
    lines.append('\tconst BYTE PLAYERBOT_WEAPON_CLASS_SHAMAN = 0x08;')
    lines.append('')
    lines.append('\tstruct TPlayerBotWeaponFamily')
    lines.append('\t{')
    lines.append('\t\tDWORD dwBaseVnum;   // the +0 of the family')
    lines.append('\t\tBYTE bSubType;')
    lines.append('\t\tBYTE bLevel;')
    lines.append('\t\tBYTE bClassMask;    // PLAYERBOT_WEAPON_CLASS_*')
    lines.append('\t\tWORD wSources;      // PLAYERBOT_WEAPON_SOURCE_*')
    lines.append('\t\tDWORD dwSourceMob;  // the monster on a bot map with the most spawn points, 0 if none')
    lines.append('\t\tWORD wSourceMap;    // where that monster stands, 0 if none')
    lines.append('\t};')
    lines.append('')
    lines.append('\tconst TPlayerBotWeaponFamily PLAYERBOT_WEAPON_ATLAS[] = {')
    reachable = 0
    for base, it, mask, sources, best in rows:
        mob, mp = (best[1], best[2]) if best else (0, 0)
        src = ','.join(name for flag, name in SOURCE_NAMES if sources & flag) or 'brak'
        if sources & (SOURCE_MERCHANT | SOURCE_COMMON | SOURCE_DROP | SOURCE_CHEST):
            reachable += 1
        lines.append('\t\t{ %5d, %d, %3d, 0x%02x, 0x%02x, %5d, %3d },\t// %s (%s)' % (
            base, it['sub'], it['level'], mask, sources, mob, mp, it['name'], src))
    lines.append('\t};')
    lines.append('}')
    lines.append('')
    lines.append('#endif')
    with io.open(out_path, 'w', encoding='ascii', newline='\n') as f:
        f.write('\n'.join(lines) + '\n')
    print('rodzin: %d, do zdobycia na mapach botow: %d -> %s' % (len(rows), reachable, out_path))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
