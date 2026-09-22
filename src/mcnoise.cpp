#include "mcnoise.hpp"
#include <cmath>

// ---------------------------------------------------------------------------
// SimplexNoise.getValue(x, y)锛氬師鐗堜簩缁?simplex锛屾湯鍦板舰鐘跺櫔澹扮殑鏍稿績
// ---------------------------------------------------------------------------
double McSimplexNoise::getValue2D(double xin, double yin) const {
    static const double SQRT3 = 1.7320508075688772;
    const double F2 = 0.5 * (SQRT3 - 1.0);
    const double G2 = (3.0 - SQRT3) / 6.0;
    static const int GRAD[16][3] = {
        {1,1,0},{-1,1,0},{1,-1,0},{-1,-1,0},
        {1,0,1},{-1,0,1},{1,0,-1},{-1,0,-1},
        {0,1,1},{0,-1,1},{0,1,-1},{0,-1,-1},
        {1,1,0},{0,-1,1},{-1,1,0},{0,-1,-1},
    };
    double s = (xin + yin) * F2;
    int i = (int)std::floor(xin + s);
    int j = (int)std::floor(yin + s);
    double t = (i + j) * G2;
    double x0 = xin - (i - t);
    double y0 = yin - (j - t);
    int i1, j1;
    if (x0 > y0) { i1 = 1; j1 = 0; } else { i1 = 0; j1 = 1; }
    double x1 = x0 - i1 + G2, y1 = y0 - j1 + G2;
    double x2 = x0 - 1.0 + 2.0 * G2, y2 = y0 - 1.0 + 2.0 * G2;
    int ii = i & 0xFF, jj = j & 0xFF;
    int gi0 = pGet(ii + pGet(jj)) % 12;
    int gi1 = pGet(ii + i1 + pGet(jj + j1)) % 12;
    int gi2 = pGet(ii + 1 + pGet(jj + 1)) % 12;
    auto corner = [](const int* g, double x, double y) {
        double tt = 0.5 - x * x - y * y;
        if (tt < 0.0) return 0.0;
        tt *= tt;
        return tt * tt * (g[0] * x + g[1] * y);
    };
    double n0 = corner(GRAD[gi0], x0, y0);
    double n1 = corner(GRAD[gi1], x1, y1);
    double n2 = corner(GRAD[gi2], x2, y2);
    return 70.0 * (n0 + n1 + n2);
}

// ---------------------------------------------------------------------------
// PerlinNoise锛氫綆鍏害鍒?0 鐨勫叓搴﹁〃锛屾尟骞呭叏 1锛堝師鐗?makeAmplitudes锛?// ---------------------------------------------------------------------------
McPerlinNoise::McPerlinNoise(McCnRandom& rnd, int lowOctave) {
    firstOctave = -lowOctave;
    int octaves = lowOctave + 1;  // 鍏害 lowOctave..0
    int zeroIdx = -firstOctave;
    levels.assign(octaves, nullptr);

    McImprovedNoise* zero = new McImprovedNoise(rnd);
    if (zeroIdx >= 0 && zeroIdx < octaves) levels[zeroIdx] = zero;
    else delete zero;
    for (int i = zeroIdx - 1; i >= 0; i--) {
        if (i < octaves) levels[i] = new McImprovedNoise(rnd);
        else rnd.consumeCount(262);
    }
    lowestFreqInputFactor = std::pow(2.0, -(double)zeroIdx);
    lowestFreqValueFactor = std::pow(2.0, octaves - 1) / (std::pow(2.0, octaves) - 1.0);
}

McPerlinNoise::~McPerlinNoise() {
    for (McImprovedNoise* n : levels) delete n;
}

double McPerlinNoise::getValue(double x, double y, double z, double yScale, double yFudge) const {
    double value = 0.0;
    double factor = lowestFreqInputFactor;
    double valueFactor = lowestFreqValueFactor;
    for (size_t i = 0; i < levels.size(); i++) {
        const McImprovedNoise* n = levels[i];
        if (n) {
            double v = n->noise(wrap(x * factor), wrap(y * factor), wrap(z * factor),
                                yScale * factor, yFudge * factor);
            value += v * valueFactor;
        }
        factor *= 2.0;
        valueFactor /= 2.0;
    }
    return value;
}

double McPerlinNoise::edgeValue(double noiseValue) const {
    double value = 0.0;
    double valueFactor = lowestFreqValueFactor;
    for (size_t i = 0; i < levels.size(); i++) {
        if (levels[i]) value += noiseValue * valueFactor;
        valueFactor /= 2.0;
    }
    return value;
}

// ---------------------------------------------------------------------------
// BlendedNoise锛氬師鐗?base_3d_noise锛堜富鍣０鎻掑€?min/max 涓ゅ眰锛?// ---------------------------------------------------------------------------
McBlendedNoise::McBlendedNoise(uint64_t seed, double xzScale_, double yScale,
                               double xzFactor_, double yFactor_, double smear)
    : xzMultiplier(684.412 * xzScale_), yMultiplier(684.412 * yScale),
      xzFactor(xzFactor_), yFactor(yFactor_), smearScaleMultiplier(smear) {
    // 鍘熺増鎸?min / max / main 鐨勯『搴忓叡鐢ㄥ悓涓€涓殢鏈烘簮锛岄『搴忎笉鑳芥敼
    McCnRandom rnd(seed);
    minLimit = new McPerlinNoise(rnd, 15);
    maxLimit = new McPerlinNoise(rnd, 15);
    main = new McPerlinNoise(rnd, 7);
}

McBlendedNoise::~McBlendedNoise() {
    delete minLimit;
    delete maxLimit;
    delete main;
}

double McBlendedNoise::compute(int blockX, int blockY, int blockZ) const {
    double limitX = blockX * xzMultiplier;
    double limitY = blockY * yMultiplier;
    double limitZ = blockZ * xzMultiplier;
    double mainX = limitX / xzFactor;
    double mainY = limitY / yFactor;
    double mainZ = limitZ / xzFactor;
    double limitSmear = yMultiplier * smearScaleMultiplier;
    double mainSmear = limitSmear / yFactor;

    double blendMin = 0.0, blendMax = 0.0, mainNoiseValue = 0.0;
    double pow = 1.0;
    for (int i = 0; i < 8; i++) {
        const McImprovedNoise* n = main->octave(i);
        if (n) {
            mainNoiseValue += n->noise(McPerlinNoise::wrap(mainX * pow),
                                       McPerlinNoise::wrap(mainY * pow),
                                       McPerlinNoise::wrap(mainZ * pow),
                                       mainSmear * pow, mainY * pow) / pow;
        }
        pow /= 2.0;
    }

    double factor = (mainNoiseValue / 10.0 + 1.0) / 2.0;
    bool isMax = factor >= 1.0;
    bool isMin = factor <= 0.0;
    pow = 1.0;
    for (int i = 0; i < 16; i++) {
        double wx = McPerlinNoise::wrap(limitX * pow);
        double wy = McPerlinNoise::wrap(limitY * pow);
        double wz = McPerlinNoise::wrap(limitZ * pow);
        double ys = limitSmear * pow;
        if (!isMax) {
            const McImprovedNoise* n = minLimit->octave(i);
            if (n) blendMin += n->noise(wx, wy, wz, ys, limitY * pow) / pow;
        }
        if (!isMin) {
            const McImprovedNoise* n = maxLimit->octave(i);
            if (n) blendMax += n->noise(wx, wy, wz, ys, limitY * pow) / pow;
        }
        pow /= 2.0;
    }
    return mcClampedLerp(factor, blendMin / 512.0, blendMax / 512.0) / 128.0;
}
