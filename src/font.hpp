#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

// 离线烘焙的像素字体：只有字形度量与 RGBA 图集，不依赖任何系统字体服务。
// 菜单把它直接画进 CPU 画布；以后游戏内文字也能用同一份数据生成四边形。
class Font {
public:
    bool Load(const std::string& pngPath, const std::string& fntPath);

    bool IsLoaded() const { return !glyphs_.empty(); }
    int LineHeight(int scale) const { return lineHeight_ * scale; }
    float Width(const std::string& utf8, int scale) const;

    // 把文字画进自上而下的 RGBA 画布。x/y 是首行左上角，argb 为显示空间颜色。
    // 阴影偏移 1 个屏幕像素，RGB 与 alpha 都乘 1/3，避免盖住点阵笔画缝隙。
    void DrawString(std::vector<uint8_t>& canvas, int canvasW, int canvasH,
                  const std::string& utf8, int x, int y, uint32_t argb,
                  int scale, bool shadow = true) const;

private:
    struct Glyph {
        uint32_t codepoint = 0;
        uint16_t x = 0, y = 0, w = 0, h = 0;
        int16_t bearingX = 0, bearingY = 0;
        uint16_t advance = 0;
    };

    std::unordered_map<uint32_t, Glyph> glyphs_;
    std::vector<uint8_t> atlas_;
    int atlasW_ = 0, atlasH_ = 0;
    int ascent_ = 0, descent_ = 0, lineHeight_ = 0;
};
