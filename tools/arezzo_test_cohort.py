#!/usr/bin/env python3
"""arezzo_test_cohort.py - picks and equips the Arezzo test cohorts
(MT2009_PLUS_AREZZO_BOTS_V1, playerbot_arezzo_bots.h) on the test server.

    arezzo_test_cohort.py <outdir> [--online FILE] [--ochao FILE] [--per-cohort 100]

Reads the test world's database (read-only, through
`docker exec mt2009plustest-db mariadb`) and writes to <outdir>:

  pick.tsv                    the chosen characters (pid, name, class, empire,
                              level before, level after, map);
  cohort_apply.sql            the change, idempotent (see below);
  cohort_revert_note.txt      how to undo it;
  playerbot_arezzo_cohort.txt the core's cohort file ("<map> <pid>" a line):
                              the two new cohorts and the Ochao test group
                              (for the Las).

Who: existing bot identities of the registry (playerbot_* accounts, seed
state complete/adopted) that the population does not bring in by itself -
not online now, and past the first 300 of their kingdom's registry, which is
the part the autospawn slider takes - with a profession already chosen, no
guild of their own to lead, no companion, shouter, takeover or retirement
row. Raised, never lowered: cohort A (level 45) from 30-45, cohort B (level
61) from 46-61; 25 of each class, the kingdoms taken in turn; bots without a
guild and without an offline shop first. They are logged in on top of the
population by the core (playerbot_arezzo_cohort.txt, ScheduleExtraBots).

What: level, experience 0, the stat and skill points the levels bring (the
bots spend them themselves: playerbot_skills.h), the hit points the levels
roll on average (random_hp/random_sp), gold for the fares and potions, the
save point moved to the kingdom's second village (a map the first channel's
game1 hosts), and eight pieces of class gear +9 with sockets and bonuses in
positions 0-6 and 10 (and arrows for an archer): what they wore there before
is deleted. The engine stores an item's bonus types as POINT_* numbers
(playerbot_engine_compat.h), which is what the rows below carry.

Idempotent: the pick goes into player.arezzo_test_cohort first, and the
character rows change only for the pids whose applied_at is still NULL; the
gear rows are deleted and put back each time. Run it with the game stopped
(the db core caches characters and would write its copy back).
"""
import sys, os, subprocess, argparse, collections

ap = argparse.ArgumentParser()
ap.add_argument('outdir')
ap.add_argument('--online', help='file of pids online now (one a line)')
ap.add_argument('--ochao', default='/opt/metin2/cache/backup-ochao-test/test_bots_150.txt',
                help='the Ochao test group (first column pid)')
ap.add_argument('--per-cohort', type=int, default=100)
ap.add_argument('--registry-skip', type=int, default=300)
ap.add_argument('--exclude-name', action='append', default=[],
                help='leave out characters whose name contains this (case-insensitive); repeatable')
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
if a.online:
    online = set(int(x) for x in open(a.online).read().split() if x.strip().isdigit())
ochao = []
if a.ochao and os.path.exists(a.ochao):
    ochao = [int(l.split()[0]) for l in open(a.ochao) if l.strip() and l.split()[0].isdigit()]

rows = q("""
SELECT p.id, p.name, p.job % 4, pi.empire, p.level, p.skill_group,
       (gm.pid IS NOT NULL), (s.owner IS NOT NULL), (g.master IS NOT NULL)
  FROM common.playerbot_seed_state l
  JOIN player.player p ON p.id = l.pid
  JOIN account.account ac ON ac.id = p.account_id
  JOIN player.player_index pi ON pi.id = ac.id
  LEFT JOIN player.guild_member gm ON gm.pid = p.id
  LEFT JOIN (SELECT DISTINCT owner FROM player.ikashop_offlineshop) s ON s.owner = p.id
  LEFT JOIN player.guild g ON g.master = p.id
 WHERE l.state IN ('complete', 'adopted') AND LEFT(ac.login, 10) = 'playerbot_'
   AND pi.empire IN (1, 2, 3) AND pi.pid1 = p.id
   AND p.id NOT IN (SELECT sidekick_pid FROM player.playerbot_sidekick)
   AND p.id NOT IN (SELECT pid FROM player.playerbot_shouter)
   AND p.id NOT IN (SELECT pid FROM common.playerbot_takeover)
   AND p.id NOT IN (SELECT pid FROM common.playerbot_retire_pick)
 ORDER BY pi.empire, p.id;
""")
# The registry's order per kingdom: the slider takes it from the front.
rank = {}
seen = collections.Counter()
for r in rows:
    e = int(r[3]); seen[e] += 1; rank[int(r[0])] = seen[e]
