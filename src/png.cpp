// ===========================================================================
// png.cpp — PNG 编码（截图用）
//
// 原来用的是 WIC（Windows 独有），为了 Linux 也能跑改成自己写：
// IHDR/IDAT/IEND + DEFLATE 存储块（不压缩）。文件会大一点，但到处能跑。
// 解码在 png_decode.cpp。
// ===========================================================================
#include "png.hpp"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

uint32_t crcEntry(int i) {   // 位级 CRC32（PNG 用）
    uint32_t c = (uint32_t)i;
    for (int k = 0; k < 8; k++) c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
    return c;
}

uint32_t crc32of(const uint8_t* buf, size_t n, uint32_t crc = 0xFFFFFFFFu) {
    static uint32_t table[256];
    static bool init = false;
    if (!init) {
        for (int i = 0; i < 256; i++) table[i] = crcEntry(i);
        init = true;
    }
    for (size_t i = 0; i < n; i++) crc = table[(crc ^ buf[i]) & 0xFF] ^ (crc >> 8);
    return crc;
}

void putBE32(std::vector<uint8_t>& v, uint32_t x) {
    v.push_back((uint8_t)(x >> 24));
    v.push_back((uint8_t)(x >> 16));
    v.push_back((uint8_t)(x >> 8));
    v.push_back((uint8_t)x);
}

void writeChunk(std::vector<uint8_t>& out, const char* type, const std::vector<uint8_t>& data) {
    putBE32(out, (uint32_t)data.size());
    size_t crcStart = out.size();
    out.insert(out.end(), type, type + 4);
    out.insert(out.end(), data.begin(), data.end());
    uint32_t crc = crc32of(&out[crcStart], out.size() - crcStart) ^ 0xFFFFFFFFu;
    putBE32(out, crc);
}

// zlib 包装：0x78 0x01（最快/不压缩）+ DEFLATE 存储块
std::vector<uint8_t> zlibStore(const std::vector<uint8_t>& raw) {
    std::vector<uint8_t> z;
    z.push_back(0x78);
    z.push_back(0x01);
    size_t pos = 0;
    if (raw.empty()) {
        z.push_back(0x01);
        z.push_back(0x00);
        z.push_back(0x00);
        z.push_back(0xFF);
        z.push_back(0xFF);
    }
    while (pos < raw.size()) {
        size_t chunk = raw.size() - pos;
        if (chunk > 65535) chunk = 65535;
        bool last = (pos + chunk >= raw.size());
        z.push_back(last ? 1 : 0);
        z.push_back((uint8_t)(chunk & 255));
        z.push_back((uint8_t)(chunk >> 8));
        z.push_back((uint8_t)(~chunk & 255));
        z.push_back((uint8_t)((~chunk >> 8) & 255));
        z.insert(z.end(), raw.begin() + (long)pos, raw.begin() + (long)(pos + chunk));
        pos += chunk;
    }
    uint32_t a = 1, b = 0;   // Adler-32
    for (uint8_t c : raw) {
        a = (a + c) % 65521;
        b = (b + a) % 65521;
    }
    putBE32(z, (b << 16) | a);
    return z;
}

} // 匿名命名空间

bool savePNG(const std::string& path, int w, int h, const std::vector<uint8_t>& bgra) {
    if (w <= 0 || h <= 0) return false;
    if (bgra.size() < (size_t)w * h * 4) return false;

    std::vector<uint8_t> raw;
    raw.reserve((size_t)h * ((size_t)w * 4 + 1));
    for (int y = 0; y < h; y++) {
        raw.push_back(0);   // filter = None
        const uint8_t* row = &bgra[(size_t)y * w * 4];
        for (int x = 0; x < w; x++) {
            raw.push_back(row[x * 4 + 2]);   // R
            raw.push_back(row[x * 4 + 1]);   // G
            raw.push_back(row[x * 4 + 0]);   // B
            raw.push_back(row[x * 4 + 3]);   // A
        }
    }

    std::vector<uint8_t> out = {137, 80, 78, 71, 13, 10, 26, 10};
    std::vector<uint8_t> ihdr;
    putBE32(ihdr, (uint32_t)w);
    putBE32(ihdr, (uint32_t)h);
    ihdr.push_back(8);   // bit depth
    ihdr.push_back(6);   // color type RGBA
    ihdr.push_back(0);   // compression
    ihdr.push_back(0);   // filter
    ihdr.push_back(0);   // interlace
    writeChunk(out, "IHDR", ihdr);
    writeChunk(out, "IDAT", zlibStore(raw));
    writeChunk(out, "IEND", {});

    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;
    size_t n = fwrite(out.data(), 1, out.size(), f);
    fclose(f);
    return n == out.size();
}
