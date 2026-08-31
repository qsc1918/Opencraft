#pragma once
// ===========================================================================
// items.hpp — 物品注册表（对齐 Minecraft 1.0 时代的物品体系）
//
// 物品 (Item) = 命名空间 ID + 类别 + 属性 的注册表条目，与方块注册表
// (blocks.hpp BLOCK_DEFS) 平行。见 docs/standards.md。
//
// 当前版本没有生存模式：物品只作为"目录 + 背包图标"存在，
// 为将来加入生存模式预留字段（工具等级/盔甲槽位）。
// 图标 tile 从 T_ITEM_BASE 起排列在图集 (atlas.cpp) 中。
// ===========================================================================
#include <cstdint>

// 图集 tile 分配: blocks.hpp 的 T_* 占 0..20，物品图标从这里开始。
inline constexpr uint8_t T_ITEM_BASE = 21;

// 工具类型（MC 1.0: 镐/斧/锹/剑）
enum ToolType : uint8_t {
    TOOL_NONE = 0, TOOL_PICKAXE, TOOL_AXE, TOOL_SHOVEL, TOOL_SWORD,
};

// 盔甲槽位（MC 1.0: 头/胸/腿/脚）
enum ArmorSlot : uint8_t {
    ARMOR_NONE = 0, ARMOR_HEAD, ARMOR_CHEST, ARMOR_LEGS, ARMOR_FEET,
};

// 物品类别
enum ItemCategory : uint8_t {
    ITEM_BLOCK = 0,  // 可放置方块（指向 BlockDef）
    ITEM_TOOL,       // 工具/武器
    ITEM_ARMOR,      // 盔甲
    ITEM_MISC,       // 杂项（打火石/末影珍珠/末影之眼…）
};

// 材质等级（MC 1.0 五档；速度/耐久/挖掘等级在生存模式实现时使用）
enum ToolTier : uint8_t {
    TIER_NONE = 0, TIER_WOODEN, TIER_STONE, TIER_IRON, TIER_GOLDEN, TIER_DIAMOND,
};

// 物品运行时 id（uint16，0 = 无；规范 ID 见 ITEM_DEFS[].id）
enum ItemId : uint16_t {
    I_NONE = 0,
    // ---- 工具 5 材质 x 4 类 ----
    I_WOODEN_PICKAXE, I_WOODEN_AXE, I_WOODEN_SHOVEL, I_WOODEN_SWORD,
    I_STONE_PICKAXE,  I_STONE_AXE,  I_STONE_SHOVEL,  I_STONE_SWORD,
    I_IRON_PICKAXE,   I_IRON_AXE,   I_IRON_SHOVEL,   I_IRON_SWORD,
    I_GOLDEN_PICKAXE, I_GOLDEN_AXE, I_GOLDEN_SHOVEL, I_GOLDEN_SWORD,
    I_DIAMOND_PICKAXE, I_DIAMOND_AXE, I_DIAMOND_SHOVEL, I_DIAMOND_SWORD,
    // ---- 盔甲 4 材质 x 4 部位 ----
    I_LEATHER_HELMET, I_LEATHER_CHESTPLATE, I_LEATHER_LEGGINGS, I_LEATHER_BOOTS,
    I_IRON_HELMET,    I_IRON_CHESTPLATE,    I_IRON_LEGGINGS,    I_IRON_BOOTS,
    I_GOLDEN_HELMET,  I_GOLDEN_CHESTPLATE,  I_GOLDEN_LEGGINGS,  I_GOLDEN_BOOTS,
    I_DIAMOND_HELMET, I_DIAMOND_CHESTPLATE, I_DIAMOND_LEGGINGS, I_DIAMOND_BOOTS,
    // ---- 杂项 ----
    I_FLINT_AND_STEEL, I_ENDER_PEARL, I_EYE_OF_ENDER,
    I_COUNT,
};

struct ItemDef {
    const char*  id;        // 命名空间 ID，如 "voxmine:iron_pickaxe"
    const char*  name;      // 显示名（中文）
    ItemCategory category;
    ToolType     tool;      // ITEM_TOOL 时有效
    ToolTier     tier;      // 工具材质等级（ITEM_TOOL/ITEM_ARMOR）
    ArmorSlot    armor;     // ITEM_ARMOR 时有效
    uint8_t      iconTile;  // 图集 tile（T_ITEM_BASE 起）
    uint8_t      blockId;   // ITEM_BLOCK 时对应的方块 id
};

