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
    nullptr, // T_END_CRYSTAL — 程序化
    "netherrack.png",      "soul_sand.png",        "glowstone.png",     "nether_brick.png",
    "obsidian.png",        "lava_still.png",       "nether_portal.png",
    "end_stone.png",       "end_portal_frame.png", nullptr,            nullptr,
    "dragon_egg.png",      "end_portal_frame_eye.png",
    "end_portal_frame_side.png",
    nullptr, // T_FIRE — 程序化
};

// 物品图标贴图（assets/item/），tile = T_ITEM_BASE + 下标，
// 顺序须与 items.hpp ITEM_DEFS 中 T_ITEM_BASE+n 的分配一致。
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
    "flint_and_steel.png", "ender_pearl.png",  "ender_eye.png",
};

const char* itemIconFile(uint8_t iconTile) {
    int i = (int)iconTile - T_ITEM_BASE;
    if (i < 0 || i >= (int)(sizeof(kItemFiles) / sizeof(kItemFiles[0]))) return nullptr;
    return kItemFiles[i];
}

// 原版这些贴图是可着色的灰度图，这里把色调直接烘焙进图集。
struct Tint { float r, g, b; };
const Tint kTileTint[T_COUNT] = {
    {0.62f, 1.02f, 0.40f}, // 草顶 → 绿色
    {1, 1, 1},              // 草侧面
    {1, 1, 1},              // 泥土
    {1, 1, 1},              // 石头
    {1, 1, 1},              // 基岩
    {1, 1, 1},              // 圆石
    {1, 1, 1},              // 木板
    {1, 1, 1},              // 原木侧面
    {1, 1, 1},              // 原木顶面
    {0.34f, 0.76f, 0.26f}, // 树叶 → 绿色
    {1, 1, 1},              // 沙子
    {1, 1, 1},              // 沙砾
    {1, 1, 1},              // 煤矿
    {1, 1, 1},              // 铁矿
    {1, 1, 1},              // 金矿
    {1, 1, 1},              // 钻石矿
    {1, 1, 1},              // 红石矿
    {0.40f, 0.62f, 1.10f}, // 水 → 蓝色
    {1, 1, 1},              // 雪
    {1, 1, 1},              // 玻璃
    {1, 1, 1},              // 白色
    {1, 1, 1},              // T_END_CRYSTAL（程序化）
    {1, 1, 1},              // 下界岩
    {0.85f, 0.75f, 0.55f}, // 灵魂沙 → 棕色调
    {1.2f, 1.15f, 0.7f},   // 萤石 → 暖亮色
    {0.7f, 0.5f, 0.45f},   // 下界砖 → 暗红棕
    {0.75f, 0.65f, 0.9f},  // 黑曜石 → 紫黑色
    {1.1f, 0.7f, 0.2f},    // 岩浆 → 橙红色
    {0.8f, 0.5f, 1.0f},    // 下界传送门 → 紫色
    {1.1f, 1.05f, 0.7f},   // 末地石 → 淡黄色
    {0.6f, 0.8f, 0.5f},    // 末地传送门框架 → 偏绿
    {0.3f, 0.2f, 0.5f},    // 末地传送门 → 暗紫
    {0.4f, 0.3f, 0.6f},    // 折跃门 → 暗紫灰
    {0.55f, 0.45f, 0.65f}, // 龙蛋 → 暗紫斑点
    {0.8f, 1.0f, 0.3f},    // 末地传送门框架眼 → 亮绿
    {0.6f, 0.8f, 0.5f},    // 末地传送门框架侧面 → 偏绿
    {1, 1, 1},             // 火（程序化，自带颜色）
};

