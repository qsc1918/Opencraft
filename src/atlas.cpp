#include "atlas.hpp"
#include "items.hpp"
#include "png.hpp"
#include <cstring>

static const char* kTileFiles[T_COUNT] = {
    "grass_block_top.png", "grass_block_side.png", "dirt.png",          "stone.png",
    "bedrock.png",         "cobblestone.png",      "oak_planks.png",    "oak_log.png",
    "oak_log_top.png",     "oak_leaves.png",       "sand.png",          "gravel.png",
    "coal_ore.png",        "iron_ore.png",         "gold_ore.png",      "diamond_ore.png",
    "redstone_ore.png",    "water_still.png",      "snow.png",          "glass.png",
    "white_concrete.png",
};

// 物品图标贴图（assets/item/），tile 序号 = T_ITEM_BASE + 数组下标，顺序与
// items.hpp ITEM_DEFS 中 T_ITEM_BASE+n 的分配一致。
static const char* kItemFiles[] = {
    "wooden_pickaxe.png",  "wooden_axe.png",   "wooden_shovel.png",  "wooden_sword.png",
    "stone_pickaxe.png",   "stone_axe.png",    "stone_shovel.png",   "stone_sword.png",
    "iron_pickaxe.png",    "iron_axe.png",     "iron_shovel.png",    "iron_sword.png",
    "golden_pickaxe.png",  "golden_axe.png",   "golden_shovel.png",  "golden_sword.png",
    "diamond_pickaxe.png", "diamond_axe.png",  "diamond_shovel.png", "diamond_sword.png",
    "leather_helmet.png",   "leather_chestplate.png", "leather_leggings.png", "leather_boots.png",
    "iron_helmet.png",      "iron_chestplate.png",    "iron_leggings.png",    "iron_boots.png",
    "golden_helmet.png",    "golden_chestplate.png",  "golden_leggings.png",  "golden_boots.png",
    "diamond_helmet.png",   "diamond_chestplate.png", "diamond_leggings.png", "diamond_boots.png",
    "flint_and_steel.png", "ender_pearl.png",  "eye_of_ender.png",
};

const char* itemIconFile(uint8_t iconTile) {
    int i = (int)iconTile - T_ITEM_BASE;
    if (i < 0 || i >= (int)(sizeof(kItemFiles) / sizeof(kItemFiles[0]))) return nullptr;
    return kItemFiles[i];
}

// Modern Minecraft uses grayscale tintable textures for these; we bake a tint in.
struct Tint { float r, g, b; };
const Tint kTileTint[T_COUNT] = {
    {0.62f, 1.02f, 0.40f}, // grass top  -> green
    {1, 1, 1},
    {1, 1, 1},
    {1, 1, 1},
    {1, 1, 1},
    {1, 1, 1},
    {1, 1, 1},
    {1, 1, 1},
    {1, 1, 1},
    {0.34f, 0.76f, 0.26f}, // leaves -> foliage green
    {1, 1, 1},
    {1, 1, 1},
    {1, 1, 1},
    {1, 1, 1},
    {1, 1, 1},
    {1, 1, 1},
    {1, 1, 1},
    {0.40f, 0.62f, 1.10f}, // water -> blue
    {1, 1, 1},
    {1, 1, 1},
    {1, 1, 1},
};

float Atlas::tileU(int tile, int corner) const {
    int tx = tile % tilesX;
    float px = (float)(tx * cellSize + tilePad);
    switch (corner) {
        case 0: return (px + 0.5f) / width;
        case 1: return (px + tileSize - 0.5f) / width;
        case 2: return (px + tileSize - 0.5f) / width;
        default: return (px + 0.5f) / width;
    }
}
float Atlas::tileV(int tile, int corner) const {
    int ty = tile / tilesX;
    float py = (float)(ty * cellSize + tilePad);
    switch (corner) {
        case 0: return (py + 0.5f) / height;
        case 1: return (py + 0.5f) / height;
        case 2: return (py + tileSize - 0.5f) / height;
        default: return (py + tileSize - 0.5f) / height;
    }
}

Atlas buildAtlas(const std::string& dir) {
    // dir = <assets>/block；物品图标在 <assets>/item/。
    std::string assetRoot = dir;
    size_t cut = assetRoot.find_last_of("/\\");
    if (cut != std::string::npos) assetRoot = assetRoot.substr(0, cut);
    Atlas a;
    a.width = a.cellSize * a.tilesX;
    a.height = a.cellSize * a.tilesY;
    a.rgba.assign((size_t)a.width * a.height * 4, 255);

    // 把 16x16 贴图写进 32x32 cell（中心 8..23），四周做边缘扩展（clamp 采样）。
    // 这样 mipmap 每一级都不会把相邻 tile 的颜色混进来，远景渲染与图集布局解耦。
    auto blitTile = [&](int tile, const std::vector<uint8_t>& img, int w, int h) {
        int tx = tile % a.tilesX, ty = tile / a.tilesX;
        int cx0 = tx * a.cellSize, cy0 = ty * a.cellSize;
        for (int y = 0; y < a.cellSize; y++) {
            for (int x = 0; x < a.cellSize; x++) {
                int sx = x - a.tilePad, sy = y - a.tilePad;
                if (sx < 0) sx = 0;
                if (sx > a.tileSize - 1) sx = a.tileSize - 1;
                if (sy < 0) sy = 0;
                if (sy > a.tileSize - 1) sy = a.tileSize - 1;
                if (!img.empty() && w >= a.tileSize && h >= a.tileSize) {
                    size_t src = ((size_t)sy * w + sx) * 4;
                    size_t dst = ((size_t)(cy0 + y) * a.width + (cx0 + x)) * 4;
                    a.rgba[dst + 0] = img[src + 0];
                    a.rgba[dst + 1] = img[src + 1];
                    a.rgba[dst + 2] = img[src + 2];
                    a.rgba[dst + 3] = img[src + 3];
                }
                // img 缺失时保持初始 255（白）
            }
        }
    };

    for (int t = 0; t < T_COUNT; t++) {
        std::string path = dir + "\\" + kTileFiles[t];
        std::vector<uint8_t> img;
        int w = 0, h = 0;
        loadPNG(path.c_str(), img, w, h);
        const Tint& tint = kTileTint[t];
        // tint 在 blit 前应用到 img 副本
        std::vector<uint8_t> tinted;
        if (!img.empty() && w >= a.tileSize && h >= a.tileSize) {
            tinted = img;
            for (size_t i = 0; i + 3 < tinted.size(); i += 4) {
                for (int c = 0; c < 3; c++) {
                    float v = tinted[i + c] * (&tint.r)[c];
                    tinted[i + c] = (uint8_t)(v > 255.0f ? 255.0f : v);
                }
            }
        }
        blitTile(t, tinted, w, h);
    }

    // 物品图标（tile = T_ITEM_BASE + n，读 <assets>/item/*.png；缺失时留白）
    for (int n = 0; n < (int)(sizeof(kItemFiles) / sizeof(kItemFiles[0])); n++) {
        int t = T_ITEM_BASE + n;
        std::string path = assetRoot + "\\item\\" + kItemFiles[n];
        std::vector<uint8_t> img;
        int w = 0, h = 0;
        loadPNG(path.c_str(), img, w, h);
        blitTile(t, img, w, h);
    }
    a.built = true;
    return a;
}
