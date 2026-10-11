#include "atlas.hpp"
#include "items.hpp"
#include "png.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
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
    "end_stone.png",       "end_portal_frame.png", "end_portal.png",   nullptr,
    "dragon_egg.png",      "end_portal_frame_eye.png",
    "end_portal_frame_side.png",
    "fire_0.png",          // 原版火焰（16x512 动画条，取首帧）
    "chorus_plant.png",    "chorus_flower.png",    "chorus_flower_dead.png",
    "iron_bars.png",       "torch.png",
    // 实体贴图不在这里：走下面的 kEntityFiles（另一个目录 + 保留原始比例）
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
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

// 实体贴图（assets/entity/），下标 = tile - T_ENTITY_PIG。
// 顺序必须与 blocks.hpp 里 T_ENTITY_* 的枚举顺序一致。
static const char* kEntityFiles[] = {
    "pig.png", "cow.png", "sheep.png",   "chicken.png", "zombie.png",   "skeleton.png",
    "creeper.png", "spider.png", "blaze.png", "enderman.png", "steve.png", "arrow.png",
    "sheep_wool.png",
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
    {1, 1, 1},             // 末地传送门 → 原版贴图，不再调色
    {0.4f, 0.3f, 0.6f},    // 折跃门 → 暗紫灰
    {0.55f, 0.45f, 0.65f}, // 龙蛋 → 暗紫斑点
    {0.8f, 1.0f, 0.3f},    // 末地传送门框架眼 → 亮绿
    {0.6f, 0.8f, 0.5f},    // 末地传送门框架侧面 → 偏绿
    {1, 1, 1},             // 火（原版贴图，自带颜色）
    {1, 1, 1},             // 紫颂植株
    {1, 1, 1},             // 紫颂花
    {1, 1, 1},             // 枯萎的紫颂花
    {1, 1, 1},             // 铁栏杆
    {1, 1, 1},             // 火把
    // 实体贴图：原版皮肤自带颜色，一律不着色
    {1,1,1},{1,1,1},{1,1,1},{1,1,1},{1,1,1},{1,1,1},
    {1,1,1},{1,1,1},{1,1,1},{1,1,1},{1,1,1},{1,1,1},
    {1,1,1},   // 羊毛层
};

// ---------------------------------------------------------------------------
// 程序化 tile：为没有 png 的 tile 代码生成 16x16 纹理（非 MC 素材）。
// extract_assets 提取的同名 png 会覆盖它；这些 tile 目前没有 png，始终走程序化。
// ---------------------------------------------------------------------------
#include "util.hpp" // 需要 hash32
// 程序化 tile 一律先在 16×16 缓冲里画，最后按该 tile 的实际分辨率缩放写进图集
// （材质包换成 32×32 时程序化贴图跟着放大，不会显得糊一块）。
static void fillProceduralTile(int t, Atlas& a) {
    std::vector<uint8_t> buf((size_t)16 * 16 * 4, 255);
    auto put = [&](int tile, int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t al) {
        (void)tile;
        size_t dst = ((size_t)y * 16 + x) * 4;
        buf[dst + 0] = r; buf[dst + 1] = g;
        buf[dst + 2] = b; buf[dst + 3] = al;
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
        // 原版贴图缺失时的兜底：纯黑底 + 小白点星
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
    default: break; // 其他 tile 留白（255）
    }
    // 把 16x16 缓冲按该 tile 的实际分辨率缩放写进图集（含边缘扩展）
    {
        const Atlas::Rect& r = a.rect[t];
        if (r.w <= 0 || r.h <= 0) return;
        for (int dy = -r.pad; dy < r.h + r.pad; dy++) {
            for (int dx = -r.pad; dx < r.w + r.pad; dx++) {
                int sx = (int)((int64_t)dx * 16 / r.w);
                int sy = (int)((int64_t)dy * 16 / r.h);
                if (sx < 0) sx = 0; if (sx > 15) sx = 15;
                if (sy < 0) sy = 0; if (sy > 15) sy = 15;
                size_t s = ((size_t)sy * 16 + sx) * 4;
                int px = r.x + dx, py = r.y + dy;
                if (px < 0 || px >= a.width || py < 0 || py >= a.height) continue;
                size_t d = ((size_t)py * a.width + px) * 4;
                a.rgba[d + 0] = buf[s + 0];
                a.rgba[d + 1] = buf[s + 1];
                a.rgba[d + 2] = buf[s + 2];
                a.rgba[d + 3] = buf[s + 3];
            }
        }
    }
}

