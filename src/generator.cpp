#include "generator.hpp"
#include "blocks.hpp"
#include "noise.hpp"
#include "util.hpp"
#include "world.hpp"
#include <algorithm>
#include <cmath>

namespace {

struct Ore {
    uint8_t id;
    int tries;
    int size;
    int minY, maxY;
};

const Ore kOres[] = {
    {B_COAL, 20, 9, 0, 96},
    {B_IRON, 20, 9, 0, 64},
    {B_GOLD, 2, 9, 0, 32},
    {B_REDSTONE, 8, 8, 0, 16},
    {B_DIAMOND, 1, 7, 0, 16},
};

inline uint8_t& ref(uint8_t* b, int x, int y, int z) { return b[x + (z << 4) + (y << 8)]; }

struct SurfaceInfo {
    int height;      // topmost terrain block y
    bool ocean;      // height < SEA_LEVEL
    bool desert;     // hot
    bool cold;       // snowy
};

SurfaceInfo surfaceFor(const Noise& temp, const Noise& moist, int wx, int wz, int height) {
    SurfaceInfo si;
    si.height = height;
    si.ocean = height < SEA_LEVEL;
    float tv = temp.value2(wx * 0.008f + 100.0f, wz * 0.008f + 100.0f);
    si.desert = tv > 0.62f;
    si.cold = tv < 0.22f;
    return si;
}

void placeTree(uint8_t* b, int wx, int wz, int rootY, Rng& rng) {
    int cxBase = (wx & 15);
    int czBase = (wz & 15);
    int trunk = 4 + rng.irange(0, 2);
    // trunk first (so leaves never replace it)
    for (int i = 0; i < trunk; i++) {
        int y = rootY + 1 + i;
        if (y < 2 || y >= WORLD_HEIGHT) continue;
        uint8_t& t = ref(b, cxBase, y, czBase);
        if (t == B_AIR || t == B_WATER) t = B_LOG;
    }
    // leaves
    for (int dy = -2; dy <= 1; dy++) {
        int ly = rootY + trunk + dy;
        if (ly < 2 || ly >= WORLD_HEIGHT) continue;
        int r = dy <= 0 ? 2 : 1;
        for (int dx = -r; dx <= r; dx++) {
            for (int dz = -r; dz <= r; dz++) {
                int x = cxBase + dx, z = czBase + dz;
                if (x < 0 || x >= CHUNK_SIZE || z < 0 || z >= CHUNK_SIZE) continue;
                if (dx == 0 && dz == 0 && dy <= 0) continue; // trunk space (dy=1 caps the trunk top)
                if (dx * dx + dz * dz > r * r + 1) continue;
                uint8_t cur = ref(b, x, ly, z);
                if (cur == B_AIR || cur == B_WATER) ref(b, x, ly, z) = B_LEAVES;
            }
        }
    }
}

} // namespace

