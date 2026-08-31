#pragma once
#include "blocks.hpp"
#include <cstdint>
#include <string>
#include <vector>

struct Atlas {
    int width = 0;          // total atlas pixel width
    int height = 0;
    int tileSize = 16;      // per-tile texture pixels
    int cellSize = 32;      // atlas cell per tile = tile + 8px edge-extension border
    int tilePad = 8;        // texture origin inside the cell
    int tilesX = 16;        // tiles across (16x16 cells = 256 tiles: blocks + items + 预留)
    int tilesY = 16;
    std::vector<uint8_t> rgba; // tightly packed RGBA
    bool built = false;

    float tileU(int tile, int corner) const; // corner 0..3: (0,0),(1,0),(1,1),(0,1)
    float tileV(int tile, int corner) const;
};

// Builds the block-texture atlas from the given directory.
// Missing tiles fall back to white.
Atlas buildAtlas(const std::string& dir);
