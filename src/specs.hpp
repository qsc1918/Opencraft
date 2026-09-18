#pragma once
// ===========================================================================
// specs.hpp — Opencraft 世界规范常量（对齐 Minecraft Java 版子集）
//
// 所有世界结构数值的唯一来源；与 MC 不同的取值是刻意简化。
// 完整规范见 docs/standards.md
// ===========================================================================
#include <cstdint>

// ---------------------------------------------------------------------------
// 区块与子区块
// MC: 区块 16×16，子区块 16×16×16，1.18+ 区块列 24 层。
// 本实现: 区块列 16×128×16，共 8 层，尚无独立 section 存储。
// ---------------------------------------------------------------------------
constexpr int CHUNK_SIZE   = 16;  // 区块水平边长（MC 规范值，勿改）
constexpr int WORLD_HEIGHT = 128; // 区块列高度（MC 1.18+: 384，此处简化）
constexpr int CHUNK_VOL    = CHUNK_SIZE * WORLD_HEIGHT * CHUNK_SIZE; // 单区块方块数 32768

constexpr int SECTION_SIZE  = 16; // 子区块边长（MC 规范值）
constexpr int SECTION_VOL   = SECTION_SIZE * SECTION_SIZE * SECTION_SIZE; // 4096
constexpr int SECTION_COUNT = WORLD_HEIGHT / SECTION_SIZE; // 子区块层数（MC 1.18+: 24）

// ---------------------------------------------------------------------------
// 高度与海平面
// MC: 主世界可建造 Y∈[-64,319]，海平面 63。
// 本实现: Y∈[0,127]，海平面 62。
// ---------------------------------------------------------------------------
constexpr int WORLD_MIN_Y = 0;                 // Y 下限（MC 1.18+: -64）
constexpr int WORLD_MAX_Y = WORLD_HEIGHT - 1;  // Y 上限（MC 1.18+: 319）
constexpr int SEA_LEVEL   = 62;                // 海平面（MC: 63）

// ---------------------------------------------------------------------------
// 光照
// MC: 方块光与天空光两条独立通道，各 0..15 共 16 级。
// 本实现: 光照引擎未启用（用烘焙面亮度），以下常量预留。
// ---------------------------------------------------------------------------
constexpr int LIGHT_LEVELS = 16; // 光照级数（0..15）
constexpr int MAX_LIGHT    = 15; // 最高光照等级

// ---------------------------------------------------------------------------
// 游戏刻
// MC: 20 TPS，每刻 50ms；实体/AI/方块更新按刻模拟。
// 本实现: 主循环为逐帧可变 dt（上限 0.05s）。
// ---------------------------------------------------------------------------
constexpr int   TICK_RATE = 20;             // 每秒刻数（MC: 20）
constexpr float TICK_DT   = 1.0f / 20.0f;   // 单刻步长（秒），50ms

// ---------------------------------------------------------------------------
// 世界边界
// MC: 默认直径 29,999,984（单侧 ±14,999,992），硬上限 60,000,000。
// 本实现: 暂不强制边界，常量预留。
// ---------------------------------------------------------------------------
constexpr int WORLD_BORDER_RADIUS = 14999992;  // MC 默认边界单侧半径
constexpr int WORLD_BORDER_MAX    = 30000000;  // MC 硬上限单侧半径

// ---------------------------------------------------------------------------
// 坐标换算（规范见 docs/standards.md §坐标）
// MC: 区块坐标 = floor(方块坐标 / 16)，负坐标同样向下取整。
// ---------------------------------------------------------------------------
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

// 方块坐标 → 区块坐标（向下取整）
inline int blockToChunkCoord(int v) { return floorDiv(v, CHUNK_SIZE); }
// 方块坐标 → 区块内局部坐标，恒在 [0, CHUNK_SIZE)。
inline int blockToChunkLocal(int v) { return floorMod(v, CHUNK_SIZE); }
// 世界 Y → 子区块层序号（0 起）
inline int blockToSectionIndex(int y) { return floorDiv(y, SECTION_SIZE); }
