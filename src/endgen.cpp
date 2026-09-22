#include "endgen.hpp"
#include "blocks.hpp"
#include "mcnoise.hpp"
#include "specs.hpp"
#include "world.hpp"
#include <algorithm>
#include <cmath>

namespace {

// ---------------------------------------------------------------------------
// 原版末地下界的基准噪声（base_3d_noise）只跟世界种子有关，
// 每个工作线程按种子缓存一份，避免每区块重建 40 个柏林噪声八度。
// ---------------------------------------------------------------------------
struct NoiseSet {
    uint32_t seed = 0xFFFFFFFFu;
    bool ready = false;
    McBlendedNoise* base3D = nullptr;  // end/base_3d_noise
    McSimplexNoise* islandShape = nullptr;
    uint64_t spikeKey = 0;

    void ensure(uint32_t s) {
        if (ready && seed == s) return;
        delete base3D;
        delete islandShape;
        seed = s;
        // end/base_3d_noise: old_blended_noise，xz 0.25 y 0.25 xz_factor 80 y_factor 160 smear 4
        base3D = new McBlendedNoise(s, 0.25, 0.25, 80.0, 160.0, 4.0);
        // end_islands 的 SimplexNoise：LegacyRandomSource(seed) 先消耗 17292 个 int
        McCnRandom rnd(s);
        rnd.consumeCount(17292);
        islandShape = new McSimplexNoise(rnd);
        ready = true;
    }
};

const NoiseSet& endNoise(uint32_t seed) {
    static thread_local NoiseSet ns;
    ns.ensure(seed);
    return ns;
}

constexpr float END_ISLAND_THRESHOLD = -0.9f;

inline float clampf2(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

// 原版 DensityFunctions.EndIslandDensityFunction.getHeightValue
// sectionX/sectionZ 是"8 格段"坐标（blockX / 8，整数除法向零取整）。
float rawHeightValue(const McSimplexNoise& n, int sectionX, int sectionZ) {
    int chunkX = sectionX / 2;
    int chunkZ = sectionZ / 2;
    int subX = sectionX % 2;
    int subZ = sectionZ % 2;
    float doffs = 100.0f - std::sqrt((float)(sectionX * sectionX + sectionZ * sectionZ)) * 8.0f;
    doffs = clampf2(doffs, -100.0f, 80.0f);

    for (int xo = -12; xo <= 12; xo++) {
        for (int zo = -12; zo <= 12; zo++) {
            int64_t tcx = (int64_t)chunkX + xo;
            int64_t tcz = (int64_t)chunkZ + zo;
            if (tcx * tcx + tcz * tcz > 4096LL &&
                n.getValue2D((double)tcx, (double)tcz) < (double)END_ISLAND_THRESHOLD) {
                float size = std::fmod(std::abs((float)tcx) * 3439.0f + std::abs((float)tcz) * 147.0f, 13.0f) + 9.0f;
                float xd = (float)(subX - xo * 2);
                float zd = (float)(subZ - zo * 2);
                float nd = 100.0f - std::sqrt(xd * xd + zd * zd) * size;
                nd = clampf2(nd, -100.0f, 80.0f);
                doffs = std::max(doffs, nd);
            }
        }
    }
    return doffs;
}

// 原版 erosion 通道的 end_islands 值：(h - 8) / 128
double islandDensity(const McSimplexNoise& n, int blockX, int blockZ) {
    return ((double)rawHeightValue(n, blockX / 8, blockZ / 8) - 8.0) / 128.0;
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

// 末地 final_density（见 noise_settings/end.json）。
// slideEndLike(caves, 0, 128, topStart 72, topEnd -184, topTarget -23.4375,
//              bottomStart 4, bottomEnd 32, bottomTarget -0.234375)
// 注意 slide 的形态：y 高处系数 s = mul(y_gradient, add(23.4375, caves))，
// y 低处对 s 做 lerp，最后整体加 -23.4375 再乘 0.64 —— 顺序不能合并。
// island 是已算好的 end_islands 值（列缓存），避免重复扫 24×24 个区块。
inline double endDensity(double island, const McBlendedNoise& base3D, int wx, int wy, int wz) {
    double cheese = island + base3D.compute(wx, wy, wz);
    double topFactor = yGradient(wy, 128 - 72, 128 + 184, 1.0, 0.0);
    double s = topFactor * (cheese + 23.4375);
    double bottomFactor = yGradient(wy, 0 + 4, 0 + 32, 0.0, 1.0);
    double t = mcLerp(bottomFactor, -0.234375, s);
    return squeeze(0.64 * (t - 23.4375));
}

} // 匿名命名空间

namespace endgen {
// 调试用（endprobe）：查询某点的 final_density
double debugDensity(uint32_t seed, int wx, int wy, int wz) {
    const NoiseSet& ns = endNoise(seed);
    double island = islandDensity(*ns.islandShape, wx, wz);
    return endDensity(island, *ns.base3D, wx, wy, wz);
}

// 调试用：base_3d_noise 原始值（应为 -1..1 量级）
double debugBase3D(uint32_t seed, int wx, int wy, int wz) {
    return endNoise(seed).base3D->compute(wx, wy, wz);
}

// 调试用：打印某列的原始密度（排查插值）。
void debugDumpColumn(uint32_t seed, int cx, int cz, int lx, int lz) {
    const NoiseSet& ns = endNoise(seed);
    const int wx = cx * CHUNK_SIZE + lx, wz = cz * CHUNK_SIZE + lz;
    double island = islandDensity(*ns.islandShape, wx, wz);
    for (int y = 120; y <= 128; y += 4) {
        double b3 = ns.base3D->compute(wx, y, wz);
        double topFactor = yGradient(y, 128 - 72, 128 + 184, 1.0, 0.0);
        double s = topFactor * (island + b3 + 23.4375);
        double bottomFactor = yGradient(y, 0 + 4, 0 + 32, 0.0, 1.0);
        double t = mcLerp(bottomFactor, -0.234375, s);
        fprintf(stderr, "  y=%3d island=%.5f base3D=%.5f top=%.5f s=%.5f bottom=%.3f t=%.5f dens=%.5f\n",
                y, island, b3, topFactor, s, bottomFactor, t, endDensity(island, *ns.base3D, wx, y, wz));
    }
}
} // namespace endgen

namespace {

// ---------------------------------------------------------------------------
// 黑曜石柱（原版 EndSpikeFeature）：10 根，半径 2..5，高 76..103，
// 每 36° 一根，距中心 42 格；size 为 1 或 2 的柱子顶部有铁栏杆笼子。
// ---------------------------------------------------------------------------
struct EndSpike {
    int cx, cz, radius, height;
    bool guarded;
};

// 原版 SPIKE_CACHE 的 key：random.nextLong() & 0xFFFF
uint64_t spikeCacheKey(uint32_t seed) {
    McCnRandom r(seed);
    return r.nextLong() & 0xFFFFULL;
}

// 原版 Util.toShuffledList(IntStream.range(0,10), RandomSource.create(key))
void buildSpikes(uint32_t seed, EndSpike out[10]) {
    McCnRandom rnd(spikeCacheKey(seed));
    int sizes[10];
    for (int i = 0; i < 10; i++) sizes[i] = i;
    for (int i = 10; i > 1; i--) {
        int swapTo = rnd.nextInt(i);
        int t = sizes[i - 1];
        sizes[i - 1] = sizes[swapTo];
        sizes[swapTo] = t;
    }
    for (int i = 0; i < 10; i++) {
        int x = (int)std::floor(42.0 * std::cos(2.0 * (-M_PI + (M_PI / 10.0) * i)));
        int z = (int)std::floor(42.0 * std::sin(2.0 * (-M_PI + (M_PI / 10.0) * i)));
        int size = sizes[i];
        out[i] = {x, z, 2 + size / 3, 76 + size * 3, size == 1 || size == 2};
    }
}

// 世界坐标写入：超出本区块直接丢弃（每个区块只写自己那份，见 endgen.hpp 说明）
inline void put(uint8_t* out, int baseWX, int baseWZ, int x, int y, int z, uint8_t id) {
    int lx = x - baseWX, lz = z - baseWZ;
    if (lx < 0 || lx >= CHUNK_SIZE || lz < 0 || lz >= CHUNK_SIZE) return;
    if (y < 0 || y >= WORLD_HEIGHT) return;
    out[lx + (lz << 4) + (y << 8)] = id;
}

// ---------------------------------------------------------------------------
// 紫颂植株：原版 1.9+ 的 ChorusFlowerBlock.generatePlant / growTreeRecursive。
// 生成期只写本区块（长到邻块的部分丢掉，邻块生成时会独立再摆一次，不会重复）；
// 随机数消耗顺序与原版递归版一致。
// ---------------------------------------------------------------------------
constexpr int CHORUS_MAX_SPREAD = 8;  // 原版 placed_feature/chorus_plant 传的 8
const int kChorusOff[4][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}}; // 北 东 南 西

bool chorusNeighborsEmpty(const uint8_t* out, int baseWX, int baseWZ, int x, int y, int z, int ignore) {
    for (int i = 0; i < 4; i++) {
        if (i == ignore) continue;
        int lx = x + kChorusOff[i][0] - baseWX, lz = z + kChorusOff[i][1] - baseWZ;
        if (lx < 0 || lx >= CHUNK_SIZE || lz < 0 || lz >= CHUNK_SIZE) return false;
        if (out[lx + (lz << 4) + (y << 8)] != B_AIR) return false;
    }
    return true;
}

bool chorusEmpty(const uint8_t* out, int baseWX, int baseWZ, int x, int y, int z) {
    int lx = x - baseWX, lz = z - baseWZ;
    if (lx < 0 || lx >= CHUNK_SIZE || lz < 0 || lz >= CHUNK_SIZE) return false;
    return out[lx + (lz << 4) + (y << 8)] == B_AIR;
}

void growChorus(uint8_t* out, int baseWX, int baseWZ, int x, int y, int z, McCnRandom& rnd) {
    struct Node { int x, y, z, depth; };
    Node stack[64];
    int sp = 0;
    stack[sp++] = {x, y, z, 0};
    put(out, baseWX, baseWZ, x, y, z, B_CHORUS_PLANT);

    while (sp > 0) {
        Node cur = stack[--sp];
        int height = rnd.nextInt(4) + 1;
        if (cur.depth == 0) height++;

        bool blocked = false;
        for (int i = 0; i < height; i++) {
            int ty = cur.y + i + 1;
            if (!chorusNeighborsEmpty(out, baseWX, baseWZ, cur.x, ty, cur.z, -1)) { blocked = true; break; }
            put(out, baseWX, baseWZ, cur.x, ty, cur.z, B_CHORUS_PLANT);
            put(out, baseWX, baseWZ, cur.x, ty - 1, cur.z, B_CHORUS_PLANT);
        }
        if (blocked) continue;

        bool placedStem = false;
        if (cur.depth < 4) {
            int stems = rnd.nextInt(4);
            if (cur.depth == 0) stems++;
            for (int i = 0; i < stems; i++) {
                int dir = rnd.nextInt(4);
                int tx = cur.x + kChorusOff[dir][0], tz = cur.z + kChorusOff[dir][1];
                int ty = cur.y + height;
                if (std::abs(tx - x) >= CHORUS_MAX_SPREAD || std::abs(tz - z) >= CHORUS_MAX_SPREAD) continue;
                if (!chorusEmpty(out, baseWX, baseWZ, tx, ty, tz)) continue;
                if (!chorusEmpty(out, baseWX, baseWZ, tx, ty - 1, tz)) continue;
                if (!chorusNeighborsEmpty(out, baseWX, baseWZ, tx, ty, tz, (dir + 2) % 4)) continue;
                placedStem = true;
                put(out, baseWX, baseWZ, tx, ty, tz, B_CHORUS_PLANT);
                put(out, baseWX, baseWZ, tx + kChorusOff[(dir + 2) % 4][0], ty,
                    tz + kChorusOff[(dir + 2) % 4][1], B_CHORUS_PLANT);
                if (sp < 64) stack[sp++] = {tx, ty, tz, cur.depth + 1};
            }
        }
        if (!placedStem) put(out, baseWX, baseWZ, cur.x, cur.y + height, cur.z, B_CHORUS_FLOWER_DEAD);
    }
}

// 末地植被（placed_feature/chorus_plant）：每区块 0..4 次尝试，
// 落在 MOTION_BLOCKING 高度上，下方必须是末地石。
void decorateChorus(uint32_t seed, int cx, int cz, uint8_t* out) {
    const int baseWX = cx * CHUNK_SIZE, baseWZ = cz * CHUNK_SIZE;
    McCnRandom rnd(seed ^ (uint64_t)(uint32_t)cx * 0x2545F4914F6CDD1DULL ^
                   (uint64_t)(uint32_t)cz * 0x9E3779B97F4A7C15ULL ^ 0xC0FFEE01ULL);
    int count = rnd.nextInt(5);  // count 0..4
    for (int i = 0; i < count; i++) {
        int lx = rnd.nextInt(16), lz = rnd.nextInt(16);
        int top = -1;
        for (int y = WORLD_HEIGHT - 1; y >= 0; y--)
            if (out[lx + (lz << 4) + (y << 8)] != B_AIR) { top = y; break; }
        if (top < 0 || top + 1 >= WORLD_HEIGHT) continue;
        if (out[lx + (lz << 4) + (top << 8)] != B_END_STONE) continue;
        growChorus(out, baseWX, baseWZ, baseWX + lx, top + 1, baseWZ + lz, rnd);
    }
}

// ---------------------------------------------------------------------------
// 返回传送门（原版 EndPodiumFeature，生成期用 active=false 的分支）
// 以 (0,64,0) 为中心：2.5 格内是 3×3 的末地门 + 基岩环，3.5 格内铺末地石，
// 中心立 4 格基岩柱，柱上四面挂火把。
// ---------------------------------------------------------------------------
void placePodium(uint8_t* out, int baseWX, int baseWZ, int oy) {
    for (int x = -4; x <= 4; x++) {
        for (int z = -4; z <= 4; z++) {
            for (int y = oy - 1; y <= oy + 32; y++) {
                double dsq = (double)x * x + (double)z * z;
                bool rim = dsq < 6.25;                 // closerThan(origin, 2.5)
                if (!rim && dsq >= 12.25) continue;    // 3.5 格之外不动
                if (y < oy)      put(out, baseWX, baseWZ, x, y, z, rim ? B_BEDROCK : B_END_STONE);
                else if (y > oy) put(out, baseWX, baseWZ, x, y, z, B_AIR);
                else             put(out, baseWX, baseWZ, x, y, z, rim ? B_END_PORTAL : B_BEDROCK);
            }
        }
    }
    for (int i = 0; i < 4; i++) put(out, baseWX, baseWZ, 0, oy + i, 0, B_BEDROCK);
    put(out, baseWX, baseWZ, 1, oy + 2, 0, B_WALL_TORCH);
    put(out, baseWX, baseWZ, -1, oy + 2, 0, B_WALL_TORCH);
    put(out, baseWX, baseWZ, 0, oy + 2, 1, B_WALL_TORCH);
    put(out, baseWX, baseWZ, 0, oy + 2, -1, B_WALL_TORCH);
}

// 返回传送门底座 y：原版从 heightmap 往下跳过基岩层，这里等效地从 y=70 向下找地面
int podiumOriginY(const uint8_t* out) {
    for (int y = 70; y > 63; y--) {
        if (out[0 + 0 + (y << 8)] != B_AIR) return y;
    }
    return 63;
}

// ---------------------------------------------------------------------------
// 外岛：原版 end_island feature（随机半径 4..6 的圆盘，逐层缩小）
// ---------------------------------------------------------------------------
void placeIsland(uint8_t* out, int baseWX, int baseWZ, int ox, int oy, int oz, McCnRandom& rnd) {
    // 原版用 float 尺寸，每层减 nextInt(2) + 0.5，这里用 1/2 整数累加器等价还原
    int size2 = (rnd.nextInt(3) + 4) * 2;
    int y = 0;
    while (size2 > 1) {
        int sz = size2 >> 1;                     // 整数部分
        int lo = -(sz + 1), hi = sz + 1;         // floor(-size) .. ceil(size)
        for (int x = lo; x <= hi; x++)
            for (int z = lo; z <= hi; z++)
                if ((float)(x * x + z * z) <= (sz + 1.0f) * (sz + 1.0f))
                    put(out, baseWX, baseWZ, ox + x, oy + y, oz + z, B_END_STONE);
        size2 -= rnd.nextInt(2) + 1;             // nextInt(2) + 0.5，单位 1/2
        y--;
    }
}

// 群系判定（原版 TheEndBiomeSource）：中心 64 区块内是 the_end，
// 之外才是 end_highlands / small_end_islands 这些外岛群系。
// 本实现只做"是不是外岛群系"，用来决定放不放外岛/紫颂（对齐原版 features 表）。
inline bool isOuterBiomeChunk(int cx, int cz) {
    return (int64_t)cx * cx + (int64_t)cz * cz > 4096LL;
}

// 一个区块里 end_island_decorated 的落地。
// 外岛半径最多 7 格，会跨区块，所以把 3×3 邻域的候选都算一遍，
// put 只保留属于本区块的方块（各区块独立、与生成顺序无关）。
void decorateIslands(uint32_t seed, int cx, int cz, uint8_t* out) {
    const int baseWX = cx * CHUNK_SIZE, baseWZ = cz * CHUNK_SIZE;
    for (int dx = -1; dx <= 1; dx++) {
        for (int dz = -1; dz <= 1; dz++) {
            int ox = cx + dx, oz = cz + dz;
            // rarity_filter chance=14 的随机源（每区块一份，确定性）
            McCnRandom rnd(seed ^ (uint64_t)(uint32_t)ox * 0x9E3779B97F4A7C15ULL ^
                           (uint64_t)(uint32_t)oz * 0xC2B2AE3D27D4EB4FULL ^ 0x5EED1501ULL);
            if (rnd.nextFloat() >= 1.0f / 14.0f) continue;
            // weighted_list：1（权重 3）或 2（权重 1）
            int count = rnd.nextInt(4) == 0 ? 2 : 1;
            for (int i = 0; i < count; i++) {
                int bx = ox * 16 + rnd.nextInt(16);
                int bz = oz * 16 + rnd.nextInt(16);
                int by = 55 + rnd.nextInt(16);  // uniform 55..70
                placeIsland(out, baseWX, baseWZ, bx, by, bz, rnd);
            }
        }
    }
}

} // 匿名命名空间

namespace endgen {

// 调试/测试用：直接给出某列的 end_islands 高度值
float islandHeightValue(int blockX, int blockZ) {
    const NoiseSet& ns = endNoise(0);
    return rawHeightValue(*ns.islandShape, blockX / 8, blockZ / 8);
}

void generateEnd(uint32_t seed, int cx, int cz, uint8_t* out) {
    std::fill(out, out + CHUNK_VOL, (uint8_t)B_AIR);

    const NoiseSet& ns = endNoise(seed);
    const int baseWX = cx * CHUNK_SIZE, baseWZ = cz * CHUNK_SIZE;

    // 采样格点：水平与垂直都每 4 格一个点（end.json 的 size_horizontal=2、
    // size_vertical=1，对应 8×8×4 方块一格；NoiseInterpolator 对每个方块插值，
    // 这里按 4 格步长直接采样，结果等价且省内存）。
    constexpr int SN = 11;   // x/z 采样点 sx∈[0,10] → cellX = sx-1 ∈ [-1,9]
    constexpr int SNY = 34;  // y 采样点 0..33 → y = 0..128
    double samples[SN][SNY][SN];

    // end_islands 只看 x/z，先按列算好（每列要扫 24×24 个区块，很贵）
    double islands[SN][SN];
    for (int sz = 0; sz < SN; sz++)
        for (int sx = 0; sx < SN; sx++)
            islands[sx][sz] = islandDensity(*ns.islandShape, baseWX + (sx - 1) * 4, baseWZ + (sz - 1) * 4);

    for (int sy = 0; sy < SNY; sy++) {
        int wy = sy * 4;
        if (wy > WORLD_HEIGHT) break;
        for (int sz = 0; sz < SN; sz++) {
            int wz = baseWZ + (sz - 1) * 4;
            for (int sx = 0; sx < SN; sx++) {
                int wx = baseWX + (sx - 1) * 4;
                samples[sx][sy][sz] = endDensity(islands[sx][sz], *ns.base3D, wx, wy, wz);
            }
        }
    }

    // 逐方块三线性插值
    for (int lz = 0; lz < CHUNK_SIZE; lz++) {
        const int cz0 = lz >> 2;
        const double fz = (lz & 3) * 0.25;
        for (int lx = 0; lx < CHUNK_SIZE; lx++) {
            const int cx0 = lx >> 2;
            const double fx = (lx & 3) * 0.25;
            const int si = cx0 + 1, sj = cz0 + 1;
            for (int y = 0; y < WORLD_HEIGHT; y++) {
                const int cy0 = y >> 2;
                const double fy = (y & 3) * 0.25;
                double v000 = samples[si][cy0][sj],      v100 = samples[si + 1][cy0][sj];
                double v010 = samples[si][cy0 + 1][sj],  v110 = samples[si + 1][cy0 + 1][sj];
                double v001 = samples[si][cy0][sj + 1],  v101 = samples[si + 1][cy0][sj + 1];
                double v011 = samples[si][cy0 + 1][sj + 1], v111 = samples[si + 1][cy0 + 1][sj + 1];
                double a0 = mcLerp(fx, v000, v100), a1 = mcLerp(fx, v001, v101);
                double b0 = mcLerp(fx, v010, v110), b1 = mcLerp(fx, v011, v111);
                double c0 = mcLerp(fy, a0, b0), c1 = mcLerp(fy, a1, b1);
                double d = mcLerp(fz, c0, c1);
                if (d > 0.0) out[lx + (lz << 4) + (y << 8)] = B_END_STONE;
            }
        }
    }

    // ---- 黑曜石柱（只在柱心所在区块生成，等价原版 isCenterWithinChunk）----
    EndSpike spikes[10];
    buildSpikes(seed, spikes);
    for (const EndSpike& s : spikes) {
        if (blockToChunkCoord(s.cx) != cx || blockToChunkCoord(s.cz) != cz) continue;
        int r2 = s.radius * s.radius + 1;
        for (int x = s.cx - s.radius; x <= s.cx + s.radius; x++) {
            for (int z = s.cz - s.radius; z <= s.cz + s.radius; z++) {
                if ((x - s.cx) * (x - s.cx) + (z - s.cz) * (z - s.cz) > r2) continue;
                for (int y = 0; y < s.height; y++) put(out, baseWX, baseWZ, x, y, z, B_OBSIDIAN);
                // 柱体上方直到 y=65 之外全部清空（原版 pos.getY() > 65 → AIR）
                for (int y = 66; y <= s.height + 10; y++) put(out, baseWX, baseWZ, x, y, z, B_AIR);
            }
        }
        // 柱顶：基岩 + 火（原版先放水晶实体，再把水晶脚下换基岩、脚下上方放火）
        put(out, baseWX, baseWZ, s.cx, s.height, s.cz, B_BEDROCK);
        put(out, baseWX, baseWZ, s.cx, s.height + 1, s.cz, B_FIRE);
        if (s.guarded) {
            for (int dx = -2; dx <= 2; dx++)
                for (int dz = -2; dz <= 2; dz++)
                    for (int dy = 0; dy <= 3; dy++) {
                        bool side = std::abs(dx) == 2 || std::abs(dz) == 2;
                        if (side || dy == 3)
                            put(out, baseWX, baseWZ, s.cx + dx, s.height + dy, s.cz + dz, B_IRON_BARS);
                    }
        }
    }

    // ---- 返回传送门 ----
    if (cx == 0 && cz == 0) placePodium(out, baseWX, baseWZ, podiumOriginY(out));

    // ---- 末地平台 ----
    if (blockToChunkCoord(END_PLATFORM_X) == cx && blockToChunkCoord(END_PLATFORM_Z) == cz) {
        for (int dx = -2; dx <= 2; dx++)
            for (int dz = -2; dz <= 2; dz++) {
                put(out, baseWX, baseWZ, END_PLATFORM_X + dx, END_PLATFORM_Y - 1, END_PLATFORM_Z + dz, B_OBSIDIAN);
                for (int dy = 0; dy < 3; dy++)
                    put(out, baseWX, baseWZ, END_PLATFORM_X + dx, END_PLATFORM_Y + dy, END_PLATFORM_Z + dz, B_AIR);
            }
    }

    // ---- 外岛与紫颂只在外岛群系生成（the_end 中心群系里没有这两个 feature）----
    if (isOuterBiomeChunk(cx, cz)) {
        decorateIslands(seed, cx, cz, out);
        decorateChorus(seed, cx, cz, out);
    }
}

int surfaceHeight(const uint8_t* out, int lx, int lz) {
    for (int y = WORLD_HEIGHT - 1; y >= 0; y--)
        if (out[lx + (lz << 4) + (y << 8)] != B_AIR) return y;
    return -1;
}

} // namespace endgen
