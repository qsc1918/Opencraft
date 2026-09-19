// build/meshdiag.cpp - dump nether chunk mesh stats (temporary)
#include "blocks.hpp"
#include "dimensions.hpp"
#include "generator.hpp"
#include "mesher.hpp"
#include "specs.hpp"
#include "world.hpp"
#include <cstdio>
#include <map>
#include <vector>

int main() {
    // 生成 3x3 区块，拼出一个 MeshView（与 World::meshChunk 的做法一致）
    static uint8_t chunks[3][3][CHUNK_VOL];
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            gen::generateForDim(DIM_NETHER, 1337, i - 1, j - 1, chunks[i][j]);

    MeshView view;
    view.blocks.fill(B_AIR);
    auto src = [&](int lx, int lz) -> uint8_t* {
        return chunks[lx + 1][lz + 1];
    };
    for (int y = 0; y < WORLD_HEIGHT; y++)
        for (int z = 0; z < 16; z++)
            for (int x = 0; x < 16; x++)
                view.set(x, y, z, src(0, 0)[x + (z << 4) + (y << 8)]);
    for (int y = 0; y < WORLD_HEIGHT; y++) {
        for (int z = 0; z < 16; z++) {
            view.set(16, y, z, chunks[1][1][0 + (z << 4) + (y << 8)]);
            view.set(-1, y, z, chunks[-1 + 1][1][15 + (z << 4) + (y << 8)]);
        }
        for (int x = 0; x < 16; x++) {
            view.set(x, y, 16, chunks[1][2][x + (0 << 4) + (y << 8)]);
            view.set(x, y, -1, chunks[1][0][x + (15 << 4) + (y << 8)]);
        }
    }

    ChunkMeshData m = buildChunkMesh(view);
    printf("opaque verts=%zu idx=%zu  water verts=%zu idx=%zu\n",
           m.opaqueVerts.size(), m.opaqueIdx.size(), m.waterVerts.size(), m.waterIdx.size());
    int minY = 999, maxY = -999, minX = 999, maxX = -999, minZ = 999, maxZ = -999, negY = 0;
    std::map<int, int> texCount;
    for (const auto& v : m.opaqueVerts) {
        if (v.y < minY) minY = v.y;
        if (v.y > maxY) maxY = v.y;
        if (v.x < minX) minX = v.x;
        if (v.x > maxX) maxX = v.x;
        if (v.z < minZ) minZ = v.z;
        if (v.z > maxZ) maxZ = v.z;
        if (v.y < 0) negY++;
        texCount[v.tex]++;
    }
    printf("vertex x[%d..%d] y[%d..%d] z[%d..%d]  negative-y verts=%d\n",
           minX, maxX, minY, maxY, minZ, maxZ, negY);
    for (auto& kv : texCount)
        printf("  tex %2d (%s): %d verts\n", kv.first,
               kv.first < B_COUNT ? BLOCK_DEFS[kv.first].name : "item/other", kv.second);
    return 0;
}
