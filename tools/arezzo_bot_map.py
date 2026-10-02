#!/usr/bin/env python3
"""arezzo_bot_map.py - the three Arezzo maps' knowledge for the bots
(MT2009_PLUS_AREZZO_BOTS_V1, playerbot_arezzo_bots.h). The Temple of Ochao's
generator (tools/ochao_bot_map.py) made for open maps.

    arezzo_bot_map.py decode <server_attr> <grid.bin>     (needs python-lzo)
    arezzo_bot_map.py gen <mapdir> <map_index> [out.inc [LEVEL_CONSTANT]]

`decode` expands a map's server_attr (SECTREE_MANAGER::LoadAttribute: int32
width/height in sectors, then per sector an lzo1x block of 128x128 uint32) to
one byte per 50-unit cell. On this host python-lzo lives in the m2pack-lzo
image:  docker run --rm -v DIR:/w m2pack-lzo:latest python3 /w/arezzo_bot_map.py decode ...

`gen` reads <mapdir>/{grid.bin,Setting.txt,Town.txt,npc.txt,regen.txt,boss.txt,
stone.txt} (the map's own files, linux-port/docker/game/arezzo/map/<name>/) and
measures, on a 100-unit grid (a cell is blocked when any of its four 50-unit
cells carries ATTR_BLOCK or ATTR_OBJECT, as the bots' navigation reads them;
water is walkable at a cost):

  * the walkable pieces: the arrival (Town.txt) and the Teleporter (npc.txt 9012)
    must share the largest one; every regen line, boss and stone point is
    checked against it (the rest are reported and left out);
  * the arrival and the exit: cell centres with open ground all round;
  * the hunting spots: the regen's spawn lines clustered (the densest window of
    CLUSTER units first, spots at least SPACING apart), each snapped to a cell
    with SNAP_CLEAR open cells all round in the main piece;
  * the routes: a Dijkstra from the Teleporter that keeps off the walls
    (corridor middles and open ground preferred, water dearer), cut into
    straight legs (each leg checked cell by cell on the 50-unit grid with
    LEG_CLEAR open 100-unit cells either side), as one tree rooted at the
    Teleporter whose leaves are the arrival, every spot, every boss point and
    every stone point. The bot walks it leg by leg: out to the Teleporter, in
    from the arrival to a far spot, to a boss for a raid;
  * the walk between spots (Dijkstra from each spot), which the spot choice
    uses instead of the straight line where the ground makes a detour.

It prints a report and writes the C++ rows (the tables in
playerbot_arezzo_bots.h) to out.inc. One-shot analysis, not part of the build.
"""
import sys, struct, math, heapq
from array import array

if len(sys.argv) >= 2 and sys.argv[1] == 'decode':
    import lzo
    d = open(sys.argv[2], 'rb').read()
    w, h = struct.unpack_from('<ii', d, 0); off = 8
    W = w * 128; H = h * 128
    grid = bytearray(W * H)
    for sy in range(h):
        for sx in range(w):
            (n,) = struct.unpack_from('<I', d, off); off += 4
            raw = lzo.decompress(d[off:off + n], False, 128 * 128 * 4); off += n
            a = struct.unpack('<16384I', raw)
            for cy in range(128):
                row = (sy * 128 + cy) * W + sx * 128
                for cx in range(128):
                    grid[row + cx] = a[cy * 128 + cx] & 0xff
    open(sys.argv[3], 'wb').write(struct.pack('<ii', W, H) + bytes(grid))
    print(w, h, W, H)
    sys.exit(0)
if len(sys.argv) < 4 or sys.argv[1] != 'gen':
    sys.exit(__doc__)

D = sys.argv[2].rstrip('/'); MAPIDX = int(sys.argv[3]); OUT = sys.argv[4] if len(sys.argv) > 4 else None
LEVEL = sys.argv[5] if len(sys.argv) > 5 else '1'
BLOCK = 0x81      # ATTR_BLOCK | ATTR_OBJECT
WATER = 0x02
CLUSTER = 3600    # spawn lines within this window of the densest one make a spot
SPACING = 6000    # spots at least this far apart
MAX_SPOTS = 24
SNAP_CLEAR = 5    # a spot stands with five open 100-unit cells all round
LEG_CLEAR = 2     # a leg keeps two open 100-unit cells either side
CAP = 8