// ===========================================================================
// Nether generator (旧版设计: Alpha/Beta 风格，无要塞)
// - 3D 噪声洞穴：netherrack 实心 + 噪声挖空
// - 基岩地板 y=0 + 基岩天花板 y=127
// - 岩浆海 y≤31
// - 萤石簇挂在天花板下方
// - 灵魂沙小块区域
// ===========================================================================
void gen::generateNether(uint32_t seed, int cx, int cz, uint8_t* out) {
    std::fill(out, out + CHUNK_VOL, (uint8_t)B_AIR);
    Noise nethN(seed ^ 0xdeadbeefU);
    Noise caveN(seed ^ 0x42cafeU);
    Noise soulN(seed ^ 0x666U);
    Noise glowN(seed ^ 0x777U);

    int baseWX = cx * CHUNK_SIZE, baseWZ = cz * CHUNK_SIZE;

    for (int lx = 0; lx < CHUNK_SIZE; lx++) {
        for (int lz = 0; lz < CHUNK_SIZE; lz++) {
            int wx = baseWX + lx, wz = baseWZ + lz;

            // 基岩地板 (y=0) 和天花板 (y=127)
            ref(out, lx, 0, lz) = B_BEDROCK;
            ref(out, lx, 127, lz) = B_BEDROCK;
            // y=1 和 y=126 有 50% 基岩概率（MC 规范）
            uint32_t h1 = hash32((uint32_t)wx * 0x9e3779b9U ^ (uint32_t)wz ^ (uint32_t)1 * 97);
            uint32_t h2 = hash32((uint32_t)wx * 0x9e3779b9U ^ (uint32_t)wz ^ (uint32_t)126 * 97);
            if ((h1 & 1) == 0) ref(out, lx, 1, lz) = B_BEDROCK;
            if ((h2 & 1) == 0) ref(out, lx, 126, lz) = B_BEDROCK;

            // 3D 噪声确定 netherrack 密实度
            for (int y = 2; y <= 125; y++) {
                float wxn = wx * 0.02f, wyn = y * 0.04f, wzn = wz * 0.02f;
                float n = nethN.fbm3(wxn, wyn, wzn, 4, 2.0f, 0.5f);
                float cave = caveN.fbm3(wx * 0.05f, y * 0.08f, wz * 0.05f, 3, 2.0f, 0.5f);

                // 密实区域：中心层 (y=40..80) 最密，向两侧递减
                float density = 0.45f;
                if (y < 40) density -= (40 - y) * 0.008f;
                if (y > 80) density -= (y - 80) * 0.01f;

                // 合并噪声
                float val = n * 0.6f + cave * 0.4f;
                if (val > density) {
                    ref(out, lx, y, lz) = B_NETHERRACK;
                }

                // 灵魂沙小块区域
                float sv = soulN.fbm3(wx * 0.03f, y * 0.05f, wz * 0.03f, 2, 2.0f, 0.5f);
                if (sv > 0.65f && ref(out, lx, y, lz) == B_NETHERRACK) {
                    ref(out, lx, y, lz) = B_SOUL_SAND;
                }
            }

            // 岩浆海 y≤31（填满空气区域）
            for (int y = 2; y <= 31; y++) {
                if (ref(out, lx, y, lz) == B_AIR) {
                    ref(out, lx, y, lz) = B_LAVA;
                }
            }

            // 萤石簇挂在天花板下方
            float gv = glowN.fbm3(wx * 0.04f, 120.0f, wz * 0.04f, 2, 2.0f, 0.5f);
            if (gv > 0.6f) {
                // 找到天花板最下方的实心块
                for (int y = 125; y >= 80; y--) {
                    if (ref(out, lx, y, lz) == B_NETHERRACK) {
                        if (y + 1 <= 126 && ref(out, lx, y + 1, lz) == B_AIR) {
                            ref(out, lx, y + 1, lz) = B_GLOWSTONE;
                        }
                        break;
                    }
                }
            }
        }
    }
}

// ===========================================================================
// End generator (最新机制)
// - 主岛: end_stone 圆形岛屿 (半径 ~100, Perlin 噪声起伏)
// - 10 根黑曜石柱 (半径 43 范围内, 高度 76-103, 2 根有铁笼)
// - 出口传送门: 5×5 基岩平台 y=63 + 中心柱到 y=67
// - 折跃门: 环绕岛屿 (max 20, 简化为静态生成)
// ===========================================================================

// 末地柱子参数：固定位置 (半径 ~43, 角度分布)
struct EndPillar {
    float angle;   // 弧度
    int height;    // 总高度（从 y=0 到 y=height）
    bool caged;    // 是否有铁笼保护水晶
};

static constexpr EndPillar kPillars[10] = {
    {0.0f,        93, false},
    {0.628f,      87, false},
    {1.257f,     101, true },  // 有铁笼
    {1.885f,      76, false},
    {2.513f,      99, false},
    {3.142f,      81, false},
    {3.770f,      95, false},
    {4.398f,     103, true },  // 有铁笼
    {5.027f,      89, false},
    {5.655f,      84, false},
};