# Characters an earlier run already picked keep their place (idempotent pick).
earlier = {}
try:
    for r in q("SELECT pid, map, target_level FROM player.arezzo_test_cohort;"):
        earlier[int(r[0])] = (int(r[1]), int(r[2]))
except SystemExit:
    pass
cands = {'A': [], 'B': []}
for r in rows:
    pid, name, cls, emp, lvl, grp, inguild, shop, master = int(r[0]), r[1], int(r[2]), int(r[3]), int(r[4]), int(r[5]), int(r[6]), int(r[7]), int(r[8])
    if pid in earlier:
        continue
    if pid in online or pid in ochao or master or grp not in (1, 2) or rank[pid] <= a.registry_skip:
        continue
    if any(x.lower() in name.lower() for x in a.exclude_name):
        continue
    if 30 <= lvl <= 45: c = 'A'
    elif 46 <= lvl <= 61: c = 'B'
    else: continue
    cands[c].append((inguild + shop, -lvl, pid, name, cls, emp, lvl, grp, inguild, shop))
pick = []
if earlier:
    for r in q("SELECT c.pid, p.name, p.job % 4, pi.empire, c.old_level, c.target_level, c.map, p.skill_group FROM player.arezzo_test_cohort c JOIN player.player p ON p.id = c.pid JOIN player.player_index pi ON pi.id = p.account_id;"):
        pick.append(dict(pid=int(r[0]), name=r[1], cls=int(r[2]), emp=int(r[3]), old=int(r[4]), lvl=int(r[5]), map=int(r[6]), grp=int(r[7])))