// ---------------------------------------------------------------------------
// 程序化 tile：为没有 png 的 tile 代码生成 16x16 纹理（非 MC 素材）。
// extract_assets 提取的同名 png 会覆盖它；这些 tile 目前没有 png，始终走程序化。
// ---------------------------------------------------------------------------
#include "util.hpp" // 需要 hash32
static void fillProceduralTile(int t, Atlas& a) {
    auto put = [&](int tile, int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t al) {
        int tx = tile % a.tilesX, ty = tile / a.tilesX;
        size_t dst = ((size_t)(ty * a.cellSize + a.tilePad + y) * a.width +
                      (tx * a.cellSize + a.tilePad + x)) * 4;
        a.rgba[dst + 0] = r; a.rgba[dst + 1] = g;
        a.rgba[dst + 2] = b; a.rgba[dst + 3] = al;
    };
    // 辅助：确定性哈希映射到 [0..255]
    auto hval = [](int x, int y, int salt) -> uint8_t {
        uint32_t h = hash32((uint32_t)((uint32_t)x * 374761393u + (uint32_t)y * 668265263u + (uint32_t)salt * 2654435761u));
        return (uint8_t)(h & 255);
    };
    switch (t) {
    case T_END_CRYSTAL: {
        // 紫水晶底 + 亮粉高光碎纹
        for (int y = 0; y < 16; y++)
            for (int x = 0; x < 16; x++) {
                uint32_t n = hash32((uint32_t)(x * 37 + y * 91 + 13));
                uint8_t v = (uint8_t)(150 + (n & 31));
                uint8_t r = (uint8_t)(v + 60), g = (uint8_t)(v * 2 / 5), b = (uint8_t)(v + 90);
                if (((x * 7 + y * 5) % 11) == 0) { r = 255; g = 170; b = 240; } // 高光纹
                put(t, x, y, r, g, b, 255);
            }
        break;
    }
    case T_NETHERRACK: {
        // 暗红岩石 + 噪点纹理
        for (int y = 0; y < 16; y++)
            for (int x = 0; x < 16; x++) {
                uint8_t n = hval(x, y, 0x100);
                uint8_t r = (uint8_t)(140 + (n & 31));
                uint8_t g = (uint8_t)(40 + ((n >> 4) & 15));
                uint8_t b = (uint8_t)(30 + ((n >> 2) & 7));
                // 细裂纹
                if ((x + y * 3) % 13 == 0 || (x * 5 + y) % 17 == 0) {
                    r -= 30; g -= 10; b -= 5;
                }
                put(t, x, y, r, g, b, 255);
            }
        break;
    }
    case T_SOUL_SAND: {
        // 棕色沙 + 暗面纹（原版脸纹简化为随机纹理）
        for (int y = 0; y < 16; y++)
            for (int x = 0; x < 16; x++) {
                uint8_t n = hval(x, y, 0x200);
                uint8_t r = (uint8_t)(110 + (n & 31));
                uint8_t g = (uint8_t)(85 + ((n >> 3) & 15));
                uint8_t b = (uint8_t)(50 + ((n >> 5) & 7));
                // 脸纹简化：4×4 暗区
                int lx = x % 8, ly = y % 8;
                if ((lx >= 1 && lx <= 2 && ly >= 1 && ly <= 2) ||
                    (lx >= 5 && lx <= 6 && ly >= 1 && ly <= 2) ||
                    (lx >= 2 && lx <= 5 && ly >= 4 && ly <= 5)) {
                    r -= 25; g -= 15; b -= 8;
                }
                put(t, x, y, r, g, b, 255);
            }
        break;
    }
    case T_GLOWSTONE: {
        // 明亮黄橙色 + 高光碎块
        for (int y = 0; y < 16; y++)
            for (int x = 0; x < 16; x++) {
                uint8_t n = hval(x, y, 0x300);
                uint8_t r = (uint8_t)(210 + (n & 31));
                uint8_t g = (uint8_t)(185 + ((n >> 2) & 31));
                uint8_t b = (uint8_t)(60 + ((n >> 4) & 15));
                // 明暗块
                if (((x * 3 + y * 7) % 9) < 3) { r += 20; g += 15; }
                put(t, x, y, r, g, b, 255);
            }
        break;
    }
    case T_NETHER_BRICK: {
        // 暗红砖 + 灰浆缝
        for (int y = 0; y < 16; y++)
            for (int x = 0; x < 16; x++) {
                uint8_t n = hval(x, y, 0x400);
                // 砖块布局：每 8 高一行，交错偏移 4
                int row = y / 4;
                int off = (row & 1) ? 4 : 0;
                bool mortar = ((x + off) % 8 == 0) || (y % 4 == 0);
                if (mortar) {
                    put(t, x, y, 50, 30, 30, 255);
                } else {
                    uint8_t r = (uint8_t)(100 + (n & 15));
                    uint8_t g = (uint8_t)(35 + ((n >> 3) & 7));
                    uint8_t b = (uint8_t)(30 + ((n >> 5) & 7));
                    put(t, x, y, r, g, b, 255);
                }
            }
        break;
    }
    case T_OBSIDIAN: {
        // 深紫黑色 + 微闪
        for (int y = 0; y < 16; y++)
            for (int x = 0; x < 16; x++) {
                uint8_t n = hval(x, y, 0x500);
                uint8_t v = (uint8_t)(20 + (n & 15));
                uint8_t r = (uint8_t)(v + ((n >> 4) & 3));
                uint8_t g = (uint8_t)(v);
                uint8_t b = (uint8_t)(v + 8 + ((n >> 6) & 7));
                put(t, x, y, r, g, b, 255);
            }
        break;
    }
    case T_LAVA: {
        // 岩浆：橙红底 + 暗红纹（静态帧，省略原版帧动画）
        for (int y = 0; y < 16; y++)
            for (int x = 0; x < 16; x++) {
                uint8_t n = hval(x, y, 0x600);
                uint8_t n2 = hval(x * 3, y * 5, 0x610);
                uint8_t r = (uint8_t)(200 + (n2 & 31));
                uint8_t g = (uint8_t)(80 + (n & 31) + ((n2 >> 4) & 15));
                uint8_t b = (uint8_t)(10 + ((n >> 5) & 7));
                // 暗纹带
                if (((x + y * 2) % 7 == 0) && (n2 & 3) == 0) {
                    r -= 60; g -= 30;
                }
                put(t, x, y, r, g, b, 255);
            }
        break;
    }
    case T_NETHER_PORTAL: {
        // 紫色漩涡纹理
        for (int y = 0; y < 16; y++)
            for (int x = 0; x < 16; x++) {
                uint8_t n = hval(x, y, 0x700);
                float cx = x - 7.5f, cy = y - 7.5f;
                float r2 = cx * cx + cy * cy;
                float wave = sinf(r2 * 0.3f + n * 0.02f) * 0.5f + 0.5f;
                uint8_t r = (uint8_t)(60 + wave * 100 + ((n >> 4) & 31));
                uint8_t g = (uint8_t)(10 + wave * 30);
                uint8_t b = (uint8_t)(120 + wave * 100 + ((n >> 2) & 31));
                put(t, x, y, r, g, b, 255);
            }
        break;
    }
    case T_END_STONE: {
        // 浅黄灰色石头
        for (int y = 0; y < 16; y++)
            for (int x = 0; x < 16; x++) {
                uint8_t n = hval(x, y, 0x800);
                uint8_t r = (uint8_t)(210 + (n & 15));
                uint8_t g = (uint8_t)(200 + ((n >> 2) & 15));
                uint8_t b = (uint8_t)(170 + ((n >> 4) & 15));
                put(t, x, y, r, g, b, 255);
            }
        break;
    }
    case T_END_PORTAL_FRAME: {
        // 绿灰石框 + 顶部凹槽
        for (int y = 0; y < 16; y++)
            for (int x = 0; x < 16; x++) {
                uint8_t n = hval(x, y, 0x900);
                uint8_t r = (uint8_t)(70 + (n & 15));
                uint8_t g = (uint8_t)(90 + ((n >> 2) & 15));
                uint8_t b = (uint8_t)(55 + ((n >> 4) & 7));
                put(t, x, y, r, g, b, 255);
            }
        break;
    }
    case T_END_PORTAL: {
        // 纯黑底 + 小白点星
        for (int y = 0; y < 16; y++)
            for (int x = 0; x < 16; x++) {
                uint8_t n = hval(x, y, 0xA00);
                bool star = (n < 12); // ~5% 白星点
                if (star) put(t, x, y, 255, 255, 255, 255);
                else      put(t, x, y, 0, 0, 8, 255);
            }
        break;
    }
    case T_END_GATEWAY: {
        // 深紫黑 + 星点（与 end_portal 类似但稍亮）
        for (int y = 0; y < 16; y++)
            for (int x = 0; x < 16; x++) {
                uint8_t n = hval(x, y, 0xB00);
                bool star = (n < 15);
                if (star) put(t, x, y, 200, 200, 255, 255);
                else      put(t, x, y, 10, 5, 20, 255);
            }
        break;
    }
    case T_DRAGON_EGG: {
        // 深紫黑 + 明斑点
        for (int y = 0; y < 16; y++)
            for (int x = 0; x < 16; x++) {
                uint8_t n = hval(x, y, 0xC00);
                uint8_t v = (uint8_t)(15 + (n & 7));
                uint8_t r = (uint8_t)(v + 5);
                uint8_t g = (uint8_t)(v);
                uint8_t b = (uint8_t)(v + 10);
                // 亮斑点
                if ((n & 7) == 0) { r += 40; b += 50; }
                put(t, x, y, r, g, b, 255);
            }
        break;
    }
    case T_FIRE: {
        // 火苗：下宽上窄，边缘透明（配合片元着色器的 alpha 剔除）
        for (int y = 0; y < 16; y++)
            for (int x = 0; x < 16; x++) {
                uint8_t n = hval(x, y, 0xD00);
                int tUp = 15 - y;               // 底部 15，顶部 0
                int width = 2 + tUp / 4;
                int dx = x > 7 ? x - 7 : 7 - x;
                if (dx > width + (n & 3)) { put(t, x, y, 0, 0, 0, 0); continue; }
                uint8_t r = (uint8_t)(215 + (n & 39));
                uint8_t g = (uint8_t)(110 + (n & 63));
                uint8_t b = (uint8_t)(15 + ((n >> 5) & 15));
                if (dx <= 1) { g = (uint8_t)(g + 70); b = (uint8_t)(b + 50); } // 焰心更亮
                put(t, x, y, r, g, b, 255);
            }
        break;
    }
    default: break; // 其他 tile 留白（255）
    }
}

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
        if (!kTileFiles[t]) { fillProceduralTile(t, a); continue; }
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
