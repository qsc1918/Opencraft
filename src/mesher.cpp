#include "mesher.hpp"
#include "blocks.hpp"
#include <algorithm>
#include <cmath>

namespace {

// 预计算面表，下标顺序必须与 Face 枚举一致：
// F_PX=0, F_NX=1, F_PY=2, F_NY=3, F_PZ=4, F_NZ=5
const int kC[6][4][3] = {
    {{1,0,1},{1,0,0},{1,1,0},{1,1,1}}, // +X 面
    {{0,0,0},{0,0,1},{0,1,1},{0,1,0}}, // -X 面
    {{0,1,1},{1,1,1},{1,1,0},{0,1,0}}, // +Y 面
    {{1,0,1},{0,0,1},{0,0,0},{1,0,0}}, // -Y 面（绕序反转，否则从下方看被剔除）
    {{0,0,1},{1,0,1},{1,1,1},{0,1,1}}, // +Z 面
    {{1,0,0},{0,0,0},{0,1,0},{1,1,0}}, // -Z 面
};
const int kU[6][4] = {
    {15,0,0,15}, {0,15,15,0}, {0,15,15,0}, {15,0,0,15}, {0,15,15,0}, {15,0,0,15},
};
const int kV[6][4] = {
    {15,15,0,0}, {15,15,0,0}, {15,15,0,0}, {15,15,0,0}, {15,15,0,0}, {15,15,0,0},
};
const int kA1[6] = {2,2,0,0,0,0};
const int kA2[6] = {1,1,2,2,1,1};
const int kNormal[6][3] = {
    {1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1},
};

// me 朝 nb 的面是否需要剔除（同类玻璃/树叶之间也剔除）。
inline bool cullFace(uint8_t me, uint8_t nb) {
    if (me == B_GLASS && nb == B_GLASS) return true;
    if (me == B_LEAVES && nb == B_LEAVES) return true;
    if (blockIsOpaque(nb)) return true;
    return false;
}

inline bool waterCull(uint8_t nb) {
    return nb == B_WATER || blockIsOpaque(nb);
}

// 流体只保留最上面那一层的顶面：同种流体叠在一起时必须剔除，
// 否则整片岩浆海每格都出一张共面顶面，会互相 z-fighting 出条纹。
inline bool fluidCull(uint8_t nb) {
    return nb == B_WATER || nb == B_LAVA || blockIsOpaque(nb);
}

const float kAoBright[4] = {0.42f, 0.64f, 0.82f, 1.0f};
const float kFaceBright[6] = {0.80f, 0.80f, 1.0f, 0.55f, 0.80f, 0.80f};
const Vec3 kSun(0.42f, 0.82f, 0.32f);

// 预计算亮度查表：kShade[面][AO][是否水面]
uint8_t kShade[6][4][2];
struct ShadeInit { ShadeInit() {
    for (int f = 0; f < 6; f++) {
        const Vec3& nrm = *reinterpret_cast<const Vec3*>(&kNormal[f][0]);
        float sun = 0.55f + 0.45f * std::max(0.0f, dot(nrm, kSun) * 0.5f + 0.5f);
        for (int a = 0; a < 4; a++) {
            float br = kFaceBright[f] * kAoBright[a] * sun;
            kShade[f][a][0] = (uint8_t)(std::min(1.0f, std::max(0.0f, br)) * 255.0f);
            float bw = br * 0.82f;
            kShade[f][a][1] = (uint8_t)(std::min(1.0f, std::max(0.0f, bw)) * 255.0f);
        }
    }
} } shadeInit;

// 自发光方块（岩浆/萤石/传送门/火等）不做面光衰减，直接满亮
inline bool emissiveBlock(uint8_t id) {
    return id < B_COUNT && BLOCK_DEFS[id].lightEmission >= 10;
}

inline uint8_t bakeShade(int face, int ao, bool water) {
    return kShade[face][ao][water ? 1 : 0];
}

} // 匿名命名空间

