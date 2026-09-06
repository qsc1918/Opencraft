#pragma once
#include <cstdint>

// ---------------------------------------------------------------------------
// 方块运行时 ID（存档/内存中的 uint8 索引）
// 规范 ID 是 BLOCK_DEFS[].id 里的命名空间字符串（Resource location），
// 两者通过方块注册表一一对应。见 docs/standards.md §方块。
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
    // --- Nether blocks ---
    B_NETHERRACK = 19,
    B_SOUL_SAND  = 20,
    B_GLOWSTONE  = 21,
    B_NETHER_BRICK = 22,
    B_OBSIDIAN    = 23,
    B_LAVA        = 24,
    B_NETHER_PORTAL = 25,
    // --- End blocks ---
    B_END_STONE       = 26,
    B_END_PORTAL_FRAME = 27,
    B_END_PORTAL       = 28,
    B_END_GATEWAY      = 29,
    B_DRAGON_EGG       = 30,
    B_END_PORTAL_FRAME_EYE = 31,  // 放了末影之眼的末地传送门框架
    B_COUNT = 32,
};

// Tile ids into the texture atlas (assigned when atlas is built)
// 20 = 白色占位；21 起为程序化纹理 tile（无 png，代码生成，EULA 安全）。
// 物品图标从 items.hpp 的 T_ITEM_BASE 开始。
enum Tile : uint8_t {
    T_GRASS_TOP = 0,   T_GRASS_SIDE = 1,  T_DIRT = 2,      T_STONE = 3,
    T_BEDROCK = 4,     T_COBBLE = 5,      T_PLANKS = 6,    T_LOG_SIDE = 7,
    T_LOG_TOP = 8,     T_LEAVES = 9,      T_SAND = 10,     T_GRAVEL = 11,
    T_COAL = 12,       T_IRON = 13,       T_GOLD = 14,     T_DIAMOND = 15,
    T_REDSTONE = 16,   T_WATER = 17,      T_SNOW = 18,     T_GLASS = 19,
    T_WHITE = 20,
    T_END_CRYSTAL = 21,   // 末影水晶体（程序化紫晶）
    // --- Nether tile ---
    T_NETHERRACK = 22,
    T_SOUL_SAND  = 23,
    T_GLOWSTONE  = 24,
    T_NETHER_BRICK = 25,
    T_OBSIDIAN    = 26,
    T_LAVA        = 27,
    T_NETHER_PORTAL = 28,
    // --- End tile ---
    T_END_STONE       = 29,
    T_END_PORTAL_FRAME = 30,
    T_END_PORTAL       = 31,
    T_END_GATEWAY      = 32,
    T_DRAGON_EGG       = 33,
    T_END_PORTAL_FRAME_EYE = 34,  // 放了眼的有眼框架
    T_COUNT = 35,
};

// Face indices（MC 语义: F_PY=up 顶面, F_NY=down 底面, ±X/±Z 四侧面）
enum Face : int { F_PX = 0, F_NX = 1, F_PY = 2, F_NY = 3, F_PZ = 4, F_NZ = 5 };

