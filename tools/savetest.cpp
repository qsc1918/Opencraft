// build/savetest.cpp - 存档按维度存取的自检（临时，不入库）
#include "blocks.hpp"
#include "dimensions.hpp"
#include "player.hpp"
#include "save.hpp"
#include "world.hpp"
#include <cstdio>
#include <string>

static int failures = 0;

static void check(bool ok, const char* what) {
    printf("%s  %s\n", ok ? "[ok]  " : "[FAIL]", what);
    if (!ok) failures++;
}

int main() {
    const std::string dir = "build/savetest_saves";
    const std::string name = "w1";

    // ---- 造三个维度各一个标记方块 ----
    {
        World w(1337);
        w.forceGenerateChunk(0, 0);
        w.setBlock(5, 70, 5, B_DIAMOND);            // 主世界
        w.setDimension(DIM_NETHER);
        w.forceGenerateChunk(0, 0);
        w.setBlock(5, 70, 5, B_GLOWSTONE);          // 下界
        w.setDimension(DIM_END);
        w.forceGenerateChunk(0, 0);
        w.setBlock(5, 70, 5, B_DRAGON_EGG);         // 末地

        Player p;
        p.cam.pos = Vec3(1.5f, 70.0f, 2.5f);
        p.dim = DIM_NETHER;                          // 存档时人在下界
        check(saveWorld(w, p, name, dir), "saveWorld");
    }

    // ---- 重新载入，检查维度与各维度的方块 ----
    {
        World w(999);
        uint32_t seed = 0;
        float sx = 0, sy = 0, sz = 0, yaw = 0, pitch = 0;
        bool flying = false;
        DimensionId dim = DIM_OVERWORLD;
        check(loadWorld(w, seed, sx, sy, sz, yaw, pitch, flying, dim, name, dir), "loadWorld");
        check(seed == 1337, "seed 还原");
        check(dim == DIM_NETHER, "玩家所在维度还原（下界）");
        check(w.getBlockInDim(DIM_OVERWORLD, 5, 70, 5) == B_DIAMOND, "主世界方块没被下界覆盖");
        check(w.getBlockInDim(DIM_NETHER, 5, 70, 5) == B_GLOWSTONE, "下界方块存回下界");
        check(w.getBlockInDim(DIM_END, 5, 70, 5) == B_DRAGON_EGG, "末地方块存回末地");
    }

    printf(failures ? "\n%d 项失败\n" : "\n全部通过\n", failures);
    return failures ? 1 : 0;
}
