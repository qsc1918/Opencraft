#include "nethergen.hpp"
#include "blocks.hpp"
#include "mcnoise.hpp"
#include "specs.hpp"
#include <algorithm>
#include <cmath>

namespace {

// ---------------------------------------------------------------------------
// 原版下界 base_3d_noise（noise_settings/nether.json 的 old_blended_noise 参数）
// 只跟世界种子有关，每线程缓存一份。
// ---------------------------------------------------------------------------
struct NetherNoiseSet {
    uint32_t seed = 0xFFFFFFFFu;
    bool ready = false;
    McBlendedNoise* base3D = nullptr;

    void ensure(uint32_t s) {
        if (ready && seed == s) return;
        delete base3D;
        seed = s;
        // xz_scale 0.25, y_scale 0.25, xz_factor 80, y_factor 160, smear 4
        base3D = new McBlendedNoise(s, 0.25, 0.25, 80.0, 160.0, 4.0);
        ready = true;
    }
};

const NetherNoiseSet& netherNoise(uint32_t seed) {
    static thread_local NetherNoiseSet ns;
    ns.ensure(seed);
    return ns;
}

// 原版 y_clamped_gradient
inline double yGradient(int y, int fromY, int toY, double fromValue, double toValue) {
    return mcClampedMap((double)y, (double)fromY, (double)toY, fromValue, toValue);
}

// 原版 Mapped.SQUEEZE
inline double squeeze(double v) {
    double c = v < -1.0 ? -1.0 : (v > 1.0 ? 1.0 : v);
    return c / 2.0 - c * c * c / 24.0;
}

// 原版 slideNetherLike：slide(base_3d_noise, 0, 128, 24, 0, 0.9375, -8, 24, 2.5)
inline double netherDensity(const McBlendedNoise& base3D, int wx, int wy, int wz) {
    double v = base3D.compute(wx, wy, wz);
    double topFactor = yGradient(wy, 128 - 24, 128 - 0, 1.0, 0.0);
    double s = mcLerp(topFactor, 0.9375, v);
    double bottomFactor = yGradient(wy, 0 + -8, 0 + 24, 0.0, 1.0);
    double t = mcLerp(bottomFactor, 2.5, s);
    return squeeze(0.64 * t);
}

inline uint8_t& ref(uint8_t* b, int x, int y, int z) { return b[x + (z << 4) + (y << 8)]; }
inline uint8_t at(const uint8_t* b, int x, int y, int z) { return b[x + (z << 4) + (y << 8)]; }

} // 匿名命名空间

namespace nethergen {

void generateNether(uint32_t seed, int cx, int cz, uint8_t* out) {
    std::fill(out, out + CHUNK_VOL, (uint8_t)B_AIR);
    const NetherNoiseSet& ns = netherNoise(seed);
    const int baseWX = cx * CHUNK_SIZE, baseWZ = cz * CHUNK_SIZE;

    // 1) 密度填充（原版 final_density > 0 即默认方块 netherrack）
    for (int lz = 0; lz < CHUNK_SIZE; lz++) {
        for (int lx = 0; lx < CHUNK_SIZE; lx++) {
            for (int y = 0; y < WORLD_HEIGHT; y++) {
                if (netherDensity(*ns.base3D, baseWX + lx, y, baseWZ + lz) > 0.0)
                    ref(out, lx, y, lz) = B_NETHERRACK;
            }
        }
    }

    // 2) 表面规则：先基岩地板/天花板，再下界荒地群系的表层，最后兜底 netherrack
    const int32_t floorHash = javaStringHash("minecraft:bedrock_floor");
    const int32_t roofHash = javaStringHash("minecraft:bedrock_roof");
    for (int lz = 0; lz < CHUNK_SIZE; lz++) {
        for (int lx = 0; lx < CHUNK_SIZE; lx++) {
            int wx = baseWX + lx, wz = baseWZ + lz;
            for (int y = 0; y < 5; y++) {
                // bedrock_floor: true_at_and_below above_bottom 0，false_at_and_above above_bottom 5
                if (y <= 0) { ref(out, lx, y, lz) = B_BEDROCK; continue; }
                float p = (float)((5.0 - y) / 5.0);
                int64_t s = (int64_t)floorHash ^ mcPosSeed(wx, y, wz);
                if (McCnRandom((uint64_t)s).nextFloat() < p) ref(out, lx, y, lz) = B_BEDROCK;
            }
            for (int y = 123; y < WORLD_HEIGHT; y++) {
                // bedrock_roof: true_at_and_below below_top 5，false_at_and_above below_top 0
                if (y >= WORLD_HEIGHT - 1) { ref(out, lx, y, lz) = B_BEDROCK; continue; }
                float p = (float)((y - (WORLD_HEIGHT - 5)) / 5.0);
                int64_t s = (int64_t)roofHash ^ mcPosSeed(wx, y, wz);
                if (McCnRandom((uint64_t)s).nextFloat() < p) ref(out, lx, y, lz) = B_BEDROCK;
            }
        }
    }

    // 3) 岩浆海：sea_level 32，y < 32 的空位灌岩浆（原版默认流体 + lava 规则）
    for (int lz = 0; lz < CHUNK_SIZE; lz++)
        for (int lx = 0; lx < CHUNK_SIZE; lx++)
            for (int y = 0; y < 32; y++)
                if (ref(out, lx, y, lz) == B_AIR) ref(out, lx, y, lz) = B_LAVA;

    // 4) 下界荒地群系表层：灵魂沙层 / 沙砾层 / 兜底地狱岩
    //    （原版用 surface noise + stone_depth 判定，这里用同频噪声做等价近似）
    for (int lz = 0; lz < CHUNK_SIZE; lz++) {
        for (int lx = 0; lx < CHUNK_SIZE; lx++) {
            int wx = baseWX + lx, wz = baseWZ + lz;
            for (int y = 32; y < 123; y++) {
                if (at(out, lx, y, lz) != B_NETHERRACK) continue;
                if (at(out, lx, y + 1, lz) != B_AIR) continue;  // 只改表层
                // 灵魂沙：y 在 30..35 之间（原版 soul_sand_layer 阈值 -0.012）
                if (y >= 30 && y < 35) {
                    float n = (float)netherDensity(*ns.base3D, wx, y, wz);
                    if (n > -0.05f) {
                        for (int d = 0; d < 4 && y - d >= 0; d++) ref(out, lx, y - d, lz) = B_SOUL_SAND;
                        continue;
                    }
                }
                // 沙砾：y 在 31..34 之间
                if (y >= 31 && y < 35) {
                    float n = (float)netherDensity(*ns.base3D, wx + 1337, y, wz - 733);
                    if (n > 0.0f) ref(out, lx, y, lz) = B_GRAVEL;
                }
            }
        }
    }
}

void generateChunk(uint32_t seed, int cx, int cz, uint8_t* out) {
    generateNether(seed, cx, cz, out);
}

} // namespace nethergen
