// portalflowtest.cpp — 传送门流程自检（开发用，不入包）
//  1) 末地门：按"玩家站环心朝外放框"的方式摆环 → 12 个眼全部插上后应激活中心 3×3；
//     朝向错一个、缺眼、缺框都不该激活。
//  2) 下界门：主世界点燃一扇门 → 传到下界应生成/找到新门并给出安全落点；
//     再传回来应"找到"原来那扇门而不是又造一扇。
// 用法: portalflowtest [seed]
#include "blocks.hpp"
#include "portal.hpp"
#include "world.hpp"
#include <cmath>
#include <cstdio>
#include <string>

static int g_fail = 0;
static void check(bool ok, const char* what) {
    printf("  [%s] %s\n", ok ? "OK" : "FAIL", what);
    if (!ok) g_fail++;
}

// 方环坐标：以 (x0,z0) 为西北角，环心 (x0+2, z0+2)
static void ringFramePos(int x0, int z0, int i, int& dx, int& dz) {
    static const int pos[12][2] = {
        {1,0},{2,0},{3,0},{0,1},{0,2},{0,3},
        {4,1},{4,2},{4,3},{1,4},{2,4},{3,4}};
    dx = pos[i][0]; dz = pos[i][1];
}

// 站在环心、朝该框架看时的 yaw（本工程 forward = (sin yaw, *, cos yaw)）
static float yawTo(int cx, int cz, int fx, int fz) {
    return std::atan2((float)(fx - cx), (float)(fz - cz));
}

// 按"玩家站环心朝外放"的方式摆一圈（朝向由 frameIdForPlacement 决定）
static void buildRingByPlacement(World& w, int x0, int y, int z0, int skipIndex = -1) {
    int cx = x0 + 2, cz = z0 + 2;
    for (int i = 0; i < 12; i++) {
        if (i == skipIndex) continue;
        int dx, dz;
        ringFramePos(x0, z0, i, dx, dz);
        float yaw = yawTo(cx, cz, x0 + dx, z0 + dz);
        w.setBlock(x0 + dx, y, z0 + dz, portal::frameIdForPlacement(yaw));
    }
}

static int countPortalBlocks(World& w, int x0, int y0, int z0, int x1, int y1, int z1) {
    int n = 0;
    for (int y = y0; y <= y1; y++)
        for (int z = z0; z <= z1; z++)
            for (int x = x0; x <= x1; x++)
                if (w.getBlock(x, y, z) == B_END_PORTAL) n++;
    return n;
}