void gen::generateEnd(uint32_t seed, int cx, int cz, uint8_t* out) {
    std::fill(out, out + CHUNK_VOL, (uint8_t)B_AIR);
    Noise endN(seed ^ 0x1234abcdU);

    int baseWX = cx * CHUNK_SIZE, baseWZ = cz * CHUNK_SIZE;

    // 主岛中心在 (0,0)，半径 ~100，Perlin 噪声起伏
    float islandRadius = 100.0f;

    for (int lx = 0; lx < CHUNK_SIZE; lx++) {
        for (int lz = 0; lz < CHUNK_SIZE; lz++) {
            int wx = baseWX + lx, wz = baseWZ + lz;
            float dist = sqrtf((float)(wx * wx + wz * wz));

            // 基岩地板 y=0
            ref(out, lx, 0, lz) = B_BEDROCK;

            // 主岛地形
            if (dist < islandRadius) {
                // 岛屿形状：圆形衰减 + Perlin 起伏
                float falloff = 1.0f - dist / islandRadius;
                falloff = falloff * falloff; // 平方衰减，边缘更陡
                float heightNoise = endN.fbm2(wx * 0.015f, wz * 0.015f, 4, 2.0f, 0.5f);
                float h = 55.0f + falloff * 40.0f + heightNoise * 12.0f;
                int topY = (int)h;
                if (topY < 1) topY = 1;
                if (topY > 126) topY = 126;

                for (int y = 1; y <= topY; y++) {
                    ref(out, lx, y, lz) = B_END_STONE;
                }
            }
            // 岛屿外的虚空（保持空气）
        }
    }

    // 放置 10 根黑曜石柱（世界坐标固定，只在对应区块放置）
    for (const auto& p : kPillars) {
        int worldPx = (int)(cosf(p.angle) * 43.0f);
        int worldPz = (int)(sinf(p.angle) * 43.0f);
        // 柱子的世界坐标 (worldPx, worldPz)，检查是否落在当前区块内
        int localPx = worldPx - baseWX;
        int localPz = worldPz - baseWZ;
        if (localPx < 0 || localPx >= CHUNK_SIZE || localPz < 0 || localPz >= CHUNK_SIZE) continue;
        // 柱子从 y=1 到 y=height，直径 1 (单方块柱)
        for (int y = 1; y <= p.height && y < WORLD_HEIGHT; y++) {
            ref(out, localPx, y, localPz) = B_OBSIDIAN;
        }
        // 铁笼：如果 caged，在柱顶周围放铁块
        if (p.caged) {
            for (int dx = -1; dx <= 1; dx++) {
                for (int dz = -1; dz <= 1; dz++) {
                    int bx = localPx + dx, bz = localPz + dz;
                    if (bx >= 0 && bx < CHUNK_SIZE && bz >= 0 && bz < CHUNK_SIZE) {
                        ref(out, bx, p.height - 1, bz) = B_IRON;
                        if (dx != 0 || dz != 0) {
                            if (p.height + 1 < WORLD_HEIGHT)
                                ref(out, bx, p.height + 1, bz) = B_IRON;
                        }
                    }
                }
            }
        }
    }

    // 出口传送门：5×5 基岩平台 y=63
    // MC: 底部 5×5 基岩框架, 中心 3×3 end_portal, 顶部基岩柱到 y=67
    for (int dx = -2; dx <= 2; dx++) {
        for (int dz = -2; dz <= 2; dz++) {
            int bx = dx + 8, bz = dz + 8; // 放在区块中心附近 (8,8)
            if (bx >= 0 && bx < CHUNK_SIZE && bz >= 0 && bz < CHUNK_SIZE) {
                // 基岩平台 y=63
                ref(out, bx, 63, bz) = B_BEDROCK;
                // 3×3 end_portal (y=64, 中心)
                if (std::abs(dx) <= 1 && std::abs(dz) <= 1) {
                    if (64 < WORLD_HEIGHT)
                        ref(out, bx, 64, bz) = B_END_PORTAL;
                }
            }
        }
    }
    // 中心基岩柱 y=65..67
    for (int y = 65; y <= 67 && y < WORLD_HEIGHT; y++) {
        if (8 >= 0 && 8 < CHUNK_SIZE) {
            ref(out, 8, y, 8) = B_BEDROCK;
        }
    }

    // 龙蛋放在 y=68（出口柱顶端）- 由主循环在杀死龙后放置
}

void gen::generateForDim(DimensionId dim, uint32_t seed, int cx, int cz, uint8_t* out) {
    switch (dim) {
    case DIM_NETHER: generateNether(seed, cx, cz, out); break;
    case DIM_END:    generateEnd(seed, cx, cz, out); break;
    default:         generateColumn(seed, cx, cz, out); break;
    }
}

