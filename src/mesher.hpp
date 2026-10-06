#pragma once
#include "world.hpp"

// 用区块本地方块视图构建不透明层与水面网格数据。
ChunkMeshData buildChunkMesh(const MeshView& view);

// 16^3 分段之间共享面的可见性（遮挡剔除用，位定义见 world.hpp 的 chunkFaceBit）
struct FaceVisibility {
    uint64_t pass = 0;    // 置位 = 面上有格子两边都不是不透明，视线能穿过
    uint64_t selfOp = 0;  // 置位 = 我方那侧整面不透明（邻居的表面同样看不见）
};
FaceVisibility buildFaceMask(const MeshView& view, int maxY);
