#pragma once
// ===========================================================================
// endgen.hpp — 末地（the_end）生成：完全复刻原版机制
//
// 对齐 Java 版 26.2：
//  - 地形：NoiseBasedChunkGenerator + noise_settings/end.json 的 final_density
//    （squeeze(0.64 * blend_density(add(-0.234375, slideEnd(sloped_cheese))))），
//    sloped_cheese = end_islands + base_3d_noise(old_blended_noise)。
//    密度按 4×4×4 采样再三线性插值，等价原版 NoiseInterpolator。
//  - 结构：10 根黑曜石柱（EndSpikeFeature）、返回传送门（EndPodiumFeature）、
//    进入末地时踩的黑曜石平台（EndPlatformFeature）。
//  - 外岛：end_island_decorated（rarity 1/14、count 1..2、in_square、高度 55..70）
//    与 chorus_plant 植被，按 3×3 区块邻域求值后裁剪到本区块。
//  - 群系：只实现 the_end（先只做一种的约定）。
// ===========================================================================
#include <cstdint>

namespace endgen {

// 中心区域 Y（返回传送门底座层，与原版一致）
constexpr int END_ORIGIN_Y = 64;
// 进入末地时落脚的 obsidian 平台（原版 placed_feature/end_platform 的固定坐标）
constexpr int END_PLATFORM_X = 100;
constexpr int END_PLATFORM_Y = 49;
constexpr int END_PLATFORM_Z = 0;
// 玩家进入末地后的落点（平台上方）
constexpr float END_SPAWN_X = END_PLATFORM_X + 0.5f;
constexpr float END_SPAWN_Y = END_PLATFORM_Y + 0.0f;
constexpr float END_SPAWN_Z = END_PLATFORM_Z + 0.5f;

// 生成一个末地区块（含地形、结构、植被）。
void generateEnd(uint32_t seed, int cx, int cz, uint8_t* out);

// 调试用：某列的 end_islands 高度值（原版 getHeightValue，范围 -100..80）。
float islandHeightValue(int blockX, int blockZ);

// 调试用：某点的末地 final_density。
double debugDensity(uint32_t seed, int wx, int wy, int wz);

// 调试用：某点的 base_3d_noise 原始值。
double debugBase3D(uint32_t seed, int wx, int wy, int wz);

// 调试用：打印某列的原始密度（排查插值）。
void debugDumpColumn(uint32_t seed, int cx, int cz, int lx, int lz);

// 地形高度查询：指定列最高的末地石/黑曜石 y；全空气返回 -1。
// 传送落地找安全点用。
int surfaceHeight(const uint8_t* out, int lx, int lz);

} // namespace endgen
