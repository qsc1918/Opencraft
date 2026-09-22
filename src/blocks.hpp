#pragma once
#include <cstdint>

// ---------------------------------------------------------------------------
// 方块运行时 ID（存档/内存中的 uint8 索引）
// 规范 ID 是 BLOCK_DEFS[].id 的命名空间字符串（资源定位符），
// 两者由注册表一一对应。见 docs/standards.md §方块。
// ---------------------------------------------------------------------------
enum Block : uint8_t {
    B_AIR = 0,
    B_STONE = 1,
    B_GRASS = 2,
    B_DIRT = 3,
    B_BEDROCK = 4,
    B_COBBLE = 5,
    B_PLANKS = 6,
    B_LOG = 7,
    B_LEAVES = 8,
    B_SAND = 9,
    B_GRAVEL = 10,
    B_COAL = 11,
    B_IRON = 12,
    B_GOLD = 13,
    B_DIAMOND = 14,
    B_REDSTONE = 15,
    B_WATER = 16,
    B_SNOW = 17,
    B_GLASS = 18,
    // --- 地狱方块 ---
    B_NETHERRACK = 19,
    B_SOUL_SAND  = 20,
    B_GLOWSTONE  = 21,
    B_NETHER_BRICK = 22,
    B_OBSIDIAN    = 23,
    B_LAVA        = 24,
    B_NETHER_PORTAL = 25,
    // --- 末地方块 ---
    B_END_STONE       = 26,
    B_END_PORTAL_FRAME = 27,
    B_END_PORTAL       = 28,
    B_END_GATEWAY      = 29,
    B_DRAGON_EGG       = 30,
    B_END_PORTAL_FRAME_EYE = 31,  // 放了末影之眼的末地传送门框架
    B_FIRE = 32,
    // --- 末地传送门框架：原版有 facing(4 向) + eye(有/无) 共 8 种状态。
    // 本实现方块存储是纯 uint8 id，没有状态位，故把朝向展开成独立 id。
    // 基准 B_END_PORTAL_FRAME 的朝向是"南"（眼凸起在 -Z 侧），
    // 顺时针 90° 依次得到西/北/东。
    B_END_PORTAL_FRAME_W = 33,        // 西
    B_END_PORTAL_FRAME_N = 34,        // 北
    B_END_PORTAL_FRAME_E = 35,        // 东
    B_END_PORTAL_FRAME_EYE_W = 36,    // 西 + 有眼
    B_END_PORTAL_FRAME_EYE_N = 37,    // 北 + 有眼
    B_END_PORTAL_FRAME_EYE_E = 38,    // 东 + 有眼
    // --- 末地植被 / 结构方块 ---
    B_CHORUS_PLANT = 39,
    B_CHORUS_FLOWER = 40,      // 活着的花（原版 AGE 0..4，这里只区分活/死）
    B_CHORUS_FLOWER_DEAD = 41, // 原版 AGE 5：不再生长
    B_IRON_BARS = 42,
    B_TORCH = 43,
    B_WALL_TORCH = 44,
    B_COUNT = 45,
};

