#pragma once
// specs.hpp — Opencraft 世界规范常量（对齐 Minecraft Java 版子集）
// 所有世界结构数值的唯一来源；与 MC 不同的取值是刻意简化
// 完整规范见 docs/standards.md。
#include <cstdint>

// 区块与子区块
// MC: 区块 16×16，子区块 16×16×16，1.18+ 区块列 24 层
// 本实现: 区块列 16×128×16，共 8 层，尚无独立 section 存储。
constexpr int CHUNK_SIZE   = 16;  // 区块水平边长（MC 规范值，勿改）。
constexpr int WORLD_HEIGHT = 128; // 区块列高度（MC 1.18+: 384，此处简化）。
constexpr int CHUNK_VOL    = CHUNK_SIZE * WORLD_HEIGHT * CHUNK_SIZE; // 单区块方块数 32768。

// 高度与海平面
// MC: 主世界可建造 Y∈[-64,319]，海平面 63
// 本实现: Y∈[0,127]，海平面 62。
constexpr int WORLD_MIN_Y = 0;                 // Y 下限（MC 1.18+: -64）。
constexpr int WORLD_MAX_Y = WORLD_HEIGHT - 1;  // Y 上限（MC 1.18+: 319）。
constexpr int SEA_LEVEL   = 62;                // 海平面（MC: 63）。

// 游戏刻
// MC: 20 TPS，每刻 50ms；实体/AI/方块更新按刻模拟
// 本实现: 主循环为逐帧可变 dt（上限 0.05s）。
constexpr int   TICK_RATE = 20;             // 每秒刻数（MC: 20）。
constexpr float TICK_DT   = 1.0f / 20.0f;   // 单刻步长（秒），50ms。

// 坐标换算（规范见 docs/standards.md §坐标）
// MC: 区块坐标 = floor(方块坐标 / 16)，负坐标同样向下取整
// C++ 的 / 与 % 对负数是向零取整，直接用会让负坐标落到上一块，必须走这两个函数。
inline int floorDiv(int a, int b) {
    int q = a / b;
    if ((a % b) != 0 && ((a < 0) != (b < 0))) q--;
    return q;
}
inline int floorMod(int a, int b) {
    int r = a % b;
    if (r != 0 && ((a < 0) != (b < 0))) r += b;
    return r;
}

// 方块坐标 → 区块坐标（向下取整）。
inline int blockToChunkCoord(int v) { return floorDiv(v, CHUNK_SIZE); }
// 方块坐标 → 区块内局部坐标，恒在 [0, CHUNK_SIZE)。
inline int blockToChunkLocal(int v) { return floorMod(v, CHUNK_SIZE); }
// 方块在单区块数组里的下标：y 是最高位，与 Chunk::blocks 的遍历顺序一致。
inline int chunkIndex(int lx, int y, int lz) { return lx + (lz << 4) + (y << 8); }

// 标准六邻域方向，顺序 +X/-X/+Y/-Y/+Z/-Z
// mesher 的 kNormal、方块面枚举 F_* 都按这个顺序，别单独再定义一份。
constexpr int kDir6[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};

// 标准水平四邻域，顺序北/东/南/西，与 kDir6 的水平部分同序。
constexpr int kDir4[4][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