namespace gen {

void generateColumn(uint32_t seed, int cx, int cz, uint8_t* out) {
    std::fill(out, out + CHUNK_VOL, (uint8_t)B_AIR);

    Noise n1(seed), n2(seed ^ 0x9e3779b9U), n3(seed ^ 0x51ed270bU);
    Noise tempN(seed ^ 0x85ebca6bU), moistN(seed ^ 0xc2b2ae35U);
    Noise caveN(seed ^ 0x27d4eb2dU), caveN2(seed ^ 0x165667b1U);
    Noise beachN(seed ^ 0x9e3779b9U ^ 0x1337U);

    int baseWX = cx * CHUNK_SIZE, baseWZ = cz * CHUNK_SIZE;

    // Precompute per-column surface height for the cave pass (avoids a
    // 16×16×128 full scan later). At this point the topmost non-air block per
    // column is exactly `height` (≥5) or bedrock at y=4 for very low terrain (height<5).
    int localTop[CHUNK_SIZE][CHUNK_SIZE];

    for (int lx = 0; lx < CHUNK_SIZE; lx++) {
        for (int lz = 0; lz < CHUNK_SIZE; lz++) {
            int wx = baseWX + lx, wz = baseWZ + lz;

            // layered heightmap. Use a smooth value-noise pyramid so adjacent
            // columns step by at most 1-2 blocks each (no terraced canyons).
            float continents = n1.fbm2(wx * 0.004f, wz * 0.004f, 3, 2.0f, 0.5f);
            float hills = n2.fbm2(wx * 0.012f, wz * 0.012f, 4, 2.0f, 0.5f);
            float detail = n3.fbm2(wx * 0.045f, wz * 0.045f, 2, 2.0f, 0.5f);

            float h = SEA_LEVEL + continents * 22.0f + hills * 8.0f + detail * 2.0f;
            int height = (int)h;
            height = std::clamp(height, 3, WORLD_HEIGHT - 1);
            localTop[lx][lz] = height >= 5 ? height : 4;

            SurfaceInfo si = surfaceFor(tempN, moistN, wx, wz, height);

            // bedrock
            ref(out, lx, 0, lz) = B_BEDROCK;
            for (int y = 1; y <= 4; y++) {
                float chance = 0.9f - y * 0.18f;
                // deterministic per block
                uint32_t hsh = hash32((uint32_t)wx * 0x9e3779b9U ^ (uint32_t)wz ^ (uint32_t)(y * 97)) * 0x85ebca6bU;
                float r = (hsh & 0xFFFF) * (1.0f / 65535.0f);
                ref(out, lx, y, lz) = r < chance ? B_BEDROCK : B_STONE;
            }

            // base fill
            for (int y = 5; y <= height; y++)
                ref(out, lx, y, lz) = B_STONE;

            // surface materials
            if (si.ocean) {
                int floorY = height;
                uint8_t mat = si.cold ? B_GRAVEL : B_SAND;
                if (floorY >= SEA_LEVEL - 2) mat = si.cold ? B_GRAVEL : B_SAND;
                for (int y = floorY; y > floorY - 3 && y >= 5; y--)
                    ref(out, lx, y, lz) = y == floorY ? mat : (si.cold && y == floorY ? mat : B_DIRT);
                ref(out, lx, floorY, lz) = mat;
            } else {
                uint8_t top, under;
                if (si.desert) { top = B_SAND; under = B_SAND; }
                else if (si.cold) { top = B_SNOW; under = B_DIRT; }
                else { top = B_GRASS; under = B_DIRT; }
                ref(out, lx, height, lz) = top;
                for (int y = height - 1; y > height - 4 && y >= 5; y--)
                    ref(out, lx, y, lz) = under;
            }
        }
    }

    // caves (3D) — localTop already computed from height during the column fill.
    for (int y = 3; y < 90; y++) {
        for (int lx = 0; lx < CHUNK_SIZE; lx++) {
            for (int lz = 0; lz < CHUNK_SIZE; lz++) {
                int wx = baseWX + lx, wz = baseWZ + lz;
                uint8_t cur = ref(out, lx, y, lz);
                if (cur == B_AIR || cur == B_BEDROCK) continue;
                int top = localTop[lx][lz];
                if (top < 0) continue;
                if (y >= top - 4) continue;
                float cn = caveN.fbm3(wx * 0.045f, y * 0.07f, wz * 0.045f, 3, 2.0f, 0.5f);
                float cn2 = caveN2.fbm3(wx * 0.09f, y * 0.14f, wz * 0.09f, 2, 2.0f, 0.5f);
                float v = cn * 0.72f + cn2 * 0.28f;
                if (v > 0.30f) {
                    ref(out, lx, y, lz) = B_AIR;
                }
            }
        }
    }

    // ores
    Rng rng((uint64_t)seed * 0x100000001ULL ^ ((uint64_t)(uint32_t)cx << 32) ^ (uint32_t)cz ^ 0x6a09e667ULL);
    for (const Ore& ore : kOres) {
        for (int t = 0; t < ore.tries; t++) {
            int ox = rng.irange(0, CHUNK_SIZE - 1), oz = rng.irange(0, CHUNK_SIZE - 1);
            int oy = rng.irange(ore.minY, ore.maxY);
            for (int s = 0; s < ore.size; s++) {
                int x = ox + rng.irange(-2, 2);
                int y = oy + rng.irange(-2, 2);
                int z = oz + rng.irange(-2, 2);
                if (x < 0 || x >= CHUNK_SIZE || z < 0 || z >= CHUNK_SIZE || y < 1 || y >= WORLD_HEIGHT) continue;
                if (ref(out, x, y, z) == B_STONE) ref(out, x, y, z) = ore.id;
            }
        }
    }

    // trees
    for (int lx = 0; lx < CHUNK_SIZE; lx++) {
        for (int lz = 0; lz < CHUNK_SIZE; lz++) {
            int wx = baseWX + lx, wz = baseWZ + lz;
            int height = 0;
            for (int y = WORLD_HEIGHT - 1; y >= 3; y--) {
                if (ref(out, lx, y, lz) != B_AIR) { height = y; break; }
            }
            if (height < SEA_LEVEL) continue;
            uint8_t top = ref(out, lx, height, lz);
            if (top != B_GRASS && top != B_SNOW) continue;
            // avoid chunk borders so trees never cross into neighbors
            if (lx < 2 || lx >= CHUNK_SIZE - 2 || lz < 2 || lz >= CHUNK_SIZE - 2) continue;
            SurfaceInfo si = surfaceFor(tempN, moistN, wx, wz, height);
            if (si.desert) continue;
            float moist = moistN.value2(wx * 0.008f + 50.0f, wz * 0.008f + 50.0f);
            float density = 0.035f + moist * 0.05f;
            if (si.cold) density *= 0.4f;
            uint32_t hsh = hash32((uint32_t)wx * 0x9e3779b9U ^ (uint32_t)wz * 0x85ebca6bU ^ seed);
            float r = (hsh & 0xFFFF) * (1.0f / 65535.0f);
            if (r < density) {
                placeTree(out, wx, wz, height, rng);
            }
        }
    }

    // water fill. Ocean/lake columns (terrain surface below sea level) flood fully;
    // on land columns the air below sea level is carved cave air, which we flood
    // only ~20% of the time (low-frequency noise -> wet cave patches, most caves dry)
    // so the underground isn't a giant lake like vanilla aquifers.
    Noise waterN(seed ^ 0xb5297a4dU);
    for (int lx = 0; lx < CHUNK_SIZE; lx++) {
        for (int lz = 0; lz < CHUNK_SIZE; lz++) {
            int wx = baseWX + lx, wz = baseWZ + lz;
            // topmost naturally-solid block = terrain surface (caves are air below it)
            int top = WORLD_HEIGHT - 1;
            while (top >= 0) {
                uint8_t b = ref(out, lx, top, lz);
                if (b != B_AIR && b != B_WATER) break;
                top--;
            }
            bool ocean = top >= 0 && top < SEA_LEVEL;
            for (int y = SEA_LEVEL - 1; y >= 0; y--) {
                if (ref(out, lx, y, lz) != B_AIR) continue;
                if (ocean) {
                    ref(out, lx, y, lz) = B_WATER;
                } else {
                    float wn = waterN.fbm3(wx * 0.06f, y * 0.08f, wz * 0.06f, 2, 2.0f, 0.5f);
                    if (wn > 0.42f) ref(out, lx, y, lz) = B_WATER;
                }
            }
        }
    }
}

} // namespace gen
