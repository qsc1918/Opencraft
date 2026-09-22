// portalselftest.cpp — 末地门 5×5 方环的几何自检（朝向规则）
// 用法: portalselftest
#include "blocks.hpp"
#include "portal.hpp"
#include <cstdio>
#include <map>

// 简单方块表：模拟 World.getBlock
static std::map<std::pair<int, int>, uint8_t> g_blocks;
static uint8_t getBlock(int x, int, int z) {
    auto it = g_blocks.find({x, z});
    return it == g_blocks.end() ? (uint8_t)B_AIR : it->second;
}

static int g_fail = 0;
static void check(bool ok, const char* what) {
    printf("  [%s] %s\n", ok ? "OK" : "FAIL", what);
    if (!ok) g_fail++;
}

// 按原版布局摆一圈：每个框架朝向环心并放上眼睛
// expectedFacing(dx,dz) 用"写死的期望值"，不调用被测的 frameRequiredFacing，
// 否则规则写错时自检会跟着一起错（这个坑踩过）。
static int expectedFacing(int dx, int dz) {
    // 环坐标（0..4，环心 2,2）：西→东、东→西、北→南、南→北
    if (dx == 0) return FRAME_EAST;
    if (dx == 4) return FRAME_WEST;
    if (dz == 0) return FRAME_SOUTH;
    return FRAME_NORTH;
}

static void buildRing(int x0, int y, int z0, bool eyes, bool correctFacing) {
    g_blocks.clear();
    for (int dx = 0; dx < 5; dx++) {
        for (int dz = 0; dz < 5; dz++) {
            if (portal::frameRequiredFacing(dx, dz) < 0) continue;
            int facing = expectedFacing(dx, dz);
            if (!correctFacing) facing = (facing + 1) & 3;
            g_blocks[{x0 + dx, z0 + dz}] = frameId(facing, eyes);
        }
    }
    (void)y;
}

int main() {
    printf("末地门方环自检：\n");

    // 朝向规则本身（对着原版要塞布局的朝向写的期望值）
    check(portal::frameRequiredFacing(0, 2) == FRAME_EAST, "西边中格 → 朝东（指向环心）");
    check(portal::frameRequiredFacing(4, 2) == FRAME_WEST, "东边中格 → 朝西");
    check(portal::frameRequiredFacing(2, 0) == FRAME_SOUTH, "北边中格 → 朝南");
    check(portal::frameRequiredFacing(2, 4) == FRAME_NORTH, "南边中格 → 朝北");
    check(portal::frameRequiredFacing(0, 1) == FRAME_EAST, "西边非中格 → 朝东");
    check(portal::frameRequiredFacing(1, 4) == FRAME_NORTH, "南边非中格 → 朝北");
    check(portal::frameRequiredFacing(2, 2) < 0, "环心不是框架位置");
    check(portal::frameRequiredFacing(0, 0) < 0, "四角不是框架位置");

    buildRing(0, 64, 0, true, true);
    check(portal::isCompleteRingAt(getBlock, 0, 64, 0), "朝向正确 + 全部有眼 → 激活");

    buildRing(0, 64, 0, false, true);
    check(!portal::isCompleteRingAt(getBlock, 0, 64, 0), "朝向正确但没眼 → 不激活");

    buildRing(0, 64, 0, true, false);
    check(!portal::isCompleteRingAt(getBlock, 0, 64, 0), "有眼但朝向全错 → 不激活");

    // 只错一个框架的朝向
    buildRing(0, 64, 0, true, true);
    g_blocks[{2, 0}] = frameId((FRAME_SOUTH + 1) & 3, true);
    check(!portal::isCompleteRingAt(getBlock, 0, 64, 0), "只错一个框架朝向 → 不激活");

    // 缺一个框架
    buildRing(0, 64, 0, true, true);
    g_blocks.erase({2, 0});
    check(!portal::isCompleteRingAt(getBlock, 0, 64, 0), "缺一个框架 → 不激活");

    // 四角与中心本来就不该有框架，多放了也不影响
    buildRing(0, 64, 0, true, true);
    g_blocks[{0, 0}] = B_STONE;
    g_blocks[{2, 2}] = B_STONE;
    check(portal::isCompleteRingAt(getBlock, 0, 64, 0), "四角/中心放别的方块 → 不影响");

    // 偏移一个方块的环不该被判成同一环
    buildRing(3, 64, 3, true, true);
    check(portal::isCompleteRingAt(getBlock, 3, 64, 3), "偏移后的环仍能识别");
    check(!portal::isCompleteRingAt(getBlock, 0, 64, 0), "偏移后原基准位置不成立");

    printf(g_fail == 0 ? "\n全部通过\n" : "\n%d 项失败\n", g_fail);
    return g_fail == 0 ? 0 : 1;
}
