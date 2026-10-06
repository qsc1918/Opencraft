// meshtest.cpp — 网格化自检（开发用，不入包）
//  1) 内部面/邻接面剔除：实心体、相邻方块之间不该出面；
//  2) 贪心合并：大平面合并成少量矩形，但**覆盖的总面积必须一格不差**；
//  3) 合并的前提：整方块 + 整张贴图 + 四角亮度相等，缺一即逐格出；
//  4) 水的叠层只留最上层顶面。
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

// 填充方块：坐标含四周邻居范围 x,z ∈ [-1,16]（越界会写坏 MeshView，调用方自己保证）
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

// 四边形个数（合并后）与总覆盖面积（以"单个方块面"为单位，合并不该改变它）
struct Geom {
    long long faces = 0;
    long long area = 0;
    bool sane = true;
};
static Geom measureOpen(const std::vector<TerrainVertex>& verts, const std::vector<uint32_t>& idx) {
    Geom g;
    if (idx.size() % 6 != 0) { g.sane = false; return g; }
    for (size_t q = 0; q + 6 <= idx.size(); q += 6) {
        if (idx[q + 1] != idx[q] + 1 || idx[q + 2] != idx[q] + 2 || idx[q + 3] != idx[q] ||
            idx[q + 4] != idx[q + 2] || idx[q + 5] != idx[q] + 3)
            g.sane = false;
        g.faces++;
        const TerrainVertex* p = &verts[idx[q]];
        bool flat = false;
        long long area2 = 1;
        for (int a = 0; a < 3; a++) {
            int lo = coordOf(p[0], a), hi = coordOf(p[0], a);
            for (int k = 1; k < 4; k++) {
                int c = coordOf(p[k], a);
                if (c < lo) lo = c;
                if (c > hi) hi = c;
            }
            if (lo == hi) flat = true; else area2 *= (hi - lo);   // 单位 1/16 格
        }
        if (!flat) g.sane = false;   // 必须有一条轴恒定，否则不是轴对齐的面
        g.area += area2 / (16 * 16); // 换算成"多少个方块面"
    }
    return g;
}
static Geom opaqueGeom(const ChunkMeshData& m) { return measureOpen(m.opaqueVerts, m.opaqueIdx); }

