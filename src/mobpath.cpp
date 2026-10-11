// ===========================================================================
// mobpath.cpp — 原版寻路移植（对照 26.2：PathFinder/WalkNodeEvaluator 等）
// ===========================================================================
#include "mobpath.hpp"
#include "blocks.hpp"
#include "mobs.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace mobpath {

float malusOf(PathType t) {
    switch (t) {
    case PathType::BLOCKED:          return -1.0f;
    case PathType::OPEN:             return 0.0f;
    case PathType::WALKABLE:         return 0.0f;
    case PathType::WATER:            return 8.0f;
    case PathType::WATER_BORDER:     return 8.0f;
    case PathType::LAVA:             return -1.0f;
    case PathType::FIRE:             return 16.0f;
    case PathType::FIRE_IN_NEIGHBOR: return 8.0f;
    case PathType::FENCE:            return -1.0f;
    case PathType::LEAVES:           return -1.0f;
    case PathType::DAMAGING:         return -1.0f;
    case PathType::DAMAGE_CAUTIOUS:  return 0.0f;
    }
    return 0.0f;
}

// ---- Node ----
float Node::distanceTo(const Node& o) const {
    float dx = (float)(o.x - x), dy = (float)(o.y - y), dz = (float)(o.z - z);
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}
float Node::distanceManhattan(const Node& o) const {
    return (float)(std::abs(o.x - x) + std::abs(o.y - y) + std::abs(o.z - z));
}

// ---- BinaryHeap（BinaryHeap.java）----
Node* BinaryHeap::insert(Node* n) {
    if (heap_.size() == heap_.capacity()) heap_.reserve(heap_.capacity() * 2 + 128);
    heap_.push_back(n);
    int idx = (int)heap_.size() - 1;
    upHeap(idx);
    return n;
}
void BinaryHeap::upHeap(int idx) {
    Node* n = heap_[idx];
    float cost = n->f;
    while (idx > 0) {
        int parent = (idx - 1) >> 1;
        Node* p = heap_[parent];
        if (!(cost < p->f)) break;
        heap_[idx] = p;
        p->heapIdx = idx;
        idx = parent;
    }
    heap_[idx] = n;
    n->heapIdx = idx;
}
void BinaryHeap::downHeap(int idx) {
    Node* n = heap_[idx];
    float cost = n->f;
    int size = (int)heap_.size();
    while (true) {
        int left = 1 + (idx << 1);
        if (left >= size) break;
        int right = left + 1;
        int best = left;
        if (right < size && heap_[right]->f < heap_[left]->f) best = right;
        if (!(heap_[best]->f < cost)) break;
        heap_[idx] = heap_[best];
        heap_[idx]->heapIdx = idx;
        idx = best;
    }
    heap_[idx] = n;
    n->heapIdx = idx;
}
Node* BinaryHeap::pop() {
    Node* top = heap_[0];
    heap_[0] = heap_.back();
    heap_.pop_back();
    if (!heap_.empty()) downHeap(0);
    top->heapIdx = -1;
    return top;
}
void BinaryHeap::changeCost(Node* n, float newCost) {
    float old = n->f;
    n->f = newCost;
    if (newCost < old) upHeap(n->heapIdx);
    else downHeap(n->heapIdx);
}

// ---- Path ----
void Path::getNextEntityPos(float bbWidth, float& x, float& y, float& z) const {
    const Node* n = nodes[next];
    x = (float)n->x + (int)(bbWidth + 1.0f) * 0.5f;
    y = (float)n->y;
    z = (float)n->z + (int)(bbWidth + 1.0f) * 0.5f;
}
void Path::getNextNodePos(int& x, int& y, int& z) const {
    const Node* n = nodes[next];
    x = n->x; y = n->y; z = n->z;
}

// ---- NodeEvaluator ----
NodeEvaluator::NodeEvaluator() {
    for (int i = 0; i < 12; i++) malusOverride_[i] = malusOf((PathType)i);
}

