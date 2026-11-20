#include "font.hpp"
#include "png.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>

namespace {

std::vector<uint32_t> DecodeUtf8(const std::string& text) {
    std::vector<uint32_t> out;
    size_t i = 0;
    while (i < text.size()) {
        uint8_t c = (uint8_t)text[i];
        uint32_t cp = c;
        int extra = 0;
        if (c >= 0xF0) { cp = c & 0x07; extra = 3; }
        else if (c >= 0xE0) { cp = c & 0x0F; extra = 2; }
        else if (c >= 0xC0) { cp = c & 0x1F; extra = 1; }
        for (int k = 0; k < extra && i + 1 < text.size(); k++) {
            cp = (cp << 6) | ((uint8_t)text[++i] & 0x3F);
        }
        out.push_back(cp);
        i++;
    }
    return out;
}

uint16_t ReadU16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
uint32_t ReadU32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

void BlendPixel(std::vector<uint8_t>& canvas, int w, int h, int x, int y,
                uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (x < 0 || y < 0 || x >= w || y >= h || a == 0) return;
    uint8_t* dst = canvas.data() + ((size_t)y * w + x) * 4;
    if (a == 255) {
        dst[0] = r; dst[1] = g; dst[2] = b; dst[3] = 255;
        return;
    }
    int inv = 255 - a;
    dst[0] = (uint8_t)((dst[0] * inv + r * a) / 255);
    dst[1] = (uint8_t)((dst[1] * inv + g * a) / 255);
    dst[2] = (uint8_t)((dst[2] * inv + b * a) / 255);
    dst[3] = (uint8_t)std::min(255, dst[3] + a);
}

} // namespace

bool Font::Load(const std::string& pngPath, const std::string& fntPath) {
    std::vector<uint8_t> pngRGBA;
    int w = 0, h = 0;
    if (!loadPNG(pngPath.c_str(), pngRGBA, w, h)) {
        fprintf(stderr, "[font] PNG 读取失败: %s\n", pngPath.c_str());
        return false;
    }
    std::ifstream file(fntPath, std::ios::binary);
    if (!file) {
        fprintf(stderr, "[font] 码点表打开失败: %s\n", fntPath.c_str());
        return false;
    }
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    const size_t kHeaderSize = 30;
    const size_t kGlyphSize = 18;
    if (data.size() < kHeaderSize || ReadU32(data.data()) != 0x464E544F) return false;
    if (ReadU32(data.data() + 4) != 1) return false;
    atlasW_ = (int)ReadU32(data.data() + 8);
    atlasH_ = (int)ReadU32(data.data() + 12);
    ascent_ = ReadU16(data.data() + 20);
    descent_ = ReadU16(data.data() + 22);
    lineHeight_ = ReadU16(data.data() + 24);
    uint32_t count = ReadU32(data.data() + 26);
    if (data.size() < kHeaderSize + (size_t)count * kGlyphSize) return false;
    glyphs_.clear();
    for (uint32_t i = 0; i < count; i++) {
        const uint8_t* p = data.data() + kHeaderSize + (size_t)i * kGlyphSize;
        Glyph g;
        g.codepoint = ReadU32(p);
        g.x = ReadU16(p + 4);
        g.y = ReadU16(p + 6);
        g.w = ReadU16(p + 8);
        g.h = ReadU16(p + 10);
        g.bearingX = (int16_t)ReadU16(p + 12);
        g.bearingY = (int16_t)ReadU16(p + 14);
        g.advance = ReadU16(p + 16);
        glyphs_[g.codepoint] = g;
    }
    atlas_ = std::move(pngRGBA);
    return true;
}

float Font::Width(const std::string& utf8, int scale) const {
    float x = 0.0f;
    for (uint32_t cp : DecodeUtf8(utf8)) {
        if (cp == ' ') { x += 3 * scale; continue; }
        auto it = glyphs_.find(cp);
        if (it == glyphs_.end()) it = glyphs_.find('?');
        if (it != glyphs_.end()) x += it->second.advance * scale;
    }
    return x;
}

void Font::DrawString(std::vector<uint8_t>& canvas, int canvasW, int canvasH,
                    const std::string& utf8, int x, int y, uint32_t argb,
                    int scale, bool shadow) const {
    if (!IsLoaded() || scale < 1) return;
    auto paint = [&](int ox, int oy, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
        int penX = ox;
        for (uint32_t cp : DecodeUtf8(utf8)) {
            if (cp == ' ') { penX += 3 * scale; continue; }
            auto it = glyphs_.find(cp);
            if (it == glyphs_.end()) it = glyphs_.find('?');
            if (it != glyphs_.end()) {
                const Glyph& gl = it->second;
                for (int row = 0; row < gl.h; row++) {
                    for (int col = 0; col < gl.w; col++) {
                        size_t src = ((size_t)(gl.y + row) * atlasW_ + gl.x + col) * 4 + 3;
                        uint8_t alpha = atlas_[src];
                        if (alpha) {
                            // 放大要画满 scale×scale 的实心块；只按间距摆放会留下空隙，
                            // 文字看起来发灰、笔画粘在一起。
                            for (int sy = 0; sy < scale; sy++) {
                                for (int sx = 0; sx < scale; sx++) {
                                    BlendPixel(canvas, canvasW, canvasH,
                                               penX + (gl.bearingX + col) * scale + sx,
                                               oy + (gl.bearingY + row) * scale + sy,
                                               r, g, b, (uint8_t)((alpha * a) / 255));
                                }
                            }
                        }
                    }
                }
                penX += gl.advance * scale;
            }
        }
    };
    uint8_t a = (uint8_t)(argb >> 24);
    uint8_t r = (uint8_t)(argb >> 16), g = (uint8_t)(argb >> 8), b = (uint8_t)argb;
    // 阴影只偏 1 个屏幕像素且很淡：否则会盖住 12px 点阵的笔画缝隙，字看着发灰。
    if (shadow) paint(x + 1, y + 1, r / 3, g / 3, b / 3, (uint8_t)(a / 3));
    paint(x, y, r, g, b, a);
}