ChunkMeshData buildChunkMesh(const MeshView& view) {
    ChunkMeshData m;
    auto& ov = m.opaqueVerts;
    auto& oi = m.opaqueIdx;
    auto& wv = m.waterVerts;
    auto& wi = m.waterIdx;
    ov.reserve(8192);
    oi.reserve(12288);
    wv.reserve(512);
    wi.reserve(768);

    // 先找最高非空气方块，跳过其上必定为空的区域。
    int maxY = 0;
    for (int z = 0; z < CHUNK_SIZE; z++)
        for (int x = 0; x < CHUNK_SIZE; x++)
            for (int y = WORLD_HEIGHT - 1; y > maxY; y--)
                if (view.at(x, y, z) != B_AIR) { maxY = y; break; }

    for (int y = 0; y <= maxY + 1 && y < WORLD_HEIGHT; y++) {
        for (int z = 0; z < CHUNK_SIZE; z++) {
            for (int x = 0; x < CHUNK_SIZE; x++) {
                uint8_t id = view.at(x, y, z);
                if (id == B_AIR) continue;

                if (id == B_WATER || id == B_LAVA) {
                    // 只出顶面：侧面/底面透过半透明水面看不见，
                    // 还会产生发暗的瑕疵。
                    if (fluidCull(view.at(x, y + 1, z))) continue;
                    {
                        int face = F_PY;
                        uint32_t base = (uint32_t)wv.size();
                        for (int c = 0; c < 4; c++) {
                            TerrainVertex vt;
                            vt.x = (int8_t)(x + kC[face][c][0]);
                            vt.y = (int8_t)(y + kC[face][c][1]);
                            vt.z = (int8_t)(z + kC[face][c][2]);
                            vt.u = (uint8_t)kU[face][c];
                            vt.v = (uint8_t)kV[face][c];
                            vt.tex = (id == B_LAVA) ? T_LAVA : T_WATER;
                            vt.shade = emissiveBlock(id) ? 255 : bakeShade(face, 3, true);
                            wv.push_back(vt);
                        }
                        wi.push_back(base);
                        wi.push_back(base + 1);
                        wi.push_back(base + 2);
                        wi.push_back(base);
                        wi.push_back(base + 2);
                        wi.push_back(base + 3);
                    }
                    continue;
                }

                if (!blockIsRenderable(id)) continue;

                // 快速剔除：六面全被遮挡
                bool anyExposed = false;
                for (int f = 0; f < 6 && !anyExposed; f++) {
                    int dx = kNormal[f][0], dy = kNormal[f][1], dz = kNormal[f][2];
                    if (!cullFace(id, view.at(x + dx, y + dy, z + dz))) anyExposed = true;
                }
                if (!anyExposed) continue;

                uint8_t tile = blockTile(id, 0);
                for (int f = 0; f < 6; f++) {
                    // 世界最高层的顶面顶点会算到 y=128，而顶点 y 是 int8_t，
                    // 溢出成 -128 会生成跨越整个世界高度的巨型面（下界天花板
                    // 全是基岩，就会变成满屏条纹）。顶层顶面本来也看不见。
                    if (f == F_PY && y >= WORLD_HEIGHT - 1) continue;
                    int dx = kNormal[f][0], dy = kNormal[f][1], dz = kNormal[f][2];
                    uint8_t nb = view.at(x + dx, y + dy, z + dz);
                    if (cullFace(id, nb)) continue;

                    uint8_t fTile = blockTile(id, f);
                    uint32_t base = (uint32_t)ov.size();
                    for (int c = 0; c < 4; c++) {
                        TerrainVertex vt;
                        vt.x = (int8_t)(x + kC[f][c][0]);
                        vt.y = (int8_t)(y + kC[f][c][1]);
                        vt.z = (int8_t)(z + kC[f][c][2]);
                        vt.u = (uint8_t)kU[f][c];
                        vt.v = (uint8_t)kV[f][c];
                        vt.tex = fTile;
                        // AO：在面外侧那一层（法线方向偏移一格）取该角点的
                        // 两条边邻居 + 对角邻居。旧实现取在本层且整体偏了一格，
                        // 会让大片平面（如下界基岩天花板）出现条纹状明暗。
                        int a1 = kA1[f], a2 = kA2[f];
                        int o1 = kC[f][c][a1] == 1 ? 1 : -1;
                        int o2 = kC[f][c][a2] == 1 ? 1 : -1;
                        int co[3] = {0, 0, 0};
                        co[a1] = o1;
                        int s1x = x + kNormal[f][0] + co[0];
                        int s1y = y + kNormal[f][1] + co[1];
                        int s1z = z + kNormal[f][2] + co[2];
                        co[a1] = 0; co[a2] = o2;
                        int s2x = x + kNormal[f][0] + co[0];
                        int s2y = y + kNormal[f][1] + co[1];
                        int s2z = z + kNormal[f][2] + co[2];
                        co[a1] = o1; co[a2] = o2;
                        int dxx = x + kNormal[f][0] + co[0];
                        int dyy = y + kNormal[f][1] + co[1];
                        int dzz = z + kNormal[f][2] + co[2];

                        bool s1 = blockIsOpaque(view.at(s1x, s1y, s1z));
                        bool s2 = blockIsOpaque(view.at(s2x, s2y, s2z));
                        bool dd = blockIsOpaque(view.at(dxx, dyy, dzz));
                        int ao;
                        if (s1 && s2) ao = 0;
                        else ao = 3 - ((s1 ? 1 : 0) + (s2 ? 1 : 0) + (dd ? 1 : 0));
                        vt.shade = emissiveBlock(id) ? 255 : bakeShade(f, ao, false);
                        ov.push_back(vt);
                    }
                    oi.push_back(base);
                    oi.push_back(base + 1);
                    oi.push_back(base + 2);
                    oi.push_back(base);
                    oi.push_back(base + 2);
                    oi.push_back(base + 3);
                }
            }
        }
    }
    return m;
}