void NodeEvaluator::prepare(World& w, const Entity& mob) {
    w_ = &w;
    mob_ = &mob;
    pool_.clear();
    poolMap_.clear();
    typeCache_.clear();
    entityWidth_ = (int)std::floor(mob.type->width + 1.0f);
    entityHeight_ = (int)std::floor(mob.type->height + 1.0f);
    entityDepth_ = entityWidth_;
}
void NodeEvaluator::done() {
    w_ = nullptr;
    mob_ = nullptr;
    pool_.clear();
    poolMap_.clear();
    typeCache_.clear();
}
Node* NodeEvaluator::getNode(int x, int y, int z) {
    uint64_t k = Node::key(x, y, z);
    auto it = poolMap_.find(k);
    if (it != poolMap_.end()) return it->second;
    pool_.emplace_back();
    Node* n = &pool_.back();
    n->x = x; n->y = y; n->z = z;
    poolMap_[k] = n;
    return n;
}

// 原版 getPathTypeStatic：先看本格，再综合"下方一格"决定是否可走
static PathType getPathTypeFromState(World& w, int x, int y, int z) {
    uint8_t b = w.getBlock(x, y, z);
    if (b == B_AIR) return PathType::OPEN;
    if (b == B_WATER) return PathType::WATER;
    if (b == B_LAVA) return PathType::LAVA;
    if (b == B_FIRE) return PathType::FIRE;
    if (b == B_IRON_BARS) return PathType::FENCE;
    if (b == B_LEAVES) return PathType::LEAVES;
    if (b == B_TORCH) return PathType::OPEN;   // 火把不挡路（原版非碰撞体）
    if (b == B_CHORUS_PLANT || b == B_CHORUS_FLOWER || b == B_CHORUS_FLOWER_DEAD)
        return PathType::DAMAGE_CAUTIOUS;       // 可站但没必要（近似原版植物）
    return PathType::BLOCKED;                    // 其余都当碰撞方块
}

PathType NodeEvaluator::getPathType(int x, int y, int z) {
    PathType t = getPathTypeFromState(*w_, x, y, z);
    if (t != PathType::OPEN) return t;
    // 原版：本格 OPEN 时看下方 —— 实心则是 WALKABLE，再按邻居修正
    PathType below = getPathTypeFromState(*w_, x, y - 1, z);
    switch (below) {
    case PathType::OPEN: case PathType::WATER: case PathType::LAVA: case PathType::WALKABLE:
        return PathType::OPEN;
    case PathType::FIRE: return PathType::FIRE;
    case PathType::DAMAGING: return PathType::DAMAGING;
    default: break;
    }
    // checkNeighbourBlocks：邻着火/岩浆/水要加扣分
    for (int dx = -1; dx <= 1; dx++)
        for (int dz = -1; dz <= 1; dz++) {
            if (dx == 0 && dz == 0) continue;
            PathType nt = getPathTypeFromState(*w_, x + dx, y, z + dz);
            if (nt == PathType::LAVA || nt == PathType::FIRE) return PathType::FIRE_IN_NEIGHBOR;
            if (nt == PathType::WATER) return PathType::WATER_BORDER;
        }
    return PathType::WALKABLE;
}

// 单格类型的缓存：一个格子的 PathType 只算一次（含 9 邻域扫描，很贵），
// 实体包围盒查询复用缓存 —— 一次 A* 里同一格常被查十几次。
PathType NodeEvaluator::getCachedCellType(int x, int y, int z) {
    uint64_t k = Node::key(x, y, z);
    auto it = typeCache_.find(k);
    if (it != typeCache_.end()) return it->second;
    PathType t = getPathType(x, y, z);
    typeCache_[k] = t;
    return t;
}

PathType NodeEvaluator::getCachedPathType(int x, int y, int z) {
    // getPathTypeOfMob 的结果不能按单格缓存（依赖实体尺寸），但内部走单格缓存
    return getPathTypeOfMob(x, y, z);
}

