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

constexpr int kMaxParts = 8;

// 原版紫颂植株/花的模型写了 "ambientocclusion": false：按整格邻块算 AO 会
// 在细柱子上糊出莫名其妙的暗角，所以这些模型不做 AO。
inline bool noAoBlock(uint8_t id) {
    return id == B_CHORUS_PLANT || id == B_CHORUS_FLOWER || id == B_CHORUS_FLOWER_DEAD;
}

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

// 原版 element 没写 uv 时由 from/to 推导的默认 UV（FaceBakery 规则）：
// u/v 各取自哪两个轴由面决定，朝 -X/-Z/-Y 的面 u 反向（从面外侧看 u 向右增）。
inline void defaultUv(const Box& b, int f, uint8_t& u0, uint8_t& v0, uint8_t& u1, uint8_t& v1) {
    switch (f) {
    case F_PX: u0 = (uint8_t)(16 - b.z1); u1 = (uint8_t)(16 - b.z0); break; // east
    case F_NX: u0 = (uint8_t)b.z0;        u1 = (uint8_t)b.z1;        break; // west
    case F_PY: u0 = (uint8_t)b.x0;        u1 = (uint8_t)b.x1;        break; // up
    case F_NY: u0 = (uint8_t)(16 - b.x0); u1 = (uint8_t)(16 - b.x1); break; // down
    case F_PZ: u0 = (uint8_t)b.x0;        u1 = (uint8_t)b.x1;        break; // south
    default:   u0 = (uint8_t)(16 - b.x1); u1 = (uint8_t)(16 - b.x0); break; // north
    }
    switch (f) {
    case F_PY: v0 = (uint8_t)b.z0;        v1 = (uint8_t)b.z1;        break;
    case F_NY: v0 = (uint8_t)b.z1;        v1 = (uint8_t)b.z0;        break;
    default:   v0 = (uint8_t)(16 - b.y1); v1 = (uint8_t)(16 - b.y0); break;
    }
}

// 紫颂植株某面是否"接得上"邻块（原版 ChorusPlantBlock#connectsTo）：
// 紫颂植株 / 紫颂花；朝下额外认末地石（BlockTags.SUPPORTS_CHORUS_PLANT）。
inline bool chorusConnects(uint8_t nb, int f) {
    if (nb == B_CHORUS_PLANT || nb == B_CHORUS_FLOWER || nb == B_CHORUS_FLOWER_DEAD) return true;
    return f == F_NY && nb == B_END_STONE;
}

// 紫颂植株：原版 blockstates 是 multipart——某面接上邻块就长出 [4,4,0]->[12,12,4]
// 的凸起（只画朝外的盖 + 四个侧面，贴内层那面不画），没接上就露出内层
// [4,4,4]->[12,12,12] 的内壁（#inside）。所以整块看着是一根 8×8 的细柱子。
inline int buildChorusPlant(Box* out, uint8_t conn) {
    int n = 0;
    Box& inner = out[n++];
    inner = Box{};
    inner.x0 = 4; inner.y0 = 4; inner.z0 = 4;
    inner.x1 = 12; inner.y1 = 12; inner.z1 = 12;
    for (int f = 0; f < 6; f++) inner.face[f].present = false;
    for (int f = 0; f < 6; f++)
        if (!(conn & (1 << f))) setFace(inner, f, T_CHORUS_PLANT, 4, 4, 12, 12, false);

    for (int f = 0; f < 6; f++) {
        if (!(conn & (1 << f))) continue;
        Box& b = out[n++];
        b = Box{};
        int a = f >> 1;                       // 法线轴：0=X,1=Y,2=Z
        int lo[3] = {4, 4, 4}, hi[3] = {12, 12, 12};
        if (f == F_PX || f == F_PY || f == F_PZ) { lo[a] = 12; hi[a] = 16; }
        else                                   { lo[a] = 0;  hi[a] = 4;  }
        b.x0 = lo[0]; b.y0 = lo[1]; b.z0 = lo[2];
        b.x1 = hi[0]; b.y1 = hi[1]; b.z1 = hi[2];
        for (int k = 0; k < 6; k++) b.face[k].present = false;
        uint8_t u0, v0, u1, v1;
        defaultUv(b, f, u0, v0, u1, v1);
        setFace(b, f, T_CHORUS_PLANT, u0, v0, u1, v1, true); // cullface=本面
        for (int k = 0; k < 6; k++) {
            if (k == f || k == (f ^ 1)) continue;            // 盖 + 贴内层那面
            defaultUv(b, k, u0, v0, u1, v1);
            setFace(b, k, T_CHORUS_PLANT, u0, v0, u1, v1, false);
        }
    }
    return n;
}