int main(int argc, char** argv) {
    uint32_t seed = argc > 1 ? (uint32_t)std::stoul(argv[1]) : 12345u;
    printf("传送门流程自检（seed=%u）：\n", seed);

    // ------------------------------------------------------------------
    // 1) 末地门激活
    // ------------------------------------------------------------------
    printf("[末地门]\n");
    {
        World w(seed);
        w.setDimension(DIM_OVERWORLD);
        const int y = 64;
        // 找一片高空区域摆环，保证周围是空气
        w.forceGenerateChunk(0, 0);
        const int x0 = 8, z0 = 8;

        // (a) 站环心朝外放 → 每个框朝向环心 → 依次插眼，最后一个应激活
        buildRingByPlacement(w, x0, y, z0);
        int litAt = -1;
        for (int i = 0; i < 12; i++) {
            int dx, dz;
            ringFramePos(x0, z0, i, dx, dz);
            if (portal::tryPlaceEyeOfEnder(w, x0 + dx, y, z0 + dz)) litAt = i;
        }
        check(litAt >= 0, "站环心朝外摆的环 + 12 眼 → 激活");
        check(countPortalBlocks(w, x0 + 1, y, z0 + 1, x0 + 3, y, z0 + 3) == 9,
              "中心 3×3 全部变成 end_portal");

        // (b) 把其中一个框拧 90° → 不该激活
        World w2(seed);
        w2.setDimension(DIM_OVERWORLD);
        w2.forceGenerateChunk(0, 0);
        buildRingByPlacement(w2, x0, y, z0);
        int bd = 0, bz2 = 0;
        ringFramePos(x0, z0, 0, bd, bz2);
        uint8_t b0 = w2.getBlock(x0 + bd, y, z0 + bz2);
        w2.setBlock(x0 + bd, y, z0 + bz2,
                    frameId((blockFrameFacing(b0) + 1) & 3, false));   // 拧 90°
        bool lit2 = false;
        for (int i = 0; i < 12; i++) {
            int dx, dz;
            ringFramePos(x0, z0, i, dx, dz);
            if (portal::tryPlaceEyeOfEnder(w2, x0 + dx, y, z0 + dz)) lit2 = true;
        }
        check(!lit2, "一个框朝向错 90° → 不激活");

        // (c) 缺一个框 → 不该激活
        World w3(seed);
        w3.setDimension(DIM_OVERWORLD);
        w3.forceGenerateChunk(0, 0);
        buildRingByPlacement(w3, x0, y, z0, 5);
        bool lit3 = false;
        for (int i = 0; i < 12; i++) {
            if (i == 5) continue;
            int dx, dz;
            ringFramePos(x0, z0, i, dx, dz);
            if (portal::tryPlaceEyeOfEnder(w3, x0 + dx, y, z0 + dz)) lit3 = true;
        }
        check(!lit3, "只有 11 个框 → 不激活");

        // (d) 有框没眼 → 不该激活
        World w4(seed);
        w4.setDimension(DIM_OVERWORLD);
        w4.forceGenerateChunk(0, 0);
        buildRingByPlacement(w4, x0, y, z0);
        bool lit4 = false;
        for (int i = 0; i < 12; i++) {
            if (i == 7) continue;   // 留一个空框
            int dx, dz;
            ringFramePos(x0, z0, i, dx, dz);
            if (portal::tryPlaceEyeOfEnder(w4, x0 + dx, y, z0 + dz)) lit4 = true;
        }
        check(!lit4, "有一个框没眼 → 不激活");

        // (e) 放框朝向映射：环心朝外看时，北边必须朝南（指向环心）
        check(portal::frameRequiredFacing(0, 2) == FRAME_EAST &&
              portal::frameRequiredFacing(4, 2) == FRAME_WEST &&
              portal::frameRequiredFacing(2, 0) == FRAME_SOUTH &&
              portal::frameRequiredFacing(2, 4) == FRAME_NORTH,
              "环上朝向规则：西→东、东→西、北→南、南→北（指向环心）");
    }

    // ------------------------------------------------------------------
    // 2) 下界门目的地
    // ------------------------------------------------------------------
    printf("[下界门]\n");
    {
        World w(seed);
        w.setDimension(DIM_OVERWORLD);
        w.forceGenerateChunk(0, 0);
        // 地表上方手动搭一扇 2×3 门（黑曜石框 + 打火石点燃）
        const int ox = 4, oz = 4;
        int baseY = 70;
        for (int x = ox; x <= ox + 3; x++) {
            w.setBlock(x, baseY - 1, oz, B_OBSIDIAN);
            w.setBlock(x, baseY + 3, oz, B_OBSIDIAN);
        }
        for (int y = baseY - 1; y <= baseY + 3; y++) {
            w.setBlock(ox, y, oz, B_OBSIDIAN);
            w.setBlock(ox + 3, y, oz, B_OBSIDIAN);
        }
        bool lit = portal::tryLightNetherPortal(w, ox + 1, baseY, oz);
        check(lit, "主世界点燃下界门");
        check(w.getBlock(ox + 1, baseY, oz) == B_NETHER_PORTAL &&
              w.getBlock(ox + 2, baseY + 2, oz) == B_NETHER_PORTAL,
              "门内 2×3 全是传送门方块");
        int srcAxis = portal::inferPortalAxis(w, ox + 1, baseY, oz);
        check(srcAxis == 0, "门轴推断 = X");

        // 去下界：目标坐标 = 主世界 / 8
        int tx = (ox + 1) / 8, ty = baseY, tz = oz / 8;
        w.setDimension(DIM_NETHER);
        for (int cx = blockToChunkCoord(tx) - 1; cx <= blockToChunkCoord(tx) + 1; cx++)
            for (int cz = blockToChunkCoord(tz) - 1; cz <= blockToChunkCoord(tz) + 1; cz++)
                w.forceGenerateChunk(cx, cz);
        Vec3 land;
        bool ok = portal::findOrCreateNetherPortal(w, tx, ty, tz, srcAxis, 16, land);
        check(ok, "下界侧找到或新建了门");
        int lx = (int)std::floor(land.x), ly = (int)std::floor(land.y), lz = (int)std::floor(land.z);
        check(w.getBlock(lx, ly, lz) == B_NETHER_PORTAL, "落点就在门方块里（不会被丢进方块/岩浆）");
        uint8_t feetBelow = w.getBlock(lx, ly - 1, lz);
        check(feetBelow == B_OBSIDIAN || feetBelow == B_NETHER_PORTAL || blockIsSolid(feetBelow),
              "落点脚下是黑曜石/实心块");
        check(w.getBlock(lx, ly + 1, lz) != B_OBSIDIAN && w.getBlock(lx, ly + 1, lz) != B_LAVA,
              "落点头部不是方块/岩浆");
        // 门框完整性：横向相邻应是门方块，纵向上面还是门方块
        int wx = srcAxis == 0 ? 1 : 0, wz = srcAxis == 0 ? 0 : 1;
        check(w.getBlock(lx + wx, ly, lz + wz) == B_NETHER_PORTAL ||
              w.getBlock(lx - wx, ly, lz - wz) == B_NETHER_PORTAL,
              "新门宽度 2 格");
        check(w.getBlock(lx, ly + 2, lz) == B_NETHER_PORTAL, "新门高度 3 格");
        check(w.getBlock(lx, ly + 3, lz) == B_OBSIDIAN ||
              w.getBlock(lx + wx, ly + 3, lz + wz) == B_OBSIDIAN ||
              w.getBlock(lx - wx, ly + 3, lz - wz) == B_OBSIDIAN,
              "新门顶上有黑曜石框");

        // 回主世界：应找到原来那扇门，而不是又造一扇
        int bx = (int)std::floor(land.x) * 8, bz = (int)std::floor(land.z) * 8;
        w.setDimension(DIM_OVERWORLD);
        Vec3 back;
        bool ok2 = portal::findOrCreateNetherPortal(w, bx, (int)land.y, bz, srcAxis, 128, back);
        check(ok2, "回主世界时找到门");
        float dx = back.x - (ox + 1.5f), dz = back.z - oz;
        check(std::fabs(dx) <= 3.0f && std::fabs(dz) <= 3.0f,
              "回程落点就在原来的门附近（没有重复造门）");
        check(w.getBlock((int)std::floor(back.x), (int)std::floor(back.y), (int)std::floor(back.z))
                  == B_NETHER_PORTAL,
              "回程落点也在门方块里");
    }

    printf(g_fail == 0 ? "\n全部通过\n" : "\n%d 项失败\n", g_fail);
    return g_fail == 0 ? 0 : 1;
}
