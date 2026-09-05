#pragma once
// ===========================================================================
// dimensions.hpp — 维度定义（对齐 Minecraft 维度体系）
//
// 维度 (Dimension) = 独立的世界空间，各自有区块表、Y 范围、天空光/雾/环境光。
// MC Java 版三大维度: overworld / the_nether / the_end。
// 详见 docs/standards.md §维度。
// ===========================================================================
#include <cstdint>

enum DimensionId : uint8_t {
    DIM_OVERWORLD = 0,  // 主世界
    DIM_NETHER    = 1,  // 下界
    DIM_END       = 2,  // 末地
    DIM_COUNT     = 3,
};

// 维度参数表（对齐 MC 规范；本实现简化 Y 范围）
struct DimensionType {
    const char* id;          // 命名空间 ID
    const char* name;        // 显示名
    int minY;                // 世界 Y 下限（MC 1.18+: overworld -64, nether 0, end 0）
    int height;              // 世界高度（MC 1.18+: overworld 384, nether 256, end 256）
    float coordinateScale;   // 坐标缩放（下界 1:8 → 8.0，其他 1.0）
    bool hasSkylight;        // 是否有天空光
    bool hasCeiling;         // 是否有基岩天花板
    bool ultrawarm;          // 超高温（下界: 用水蒸发、床爆炸）
    float ambientLight;      // 环境光 0..1（下界 0.1，其他 0.0）
    uint8_t fogColorR, fogColorG, fogColorB;  // 雾颜色 RGB
    uint8_t skyColorR, skyColorG, skyColorB;  // 天空颜色 RGB
};

inline constexpr DimensionType DIMENSION_TYPES[DIM_COUNT] = {
    // overworld: 本实现 Y 0..127，MC 规范值 -64..319
    {"voxmine:overworld", "主世界",   0, 128, 1.0f,  true, false, false, 0.0f,
     180, 200, 220,   // 雾: 浅蓝
     153, 196, 229},  // 天空: MC 蓝
    // nether: 旧版设计 Y 0..127，有天花板，无天光
    {"voxmine:the_nether", "下界",    0, 128, 8.0f,  false, true, true, 0.1f,
     40, 10, 10,      // 雾: 暗红
     30, 10, 10},     // 天空: 暗红
    // end: 无天光，无天花板
    {"voxmine:the_end", "末地",      0, 128, 1.0f,  false, false, false, 0.0f,
     11, 0, 44,       // 雾: 深紫黑 #0B002C
     0, 0, 32},       // 天空: 深紫 #000020
};

inline const DimensionType& getDimensionType(DimensionId id) {
    return DIMENSION_TYPES[id < DIM_COUNT ? id : (uint8_t)DIM_OVERWORLD];
}

// 坐标缩放：在下界时，主世界坐标 1 格 = 下界坐标 1/8
// worldToDim: 主世界坐标 → 维度坐标
// dimToWorld: 维度坐标 → 主世界坐标
inline float worldToDim(float v, DimensionId dim) {
    return v / getDimensionType(dim).coordinateScale;
}
inline float dimToWorld(float v, DimensionId dim) {
    return v * getDimensionType(dim).coordinateScale;
}
