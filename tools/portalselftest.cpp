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
static void buildRing(int x0, int y, int z0, bool eyes, bool correctFacing) {
    g_blocks.clear();
    for (int dx = 0; dx < 5; dx++) {
        for (int dz = 0; dz < 5; dz++) {
            int need = portal::frameRequiredFacing(dx, dz);
            if (need < 0) continue;
            int facing = correctFacing ? need : (need + 1) & 3;
            g_blocks[{x0 + dx, z0 + dz}] = frameId(facing, eyes);
        }
    }
    (void)y;
}

int main() {
    printf("末地门方环自检：\n");

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
