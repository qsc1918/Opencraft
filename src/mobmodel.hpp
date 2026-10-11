#pragma once
// ===========================================================================
// mobmodel.hpp — 生物/玩家模型（原版 ModelPart 层级 + Box 贴图）
//
// 数据全部来自 docs/mob_models.md（从 26.2 反编译源码逐字抄的 texOffs 与
// addBox 包围盒），渲染时按原版约定换算：
//   模型像素 → 世界偏移：wx = (mx·cos r − mz·sin r)/16
//                       wy = 1.501 − my/16
//                       wz = (−mx·sin r − mz·cos r)/16
//   （r = 实体 yaw，模型 y 轴朝下，y=24 是脚底；见 mob_models.md §1.2）
//
// 顶点 uv 用 0..255 表示整张贴图的比例（实体着色器按 /256 取样），
// 这样一个 64 宽的贴图正好 1 像素 = 4 个单位，逐面 texOffs 不会丢精度。
// ===========================================================================
#include "entity.hpp"
#include "world.hpp"
#include <vector>

// part 的动画槽：决定这一帧要不要摆动
enum MobAnim : uint8_t {
    AN_NONE = 0,
    AN_HEAD,
    AN_LEG_R,   // 类人：右腿；四足：右前腿
    AN_LEG_L,   // 类人：左腿；四足：左前腿
    AN_LEG_R2,  // 四足：右后腿
    AN_LEG_L2,  // 四足：左后腿
    AN_ARM_R,
    AN_ARM_L,
    AN_WING_R,
    AN_WING_L,
    AN_ROD,     // 烈焰人的棒（整体绕 Y 自旋）
    AN_SPIDER_LEG,
};

struct MobCube {
    float x0, y0, z0;   // 模型像素（y 向下）
    float w, h, d;      // 尺寸（像素）
    float def = 0.0f;   // CubeDeformation：各轴外扩（原版 0.5 / -0.5）
    int u = 0, v = 0;   // texOffs
    bool mirror = false;// mirror=true：面内 u 翻转
    uint8_t tile = 0;   // 0=用模型贴图；否则覆盖（羊的羊毛层用 sheep_wool）
};

struct MobPart {
    float ox, oy, oz;   // offset（像素，相对父 part）
    float rx, ry, rz;   // 静态旋转（弧度，Rz·Ry·Rx）
    int parent;         // -1 = 挂在 root 上
    uint8_t anim;       // MobAnim
    int cube0, cubeN;   // cube 区间
};

struct MobModel {
    uint8_t tile;       // 图集图块（blocks.hpp 的 T_ENTITY_*）
    int texW, texH;     // 贴图像素尺寸
    bool quadruped;     // 四足：头部偏航按 0.6662 打折（原版 QuadrupedModel）
    const MobPart* parts;
    int partN;
    const MobCube* cubes;
    int cubeN;
};

// 取模型数据；没有模型的实体返回 nullptr。
const MobModel* mobModel(EntityKind kind);
// 实体类型的中文名（调试输出用）
const char* mobName(EntityKind kind);

// 本帧的动画参数
struct MobAnimState {
    float limbSwing = 0.0f;    // 走路相位
    float limbAmount = 0.0f;   // 幅度 0..1
    float headYaw = 0.0f;      // 头相对身体的偏转（弧度）
    float headPitch = 0.0f;    // 俯仰（弧度，正=低头）
    float age = 0.0f;          // 存在时间（秒）
    bool  flying = false;      // 悬浮（鸡下落/烈焰人）
};

// 生成一个实体的盒模型顶点（坐标是相对实体脚部的 1/16 格整数）
void buildMobMesh(std::vector<TerrainVertex>& out, const Entity& e, const MobAnimState& an);
