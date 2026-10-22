"""Builds the full Overworld: the existing town block in the middle, zones banded by distance from
it (level 1 beside town, 30 at the edges), paths to the four lairs. Deterministic.

usage: python3 -I tools/world/make_map.py tests/fixtures/starter_world/world/overworld.txt \
         tests/fixtures/starter_world/world/world.json data/world/overworld.txt data/world/world.json
The output is the shipped map; edit the script and rerun rather than hand-editing the result.
"""
import json, math, random, sys
from collections import deque

RING = 2         # tiles of each level's band around the town
MARGIN = 2 + 30 * RING + 1
OX, OY = MARGIN, MARGIN  # where the old 72x30 block goes
rng = random.Random(7)

old_rows = open(sys.argv[1]).read().splitlines()
old = json.load(open(sys.argv[2]))
ow, oh = len(old_rows[0]), len(old_rows)
W, H = ow + 2 * MARGIN, oh + 2 * MARGIN


def noise_field(scale, seed):
    r = random.Random(seed)
    pts = {}

    def at(ix, iy):
        if (ix, iy) not in pts:
            pts[(ix, iy)] = r.random()
        return pts[(ix, iy)]

    def f(x, y):
        x, y = x / scale, y / scale
        ix, iy = math.floor(x), math.floor(y)
        fx, fy = x - ix, y - iy
        sx, sy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)
        a, b, c, d = at(ix, iy), at(ix + 1, iy), at(ix, iy + 1), at(ix + 1, iy + 1)
        return (a + (b - a) * sx) * (1 - sy) + (c + (d - c) * sx) * sy

    return f


wet, wood, stone, lush = (noise_field(s, k) for s, k in ((19, 1), (9, 2), (23, 3), (7, 4)))
grid = [["." for _ in range(W)] for _ in range(H)]
cx, cy = OX + ow / 2, OY + oh / 2
for y in range(H):
    for x in range(W):
        # Further out is wilder: more trees, rock and water.
        d = max(abs(x - cx) / (W / 2), abs(y - cy) / (H / 2))
        if wet(x, y) > 0.80 - 0.05 * d:
            grid[y][x] = "~"
        elif wet(x, y) > 0.75 - 0.05 * d:
            grid[y][x] = "s"
        elif stone(x, y) > 0.86 - 0.12 * d:
            grid[y][x] = "^"
        elif wood(x, y) > 0.78 - 0.18 * d:
            grid[y][x] = "T"
        elif lush(x, y) > 0.62:
            grid[y][x] = ","

# The old town block, its border trees opened so the town meets the wilds.
for y in range(oh):
    for x in range(ow):
        ch = old_rows[y][x]
        if (y in (0, oh - 1) or x in (0, ow - 1)) and ch == "T":
            ch = "."
        grid[OY + y][OX + x] = ch
for y in range(H):
    for x in range(W):
        if x in (0, W - 1) or y in (0, H - 1):
            grid[y][x] = "T"


def shift(t):
    return (t[0] + OX, t[1] + OY)


spawn = shift((old["spawn"]["x"], old["spawn"]["y"]))
town = (OX - 2, OY - 2, OX + ow + 2, OY + oh + 2)  # no zones in here


