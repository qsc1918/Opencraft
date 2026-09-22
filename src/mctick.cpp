#include "mctick.hpp"
#include "blocks.hpp"
#include "world.hpp"
#include <cmath>
#include <vector>

namespace {

// 随机刻半径（区块）。原版按加载范围抽，这里取一个够用的固定值。
constexpr int kTickChunkRadius = 4;
// 每区块每刻抽取的方块数（原版是每子区块 3 个；这里按整列折中成 4 个）
constexpr int kSamplesPerChunk = 4;

// 随机刻用的随机源：原版是每维度一个随机源，不需要存档一致，用固定种子即可。
uint64_t g_rng = 0x9E3779B97F4A7C15ULL;
inline uint32_t rndNext() {
    g_rng += 0x9E3779B97F4A7C15ULL;
    uint64_t z = g_rng;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return (uint32_t)(z ^ (z >> 31));
}
inline int rndInt(int bound) { return bound <= 0 ? 0 : (int)(rndNext() % (uint32_t)bound); }

// ---- 原版 ChorusPlantBlock.getStateWithConnections / canSurvive 的等价判断 ----
bool isChorusPart(uint8_t b) {
    return b == B_CHORUS_PLANT || b == B_CHORUS_FLOWER || b == B_CHORUS_FLOWER_DEAD;
}

// 只有空气与紫颂植株算"空"（原版 isEmptyBlock：所有非实心方块都算空，
// 本工程没有流体/草等半方块，直接按空气处理即可）
bool emptyForChorus(uint8_t b) { return b == B_AIR; }

// 原版 allNeighborsEmpty(level, pos, ignore)
bool allNeighborsEmpty(const World& w, int x, int y, int z, int ignoreDx, int ignoreDz) {
    static const int off[4][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
    for (const auto& o : off) {
        if (o[0] == ignoreDx && o[1] == ignoreDz) continue;
        if (!emptyForChorus(w.getBlock(x + o[0], y, z + o[1]))) return false;
    }
    return true;
}

bool flowerCanSurvive(const World& w, int x, int y, int z) {
    uint8_t below = w.getBlock(x, y - 1, z);
    if (below != B_CHORUS_PLANT && below != B_END_STONE) {
        if (below != B_AIR) return false;
        bool one = false;
        static const int off[4][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
        for (const auto& o : off) {
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

bool plantCanSurvive(const World& w, int x, int y, int z) {
    uint8_t below = w.getBlock(x, y - 1, z);
    bool aboveAndBelow = w.getBlock(x, y + 1, z) != B_AIR && below != B_AIR;
    static const int off[4][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
    for (const auto& o : off) {
        int nx = x + o[0], nz = z + o[1];
        if (w.getBlock(nx, y, nz) == B_CHORUS_PLANT) {
            if (aboveAndBelow) return false;
            uint8_t b2 = w.getBlock(nx, y - 1, nz);
            if (b2 == B_CHORUS_PLANT || b2 == B_END_STONE) return true;
        }
    }
    return below == B_CHORUS_PLANT || below == B_END_STONE;
}

// 原版 ChorusFlowerBlock.randomTick 的等价实现（age 只区分活/死，
// 死花是独立 id，所以 currentAge 用 0..4 表示活着、5 表示枯萎）。
bool tickChorusFlower(World& w, int x, int y, int z) {
    if (!flowerCanSurvive(w, x, y, z)) { w.setBlock(x, y, z, B_AIR); return true; }

    int above = y + 1;
    if (w.getBlock(x, above, z) != B_AIR || above > WORLD_MAX_Y) return false;

    bool growUpwards = false;
    bool pillarOnSupport = false;
    uint8_t below = w.getBlock(x, y - 1, z);
    if (below == B_END_STONE) {
        growUpwards = true;
    } else if (below == B_CHORUS_PLANT) {
        int height = 1;
        for (int i = 0; i < 4; i++) {
            uint8_t t = w.getBlock(x, y - (height + 1), z);
            if (t != B_CHORUS_PLANT) {
                if (t == B_END_STONE) pillarOnSupport = true;
                break;
            }
            height++;
        }
        if (height < 2 || height <= rndInt(pillarOnSupport ? 5 : 4)) growUpwards = true;
    } else if (below == B_AIR) {
        growUpwards = true;
    }

    if (growUpwards && allNeighborsEmpty(w, x, above, z, 99, 99) && w.getBlock(x, y + 2, z) == B_AIR) {
        w.setBlock(x, y, z, B_CHORUS_PLANT);
        w.setBlock(x, above, z, B_CHORUS_FLOWER);
        return true;
    }

    // 分支（原版 currentAge < 4 才分叉；这里活花都允许，等价于 age 0..4 的绝大多数情况）
    int attempts = rndInt(4);
    if (pillarOnSupport) attempts++;
    bool branched = false;
    static const int off[4][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
    for (int i = 0; i < attempts; i++) {
        const int* d = off[rndInt(4)];
        int tx = x + d[0], tz = z + d[1];
        if (w.getBlock(tx, y, tz) != B_AIR) continue;
        if (w.getBlock(tx, y - 1, tz) != B_AIR) continue;
        if (!allNeighborsEmpty(w, tx, y, tz, -d[0], -d[1])) continue;
        w.setBlock(tx, y, tz, B_CHORUS_FLOWER);
        branched = true;
    }
    if (branched) {
        w.setBlock(x, y, z, B_CHORUS_PLANT);
    } else {
        w.setBlock(x, y, z, B_CHORUS_FLOWER_DEAD);
    }
    return true;
}

} // 匿名命名空间

namespace mctick {

bool randomTickBlock(World& w, int x, int y, int z) {
    uint8_t b = w.getBlock(x, y, z);
    if (b == B_CHORUS_FLOWER) return tickChorusFlower(w, x, y, z);
    return false;
}

void tickRandomBlocks(World& w, float px, float pz, float dt) {
    static float acc = 0.0f;
    acc += dt * TICK_RATE;
    if (acc < 1.0f) return;
    int steps = (int)acc;
    if (steps > 5) steps = 5;  // 卡顿时限制补刻数量
    acc -= (float)steps;

    int ccx = blockToChunkCoord((int)std::floor(px));
    int ccz = blockToChunkCoord((int)std::floor(pz));
    for (int s = 0; s < steps; s++) {
        for (int cx = ccx - kTickChunkRadius; cx <= ccx + kTickChunkRadius; cx++) {
            for (int cz = ccz - kTickChunkRadius; cz <= ccz + kTickChunkRadius; cz++) {
                for (int i = 0; i < kSamplesPerChunk; i++) {
                    int bx = cx * CHUNK_SIZE + rndInt(CHUNK_SIZE);
                    int bz = cz * CHUNK_SIZE + rndInt(CHUNK_SIZE);
                    int by = rndInt(WORLD_HEIGHT);
                    randomTickBlock(w, bx, by, bz);
                }
            }
        }
    }
}

} // namespace mctick