// 16 格宽受顶点 uv 是 uint8 限制：单方向最多铺 15 格，剩一条窄边单独出
// （合并，顶点里 1 格 = 16 单位，255/16 = 15）
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
        Geom g = opaqueGeom(m);
        checkEq(quads(m), 6, "不透明四边形数（六面各一张）");
        checkEq(g.area, 6, "覆盖面积（合并前也是 6）");
        checkEq((long long)m.opaqueVerts.size(), 24, "顶点数");
        check(g.sane, "每个四边形四点共面且轴对齐");
    }

    printf("== 实心柱：内部面全剔，外表面合并成矩形 ==\n");
    {
        MeshView v;
        v.blocks.fill(B_AIR);
        // 连四周邻居一起填满，这样区块内每个方块的六面邻居都是石头
        fillBox(v, -1, 0, -1, 16, WORLD_HEIGHT - 1, 16, B_STONE);
        ChunkMeshData m = buildChunkMesh(v);
        Geom g = opaqueGeom(m);
        checkEq(g.area, 512, "覆盖面积（只剩 y=0 底面与 y=127 顶面各 16x16）");
        checkEq(quads(m), 8, "四边形数（每层合并成 15x15 + 三条边，共 4 张）");
        checkEq((long long)m.opaqueVerts.size(), 32, "顶点数");
        check(g.sane, "每个四边形四点共面且轴对齐");
    }

    printf("== 相邻方块：贴合面互相剔除 ==\n");
    {
        MeshView v;
        v.blocks.fill(B_AIR);
        v.set(8, 8, 8, B_STONE);
        v.set(9, 8, 8, B_STONE);
        ChunkMeshData m = buildChunkMesh(v);
        Geom g = opaqueGeom(m);
        checkEq(g.area, 10, "覆盖面积（12 面扣掉贴合的 2 面）");
        checkEq(quads(m), 6, "四边形数（上下与南北各自合并）");
    }
    {
        MeshView v;
        v.blocks.fill(B_AIR);
        // 玻璃不是不透明方块，但同类之间仍要剔（否则玻璃墙内部一片几乎看不见的面）
        v.set(8, 8, 8, B_GLASS);
        v.set(9, 8, 8, B_GLASS);
        ChunkMeshData m = buildChunkMesh(v);
        checkEq(opaqueGeom(m).area, 10, "相邻两格玻璃：贴合面剔除");
    }

    printf("== 合并前提：贴图不同不合并 ==\n");
    {
        MeshView v;
        v.blocks.fill(B_AIR);
        fillBox(v, 0, 8, 0, 15, 8, 0, B_STONE);   // 一条东西向的石排
        v.set(3, 8, 0, B_DIRT);                   // 中间换一块泥土，把 A A 面劈成两段
        ChunkMeshData m = buildChunkMesh(v);
        // 东西向一排 16 块：上下南北各 16 张，只有两端的 ±X 露出来 → 16*4+2
        checkEq(opaqueGeom(m).area, 66, "覆盖面积（±X 只有排的两端露出来）");
        check(quads(m) > 6, "贴图不同导致合并被打断（四边形多于全合并的 6 张）");
    }

    printf("== 合并前提：四角 AO 不等不合并 ==\n");
    {
        MeshView v;
        v.blocks.fill(B_AIR);
        fillBox(v, 0, 8, 0, 15, 8, 15, B_STONE);  // 一层石地板
        ChunkMeshData m = buildChunkMesh(v);
        const long long floorArea = opaqueGeom(m).area;
        const long long floorQuads = quads(m);
        v.set(5, 9, 5, B_STONE);                  // 地板上放一块，周围一圈顶面 AO 变差
        ChunkMeshData m2 = buildChunkMesh(v);
        // 新方块自己露出 5 个面（底面被脚下的地板剔掉），同时盖掉地板的 1 张顶面
        checkEq(opaqueGeom(m2).area, floorArea + 4, "地板上多一块方块：净增 4 个面");
        check(quads(m2) > floorQuads, "AO 变化处的面不再参与合并");
    }

    printf("== 多 element 模型不参与合并（火把）==\n");
    {
        MeshView v;
        v.blocks.fill(B_AIR);
        v.set(8, 8, 8, B_TORCH);
        ChunkMeshData m = buildChunkMesh(v);
        checkEq(quads(m), 6, "火把：底面+焰心+四个杆面，逐张出");
        check(opaqueGeom(m).area < 6, "火把面比整方块小，面积自然更少");
    }

    printf("== 合并后的 uv 随跨度线性铺开 ==\n");
    {
        MeshView v;
        v.blocks.fill(B_AIR);
        fillBox(v, 0, 8, 0, 2, 8, 0, B_STONE);   // 3 格宽的一条石排
        ChunkMeshData m = buildChunkMesh(v);
        long long wide = 0;
        for (size_t q = 0; q + 6 <= m.opaqueIdx.size(); q += 6) {
            const TerrainVertex* p = &m.opaqueVerts[m.opaqueIdx[q]];
            int lo = 1 << 30, hi = -1;
            for (int k = 0; k < 4; k++) {
                int c = p[k].x;
                if (c < lo) lo = c;
                if (c > hi) hi = c;
            }
            if (hi - lo == 3 * 16) { wide++; for (int k = 0; k < 4; k++) {
                check(p[k].u == 0 || p[k].u == 48, "3 格宽面的 u 取 0 或 48（1 格 = 16）");
                check(p[k].w == 1 || p[k].w == 3, "跨了 1 格以上要打上 uv 平铺标记");
            }}
        }
        check(wide >= 2, "至少顶面/底面/侧面里有横跨 3 格的合并面");
    }

    printf("== 水面：同种流体叠层只留最上层顶面 ==\n");
    {
        MeshView v;
        v.blocks.fill(B_AIR);
        fillBox(v, 0, 62, 0, 15, 62, 15, B_WATER);
        ChunkMeshData m = buildChunkMesh(v);
        checkEq(measureOpen(m.waterVerts, m.waterIdx).area, 16 * 16, "单层水：每格一张顶面");
        checkEq(waterQuads(m), 4, "合并后只剩 4 张（15x15 + 三条边）");
        fillBox(v, 0, 63, 0, 15, 63, 15, B_WATER);
        m = buildChunkMesh(v);
        checkEq(measureOpen(m.waterVerts, m.waterIdx).area, 16 * 16, "再叠一层水：仍然只有一层顶面");
    }
    {
        MeshView v;
        v.blocks.fill(B_AIR);
        fillBox(v, 0, 62, 0, 15, 62, 15, B_WATER);
        fillBox(v, 0, 63, 0, 15, 63, 15, B_STONE);
        ChunkMeshData m = buildChunkMesh(v);
        checkEq(waterQuads(m), 0, "水上方是不透明方块：顶面剔除");
    }
    {
        MeshView v;
        v.blocks.fill(B_AIR);
        // 高度差了两格的水面不能并成一张（不共面）
        fillBox(v, 0, 62, 0, 15, 62, 15, B_WATER);
        v.set(3, 63, 3, B_AIR);
        fillBox(v, 3, 62, 3, 3, 62, 3, B_WATER);
        ChunkMeshData m = buildChunkMesh(v);
        checkEq(measureOpen(m.waterVerts, m.waterIdx).area, 16 * 16, "水面覆盖面积不变");
    }

    printf("== 遮挡剔除：分段共享面的可见性 ==\n");
    {
        MeshView v;
        v.blocks.fill(B_AIR);
        // 连四周邻居一起填满：每个分段的六个面两边全是不透明
        fillBox(v, -1, 0, -1, 16, WORLD_HEIGHT - 1, 16, B_STONE);
        FaceVisibility vis = buildFaceMask(v, WORLD_HEIGHT - 1);
        bool allPassZero = vis.pass == 0;
        bool allSelfOp = true;
        for (int s = 0; s < 8; s++) {
            for (int d = 0; d < 4; d++)
                if (!(vis.selfOp & chunkFaceBit(s, d))) allSelfOp = false;
            if (s < 7 && !(vis.selfOp & chunkFaceBit(s, 4))) allSelfOp = false;
        }
        check(allPassZero, "整块实心：没有任何一面能穿过");
        check(allSelfOp, "整块实心：每个面的我方一侧都不透明（墙后可剔）");
    }
    {
        MeshView v;
        v.blocks.fill(B_AIR);
        fillBox(v, -1, 0, -1, 16, WORLD_HEIGHT - 1, 16, B_STONE);
        // 在第 3 段（y=48..63）的 +X 面上开一个洞：那面就能穿过、也不再是整面实心
        v.set(15, 55, 3, B_AIR);
        v.set(16, 55, 3, B_AIR);
        FaceVisibility vis = buildFaceMask(v, WORLD_HEIGHT - 1);
        check((vis.pass & chunkFaceBit(3, 0)) != 0, "墙上开洞：+X 面可以穿过");
        check((vis.selfOp & chunkFaceBit(3, 0)) == 0, "墙上开洞：+X 面我方一侧不再是整面实心");
        check((vis.pass & chunkFaceBit(2, 0)) == 0, "没开洞的其他段仍然穿不过");
    }
    {
        MeshView v;
        v.blocks.fill(B_AIR);
        // 只有 y=0 一层石头：最低那段的底面（与世界下界之间）应当算"能穿过"
        fillBox(v, 0, 0, 0, 15, 0, 15, B_STONE);
        FaceVisibility vis = buildFaceMask(v, 0);
        check((vis.pass & chunkFaceBit(1, 4)) != 0, "上面那段与这段之间是空的：能穿过");
        check((vis.selfOp & chunkFaceBit(0, 4)) == 0, "这段顶面之上是空气：不算整面实心");
    }

    printf("\n%s（失败 %d 项）\n", g_fail == 0 ? "全部通过" : "有失败用例", g_fail);
    return g_fail == 0 ? 0 : 1;
}