def level_at(x, y):
    # 1 beside the town, one level more every RING tiles out, 30 at the map's edge.
    d = max(town[0] - x, x - town[2] + 1, town[1] - y, y - town[3] + 1, 1)
    return min(30, 1 + (d - 1) // RING)


# Lairs where the zones' level matches the mission's: Avaritia 15 west, Gula 18 south,
# the Gorgon (Medusa) 20 east, Ira 22 north.
def find_lair(want, direction):
    best = None
    for y in range(3, H - 3):
        for x in range(3, W - 3):
            if direction == "west" and x > OX - 4: continue
            if direction == "east" and x < OX + ow + 4: continue
            if direction == "north" and y > OY - 4: continue
            if direction == "south" and y < OY + oh + 4: continue
            if level_at(x, y) != want: continue
            score = abs(y - spawn[1]) if direction in ("west", "east") else abs(x - spawn[0])
            if best is None or score < best[0]:
                best = (score, (x, y))
    return best[1]


lairs = [("Lair of Avaritia", "avaritia", 15, "west"), ("Lair of Gula", "gula", 18, "south"),
         ("Lair of Medusa", "the_gorgon", 20, "east"), ("Lair of Ira", "ira", 22, "north")]
old_avaritia = shift((67, 14))
grid[old_avaritia[1]][old_avaritia[0]] = ","  # the old placeholder lair moves out west
lair_at = {}
for name, mission, level, direction in lairs:
    lair_at[mission] = find_lair(level, direction)


def carve_path(a, b):
    # An L of path tiles, walking x then y, through anything but town walls.
    x, y = a
    while (x, y) != b:
        if x != b[0]:
            x += 1 if b[0] > x else -1
        else:
            y += 1 if b[1] > y else -1
        if grid[y][x] not in "#_":
            grid[y][x] = ":"


plaza_exits = {"west": (OX + 1, spawn[1]), "east": (OX + ow - 2, spawn[1]),
               "north": (spawn[0], OY), "south": (spawn[0], OY + oh - 1)}
for name, mission, level, direction in lairs:
    carve_path(plaza_exits[direction], lair_at[mission])
    lx, ly = lair_at[mission]
    for yy in range(ly - 1, ly + 2):  # a clearing round each lair
        for xx in range(lx - 1, lx + 2):
            grid[yy][xx] = ":" if (xx, yy) == (lx, ly) else "_"

# Anything walkable that can't be reached from spawn becomes trees, so no monster spawns there.
walk = set(".,:s_")
seen = {spawn}
q = deque([spawn])
while q:
    x, y = q.popleft()
    for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
        if 0 <= nx < W and 0 <= ny < H and (nx, ny) not in seen and grid[ny][nx] in walk:
            seen.add((nx, ny))
            q.append((nx, ny))
for y in range(H):
    for x in range(W):
        if grid[y][x] in walk and (x, y) not in seen:
            grid[y][x] = "T"

band_names = [(1, "Meadow"), (4, "Heath"), (8, "Fen"), (12, "Old Wood"), (16, "Barrows"),
              (20, "Ashen Scarp"), (25, "Blight"), (30, "World's Edge")]


def band(level):
    return [n for lo, n in band_names if level >= lo][-1]


zones = []
for level in range(1, 31):
    # The ring of tiles at this level: the box RING*level out from town minus the one inside it.
    i0, i1 = (level - 1) * RING, level * RING
    ox0, oy0, ox1, oy1 = town[0] - i1, town[1] - i1, town[2] + i1, town[3] + i1
    ix0, iy0, ix1, iy1 = town[0] - i0, town[1] - i0, town[2] + i0, town[3] + i0
    if level == 30:  # the last ring runs to the border
        ox0, oy0, ox1, oy1 = 1, 1, W - 1, H - 1
    strips = [("North", ox0, oy0, ox1, iy0), ("South", ox0, iy1, ox1, oy1),
              ("West", ox0, iy0, ix0, iy1), ("East", ix1, iy0, ox1, iy1)]
    for side, x0, y0, x1, y1 in strips:
        x0, y0, x1, y1 = max(x0, 1), max(y0, 1), min(x1, W - 1), min(y1, H - 1)
        if x1 <= x0 or y1 <= y0:
            continue
        open_tiles = sum((x, y) in seen for y in range(y0, y1) for x in range(x0, x1))
        if open_tiles < 8:
            continue
        zones.append({"name": f"{side} {band(level)}", "x": x0, "y": y0, "w": x1 - x0, "h": y1 - y0,
                      "level": level, "count": max(1, min(50, open_tiles // 90))})
# The town's own meadow stays, beside the plaza.
for z in old["zones"]:
    zones.append({**z, "x": z["x"] + OX, "y": z["y"] + OY})

landmarks = []
for l in old["landmarks"]:
    if "mission" in l["opens"]:
        continue
    x, y = shift((l["x"], l["y"]))
    landmarks.append({**l, "x": x, "y": y})
for name, mission, level, direction in lairs:
    x, y = lair_at[mission]
    landmarks.append({"name": name, "x": x, "y": y, "opens": {"mission": mission}})

open(sys.argv[3], "w").write("\n".join("".join(r) for r in grid) + "\n")
out = {"legend": old["legend"], "spawn": {"x": spawn[0], "y": spawn[1]},
       "landmarks": landmarks, "zones": zones}
with open(sys.argv[4], "w") as f:
    f.write("{\n")
    f.write('  "legend": ' + json.dumps(old["legend"], indent=4).replace("\n", "\n  ") + ",\n")
    f.write(f'  "spawn": {json.dumps(out["spawn"])},\n')
    f.write('  "landmarks": [\n' + ",\n".join("    " + json.dumps(l) for l in landmarks) + "\n  ],\n")
    f.write('  "zones": [\n' + ",\n".join("    " + json.dumps(z) for z in zones) + "\n  ]\n}\n")
levels = sorted({z["level"] for z in zones})
print("zones", len(zones), "levels", levels)
print("lairs", lair_at)
