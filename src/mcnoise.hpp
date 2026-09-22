#pragma once
// ===========================================================================
// mcnoise.hpp — 原版噪声/随机数复刻
//
// 末地、下界的地形生成完全按原版 density_function / noise 数据驱动，
// 因此这里逐位复刻 MC 的随机源与噪声，保证同种子地形一致。
// 对齐版本：Java 版 26.2（net.minecraft.world.level.levelgen.synth 等）。
// ===========================================================================
#include "util.hpp"
#include <cstdint>
#include <vector>

// ---------------------------------------------------------------------------
// 原版 LegacyRandomSource：48 位 LCG，next(bits) 取高 bits 位。
// 所有原版结构（噪声八度、末地柱）都由它驱动，必须逐位一致。
// ---------------------------------------------------------------------------
class McCnRandom {
public:
    explicit McCnRandom(uint64_t seed) { setSeed(seed); }
    void setSeed(uint64_t seed) { state_ = (seed ^ 25214903917ULL) & 281474976710655ULL; }

    // 高 bits 位；bits 在 1..32
    int32_t next(int bits) {
        state_ = (state_ * 25214903917ULL + 11ULL) & 281474976710655ULL;
        return (int32_t)(state_ >> (48 - bits));
    }
    int32_t nextInt() { return next(32); }
    // 原版 nextInt(bound)：2 的幂走乘法路径，否则拒绝采样
    int32_t nextInt(int32_t bound) {
        if (bound <= 0) return 0;
        if ((bound & (bound - 1)) == 0) return (int32_t)((int64_t)bound * next(31) >> 31);
        int32_t sample, modulo;
        do {
            sample = next(31);
            modulo = sample % bound;
        } while (sample - modulo + (bound - 1) < 0);
        return modulo;
    }
    // 高 32 位在前
    uint64_t nextLong() {
        int32_t hi = next(32), lo = next(32);
        return ((uint64_t)(uint32_t)hi << 32) + (uint32_t)lo;
    }
    double nextDouble() {
        int32_t hi = next(26), lo = next(27);
        int64_t combined = ((int64_t)hi << 27) + lo;
        return (double)combined * 1.1102230246251565e-16;
    }
    float nextFloat() { return (float)(next(24) * 5.9604645e-8); }
    void consumeCount(int n) { for (int i = 0; i < n; i++) nextInt(); }

private:
    uint64_t state_ = 0;
};

// Java 字符串 hashCode：用于 fromHashOf
inline int32_t javaStringHash(const char* s) {
    int32_t h = 0;
    for (; *s; s++) h = 31 * h + (int32_t)(uint8_t)*s;
    return h;
}

// ---------------------------------------------------------------------------
// Mth 辅助（名字带 mc 前缀，避免和 util.hpp 的 clampf 混淆）
// ---------------------------------------------------------------------------
inline double mcLerp(double a, double p0, double p1) { return p0 + a * (p1 - p0); }
inline double mcSmoothstep(double x) { return x * x * x * (x * (x * 6.0 - 15.0) + 10.0); }
inline double mcInverseLerp(double v, double a, double b) { return (v - a) / (b - a); }
inline double mcClampedLerp(double f, double a, double b) {
    if (f < 0.0) return a;
    return f > 1.0 ? b : mcLerp(f, a, b);
}
inline double mcClampedMap(double v, double fa, double fb, double ta, double tb) {
    return mcClampedLerp(mcInverseLerp(v, fa, fb), ta, tb);
}
// 原版 Mth.getSeed：结构定位种子
inline int64_t mcPosSeed(int x, int y, int z) {
    int64_t l = (int64_t)(x * 3129871) ^ ((int64_t)z * 116129781LL) ^ (int64_t)y;
    l = l * l * 42317861LL + l * 11LL;
    return l >> 16;
}

