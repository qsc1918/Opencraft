// ===========================================================================
// mobmodel.cpp — 生物/玩家模型数据（原版 ModelPart/Box）与盒模型生成
//
// 数据来自 docs/mob_models.md（逐字取自 26.2 反编译源码的 texOffs 与 addBox）。
// 换算约定见 mobmodel.hpp 与 mob_models.md §1.2/§1.4。
//
// UV 展开约定（原版 Box，见 mob_models.md §1.4）：
//   贴图上一行是 顶|底，下一行是 西|北|东|南 四个侧面连排：
//     [U, U+d]=西(-X)  [U+d, U+d+w]=北(-Z,正面)  [U+d+w, U+d+w+d]=东(+X)
//     [U+d+w+d, U+2d+2w]=南(+Z,背面)
//   侧面 v 沿模型 y（矩形上沿 = 模型 minY = 世界顶，袖子在肩上）；
//   ±X 面 u 沿 z、±Z 面 u 沿 x；展开图连续 ⇒ -X 面 u 朝 -z 增、
//   +Z 面 u 随 x 减（即从背面看文字不镜像）。
//   顶/底面 u 沿 x、v 沿 z。
//   ⚠ 之前 -X/+Z/顶三个面把 u 配到了错误的轴上（贴图旋转 90°）：
//   袖子横条变竖条、腿的鞋底横条变竖灰条 —— 改成按角点位置推导 uv 后修复。
// ===========================================================================
#include "mobmodel.hpp"
#include "blocks.hpp"
#include "entities.hpp"
#include "mobs.hpp"
#include "world.hpp"
#include <cmath>
#include <cstring>

namespace {

constexpr float PI = 3.14159265f;
constexpr float DEG = PI / 180.0f;

// ---- 模型数据（见 mobmodel_data.inc）----
#include "mobmodel_data.inc"

// ---- 变换：R = Rz·Ry·Rx，平移在最后（ModelPart.translateAndRotate 的顺序）----
struct Xf {
    float r[9];    // 行主序 3x3
    float t[3];
};

Xf makeXf(float ox, float oy, float oz, float rx, float ry, float rz) {
    float cx = cosf(rx), sx = sinf(rx);
    float cy = cosf(ry), sy = sinf(ry);
    float cz = cosf(rz), sz = sinf(rz);
    const float ax[9] = {1, 0, 0, 0, cx, -sx, 0, sx, cx};              // Rx
    const float ay[9] = {cy, 0, sy, 0, 1, 0, -sy, 0, cy};              // Ry
    const float az[9] = {cz, -sz, 0, sz, cz, 0, 0, 0, 1};             // Rz
    float tmp[9];
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) {
            float s = 0;
            for (int k = 0; k < 3; k++) s += ay[i * 3 + k] * ax[k * 3 + j];
            tmp[i * 3 + j] = s;
        }
    Xf m;
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) {
            float s = 0;
            for (int k = 0; k < 3; k++) s += az[i * 3 + k] * tmp[k * 3 + j];
            m.r[i * 3 + j] = s;
        }
    m.t[0] = ox; m.t[1] = oy; m.t[2] = oz;
    return m;
}

Xf mulXf(const Xf& a, const Xf& b) {   // a ∘ b
    Xf o;
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) {
            float s = 0;
            for (int k = 0; k < 3; k++) s += a.r[i * 3 + k] * b.r[k * 3 + j];
            o.r[i * 3 + j] = s;
        }
    for (int i = 0; i < 3; i++) {
        float s = a.t[i];
        for (int k = 0; k < 3; k++) s += a.r[i * 3 + k] * b.t[k];
        o.t[i] = s;
    }
    return o;
}

void applyXf(const Xf& m, float x, float y, float z, float& ox, float& oy, float& oz) {
    ox = m.r[0] * x + m.r[1] * y + m.r[2] * z + m.t[0];
    oy = m.r[3] * x + m.r[4] * y + m.r[5] * z + m.t[1];
    oz = m.r[6] * x + m.r[7] * y + m.r[8] * z + m.t[2];
}

// ---- 顶点 ----
// 实体管线的 uv 是"整张贴图的比例"：0..255 对应 0..1（着色器按 /256 取样），
// 这样一个 64 宽的贴图 1 像素 = 4 个单位，texOffs 不会因取整而错位。
inline uint8_t uvByte(float px, int texSize) {
    float b = px * 255.0f / (float)texSize;
    if (b < 0) b = 0;
    if (b > 255) b = 255;
    return (uint8_t)(b + 0.5f);
}

