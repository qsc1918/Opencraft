#include "blockupdate.hpp"
#include "blocks.hpp"
#include "entities.hpp"
#include "specs.hpp"
#include "world.hpp"
#include <algorithm>
#include <memory>
#include <vector>

// 实现要点（对照 方块更新.txt）：
//  * 更新必须由一次方块放置/破坏/状态变化产生，所以入口只有 World::setBlock。
//  * NC 更新核的传播顺序是 西、东、下、上、北、南；PP 是 西、东、北、南、下、上
//  * 传播走深度优先：栈里每帧只推进一个方向，过程中新产生的更新先入 pending，
//    下一轮按"后入先出"压栈。
//  * pending/budget 框架对应原版 max-chained-neighbor-updates，但本工程的接收方
//    只做计划刻入队、不产生新的邻居更新，所以上限与去重当前都未接通；接红石/流体
//    更新时必须把 kMaxUpdates 与"同坐标不重复入队"一起补上
//  * 接收方只实现本工程已有方块会受影响的行为（受重力、附着、支撑），
//    红石/流体等没有对应方块的部分不实现，receiveNC 目前是空操作。

namespace {

enum : uint8_t { K_NC = 0, K_PP = 1 };

// [更新种类][方向序号] = (dx,dy,dz)
constexpr int kDirs[2][6][3] = {
    {{-1, 0, 0}, {1, 0, 0}, {0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}}, // NC
    {{-1, 0, 0}, {1, 0, 0}, {0, 0, -1}, {0, 0, 1}, {0, -1, 0}, {0, 1, 0}}, // PP
};

struct Frame { int32_t x, y, z; uint8_t kind; uint8_t dir; };

// 单次方块改动的传播上限：沙子整片坍塌也是这个量级，够用。
constexpr int kMaxUpdates = 65536;

// 原版按"上表面是否完整"(isFaceFull) 判断能不能支撑；本工程碰撞箱都是整格，
// 实心方块即可支撑（玻璃/树叶在原版也算完整上表面，行为一致）。
bool canSupport(const World& w, int x, int y, int z) {
    return blockIsSolid(w.getBlock(x, y, z));
}

bool isChorusPart(uint8_t b) {
    return b == B_CHORUS_PLANT || b == B_CHORUS_FLOWER || b == B_CHORUS_FLOWER_DEAD;
}

// 紫颂：等价于原版 ChorusPlantBlock#canSurvive / ChorusFlowerBlock#canSurvive
bool plantCanSurvive(const World& w, int x, int y, int z) {
    uint8_t below = w.getBlock(x, y - 1, z);
    bool aboveAndBelow = w.getBlock(x, y + 1, z) != B_AIR && below != B_AIR;
    for (const auto& o : kDir4) {
        int nx = x + o[0], nz = z + o[1];
        if (w.getBlock(nx, y, nz) == B_CHORUS_PLANT) {
            if (aboveAndBelow) return false;
            uint8_t b2 = w.getBlock(nx, y - 1, nz);
            if (b2 == B_CHORUS_PLANT || b2 == B_END_STONE) return true;
        }
    }
    return below == B_CHORUS_PLANT || below == B_END_STONE;
}

bool flowerCanSurvive(const World& w, int x, int y, int z) {
    uint8_t below = w.getBlock(x, y - 1, z);
    if (below != B_CHORUS_PLANT && below != B_END_STONE) {
        if (below != B_AIR) return false;
        bool one = false;
        for (const auto& o : kDir4) {
            uint8_t nb = w.getBlock(x + o[0], y, z + o[1]);
            if (nb == B_CHORUS_PLANT) {
                if (one) return false;
                one = true;
            } else if (nb != B_AIR) {
                return false;
            }
        }
        return one;
    }
    return true;
}

// 接收
// NC 更新：原版接收方全是红石元件与流体，本工程都还没有，暂不实现。
void receiveNC(World&, int, int, int) {
    // 红石线/中继器/比较器/投掷器…未实现；流体流动也未实现。
}

// PP 更新：形状与附着性检查（原版 received 列表里与本工程相关的几条）
void receivePP(World& w, int x, int y, int z) {
    uint8_t id = w.getBlock(x, y, z);
    if (id == B_AIR) return;
    // 受重力方块：支撑不住就排 1 刻后变成下落的方块（原版 SandBlock#tick 延迟 2 刻，
    // 本工程 delay=1 实际也是 2 刻后到期：先 delay-- 再判 <=0，见 runScheduledTicks）
    if (bu::isFallingBlock(id)) {
        if (y > WORLD_MIN_Y && !canSupport(w, x, y - 1, z)) bu::scheduleTick(w, x, y, z, 1);
        return;
    }
    // 附着性方块（火把/紫颂/火）：不能留就排 1 刻后破坏（原版延迟 1 刻）
    if (!bu::blockCanSurvive(w, x, y, z)) bu::scheduleTick(w, x, y, z, 1);
}

void propagate(World& w, uint8_t kind, int x, int y, int z) {
    std::vector<Frame> stack;
    std::vector<Frame> pending;
    // 同一坐标只保留一帧：6 个邻居都会往同一格排更新，不去重队列会涨爆。
    // key 用 x/z 各 16 位 + y 8 位，坐标超范围时退化为不去重（不会漏更新）。
    auto frameKey = [](int fx, int fy, int fz) -> uint64_t {
        if (fx < -32768 || fx > 32767 || fz < -32768 || fz > 32767) return 0;
        return ((uint64_t)(uint32_t)(fx + 32768) << 40) |
               ((uint64_t)(uint32_t)(fz + 32768) << 24) |
               (uint64_t)(uint32_t)(fy + 128);
    };
    std::vector<uint64_t> seen;
    int budget = kMaxUpdates;
    auto push = [&](const Frame& f) {
        if (budget <= 0) return;
        uint64_t k = frameKey(f.x, f.y, f.z);
        if (k != 0) {
            for (uint64_t s : seen) if (s == k) return;   // 已在队列里
            seen.push_back(k);
        }
        budget--;
        pending.push_back(f);
    };
    // 起点直接入队，不走 push 的去重（它会把起点自己挡掉）。
    budget--;
    pending.push_back({x, y, z, kind, 0});
    seen.push_back(frameKey(x, y, z));
    while (!stack.empty() || !pending.empty()) {
        if (!pending.empty()) {
            // pending 是"本帧新产生的更新"，倒序压栈让最后产生的先执行 → 深度优先。
            for (size_t i = pending.size(); i-- > 0;) stack.push_back(pending[i]);
            pending.clear();
        }
        while (true) {
            Frame& top = stack.back();
            if (top.dir >= 6) { stack.pop_back(); break; }
            const int* d = kDirs[top.kind][top.dir++];
            int nx = top.x + d[0], ny = top.y + d[1], nz = top.z + d[2];
            if (ny < WORLD_MIN_Y || ny >= WORLD_HEIGHT) continue;
            if (top.kind == K_NC) receiveNC(w, nx, ny, nz);
            else receivePP(w, nx, ny, nz);
            if (!pending.empty()) break;   // 有新更新：回到外层先压栈
        }
    }
}

void spawnFalling(World& w, int x, int y, int z, uint8_t id) {
    if (y <= WORLD_MIN_Y) return;
    // 移除原方块（会产生一轮更新，沙子堆就是这么一层层塌下去的）
    w.setBlock(x, y, z, B_AIR);
    auto fb = std::make_unique<FallingBlock>();
    fb->block = id;
    fb->pos = Vec3(x + 0.5f, (float)y, z + 0.5f);   // pos 是脚部（底面）中心
    w.spawnEntity(std::move(fb));
}

} // 匿名命名空间