// ---------------------------------------------------------------------------
// 原版 ImprovedNoise（Perlin 噪声的单八度）
// ---------------------------------------------------------------------------
class McImprovedNoise {
public:
    explicit McImprovedNoise(McCnRandom& rnd) {
        xo = rnd.nextDouble() * 256.0;
        yo = rnd.nextDouble() * 256.0;
        zo = rnd.nextDouble() * 256.0;
        for (int i = 0; i < 256; i++) p[i] = (uint8_t)i;
        for (int i = 0; i < 256; i++) {
            int off = rnd.nextInt(256 - i);
            uint8_t t = p[i];
            p[i] = p[i + off];
            p[i + off] = t;
        }
    }
    // yScale/yFudge 非零时按原版把 y 量化到分层（BlendedNoise 用）
    double noise(double x, double y, double z, double yScale, double yFudge) const {
        x += xo; y += yo; z += zo;
        int xf = (int)std::floor(x), yf = (int)std::floor(y), zf = (int)std::floor(z);
        double xr = x - xf, yr = y - yf, zr = z - zf;
        double yrFudge;
        if (yScale != 0.0) {
            double limit = (yFudge >= 0.0 && yFudge < yr) ? yFudge : yr;
            yrFudge = std::floor(limit / yScale + 1.0e-7) * yScale;
        } else {
            yrFudge = 0.0;
        }
        return sampleAndLerp(xf, yf, zf, xr, yr - yrFudge, zr, yr);
    }

private:
    int pGet(int x) const { return p[x & 0xFF]; }
    static double gradDot(int hash, double x, double y, double z) {
        static const int GRAD[16][3] = {
            {1,1,0},{-1,1,0},{1,-1,0},{-1,-1,0},
            {1,0,1},{-1,0,1},{1,0,-1},{-1,0,-1},
            {0,1,1},{0,-1,1},{0,1,-1},{0,-1,-1},
            {1,1,0},{0,-1,1},{-1,1,0},{0,-1,-1},
        };
        const int* g = GRAD[hash & 15];
        return g[0] * x + g[1] * y + g[2] * z;
    }
    double sampleAndLerp(int x, int y, int z, double xr, double yr, double zr, double yrOrig) const {
        int x0 = pGet(x), x1 = pGet(x + 1);
        int xy00 = pGet(x0 + y), xy01 = pGet(x0 + y + 1);
        int xy10 = pGet(x1 + y), xy11 = pGet(x1 + y + 1);
        double d000 = gradDot(pGet(xy00 + z), xr, yr, zr);
        double d100 = gradDot(pGet(xy10 + z), xr - 1.0, yr, zr);
        double d010 = gradDot(pGet(xy01 + z), xr, yr - 1.0, zr);
        double d110 = gradDot(pGet(xy11 + z), xr - 1.0, yr - 1.0, zr);
        double d001 = gradDot(pGet(xy00 + z + 1), xr, yr, zr - 1.0);
        double d101 = gradDot(pGet(xy10 + z + 1), xr - 1.0, yr, zr - 1.0);
        double d011 = gradDot(pGet(xy01 + z + 1), xr, yr - 1.0, zr - 1.0);
        double d111 = gradDot(pGet(xy11 + z + 1), xr - 1.0, yr - 1.0, zr - 1.0);
        double xa = mcSmoothstep(xr), ya = mcSmoothstep(yrOrig), za = mcSmoothstep(zr);
        double l0 = mcLerp(xa, d000, d100), l1 = mcLerp(xa, d010, d110);
        double l2 = mcLerp(xa, d001, d101), l3 = mcLerp(xa, d011, d111);
        return mcLerp(za, mcLerp(ya, l0, l1), mcLerp(ya, l2, l3));
    }

    uint8_t p[256];
public:
    double xo = 0, yo = 0, zo = 0;
};

// ---------------------------------------------------------------------------
// 原版 SimplexNoise：末地主岛/外岛形状噪声（end_islands）用它
// ---------------------------------------------------------------------------
class McSimplexNoise {
public:
    explicit McSimplexNoise(McCnRandom& rnd) {
        xo = rnd.nextDouble() * 256.0;
        yo = rnd.nextDouble() * 256.0;
        zo = rnd.nextDouble() * 256.0;
        for (int i = 0; i < 256; i++) p[i] = i;
        for (int i = 0; i < 256; i++) {
            int off = rnd.nextInt(256 - i);
            int t = p[i];
            p[i] = p[off + i];
            p[off + i] = t;
        }
    }
    double getValue2D(double xin, double yin) const;

private:
    int pGet(int x) const { return p[x & 0xFF]; }
    int p[512];
public:
    double xo = 0, yo = 0, zo = 0;
};

// ---------------------------------------------------------------------------
// 原版 PerlinNoise：八度叠加的柏林噪声（BlendedNoise 的组成部分）
// ---------------------------------------------------------------------------
class McPerlinNoise {
public:
    // 原版 createLegacyForBlendedNoise：八度从 lowOctave..0，创建后不再消耗随机源
    McPerlinNoise(McCnRandom& rnd, int lowOctave);
    ~McPerlinNoise();
    McPerlinNoise(const McPerlinNoise&) = delete;
    McPerlinNoise& operator=(const McPerlinNoise&) = delete;
    double getValue(double x, double y, double z, double yScale, double yFudge) const;
    double edgeValue(double noiseValue) const;
    double maxBrokenValue(double yScale) const { return edgeValue(yScale + 2.0); }
    const McImprovedNoise* octave(int i) const { return levels[(int)levels.size() - 1 - i]; }
    static double wrap(double x) {
        return x - std::floor(x / 3.3554432e7 + 0.5) * 3.3554432e7;
    }

private:
    std::vector<McImprovedNoise*> levels;  // 可能含 nullptr（振幅为 0 的八度）
    int firstOctave = 0;
    double lowestFreqInputFactor = 1.0;
    double lowestFreqValueFactor = 1.0;
};

// ---------------------------------------------------------------------------
// 原版 BlendedNoise：末地/下界的 base_3d_noise
// ---------------------------------------------------------------------------
class McBlendedNoise {
public:
    // 原版 BlendedNoise(xzScale, yScale, xzFactor, yFactor, smear) + withNewRandom(seed)
    McBlendedNoise(uint64_t seed, double xzScale, double yScale,
                   double xzFactor, double yFactor, double smearScaleMultiplier);
    ~McBlendedNoise();
    McBlendedNoise(const McBlendedNoise&) = delete;
    McBlendedNoise& operator=(const McBlendedNoise&) = delete;
    double compute(int blockX, int blockY, int blockZ) const;

private:
    // 三段噪声必须按原版顺序共用同一个随机源，指针避免浅拷贝
    McPerlinNoise* minLimit;
    McPerlinNoise* maxLimit;
    McPerlinNoise* main;
    double xzMultiplier, yMultiplier, xzFactor, yFactor, smearScaleMultiplier;
};