def rd(name):
    try:
        return [l.split() for l in open(D + '/' + name, errors='replace') if l.strip() and not l.startswith('//')]
    except FileNotFoundError:
        return []
BX = BY = None
for p in rd('Setting.txt'):
    if p[0] == 'BasePosition': BX, BY = int(p[1]), int(p[2])
d = open(D + '/grid.bin', 'rb').read(); FW, FH = struct.unpack_from('<ii', d); fg = d[8:]
W, H = FW // 2, FH // 2
N = W * H
blk = bytearray(N); wat = bytearray(N)
for y in range(H):
    for x in range(W):
        a = fg[(2 * y) * FW + 2 * x]; b = fg[(2 * y) * FW + 2 * x + 1]
        c = fg[(2 * y + 1) * FW + 2 * x]; e = fg[(2 * y + 1) * FW + 2 * x + 1]
        i = y * W + x
        if (a | b | c | e) & BLOCK: blk[i] = 1
        elif (a | b | c | e) & WATER: wat[i] = 1
# clearance in 100-unit cells (4-neighbour BFS from the blocked cells and the edge)
clr = bytearray([CAP]) * N
from collections import deque
q = deque()
for i in range(N):
    x, y = i % W, i // W
    if blk[i]: clr[i] = 0; q.append(i)
    elif x == 0 or y == 0 or x == W - 1 or y == H - 1: clr[i] = 1; q.append(i)
while q:
    j = q.popleft(); x, y = j % W, j // W; c = clr[j]
    if c >= CAP: continue
    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        X, Y = x + dx, y + dy
        if 0 <= X < W and 0 <= Y < H:
            k = Y * W + X
            if clr[k] > c + 1: clr[k] = c + 1; q.append(k)
# walkable pieces (8-neighbour without corner cutting, as the navigation)
comp = array('i', [0]) * N; sizes = [0]
for s in range(N):
    if blk[s] or comp[s]: continue
    cid = len(sizes); comp[s] = cid; n = 0; st = [s]
    while st:
        j = st.pop(); n += 1; x, y = j % W, j // W
        for dx in (-1, 0, 1):
            for dy in (-1, 0, 1):
                if not dx and not dy: continue
                X, Y = x + dx, y + dy
                if 0 <= X < W and 0 <= Y < H:
                    k = Y * W + X
                    if blk[k] or comp[k]: continue
                    if dx and dy and (blk[y * W + X] or blk[Y * W + x]): continue
                    comp[k] = cid; st.append(k)
    sizes.append(n)
