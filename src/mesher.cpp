#include "mesher.hpp"
#include "blocks.hpp"
#include <algorithm>
#include <cmath>

namespace {

// 预计算面表，下标顺序必须与 Face 枚举一致：
// F_PX=0, F_NX=1, F_PY=2, F_NY=3, F_PZ=4, F_NZ=5
// 原版 FaceBakery 的角点顺序（每个角点分量 0=element 的 min、1=max）：
// 顶点 0..3 三角化为 0,1,2 / 0,2,3，法线 (p1-p0)×(p2-p0) 指向 element 外侧，
// 与管线的前面绕序一致。
const uint8_t kCorner[6][4][3] = {
    {{1,1,1},{1,0,1},{1,0,0},{1,1,0}}, // +X east
    {{0,1,0},{0,0,0},{0,0,1},{0,1,1}}, // -X west
    {{0,1,0},{0,1,1},{1,1,1},{1,1,0}}, // +Y up
    {{0,0,1},{0,0,0},{1,0,0},{1,0,1}}, // -Y down
    {{0,1,1},{0,0,1},{1,0,1},{1,1,1}}, // +Z south
    {{1,1,0},{1,0,0},{0,0,0},{0,1,0}}, // -Z north
};
// 原版 CuboidFace.UVs：顶点 0=(minU,minV) 1=(minU,maxV) 2=(maxU,maxV) 3=(maxU,minV)
const uint8_t kUvCorner[4][2] = {{0,0},{0,1},{1,1},{1,0}};

const int kNormal[6][3] = {
    {1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1},
};

// me 朝 nb 的面是否需要剔除（同类玻璃/树叶之间也剔除）。
// 邻居遮挡按"整面"判断：末地门框架只有 13/16 高，遮不住邻居的整个面，
// 所以它旁边的方块（比如框顶上方那块）该画的面必须画，否则会透出方块内部。
inline bool cullFace(uint8_t me, uint8_t nb) {
    if (me == B_GLASS && nb == B_GLASS) return true;
    if (me == B_LEAVES && nb == B_LEAVES) return true;
    // 两个框架贴着时遮挡形状相同，接触面重合，剔掉避免 z-fighting
    if (blockIsPortalFrame(me) && blockIsPortalFrame(nb)) return true;
    if (blockIsPortalFrame(nb)) return false;
    if (blockIsOpaque(nb)) return true;
    return false;
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

// ---------------------------------------------------------------------------
// 方块模型：一个方块由若干长方体（原版 element）拼成，坐标 1/16 格、0..16。
// 与原版一致，UV 与位置分开：每个面有自己的图块与 UV 矩形（0..16 像素），
// cullface 只写在贴在方块边界上的面上，没写的面（模型内部面）永不被邻居剔除。
// ---------------------------------------------------------------------------
struct BoxFace {
    uint8_t tile = T_WHITE;
    uint8_t u0 = 0, v0 = 0, u1 = 16, v1 = 16;
    bool present = true;   // 该面是否存在（原版模型里没写的面不画）
    bool cull = false;     // 原版 cullface：允许被相邻不透明整方块剔除
};

struct Box {
    int x0 = 0, y0 = 0, z0 = 0, x1 = 16, y1 = 16, z1 = 16;
    BoxFace face[6];
};

inline void setFace(Box& b, int f, uint8_t tile, uint8_t u0, uint8_t v0, uint8_t u1, uint8_t v1,
                    bool cull) {
    b.face[f].tile = tile;
    b.face[f].u0 = u0; b.face[f].v0 = v0; b.face[f].u1 = u1; b.face[f].v1 = v1;
    b.face[f].present = true;
    b.face[f].cull = cull;
}

// 该面是否贴在方块边界上（只有边界面的 cullface 才有意义，也是 AO 采样的前提）
inline bool onBoundary(const Box& b, int f) {
    switch (f) {
    case F_PX: return b.x1 == 16;
    case F_NX: return b.x0 == 0;
    case F_PY: return b.y1 == 16;
    case F_NY: return b.y0 == 0;
    case F_PZ: return b.z1 == 16;
    default:   return b.z0 == 0;
    }
}

// ---------------------------------------------------------------------------
// 末地传送门框架：原版 models/block/end_portal_frame(_filled).json
//  - element0 [0,0,0]->[16,13,16]：down=end_stone(cullface)、up=frame_top(无 cullface)、
//    四侧 uv [0,3,16,16]（侧面贴图顶部 3 行是透明的，正好跳过）(cullface 同名)。
//  - element1 [4,13,4]->[12,16,12]（仅"有眼"）：up uv [4,4,12,12](cullface up)、
//    四侧 uv [4,0,12,3]（无 cullface，绝不因邻居消失）、没有 down 面。
// 眼块是"居中"的 8×8 柱子、凸出框顶 3/16（对应原版 SHAPE_FULL），不随朝向偏移。
// ---------------------------------------------------------------------------
inline int buildParts(uint8_t id, Box* out) {
    if (blockIsPortalFrame(id)) {
        Box& b = out[0];
        b = Box{};
        b.y1 = 13;
        setFace(b, F_NY, T_END_STONE, 0, 0, 16, 16, true);
        setFace(b, F_PY, T_END_PORTAL_FRAME, 0, 0, 16, 16, false);
        for (int f : {F_PX, F_NX, F_PZ, F_NZ})
            setFace(b, f, T_END_PORTAL_FRAME_SIDE, 0, 3, 16, 16, true);
        if (!blockFrameHasEye(id)) return 1;

        Box& e = out[1];
        e = Box{};
        e.x0 = 4; e.x1 = 12; e.y0 = 13; e.y1 = 16; e.z0 = 4; e.z1 = 12;
        for (int f = 0; f < 6; f++) e.face[f].present = false;
        setFace(e, F_PY, T_END_PORTAL_FRAME_EYE, 4, 4, 12, 12, true);
        for (int f : {F_PX, F_NX, F_PZ, F_NZ})
            setFace(e, f, T_END_PORTAL_FRAME_EYE, 4, 0, 12, 3, false);
        return 2;
    }
    out[0] = Box{};
    for (int f = 0; f < 6; f++) setFace(out[0], f, blockTile(id, f), 0, 0, 16, 16, true);
    return 1;
}

inline uint8_t bakeShade(int face, int ao, bool water) {
    return kShade[face][ao][water ? 1 : 0];
}

// ---------------------------------------------------------------------------
// AO：在原版里是"面外侧那一层"（法线方向偏移一格）的角点邻居，
// 角点朝外的两个方向由该角落在 element 的 min 还是 max 侧决定。
// ---------------------------------------------------------------------------
inline int aoForFace(const MeshView& view, int x, int y, int z, int f, int c) {
    int na = f >> 1;                 // 法线轴：0=X,1=Y,2=Z（Face 枚举成对排列）
    int a1 = (na + 1) % 3, a2 = (na + 2) % 3;
    int o1 = kCorner[f][c][a1] ? 1 : -1;
    int o2 = kCorner[f][c][a2] ? 1 : -1;
    int co[3] = {0, 0, 0};
    co[a1] = o1;
    bool s1 = blockIsOpaque(view.at(x + kNormal[f][0] + co[0],
                                    y + kNormal[f][1] + co[1],
                                    z + kNormal[f][2] + co[2]));
    co[a1] = 0; co[a2] = o2;
    bool s2 = blockIsOpaque(view.at(x + kNormal[f][0] + co[0],
                                    y + kNormal[f][1] + co[1],
                                    z + kNormal[f][2] + co[2]));
    co[a1] = o1; co[a2] = o2;
    bool dd = blockIsOpaque(view.at(x + kNormal[f][0] + co[0],
                                    y + kNormal[f][1] + co[1],
                                    z + kNormal[f][2] + co[2]));
    if (s1 && s2) return 0;
    return 3 - ((s1 ? 1 : 0) + (s2 ? 1 : 0) + (dd ? 1 : 0));
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
                        const int face = F_PY;
                        uint32_t base = (uint32_t)wv.size();
                        for (int c = 0; c < 4; c++) {
                            TerrainVertex vt;
                            vt.x = (int16_t)((x + kCorner[face][c][0]) * 16);
                            vt.y = (int16_t)((y + kCorner[face][c][1]) * 16);
                            vt.z = (int16_t)((z + kCorner[face][c][2]) * 16);
                            vt.w = 0;
                            vt.u = kUvCorner[c][0] ? 16 : 0;
                            vt.v = kUvCorner[c][1] ? 16 : 0;
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

                Box parts[2];
                const int partCount = buildParts(id, parts);
                for (int pi = 0; pi < partCount; pi++) {
                    const Box& box = parts[pi];
                    for (int f = 0; f < 6; f++) {
                        const BoxFace& bf = box.face[f];
                        if (!bf.present) continue;
                        // 只有"贴方块边界的 cullface 面"才被不透明邻居剔除；
                        // 模型内部面（如眼块四周、框顶面）永远画，否则贴着方块摆
                        // 就会缺面。
                        if (bf.cull && onBoundary(box, f) &&
                            cullFace(id, view.at(x + kNormal[f][0], y + kNormal[f][1],
                                                 z + kNormal[f][2])))
                            continue;

                        uint32_t base = (uint32_t)ov.size();
                        for (int c = 0; c < 4; c++) {
                            TerrainVertex vt;
                            int cx16 = kCorner[f][c][0] ? box.x1 : box.x0;
                            int cy16 = kCorner[f][c][1] ? box.y1 : box.y0;
                            int cz16 = kCorner[f][c][2] ? box.z1 : box.z0;
                            vt.x = (int16_t)(x * 16 + cx16);
                            vt.y = (int16_t)(y * 16 + cy16);
                            vt.z = (int16_t)(z * 16 + cz16);
                            vt.w = 0;
                            vt.u = kUvCorner[c][0] ? bf.u1 : bf.u0;
                            vt.v = kUvCorner[c][1] ? bf.v1 : bf.v0;
                            vt.tex = bf.tile;
                            vt.shade = emissiveBlock(id)
                                          ? 255
                                          : bakeShade(f, aoForFace(view, x, y, z, f, c), false);
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
    }
    return m;
}
