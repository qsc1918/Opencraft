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
    int height;      // 地表最高方块 y
    bool ocean;      // 条件：height < SEA_LEVEL
    bool desert;     // 炎热
    bool cold;       // 寒冷
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
    // 先放树干，避免之后被树叶覆盖
    for (int i = 0; i < trunk; i++) {
        int y = rootY + 1 + i;
        if (y < 2 || y >= WORLD_HEIGHT) continue;
        uint8_t& t = ref(b, cxBase, y, czBase);
        if (t == B_AIR || t == B_WATER) t = B_LOG;
    }
    // 树叶
    for (int dy = -2; dy <= 1; dy++) {
        int ly = rootY + trunk + dy;
        if (ly < 2 || ly >= WORLD_HEIGHT) continue;
        int r = dy <= 0 ? 2 : 1;
        for (int dx = -r; dx <= r; dx++) {
            for (int dz = -r; dz <= r; dz++) {
                int x = cxBase + dx, z = czBase + dz;
                if (x < 0 || x >= CHUNK_SIZE || z < 0 || z >= CHUNK_SIZE) continue;
                if (dx == 0 && dz == 0 && dy <= 0) continue; // 留出树干位置（dy=1 封顶）
                if (dx * dx + dz * dz > r * r + 1) continue;
                uint8_t cur = ref(b, x, ly, z);
                if (cur == B_AIR || cur == B_WATER) ref(b, x, ly, z) = B_LEAVES;
            }
        }
    }
}

} // 匿名命名空间