// 物品图标 tile 的顺序必须与下面的定义顺序一致（T_ITEM_BASE + 序号）。
inline constexpr ItemDef ITEM_DEFS[I_COUNT] = {
    /*0  无  */ {"voxmine:none", "无", ITEM_MISC, TOOL_NONE, TIER_NONE, ARMOR_NONE, 0, 0},
    /*1  */ {"voxmine:wooden_pickaxe",  "木镐",     ITEM_TOOL, TOOL_PICKAXE, TIER_WOODEN,  ARMOR_NONE, T_ITEM_BASE + 0,  0},
    /*2  */ {"voxmine:wooden_axe",      "木斧",     ITEM_TOOL, TOOL_AXE,     TIER_WOODEN,  ARMOR_NONE, T_ITEM_BASE + 1,  0},
    /*3  */ {"voxmine:wooden_shovel",   "木锹",     ITEM_TOOL, TOOL_SHOVEL,  TIER_WOODEN,  ARMOR_NONE, T_ITEM_BASE + 2,  0},
    /*4  */ {"voxmine:wooden_sword",    "木剑",     ITEM_TOOL, TOOL_SWORD,   TIER_WOODEN,  ARMOR_NONE, T_ITEM_BASE + 3,  0},
    /*5  */ {"voxmine:stone_pickaxe",   "石镐",     ITEM_TOOL, TOOL_PICKAXE, TIER_STONE,   ARMOR_NONE, T_ITEM_BASE + 4,  0},
    /*6  */ {"voxmine:stone_axe",       "石斧",     ITEM_TOOL, TOOL_AXE,     TIER_STONE,   ARMOR_NONE, T_ITEM_BASE + 5,  0},
    /*7  */ {"voxmine:stone_shovel",    "石锹",     ITEM_TOOL, TOOL_SHOVEL,  TIER_STONE,   ARMOR_NONE, T_ITEM_BASE + 6,  0},
    /*8  */ {"voxmine:stone_sword",     "石剑",     ITEM_TOOL, TOOL_SWORD,   TIER_STONE,   ARMOR_NONE, T_ITEM_BASE + 7,  0},
    /*9  */ {"voxmine:iron_pickaxe",    "铁镐",     ITEM_TOOL, TOOL_PICKAXE, TIER_IRON,    ARMOR_NONE, T_ITEM_BASE + 8,  0},
    /*10 */ {"voxmine:iron_axe",        "铁斧",     ITEM_TOOL, TOOL_AXE,     TIER_IRON,    ARMOR_NONE, T_ITEM_BASE + 9,  0},
    /*11 */ {"voxmine:iron_shovel",     "铁锹",     ITEM_TOOL, TOOL_SHOVEL,  TIER_IRON,    ARMOR_NONE, T_ITEM_BASE + 10, 0},
    /*12 */ {"voxmine:iron_sword",      "铁剑",     ITEM_TOOL, TOOL_SWORD,   TIER_IRON,    ARMOR_NONE, T_ITEM_BASE + 11, 0},
    /*13 */ {"voxmine:golden_pickaxe",  "金镐",     ITEM_TOOL, TOOL_PICKAXE, TIER_GOLDEN,  ARMOR_NONE, T_ITEM_BASE + 12, 0},
    /*14 */ {"voxmine:golden_axe",      "金斧",     ITEM_TOOL, TOOL_AXE,     TIER_GOLDEN,  ARMOR_NONE, T_ITEM_BASE + 13, 0},
    /*15 */ {"voxmine:golden_shovel",   "金锹",     ITEM_TOOL, TOOL_SHOVEL,  TIER_GOLDEN,  ARMOR_NONE, T_ITEM_BASE + 14, 0},
    /*16 */ {"voxmine:golden_sword",    "金剑",     ITEM_TOOL, TOOL_SWORD,   TIER_GOLDEN,  ARMOR_NONE, T_ITEM_BASE + 15, 0},
    /*17 */ {"voxmine:diamond_pickaxe", "钻石镐",   ITEM_TOOL, TOOL_PICKAXE, TIER_DIAMOND, ARMOR_NONE, T_ITEM_BASE + 16, 0},
    /*18 */ {"voxmine:diamond_axe",     "钻石斧",   ITEM_TOOL, TOOL_AXE,     TIER_DIAMOND, ARMOR_NONE, T_ITEM_BASE + 17, 0},
    /*19 */ {"voxmine:diamond_shovel",  "钻石锹",   ITEM_TOOL, TOOL_SHOVEL,  TIER_DIAMOND, ARMOR_NONE, T_ITEM_BASE + 18, 0},
    /*20 */ {"voxmine:diamond_sword",   "钻石剑",   ITEM_TOOL, TOOL_SWORD,   TIER_DIAMOND, ARMOR_NONE, T_ITEM_BASE + 19, 0},
    /*21 */ {"voxmine:leather_helmet",       "皮革帽子", ITEM_ARMOR, TOOL_NONE, TIER_NONE, ARMOR_HEAD,  T_ITEM_BASE + 20, 0},
    /*22 */ {"voxmine:leather_chestplate",   "皮革外套", ITEM_ARMOR, TOOL_NONE, TIER_NONE, ARMOR_CHEST, T_ITEM_BASE + 21, 0},
    /*23 */ {"voxmine:leather_leggings",     "皮革裤子", ITEM_ARMOR, TOOL_NONE, TIER_NONE, ARMOR_LEGS,  T_ITEM_BASE + 22, 0},
    /*24 */ {"voxmine:leather_boots",        "皮革靴子", ITEM_ARMOR, TOOL_NONE, TIER_NONE, ARMOR_FEET,  T_ITEM_BASE + 23, 0},
    /*25 */ {"voxmine:iron_helmet",          "铁头盔",   ITEM_ARMOR, TOOL_NONE, TIER_IRON, ARMOR_HEAD,  T_ITEM_BASE + 24, 0},
    /*26 */ {"voxmine:iron_chestplate",      "铁胸甲",   ITEM_ARMOR, TOOL_NONE, TIER_IRON, ARMOR_CHEST, T_ITEM_BASE + 25, 0},
    /*27 */ {"voxmine:iron_leggings",        "铁护腿",   ITEM_ARMOR, TOOL_NONE, TIER_IRON, ARMOR_LEGS,  T_ITEM_BASE + 26, 0},
    /*28 */ {"voxmine:iron_boots",           "铁靴子",   ITEM_ARMOR, TOOL_NONE, TIER_IRON, ARMOR_FEET,  T_ITEM_BASE + 27, 0},
    /*29 */ {"voxmine:golden_helmet",        "金头盔",   ITEM_ARMOR, TOOL_NONE, TIER_GOLDEN, ARMOR_HEAD,  T_ITEM_BASE + 28, 0},
    /*30 */ {"voxmine:golden_chestplate",    "金胸甲",   ITEM_ARMOR, TOOL_NONE, TIER_GOLDEN, ARMOR_CHEST, T_ITEM_BASE + 29, 0},
    /*31 */ {"voxmine:golden_leggings",      "金护腿",   ITEM_ARMOR, TOOL_NONE, TIER_GOLDEN, ARMOR_LEGS,  T_ITEM_BASE + 30, 0},
    /*32 */ {"voxmine:golden_boots",         "金靴子",   ITEM_ARMOR, TOOL_NONE, TIER_GOLDEN, ARMOR_FEET,  T_ITEM_BASE + 31, 0},
    /*33 */ {"voxmine:diamond_helmet",       "钻石头盔", ITEM_ARMOR, TOOL_NONE, TIER_DIAMOND, ARMOR_HEAD,  T_ITEM_BASE + 32, 0},
    /*34 */ {"voxmine:diamond_chestplate",   "钻石胸甲", ITEM_ARMOR, TOOL_NONE, TIER_DIAMOND, ARMOR_CHEST, T_ITEM_BASE + 33, 0},
    /*35 */ {"voxmine:diamond_leggings",     "钻石护腿", ITEM_ARMOR, TOOL_NONE, TIER_DIAMOND, ARMOR_LEGS,  T_ITEM_BASE + 34, 0},
    /*36 */ {"voxmine:diamond_boots",        "钻石靴子", ITEM_ARMOR, TOOL_NONE, TIER_DIAMOND, ARMOR_FEET,  T_ITEM_BASE + 35, 0},
    /*37 */ {"voxmine:flint_and_steel", "打火石",     ITEM_MISC, TOOL_NONE, TIER_NONE, ARMOR_NONE, T_ITEM_BASE + 36, 0},
    /*38 */ {"voxmine:ender_pearl",     "末影珍珠",   ITEM_MISC, TOOL_NONE, TIER_NONE, ARMOR_NONE, T_ITEM_BASE + 37, 0},
    /*39 */ {"voxmine:eye_of_ender",    "末影之眼",   ITEM_MISC, TOOL_NONE, TIER_NONE, ARMOR_NONE, T_ITEM_BASE + 38, 0},
};

inline const ItemDef& itemDef(uint16_t id) {
    return ITEM_DEFS[id < I_COUNT ? id : (uint16_t)I_NONE];
}

// 图标 tile 对应的物品贴图文件名（atlas.cpp 用；顺序 = ITEM_DEFS 1..I_COUNT-1）
const char* itemIconFile(uint8_t iconTile);