// 面亮度：与地形 kFaceBright 同一套（原版固定值）
const float kFaceShade[6] = {0.6f, 0.6f, 1.0f, 0.5f, 0.8f, 0.8f};  // +X -X +Y -Y +Z -Z

struct Vtx3 { float x, y, z; };

// 两个三角形用的角点下标，以及六个面的角点表（+X -X +Y -Y +Z -Z）。
// 每行的 4 个下标构成面的一个环（c0→c1→c2→c3）。
const int kIdx[6] = {0, 1, 2, 0, 2, 3};
const int kFace[6][4] = {
    {1, 5, 7, 3},   // +X (maxX)：c0→c1 沿 +z
    {0, 2, 6, 4},   // -X (minX)：c0→c1 沿 +y
    {0, 4, 5, 1},   // +Y (顶面)：c0→c1 沿 +z
    {2, 3, 7, 6},   // -Y (底面)：c0→c1 沿 +x
    {4, 6, 7, 5},   // +Z (maxZ)：c0→c1 沿 +y
    {0, 1, 3, 2},   // -Z (minZ)：c0→c1 沿 +x
};

// 四个角点 + 逐角点 uv（贴图像素），出两个三角形。
// uv 必须与角点一一对应地传入 —— 不能用固定 {u0,u1,u1,u0} 模式去配
// 不同面的角点顺序（那正是之前贴图旋转 90° 的根源）。
void pushQuad(std::vector<TerrainVertex>& out, const Vtx3 c[4],
              const float uu[4], const float vv[4], uint8_t tile,
              int texW, int texH, float shade, bool mirror) {
    for (int i = 0; i < 6; i++) {
        int k = kIdx[i];
        const Vtx3& p = c[k];
        float su = uu[k], sv = vv[k];
        if (mirror) su = uu[0] + uu[1] - su;   // 原版 mirror()：面内 u 翻转
        TerrainVertex v;
        float x = p.x * 16.0f, y = p.y * 16.0f, z = p.z * 16.0f;
        v.x = (int16_t)std::lround(clampf(x, -32760.0f, 32760.0f));
        v.y = (int16_t)std::lround(clampf(y, -32760.0f, 32760.0f));
        v.z = (int16_t)std::lround(clampf(z, -32760.0f, 32760.0f));
        v.w = 0;
        v.u = uvByte(su, texW);
        v.v = uvByte(sv, texH);
        v.tex = tile;
        v.shade = (uint8_t)std::lround(clampf(shade, 0.0f, 1.0f) * 255.0f);
        out.push_back(v);
    }
}

// 整张贴图铺满一个面（末影水晶 / 下落的方块 / 箭 / 火球）
void emitBoxFull(std::vector<TerrainVertex>& out, Vec3 center, Vec3 half, float yaw,
                 uint8_t tile, int texW, int texH, float light) {
    float cy = cosf(yaw), sy = sinf(yaw);
    Vtx3 c[8];
    for (int i = 0; i < 8; i++) {
        float lx = (i & 1) ? half.x : -half.x;
        float ly = (i & 2) ? half.y : -half.y;
        float lz = (i & 4) ? half.z : -half.z;
        c[i] = {center.x + lx * cy - lz * sy, center.y + ly, center.z + lx * sy + lz * cy};
    }
    for (int f = 0; f < 6; f++) {
        Vtx3 q[4];
        float uu[4], vv[4];
        for (int i = 0; i < 4; i++) {
            q[i] = c[kFace[f][i]];
            uu[i] = (i == 1 || i == 2) ? (float)texW : 0.0f;
            vv[i] = (i >= 2) ? (float)texH : 0.0f;
        }
        pushQuad(out, q, uu, vv, tile, texW, texH, kFaceShade[f] * light, false);
    }
}

} // 匿名命名空间

// ---------------------------------------------------------------------------
const MobModel* mobModel(EntityKind kind) {
    switch (kind) {
    case EntityKind::Player:    return &kModelPlayer;
    case EntityKind::Pig:       return &kModelPig;
    case EntityKind::Cow:       return &kModelCow;
    case EntityKind::Sheep:     return &kModelSheep;
    case EntityKind::Chicken:   return &kModelChicken;
    case EntityKind::Zombie:    return &kModelZombie;
    case EntityKind::Skeleton:  return &kModelSkeleton;
    case EntityKind::Creeper:   return &kModelCreeper;
    case EntityKind::Spider:    return &kModelSpider;
    case EntityKind::Blaze:     return &kModelBlaze;
    case EntityKind::Enderman:  return &kModelEnderman;
    default: return nullptr;
    }
}

