#pragma once
#include "dimensions.hpp"
#include <cstdint>

namespace gen {
// 生成完整区块列 (16 x WORLD_HEIGHT x 16) 的方块 id。
// 相同 (seed, cx, cz) 结果确定；out 需有 CHUNK_VOL 字节。
void generateColumn(uint32_t seed, int cx, int cz, uint8_t* out);

// 生成下界区块列（原版机制：3D 密度 + 基岩上下限 + 岩浆海 + 表面规则）。
void generateNether(uint32_t seed, int cx, int cz, uint8_t* out);

// 生成末地区块列（原版机制：end 密度函数主岛 + 黑曜石柱 + 返回传送门 + 外岛）。
// 实现在 endgen.cpp。
void generateEnd(uint32_t seed, int cx, int cz, uint8_t* out);

// 按维度分发。
void generateForDim(DimensionId dim, uint32_t seed, int cx, int cz, uint8_t* out);
}
