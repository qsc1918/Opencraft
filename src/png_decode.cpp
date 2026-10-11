// ===========================================================================
// png_decode.cpp — 自带的极简 PNG 解码器（不依赖 WIC / zlib / libpng）
//
// 为什么不用 WIC：WIC 是 Windows 独有的，为了让 Linux 也能跑就把解码器
// 换成自带的。这个实现只依赖 C++ 标准库，Windows / Linux 行为完全一致
// （原版贴图都是 8bit、非隔行的标准 PNG，足够覆盖；隔行(Adam7)不支持）。
//
// 支持：位深 1/2/4/8/16，颜色类型 0(灰) 2(RGB) 3(调色板) 4(灰+A) 6(RGBA)，
//       tRNS 透明色，DEFLATE 的 stored/fixed/dynamic 三种块。
// ===========================================================================
#include "png.hpp"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

// ---- DEFLATE (RFC 1951) --------------------------------------------------
struct BitReader {
    const uint8_t* p;
    size_t n, pos = 0;
    uint32_t bitBuf = 0;
    int bitCnt = 0;
    bool bad = false;

    int bit() {
        if (bitCnt == 0) {
            if (pos >= n) { bad = true; return 0; }
            bitBuf = p[pos++];
            bitCnt = 8;
        }
        int b = bitBuf & 1;
        bitBuf >>= 1;
        bitCnt--;
        return b;
    }
    uint32_t bits(int cnt) {
        uint32_t v = 0;
        for (int i = 0; i < cnt; i++) v |= (uint32_t)bit() << i;
        return v;
    }
    void align() { bitCnt = 0; }
};

// 规范霍夫曼表：按码长统计
struct Huff {
    int count[16] = {0};
    std::vector<int> symbol;
};

void buildHuff(Huff& h, const uint8_t* lengths, int n) {
    memset(h.count, 0, sizeof(h.count));
    for (int i = 0; i < n; i++) h.count[lengths[i]]++;
    h.count[0] = 0;
    int offs[16] = {0};
    for (int i = 1; i < 16; i++) offs[i] = offs[i - 1] + h.count[i - 1];
    h.symbol.assign((size_t)n, 0);
    for (int i = 0; i < n; i++)
        if (lengths[i]) h.symbol[(size_t)offs[lengths[i]]++] = i;
}

int decodeSym(BitReader& br, const Huff& h) {
    int code = 0, first = 0, index = 0;
    for (int len = 1; len < 16; len++) {
        code |= br.bit();
        if (br.bad) return -1;
        int cnt = h.count[len];
        if (code - first < cnt) return h.symbol[(size_t)(index + (code - first))];
        index += cnt;
        first = (first + cnt) << 1;
        code <<= 1;
    }
    return -1;
}

