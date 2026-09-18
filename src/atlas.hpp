#pragma once
#include "blocks.hpp"
#include <cstdint>
#include <string>
#include <vector>

struct Atlas {
    int width = 0;          // 图集总宽（像素）
    int height = 0;
    int tileSize = 16;      // 每格贴图像素
    int cellSize = 32;      // 每格占位 = 贴图 + 8px 边缘扩展
    int tilePad = 8;        // 贴图在格内的起点偏移
    int tilesX = 16;        // 横向格数（16x16 = 256 格：方块 + 物品 + 预留）
    int tilesY = 16;
    std::vector<uint8_t> rgba; // 紧凑排列的 RGBA
    bool built = false;

    float tileU(int tile, int corner) const; // corner 0..3 对应 (0,0),(1,0),(1,1),(0,1)
    float tileV(int tile, int corner) const;
};

// 从指定目录构建方块贴图图集。
// 缺失的格子保持白色。
Atlas buildAtlas(const std::string& dir);