float Atlas::tileU(int tile, int corner) const {
    if (tile < 0 || tile >= (int)rect.size()) return 0.0f;
    const Rect& r = rect[tile];
    float px = (float)r.x;
    int w = r.w > 0 ? r.w : baseTile;
    switch (corner) {
        case 0: return (px + 0.5f) / width;
        case 1: return (px + w - 0.5f) / width;
        case 2: return (px + w - 0.5f) / width;
        default: return (px + 0.5f) / width;
    }
}
float Atlas::tileV(int tile, int corner) const {
    if (tile < 0 || tile >= (int)rect.size()) return 0.0f;
    const Rect& r = rect[tile];
    float py = (float)r.y;
    int h = r.h > 0 ? r.h : baseTile;
    switch (corner) {
        case 0: return (py + 0.5f) / height;
        case 1: return (py + 0.5f) / height;
        case 2: return (py + h - 0.5f) / height;
        default: return (py + h - 0.5f) / height;
    }
}

// ---------------------------------------------------------------------------
// 末地门：原版是 17 层叠加（shaders/core/rendertype_end_portal.fsh）——
// 底色 = end_sky × COLORS[0]，再叠 16 层"星空贴图各转一个角度/缩放/位移
// 后乘一个固定颜色"。引擎每像素只采一次样，所以建图集时按同一套参数把
// 这些层预先合成到一张贴图上，运行时一次采样即可得到原版观感。
// ---------------------------------------------------------------------------
static const float kEndPortalColors[16][3] = {
    {0.022087f, 0.098399f, 0.110818f}, {0.011892f, 0.095924f, 0.089485f},
    {0.027636f, 0.101689f, 0.100326f}, {0.046564f, 0.109883f, 0.114838f},
    {0.064901f, 0.117696f, 0.097189f}, {0.063761f, 0.086895f, 0.123646f},
    {0.084817f, 0.111994f, 0.166380f}, {0.097489f, 0.154120f, 0.091064f},
    {0.106152f, 0.131144f, 0.195191f}, {0.097721f, 0.110188f, 0.187229f},
    {0.133516f, 0.138278f, 0.148582f}, {0.070006f, 0.243332f, 0.235792f},
    {0.196766f, 0.142899f, 0.214696f}, {0.047281f, 0.315338f, 0.321970f},
    {0.204675f, 0.390010f, 0.302066f}, {0.080955f, 0.314821f, 0.661491f},
};

// 双线性采样（结果 wrap 到 [0,1)，与原版贴图循环一致）
static void sampleWrap(const std::vector<uint8_t>& img, int w, int h, float u, float v,
                       float* rgb) {
    u -= floorf(u); v -= floorf(v);
    float fx = u * w - 0.5f, fy = v * h - 0.5f;
    int x0 = (int)floorf(fx), y0 = (int)floorf(fy);
    float tx = fx - x0, ty = fy - y0;
    rgb[0] = rgb[1] = rgb[2] = 0.0f;
    for (int j = 0; j < 2; j++)
        for (int i = 0; i < 2; i++) {
            int sx = (x0 + i) % w; if (sx < 0) sx += w;
            int sy = (y0 + j) % h; if (sy < 0) sy += h;
            float wgt = (i ? tx : 1.0f - tx) * (j ? ty : 1.0f - ty);
            size_t s = ((size_t)sy * w + sx) * 4;
            rgb[0] += img[s + 0] * wgt / 255.0f;
            rgb[1] += img[s + 1] * wgt / 255.0f;
            rgb[2] += img[s + 2] * wgt / 255.0f;
        }
}

// 按纹理坐标取一个点（wrap 寻址，最近邻）。
static void samplePoint(const std::vector<uint8_t>& img, int w, int h, float u, float v, float* rgb) {
    u -= floorf(u); v -= floorf(v);
    int x = (int)(u * w), y = (int)(v * h);
    if (x < 0) x = 0; if (x >= w) x = w - 1;
    if (y < 0) y = 0; if (y >= h) y = h - 1;
    size_t s = ((size_t)y * w + x) * 4;
    rgb[0] = img[s + 0] / 255.0f;
    rgb[1] = img[s + 1] / 255.0f;
    rgb[2] = img[s + 2] / 255.0f;
}