// 紫颂花：原版 models/block/template_chorus_flower.json 的 6 个 element（花杯形，
// 四角内凹）。#texture=花瓣贴图，#bottom=紫颂植株贴图；模型里没有 cullface。
inline int buildChorusFlower(Box* out, uint8_t id) {
    const uint8_t tex = (id == B_CHORUS_FLOWER_DEAD) ? T_CHORUS_FLOWER_DEAD : T_CHORUS_FLOWER;
    const uint8_t bot = T_CHORUS_PLANT; // #bottom
    int n = 0;
    auto elem = [&](int x0, int y0, int z0, int x1, int y1, int z1) -> Box& {
        Box& b = out[n++];
        b = Box{};
        b.x0 = x0; b.y0 = y0; b.z0 = z0;
        b.x1 = x1; b.y1 = y1; b.z1 = z1;
        for (int k = 0; k < 6; k++) b.face[k].present = false;
        return b;
    };
    // 顶盖 [2,14,2]->[14,16,14]
    {
        Box& b = elem(2, 14, 2, 14, 16, 14);
        setFace(b, F_PY, tex, 2, 2, 14, 14, false);
        for (int f : {F_NX, F_PX, F_NZ, F_PZ}) setFace(b, f, bot, 2, 0, 14, 2, false);
    }
    // 西壁 [0,2,2]->[2,14,14]
    {
        Box& b = elem(0, 2, 2, 2, 14, 14);
        setFace(b, F_NY, bot, 16, 14, 14, 2, false);
        setFace(b, F_PY, bot, 0, 2, 2, 14, false);
        setFace(b, F_NZ, bot, 14, 2, 16, 14, false);
        setFace(b, F_PZ, bot, 0, 2, 2, 14, false);
        setFace(b, F_NX, tex, 2, 2, 14, 14, false);
    }
    // 北壁 [2,2,0]->[14,14,2]
    {
        Box& b = elem(2, 2, 0, 14, 14, 2);
        setFace(b, F_NY, bot, 14, 2, 2, 0, false);
        setFace(b, F_PY, bot, 2, 0, 14, 2, false);
        setFace(b, F_NZ, tex, 2, 2, 14, 14, false);
        setFace(b, F_NX, bot, 0, 2, 2, 14, false);
        setFace(b, F_PX, bot, 14, 2, 16, 14, false);
    }
    // 南壁 [2,2,14]->[14,14,16]
    {
        Box& b = elem(2, 2, 14, 14, 14, 16);
        setFace(b, F_NY, bot, 14, 16, 2, 14, false);
        setFace(b, F_PY, bot, 2, 14, 14, 16, false);
        setFace(b, F_PZ, tex, 2, 2, 14, 14, false);
        setFace(b, F_NX, bot, 14, 2, 16, 14, false);
        setFace(b, F_PX, bot, 0, 2, 2, 14, false);
    }
    // 东壁 [14,2,2]->[16,14,14]
    {
        Box& b = elem(14, 2, 2, 16, 14, 14);
        setFace(b, F_NY, bot, 2, 14, 0, 2, false);
        setFace(b, F_PY, bot, 14, 2, 16, 14, false);
        setFace(b, F_NZ, bot, 0, 2, 2, 14, false);
        setFace(b, F_PZ, bot, 14, 2, 16, 14, false);
        setFace(b, F_PX, tex, 2, 2, 14, 14, false);
    }
    // 杯身 [2,0,2]->[14,14,14]
    {
        Box& b = elem(2, 0, 2, 14, 14, 14);
        setFace(b, F_PY, bot, 2, 2, 14, 14, false);
        setFace(b, F_NY, bot, 14, 14, 2, 2, false);
        for (int f : {F_NX, F_PX, F_NZ, F_PZ}) setFace(b, f, bot, 2, 2, 14, 16, false);
    }
    return n;
}

// ---------------------------------------------------------------------------
// 末地传送门框架：原版 models/block/end_portal_frame(_filled).json
//  - element0 [0,0,0]->[16,13,16]：down=end_stone(cullface)、up=frame_top(无 cullface)、
//    四侧 uv [0,3,16,16]（侧面贴图顶部 3 行是透明的，正好跳过）(cullface 同名)。
//  - element1 [4,13,4]->[12,16,12]（仅"有眼"）：up uv [4,4,12,12](cullface up)、
//    四侧 uv [4,0,12,3]（无 cullface，绝不因邻居消失）、没有 down 面。
// 眼块是"居中"的 8×8 柱子、凸出框顶 3/16（对应原版 SHAPE_FULL），不随朝向偏移。
// ---------------------------------------------------------------------------
inline int buildParts(uint8_t id, Box* out, uint8_t conn) {
    if (id == B_CHORUS_PLANT) return buildChorusPlant(out, conn);
    if (id == B_CHORUS_FLOWER || id == B_CHORUS_FLOWER_DEAD) return buildChorusFlower(out, id);
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

                // 紫颂植株按六面邻块决定"长不长凸起"，先算连接掩码
                uint8_t conn = 0;
                if (id == B_CHORUS_PLANT) {
                    for (int f = 0; f < 6; f++)
                        if (chorusConnects(view.at(x + kNormal[f][0], y + kNormal[f][1],
                                                   z + kNormal[f][2]), f))
                            conn |= (uint8_t)(1 << f);
                }
                Box parts[kMaxParts]; // 植株最多 1 个内层 + 6 个凸起
                const int partCount = buildParts(id, parts, conn);
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
                                          : bakeShade(f,
                                                      noAoBlock(id) ? 3
                                                                    : aoForFace(view, x, y, z, f, c),
                                                      false);
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
