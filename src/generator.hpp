#pragma once
#include "dimensions.hpp"
#include <cstdint>

namespace gen {
// Generates a full chunk column (16 x WORLD_HEIGHT x 16) of block ids.
// Deterministic given (seed, cx, cz). Out points to CHUNK_VOL bytes.
void generateColumn(uint32_t seed, int cx, int cz, uint8_t* out);

// Generates a nether chunk column (old design: 3D noise caverns, lava sea).
void generateNether(uint32_t seed, int cx, int cz, uint8_t* out);

// Generates an end chunk column (main island + pillars + exit portal).
void generateEnd(uint32_t seed, int cx, int cz, uint8_t* out);

// Dispatch by dimension.
void generateForDim(DimensionId dim, uint32_t seed, int cx, int cz, uint8_t* out);
}