// ---------------------------------------------------------------------------
// 方块注册表（对齐 MC 规范: 每种方块 = 命名空间 ID + 一组规范属性）
//  - id:            命名空间 ID（本项目的默认命名空间为 voxmine:，规范见
//                   docs/standards.md §命名空间ID）。未来存档/联机/模组
//                   以此字符串为准，运行时 uint8 id 仅是内部索引。
//  - name:          显示名（中文）。
//  - opaque:        完全遮挡（用于面剔除；等价 MC 的 opaque 全方块）。
//  - solid:         有碰撞箱（参与玩家/实体碰撞）。
//  - lightEmission: 发光等级 0..15（MC 光照规范；当前无光源，全 0，光照引擎预留）。
//  - opacity:       光照衰减 0..15（MC: 不透明块 15，树叶/水 1，空气/玻璃 0）。
//  - tileTop/Side/Bottom: 图集 tile（F_PY / 侧面 / F_NY）。
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
    /*B_AIR     */ {"voxmine:air",            "空气",     false, false, 0, 0,  T_WHITE,   T_WHITE,   T_WHITE  },
    /*B_STONE    */ {"voxmine:stone",          "石头",     true,  true,  0, 15, T_STONE,   T_STONE,   T_STONE  },
    /*B_GRASS    */ {"voxmine:grass_block",    "草方块",   true,  true,  0, 15, T_GRASS_TOP, T_GRASS_SIDE, T_DIRT},
    /*B_DIRT     */ {"voxmine:dirt",           "泥土",     true,  true,  0, 15, T_DIRT,    T_DIRT,    T_DIRT   },
    /*B_BEDROCK  */ {"voxmine:bedrock",        "基岩",     true,  true,  0, 15, T_BEDROCK, T_BEDROCK, T_BEDROCK},
    /*B_COBBLE   */ {"voxmine:cobblestone",    "圆石",     true,  true,  0, 15, T_COBBLE,  T_COBBLE,  T_COBBLE },
    /*B_PLANKS   */ {"voxmine:oak_planks",     "橡木木板", true,  true,  0, 15, T_PLANKS,  T_PLANKS,  T_PLANKS },
    /*B_LOG      */ {"voxmine:oak_log",        "橡木原木", true,  true,  0, 15, T_LOG_TOP, T_LOG_SIDE, T_LOG_TOP},
    /*B_LEAVES   */ {"voxmine:oak_leaves",     "橡木树叶", false, true,  0, 1,  T_LEAVES,  T_LEAVES,  T_LEAVES },
    /*B_SAND     */ {"voxmine:sand",           "沙子",     true,  true,  0, 15, T_SAND,    T_SAND,    T_SAND   },
    /*B_GRAVEL   */ {"voxmine:gravel",         "沙砾",     true,  true,  0, 15, T_GRAVEL,  T_GRAVEL,  T_GRAVEL },
    /*B_COAL     */ {"voxmine:coal_ore",       "煤矿石",   true,  true,  0, 15, T_COAL,    T_COAL,    T_COAL   },
    /*B_IRON     */ {"voxmine:iron_ore",       "铁矿石",   true,  true,  0, 15, T_IRON,    T_IRON,    T_IRON   },
    /*B_GOLD     */ {"voxmine:gold_ore",       "金矿石",   true,  true,  0, 15, T_GOLD,    T_GOLD,    T_GOLD   },
    /*B_DIAMOND  */ {"voxmine:diamond_ore",    "钻石矿石", true,  true,  0, 15, T_DIAMOND, T_DIAMOND, T_DIAMOND},
    /*B_REDSTONE */ {"voxmine:redstone_ore",   "红石矿石", true,  true,  0, 15, T_REDSTONE, T_REDSTONE, T_REDSTONE},
    /*B_WATER    */ {"voxmine:water",          "水",       false, false, 0, 1,  T_WATER,   T_WATER,   T_WATER  },
    /*B_SNOW     */ {"voxmine:snow",           "雪块",     true,  true,  0, 15, T_SNOW,    T_SNOW,    T_SNOW   },
    /*B_GLASS    */ {"voxmine:glass",          "玻璃",     false, true,  0, 0,  T_GLASS,   T_GLASS,   T_GLASS  },
    // --- Nether ---
    /*B_NETHERRACK*/ {"voxmine:netherrack",    "地狱岩",   true,  true,  0, 15, T_NETHERRACK, T_NETHERRACK, T_NETHERRACK},
    /*B_SOUL_SAND */ {"voxmine:soul_sand",     "灵魂沙",   true,  true,  0, 15, T_SOUL_SAND,  T_SOUL_SAND,  T_SOUL_SAND},
    /*B_GLOWSTONE */ {"voxmine:glowstone",     "萤石",     true,  true,  15, 1,  T_GLOWSTONE,  T_GLOWSTONE,  T_GLOWSTONE},
    /*B_NETHER_BRICK*/ {"voxmine:nether_brick","地狱砖",   true,  true,  0, 15, T_NETHER_BRICK, T_NETHER_BRICK, T_NETHER_BRICK},
    /*B_OBSIDIAN  */ {"voxmine:obsidian",      "黑曜石",   true,  true,  0, 15, T_OBSIDIAN,  T_OBSIDIAN,  T_OBSIDIAN},
    /*B_LAVA      */ {"voxmine:lava",          "岩浆",     false, false, 15, 1,  T_LAVA,      T_LAVA,      T_LAVA  },
    /*B_NETHER_PORTAL*/ {"voxmine:nether_portal","传送门", false, false, 11, 0,  T_NETHER_PORTAL, T_NETHER_PORTAL, T_NETHER_PORTAL},
    // --- End ---
    /*B_END_STONE */ {"voxmine:end_stone",     "末地石",   true,  true,  0, 15, T_END_STONE, T_END_STONE, T_END_STONE},
    /*B_END_PORTAL_FRAME*/ {"voxmine:end_portal_frame","末地传送门框架", true, true, 0, 15, T_END_PORTAL_FRAME, T_END_PORTAL_FRAME, T_END_PORTAL_FRAME},
    /*B_END_PORTAL */ {"voxmine:end_portal",   "末地传送门", false, false, 15, 0, T_END_PORTAL, T_END_PORTAL, T_END_PORTAL},
    /*B_END_GATEWAY*/ {"voxmine:end_gateway",  "折跃门",   false, false, 15, 0,  T_END_GATEWAY, T_END_GATEWAY, T_END_GATEWAY},
    /*B_DRAGON_EGG*/ {"voxmine:dragon_egg",    "龙蛋",     true,  true,  1,  15, T_DRAGON_EGG, T_DRAGON_EGG, T_DRAGON_EGG},
    /*B_END_PORTAL_FRAME_EYE*/ {"voxmine:end_portal_frame_eye","末地传送门框架(有眼)", true, true, 0, 15, T_END_PORTAL_FRAME_EYE, T_END_PORTAL_FRAME_EYE, T_END_PORTAL_FRAME_EYE},
};

// 注册表访问；越界回落为空气定义（等价 MC 对未知方块的容错处理）。
inline const BlockDef& blockDef(uint8_t id) {
    return BLOCK_DEFS[id < B_COUNT ? id : (uint8_t)B_AIR];
}

// ---- 属性查询（由注册表驱动；对越界 id 保持旧行为: 视为不透明/有碰撞）----
inline bool blockIsOpaque(uint8_t id) {
    return id < B_COUNT ? BLOCK_DEFS[id].opaque : true;
}

inline bool blockIsSolid(uint8_t id) {
    return id < B_COUNT ? BLOCK_DEFS[id].solid : true;
}

inline bool blockIsRenderable(uint8_t id) {
    return id != B_AIR;
}

// 方块在指定面使用的贴图 tile。水等特殊渲染仍由 mesher 单独处理。
inline uint8_t blockTile(uint8_t id, int face) {
    if (id >= B_COUNT) return T_WHITE;
    const BlockDef& d = BLOCK_DEFS[id];
    if (face == F_PY) return d.tileTop;
    if (face == F_NY) return d.tileBottom;
    return d.tileSide;
}
