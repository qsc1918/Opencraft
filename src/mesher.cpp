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
// 面亮度取原版固定值（原版没有方向性太阳项，只有这张固定表）：
// 顶面 1.0、底面 0.5、南北 0.8、东西 0.6。着色器按"显示空间乘法"应用
// （线性空间乘 系数^2.2），所以开阔地顶面的显示值就等于贴图原值。
const float kFaceBright[6] = {0.60f, 0.60f, 1.0f, 0.50f, 0.80f, 0.80f};

// 预计算亮度查表：kShade[面][AO][是否水面]
uint8_t kShade[6][4][2];
struct ShadeInit { ShadeInit() {
    for (int f = 0; f < 6; f++) {
        for (int a = 0; a < 4; a++) {
            float br = kFaceBright[f] * kAoBright[a];
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
    // 自定义 8 角点（1/16 格，可越出 0..16）：斜 22.5° 的墙上火把等
    // 非轴对齐几何用。下标 = ix*4 + iy*2 + iz（ix/iy/iz ∈ {0,1}，
    // 0 取 min 侧、1 取 max 侧，与 kCorner 的标志位一致）。
    bool custom = false;
    float cv[8][3] = {};
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
// 火把（原版 models/block/template_torch.json 的轴对齐还原）：
// 一根 2×2×10 的小杆立在方块中心，顶面是焰心、四侧是杆身竖条。
inline int buildTorch(Box* out) {
    Box& b = out[0];
    b = Box{};
    b.x0 = 7; b.y0 = 0; b.z0 = 7;
    b.x1 = 9; b.y1 = 10; b.z1 = 9;
    for (int f = 0; f < 6; f++) b.face[f].present = false;
    setFace(b, F_NY, T_TORCH, 7, 13, 9, 15, true);  // 原版 cullface=down
    setFace(b, F_PY, T_TORCH, 7, 6, 9, 8, false);   // 焰心
    for (int f : {F_PX, F_NX, F_PZ, F_NZ})
        setFace(b, f, T_TORCH, 7, 6, 9, 16, false); // 杆身
    return 1;
}

// 墙上火把：原版 models/block/template_torch_wall.json——竖杆
// [-1,3.5,7]->[1,13.5,9] 绕 z 轴在 (0,3.5,8) 转 -22.5°（下端插在墙上、
// 上端外倾），再按朝向绕块中心 (8,8) 转 y。几何不是轴对齐的，
// 走 Box::custom 自定义角点；UV 与地面火把完全一致（贴图随几何一起转）。
inline int buildWallTorch(Box* out, int facing) {
    const float c22 = 0.9238795f;   // cos(-22.5°)
    const float s22 = -0.3826834f;  // sin(-22.5°)
    const float ox = 0.0f, oy = 3.5f;
    Box& b = out[0];
    b = Box{};
    b.custom = true;
    for (int ix = 0; ix < 2; ix++)
        for (int iy = 0; iy < 2; iy++)
            for (int iz = 0; iz < 2; iz++) {
                float x = ix ? 1.0f : -1.0f;
                float y = iy ? 13.5f : 3.5f;
                float z = iz ? 9.0f : 7.0f;
                // 绕 z 轴 -22.5°（原点 (0,3.5)）
                float rx = ox + (x - ox) * c22 - (y - oy) * s22;
                float ry = oy + (x - ox) * s22 + (y - oy) * c22;
                // 绕块中心 (8,8) 把外倾方向 (+X) 转到朝向
                float ax = rx - 8.0f, az = z - 8.0f;
                float fx, fz;
                switch (facing) {
                case FRAME_EAST:  fx = ax;   fz = az;   break;
                case FRAME_SOUTH: fx = -az;  fz = ax;   break;
                case FRAME_WEST:  fx = -ax;  fz = -az;  break;
                default:          fx = az;   fz = -ax;  break; // 北
                }
                float* p = b.cv[ix * 4 + iy * 2 + iz];
                p[0] = fx + 8.0f;
                p[1] = ry;
                p[2] = fz + 8.0f;
            }
    for (int f = 0; f < 6; f++) b.face[f].present = false;
    setFace(b, F_NY, T_TORCH, 7, 13, 9, 15, false);
    setFace(b, F_PY, T_TORCH, 7, 6, 9, 8, false);   // 焰心
    for (int f : {F_PX, F_NX, F_PZ, F_NZ})
        setFace(b, f, T_TORCH, 7, 6, 9, 16, false); // 杆身
    return 1;
}

// 火（原版 fire_floor 是 4 片斜面，这里用轴对齐十字双面片近似）：
// 两片互相垂直的 1px 薄板，各画自己的一对"大面"（±法线方向），
// 薄边不画。画错面的话整片火只剩 1px 细线。
inline int buildFire(Box* out) {
    Box& a = out[0];
    a = Box{};
    a.x0 = 7; a.y0 = 0; a.z0 = 0;
    a.x1 = 8; a.y1 = 16; a.z1 = 16;
    for (int f = 0; f < 6; f++) a.face[f].present = false;
    setFace(a, F_NX, T_FIRE, 0, 0, 16, 16, false); // x=7 大面
    setFace(a, F_PX, T_FIRE, 0, 0, 16, 16, false); // x=8 大面
    Box& c = out[1];
    c = Box{};
    c.z0 = 7; c.y0 = 0; c.x0 = 0;
    c.z1 = 8; c.y1 = 16; c.x1 = 16;
    for (int f = 0; f < 6; f++) c.face[f].present = false;
    setFace(c, F_NZ, T_FIRE, 0, 0, 16, 16, false); // z=7 大面
    setFace(c, F_PZ, T_FIRE, 0, 0, 16, 16, false); // z=8 大面
    return 2;
}

inline int buildParts(uint8_t id, Box* out, uint8_t conn) {
    if (id == B_CHORUS_PLANT) return buildChorusPlant(out, conn);
    if (id == B_CHORUS_FLOWER || id == B_CHORUS_FLOWER_DEAD) return buildChorusFlower(out, id);
    if (id == B_TORCH) return buildTorch(out);
    if (blockIsWallTorch(id)) return buildWallTorch(out, blockWallTorchFacing(id));
    if (id == B_FIRE) return buildFire(out);
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

inline uint8_t bakeShade(int face, int ao, bool water) {
    return kShade[face][ao][water ? 1 : 0];
}

// ---------------------------------------------------------------------------
// 贪心网格化：把同一朝向、同贴图、同亮度的整方块面合并成一张大矩形。
// 前提是"合并后必须和逐格出面逐像素一致"，所以一张面只有在**四个角亮度全相等**
// 时才参与合并（见 canMergeFace）；多 element 模型、自定义 UV 的面走原来的路径。
// ---------------------------------------------------------------------------

// 每个朝向的面内两条轴、uv 由哪条轴驱动；由 kCorner/kUvCorner 现推，避免手抄表
struct FaceLayout {
    int na = 0;               // 法线轴
    int slotAxis[2] = {0, 0}; // 另外两条轴
    int uSlot = 0, uFlip = 0; // uv 取 slotAxis[uSlot]，uFlip=1 表示随该轴反向
    int vSlot = 0, vFlip = 0;
};
FaceLayout kLayout[6];
int kAoIdx[6][4];             // 角点 c 在面内 2x2 里的序号 = bit(slot0) + 2*bit(slot1)

struct LayoutInit {
    LayoutInit() {
        for (int f = 0; f < 6; f++) {
            FaceLayout& L = kLayout[f];
            L.na = f >> 1;
            int n = 0;
            for (int a = 0; a < 3; a++)
                if (a != L.na) L.slotAxis[n++] = a;
            for (int comp = 0; comp < 2; comp++) {
                for (int s = 0; s < 2; s++) {
                    // 取 uv 大端的两个角点必须落在该轴同一侧、另两个落在对侧，
                    // 满足这条的轴就是驱动该 uv 分量的轴
                    int want = -1;
                    bool ok = true;
                    for (int c = 0; c < 4; c++) {
                        int w = kUvCorner[c][comp] ? kCorner[f][c][L.slotAxis[s]]
                                                   : 1 - kCorner[f][c][L.slotAxis[s]];
                        if (want < 0) want = w;
                        else if (want != w) { ok = false; break; }
                    }
                    if (!ok || want < 0) continue;
                    if (comp == 0) { L.uSlot = s; L.uFlip = want ? 0 : 1; }
                    else           { L.vSlot = s; L.vFlip = want ? 0 : 1; }
                }
            }
            for (int c = 0; c < 4; c++)
                kAoIdx[f][c] = kCorner[f][c][L.slotAxis[0]] + 2 * kCorner[f][c][L.slotAxis[1]];
        }
    }
} layoutInit;

// 待合并面：tile(8 位) + 亮度(8 位) + 标记，塞进一个 uint32
enum : uint32_t { kFacePresent = 1u << 16, kFaceUsed = 1u << 17 };
inline uint32_t packFace(uint8_t tile, uint8_t shade) {
    return (uint32_t)tile | ((uint32_t)shade << 8) | kFacePresent;
}
inline bool facePresent(uint32_t c) { return (c & kFacePresent) != 0; }
inline bool faceUsed(uint32_t c) { return (c & kFaceUsed) != 0; }
inline uint8_t faceTile(uint32_t c) { return (uint8_t)(c & 0xFFu); }
inline uint8_t faceShade(uint32_t c) { return (uint8_t)((c >> 8) & 0xFFu); }

// 一整列 [f][y][z][x] 的待合并面表；thread_local 让每个构网格线程各用一份
struct GreedyGrid {
    std::vector<uint32_t> cells;
    int clearedTo = -1;
    void reset(int upto) {
        const size_t perF = (size_t)WORLD_HEIGHT * CHUNK_SIZE * CHUNK_SIZE;
        if (cells.empty()) cells.assign(perF * 6, 0);
        // 只清"上次和上上次用到过"的高度，避免每次都 memset 整个 [0,128)
        int need = upto > clearedTo ? upto : clearedTo;
        if (need < 0) return;
        const size_t n = (size_t)(need + 1) * CHUNK_SIZE * CHUNK_SIZE;
        for (int f = 0; f < 6; f++)
            memset(cells.data() + (size_t)f * perF, 0, n * sizeof(uint32_t));
        clearedTo = upto;
    }
    uint32_t& at(int f, int x, int y, int z) {
        return cells[(((size_t)f * WORLD_HEIGHT + y) * CHUNK_SIZE + z) * CHUNK_SIZE + x];
    }
};
thread_local GreedyGrid g_grid;

// 水面只有顶面，一层一张表就够；相邻水面高度不同就不共面，所以 y 也要参与比较
struct WaterCell {
    uint8_t tile = 0, shade = 0, y = 0;
    bool present = false, used = false;
};
inline bool sameWater(const WaterCell& a, const WaterCell& b) {
    return a.present && !a.used && a.tile == b.tile && a.shade == b.shade && a.y == b.y;
}

// uv 是 uint8（以 1/16 格为单位），合并超过 15 格就溢出 255，按这个上限截断
const int kMaxMergeBlocks = 15;
// 顶点 w 分量交给着色器：合并出的大面 uv 会超出单个图块，需要按图块平铺
enum : int16_t { kWrapU = 1, kWrapV = 2 };

// 输出一张轴对齐矩形：coord=每条轴的起点（1/16 格），span=跨度（法线轴为 0）
static void emitRect(std::vector<TerrainVertex>& verts, std::vector<uint32_t>& idx, int f,
                     const int coord[3], const int span[3], uint8_t tile, uint8_t shade) {
    const FaceLayout& L = kLayout[f];
    const uint32_t base = (uint32_t)verts.size();
    const int su = span[L.slotAxis[L.uSlot]];
    const int sv = span[L.slotAxis[L.vSlot]];
    for (int c = 0; c < 4; c++) {
        TerrainVertex vt{};
        vt.x = (int16_t)(coord[0] + (kCorner[f][c][0] ? span[0] : 0));
        vt.y = (int16_t)(coord[1] + (kCorner[f][c][1] ? span[1] : 0));
        vt.z = (int16_t)(coord[2] + (kCorner[f][c][2] ? span[2] : 0));
        const bool atMaxU = kCorner[f][c][L.slotAxis[L.uSlot]] != 0;
        const bool atMaxV = kCorner[f][c][L.slotAxis[L.vSlot]] != 0;
        vt.u = (uint8_t)((L.uFlip ? !atMaxU : atMaxU) ? su : 0);
        vt.v = (uint8_t)((L.vFlip ? !atMaxV : atMaxV) ? sv : 0);
        vt.tex = tile;
        vt.shade = shade;
        vt.w = (int16_t)((su > 16 ? kWrapU : 0) | (sv > 16 ? kWrapV : 0));
        verts.push_back(vt);
    }
    idx.push_back(base);     idx.push_back(base + 1); idx.push_back(base + 2);
    idx.push_back(base);     idx.push_back(base + 2); idx.push_back(base + 3);
}

// 只有"标准整方块 + 整张贴图"的面能安全合并：多 element 模型（火把、紫颂、
// 末地门框架）与自定义 UV 的面会按原样逐个出。
inline bool mergeableFace(int f, const Box& box, const BoxFace& bf) {
    if (box.custom) return false;
    if (box.x0 || box.y0 || box.z0) return false;
    if (box.x1 != 16 || box.y1 != 16 || box.z1 != 16) return false;
    if (!bf.cull || !onBoundary(box, f)) return false;
    return bf.u0 == 0 && bf.v0 == 0 && bf.u1 == 16 && bf.v1 == 16;
}

// 四个角的亮度必须全相等才允许合并（合并后整张只剩四个角插值，与原逐格渐变不符）；
// 自发光方块本来就恒为满亮，天然满足。
inline bool uniformFaceShade(const MeshView& view, int x, int y, int z, int f, uint8_t id,
                            uint8_t& out) {
    if (emissiveBlock(id)) { out = 255; return true; }
    if (noAoBlock(id))     { out = bakeShade(f, 3, false); return true; }
    out = bakeShade(f, aoForFace(view, x, y, z, f, 0), false);
    for (int c = 1; c < 4; c++)
        if (bakeShade(f, aoForFace(view, x, y, z, f, c), false) != out) return false;
    return true;
}

// 某个朝向的所有面做一轮二维贪心：沿法线轴分层，层内在另外两条轴上合并成矩形
static void mergeFaceLayer(int f, int maxY, std::vector<TerrainVertex>& ov,
                           std::vector<uint32_t>& oi) {
    const FaceLayout& L = kLayout[f];
    const int planes = (L.na == 1) ? (maxY + 1) : CHUNK_SIZE;
    const int dim0 = (L.slotAxis[0] == 1) ? (maxY + 1) : CHUNK_SIZE;
    const int dim1 = (L.slotAxis[1] == 1) ? (maxY + 1) : CHUNK_SIZE;
    auto cellAt = [&](int p, int i0, int i1) -> uint32_t& {
        int co[3];
        co[L.na] = p;
        co[L.slotAxis[0]] = i0;
        co[L.slotAxis[1]] = i1;
        return g_grid.at(f, co[0], co[1], co[2]);
    };
    for (int p = 0; p < planes; p++) {
        for (int i1 = 0; i1 < dim1; i1++) {
            for (int i0 = 0; i0 < dim0; i0++) {
                uint32_t& c0 = cellAt(p, i0, i1);
                if (!facePresent(c0) || faceUsed(c0)) continue;
                const uint32_t key = c0 & (kFacePresent | 0xFFFFu);
                // 先沿 slot0 尽量延长
                int w = 1;
                while (w < kMaxMergeBlocks && i0 + w < dim0 &&
                       (cellAt(p, i0 + w, i1) & (kFacePresent | kFaceUsed | 0xFFFFu)) == key &&
                       faceTile(cellAt(p, i0 + w, i1)) == faceTile(c0))
                    w++;
                // 再沿 slot1 成行延长
                int h = 1;
                while (h < kMaxMergeBlocks && i1 + h < dim1) {
                    bool row = true;
                    for (int k = 0; k < w && row; k++) {
                        uint32_t& cc = cellAt(p, i0 + k, i1 + h);
                        if (!facePresent(cc) || faceUsed(cc) ||
                            faceTile(cc) != faceTile(c0) || faceShade(cc) != faceShade(c0))
                            row = false;
                    }
                    if (!row) break;
                    h++;
                }
                for (int b = 0; b < h; b++)
                    for (int a = 0; a < w; a++)
                        cellAt(p, i0 + a, i1 + b) |= kFaceUsed;
                int coord[3] = {0, 0, 0}, span[3] = {0, 0, 0};
                coord[L.na] = (kNormal[f][L.na] > 0 ? (p + 1) : p) * 16;
                coord[L.slotAxis[0]] = i0 * 16;
                coord[L.slotAxis[1]] = i1 * 16;
                span[L.slotAxis[0]] = w * 16;
                span[L.slotAxis[1]] = h * 16;
                emitRect(ov, oi, f, coord, span, faceTile(c0), faceShade(c0));
            }
        }
    }
}


} // 匿名命名空间

ChunkMeshData buildChunkMesh(const MeshView& view) {
    ChunkMeshData m;
    auto& ov = m.opaqueVerts;
    auto& oi = m.opaqueIdx;
    auto& wv = m.waterVerts;
    auto& wi = m.waterIdx;
    ov.reserve(4096);
    oi.reserve(6144);
    wv.reserve(512);
    wi.reserve(768);

    // 先找最高非空气方块，跳过其上必定为空的区域。
    int maxY = 0;
    for (int z = 0; z < CHUNK_SIZE; z++)
        for (int x = 0; x < CHUNK_SIZE; x++)
            for (int y = WORLD_HEIGHT - 1; y > maxY; y--)
                if (view.at(x, y, z) != B_AIR) { maxY = y; break; }

    g_grid.reset(maxY);

    // 水面只有顶层顶面一层，单独收一张 16x16 的表再合并
    WaterCell waterPlane[CHUNK_SIZE * CHUNK_SIZE] = {};

    for (int y = 0; y <= maxY + 1 && y < WORLD_HEIGHT; y++) {
        for (int z = 0; z < CHUNK_SIZE; z++) {
            for (int x = 0; x < CHUNK_SIZE; x++) {
                uint8_t id = view.at(x, y, z);
                if (id == B_AIR) continue;

                if (id == B_WATER || id == B_LAVA) {
                    // 只出顶面：侧面/底面透过半透明水面看不见，
                    // 还会产生发暗的瑕疵。
                    if (fluidCull(view.at(x, y + 1, z))) continue;
                    WaterCell& wc = waterPlane[x + z * CHUNK_SIZE];
                    wc.present = true;
                    wc.y = (uint8_t)y;
                    wc.tile = (uint8_t)((id == B_LAVA) ? T_LAVA : T_WATER);
                    wc.shade = emissiveBlock(id) ? (uint8_t)255 : bakeShade(F_PY, 3, true);
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

                        // 能合并的面先记进该朝向的表，稍后统一做贪心；其余（多 element
                        // 模型、AO 四角不等、自定义 UV）按原来的方式逐张出。
                        uint8_t ushade = 0;
                        if (mergeableFace(f, box, bf) &&
                            uniformFaceShade(view, x, y, z, f, id, ushade)) {
                            g_grid.at(f, x, y, z) = packFace(bf.tile, ushade);
                            continue;
                        }

                        uint32_t base = (uint32_t)ov.size();
                        for (int c = 0; c < 4; c++) {
                            TerrainVertex vt;
                            int16_t vx, vy, vz;
                            if (box.custom) {
                                // 斜 22.5° 墙上火把等自定义角点几何（1/16 格浮点，
                                // 就近取整到引擎的 1/16 格整数网格）
                                const float* p = box.cv[kCorner[f][c][0] * 4 +
                                                        kCorner[f][c][1] * 2 +
                                                        kCorner[f][c][2]];
                                vx = (int16_t)lroundf(p[0]);
                                vy = (int16_t)lroundf(p[1]);
                                vz = (int16_t)lroundf(p[2]);
                            } else {
                                vx = kCorner[f][c][0] ? box.x1 : box.x0;
                                vy = kCorner[f][c][1] ? box.y1 : box.y0;
                                vz = kCorner[f][c][2] ? box.z1 : box.z0;
                            }
                            vt.x = (int16_t)(x * 16 + vx);
                            vt.y = (int16_t)(y * 16 + vy);
                            vt.z = (int16_t)(z * 16 + vz);
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
    // ---- 贪心合并：六个朝向各来一轮，最后是水面那一层 ----
    for (int f = 0; f < 6; f++) mergeFaceLayer(f, maxY, ov, oi);

    const FaceLayout& wl = kLayout[F_PY];
    for (int i1 = 0; i1 < CHUNK_SIZE; i1++) {
        for (int i0 = 0; i0 < CHUNK_SIZE; i0++) {
            WaterCell& c0 = waterPlane[i0 + i1 * CHUNK_SIZE];
            if (!c0.present || c0.used) continue;
            // 只有同高度的水面才是真正的共面，合并时必须连 y 一起比
            int w = 1;
            while (w < kMaxMergeBlocks && i0 + w < CHUNK_SIZE &&
                   sameWater(waterPlane[i0 + w + i1 * CHUNK_SIZE], c0))
                w++;
            int h = 1;
            while (h < kMaxMergeBlocks && i1 + h < CHUNK_SIZE) {
                bool row = true;
                for (int k = 0; k < w && row; k++)
                    if (!sameWater(waterPlane[i0 + k + (i1 + h) * CHUNK_SIZE], c0)) row = false;
                if (!row) break;
                h++;
            }
            for (int b = 0; b < h; b++)
                for (int a = 0; a < w; a++)
                    waterPlane[i0 + a + (i1 + b) * CHUNK_SIZE].used = true;
            int coord[3] = {0, 0, 0}, span[3] = {0, 0, 0};
            coord[wl.na] = (kNormal[F_PY][wl.na] > 0 ? (c0.y + 1) : c0.y) * 16;
            coord[wl.slotAxis[0]] = i0 * 16;
            coord[wl.slotAxis[1]] = i1 * 16;
            span[wl.slotAxis[0]] = w * 16;
            span[wl.slotAxis[1]] = h * 16;
            emitRect(wv, wi, F_PY, coord, span, c0.tile, c0.shade);
        }
    }
    return m;
}
