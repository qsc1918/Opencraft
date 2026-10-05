#pragma once
#include "blocks.hpp"
#include <cstdint>
#include <string>
#include <vector>

// 图集：贴图按"原生分辨率"排版，每个 tile 一个独立像素矩形（不再要求 16×16）。
// 这样原版高清贴图（如 end_portal 256×256）能原样上屏，后续材质包换分辨率
// （32×32 / 64×64）也不用改代码——只要 png 分辨率变了，图集自动重排。
// 每个 tile 四周留 pad（默认边长的一半，上限 32px）做边缘扩展，
// 防止 mip 采样把隔壁 tile 的颜色混进来。
struct Atlas {
    int width = 0;          // 图集总宽（像素）
    int height = 0;
    int baseTile = 16;      // 基准边长：程序化贴图按它生成
    struct Rect {
        int x = 0, y = 0, w = 0, h = 0; // 贴图本体在 atlas 中的像素矩形（不含 pad）
        int pad = 0;                    // 该贴图四周保留的边缘扩展像素
    };
    std::vector<Rect> rect; // 下标 = tile id；与着色器 tileRect[] 一一对应
    std::vector<uint8_t> rgba;
    bool built = false;

    float tileU(int tile, int corner) const; // corner 0..3 对应 (0,0),(1,0),(1,1),(0,1)
    float tileV(int tile, int corner) const;
};

// tile 数量上限（着色器 UBO 数组同样大小；方块 T_COUNT + 物品图标 + 预留）
enum { kAtlasMaxTiles = 128 };

// 从指定目录构建方块贴图图集。
// 缺失的贴图保持白色。
Atlas buildAtlas(const std::string& dir);
