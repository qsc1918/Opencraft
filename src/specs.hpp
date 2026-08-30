#pragma once
// ===========================================================================
// specs.hpp — VoxMine 世界规范常量（对齐 Minecraft Java 版规范的子集）
//
// 本文件是所有"世界结构"数值的唯一来源（Single Source of Truth）。
// 每个常量都标注了 Minecraft 的规范值；与 MC 不同的取值是当前实现的
// 刻意简化，演进路线见 docs/standards.md。
// 术语与完整规范文档：docs/standards.md
// ===========================================================================
#include <cstdint>

// ---------------------------------------------------------------------------
// 区块 (Chunk) 与子区块 (Section/Sub-chunk)
// MC 规范: 区块水平 16×16；子区块(section)为 16×16×16 立方；
//          Java 1.18+ 区块列 16×384×16 = 24 个 section。
// 本实现:  区块列 16×128×16 = 8 个 section 的数据量（尚无独立 section 存储）。
// ---------------------------------------------------------------------------
constexpr int CHUNK_SIZE   = 16;  // 区块水平边长（MC: 16，规范值，不可修改）
constexpr int WORLD_HEIGHT = 128; // 区块列高度（MC Java 1.18+: 384；本实现简化为 128）
constexpr int CHUNK_VOL    = CHUNK_SIZE * WORLD_HEIGHT * CHUNK_SIZE; // 单区块方块数 32768

constexpr int SECTION_SIZE  = 16; // 子区块边长（MC: 16，规范值）
constexpr int SECTION_VOL   = SECTION_SIZE * SECTION_SIZE * SECTION_SIZE; // 4096
constexpr int SECTION_COUNT = WORLD_HEIGHT / SECTION_SIZE; // 区块列的子区块层数（MC 1.18+: 24）

// ---------------------------------------------------------------------------
// 高度与海平面
// MC 规范: 主世界可建造 Y∈[-64,319]，海平面 sea level = 63。
// 本实现:  Y∈[0,127]，海平面 62（生成器以 SEA_LEVEL 为基准水位）。
// ---------------------------------------------------------------------------
constexpr int WORLD_MIN_Y = 0;                 // 世界 Y 下限（MC 1.18+: -64）
constexpr int WORLD_MAX_Y = WORLD_HEIGHT - 1;  // 世界 Y 上限（MC 1.18+: 319）
constexpr int SEA_LEVEL   = 62;                // 海平面（MC Java: 63）

// ---------------------------------------------------------------------------
// 光照 (Light)
// MC 规范: 方块光(block light)与天空光(sky light)两条独立通道，
//          取值均为 0..15 共 16 级；传播每格衰减 max(1, opacity)。
// 本实现:  光照引擎未启用（烘焙面亮度），以下常量为光照系统预留。
// ---------------------------------------------------------------------------
constexpr int LIGHT_LEVELS = 16; // 光照级数（0..15）
constexpr int MAX_LIGHT    = 15; // 最高光照等级

// ---------------------------------------------------------------------------
// 游戏刻 (Game Tick)
// MC 规范: 20 TPS（每秒 20 刻），每刻 50ms；实体/AI/方块更新按刻模拟。
// 本实现:  主循环为逐帧可变 dt（上限 0.05s）；联机与确定性模拟时以此为准。
// ---------------------------------------------------------------------------
constexpr int   TICK_RATE = 20;             // 每秒游戏刻（MC: 20）
constexpr float TICK_DT   = 1.0f / 20.0f;   // 单刻步长（秒），50ms

// ---------------------------------------------------------------------------
// 世界边界 (World Border)
// MC 规范: 默认中心 (0,0)，默认直径 29,999,984 方块（即每侧 ±14,999,992），
//          硬上限直径 60,000,000（每侧 ±30,000,000）。
// 本实现:  暂不强制边界，常量预留（存的是 MC 默认的单侧半径）。
// ---------------------------------------------------------------------------
constexpr int WORLD_BORDER_RADIUS = 14999992;  // MC 默认世界边界单侧半径
constexpr int WORLD_BORDER_MAX    = 30000000;  // MC 世界硬上限单侧半径

// ---------------------------------------------------------------------------
// 坐标换算（坐标系规范见 docs/standards.md §坐标）
// MC 规范: 区块坐标 = floor(方块坐标 / 16)；负坐标同样向下取整，
//          即 x=-1 属于区块 -1 的局部 x=15。
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

// 方块坐标 → 区块坐标。区块界限: 方块坐标除以 16 向下取整。
inline int blockToChunkCoord(int v) { return floorDiv(v, CHUNK_SIZE); }
// 方块坐标 → 区块内局部坐标，恒在 [0, CHUNK_SIZE)。
inline int blockToChunkLocal(int v) { return floorMod(v, CHUNK_SIZE); }
// 世界 Y → 子区块层数序号（0 起，最大 SECTION_COUNT-1）。
inline int blockToSectionIndex(int y) { return floorDiv(y, SECTION_SIZE); }
