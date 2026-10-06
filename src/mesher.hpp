#pragma once
#include "world.hpp"

// 用区块本地方块视图构建不透明层与水面网格数据。
ChunkMeshData buildChunkMesh(const MeshView& view);

// 16^3 分段之间共享面"视线能不能穿过去"（遮挡剔除用，位定义见 world.hpp 的 chunkFaceBit）
uint64_t buildFaceMask(const MeshView& view, int maxY);
