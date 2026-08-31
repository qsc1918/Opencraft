#pragma once
#include "camera.hpp"
#include "entity.hpp"
#include "window.hpp"

class World;

// 玩家 = 当前唯一实体（规范见 docs/standards.md §实体）。
// 碰撞箱/眼高等规范值统一取自 ENTITY_PLAYER（MC player: 0.6×1.8、眼高 1.62）。
// 坐标约定:
//   cam.pos  = 眼睛位置（渲染相机）
//   ent.pos  = 脚部中心（MC 实体 Pos 语义），由 syncEntity() 与 cam 保持同步
struct Player {
    Entity ent;       // 默认构造；由 syncEntity 填充 type/kind/id（每帧同步）
    Camera cam;
    Vec3 vel{0, 0, 0};
    bool flying = false;
    bool onGround = false;
    bool inWater = false;
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