static std::vector<uint8_t> compositeEndPortal(int size,
                                               const std::vector<uint8_t>& star, int sw, int sh,
                                               const std::vector<uint8_t>& sky, int kw, int kh) {
    const int kN = 4;                     // 每层的盒式采样点数（N×N）
    const float kEndPortalAnimPhase = 0.35f; // 代替原版随时间的 GameTime 相位（静态）
    std::vector<uint8_t> out((size_t)size * size * 4, 255);
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            float fx = ((float)x + 0.5f) / size, fy = ((float)y + 0.5f) / size;
            float c[3] = {0, 0, 0};
            if (!sky.empty()) { // 底色：end_sky × COLORS[0]（原版直接按投影坐标采，无层变换）
                float s[3];
                sampleWrap(sky, kw, kh, fx * 2.0f, fy * 2.0f, s);
                for (int k = 0; k < 3; k++) c[k] += s[k] * kEndPortalColors[0][k];
            }
            for (int i = 1; i <= 16; i++) {
                // 原版 end_portal_layer()：SCALE_TRANSLATE(0.5x+0.25) → 平移
                // (17/i, (2+i/1.5)·GameTime·1.5) → 旋转 (i²·4321+i·9)·2° → 缩放
                // 2·(4.5-i/4)。GameTime 那项在原版是动的，这里取固定相位。
                float scale = (4.5f - i / 4.0f) * 2.0f;
                float ang = (i * i * 4321.0f + i * 9.0f) * 2.0f * 3.14159265f / 180.0f;
                float ca = cosf(ang), sa = sinf(ang);
                float u0 = 0.5f * fx + 0.25f + 17.0f / i;
                float v0 = 0.5f * fy + 0.25f + (2.0f + i / 1.5f) * kEndPortalAnimPhase;
                float rx = scale * (ca * u0 - sa * v0);
                float ry = scale * (sa * u0 + ca * v0);
                // 原版是 textureProj + mip：一个像素覆盖星图上一小片（约 scale·0.5 个
                // 纹素）并被平均掉。点采样只会零星命中 0.5% 的亮星，整体比原版暗一大截，
                // 所以这里在同一覆盖范围里做 N×N 盒式平均。
                float fp = scale * 0.5f / size;   // 每像素在层空间覆盖的纹理单位数
                float acc[3] = {0, 0, 0};
                for (int a = 0; a < kN; a++)
                    for (int b = 0; b < kN; b++) {
                        float s[3];
                        samplePoint(star, sw, sh,
                                    rx + ((a + 0.5f) / kN - 0.5f) * fp,
                                    ry + ((b + 0.5f) / kN - 0.5f) * fp, s);
                        acc[0] += s[0]; acc[1] += s[1]; acc[2] += s[2];
                    }
                const float* col = kEndPortalColors[i - 1];
                for (int k = 0; k < 3; k++) c[k] += acc[k] / (kN * kN) * col[k];
            }
            // c 就是原版着色器算出的显示色（原版输出到非 sRGB 帧缓冲，算出的值
            // 直接就是显示值）。贴图字节也是显示色，采样时由 sRGB 视图解回线性、
            // 交换链再编码回去，所以这里直接按显示色写字节即可。
            size_t d = ((size_t)y * size + x) * 4;
            for (int k = 0; k < 3; k++) {
                float v = c[k] * 255.0f;
                out[d + k] = (uint8_t)(v > 255.0f ? 255.0f : (v < 0.0f ? 0.0f : v));
            }
            out[d + 3] = 255;
        }
    }
    return out;
}

// 贴图源：像素 + 目标尺寸（目标尺寸 = 原图尺寸，动画条取首帧）
struct TileSrc {
    std::vector<uint8_t> px;
    int sw = 0, sh = 0;   // 源图尺寸
    int w = 0, h = 0;     // 目标 tile 尺寸
    bool procedural = false;
};