// ===========================================================================
// 下界生成（对齐原版风格）
// - 主体是连成一片的地狱岩，用 3D 噪声挖出大洞穴，而不是零散浮岛
// - 基岩地板 y=0、天花板 y=127（y=1/126 各 50%）
// - y≤31 的非实心处灌成岩浆海（顶面平在 y=31）
// - 萤石簇挂在天花板下方，少量灵魂沙块
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

            // 基岩地板 (y=0/1) 和天花板 (y=126/127)：整层实心。
            // 不要留 50% 随机缺口——那样从下方斜看会露出上层基岩的侧面，
            // 整片天花板会变成明暗条纹。
            ref(out, lx, 0, lz) = B_BEDROCK;
            ref(out, lx, 1, lz) = B_BEDROCK;
            ref(out, lx, 126, lz) = B_BEDROCK;
            ref(out, lx, 127, lz) = B_BEDROCK;

            // 主密度：阈值以下为地狱岩，噪声高处被挖成洞穴。
            // 分三段：岩浆层最空、中部大洞穴、天花板附近最实。
            for (int y = 2; y <= 125; y++) {
                float n = nethN.fbm3(wx * 0.02f, y * 0.045f, wz * 0.02f, 3, 2.0f, 0.5f);
                float cave = caveN.fbm3(wx * 0.05f, y * 0.09f, wz * 0.05f, 2, 2.0f, 0.5f);
                float density;
                if (y <= 34) density = -0.15f;                       // 岩浆海：大部分是空的
                else if (y < 44) density = -0.15f + (y - 34) * 0.022f; // 过渡到中部
                else density = 0.07f;                                // 中部：大洞穴
                if (y > 100) density += (y - 100) * 0.028f;           // 靠近天花板更实
                if (n * 0.65f + cave * 0.35f < density) {
                    ref(out, lx, y, lz) = B_NETHERRACK;
                }
            }

            // 岩浆海 y≤31：非实心处灌岩浆，表面平在 y=31
            for (int y = 2; y <= 31; y++) {
                if (ref(out, lx, y, lz) == B_AIR) {
                    ref(out, lx, y, lz) = B_LAVA;
                }
            }

            // 灵魂沙：地狱岩表层成片的区域（对齐原版灵魂沙峡谷）
            float sv = soulN.fbm3(wx * 0.02f, 48.0f, wz * 0.02f, 2, 2.0f, 0.5f);
            if (sv > 0.35f) {
                for (int y = 34; y <= 96; y++) {
                    if (ref(out, lx, y, lz) != B_NETHERRACK) continue;
                    if (ref(out, lx, y + 1, lz) != B_AIR) continue;
                    for (int d = 0; d < 4 && y - d >= 34; d++)
                        ref(out, lx, y - d, lz) = B_SOUL_SAND;
                }
            }

            // 萤石簇挂在天花板下方
            float gv = glowN.fbm3(wx * 0.04f, 120.0f, wz * 0.04f, 2, 2.0f, 0.5f);
            if (gv > 0.45f) {
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
// 末地生成（最新机制）
// - 主岛：end_stone 圆形岛屿（半径 ~100，Perlin 起伏）
// - 10 根黑曜石柱（半径 43 内，高 76-103，2 根带铁笼）
// - 出口传送门：5×5 基岩平台 y=63，中心柱到 y=67
// - 折跃门：环绕岛屿（最多 20，简化为静态生成）
// ===========================================================================

// 末地柱参数：固定半径 ~43，按角度分布
struct EndPillar {
    float angle;   // 弧度
    int height;    // 总高度（y=0 到 y=height）
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

    // 主岛中心 (0,0)，半径 ~100，Perlin 起伏
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

    // 10 根黑曜石柱坐标固定，只在所属区块内放置
    for (const auto& p : kPillars) {
        int worldPx = (int)(cosf(p.angle) * 43.0f);
        int worldPz = (int)(sinf(p.angle) * 43.0f);
        // 判断柱子的世界坐标是否落在本区块
        int localPx = worldPx - baseWX;
        int localPz = worldPz - baseWZ;
        if (localPx < 0 || localPx >= CHUNK_SIZE || localPz < 0 || localPz >= CHUNK_SIZE) continue;
        // 单方块柱：y=1 到 y=height
        for (int y = 1; y <= p.height && y < WORLD_HEIGHT; y++) {
            ref(out, localPx, y, localPz) = B_OBSIDIAN;
        }
        // caged 时在柱顶周围放铁块
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

    // 出口传送门：5×5 基岩平台 y=63（只在 chunk(0,0) 生成，否则每块都有）
    // 原版结构：底部 5×5 基岩框架，中心 3×3 end_portal，顶部基岩柱到 y=67
    if (cx == 0 && cz == 0) {
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
        // 龙蛋在 y=68（出口柱顶端），由主循环杀死龙后放置
    }
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

    // 预计算每列地表高度供洞穴阶段使用，省掉之后 16×16×128 全扫描。
    // 此时每列最高非空气块就是 height（≥5）；地形极低时 (height<5) 为 y=4 基岩。
    int localTop[CHUNK_SIZE][CHUNK_SIZE];

    for (int lx = 0; lx < CHUNK_SIZE; lx++) {
        for (int lz = 0; lz < CHUNK_SIZE; lz++) {
            int wx = baseWX + lx, wz = baseWZ + lz;

            // 分层高度图：用平滑噪声金字塔，使相邻列落差最多 1-2 格，
            // 避免出现阶梯状峡谷。
            float continents = n1.fbm2(wx * 0.004f, wz * 0.004f, 3, 2.0f, 0.5f);
            float hills = n2.fbm2(wx * 0.012f, wz * 0.012f, 4, 2.0f, 0.5f);
            float detail = n3.fbm2(wx * 0.045f, wz * 0.045f, 2, 2.0f, 0.5f);

            float h = SEA_LEVEL + continents * 22.0f + hills * 8.0f + detail * 2.0f;
            int height = (int)h;
            height = std::clamp(height, 3, WORLD_HEIGHT - 1);
            localTop[lx][lz] = height >= 5 ? height : 4;

            SurfaceInfo si = surfaceFor(tempN, moistN, wx, wz, height);

            // 基岩层
            ref(out, lx, 0, lz) = B_BEDROCK;
            for (int y = 1; y <= 4; y++) {
                float chance = 0.9f - y * 0.18f;
                // 逐方块确定性随机
                uint32_t hsh = hash32((uint32_t)wx * 0x9e3779b9U ^ (uint32_t)wz ^ (uint32_t)(y * 97)) * 0x85ebca6bU;
                float r = (hsh & 0xFFFF) * (1.0f / 65535.0f);
                ref(out, lx, y, lz) = r < chance ? B_BEDROCK : B_STONE;
            }

            // 基础填充
            for (int y = 5; y <= height; y++)
                ref(out, lx, y, lz) = B_STONE;

            // 地表材质
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

    // 洞穴 (3D)：localTop 已在填列阶段算好
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

    // 矿石
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

    // 树木
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
            // 避开区块边界，防止树跨到邻块
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

    // 注水：海/湖列（地表低于海平面）整列灌满；陆地上低于海平面的空气
    // 多是洞穴，只按低频噪声约 20% 注水，避免地下变成原版含水层那样的大湖。
    Noise waterN(seed ^ 0xb5297a4dU);
    for (int lx = 0; lx < CHUNK_SIZE; lx++) {
        for (int lz = 0; lz < CHUNK_SIZE; lz++) {
            int wx = baseWX + lx, wz = baseWZ + lz;
            // 最高自然实心块即地表（其下洞穴已是空气）
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

} // 命名空间 gen