namespace bu {

bool isFallingBlock(uint8_t id) {
    // 原版受重力列表里本工程已有的：沙子、沙砾、龙蛋。
    return id == B_SAND || id == B_GRAVEL || id == B_DRAGON_EGG;
}

bool chorusPlantCanSurvive(const World& w, int x, int y, int z) {
    return plantCanSurvive(w, x, y, z);
}

bool chorusFlowerCanSurvive(const World& w, int x, int y, int z) {
    return flowerCanSurvive(w, x, y, z);
}

bool blockCanSurvive(const World& w, int x, int y, int z) {
    uint8_t id = w.getBlock(x, y, z);
    switch (id) {
    case B_TORCH:  // 地面火把靠下方支撑
        return canSupport(w, x, y - 1, z);
    case B_FIRE:   // 火：下方要有可支撑的方块（原版还要可燃物，这里简化）
        return canSupport(w, x, y - 1, z);
    case B_CHORUS_PLANT:
        return plantCanSurvive(w, x, y, z);
    case B_CHORUS_FLOWER:
    case B_CHORUS_FLOWER_DEAD:
        return flowerCanSurvive(w, x, y, z);
    default:
        if (blockIsWallTorch(id)) {
            // 朝向 = 火把头离开墙的方向，所以墙在反方向
            int dx = 0, dz = 0;
            frameFacingDir(blockWallTorchFacing(id), dx, dz);
            return canSupport(w, x - dx, y, z - dz);
        }
        return true;
    }
}

void onBlockChanged(World& w, int x, int y, int z, uint32_t flags) {
    // 原版顺序：先 NC 更新，后 PP 更新（更新优先级一节）
    if (!(flags & U_NO_NC)) propagate(w, K_NC, x, y, z);
    if (!(flags & U_NO_PP)) propagate(w, K_PP, x, y, z);
}

void scheduleTick(World& w, int x, int y, int z, int delay) {
    auto& q = w.blockTicks();
    // 六个邻居都可能给同一格排计划刻，不去重会让同一格在一刻内被处理多次。
    for (auto& e : q) {
        if (e.x == x && e.y == y && e.z == z) {
            int d = delay > 0 ? delay : 1;
            if (d < e.delay) e.delay = d;   // 取更早到期
            return;
        }
    }
    TickEntry t;
    t.x = x; t.y = y; t.z = z;
    t.delay = delay > 0 ? delay : 1;
    t.order = w.nextTickOrder();
    q.push_back(t);
}

void runScheduledTicks(World& w) {
    auto& q = w.blockTicks();
    if (q.empty()) return;
    std::vector<TickEntry> due;
    for (auto& t : q) {
        if (t.delay > 0) t.delay--;
        else due.push_back(t);
    }
    if (due.empty()) return;
    // 到期的先出队，再按插入顺序执行（处理过程中新加的计划刻留到下一刻）
    q.erase(std::remove_if(q.begin(), q.end(),
                           [](const TickEntry& t) { return t.delay <= 0; }),
            q.end());
    std::stable_sort(due.begin(), due.end(),
                     [](const TickEntry& a, const TickEntry& b) { return a.order < b.order; });
    for (const auto& t : due) {
        uint8_t id = w.getBlock(t.x, t.y, t.z);
        if (id == B_AIR) continue;
        if (isFallingBlock(id)) {
            if (t.y > WORLD_MIN_Y && !canSupport(w, t.x, t.y - 1, t.z))
                spawnFalling(w, t.x, t.y, t.z, id);
        } else if (!blockCanSurvive(w, t.x, t.y, t.z)) {
            // 原版是掉落为物品；本工程还没有物品实体，直接消失。
            w.setBlock(t.x, t.y, t.z, B_AIR);
        }
    }
}

} // namespace bu