// 图集图块 id（建图集时分配）。
// 20 = 白色占位；21 起为程序化纹理图块（无 png，代码生成，EULA 安全）。
// 物品图标从 items.hpp 的 T_ITEM_BASE 开始。
enum Tile : uint8_t {
    T_GRASS_TOP = 0,   T_GRASS_SIDE = 1,  T_DIRT = 2,      T_STONE = 3,
    T_BEDROCK = 4,     T_COBBLE = 5,      T_PLANKS = 6,    T_LOG_SIDE = 7,
    T_LOG_TOP = 8,     T_LEAVES = 9,      T_SAND = 10,     T_GRAVEL = 11,
    T_COAL = 12,       T_IRON = 13,       T_GOLD = 14,     T_DIAMOND = 15,
    T_REDSTONE = 16,   T_WATER = 17,      T_SNOW = 18,     T_GLASS = 19,
    T_WHITE = 20,
    T_END_CRYSTAL = 21,   // 末影水晶体（程序化紫晶）
    // --- 地狱图块 ---
    T_NETHERRACK = 22,
    T_SOUL_SAND  = 23,
    T_GLOWSTONE  = 24,
    T_NETHER_BRICK = 25,
    T_OBSIDIAN    = 26,
    T_LAVA        = 27,
    T_NETHER_PORTAL = 28,
    // --- 末地图块 ---
    T_END_STONE       = 29,
    T_END_PORTAL_FRAME = 30,
    T_END_PORTAL       = 31,
    T_END_GATEWAY      = 32,
    T_DRAGON_EGG       = 33,
    T_END_PORTAL_FRAME_EYE = 34,  // 放了眼的有眼框架
    T_END_PORTAL_FRAME_SIDE = 35, // 末地传送门框架侧面
    T_FIRE = 36,
    // --- 末地植被 / 结构 ---
    T_CHORUS_PLANT = 37,
    T_CHORUS_FLOWER = 38,
    T_CHORUS_FLOWER_DEAD = 39,
    T_IRON_BARS = 40,
    T_TORCH = 41,
    T_COUNT = 42,
};

// 面索引（MC 语义: F_PY 顶面, F_NY 底面, ±X/±Z 四侧面）
enum Face : int { F_PX = 0, F_NX = 1, F_PY = 2, F_NY = 3, F_PZ = 4, F_NZ = 5 };

// ---------------------------------------------------------------------------
// 方块注册表（对齐 MC: 每种方块 = 命名空间 ID + 一组规范属性）
//  - id:            命名空间 ID（默认命名空间 opencraft:，规范见
//                   docs/standards.md §命名空间ID）。存档/联机/模组
//                   以此字符串为准，运行时 uint8 只是内部索引。
//  - name:          显示名（中文）。
//  - opaque:        完全遮挡（面剔除用；等价 MC 不透明方块）。
//  - solid:         有碰撞箱。
//  - lightEmission: 发光等级 0..15；当前无光源，全 0，为光照引擎预留。
//  - opacity:       光照衰减 0..15（不透明 15，树叶/水 1，空气/玻璃 0）。
//  - tileTop/Side/Bottom: 图集图块（F_PY / 侧面 / F_NY）。
// ---------------------------------------------------------------------------
struct BlockDef {
    const char* id;
    const char* name;
    bool        opaque;
    bool        solid;
    uint8_t     lightEmission;
    uint8_t     opacity;
    uint8_t     tileTop;
    uint8_t     tileSide;
    uint8_t     tileBottom;
};