PathType NodeEvaluator::getPathTypeOfMob(int x, int y, int z) {
    // 原版 getPathTypeWithinMobBB + getPathTypeOfMob：
    //   收集实体包围盒覆盖的所有格子的类型（EnumSet），
    //   再**按 PathType 的枚举顺序**遍历，扣分最大者胜出。
    //   ⚠ 不能按"空间遍历顺序"收集就取最后/最大 —— OPEN(1) 排在 WALKABLE(2) 前面，
    //   按枚举序 WALKABLE 才会赢；否则站在地上的怪会被判成"悬空 OPEN"，
    //   寻路直接找不到起点邻居（A* 一个节点都扩不出来）。
    bool has[12] = {false};
    for (int dx = 0; dx < entityWidth_; dx++)
        for (int dy = 0; dy < entityHeight_; dy++)
            for (int dz = 0; dz < entityDepth_; dz++) {
                PathType t = getCachedCellType(x + dx, y + dy, z + dz);
                if (malusOfMob(t) < 0.0f) return t;    // 有致命类型直接返回
                has[(int)t] = true;
            }
    PathType highest = PathType::BLOCKED;
    float highestMalus = malusOfMob(highest);
    for (int i = 0; i < 12; i++) {
        if (!has[i]) continue;
        float m = malusOfMob((PathType)i);
        if (m >= highestMalus) { highestMalus = m; highest = (PathType)i; }
    }
    PathType curType = getCachedCellType(x, y, z);
    if (entityWidth_ > 1) return highest;
    return (curType == PathType::OPEN && highest != PathType::OPEN && highestMalus == 0.0f)
               ? PathType::OPEN : highest;
}

double NodeEvaluator::getFloorLevel(int x, int y, int z) {
    // 原版 getFloorLevel(level, pos)：pos 下方方块的碰撞顶面
    return (double)y;   // 本工程方块都是整格碰撞，顶面即 y
}

Node* NodeEvaluator::getStart() {
    // 原版 getStart 的地面分支：onGround 时 y = floor(y + 0.5)
    int sx = (int)std::floor(mob_->pos.x);
    int sz = (int)std::floor(mob_->pos.z);
    int sy = (int)std::floor(mob_->pos.y + 0.5f);
    if (canStartAt(sx, sy, sz)) return getStartNodeAt(sx, sy, sz);
    // 四角尝试（原版对包围盒四角各试一次）
    float hw = mob_->type->width * 0.5f;
    float px = mob_->pos.x, pz = mob_->pos.z;
    const float corners[4][2] = {
        {px - hw, pz - hw}, {px - hw, pz + hw}, {px + hw, pz - hw}, {px + hw, pz + hw},
    };
    for (auto& c : corners)
        if (canStartAt((int)std::floor(c[0]), sy, (int)std::floor(c[1])))
            return getStartNodeAt((int)std::floor(c[0]), sy, (int)std::floor(c[1]));
    return getStartNodeAt(sx, sy, sz);
}
Node* NodeEvaluator::getStartNodeAt(int x, int y, int z) {
    Node* n = getNode(x, y, z);
    n->type = getCachedPathType(x, y, z);
    n->costMalus = malusOfMob(n->type);
    return n;
}
bool NodeEvaluator::canStartAt(int x, int y, int z) {
    PathType t = getCachedPathType(x, y, z);
    return t != PathType::OPEN && malusOfMob(t) >= 0.0f;
}

Node* NodeEvaluator::getTarget(int x, int y, int z) {
    Node* n = getNode(x, y, z);
    n->bestHeuristic = 3.0e38f;   // Target
    return n;
}

int NodeEvaluator::getNeighbors(Node* out[32], Node* pos) {
    int p = 0;
    // 跳跃高度：maxUpStep 以上才需要跳（本工程生物 maxUpStep=0.6 < 1 → 跳 1 格）
    PathType above = getCachedPathType(pos->x, pos->y + 1, pos->z);
    PathType cur = getCachedPathType(pos->x, pos->y, pos->z);
    int jumpSize = (malusOfMob(above) >= 0.0f && cur != PathType::BLOCKED)
                       ? (int)std::floor(std::max(1.0f, 0.6f)) : 0;
    double posHeight = getFloorLevel(pos->x, pos->y, pos->z);

    static const int DIRS[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};   // 东 西 南 北
    Node* sides[4] = {nullptr, nullptr, nullptr, nullptr};
    for (int d = 0; d < 4; d++) {
        Node* n = findAcceptedNode(pos->x + DIRS[d][0], pos->y, pos->z + DIRS[d][1],
                                   jumpSize, posHeight, d, cur);
        if (getenv("OPENCRAFT_PATH_DEBUG"))
            fprintf(stderr, "[nb] d=%d jump=%d cur=%d (%d,%d,%d)->%p type=%d malus=%.1f closed=%d\n",
                    d, jumpSize, (int)cur, pos->x + DIRS[d][0], pos->y, pos->z + DIRS[d][1],
                    (void*)n, n ? (int)n->type : -1, n ? n->costMalus : -9.0f,
                    n ? (n->closed ? 1 : 0) : -1);
        sides[d] = n;
        if (isNeighborValid(n, pos)) out[p++] = n;
    }
    // 对角：两个正交邻居都可走才试（原版 isDiagonalValid）
    for (int d = 0; d < 4; d++) {
        int d2 = (d + 1) & 3;   // 顺时针
        if (isDiagonalValidPair(pos, sides[d], sides[d2])) {
            Node* n = findAcceptedNode(pos->x + DIRS[d][0] + DIRS[d2][0], pos->y,
                                       pos->z + DIRS[d][1] + DIRS[d2][1],
                                       jumpSize, posHeight, d, cur);
            if (isDiagonalValidNode(n)) out[p++] = n;
        }
    }
    return p;
}