def w2c(wx, wy): return ((wx - BX) // 100, (wy - BY) // 100)
def c2w(cx, cy): return (BX + cx * 100 + 50, BY + cy * 100 + 50)
def snap(cx, cy, minclr, want=None, r=40):
    best = None
    for dy in range(-r, r + 1):
        for dx in range(-r, r + 1):
            X, Y = cx + dx, cy + dy
            if 0 <= X < W and 0 <= Y < H:
                k = Y * W + X
                if clr[k] >= minclr and (want is None or comp[k] == want):
                    dd = dx * dx + dy * dy
                    if best is None or dd < best[0]: best = (dd, X, Y)
    return (best[1], best[2]) if best else None
town = [(int(p[0]), int(p[1])) for p in rd('Town.txt')][0]
tele = [(int(p[1]), int(p[2])) for p in rd('npc.txt') if p[0] == 'm' and p[-1] == '9012'][0]
tcell = snap(*w2c(BX + tele[0] * 100, BY + tele[1] * 100), 3)
main = comp[tcell[1] * W + tcell[0]]
arr = snap(*w2c(BX + town[0] * 100, BY + town[1] * 100), 4, main)
walkable = sum(1 for i in range(N) if not blk[i])
print('map %d base (%d,%d) grid %dx%d walkable %.1f%% pieces %d main %d cells (%.1f%% of walkable)' % (
    MAPIDX, BX, BY, W, H, 100.0 * walkable / N, len(sizes) - 1, sizes[main], 100.0 * sizes[main] / walkable))
print('teleporter npc cell', tele, '-> world', c2w(*tcell), ' arrival (Town)', town, '-> world', c2w(*arr))
# points of interest
def pts(name, typ=None):
    out = []
    for p in rd(name):
        if len(p) < 11 or (typ and p[0] != typ): continue
        out.append((BX + int(p[1]) * 100, BY + int(p[2]) * 100, int(p[-1]), p[7]))
    return out
regen = pts('regen.txt'); bosses = pts('boss.txt'); stones = pts('stone.txt')
def inmain(wx, wy):
    c = w2c(wx, wy)
    s = snap(c[0], c[1], 1, main, 12)
    return s
lost = [r for r in regen if not inmain(r[0], r[1])]
print('regen lines %d, outside the main piece %d' % (len(regen), len(lost)))
# spots
rem = [r for r in regen if r not in lost]; hubs = []
while rem and len(hubs) < MAX_SPOTS:
    best = None
    for sx, sy, v, _ in rem:
        n = sum(1 for tx, ty, _, _ in rem if abs(tx - sx) < CLUSTER and abs(ty - sy) < CLUSTER)
        if best is None or n > best[0]: best = (n, sx, sy)
    n, sx, sy = best
    if n < 3: break
    near = [(tx, ty) for tx, ty, _, _ in rem if abs(tx - sx) < CLUSTER and abs(ty - sy) < CLUSTER]
    mx = sum(p[0] for p in near) // len(near); my = sum(p[1] for p in near) // len(near)
    c = snap(*w2c(mx, my), SNAP_CLEAR, main) or snap(*w2c(mx, my), 3, main)
    if c:
        hx, hy = c2w(*c)
        if all(math.hypot(hx - a, hy - b) >= SPACING for a, b, _ in hubs):
            hubs.append((hx, hy, n))
    rem = [r for r in rem if not (abs(r[0] - sx) < CLUSTER + 400 and abs(r[1] - sy) < CLUSTER + 400)]
print('spots', len(hubs), 'covering', sum(h[2] for h in hubs), 'of', len(regen) - len(lost), 'lines')
# Dijkstra from the Teleporter on the 100-unit grid
INF = 1 << 60
def dijkstra(src):
    dist = array('q', [INF]) * N; par = array('i', [-1]) * N
    s = src[1] * W + src[0]; dist[s] = 0; h = [(0, s)]
    nb = ((1, 0, 10), (-1, 0, 10), (0, 1, 10), (0, -1, 10), (1, 1, 14), (1, -1, 14), (-1, 1, 14), (-1, -1, 14))
    while h:
        dd, j = heapq.heappop(h)
        if dd > dist[j]: continue
        x, y = j % W, j // W
        for dx, dy, c in nb:
            X, Y = x + dx, y + dy
            if 0 <= X < W and 0 <= Y < H:
                k = Y * W + X
                if blk[k]: continue
                if dx and dy and (blk[y * W + X] or blk[Y * W + x]): continue
                pen = 0 if clr[k] >= 4 else (4 - clr[k]) * 12
                if wat[k]: pen += 8
                nd = dd + c + pen
                if nd < dist[k]: dist[k] = nd; par[k] = j; heapq.heappush(h, (nd, k))
    return dist, par
dist, par = dijkstra(tcell)
def fine_clear(a, b, margin):
    """The leg a->b (100-unit cells) walked on the 50-unit grid, and on the
    100-unit grid with `margin` open cells to its sides."""
    (x0, y0), (x1, y1) = a, b
    n = max(abs(x1 - x0), abs(y1 - y0)) * 4 + 1
    for i in range(n + 1):
        t = i / n
        fx = int((x0 + 0.5 + (x1 - x0) * t) * 2); fy = int((y0 + 0.5 + (y1 - y0) * t) * 2)
        if fx < 0 or fy < 0 or fx >= FW or fy >= FH or fg[fy * FW + fx] & BLOCK: return False
        cx, cy = fx // 2, fy // 2
        if clr[cy * W + cx] < margin: return False
    return True
leaves = [('arrival', arr)]
for i, hb in enumerate(hubs): leaves.append(('spot%d' % i, w2c(hb[0], hb[1])))
for b in bosses:
    c = snap(*w2c(b[0], b[1]), 3, main)
    if c: leaves.append(('boss%d' % b[2], c))
for s in stones:
    c = snap(*w2c(s[0], s[1]), 3, main)
    if c: leaves.append(('stone%d' % s[2], c))
nodes = [{'c': tcell, 'next': -1, 'name': 'teleporter'}]; cell2node = {tcell[1] * W + tcell[0]: 0}
intree = set(cell2node)
def simplify(cells):
    pts = [(c % W, c // W) for c in cells]; out = [0]; i = 0
    while i < len(pts) - 1:
        j = min(len(pts) - 1, i + 400)
        while j > i + 1 and not fine_clear(pts[i], pts[j], LEG_CLEAR): j -= 1
        out.append(j); i = j
    return pts, out
unreached = []
for name, (cx, cy) in leaves:
    k = cy * W + cx
    if dist[k] >= INF: unreached.append(name); continue
    chain = [k]
    while chain[-1] not in intree and par[chain[-1]] >= 0: chain.append(par[chain[-1]])
    join = chain[-1]
    if join not in cell2node:
        sub = [join]
        while sub[-1] not in cell2node: sub.append(par[sub[-1]])
        sp, ss = simplify(sub)
        nx = cell2node[sub[-1]]
        for idx in reversed(ss[:-1]):
            nodes.append({'c': sp[idx], 'next': nx, 'name': 'join' if idx == 0 else ''}); cell2node[sub[idx]] = len(nodes) - 1; nx = len(nodes) - 1
    pts_, simp = simplify(chain)
    nxt = cell2node[join]
    for idx in reversed(simp[:-1]):
        nodes.append({'c': pts_[idx], 'next': nxt, 'name': name if idx == 0 else ''}); cell2node[chain[idx]] = len(nodes) - 1; nxt = len(nodes) - 1
    for c in chain: intree.add(c)
bad = sum(1 for n in nodes if n['next'] >= 0 and not fine_clear(n['c'], nodes[n['next']]['c'], 1))
print('route tree: %d corners, legs failing the 50-unit check %d, leaves unreachable %s' % (len(nodes), bad, unreached or 'none'))
def walk_of(cell): return dist[cell[1] * W + cell[0]] / 10 * 100
print('walk to the Teleporter: arrival %.1f km; spots max %.1f km' % (walk_of(arr) / 1000, max(walk_of(w2c(h[0], h[1])) for h in hubs) / 1000))
# walk between spots
hw = []
detour = []
for i, hb in enumerate(hubs):
    di, _ = dijkstra(w2c(hb[0], hb[1]))
    row = []
    for j, hc in enumerate(hubs):
        c = w2c(hc[0], hc[1]); v = di[c[1] * W + c[0]]
        wv = int(v / 10 * 100) if v < INF else 999999
        row.append(wv)
        if j > i:
            straight = math.hypot(hb[0] - hc[0], hb[1] - hc[1])
            if straight > 0: detour.append(wv / straight)
    hw.append(row)
detour.sort()
print('walk / straight line between spots: median %.2f  p90 %.2f  max %.2f' % (
    detour[len(detour) // 2], detour[int(len(detour) * 0.9)], detour[-1]))
for i, hb in enumerate(hubs):
    print('  spot %2d (%d,%d) lines %d walk_to_teleporter %.1f km' % (i, hb[0], hb[1], hb[2], walk_of(w2c(hb[0], hb[1])) / 1000))
for b in bosses: print('  boss %d at (%d,%d) %s' % (b[2], b[0], b[1], b[3]))
if OUT:
    o = []
    o.append('\t// map %d: arrival (%d, %d), Teleporter (%d, %d); %d spots, %d corners.' % ((MAPIDX,) + c2w(*arr) + c2w(*tcell) + (len(hubs), len(nodes))))
    o.append('\tconst TPlayerBotHuntingHub PLAYERBOT_AREZZO_HUBS_%d[] = {' % MAPIDX)
    for hb in hubs: o.append('\t\t{ %d, %d, %s, 255, false, 0 },\t// %d spawn lines' % (hb[0], hb[1], LEVEL, hb[2]))
    o.append('\t};')
    o.append('\tconst TPlayerBotArezzoNode PLAYERBOT_AREZZO_TREE_%d[] = {' % MAPIDX)
    line = []
    for i, n in enumerate(nodes):
        line.append('{ %d, %d, %d }' % (c2w(*n['c']) + (n['next'],)))
        if len(line) == 4: o.append('\t\t' + ', '.join(line) + ','); line = []
    if line: o.append('\t\t' + ', '.join(line) + ',')
    o.append('\t};')
    o.append('\tconst int PLAYERBOT_AREZZO_WALK_%d[%d][%d] = {' % (MAPIDX, len(hubs), len(hubs)))
    for r in hw: o.append('\t\t{ ' + ', '.join(str(v) for v in r) + ' },')
    o.append('\t};')
    open(OUT, 'w').write('\n'.join(o) + '\n')
    print('written', OUT)