inline constexpr BlockDef BLOCK_DEFS[B_COUNT] = {
    /*B_AIR     */ {"opencraft:air",            "空气",     false, false, 0, 0,  T_WHITE,   T_WHITE,   T_WHITE  },
    /*B_STONE    */ {"opencraft:stone",          "石头",     true,  true,  0, 15, T_STONE,   T_STONE,   T_STONE  },
    /*B_GRASS    */ {"opencraft:grass_block",    "草方块",   true,  true,  0, 15, T_GRASS_TOP, T_GRASS_SIDE, T_DIRT},
    /*B_DIRT     */ {"opencraft:dirt",           "泥土",     true,  true,  0, 15, T_DIRT,    T_DIRT,    T_DIRT   },
    /*B_BEDROCK  */ {"opencraft:bedrock",        "基岩",     true,  true,  0, 15, T_BEDROCK, T_BEDROCK, T_BEDROCK},
    /*B_COBBLE   */ {"opencraft:cobblestone",    "圆石",     true,  true,  0, 15, T_COBBLE,  T_COBBLE,  T_COBBLE },
    /*B_PLANKS   */ {"opencraft:oak_planks",     "橡木木板", true,  true,  0, 15, T_PLANKS,  T_PLANKS,  T_PLANKS },
    /*B_LOG      */ {"opencraft:oak_log",        "橡木原木", true,  true,  0, 15, T_LOG_TOP, T_LOG_SIDE, T_LOG_TOP},
    /*B_LEAVES   */ {"opencraft:oak_leaves",     "橡木树叶", false, true,  0, 1,  T_LEAVES,  T_LEAVES,  T_LEAVES },
    /*B_SAND     */ {"opencraft:sand",           "沙子",     true,  true,  0, 15, T_SAND,    T_SAND,    T_SAND   },
    /*B_GRAVEL   */ {"opencraft:gravel",         "沙砾",     true,  true,  0, 15, T_GRAVEL,  T_GRAVEL,  T_GRAVEL },
    /*B_COAL     */ {"opencraft:coal_ore",       "煤矿石",   true,  true,  0, 15, T_COAL,    T_COAL,    T_COAL   },
    /*B_IRON     */ {"opencraft:iron_ore",       "铁矿石",   true,  true,  0, 15, T_IRON,    T_IRON,    T_IRON   },
    /*B_GOLD     */ {"opencraft:gold_ore",       "金矿石",   true,  true,  0, 15, T_GOLD,    T_GOLD,    T_GOLD   },
    /*B_DIAMOND  */ {"opencraft:diamond_ore",    "钻石矿石", true,  true,  0, 15, T_DIAMOND, T_DIAMOND, T_DIAMOND},
    /*B_REDSTONE */ {"opencraft:redstone_ore",   "红石矿石", true,  true,  0, 15, T_REDSTONE, T_REDSTONE, T_REDSTONE},
    /*B_WATER    */ {"opencraft:water",          "水",       false, false, 0, 1,  T_WATER,   T_WATER,   T_WATER  },
    /*B_SNOW     */ {"opencraft:snow",           "雪块",     true,  true,  0, 15, T_SNOW,    T_SNOW,    T_SNOW   },
    /*B_GLASS    */ {"opencraft:glass",          "玻璃",     false, true,  0, 0,  T_GLASS,   T_GLASS,   T_GLASS  },
    // --- 地狱 ---
    /*B_NETHERRACK*/ {"opencraft:netherrack",    "地狱岩",   true,  true,  0, 15, T_NETHERRACK, T_NETHERRACK, T_NETHERRACK},
    /*B_SOUL_SAND */ {"opencraft:soul_sand",     "灵魂沙",   true,  true,  0, 15, T_SOUL_SAND,  T_SOUL_SAND,  T_SOUL_SAND},
    /*B_GLOWSTONE */ {"opencraft:glowstone",     "萤石",     true,  true,  15, 1,  T_GLOWSTONE,  T_GLOWSTONE,  T_GLOWSTONE},
    /*B_NETHER_BRICK*/ {"opencraft:nether_brick","地狱砖",   true,  true,  0, 15, T_NETHER_BRICK, T_NETHER_BRICK, T_NETHER_BRICK},
    /*B_OBSIDIAN  */ {"opencraft:obsidian",      "黑曜石",   true,  true,  0, 15, T_OBSIDIAN,  T_OBSIDIAN,  T_OBSIDIAN},
    /*B_LAVA      */ {"opencraft:lava",          "岩浆",     false, false, 15, 1,  T_LAVA,      T_LAVA,      T_LAVA  },
    /*B_NETHER_PORTAL*/ {"opencraft:nether_portal","传送门", false, false, 11, 0,  T_NETHER_PORTAL, T_NETHER_PORTAL, T_NETHER_PORTAL},
    // --- 末地 ---
    /*B_END_STONE */ {"opencraft:end_stone",     "末地石",   true,  true,  0, 15, T_END_STONE, T_END_STONE, T_END_STONE},
    // 框架：顶面/侧面纹理不同，只有 13/16 高（高度见 mesher 的 blockHeight16）
    /*B_END_PORTAL_FRAME*/ {"opencraft:end_portal_frame","末地传送门框架", true, true, 0, 15, T_END_PORTAL_FRAME, T_END_PORTAL_FRAME_SIDE, T_END_STONE},
    /*B_END_PORTAL */ {"opencraft:end_portal",   "末地传送门", false, false, 15, 0, T_END_PORTAL, T_END_PORTAL, T_END_PORTAL},
    /*B_END_GATEWAY*/ {"opencraft:end_gateway",  "折跃门",   false, false, 15, 0,  T_END_GATEWAY, T_END_GATEWAY, T_END_GATEWAY},
    /*B_DRAGON_EGG*/ {"opencraft:dragon_egg",    "龙蛋",     true,  true,  1,  15, T_DRAGON_EGG, T_DRAGON_EGG, T_DRAGON_EGG},
    /*B_END_PORTAL_FRAME_EYE*/ {"opencraft:end_portal_frame_eye","末地传送门框架(有眼)", true, true, 0, 15, T_END_PORTAL_FRAME_EYE, T_END_PORTAL_FRAME_SIDE, T_END_STONE},
    // --- 火 ---
    /*B_FIRE     */ {"opencraft:fire",           "火",       false, false, 15, 0,  T_FIRE,    T_FIRE,    T_FIRE   },
    // --- 末地传送门框架朝向变体（见 Block 枚举里的说明）---
    /*B_END_PORTAL_FRAME_W*/ {"opencraft:end_portal_frame_west","末地传送门框架", true, true, 0, 15, T_END_PORTAL_FRAME, T_END_PORTAL_FRAME_SIDE, T_END_STONE},
    /*B_END_PORTAL_FRAME_N*/ {"opencraft:end_portal_frame_north","末地传送门框架", true, true, 0, 15, T_END_PORTAL_FRAME, T_END_PORTAL_FRAME_SIDE, T_END_STONE},
    /*B_END_PORTAL_FRAME_E*/ {"opencraft:end_portal_frame_east","末地传送门框架", true, true, 0, 15, T_END_PORTAL_FRAME, T_END_PORTAL_FRAME_SIDE, T_END_STONE},
    /*B_END_PORTAL_FRAME_EYE_W*/ {"opencraft:end_portal_frame_eye_west","末地传送门框架(有眼)", true, true, 0, 15, T_END_PORTAL_FRAME_EYE, T_END_PORTAL_FRAME_SIDE, T_END_STONE},
    /*B_END_PORTAL_FRAME_EYE_N*/ {"opencraft:end_portal_frame_eye_north","末地传送门框架(有眼)", true, true, 0, 15, T_END_PORTAL_FRAME_EYE, T_END_PORTAL_FRAME_SIDE, T_END_STONE},
    /*B_END_PORTAL_FRAME_EYE_E*/ {"opencraft:end_portal_frame_eye_east","末地传送门框架(有眼)", true, true, 0, 15, T_END_PORTAL_FRAME_EYE, T_END_PORTAL_FRAME_SIDE, T_END_STONE},
    // --- 末地植被 / 结构 ---
    /*B_CHORUS_PLANT*/ {"opencraft:chorus_plant", "紫颂植株", true,  true,  0, 15, T_CHORUS_PLANT, T_CHORUS_PLANT, T_CHORUS_PLANT},
    /*B_CHORUS_FLOWER*/ {"opencraft:chorus_flower","紫颂花",  true,  true,  0, 15, T_CHORUS_FLOWER, T_CHORUS_FLOWER, T_CHORUS_FLOWER},
    /*B_CHORUS_FLOWER_DEAD*/ {"opencraft:chorus_flower_dead","紫颂花(枯萎)", true, true, 0, 15, T_CHORUS_FLOWER_DEAD, T_CHORUS_FLOWER_DEAD, T_CHORUS_FLOWER_DEAD},
    /*B_IRON_BARS*/ {"opencraft:iron_bars",      "铁栏杆",   false, true,  0, 0,  T_IRON_BARS, T_IRON_BARS, T_IRON_BARS},
    /*B_TORCH    */ {"opencraft:torch",          "火把",     false, false, 14, 0,  T_TORCH,   T_TORCH,   T_TORCH  },
    /*B_WALL_TORCH*/ {"opencraft:wall_torch",    "墙上的火把", false, false, 14, 0, T_TORCH,   T_TORCH,   T_TORCH  },
};
static_assert(sizeof(BLOCK_DEFS) / sizeof(BLOCK_DEFS[0]) == B_COUNT,
              "BLOCK_DEFS 必须按 Block id 顺序逐一列出，新增方块要同步补在这里");

