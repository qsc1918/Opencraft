#pragma once
// ===========================================================================
// mobpath.hpp — 原版寻路的移植（net/minecraft/world/level/pathfinder/*）
//
// 逐条对照 26.2 反编译源码：Node / Target / BinaryHeap / Path / PathFinder /
// WalkNodeEvaluator。方块世界换成 World::getBlock；碰撞用 mobs::boxSolid。
// 坐标系：方块整数坐标（Node.x/y/z 是"脚下方块"的坐标），与原版一致。
// ===========================================================================
#include "entity.hpp"
#include <cstdint>
#include <deque>
#include <unordered_map>
#include <vector>

namespace mobpath {

// 目标方块坐标
struct BlockPos3 { int x = 0, y = 0, z = 0; };

// PathType：值与原版 PathType 的默认扣分一致（PathType.java）
enum class PathType : uint8_t {
    BLOCKED,        // -1 不可通行
    OPEN,           // 0   空气（悬空）
    WALKABLE,       // 0   可站立的地面节点
    WATER,          // 8   水（地面生物默认会避开，但可游）
    WATER_BORDER,   // 8   紧邻水
    LAVA,           // -1
    FIRE,           // 16
    FIRE_IN_NEIGHBOR, // 8
    FENCE,          // -1  （铁栏杆/栅栏：不可越过）
    LEAVES,         // -1
    DAMAGING,       // -1  （仙人掌等，本工程暂无）
    DAMAGE_CAUTIOUS,// 0
};
float malusOf(PathType t);   // 原版 PathType 枚举自带的默认扣分

// ---- Node / Target（Target 是带"最佳节点"记忆的 Node）----
struct Node {
    int x = 0, y = 0, z = 0;
    int heapIdx = -1;
    float g = 0.0f, h = 0.0f, f = 0.0f;
    Node* cameFrom = nullptr;
    bool closed = false;
    float walkedDistance = 0.0f;
    float costMalus = 0.0f;
    PathType type = PathType::BLOCKED;
    // Target 扩展字段（原版 Target extends Node）
    float bestHeuristic = 3.0e38f;
    Node* bestNode = nullptr;
    bool reached = false;

    static uint64_t key(int x, int y, int z) {
        return (uint64_t)(uint32_t)(x + 32768) << 20
             | (uint64_t)(uint32_t)(z + 32768) << 40
             | (uint64_t)(uint16_t)(int16_t)y;
    }
    float distanceTo(const Node& o) const;
    float distanceManhattan(const Node& o) const;
    bool inOpenSet() const { return heapIdx >= 0; }
};

// ---- 二叉堆（BinaryHeap.java：按 f 排序的最小堆）----
class BinaryHeap {
public:
    Node* insert(Node* n);
    Node* pop();
    void changeCost(Node* n, float newCost);
    void clear() { heap_.clear(); }
    bool empty() const { return heap_.empty(); }
private:
    void upHeap(int i);
    void downHeap(int i);
    std::vector<Node*> heap_;
};

// ---- Path（找到的路径；节点指针归 NodeEvaluator 的节点池所有）----
class Path {
public:
    std::vector<Node*> nodes;
    int next = 0;
    Node* target = nullptr;      // 路径终点（供 navigation 记录 targetPos）
    bool reached = false;

    bool isDone() const { return next >= (int)nodes.size(); }
    void advance() { next++; }
    Node* getNextNode() const { return nodes[next]; }
    // 原版 getEntityPosAtNode：节点中心向 bbWidth 偏移半格
    void getNextEntityPos(float bbWidth, float& x, float& y, float& z) const;
    void getNextNodePos(int& x, int& y, int& z) const;
};

// ---- A*（PathFinder.java）----
class PathFinder {
public:
    explicit PathFinder(int maxVisitedNodes) : maxVisitedNodes_(maxVisitedNodes) {}
    // targets：目标方块列表（可多个）。找到返回路径；找不到返回"最接近"的路径或 nullptr。
    struct Result { Path path; bool ok; };
    Path findPath(class World& w, const Entity& mob,
                  const std::vector<BlockPos3>& targets,
                  float maxPathLength, int reachRange, class NodeEvaluator& eval);

private:
    int maxVisitedNodes_;
    BinaryHeap openSet_;
};

// ---- WalkNodeEvaluator（节点生成与地形判定）----
class NodeEvaluator {
public:
    NodeEvaluator();   // 初始化 PathType → 扣分表
    void prepare(World& w, const Entity& mob);
    void done();
    Node* getStart();
    Node* getTarget(int x, int y, int z);
    int getNeighbors(Node* out[32], Node* pos);
    PathType getPathType(int x, int y, int z);
    PathType getPathTypeOfMob(int x, int y, int z);
    // 原版 getFloorLevel：pos 下方碰撞体的顶面高度
    double getFloorLevel(int x, int y, int z);
    void setCanFloat(bool v) { canFloat_ = v; }
    bool canFloat() const { return canFloat_; }
    float malusOfMob(PathType t) const { return malusOverride_[(int)t]; }
    // 覆盖某类型的路径扣分（原版 Mob.setPathfindingMalus）
    void setMalus(PathType t, float m) { malusOverride_[(int)t] = m; }

private:
    Node* getNode(int x, int y, int z);
    Node* getStartNodeAt(int x, int y, int z);
    bool canStartAt(int x, int y, int z);
    Node* findAcceptedNode(int x, int y, int z, int jumpSize, double nodeHeight, int dirIdx, PathType cur);
    Node* tryJumpOn(int x, int y, int z, int jumpSize, double nodeHeight, int dirIdx, PathType cur);
    Node* tryFindFirstGroundNodeBelow(int x, int y, int z);
    bool isNeighborValid(Node* n, Node* cur);
    bool isDiagonalValidPair(Node* pos, Node* ew, Node* ns);
    bool isDiagonalValidNode(Node* n);
    bool hasCollisions(float x0, float y0, float z0, float x1, float y1, float z1);
    PathType getCachedPathType(int x, int y, int z);
    PathType getCachedCellType(int x, int y, int z);   // 单格类型缓存（含邻域扫描）

    World* w_ = nullptr;
    const Entity* mob_ = nullptr;
    std::deque<Node> pool_;                          // 节点池（deque：指针稳定）
    std::unordered_map<uint64_t, Node*> poolMap_;    // key → 节点
    int entityWidth_ = 1, entityHeight_ = 1, entityDepth_ = 1;
    bool canFloat_ = false;
    float malusOverride_[12];                        // 下标 = PathType
    std::unordered_map<uint64_t, PathType> typeCache_;
};

} // namespace mobpath