bool NodeEvaluator::isNeighborValid(Node* n, Node* cur) {
    return n && !n->closed && (n->costMalus >= 0.0f || cur->costMalus < 0.0f);
}
bool NodeEvaluator::isDiagonalValidPair(Node* pos, Node* ew, Node* ns) {
    if (!ns || !ew || ns->y > pos->y || ew->y > pos->y) return false;
    if (mob_->type->width > 1.0f && (ew->costMalus > 0.0f || ns->costMalus > 0.0f)) return false;
    return (ns->y < pos->y || ns->costMalus >= 0.0f) && (ew->y < pos->y || ew->costMalus >= 0.0f);
}
bool NodeEvaluator::isDiagonalValidNode(Node* n) {
    return n && !n->closed && n->costMalus >= 0.0f;
}

Node* NodeEvaluator::findAcceptedNode(int x, int y, int z, int jumpSize, double nodeHeight,
                                      int dirIdx, PathType cur) {
    Node* best = nullptr;
    double maxYTarget = getFloorLevel(x, y, z);
    if (maxYTarget - nodeHeight > 1.125) return nullptr;   // getMobJumpHeight()

    PathType pathType = getCachedPathType(x, y, z);
    float pathCost = malusOfMob(pathType);
    if (pathCost >= 0.0f) {
        best = getNode(x, y, z);
        best->type = pathType;
        best->costMalus = std::max(best->costMalus, pathCost);
    }
    if (pathType != PathType::WALKABLE) {
        if ((best == nullptr || best->costMalus < 0.0f) && jumpSize > 0 && pathType != PathType::FENCE) {
            best = tryJumpOn(x, y, z, jumpSize, nodeHeight, dirIdx, cur);
        } else if (pathType == PathType::WATER && !canFloat_) {
            // 原版 tryFindFirstNonWaterBelow：水底找可站节点（本工程按空气处理略过）
            best = best;
        } else if (pathType == PathType::OPEN) {
            // 悬空：向下找地面节点，超过最大摔落距离就堵死
            Node* dn = nullptr;
            int cy = y - 1;
            for (; cy >= 0; cy--) {
                if (y - cy > 3) {   // getMaxFallDistance ≈ 3（多数生物）
                    dn = getNode(x, cy, z);
                    dn->type = PathType::BLOCKED;
                    dn->costMalus = -1.0f;
                    return dn;
                }
                PathType pt = getCachedPathType(x, cy, z);
                if (pt != PathType::OPEN) {
                    float pc = malusOfMob(pt);
                    dn = getNode(x, cy, z);
                    dn->type = pt;
                    dn->costMalus = pc >= 0.0f ? std::max(dn->costMalus, pc) : -1.0f;
                    return dn;
                }
            }
            dn = getNode(x, y, z);
            dn->type = PathType::BLOCKED;
            dn->costMalus = -1.0f;
            return dn;
        }
    }
    return best;
}

