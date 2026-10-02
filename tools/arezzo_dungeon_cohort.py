#!/usr/bin/env python3
"""arezzo_dungeon_cohort.py - picks and equips the Arezzo dungeon test cohort
(MT2009_PLUS_AREZZO_DUNGEON_BOTS_V1, playerbot_arezzo_dungeon_bots.h) on the
test server: 30 bots for each of the three Arezzo dungeons, which run them
start to finish in a loop.

    arezzo_dungeon_cohort.py <outdir> [--online FILE] [--per-dungeon 30]

Reads the test world's database (read-only, through `docker exec
mt2009plustest-db mariadb`) and writes to <outdir>:

  pick.tsv                              who (pid, name, class, group, kingdom,
                                        level before/after, dungeon);
  pids.txt                              the pids;
  dg_apply.sql                          the change (idempotent, see below);
  dg_backup_and_apply.sh                backs the rows up once, then applies;
  dg_revert.sh                          puts the rows back and drops the pick;
  playerbot_arezzo_dungeon_cohort.txt   the core's cohort file ("<key> <pid>").

The dungeons live on game2 of channel 1 (MAP_ALLOW 364 365 366) and a bot
cannot cross cores, so the cohort lives there: each character's save point
is moved into its dungeon's lobby (the dungeon map itself, beside its guard),
and game2 logs them in from the cohort file. game1 refuses the same pids (the
cohort file is copied into its directory too), so no character is ever
loaded twice.

Who: existing bot identities of the registry (playerbot_* accounts) assigned
to channel 1 and not seen for three hours (or never), not online on game1
(--online: the pids in its playerbot_status.tsv), past the first 600 of their
kingdom's registry (the population's part), not in the Arezzo map cohorts
(player.arezzo_test_cohort) nor the Ochao test group, no companion, shouter,
takeover or retirement row, no guild master and keeping no offline shop (bots
without a guild first: the dungeon pass claims the tick ahead of the guild
war, so a member's guild never calls it away), with a profession chosen - and a ninja only of the dagger path (no arrows to keep).
A never-played identity (level 1) is raised as well and given a
profession (skill_group) the gear is for. Per dungeon ten of each kingdom (a party is one kingdom: the Shaman's buffs
reach only its own), 3/2/3/2 warrior/ninja/sura/shaman a kingdom, the highest
levels of the band first. Raised, never lowered.

  wukong   (364, Wzgorze Wukonga, from 45; monsters 44-55):   level 50
  skorpion (365, Ruiny Skorpiona, from 65; monsters 66-72):   level 70
  dzungla  (366, Starozytna Dzungla, from 95; monsters 97-106): level 100

What: level, experience 0, the stat and skill points the levels bring (the
bots spend them themselves), the average hit points, 30 000 000 yang (the
potions are bought in the lobby out of it), the save point in the lobby, and
nine pieces of class gear +9 of the level with sockets and bonuses (what was
worn before is deleted - the backup keeps it). Run with the game stopped.
"""
import sys, os, argparse, collections, subprocess

ap = argparse.ArgumentParser()
ap.add_argument('outdir')
ap.add_argument('--online', help='file of pids online now (first column, header ignored)')
ap.add_argument('--ochao', default='/opt/metin2/cache/backup-ochao-test/test_bots_150.txt')
ap.add_argument('--per-dungeon', type=int, default=30)
ap.add_argument('--registry-skip', type=int, default=600)
a = ap.parse_args()
os.makedirs(a.outdir, exist_ok=True)


def q(sql):
    r = subprocess.run(['docker', 'exec', '-i', 'mt2009plustest-db', 'sh', '-c',
                        'mariadb -uroot -p"$MARIADB_ROOT_PASSWORD" -N -B --default-character-set=latin1'],
                       input=sql.encode('latin-1'), capture_output=True)
    if r.returncode:
        sys.exit(r.stderr.decode(errors='replace'))
    return [l.split('\t') for l in r.stdout.decode('latin-1').splitlines() if l]


