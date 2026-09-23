# -*- coding: utf-8 -*-
"""Which monster race a map is actually made of, and whether it can be bought.

A "strong against orcs" line multiplies a bot's whole attack in Orc Valley and
does nothing at all on the desert, so the reroll pass and the gear scorer both
have to know what stands on the map before they can say what a line is worth.
Guessing produced two answers that disagreed: the gear scorer paid 600 a point
for a race line while the reroll scorer paid one, so a bot bought the shield for
the line and then rerolled it away.

Two engine facts decide the output and neither is negotiable:

* `battle.cpp CalcAttBonus` walks the races as an **else-if** chain - ANIMAL,
  UNDEAD, DEVIL, HUMAN, ORC, MILGYO, INSECT, FIRE, ICE, DESERT, TREE - so a
  monster contributes to exactly one of them, the first flag it carries.
* Only six of those eleven can be raised by an item at all. `char.cpp` maps
  APPLY_ATTBONUS_HUMAN/ANIMAL/ORC/MILGYO/UNDEAD/DEVIL onto their POINTs and
  nothing maps onto INSECT, FIRE, ICE, DESERT or TREE. So on a map of ice
  creatures or desert scorpions no race line is worth anything, whatever it
  says on the item.

Weighted by spawn points, because a hub is chosen by density: a map with one
orc and four hundred scorpions is a desert map.

    python tools/analyse_map_races.py
    python tools/analyse_map_races.py 64
"""
from __future__ import print_function

import collections
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import analyse_map_spawns as spawns  # noqa: E402

try:
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')
except AttributeError:
    pass

# battle.cpp's own order. The first flag a monster carries is the only one that
# pays; the rest of its flags are decoration.
RACE_ORDER = ['ANIMAL', 'UNDEAD', 'DEVIL', 'HUMAN', 'ORC', 'MILGYO',
              'INSECT', 'FIRE', 'ICE', 'DESERT', 'TREE']

# The six an item can actually carry. char.cpp has no APPLY for the others.
BUYABLE = set(['ANIMAL', 'UNDEAD', 'DEVIL', 'HUMAN', 'ORC', 'MILGYO'])

# The element a monster attacks with, and the resistance that answers it.
# `battle.cpp CalcMagicDamage` reads the attacker's ATT_ flag and takes the
# victim's matching POINT_RESIST_*; unlike the race chain a monster may carry
# none, and most do.
ELEMENTS = [
    ('ATT_ELEC', 'RESIST_ELEC'), ('ATT_FIRE', 'RESIST_FIRE'),
    ('ATT_ICE', 'RESIST_ICE'), ('ATT_WIND', 'RESIST_WIND'),
    ('ATT_EARTH', 'RESIST_EARTH'), ('ATT_DARK', 'RESIST_DARK'),
]

# Every map a bot is allowed to stand on, with the name our constants use.
MAPS = [
    (1, 'metin2_map_a1', 'Yongan (Shinsoo M1)'),
    (3, 'metin2_map_a3', 'Jayang (Shinsoo M2)'),
    (4, 'metin2_map_guild_01', 'Shinsoo M3'),
    (5, 'metin2_map_monkey_dungeon_11', 'Loch Malp latwy (Shinsoo)'),
    (21, 'metin2_map_b1', 'Joan (Chunjo M1)'),
    (23, 'metin2_map_b3', 'Bokjung (Chunjo M2)'),
    (24, 'metin2_map_guild_02', 'Chunjo M3'),
    (25, 'metin2_map_monkey_dungeon_12', 'Loch Malp latwy (Chunjo)'),
    (41, 'metin2_map_c1', 'Pyongmoo (Jinno M1)'),
    (43, 'metin2_map_c3', 'Bakra (Jinno M2)'),
    (44, 'metin2_map_guild_03', 'Jinno M3'),
    (45, 'metin2_map_monkey_dungeon_13', 'Loch Malp latwy (Jinno)'),
    (61, 'map_n_snowm_01', 'Gora Sohan'),
    (63, 'metin2_map_n_desert_01', 'Pustynia Yongbi'),
    (64, 'map_n_threeway', 'Dolina Orkow'),
    (65, 'metin2_map_milgyo', 'Swiatynia Hwang'),
    (71, 'metin2_map_spiderdungeon_02', 'Loch Pajakow V2'),
    (104, 'metin2_map_spiderdungeon', 'Loch Pajakow V1'),
    (108, 'metin2_map_monkey_dungeon2', 'Loch Malp sredni'),
    (109, 'metin2_map_monkey_dungeon3', 'Loch Malp trudny'),
]


def race_of(mob):
    """The one race that pays, or None.

    The official mob_proto carries five monsters whose flags read
    "FIR,ATT_FIREE" - a typo in the file itself - and those name no race at
    all. Reading the column as text rather than repairing it keeps this an
    account of what the engine will do, which for those five is nothing.
    """
    flags = set(f.strip() for f in (mob.get('race') or '').split(','))
    for r in RACE_ORDER:
        if r in flags:
            return r
    return None


def measure(folder, mobs):
    counts, elements = collections.Counter(), collections.Counter()
    battle = collections.Counter()
    for _kind, _cx, _cy, vnum in spawns.resolve(folder):
        mob = mobs.get(vnum)
        # Metin stones, NPCs, warps and doors are not monsters and pay no race
        # bonus; only the TYPE column says which is which.
        if not mob or mob.get('type') != 'MONSTER':
            continue
        counts[race_of(mob) or '-'] += 1
        battle[mob.get('battle') or '-'] += 1
        flags = set(f.strip() for f in (mob.get('race') or '').split(','))
        for att, resist in ELEMENTS:
            if att in flags:
                elements[resist] += 1
    return counts, elements, battle


def main(argv):
    mobs = spawns.load_mobs()
    wanted = set(int(a) for a in argv) if argv else None
    print('%-5s %-28s %7s  %s' % ('mapa', 'nazwa', 'spawnow', 'sklad (udzial %)'))
    for index, folder, label in MAPS:
        if wanted and index not in wanted:
            continue
        counts, elements, battle = measure(folder, mobs)
        total = sum(counts.values())
        if not total:
            print('%-5d %-28s %7d  -' % (index, label, 0))
            continue
        parts = []
        for race, n in counts.most_common():
            mark = '' if race in BUYABLE else '*'
            parts.append('%s%s %d%%' % (race, mark, round(100.0 * n / total)))
        top = [r for r, _ in counts.most_common() if r in BUYABLE]
        best = counts[top[0]] * 100 // total if top else 0
        print('%-5d %-28s %7d  %s' % (index, label, total, ', '.join(parts[:5])))
        print('%-5s %-28s %7s  -> %s' % (
            '', '', '',
            ('bonus na rase: %s, pokrywa %d%% mapy' % (top[0], best))
            if top and best >= 20 else
            'bonus na rase nic tu nie daje'))
        ranged = battle['RANGE'] + battle['MAGIC']
        print('%-5s %-28s %7s  -> dystansowe %d%% (tyle warte '
              'ODP. NA STRZALY i UNIK)' % ('', '', '', ranged * 100 // total))
        if elements:
            eparts = ['%s %d%%' % (r, round(100.0 * n / total))
                      for r, n in elements.most_common(3)]
            hit = elements.most_common(1)[0]
            print('%-5s %-28s %7s  -> zywiol: %s%s' % (
                '', '', '', ', '.join(eparts),
                '' if hit[1] * 100 // total >= 20 else ' (za malo, pomijamy)'))
    print('')
    print('* rasa, ktorej zaden przedmiot nie potrafi objac '
          '(brak APPLY w char.cpp)')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
