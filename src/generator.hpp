#pragma once
#include "dimensions.hpp"
#include <cstdint>

namespace gen {
// 生成完整区块列 (16 x WORLD_HEIGHT x 16) 的方块 id。
// 相同 (seed, cx, cz) 结果确定；out 需有 CHUNK_VOL 字节。
void generateColumn(uint32_t seed, int cx, int cz, uint8_t* out);

// 生成下界区块列（旧设计：3D 噪声洞穴 + 岩浆海）。
void generateNether(uint32_t seed, int cx, int cz, uint8_t* out);

// 生成末地区块列（主岛 + 黑曜石柱 + 出口传送门）。
void generateEnd(uint32_t seed, int cx, int cz, uint8_t* out);

// 按维度分发。
void generateForDim(DimensionId dim, uint32_t seed, int cx, int cz, uint8_t* out);
}