online = set()
if a.online and os.path.exists(a.online):
    for l in open(a.online, errors='replace'):
        f = l.split('\t')[0].strip()
        if f.isdigit():
            online.add(int(f))
ochao = set()
if a.ochao and os.path.exists(a.ochao):
    ochao = set(int(l.split()[0]) for l in open(a.ochao) if l.strip() and l.split()[0].isdigit())

# key, map, target level, level band, lobby (base cell + the guard's entry cell, as the quests' cfg)
DUNGEONS = [
    ('wukong', 364, 50, (40, 50), (8448 + 264, 5376 + 273)),
    ('skorpion', 365, 70, (55, 70), (8448 + 268, 5888 + 228)),
    ('dzungla', 366, 100, (80, 100), (7680 + 384, 5376 + 374)),
]

rows = q("""
SELECT p.id, p.name, p.job % 4, pi.empire, p.level, p.skill_group,
       (SELECT COUNT(*) FROM player.guild_member gm WHERE gm.pid = p.id)
  FROM common.playerbot_seed_state l
  JOIN player.player p ON p.id = l.pid
  JOIN account.account ac ON ac.id = p.account_id
  JOIN player.player_index pi ON pi.id = ac.id
  JOIN common.playerbot_channel_assignment ca ON ca.pid = p.id
 WHERE l.state IN ('complete', 'adopted') AND LEFT(ac.login, 10) = 'playerbot_'
   AND pi.empire IN (1, 2, 3) AND pi.pid1 = p.id
   AND ca.channel = 1 AND ca.requested_channel IN (0, 1)
   AND (ca.last_seen IS NULL OR ca.last_seen < NOW() - INTERVAL 3 HOUR)
   AND p.id NOT IN (SELECT sidekick_pid FROM player.playerbot_sidekick)
   AND p.id NOT IN (SELECT pid FROM player.playerbot_shouter)
   AND p.id NOT IN (SELECT pid FROM common.playerbot_takeover)
   AND p.id NOT IN (SELECT pid FROM common.playerbot_retire_pick)
   AND p.id NOT IN (SELECT master FROM player.guild)
   AND p.id NOT IN (SELECT DISTINCT owner FROM player.ikashop_offlineshop)
 ORDER BY pi.empire, p.id;
""")
allreg = q("""
SELECT p.id, pi.empire FROM common.playerbot_seed_state l JOIN player.player p ON p.id = l.pid
  JOIN account.account ac ON ac.id = p.account_id JOIN player.player_index pi ON pi.id = ac.id
 WHERE LEFT(ac.login, 10) = 'playerbot_' AND pi.empire IN (1, 2, 3) ORDER BY pi.empire, p.id;
""")
rank = {}
seen = collections.Counter()
for r in allreg:
    e = int(r[1]); seen[e] += 1; rank[int(r[0])] = seen[e]
excluded = set()
try:
    excluded = set(int(r[0]) for r in q("SELECT pid FROM player.arezzo_test_cohort;"))
except SystemExit:
    pass
earlier = {}
try:
    for r in q("SELECT pid, dungeon FROM player.arezzo_dungeon_cohort;"):
        earlier[int(r[0])] = r[1]
except SystemExit:
    pass

cands = collections.defaultdict(list)   # (key, kingdom, class) -> [(rank tuple, pid, name, grp, lvl)]
for r in rows:
    pid, name, cls, emp, lvl, grp, inguild = int(r[0]), r[1], int(r[2]), int(r[3]), int(r[4]), int(r[5]), int(r[6])
    if pid in earlier or pid in online or pid in ochao or pid in excluded or rank.get(pid, 0) <= a.registry_skip:
        continue
    if grp not in (0, 1, 2) or (cls == 1 and grp == 2):
        continue
    for key, mp, target, (lo, hi), lobby in DUNGEONS:
        # Played characters of the band first, then the never-played identities
        # past the population (level 1, no profession: given one below).
        if lo <= lvl <= hi:
            cands[(key, emp, cls)].append(((0, inguild, -lvl), pid, name, grp, lvl))
        elif lvl < lo and grp == 0:
            cands[(key, emp, cls)].append(((1, inguild, pid), pid, name, grp, lvl))