const int kLenBase[29] = {3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
const int kLenExtra[29] = {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
const int kDistBase[30] = {1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
const int kDistExtra[30] = {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};

bool inflate(const uint8_t* src, size_t n, std::vector<uint8_t>& out) {
    BitReader br{src, n};
    Huff fixedLit, fixedDist;
    {
        uint8_t l[288];
        for (int i = 0; i < 144; i++) l[i] = 8;
        for (int i = 144; i < 256; i++) l[i] = 9;
        for (int i = 256; i < 280; i++) l[i] = 7;
        for (int i = 280; i < 288; i++) l[i] = 8;
        buildHuff(fixedLit, l, 288);
        uint8_t d[30];
        for (int i = 0; i < 30; i++) d[i] = 5;
        buildHuff(fixedDist, d, 30);
    }
    while (true) {
        int last = br.bit();
        int type = (int)br.bits(2);
        if (br.bad) return false;
        if (type == 0) {                       // 存储块
            br.align();
            if (br.pos + 4 > br.n) return false;
            unsigned len = (unsigned)br.p[br.pos] | ((unsigned)br.p[br.pos + 1] << 8);
            br.pos += 4;
            if (br.pos + len > br.n) return false;
            out.insert(out.end(), br.p + br.pos, br.p + br.pos + len);
            br.pos += len;
        } else if (type == 1 || type == 2) {   // 固定/动态霍夫曼
            Huff litH, distH;
            const Huff* lit = &fixedLit;
            const Huff* dist = &fixedDist;
            if (type == 2) {
                int hlit = (int)br.bits(5) + 257;
                int hdist = (int)br.bits(5) + 1;
                int hclen = (int)br.bits(4) + 4;
                static const int ord[19] = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
                uint8_t clen[19] = {0};
                for (int i = 0; i < hclen; i++) clen[ord[i]] = (uint8_t)br.bits(3);
                Huff codeH;
                buildHuff(codeH, clen, 19);
                std::vector<uint8_t> lens((size_t)(hlit + hdist), 0);
                int i = 0;
                while (i < hlit + hdist) {
                    int sym = decodeSym(br, codeH);
                    if (sym < 0) return false;
                    if (sym < 16) lens[(size_t)i++] = (uint8_t)sym;
                    else if (sym == 16) {
                        if (i == 0) return false;
                        uint8_t prev = lens[(size_t)i - 1];
                        int rep = 3 + (int)br.bits(2);
                        while (rep-- && i < hlit + hdist) lens[(size_t)i++] = prev;
                    } else if (sym == 17) {
                        int rep = 3 + (int)br.bits(3);
                        while (rep-- && i < hlit + hdist) lens[(size_t)i++] = 0;
                    } else {
                        int rep = 11 + (int)br.bits(7);
                        while (rep-- && i < hlit + hdist) lens[(size_t)i++] = 0;
                    }
                }
                buildHuff(litH, lens.data(), hlit);
                buildHuff(distH, lens.data() + hlit, hdist);
                lit = &litH;
                dist = &distH;
            }
            while (true) {
                int sym = decodeSym(br, *lit);
                if (sym < 0) return false;
                if (sym < 256) {
                    out.push_back((uint8_t)sym);
                } else if (sym == 256) {
                    break;
                } else {
                    sym -= 257;
                    if (sym >= 29) return false;
                    unsigned len = (unsigned)kLenBase[sym] + br.bits(kLenExtra[sym]);
                    int ds = decodeSym(br, *dist);
                    if (ds < 0 || ds >= 30) return false;
                    unsigned d = (unsigned)kDistBase[ds] + br.bits(kDistExtra[ds]);
                    if (d > out.size()) return false;
                    size_t start = out.size() - d;
                    for (unsigned k = 0; k < len; k++) out.push_back(out[start + k]);
                }
                if (br.bad) return false;
            }
        } else {
            return false;
        }
        if (last) break;
        if (br.pos >= br.n && br.bitCnt == 0) break;
    }
    return true;
}

// ---- PNG ----------------------------------------------------------------
inline int paeth(int a, int b, int c) {
    int p = a + b - c;
    int pa = p > a ? p - a : a - p;
    int pb = p > b ? p - b : b - p;
    int pc = p > c ? p - c : c - p;
    if (pa <= pb && pa <= pc) return a;
    if (pb <= pc) return b;
    return c;
}

uint32_t readBE32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

} // 匿名命名空间

bool loadPNG(const char* path, std::vector<uint8_t>& rgba, int& w, int& h) {
    w = h = 0;
    rgba.clear();
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    std::vector<uint8_t> file;
    {
        fseek(f, 0, SEEK_END);
        long n = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (n <= 0) { fclose(f); return false; }
        file.resize((size_t)n);
        if (fread(file.data(), 1, (size_t)n, f) != (size_t)n) { fclose(f); return false; }
    }
    fclose(f);

    static const uint8_t kSig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (file.size() < 8 || memcmp(file.data(), kSig, 8) != 0) return false;

    int bitDepth = 8, colorType = 6, interlace = 0;
    uint32_t pw = 0, ph = 0;
    std::vector<uint8_t> idat, palette, trns;
    size_t p = 8;
    while (p + 8 <= file.size()) {
        uint32_t len = readBE32(&file[p]);
        const char* type = (const char*)&file[p + 4];
        p += 8;
        if (p + len + 4 > file.size()) break;
        const uint8_t* data = &file[p];
        if (!memcmp(type, "IHDR", 4) && len >= 13) {
            pw = readBE32(data);
            ph = readBE32(data + 4);
            bitDepth = data[8];
            colorType = data[9];
            interlace = data[12];
        } else if (!memcmp(type, "PLTE", 4)) {
            palette.assign(data, data + len);
        } else if (!memcmp(type, "tRNS", 4)) {
            trns.assign(data, data + len);
        } else if (!memcmp(type, "IDAT", 4)) {
            idat.insert(idat.end(), data, data + len);
        } else if (!memcmp(type, "IEND", 4)) {
            break;
        }
        p += len + 4;   // 跳过 CRC
    }
    if (!pw || !ph || idat.empty() || interlace != 0) return false;
    if (pw > 16384 || ph > 16384) return false;   // 防御异常尺寸

    int channels;
    switch (colorType) {
    case 0: channels = 1; break;
    case 2: channels = 3; break;
    case 3: channels = 1; break;
    case 4: channels = 2; break;
    case 6: channels = 4; break;
    default: return false;
    }

    // IDAT 里是 zlib 流：2 字节头（CMF/FLG）+ DEFLATE 数据 + 4 字节 Adler-32
    std::vector<uint8_t> raw;
    raw.reserve((size_t)ph * ((size_t)pw * channels * bitDepth + 7) / 8 + 16);
    if (idat.size() < 3) return false;
    if (((idat[0] & 0x0F) != 8) || (((idat[0] << 8) | idat[1]) % 31 != 0)) return false;  // 必须是 deflate
    if (idat[1] & 0x20) return false;   // 带预置字典的 zlib 流不支持（原版贴图没有）
    if (!inflate(idat.data() + 2, idat.size() - 2, raw)) return false;

    // 反滤波（PNG 每行前面一个 filter 字节）
    const size_t bpp = (size_t)((channels * bitDepth + 7) / 8);   // 每像素字节数（至少 1）
    const size_t stride = ((size_t)pw * channels * bitDepth + 7) / 8;
    if (raw.size() < (stride + 1) * ph) return false;
    std::vector<uint8_t> img(stride * ph);
    const uint8_t* src = raw.data();
    for (uint32_t y = 0; y < ph; y++) {
        uint8_t filter = *src++;
        uint8_t* cur = &img[stride * y];
        const uint8_t* prev = y ? &img[stride * (y - 1)] : nullptr;
        for (size_t i = 0; i < stride; i++) {
            int a = (i >= bpp) ? cur[i - bpp] : 0;
            int b = prev ? prev[i] : 0;
            int c = (prev && i >= bpp) ? prev[i - bpp] : 0;
            int v = src[i];
            switch (filter) {
            case 0: break;
            case 1: v += a; break;
            case 2: v += b; break;
            case 3: v += (a + b) / 2; break;
            case 4: v += paeth(a, b, c); break;
            default: return false;
            }
            cur[i] = (uint8_t)(v & 255);
        }
        src += stride;
    }

    // 展开成 RGBA8
    rgba.assign((size_t)pw * ph * 4, 255);
    for (uint32_t y = 0; y < ph; y++) {
        const uint8_t* row = &img[stride * y];
        for (uint32_t x = 0; x < pw; x++) {
            uint8_t* dst = &rgba[((size_t)y * pw + x) * 4];
            // 取第 idx 个样本（0..channels-1），支持 1/2/4/8/16 位深。
            // scaleTo8：位深小于 8 时把灰度值归一化到 0..255；
            // 调色板索引不能归一化（那就不是索引了），所以由调用方决定。
            const bool scaleTo8 = (colorType != 3);
            auto sample = [&](int idx) -> int {
                if (bitDepth == 8) return row[(size_t)x * channels + idx];
                if (bitDepth == 16) return row[((size_t)x * channels + idx) * 2];   // 取高字节
                int bitsPer = bitDepth;
                size_t bitPos = ((size_t)x * channels + idx) * bitsPer;
                int byte = (int)(bitPos / 8);
                int shift = 8 - bitsPer - (int)(bitPos % 8);
                int mask = (1 << bitsPer) - 1;
                int v = (row[byte] >> shift) & mask;
                return scaleTo8 ? v * 255 / mask : v;
            };
            switch (colorType) {
            case 0: {   // 灰度
                int g = sample(0);
                dst[0] = dst[1] = dst[2] = (uint8_t)g;
                dst[3] = 255;
                if (!trns.empty() && trns.size() >= 2) {
                    int key = ((int)trns[0] << 8) | trns[1];
                    if (bitDepth == 16 ? g == (key >> 8) : g == key) dst[3] = 0;
                }
                break;
            }
            case 2:     // RGB
                dst[0] = (uint8_t)sample(0);
                dst[1] = (uint8_t)sample(1);
                dst[2] = (uint8_t)sample(2);
                dst[3] = 255;
                if (trns.size() >= 6) {
                    int kr = ((int)trns[0] << 8) | trns[1];
                    int kg = ((int)trns[2] << 8) | trns[3];
                    int kb = ((int)trns[4] << 8) | trns[5];
                    if (dst[0] == (uint8_t)(kr >> 8) && dst[1] == (uint8_t)(kg >> 8) &&
                        dst[2] == (uint8_t)(kb >> 8))
                        dst[3] = 0;
                }
                break;
            case 3: {   // 调色板
                int idx = sample(0);
                if ((size_t)(idx * 3 + 2) < palette.size()) {
                    dst[0] = palette[(size_t)idx * 3 + 0];
                    dst[1] = palette[(size_t)idx * 3 + 1];
                    dst[2] = palette[(size_t)idx * 3 + 2];
                }
                dst[3] = (idx >= 0 && (size_t)idx < trns.size()) ? trns[(size_t)idx] : 255;
                break;
            }
            case 4:     // 灰度 + A
                dst[0] = dst[1] = dst[2] = (uint8_t)sample(0);
                dst[3] = (uint8_t)sample(1);
                break;
            default:    // 6: RGBA
                dst[0] = (uint8_t)sample(0);
                dst[1] = (uint8_t)sample(1);
                dst[2] = (uint8_t)sample(2);
                dst[3] = (uint8_t)sample(3);
                break;
            }
        }
    }
    w = (int)pw;
    h = (int)ph;
    return true;
}