// ---- 末地传送门框架：朝向 / 有眼查询 ----
enum FrameFacing : int { FRAME_SOUTH = 0, FRAME_WEST = 1, FRAME_NORTH = 2, FRAME_EAST = 3 };

inline bool blockIsPortalFrame(uint8_t id) {
    return id == B_END_PORTAL_FRAME || id == B_END_PORTAL_FRAME_W ||
           id == B_END_PORTAL_FRAME_N || id == B_END_PORTAL_FRAME_E ||
           id == B_END_PORTAL_FRAME_EYE || id == B_END_PORTAL_FRAME_EYE_W ||
           id == B_END_PORTAL_FRAME_EYE_N || id == B_END_PORTAL_FRAME_EYE_E;
}

inline bool blockFrameHasEye(uint8_t id) {
    return id == B_END_PORTAL_FRAME_EYE || id == B_END_PORTAL_FRAME_EYE_W ||
           id == B_END_PORTAL_FRAME_EYE_N || id == B_END_PORTAL_FRAME_EYE_E;
}

// 未列出的 id 按南处理
inline int blockFrameFacing(uint8_t id) {
    switch (id) {
    case B_END_PORTAL_FRAME_W: case B_END_PORTAL_FRAME_EYE_W: return FRAME_WEST;
    case B_END_PORTAL_FRAME_N: case B_END_PORTAL_FRAME_EYE_N: return FRAME_NORTH;
    case B_END_PORTAL_FRAME_E: case B_END_PORTAL_FRAME_EYE_E: return FRAME_EAST;
    default: return FRAME_SOUTH;
    }
}