pick = []
if earlier:
    for r in q("SELECT c.pid, p.name, p.job % 4, pi.empire, c.old_level, c.target_level, c.dungeon, p.skill_group "
               "FROM player.arezzo_dungeon_cohort c JOIN player.player p ON p.id = c.pid "
               "JOIN player.player_index pi ON pi.id = p.account_id;"):
        pick.append(dict(pid=int(r[0]), name=r[1], cls=int(r[2]), emp=int(r[3]), old=int(r[4]), lvl=int(r[5]),
                         key=r[6], grp=int(r[7]), oldgrp=int(r[7])))
used = set(p['pid'] for p in pick)
CLASS_QUOTA = [3, 2, 3, 2]


def take(key, target, emp, want):
    """Up to `want` more of the kingdom for the dungeon, the class quota first."""
    took = collections.Counter(p['cls'] for p in pick if p['key'] == key and p['emp'] == emp)
    total = sum(took.values()) + want
    quota = [total * q // 10 for q in CLASS_QUOTA]
    k = 0
    while sum(quota) < total:
        quota[(0, 2, 1, 3)[k % 4]] += 1
        k += 1
    got = 0
    order = []
    for cls in range(4):
        for t in sorted(cands[(key, emp, cls)]):
            order.append((cls, t))
    for pass_ in (0, 1):
        for cls, t in order:
            if got >= want:
                break
            if t[1] in used or (pass_ == 0 and took[cls] >= quota[cls]):
                continue
            used.add(t[1]); took[cls] += 1; got += 1
            grp = t[3]
            if grp == 0:
                # a profession for the never-played: the ninja the dagger, the
                # others the two paths in turn
                grp = 1 if cls == 1 else 1 + (took[cls] % 2)
            pick.append(dict(pid=t[1], name=t[2], cls=cls, emp=emp, old=t[4], lvl=target, key=key, grp=grp, oldgrp=t[3]))
    return got


# Ten of each kingdom; a kingdom with nobody left over gives its share to the
# others in fives (a party is one kingdom).
for key, mp, target, band, lobby in DUNGEONS:
    have = collections.Counter(p['emp'] for p in pick if p['key'] == key)
    short = 0
    for emp in (1, 2, 3):
        want = a.per_dungeon // 3 - have[emp]
        if want > 0:
            short += want - take(key, target, emp, want)
    for emp in (1, 2, 3):
        while short >= 5 and take(key, target, emp, 5) == 5:
            short -= 5
    if short:
        print('WARNING: %s: %d short' % (key, short))

# ---- the gear: bonuses as POINT_* numbers (playerbot_engine_compat.h)
P_MAX_HP, P_ST, P_HT, P_DX, P_IQ = 6, 12, 13, 14, 15
P_ATT_SPEED, P_MOV_SPEED, P_HP_REGEN, P_POISON, P_STUN = 17, 19, 32, 37, 38
P_CRIT, P_PEN, P_HUMAN, P_ANIMAL, P_DEVIL = 40, 41, 43, 44, 48
P_STEAL_HP, P_BLOCK, P_DODGE, P_RES_SWORD, P_RES_BOW, P_RES_MAGIC = 63, 67, 68, 69, 74, 77
P_REFLECT, P_POISON_REDUCE, P_IMMUNE_STUN, P_AVG = 79, 81, 88, 122
# (class, group) -> weapon; the classes warrior, ninja, sura, shaman (ninja only of group 1)
WEAPON = {
    'wukong':   {(0, 1): 119, (0, 2): 3109, (1, 1): 1079, (2, 1): 119, (2, 2): 119, (3, 1): 7109, (3, 2): 5069},
    'skorpion': {(0, 1): 179, (0, 2): 3159, (1, 1): 1129, (2, 1): 259, (2, 2): 259, (3, 1): 7159, (3, 2): 5139},
    'dzungla':  {(0, 1): 279, (0, 2): 3169, (1, 1): 4049, (2, 1): 289, (2, 2): 289, (3, 1): 7199, (3, 2): 5339},
}
ARMOUR = {'wukong': [11259, 11459, 11659, 11859], 'skorpion': [11299, 11499, 11699, 11899],
          'dzungla': [12019, 12029, 12039, 12049]}
HELMET = {'wukong': [12249, 12389, 12529, 12669], 'skorpion': [12269, 12399, 12539, 12679],
          'dzungla': [12289, 12409, 12549, 12689]}
SHIELD = {'wukong': 13049, 'skorpion': 13069, 'dzungla': 13149}
WRIST = {'wukong': 14149, 'skorpion': 14229, 'dzungla': 14549}
FOOTS = {'wukong': 15149, 'skorpion': 15229, 'dzungla': 15419}
NECK = {'wukong': 16149, 'skorpion': 16229, 'dzungla': 16549}
EAR = {'wukong': 17149, 'skorpion': 17229, 'dzungla': 17549}
HP = {'wukong': 1500, 'skorpion': 2000, 'dzungla': 2500}


def items(p):
    k = p['key']
    cls, grp = p['cls'], p['grp']
    stat = P_IQ if cls == 3 or (cls == 2 and grp == 2) else P_ST
    hp = HP[k]
    wsock = [28437, 28430, 28431]
    bsock = [28441, 28441, 28439]
    return [
        (0, ARMOUR[k][cls], 1, bsock, [(P_MAX_HP, hp), (P_RES_SWORD, 15), (P_RES_BOW, 15), (P_RES_MAGIC, 15), (P_DODGE, 10)]),
        (1, HELMET[k][cls], 1, [0, 0, 0], [(P_ATT_SPEED, 8), (P_HP_REGEN, 12), (P_RES_MAGIC, 15), (P_POISON, 8)]),
        (2, FOOTS[k], 1, [0, 0, 0], [(P_MAX_HP, hp), (P_MOV_SPEED, 8), (P_CRIT, 10), (P_STUN, 8)]),
        (3, WRIST[k], 1, [0, 0, 0], [(P_MAX_HP, hp), (P_STEAL_HP, 10), (P_PEN, 10)]),
        (4, WEAPON[k][(cls, grp)], 1, wsock, [(P_AVG, 45), (P_CRIT, 10), (stat, 12), (P_PEN, 10), (P_DEVIL, 20)]),
        (5, NECK[k], 1, [0, 0, 0], [(P_MAX_HP, hp), (P_CRIT, 10), (P_PEN, 10), (P_STEAL_HP, 10)]),
        (6, EAR[k], 1, [0, 0, 0], [(P_ANIMAL, 20), (P_DEVIL, 20), (P_MOV_SPEED, 8), (P_POISON_REDUCE, 5)]),
        (10, SHIELD[k], 1, [0, 0, 0], [(P_HT, 12), (P_BLOCK, 15), (P_REFLECT, 10), (P_IMMUNE_STUN, 1)]),
    ]


lobby = dict((key, l) for key, mp, target, band, l in DUNGEONS)
mapof = dict((key, mp) for key, mp, target, band, l in DUNGEONS)
pids = sorted(p['pid'] for p in pick)
plist = ','.join(str(x) for x in pids)
S = []
S.append('-- MT2009_PLUS_AREZZO_DUNGEON_BOTS_V1: the Arezzo dungeon test cohort (tools/arezzo_dungeon_cohort.py).')
S.append('-- %d characters. Run with the game stopped.' % len(pick))
S.append('SET NAMES latin1;')
S.append('CREATE TABLE IF NOT EXISTS player.arezzo_dungeon_cohort (pid INT UNSIGNED NOT NULL PRIMARY KEY, '
         'dungeon VARCHAR(16) NOT NULL, target_level TINYINT UNSIGNED NOT NULL, old_level TINYINT UNSIGNED NOT NULL, '
         'picked_at DATETIME NOT NULL, applied_at DATETIME NULL) ENGINE=InnoDB;')
S.append('START TRANSACTION;')
S.append('INSERT IGNORE INTO player.arezzo_dungeon_cohort (pid, dungeon, target_level, old_level, picked_at) VALUES')
S.append(',\n'.join("  (%d, '%s', %d, %d, NOW())" % (p['pid'], p['key'], p['lvl'], p['old']) for p in pick) + ';')
S.append("""UPDATE player.player p JOIN player.arezzo_dungeon_cohort c ON c.pid = p.id AND c.applied_at IS NULL
   SET p.stat_point = GREATEST(p.stat_point, LEAST(3 * (c.target_level - 1), 344) - (p.st + p.ht + p.dx + p.iq - 16)),
       p.skill_point = p.skill_point + GREATEST(0, c.target_level - GREATEST(p.level, 4)),
       p.sub_skill_point = p.sub_skill_point + GREATEST(0, c.target_level - GREATEST(p.level, 8)),
       p.random_hp = GREATEST(p.random_hp, 40 * (c.target_level - 1)),
       p.random_sp = GREATEST(p.random_sp, 20 * (c.target_level - 1)),
       p.gold = GREATEST(p.gold, 30000000),
       p.exp = 0, p.level_step = 0,
       p.level = GREATEST(p.level, c.target_level);""")
# The save point (and the exit point) in the dungeon's lobby, on game2 - every time, not only the first:
# a character that went home in between is put back.
for key in ('wukong', 'skorpion', 'dzungla'):
    x, y = lobby[key]
    S.append("""UPDATE player.player p JOIN player.arezzo_dungeon_cohort c ON c.pid = p.id AND c.dungeon = '%s'
   SET p.map_index = %d, p.x = %d, p.y = %d, p.exit_map_index = %d, p.exit_x = %d, p.exit_y = %d;""" % (
        key, mapof[key], x * 100, y * 100, mapof[key], x * 100, y * 100))
for p in pick:
    if p['oldgrp'] == 0:
        S.append('UPDATE player.player SET skill_group = %d WHERE id = %d AND skill_group = 0;' % (p['grp'], p['pid']))
S.append('UPDATE player.arezzo_dungeon_cohort SET applied_at = NOW() WHERE applied_at IS NULL;')
S.append("DELETE FROM player.item WHERE window = 'EQUIPMENT' AND pos IN (0,1,2,3,4,5,6,9,10) AND owner_id IN (%s);" % plist)
ins = []
for p in sorted(pick, key=lambda x: x['pid']):
    for pos, vnum, count, sock, attrs in items(p):
        at = list(attrs) + [(0, 0)] * (7 - len(attrs))
        ins.append("  (%d, 'EQUIPMENT', %d, %d, %d, %d, %d, %d, %s)" % (
            p['pid'], pos, count, vnum, sock[0], sock[1], sock[2], ', '.join('%d, %d' % t for t in at)))
S.append('INSERT INTO player.item (owner_id, window, pos, count, vnum, socket0, socket1, socket2, '
         'attrtype0, attrvalue0, attrtype1, attrvalue1, attrtype2, attrvalue2, attrtype3, attrvalue3, '
         'attrtype4, attrvalue4, attrtype5, attrvalue5, attrtype6, attrvalue6) VALUES\n' + ',\n'.join(ins) + ';')
S.append('COMMIT;')
S.append('SELECT c.dungeon, p.level, p.map_index, COUNT(*) AS bots FROM player.arezzo_dungeon_cohort c '
         'JOIN player.player p ON p.id = c.pid GROUP BY c.dungeon, p.level, p.map_index;')
open(os.path.join(a.outdir, 'dg_apply.sql'), 'w', encoding='latin-1').write('\n'.join(S) + '\n')
with open(os.path.join(a.outdir, 'pick.tsv'), 'w', encoding='latin-1') as f:
    f.write('pid\tname\tclass\tgroup\tkingdom\tlevel_before\tlevel_after\tdungeon\n')
    for p in sorted(pick, key=lambda x: (x['key'], x['emp'], x['cls'], x['pid'])):
        f.write('%d\t%s\t%d\t%d\t%d\t%d\t%d\t%s\n' % (p['pid'], p['name'], p['cls'], p['grp'], p['emp'], p['old'], p['lvl'], p['key']))
with open(os.path.join(a.outdir, 'playerbot_arezzo_dungeon_cohort.txt'), 'w') as f:
    f.write('# MT2009_PLUS_AREZZO_DUNGEON_BOTS_V1: "<dungeon> <pid>" - wukong (364), skorpion (365), dzungla (366)\n')
    for p in sorted(pick, key=lambda x: (x['key'], x['pid'])):
        f.write('%s %d\n' % (p['key'], p['pid']))
open(os.path.join(a.outdir, 'pids.txt'), 'w').write(' '.join(str(x) for x in pids) + '\n')
open(os.path.join(a.outdir, 'dg_backup_and_apply.sh'), 'w').write('''#!/bin/sh
# MT2009_PLUS_AREZZO_DUNGEON_BOTS_V1: back the picked characters up (once), then apply dg_apply.sql.
# Run with the test game stopped (docker compose stop game), the db up.
set -eu
cd "$(dirname "$0")"
B=${B:-/opt/metin2/cache/arezzo-test/dg_backup_before.sql}
PIDS=$(tr ' ' ',' < pids.txt | sed 's/,*$//')
DB="docker exec -i mt2009plustest-db sh -c"
if [ ! -s "$B" ]; then
  $DB "mariadb-dump -uroot -p\\"\\$MARIADB_ROOT_PASSWORD\\" --no-create-info --replace --hex-blob player player --where='id IN ($PIDS)'; mariadb-dump -uroot -p\\"\\$MARIADB_ROOT_PASSWORD\\" --no-create-info --replace --hex-blob player item --where='owner_id IN ($PIDS) AND window=\\"EQUIPMENT\\"'" > "$B.tmp"
  { echo "USE player;"; echo "DELETE FROM item WHERE owner_id IN ($PIDS) AND window='EQUIPMENT';"; cat "$B.tmp"; } > "$B"
  rm -f "$B.tmp"
  echo "backup: $(grep -c 'REPLACE INTO' "$B") statements in $B"
else
  echo "$B already there - kept (the state before the first apply)"
fi
$DB 'mariadb -uroot -p"$MARIADB_ROOT_PASSWORD"' < dg_apply.sql
''')
open(os.path.join(a.outdir, 'dg_revert.sh'), 'w').write('''#!/bin/sh
# MT2009_PLUS_AREZZO_DUNGEON_BOTS_V1: undo the dungeon cohort. With the test game stopped:
#   1. remove playerbot_arezzo_dungeon_cohort.txt from channel1/game1 and channel1/game2;
#   2. run this (restores player.player and the worn items as they were, drops the pick table and
#      the experience lock the cohort carried - affect type 310 is AFFECT_EXP_BLOCK; the persona
#      pass would lift it anyway).
set -eu
cd "$(dirname "$0")"
B=${B:-/opt/metin2/cache/arezzo-test/dg_backup_before.sql}
PIDS=$(tr ' ' ',' < pids.txt | sed 's/,*$//')
DB="docker exec -i mt2009plustest-db sh -c"
$DB 'mariadb -uroot -p"$MARIADB_ROOT_PASSWORD"' < "$B"
echo "DELETE FROM player.affect WHERE dwPID IN ($PIDS) AND bType = 310; DROP TABLE IF EXISTS player.arezzo_dungeon_cohort;" | $DB 'mariadb -uroot -p"$MARIADB_ROOT_PASSWORD"'
''')
cnt = collections.Counter((p['key'], p['cls']) for p in pick)
emp = collections.Counter((p['key'], p['emp']) for p in pick)
print('picked', len(pick))
for key, mp, target, band, l in DUNGEONS:
    print(' ', key, 'classes', [cnt[(key, c)] for c in range(4)], 'kingdoms', [emp[(key, e)] for e in (1, 2, 3)],
          'level before', sorted(collections.Counter(p['old'] for p in pick if p['key'] == key).items()))
