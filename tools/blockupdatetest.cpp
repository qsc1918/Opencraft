// blockupdatetest — 方块更新自检（PP 更新传播 + 计划刻 + 下落的方块）
//
// 用法: blockupdatetest <seed>
// 用例的期望值全部写死，不调用被测函数生成期望。
#include "blockupdate.hpp"
#include "blocks.hpp"
#include "entities.hpp"
#include "specs.hpp"
#include "world.hpp"
#include <cstdio>
#include <cstdlib>
#include <string>

static int g_fail = 0, g_pass = 0;

static void check(bool cond, const char* name) {
    if (cond) { g_pass++; printf("  [OK] %s\n", name); }
    else { g_fail++; printf("  [FAIL] %s\n", name); }
}

// 测试台：在 (0,0) 区块上方 y≈100 造一小块空地，避开生成的地形
static const int kX = 8, kZ = 8;
static const int kFloorY = 100;

// 用"不产生更新"的方式搭台子，再单独触发更新，保证起点确定
static void buildPad(World& w) {
    w.forceGenerateChunk(0, 0);
    for (int y = kFloorY - 1; y <= kFloorY + 6; y++)
        for (int dx = -1; dx <= 1; dx++)
            for (int dz = -1; dz <= 1; dz++)
                w.setBlock(kX + dx, y, kZ + dz, B_AIR, bu::U_NO_ALL);
    w.setBlockInDim(DIM_OVERWORLD, kX, kFloorY, kZ, B_STONE, bu::U_NO_ALL);
}

static void ticks(World& w, int n) {
    for (int i = 0; i < n; i++) w.tickEntities(TICK_DT);
}

static int countFalling(const World& w) {
    int n = 0;
    for (const auto& e : w.entities())
        if (!e->dead && e->kind == EntityKind::FallingBlock) n++;
    return n;
}