inline uint8_t frameId(int facing, bool eye) {
    static const uint8_t kPlain[4] = {B_END_PORTAL_FRAME, B_END_PORTAL_FRAME_W,
                                      B_END_PORTAL_FRAME_N, B_END_PORTAL_FRAME_E};
    static const uint8_t kEye[4] = {B_END_PORTAL_FRAME_EYE, B_END_PORTAL_FRAME_EYE_W,
                                    B_END_PORTAL_FRAME_EYE_N, B_END_PORTAL_FRAME_EYE_E};
    return eye ? kEye[facing & 3] : kPlain[facing & 3];
}

// 朝向 (dx,dz) 单位向量：南=+Z、西=-X、北=-Z、东=+X
inline void frameFacingDir(int facing, int& dx, int& dz) {
    switch (facing & 3) {
    case FRAME_WEST:  dx = -1; dz = 0;  break;
    case FRAME_NORTH: dx = 0;  dz = -1; break;
    case FRAME_EAST:  dx = 1;  dz = 0;  break;
    default:          dx = 0;  dz = 1;  break; // 南
    }
}

// 注册表访问；越界回落为空气（等价 MC 未知方块容错）。
inline const BlockDef& blockDef(uint8_t id) {
    return BLOCK_DEFS[id < B_COUNT ? id : (uint8_t)B_AIR];
}

// ---- 属性查询（注册表驱动；越界 id 沿用旧行为: 不透明/有碰撞）----
inline bool blockIsOpaque(uint8_t id) {
    return id < B_COUNT ? BLOCK_DEFS[id].opaque : true;
}

inline bool blockIsSolid(uint8_t id) {
    return id < B_COUNT ? BLOCK_DEFS[id].solid : true;
}

inline bool blockIsRenderable(uint8_t id) {
    return id != B_AIR;
}

// 方块指定面用的图块；水等特殊渲染仍由 mesher 单独处理。
inline uint8_t blockTile(uint8_t id, int face) {
    if (id >= B_COUNT) return T_WHITE;
    const BlockDef& d = BLOCK_DEFS[id];
    if (face == F_PY) return d.tileTop;
    if (face == F_NY) return d.tileBottom;
    return d.tileSide;
}
