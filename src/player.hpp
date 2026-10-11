#pragma once
#include "camera.hpp"
#include "dimensions.hpp"
#include "entity.hpp"
#include "platform.hpp"
#include "mobs.hpp"
#include "window.hpp"

class World;

// 玩家 = 当前唯一实体（规范见 docs/standards.md §实体）。
// 碰撞箱/眼高等规范值统一取自 ENTITY_PLAYER（MC player: 0.6×1.8、眼高 1.62）。
// 坐标约定:
//   cam.pos  = 眼睛位置（渲染相机）
//   ent.pos  = 脚部中心（MC 实体 Pos 语义），由 syncEntity() 与 cam 保持同步
struct Player {
    // 玩家就是实体（MC Player extends LivingEntity）：ent 指向注册进 World 的
    // 那一个，生物靠它找目标、攻击它。每帧由 syncEntity 同步位置/速度/朝向。
    PlayerEntity* ent = nullptr;
    Camera cam;
    Vec3 vel{0, 0, 0};
    bool flying = false;
    bool onGround = false;
    bool inWater = false;
    DimensionId dim = DIM_OVERWORLD;  // 当前所在维度
    Vec3 spawnPos{8.5f, 80.0f, 8.5f}; // 出生点（死亡后回这里）
    float height = ENTITY_PLAYER.height;          // 1.8（碰撞箱高）
    float halfWidth = ENTITY_PLAYER.width * 0.5f; // 0.3（碰撞箱半宽）
    float eyeHeight = ENTITY_PLAYER.eyeHeight;    // 1.62（眼高）

    void update(Input& in, World& world, float dt);
    // 眼睛坐标(相机) → 实体坐标（脚部中心 + 速度 + 朝向）。
    // 未来存档(实体 Pos)/联机/实体系统统一从 ent 读，避免两套真值。
    void syncEntity();
    bool solidAt(const World& w, int x, int y, int z) const;
    bool collides(const World& w, float x0, float y0, float z0, float x1, float y1, float z1) const;
    bool spaceHeld_ = false;
    float spaceTapTimer_ = 1.0f;
};
