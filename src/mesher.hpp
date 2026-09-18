#pragma once
#include "world.hpp"

// 用区块本地方块视图构建不透明层与水面网格数据。
ChunkMeshData buildChunkMesh(const MeshView& view);
