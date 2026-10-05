// meshtest.cpp — 网格化自检（开发用，不入包）
//  1) 内部面/邻接面剔除：实心体、相邻方块之间不该出面；
//  2) 输出结构的合法性：每个四边形必须四点共面且轴对齐（能被贪心合并的前提）；
//  3) 水：同种流体叠层时只出最上面那层的顶面。
// 期望值全部写死在下面（不用被测函数生成期望）。
// 用法: meshtest
#include "mesher.hpp"
#include "world.hpp"
#include <cmath>
#include <cstdio>

static int g_fail = 0;
static void checkEq(long long got, long long want, const char* what) {
    printf("  [%s] %s = %lld（期望 %lld）\n", got == want ? "OK" : "FAIL", what, got, want);
    if (got != want) g_fail++;
}
static void check(bool ok, const char* what) {
    printf("  [%s] %s\n", ok ? "OK" : "FAIL", what);
    if (!ok) g_fail++;
}

// 填充方块：坐标含四周邻居范围 x,z ∈ [-1,16]（越界会写坏 MeshView，故调用方自己保证）
static void fillBox(MeshView& v, int x0, int y0, int z0, int x1, int y1, int z1, uint8_t id) {
    for (int y = y0; y <= y1; y++)
        for (int z = z0; z <= z1; z++)
            for (int x = x0; x <= x1; x++)
                v.set(x, y, z, id);
}

static long long quads(const ChunkMeshData& m) { return (long long)m.opaqueIdx.size() / 6; }
static long long waterQuads(const ChunkMeshData& m) { return (long long)m.waterIdx.size() / 6; }

static int16_t coordOf(const TerrainVertex& v, int axis) {
    return axis == 0 ? v.x : (axis == 1 ? v.y : v.z);
}

// 每个四边形必须四点共面且有一条轴恒定（轴对齐立方体面）
static bool geometrySane(const ChunkMeshData& m) {
    const size_t n = m.opaqueIdx.size();
    if (n % 6 != 0) return false;
    for (size_t q = 0; q < n; q += 6) {
        if (m.opaqueIdx[q + 0] + 1 != m.opaqueIdx[q + 1]) return false;
        if (m.opaqueIdx[q + 1] + 1 != m.opaqueIdx[q + 2]) return false;
        if (m.opaqueIdx[q + 3] != m.opaqueIdx[q + 0]) return false;
        if (m.opaqueIdx[q + 4] != m.opaqueIdx[q + 2]) return false;
        if (m.opaqueIdx[q + 5] != m.opaqueIdx[q + 0] + 3) return false;
        const TerrainVertex* p = &m.opaqueVerts[m.opaqueIdx[q]];
        // 四个顶点里必有一条轴坐标全同：否则说明这条轴并不是法线，无法参与合并
        bool flat = false;
        for (int a = 0; a < 3; a++) {
            int16_t vals[4] = {coordOf(p[0], a), coordOf(p[1], a), coordOf(p[2], a), coordOf(p[3], a)};
            bool same = true;
            for (int k = 1; k < 4; k++) if (vals[k] != vals[0]) same = false;
            if (same) flat = true;
        }
        if (!flat) return false;
    }
    return true;
}

int main() {
    printf("== 空区块 ==\n");
    {
        MeshView v;
        v.blocks.fill(B_AIR);
        ChunkMeshData m = buildChunkMesh(v);
        checkEq(quads(m), 0, "不透明四边形数");
        checkEq(waterQuads(m), 0, "水面四边形数");
    }

    printf("== 孤立方块 ==\n");
    {
        MeshView v;
        v.blocks.fill(B_AIR);
        v.set(8, 8, 8, B_STONE);
        ChunkMeshData m = buildChunkMesh(v);
        checkEq(quads(m), 6, "不透明四边形数（六面各一张）");
        checkEq((long long)m.opaqueVerts.size(), 24, "顶点数");
        check(geometrySane(m), "每个四边形四点共面且轴对齐");
    }

    printf("== 实心柱：内部面必须全剔掉 ==\n");
    {
        MeshView v;
        v.blocks.fill(B_AIR);
        // 连四周邻居一起填满，这样区块内每个方块的六面邻居都是石头
        fillBox(v, -1, 0, -1, 16, WORLD_HEIGHT - 1, 16, B_STONE);
        ChunkMeshData m = buildChunkMesh(v);
        // 只剩 y=0 的底面和 y=127 的顶面各 16x16 张 —— 一张内部面都不该有
        checkEq(quads(m), 512, "不透明四边形数（仅上下两处外表面）");
        checkEq((long long)m.opaqueVerts.size(), 512 * 4, "顶点数");
    }

    printf("== 相邻方块：贴合面互相剔除 ==\n");
    {
        MeshView v;
        v.blocks.fill(B_AIR);
        v.set(8, 8, 8, B_STONE);
        v.set(9, 8, 8, B_STONE);
        ChunkMeshData m = buildChunkMesh(v);
        checkEq(quads(m), 10, "两个方块 12 面扣掉贴合的 2 面");
    }
    {
        MeshView v;
        v.blocks.fill(B_AIR);
        // 玻璃不是不透明方块，但同类之间仍要剔（否则玻璃墙内部一片片几乎看不见的面）
        v.set(8, 8, 8, B_GLASS);
        v.set(9, 8, 8, B_GLASS);
        ChunkMeshData m = buildChunkMesh(v);
        checkEq(quads(m), 10, "相邻两格玻璃扣掉贴合面");
    }

    printf("== 水面：同种流体叠层只留最上层顶面 ==\n");
    {
        MeshView v;
        v.blocks.fill(B_AIR);
        fillBox(v, 0, 62, 0, 15, 62, 15, B_WATER);
        ChunkMeshData m = buildChunkMesh(v);
        checkEq(waterQuads(m), 16 * 16, "单层水：每格一张顶面");
        fillBox(v, 0, 63, 0, 15, 63, 15, B_WATER);
        m = buildChunkMesh(v);
        checkEq(waterQuads(m), 16 * 16, "再叠一层水：仍然只有一层顶面（不会 z-fighting）");
    }
    {
        MeshView v;
        v.blocks.fill(B_AIR);
        fillBox(v, 0, 62, 0, 15, 62, 15, B_WATER);
        fillBox(v, 0, 63, 0, 15, 63, 15, B_STONE);
        ChunkMeshData m = buildChunkMesh(v);
        checkEq(waterQuads(m), 0, "水上方是不透明方块：顶面剔除");
    }

    printf("\n%s（失败 %d 项）\n", g_fail == 0 ? "全部通过" : "有失败用例", g_fail);
    return g_fail == 0 ? 0 : 1;
}
