#pragma once
// ===========================================================================
// mctick.hpp — 原版方块随机刻（random tick）的最小实现
//
// 原版每个子区块每刻随机抽 3 个方块，只对"会随机刻"的方块调用 randomTick。
// 本实现按区块抽（每区块每刻 1 次），对紫颂花复刻 ChorusFlowerBlock.randomTick，
// 让种下的紫颂花自己长成植株。详见 endgen.cpp 的紫颂生成说明。
// ===========================================================================
#include "specs.hpp"

class World;

namespace mctick {

// 每帧调用：内部按 TICK_RATE 累积并执行若干次方块随机刻。
// 只处理玩家附近 RANDOM_TICK_RADIUS 个区块内的方块。
void tickRandomBlocks(World& w, float px, float pz, float dt);

// 单个方块的随机刻行为（原版 Block.randomTick 的对应实现）。返回 true 表示有改动。
bool randomTickBlock(World& w, int x, int y, int z);

} // namespace mctick