for c, target, mp in (('A', 45, 360), ('B', 61, 361)):
    have = sum(1 for p in pick if p['map'] == mp)
    need = a.per_cohort - have
    if need <= 0:
        continue
    per_class = collections.defaultdict(lambda: collections.defaultdict(list))
    for t in sorted(cands[c]):
        per_class[t[4]][t[5]].append(t)
    quota = [need // 4 + (1 if i < need % 4 else 0) for i in range(4)]
    for cls in range(4):
        took = 0; e = 0; empties = 0
        while took < quota[cls] and empties < 3:
            emp = 1 + (e % 3); e += 1
            lst = per_class[cls][emp]
            if not lst:
                empties += 1; continue
            empties = 0
            t = lst.pop(0)
            pick.append(dict(pid=t[2], name=t[3], cls=cls, emp=emp, old=t[6], lvl=target, map=mp, grp=t[7]))
            took += 1
        if took < quota[cls]:
            print('WARNING: cohort %s class %d: only %d of %d' % (c, cls, took, quota[cls]))

# ---- the gear: (vnum by class/group), bonuses as POINT_* numbers
P_MAX_HP, P_ST, P_HT, P_DX, P_IQ = 6, 12, 13, 14, 15
P_ATT_SPEED, P_MOV_SPEED, P_HP_REGEN, P_POISON, P_STUN = 17, 19, 32, 37, 38
P_CRIT, P_PEN, P_HUMAN, P_ANIMAL, P_DEVIL = 40, 41, 43, 44, 48
P_STEAL_HP, P_BLOCK, P_DODGE, P_RES_SWORD, P_RES_BOW, P_RES_MAGIC = 63, 67, 68, 69, 74, 77
P_REFLECT, P_POISON_REDUCE, P_IMMUNE_STUN, P_AVG = 79, 81, 88, 122
def weapon(cls, grp, tier):
    # playerbot_gear.h IsPlayerBotWeaponSubTypeFor: warrior 1 sword, 2 two-handed or
    # sword; ninja 1 dagger/sword, 2 bow; sura sword; shaman bell or fan.
    A = {(0, 1): 299, (0, 2): 3219, (1, 1): 1179, (1, 2): 2159, (2, 1): 299, (2, 2): 299, (3, 1): 7169, (3, 2): 5119}
    B = {(0, 1): 129, (0, 2): 3119, (1, 1): 1089, (1, 2): 2119, (2, 1): 129, (2, 2): 129, (3, 1): 7119, (3, 2): 5079}
    return (A if tier == 'A' else B)[(cls, grp)]
ARMOUR = {'A': [11259, 11459, 11659, 11859], 'B': [11289, 11489, 11689, 11889]}
HELMET = {'A': [12249, 12389, 12529, 12669], 'B': [12269, 12399, 12539, 12679]}
SHIELD = {'A': 13049, 'B': 13069}
WRIST = {'A': 14149, 'B': 14229}
FOOTS = {'A': 15149, 'B': 15229}
NECK = {'A': 16149, 'B': 16229}
EAR = {'A': 17149, 'B': 17209}
ARROW = {'A': 8004, 'B': 8005}
def items(p):
    tier = 'A' if p['map'] == 360 else 'B'
    cls, grp = p['cls'], p['grp']
    stat = P_IQ if cls == 3 else (P_DX if (cls == 1 and grp == 2) else P_ST)
    w = weapon(cls, grp, tier)
    wsock = [28437, 28430] + ([28431] if tier == 'B' else [0])
    bsock = [28441, 28441] + ([28439] if tier == 'B' else [0])
    out = [
        (0, ARMOUR[tier][cls], 1, bsock, [(P_MAX_HP, 1500), (P_RES_SWORD, 15), (P_RES_BOW, 15), (P_RES_MAGIC, 15), (P_DODGE, 10)]),
        (1, HELMET[tier][cls], 1, [0, 0, 0], [(P_ATT_SPEED, 8), (P_HP_REGEN, 12), (P_RES_MAGIC, 15), (P_POISON, 8)]),
        (2, FOOTS[tier], 1, [0, 0, 0], [(P_MAX_HP, 1500), (P_MOV_SPEED, 8), (P_CRIT, 10), (P_STUN, 8)]),
        (3, WRIST[tier], 1, [0, 0, 0], [(P_MAX_HP, 1500), (P_STEAL_HP, 10), (P_PEN, 10)]),
        (4, w, 1, wsock, [(P_AVG, 45), (P_CRIT, 10), (stat, 12), (P_PEN, 10), (P_HUMAN, 10)]),
        (5, NECK[tier], 1, [0, 0, 0], [(P_MAX_HP, 1500), (P_CRIT, 10), (P_PEN, 10), (P_STEAL_HP, 10)]),
        (6, EAR[tier], 1, [0, 0, 0], [(P_ANIMAL, 20), (P_DEVIL, 20), (P_MOV_SPEED, 8), (P_POISON_REDUCE, 5)]),
        (10, SHIELD[tier], 1, [0, 0, 0], [(P_HT, 12), (P_BLOCK, 15), (P_REFLECT, 10), (P_IMMUNE_STUN, 1)]),
    ]
    if cls == 1 and grp == 2:
        out.append((9, ARROW[tier], 500, [0, 0, 0], []))
    return out
# The kingdom's second village (apply.sh's stranded-bot points; game1 hosts 3, 23, 43).
M2 = {1: (3, 400200, 899500), 2: (23, 145500, 240000), 3: (43, 906400, 221400)}

pids = sorted(p['pid'] for p in pick)
plist = ','.join(str(x) for x in pids)
S = []
S.append('-- MT2009_PLUS_AREZZO_BOTS_V1: the Arezzo test cohorts (tools/arezzo_test_cohort.py).')
S.append('-- %d characters: %d for 360 (level 45), %d for 361 (level 61). Run with the game stopped.' % (
    len(pick), sum(1 for p in pick if p['map'] == 360), sum(1 for p in pick if p['map'] == 361)))
S.append('SET NAMES latin1;')
S.append('CREATE TABLE IF NOT EXISTS player.arezzo_test_cohort (pid INT UNSIGNED NOT NULL PRIMARY KEY, map INT NOT NULL, '
         'target_level TINYINT UNSIGNED NOT NULL, old_level TINYINT UNSIGNED NOT NULL, picked_at DATETIME NOT NULL, '
         'applied_at DATETIME NULL) ENGINE=InnoDB;')
S.append('START TRANSACTION;')
S.append('INSERT IGNORE INTO player.arezzo_test_cohort (pid, map, target_level, old_level, picked_at) VALUES')
S.append(',\n'.join('  (%d, %d, %d, %d, NOW())' % (p['pid'], p['map'], p['lvl'], p['old']) for p in pick) + ';')
# Only rows not yet applied, and never lowering a level. MariaDB evaluates the
# assignments left to right with the new values, so level is set last.
S.append("""UPDATE player.player p JOIN player.arezzo_test_cohort c ON c.pid = p.id AND c.applied_at IS NULL
   SET p.stat_point = GREATEST(p.stat_point, LEAST(3 * (c.target_level - 1), 344) - (p.st + p.ht + p.dx + p.iq - 16)),
       p.skill_point = p.skill_point + GREATEST(0, c.target_level - GREATEST(p.level, 4)),
       p.sub_skill_point = p.sub_skill_point + GREATEST(0, c.target_level - GREATEST(p.level, 8)),
       p.random_hp = GREATEST(p.random_hp, 40 * (c.target_level - 1)),
       p.random_sp = GREATEST(p.random_sp, 20 * (c.target_level - 1)),
       p.gold = GREATEST(p.gold, 3000000),
       p.exp = 0, p.level_step = 0,
       p.level = GREATEST(p.level, c.target_level);""")
S.append("""UPDATE player.player p JOIN player.arezzo_test_cohort c ON c.pid = p.id AND c.applied_at IS NULL
  JOIN account.account ac ON ac.id = p.account_id JOIN player.player_index pi ON pi.id = ac.id
   SET p.map_index = CASE pi.empire WHEN 1 THEN 3 WHEN 3 THEN 43 ELSE 23 END,
       p.x = CASE pi.empire WHEN 1 THEN 400200 WHEN 3 THEN 906400 ELSE 145500 END,
       p.y = CASE pi.empire WHEN 1 THEN 899500 WHEN 3 THEN 221400 ELSE 240000 END;""")
S.append('UPDATE player.arezzo_test_cohort SET applied_at = NOW() WHERE applied_at IS NULL;')
S.append('DELETE FROM player.item WHERE window = \'EQUIPMENT\' AND pos IN (0,1,2,3,4,5,6,9,10) AND owner_id IN (%s);' % plist)
ins = []
for p in sorted(pick, key=lambda x: x['pid']):
    for pos, vnum, count, sock, attrs in items(p):
        at = list(attrs) + [(0, 0)] * (7 - len(attrs))
        ins.append('  (%d, \'EQUIPMENT\', %d, %d, %d, %d, %d, %d, %s)' % (
            p['pid'], pos, count, vnum, sock[0], sock[1], sock[2], ', '.join('%d, %d' % t for t in at)))
S.append('INSERT INTO player.item (owner_id, window, pos, count, vnum, socket0, socket1, socket2, '
         'attrtype0, attrvalue0, attrtype1, attrvalue1, attrtype2, attrvalue2, attrtype3, attrvalue3, '
         'attrtype4, attrvalue4, attrtype5, attrvalue5, attrtype6, attrvalue6) VALUES\n' + ',\n'.join(ins) + ';')
S.append('COMMIT;')
S.append('SELECT c.map, p.level, COUNT(*) AS bots FROM player.arezzo_test_cohort c JOIN player.player p ON p.id = c.pid GROUP BY c.map, p.level;')
S.append("SELECT COUNT(*) AS gear_rows FROM player.item WHERE window = 'EQUIPMENT' AND owner_id IN (%s) AND pos IN (0,1,2,3,4,5,6,9,10);" % plist)
open(os.path.join(a.outdir, 'cohort_apply.sql'), 'w', encoding='latin-1').write('\n'.join(S) + '\n')
with open(os.path.join(a.outdir, 'pick.tsv'), 'w', encoding='latin-1') as f:
    f.write('pid\tname\tclass\tgroup\tempire\tlevel_before\tlevel_after\tmap\n')
    for p in sorted(pick, key=lambda x: (x['map'], x['cls'], x['emp'], x['pid'])):
        f.write('%d\t%s\t%d\t%d\t%d\t%d\t%d\t%d\n' % (p['pid'], p['name'], p['cls'], p['grp'], p['emp'], p['old'], p['lvl'], p['map']))
with open(os.path.join(a.outdir, 'playerbot_arezzo_cohort.txt'), 'w') as f:
    f.write('# MT2009_PLUS_AREZZO_BOTS_V1: "<map> <pid>" - logged in on top of the population, never rested\n')
    for p in sorted(pick, key=lambda x: (x['map'], x['pid'])):
        f.write('%d %d\n' % (p['map'], p['pid']))
    for pid in ochao:
        f.write('362 %d\n' % pid)
open(os.path.join(a.outdir, 'cohort_revert_note.txt'), 'w').write(
    'Undo: restore the backup taken before cohort_apply.sql (backup_before.sql: player.player and\n'
    'player.item rows of the picked pids), with the game stopped:\n'
    '  docker exec -i mt2009plustest-db sh -c \'mariadb -uroot -p"$MARIADB_ROOT_PASSWORD"\' < backup_before.sql\n'
    'then DROP TABLE player.arezzo_test_cohort.\n'
    'The pids: %s\n' % plist)
open(os.path.join(a.outdir, 'pids.txt'), 'w').write(' '.join(str(x) for x in pids) + '\n')
cnt = collections.Counter((p['map'], p['cls']) for p in pick)
emp = collections.Counter((p['map'], p['emp']) for p in pick)
print('picked', len(pick), dict(cnt), dict(emp))
print('level before: A %s  B %s' % (
    sorted(collections.Counter(p['old'] for p in pick if p['map'] == 360).items()),
    sorted(collections.Counter(p['old'] for p in pick if p['map'] == 361).items())))