Atlas buildAtlas(const std::string& dir) {
    // dir = <assets>/block；物品图标在 <assets>/item/。
    std::string assetRoot = dir;
    size_t cut = assetRoot.find_last_of("/\\");
    if (cut != std::string::npos) assetRoot = assetRoot.substr(0, cut);
    Atlas a;
    a.rect.assign(kAtlasMaxTiles, Atlas::Rect{});
    const int itemCount = (int)(sizeof(kItemFiles) / sizeof(kItemFiles[0]));

    // ---- 1. 读所有贴图，决定每个 tile 的原生尺寸 ----
    std::vector<TileSrc> src(kAtlasMaxTiles);
    auto loadInto = [](TileSrc& s, const std::string& path) {
        std::vector<uint8_t> img;
        int w = 0, h = 0;
        if (!loadPNG(path.c_str(), img, w, h) || img.empty()) return;
        // 动画条（如 fire_0 16x512）：高是宽的整倍数 → 取首帧
        // 其他非正方形：按左上角裁成正方形
        int size = w;
        if (h != w) {
            if (h > w && (h % w) == 0) size = w;
            else size = (w < h) ? w : h;
        }
        if (size < 4) size = 4;
        if (size > 256) size = 256; // 上限：再大图集与显存吃不消
        s.px = std::move(img);
        s.sw = w; s.sh = h;
        s.w = s.h = size;
    };

    for (int t = 0; t < T_COUNT; t++) {
        if (!kTileFiles[t]) { src[t].procedural = true; continue; }
        loadInto(src[t], dir + "/" + kTileFiles[t]);
    }
    // 实体贴图：保留原始宽高（64×64 / 64×32），模型按原版 texOffs 取子矩形，
    // 一旦被裁成正方形，逐面 UV 就全错了。
    const int entityCount = (int)(sizeof(kEntityFiles) / sizeof(kEntityFiles[0]));
    for (int n = 0; n < entityCount; n++) {
        int t = T_ENTITY_PIG + n;
        std::vector<uint8_t> img;
        int w = 0, h = 0;
        if (!loadPNG((assetRoot + "/entity/" + kEntityFiles[n]).c_str(), img, w, h) || img.empty()) continue;
        src[t].px = std::move(img);
        src[t].sw = src[t].w = w;
        src[t].sh = src[t].h = h;
        src[t].procedural = false;   // kTileFiles 里是 nullptr，这里补上真图后取消程序化
    }
    for (int n = 0; n < itemCount; n++)
        loadInto(src[T_ITEM_BASE + n], assetRoot + "/item/" + kItemFiles[n]);

    // 末地门：星空 + end_sky 预合成（原版 17 层叠加的静态等价物）
    if (!src[T_END_PORTAL].px.empty()) {
        std::vector<uint8_t> sky;
        int kw = 0, kh = 0;
        loadPNG((dir + "/end_sky.png").c_str(), sky, kw, kh);
        int size = src[T_END_PORTAL].w;
        std::vector<uint8_t> comp =
            compositeEndPortal(size, src[T_END_PORTAL].px, src[T_END_PORTAL].sw,
                               src[T_END_PORTAL].sh, sky, kw, kh);
        src[T_END_PORTAL].px = std::move(comp);
        src[T_END_PORTAL].sw = src[T_END_PORTAL].sh = size;
    }

    // 程序化贴图按"材质包主流分辨率"生成：取已加载贴图里最常见的尺寸
    int common = a.baseTile;
    {
        int bestCount = 0;
        for (int s = 4; s <= 256; s <<= 1) {
            int c = 0;
            for (int t = 0; t < kAtlasMaxTiles; t++)
                if (!src[t].procedural && src[t].w == s) c++;
            if (c > bestCount) { bestCount = c; common = s; }
        }
    }
    for (int t = 0; t < kAtlasMaxTiles; t++)
        if (src[t].procedural) src[t].w = src[t].h = common;

    // ---- 2. 书架式排版：每个 tile 独立矩形 + 四周 pad ----
    auto padOf = [](int size) { return size / 2 > 32 ? 32 : (size / 2 < 2 ? 2 : size / 2); };
    struct Item { int tile; int w, h, pad; };
    std::vector<Item> items;
    for (int t = 0; t < kAtlasMaxTiles; t++) {
        int w = src[t].w ? src[t].w : common;
        int h = src[t].h ? src[t].h : common;
        items.push_back({t, w, h, padOf(w > h ? h : w)});
    }
    std::sort(items.begin(), items.end(), [](const Item& a2, const Item& b2) {
        return a2.h != b2.h ? a2.h > b2.h : a2.tile < b2.tile;
    });
    int shelfW = 512;
    for (const Item& it : items) {
        int bw = it.w + 2 * it.pad;
        if (bw > shelfW) shelfW = bw;
    }
    int x = 0, y = 0, rowH = 0, totalH = 0;
    for (const Item& it : items) {
        int bw = it.w + 2 * it.pad, bh = it.h + 2 * it.pad;
        if (x + bw > shelfW) { x = 0; y += rowH; rowH = 0; }
        a.rect[it.tile] = Atlas::Rect{x + it.pad, y + it.pad, it.w, it.h, it.pad};
        x += bw;
        if (bh > rowH) rowH = bh;
    }
    totalH = y + rowH;
    a.width = shelfW;
    a.height = totalH > 0 ? totalH : common;
    a.rgba.assign((size_t)a.width * a.height * 4, 255);

    // ---- 3. 写入像素：按目标尺寸缩放 + 四周边缘扩展（clamp 采样）----
    // 边缘扩展让 mipmap 每一级都不会把隔壁 tile 的颜色混进来。
    auto blitTile = [&](int tile, const std::vector<uint8_t>& img, int sw, int sh) {
        if (img.empty()) return;                 // 缺失贴图保持白
        const Atlas::Rect& r = a.rect[tile];
        if (r.w <= 0 || r.h <= 0) return;
        for (int dy = -r.pad; dy < r.h + r.pad; dy++) {
            for (int dx = -r.pad; dx < r.w + r.pad; dx++) {
                int sx = (int)((int64_t)dx * sw / r.w);
                int sy = (int)((int64_t)dy * sh / r.h);
                if (sx < 0) sx = 0; if (sx > sw - 1) sx = sw - 1;
                if (sy < 0) sy = 0; if (sy > sh - 1) sy = sh - 1;
                size_t s = ((size_t)sy * sw + sx) * 4;
                int px = r.x + dx, py = r.y + dy;
                if (px < 0 || px >= a.width || py < 0 || py >= a.height) continue;
                size_t d = ((size_t)py * a.width + px) * 4;
                a.rgba[d + 0] = img[s + 0];
                a.rgba[d + 1] = img[s + 1];
                a.rgba[d + 2] = img[s + 2];
                a.rgba[d + 3] = img[s + 3];
            }
        }
    };

    for (int t = 0; t < T_COUNT; t++) {
        if (src[t].procedural) {
            fillProceduralTile(t, a); // 内部先画 16x16 再按 tile 尺寸缩放写入
            continue;
        }
        std::vector<uint8_t>& img = src[t].px;
        if (img.empty()) continue;
        const Tint& tint = kTileTint[t];
        std::vector<uint8_t> tinted = img;
        for (size_t i = 0; i + 3 < tinted.size(); i += 4)
            for (int c = 0; c < 3; c++) {
                float v = tinted[i + c] * (&tint.r)[c];
                tinted[i + c] = (uint8_t)(v > 255.0f ? 255.0f : v);
            }
        int sw = src[t].sw, sh = src[t].sh;
        // 动画条：只取首帧（高 = 宽的整倍数）
        if (sh != sw && sh > sw && (sh % sw) == 0) sh = sw;
        blitTile(t, tinted, sw, sh);
    }

    // 物品图标（tile = T_ITEM_BASE + n）
    for (int n = 0; n < itemCount; n++) {
        int t = T_ITEM_BASE + n;
        if (src[t].px.empty()) continue;
        int sw = src[t].sw, sh = src[t].sh;
        if (sh != sw && sh > sw && (sh % sw) == 0) sh = sw;
        blitTile(t, src[t].px, sw, sh);
    }

    // 有眼的框架不再把眼贴图合进"框架顶面"图块：mesher 按原版模型另外画一个
    // 居中的眼 element（顶面 uv 4,4,12,12 / 侧面 uv 4,0,12,3），
    // 所以 T_END_PORTAL_FRAME_EYE 必须保持 end_portal_frame_eye.png 原样。

    fprintf(stderr, "[atlas] %dx%d px 共 %d 格，基准贴图 %d px（原分辨率贴图按原尺寸入图集）\n",
            a.width, a.height, kAtlasMaxTiles, common);

    a.built = true;
    return a;
}
