#include "game/dungeon/floor.hpp"

#include "game/rng.hpp"

#include <memory>

namespace heroes::game::dungeon {

namespace {

struct Node {
    Rect area;
    std::unique_ptr<Node> left, right; // both or neither
    int room = -1;                     // index into Floor::rooms, for a leaf
};

std::uint32_t draw(Rng& rng, int bound) { return rng.below(static_cast<std::uint32_t>(bound)); }

// Step 1: splits `area` until maxDepth or too small, left child first.
std::unique_ptr<Node> split(const DungeonDef& def, Rng& rng, Rect area, int depth,
                            std::vector<Node*>& leaves) {
    auto node = std::make_unique<Node>();
    node->area = area;
    const auto& bsp = def.bsp;
    const bool canV = area.w >= 2 * bsp.minLeafWidth, canH = area.h >= 2 * bsp.minLeafHeight;
    if (depth == bsp.maxDepth || (!canV && !canH)) {
        leaves.push_back(node.get());
        return node;
    }
    bool vertical;
    if (canV != canH)
        vertical = canV;
    else if (4 * area.w >= 5 * area.h)
        vertical = true;
    else if (4 * area.h >= 5 * area.w)
        vertical = false;
    else
        vertical = draw(rng, 2) == 0;
    if (vertical) {
        const int cut = bsp.minLeafWidth + static_cast<int>(draw(rng, area.w - 2 * bsp.minLeafWidth + 1));
        node->left = split(def, rng, {area.x, area.y, cut, area.h}, depth + 1, leaves);
        node->right = split(def, rng, {area.x + cut, area.y, area.w - cut, area.h}, depth + 1, leaves);
    } else {
        const int cut = bsp.minLeafHeight + static_cast<int>(draw(rng, area.h - 2 * bsp.minLeafHeight + 1));
        node->left = split(def, rng, {area.x, area.y, area.w, cut}, depth + 1, leaves);
        node->right = split(def, rng, {area.x, area.y + cut, area.w, area.h - cut}, depth + 1, leaves);
    }
    return node;
}

Tile& tileAt(Floor& f, Pos p) { return f.tiles[static_cast<std::size_t>(p.y * f.width + p.x)]; }
Pos center(const Rect& r) { return {r.x + r.w / 2, r.y + r.h / 2}; }

// The room standing for a subtree: its leftmost leaf's.
int rep(const Node& n) { return n.left ? rep(*n.left) : n.room; }

// Step 3: children first, left before right; each internal node joins its two sides with an L.
void connect(const Node& n, Floor& f, Rng& rng) {
    if (!n.left)
        return;
    connect(*n.left, f, rng);
    connect(*n.right, f, rng);
    const Pos a = center(f.rooms[static_cast<std::size_t>(rep(*n.left))]);
    const Pos b = center(f.rooms[static_cast<std::size_t>(rep(*n.right))]);
    const bool horizontalFirst = draw(rng, 2) == 0;
    const Pos corner = horizontalFirst ? Pos{b.x, a.y} : Pos{a.x, b.y};
    const auto carve = [&](Pos from, Pos to) {
        for (int x = std::min(from.x, to.x); x <= std::max(from.x, to.x); ++x)
            for (int y = std::min(from.y, to.y); y <= std::max(from.y, to.y); ++y)
                tileAt(f, {x, y}) = Tile::Floor;
    };
    carve(a, corner);
    carve(corner, b);
}

// Removes and returns a random entry, keeping the rest in order.
Pos takeRandom(std::vector<Pos>& candidates, Rng& rng) {
    const auto i = static_cast<std::ptrdiff_t>(draw(rng, static_cast<int>(candidates.size())));
    const Pos p = candidates[static_cast<std::size_t>(i)];
    candidates.erase(candidates.begin() + i);
    return p;
}

} // namespace

Floor generateFloor(const DungeonDef& def, std::uint64_t expeditionSeed, int floorIndex) {
    Rng rng = Rng::stream(expeditionSeed, static_cast<std::uint64_t>(floorIndex));
    Floor f;
    f.width = def.width;
    f.height = def.height;
    f.index = floorIndex;
    f.tiles.assign(static_cast<std::size_t>(f.width * f.height), Tile::Wall);
    const bool last = floorIndex == def.floors;

    std::vector<Node*> leaves;
    const std::unique_ptr<Node> root = split(def, rng, {0, 0, f.width, f.height}, 0, leaves);

    // Step 2: one room per leaf, in creation order, inside a one-tile margin.
    const auto& bsp = def.bsp;
    for (Node* leaf : leaves) {
        const Rect& a = leaf->area;
        const int rw = bsp.minRoomWidth + static_cast<int>(draw(rng, a.w - 2 - bsp.minRoomWidth + 1));
        const int rh = bsp.minRoomHeight + static_cast<int>(draw(rng, a.h - 2 - bsp.minRoomHeight + 1));
        const int rx = a.x + 1 + static_cast<int>(draw(rng, a.w - 2 - rw + 1));
        const int ry = a.y + 1 + static_cast<int>(draw(rng, a.h - 2 - rh + 1));
        for (int y = ry; y < ry + rh; ++y)
            for (int x = rx; x < rx + rw; ++x)
                tileAt(f, {x, y}) = Tile::Floor;
        leaf->room = static_cast<int>(f.rooms.size());
        f.rooms.push_back({rx, ry, rw, rh});
    }
    connect(*root, f, rng);

    // Step 4: arrive in the first room; the stairs (or the elite) in the farthest room.
    f.entry = center(f.rooms[0]);
    if (floorIndex == 1)
        tileAt(f, f.entry) = Tile::StairsUp;
    const std::vector<int> dist = bfsDistances(f, f.entry);
    const auto distAt = [&](Pos p) { return dist[static_cast<std::size_t>(p.y * f.width + p.x)]; };
    Pos farthest = f.entry;
    for (const Rect& room : f.rooms)
        if (distAt(center(room)) > distAt(farthest))
            farthest = center(room);
    std::optional<Pos> eliteSpot;
    if (last)
        eliteSpot = farthest;
    else {
        f.stairsDown = farthest;
        tileAt(f, farthest) = Tile::StairsDown;
    }

    // Step 5: enemies, away from the entry.
    std::vector<Pos> candidates;
    for (int y = 0; y < f.height; ++y)
        for (int x = 0; x < f.width; ++x)
            if (const Pos p{x, y}; f.at(p) == Tile::Floor && distAt(p) > def.safeRadius && p != eliteSpot)
                candidates.push_back(p);
    if (eliteSpot)
        f.enemies.push_back({def.elite.enemy, *eliteSpot, false, 0, true});
    const EnemyPool& pool = def.poolFor(floorIndex);
    int totalWeight = 0;
    for (const WeightedEnemy& e : pool.enemies)
        totalWeight += e.weight;
    const int enemies = def.enemyCount.base + def.enemyCount.perFloor * (floorIndex - 1) +
                        static_cast<int>(draw(rng, def.enemyCount.random));
    for (int i = 0; i < enemies && !candidates.empty(); ++i) {
        const Pos at = takeRandom(candidates, rng);
        int r = static_cast<int>(draw(rng, totalWeight));
        auto kind = pool.enemies.begin();
        while (r >= kind->weight)
            r -= (kind++)->weight;
        f.enemies.push_back({kind->id, at});
    }

    // Step 6: chests, from what's left.
    const int chests = def.chestCount.base + static_cast<int>(draw(rng, def.chestCount.random)) +
                       (last ? def.chestCount.lastFloorBonus : 0);
    for (int i = 0; i < chests && !candidates.empty(); ++i)
        f.chests.push_back({takeRandom(candidates, rng)});
    return f;
}

std::vector<int> bfsDistances(const Floor& floor, Pos from) {
    std::vector<int> dist(floor.tiles.size(), -1);
    const auto index = [&](Pos p) { return static_cast<std::size_t>(p.y * floor.width + p.x); };
    std::vector<Pos> queue{from};
    dist[index(from)] = 0;
    for (std::size_t head = 0; head < queue.size(); ++head) {
        const Pos p = queue[head];
        for (const Pos n : {Pos{p.x, p.y - 1}, Pos{p.x + 1, p.y}, Pos{p.x, p.y + 1}, Pos{p.x - 1, p.y}})
            if (floor.walkable(n) && dist[index(n)] < 0) {
                dist[index(n)] = dist[index(p)] + 1;
                queue.push_back(n);
            }
    }
    return dist;
}

std::string renderAscii(const Floor& floor) {
    std::string rows;
    for (int y = 0; y < floor.height; ++y) {
        for (int x = 0; x < floor.width; ++x) {
            const Pos p{x, y};
            char c = "#.><"[static_cast<int>(floor.at(p))];
            for (const MapChest& chest : floor.chests)
                if (chest.pos == p)
                    c = '$';
            for (const MapEnemy& e : floor.enemies)
                if (e.pos == p)
                    c = e.elite ? 'E' : 'e';
            rows += c;
        }
        rows += '\n';
    }
    return rows;
}

} // namespace heroes::game::dungeon
