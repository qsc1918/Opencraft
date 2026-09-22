#pragma once
// ===========================================================================
// nethergen.hpp — 下界（the_nether）生成：复刻原版机制
//
// 对齐 Java 版 26.2：
//  - 地形：noise_settings/nether.json 的 final_density
//    （squeeze(0.64 * slide(base_3d_noise, 0,128, top 24/0 → 0.9375, bottom -8/24 → 2.5))）
//  - 表面规则：基岩地板（y 0..4 渐变）/ 天花板（y 123..127 渐变）、
//    sea_level 32 的岩浆海、下界荒地群系表层（灵魂沙/沙砾/地狱岩）
//  - 群系只实现 nether_wastes（下界荒地），与"群系先只加一种"的约定一致
// ===========================================================================
#include <cstdint>

namespace nethergen {

void generateNether(uint32_t seed, int cx, int cz, uint8_t* out);

} // namespace nethergen