int main(int argc, char** argv) {
    uint32_t seed = argc > 1 ? (uint32_t)std::strtoul(argv[1], nullptr, 10) : 12345u;

    // ---- 用例 1：沙子失去支撑 → 变成下落的方块 → 落地还原成沙子 ----
    {
        World w(seed);
        buildPad(w);
        w.setBlock(kX, kFloorY + 1, kZ, B_SAND, bu::U_NO_ALL);
        check(w.getBlock(kX, kFloorY + 1, kZ) == B_SAND, "搭台：沙子已在石头上");
        // 抽掉下方的石头（正常更新）→ PP 更新传到沙子 → 计划刻 → 下落
        w.setBlock(kX, kFloorY, kZ, B_AIR);
        ticks(w, 3);
        check(w.getBlock(kX, kFloorY + 1, kZ) == B_AIR, "抽掉支撑后原格变空");
        check(countFalling(w) == 1, "产生 1 个下落的方块实体");

        // 落点：脚下第一个实心方块的顶面（台子下面就是生成的地形）
        int landY = WORLD_MIN_Y;
        for (int y = kFloorY - 1; y >= WORLD_MIN_Y; y--)
            if (blockIsSolid(w.getBlock(kX, y, kZ))) { landY = y + 1; break; }
        ticks(w, 600);
        check(countFalling(w) == 0, "落地后实体消失");
        check(w.getBlock(kX, landY, kZ) == B_SAND, "沙子落在第一个实心方块顶面");
        printf("    (落点 y=%d)\n", landY);
    }

    // ---- 用例 2：沙子柱整列坍塌 ----
    {
        World w(seed);
        buildPad(w);
        for (int i = 1; i <= 3; i++) w.setBlock(kX, kFloorY + i, kZ, B_SAND, bu::U_NO_ALL);
        w.setBlock(kX, kFloorY, kZ, B_AIR);
        ticks(w, 8);
        check(countFalling(w) >= 2, "整列沙子依次变下落方块");
        ticks(w, 600);
        check(countFalling(w) == 0, "全部落地");
        int sand = 0;
        for (int y = WORLD_MIN_Y; y <= kFloorY + 3; y++)
            if (w.getBlock(kX, y, kZ) == B_SAND) sand++;
        check(sand == 3, "3 格沙子一块不少（没有凭空消失）");
    }

    // ---- 用例 3：地面火把失去支撑 → 延迟破坏 ----
    {
        World w(seed);
        buildPad(w);
        w.setBlock(kX, kFloorY + 1, kZ, B_TORCH, bu::U_NO_ALL);
        w.setBlock(kX, kFloorY, kZ, B_AIR);
        ticks(w, 4);
        check(w.getBlock(kX, kFloorY + 1, kZ) == B_AIR, "火把失去支撑后消失");
        // 有支撑时不该消失
        w.setBlock(kX, kFloorY, kZ, B_STONE, bu::U_NO_ALL);
        w.setBlock(kX, kFloorY + 1, kZ, B_TORCH, bu::U_NO_ALL);
        w.setBlock(kX + 1, kFloorY, kZ, B_STONE);   // 旁边放块石头触发更新
        ticks(w, 4);
        check(w.getBlock(kX, kFloorY + 1, kZ) == B_TORCH, "有支撑的火把不受影响");
    }

    // ---- 用例 4：墙上火把失去附着的墙 → 延迟破坏 ----
    {
        World w(seed);
        buildPad(w);
        // 东向墙上火把：头指向 +X，墙在 -X 侧
        w.setBlock(kX, kFloorY, kZ, B_STONE, bu::U_NO_ALL);
        w.setBlock(kX + 1, kFloorY, kZ, B_WALL_TORCH_E, bu::U_NO_ALL);
        w.setBlock(kX, kFloorY, kZ, B_AIR);
        ticks(w, 4);
        check(w.getBlock(kX + 1, kFloorY, kZ) == B_AIR, "墙上火把失去墙面后消失");
        // 墙还在时不受影响
        w.setBlock(kX, kFloorY, kZ, B_STONE, bu::U_NO_ALL);
        w.setBlock(kX + 1, kFloorY, kZ, B_WALL_TORCH_E, bu::U_NO_ALL);
        w.setBlock(kX, kFloorY + 2, kZ, B_STONE);
        ticks(w, 4);
        check(w.getBlock(kX + 1, kFloorY, kZ) == B_WALL_TORCH_E, "墙还在时墙上火把保留");
    }

    // ---- 用例 5：紫颂植株悬空 → 消失（原版 canSurvive）----
    {
        World w(seed);
        buildPad(w);
        w.setBlock(kX, kFloorY + 1, kZ, B_CHORUS_PLANT, bu::U_NO_ALL);
        // 在植株旁边放方块：PP 更新只传到改动方块的 6 个邻居，必须挑相邻的格
        w.setBlock(kX + 1, kFloorY + 1, kZ, B_STONE);
        ticks(w, 4);
        check(w.getBlock(kX, kFloorY + 1, kZ) == B_AIR, "悬空的紫颂植株被破坏");
        // 长在末地石上则保留
        w.setBlock(kX, kFloorY, kZ, B_END_STONE, bu::U_NO_ALL);
        w.setBlock(kX, kFloorY + 1, kZ, B_CHORUS_PLANT, bu::U_NO_ALL);
        w.setBlock(kX - 1, kFloorY + 1, kZ, B_STONE);
        ticks(w, 4);
        check(w.getBlock(kX, kFloorY + 1, kZ) == B_CHORUS_PLANT, "长在末地石上的植株保留");
    }

    // ---- 用例 6：世界生成写入不产生更新（悬空沙子保持不动）----
    {
        World w(seed);
        buildPad(w);
        int above = kFloorY + 1;
        w.setBlock(kX, above, kZ, B_SAND, bu::U_NO_ALL);   // 悬空沙子：模拟生成写入
        ticks(w, 10);
        check(w.getBlock(kX, above, kZ) == B_SAND, "生成写入的悬空沙子不会自己掉");
    }

    printf("\n%s（通过 %d，失败 %d）\n", g_fail ? "有失败" : "全部通过", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