Node* NodeEvaluator::tryJumpOn(int x, int y, int z, int jumpSize, double nodeHeight,
                               int dirIdx, PathType cur) {
    // 原版：先看上方一格能不能站，再看跳跃过程中会不会撞头
    Node* above = findAcceptedNode(x, y + 1, z, jumpSize - 1, nodeHeight, dirIdx, cur);
    if (above == nullptr) return nullptr;
    if (mob_->type->width >= 1.0f) return above;
    if (above->type != PathType::OPEN && above->type != PathType::WALKABLE) return above;
    // 细实体：检查跳上去之后头部空间（当前格到目标格之间）
    static const int DIRS[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
    float cx = (float)x - DIRS[dirIdx][0] + 0.5f;
    float cz = (float)z - DIRS[dirIdx][1] + 0.5f;
    float hw = mob_->type->width * 0.5f;
    float h = mob_->type->height;
    if (hasCollisions(cx - hw, (float)y + 1.001f, cz - hw,
                      cx + hw, (float)y + 1.0f + h - 0.002f, cz + hw))
        return nullptr;
    return above;
}

bool NodeEvaluator::hasCollisions(float x0, float y0, float z0, float x1, float y1, float z1) {
    return mobs::boxSolid(*w_, x0, y0, z0, x1, y1, z1);
}

// ---- PathFinder（PathFinder.findPath 的忠实移植）----
Path PathFinder::findPath(World& w, const Entity& mob,
                          const std::vector<BlockPos3>& targets,
                          float maxPathLength, int reachRange, NodeEvaluator& eval) {
    openSet_.clear();
    (void)w;
    Node* from = eval.getStart();
    if (!from) return {};

    // 每个目标一个 Target 节点
    std::vector<Node*> tos;
    tos.reserve(targets.size());
    for (auto& t : targets) tos.push_back(eval.getTarget(t.x, t.y, t.z));

    from->g = 0.0f;
    from->cameFrom = nullptr;
    from->closed = false;
    from->walkedDistance = 0.0f;
    // getBestH
    float bestH = 3.0e38f;
    for (Node* t : tos) {
        float h = from->distanceTo(*t);
        if (h < t->bestHeuristic) { t->bestHeuristic = h; t->bestNode = from; }
        bestH = std::min(bestH, h);
    }
    from->h = bestH;
    from->f = from->h;
    openSet_.insert(from);

    int count = 0;
    std::vector<Node*> reachedTargets;
    int maxVisited = (int)(maxVisitedNodes_);

    while (!openSet_.empty()) {
        if (++count >= maxVisited) break;
        Node* current = openSet_.pop();
        current->closed = true;
        for (Node* t : tos)
            if (current->distanceManhattan(*t) <= (float)reachRange) {
                t->reached = true;
                reachedTargets.push_back(t);
            }
        if (!reachedTargets.empty()) break;
        if (current->distanceTo(*from) < maxPathLength) {
            Node* neighbors[32];
            int n = eval.getNeighbors(neighbors, current);
            for (int i = 0; i < n; i++) {
                Node* nb = neighbors[i];
                float dist = current->distanceTo(*nb);
                nb->walkedDistance = current->walkedDistance + dist;
                float tentative = current->g + dist + nb->costMalus;
                if (nb->walkedDistance < maxPathLength && (!nb->inOpenSet() || tentative < nb->g)) {
                    nb->cameFrom = current;
                    nb->g = tentative;
                    float h = 3.0e38f;
                    for (Node* t : tos) {
                        float hh = nb->distanceTo(*t);
                        if (hh < t->bestHeuristic) { t->bestHeuristic = hh; t->bestNode = nb; }
                        h = std::min(h, hh);
                    }
                    nb->h = h * 1.5f;
                    if (nb->inOpenSet()) openSet_.changeCost(nb, nb->g + nb->h);
                    else { nb->f = nb->g + nb->h; openSet_.insert(nb); }
                }
            }
        }
    }

    // 重建路径：到过目标取最短；没到过取离目标最近的
    Node* bestEnd = nullptr;
    if (!reachedTargets.empty()) {
        int bestCount = 1 << 30;
        for (Node* t : reachedTargets) {
            if (!t->bestNode) continue;
            // 数节点数
            int c = 0;
            for (Node* n = t->bestNode; n; n = n->cameFrom) c++;
            if (c < bestCount) { bestCount = c; bestEnd = t->bestNode; }
        }
    } else {
        float bestDist = 3.0e38f;
        for (Node* t : tos) {
            if (!t->bestNode) continue;
            float d = t->bestNode->distanceManhattan(*t);
            if (d < bestDist) { bestDist = d; bestEnd = t->bestNode; }
        }
    }
    if (!bestEnd) return {};

    Path path;
    for (Node* n = bestEnd; n; n = n->cameFrom)
        path.nodes.push_back(n);
    std::reverse(path.nodes.begin(), path.nodes.end());
    path.reached = !reachedTargets.empty();
    path.target = bestEnd;
    path.next = 1;   // 第 0 个是脚下
    if (path.nodes.size() <= 1) path.next = 1;
    return path;
}

} // namespace mobpath