const char* mobName(EntityKind kind) {
    switch (kind) {
    case EntityKind::Player:   return "玩家";
    case EntityKind::Pig:      return "猪";
    case EntityKind::Cow:      return "牛";
    case EntityKind::Sheep:    return "羊";
    case EntityKind::Chicken:  return "鸡";
    case EntityKind::Zombie:   return "僵尸";
    case EntityKind::Skeleton: return "骷髅";
    case EntityKind::Creeper:  return "苦力怕";
    case EntityKind::Spider:   return "蜘蛛";
    case EntityKind::Blaze:    return "烈焰人";
    case EntityKind::Enderman: return "末影人";
    case EntityKind::Arrow:    return "箭";
    case EntityKind::Fireball: return "小火球";
    case EntityKind::FallingBlock: return "下落的方块";
    case EntityKind::EndCrystal:   return "末影水晶";
    default: return "实体";
    }
}

// ---------------------------------------------------------------------------
void buildMobMesh(std::vector<TerrainVertex>& out, const Entity& e, const MobAnimState& an) {
    // 环境亮度：白天 1.0、夜里约 0.3（与地形的昼夜曲线同量级）
    float light = 0.30f + 0.70f * mobs::dayLight();

    // ---- 没有原版模型的实体：直接拼盒模型 ----
    switch (e.kind) {
    case EntityKind::EndCrystal: {
        float ph = an.age * 1.5f;
        float y = 1.0f + sinf(an.age * 1.2f) * 0.15f;
        emitBoxFull(out, Vec3(0, y, 0), Vec3(0.8f, 0.75f, 0.8f), ph, T_END_CRYSTAL, 16, 16, light);
        emitBoxFull(out, Vec3(0, y, 0), Vec3(0.35f, 0.45f, 0.35f), -ph * 0.7f, T_END_CRYSTAL, 16, 16, light);
        return;
    }
    case EntityKind::FallingBlock: {
        const auto* fb = static_cast<const FallingBlock*>(&e);
        emitBoxFull(out, Vec3(0, 0.49f, 0), Vec3(0.49f, 0.49f, 0.49f), 0.0f,
                blockTile(fb->block, F_PZ), 16, 16, light);
        return;
    }
    case EntityKind::Arrow:
        // 箭是 2D 贴图，盒模型贴不出朝向感，用一根细棍（朝向由 yaw 决定）
        emitBoxFull(out, Vec3(0, 0, 0), Vec3(0.05f, 0.05f, 0.5f), e.yaw, T_WHITE, 16, 16, light);
        return;
    case EntityKind::Fireball:
        emitBoxFull(out, Vec3(0, 0, 0), Vec3(0.22f, 0.22f, 0.22f), 0.0f, T_FIRE, 16, 16, light);
        return;
    default: break;
    }

    const MobModel* m = mobModel(e.kind);
    if (!m) return;

    // ---- 部件变换 ----
    const float hs = m->quadruped ? 0.6662f : 1.0f;   // 四足的头偏航要打折（原版）
    const int n = m->partN;
    Xf xf[24];
    float sw  = cosf(an.limbSwing * 0.6662f) * 1.4f * an.limbAmount;
    float sw2 = cosf(an.limbSwing * 0.6662f + PI) * 1.4f * an.limbAmount;
    for (int i = 0; i < n; i++) {
        const MobPart& p = m->parts[i];
        float rx = p.rx, ry = p.ry, rz = p.rz;
        switch (p.anim) {
        case AN_HEAD:
            ry += an.headYaw * hs;
            rx += an.headPitch * hs;
            break;
        case AN_LEG_R:  rx += sw;  break;   // 右腿 / 四足右前腿
        case AN_LEG_L:  rx += sw2; break;   // 左腿 / 四足左前腿
        case AN_LEG_R2: rx += sw2; break;   // 四足右后腿
        case AN_LEG_L2: rx += sw;  break;   // 四足左后腿
        case AN_ARM_R:  rx -= sw2 * 0.5f; break;
        case AN_ARM_L:  rx -= sw * 0.5f;  break;
        case AN_WING_R:
        case AN_WING_L:
            // 鸡翅膀：悬空时拍打，落地时收着
            if (an.flying) rz += sinf(an.age * 20.0f) * 0.6f * (p.anim == AN_WING_R ? 1.0f : -1.0f);
            break;
        case AN_ROD:
            ry += an.age * ((i & 1) ? -1.6f : 1.6f);   // 烈焰人的棒绕 Y 自旋
            break;
        case AN_SPIDER_LEG:
            ry += sinf(an.age * 6.0f + (float)i * 0.7f) * 0.12f;
            break;
        default: break;
        }
        Xf local = makeXf(p.ox, p.oy, p.oz, rx, ry, rz);
        xf[i] = (p.parent >= 0 && p.parent < i) ? mulXf(xf[p.parent], local) : local;
    }

    // ---- 逐 cube 出面 ----
    // uv 直接从"局部角点位置"推导（变形/缩放都不影响贴图对位），
    // 再把角点位置过部件变换与模型→世界变换。
    const float cy = cosf(e.yaw), sy = sinf(e.yaw);
    for (int i = 0; i < n; i++) {
        const MobPart& p = m->parts[i];
        for (int k = 0; k < p.cubeN; k++) {
            const MobCube& cube = m->cubes[p.cube0 + k];
            const float U = (float)cube.u, V = (float)cube.v;
            const float w = cube.w, h = cube.h, d = cube.d;
            float xa = cube.x0 - cube.def, ya = cube.y0 - cube.def, za = cube.z0 - cube.def;
            float xb = cube.x0 + cube.w + cube.def, yb = cube.y0 + cube.h + cube.def,
                  zb = cube.z0 + cube.d + cube.def;
            Vtx3 loc[8] = {
                {xa, ya, za}, {xb, ya, za}, {xa, yb, za}, {xb, yb, za},
                {xa, ya, zb}, {xb, ya, zb}, {xa, yb, zb}, {xb, yb, zb},
            };
            for (int f = 0; f < 6; f++) {
                Vtx3 q[4];
                float uu[4], vv[4];
                for (int i2 = 0; i2 < 4; i2++) {
                    const Vtx3& lp = loc[kFace[f][i2]];
                    // 该角点在面内的归一化参数（0..1）
                    float su, sv;
                    switch (f) {
                    case 0:  // +X：u 从前(za)到后(zb)，v 从顶(ya)到底(yb)
                        su = (lp.z - za) / (zb - za);
                        sv = (lp.y - ya) / (yb - ya);
                        uu[i2] = U + d + w + su * d;
                        vv[i2] = V + d + sv * h;
                        break;
                    case 1:  // -X：u 从后(zb)到前(za)，展开图连续
                        su = (zb - lp.z) / (zb - za);
                        sv = (lp.y - ya) / (yb - ya);
                        uu[i2] = U + su * d;
                        vv[i2] = V + d + sv * h;
                        break;
                    case 2:  // 模型 minY = 世界顶面：u 沿 x、v 沿 z
                        su = (lp.x - xa) / (xb - xa);
                        sv = (lp.z - za) / (zb - za);
                        uu[i2] = U + d + su * w;
                        vv[i2] = V + sv * d;
                        break;
                    case 3:  // 模型 maxY = 世界底面
                        su = (lp.x - xa) / (xb - xa);
                        sv = (lp.z - za) / (zb - za);
                        uu[i2] = U + d + w + su * w;
                        vv[i2] = V + sv * d;
                        break;
                    case 4:  // +Z 背面：u 随 x 减（与东面拼接成连续展开图）
                        su = (xb - lp.x) / (xb - xa);
                        sv = (lp.y - ya) / (yb - ya);
                        uu[i2] = U + d + w + d + su * w;
                        vv[i2] = V + d + sv * h;
                        break;
                    default: // -Z 正面
                        su = (lp.x - xa) / (xb - xa);
                        sv = (lp.y - ya) / (yb - ya);
                        uu[i2] = U + d + su * w;
                        vv[i2] = V + d + sv * h;
                        break;
                    }
                    // 位置过部件变换 → 模型像素 → 相对脚部的方块偏移
                    float mx, my, mz;
                    applyXf(xf[i], lp.x, lp.y, lp.z, mx, my, mz);
                    q[i2].x = (mx * cy - mz * sy) / 16.0f;
                    q[i2].y = 1.501f - my / 16.0f;
                    q[i2].z = (-mx * sy - mz * cy) / 16.0f;
                }
                // 羊毛层这类覆盖贴图的 cube 用 cube.tile，其余用模型贴图
                uint8_t tile = cube.tile ? cube.tile : m->tile;
                pushQuad(out, q, uu, vv, tile, m->texW, m->texH,
                         kFaceShade[f] * light, cube.mirror);
            }
        }
    }
}
