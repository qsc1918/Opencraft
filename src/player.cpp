#include "player.hpp"
#include "world.hpp"
#include <windows.h>
#include <algorithm>
#include <cmath>

bool Player::solidAt(const World& w, int x, int y, int z) const {
    return blockIsSolid(w.getBlock(x, y, z));
}

bool Player::collides(const World& w, float x0, float y0, float z0, float x1, float y1, float z1) const {
    int bx0 = (int)std::floor(x0), by0 = (int)std::floor(y0), bz0 = (int)std::floor(z0);
    int bx1 = (int)std::floor(x1), by1 = (int)std::floor(y1), bz1 = (int)std::floor(z1);
    for (int by = by0; by <= by1; by++)
        for (int bx = bx0; bx <= bx1; bx++)
            for (int bz = bz0; bz <= bz1; bz++)
                if (solidAt(w, bx, by, bz)) return true;
    return false;
}

static const float EPS = 1e-4f;

// 该位置是什么流体（水/岩浆），用于游泳与上浮
static uint8_t fluidAt(const World& w, float fx, float fy, float fz) {
    uint8_t b = w.getBlock((int)std::floor(fx), (int)std::floor(fy), (int)std::floor(fz));
    return (b == B_WATER || b == B_LAVA) ? b : (uint8_t)B_AIR;
}

void Player::update(Input& in, World& world, float dt) {
    if (dt > 0.05f) dt = 0.05f;

    // 切换飞行：双击空格（创造模式，同原版）
    bool space = in.keys[VK_SPACE];
    spaceTapTimer_ += dt;
    if (space && !spaceHeld_) {
        if (spaceTapTimer_ < 0.3f) {
            flying = !flying;
            vel = Vec3(0, 0, 0);
            spaceTapTimer_ = 1.0f;
        } else {
            spaceTapTimer_ = 0.0f;
        }
    }
    spaceHeld_ = space;

    // 眼位与脚部所在流体：眼睛在水里才算"在水下"（原版同）
    uint8_t eyeFluid = fluidAt(world, cam.pos.x, cam.pos.y, cam.pos.z);
    uint8_t feetFluid = fluidAt(world, cam.pos.x, cam.pos.y - eyeHeight + 0.1f, cam.pos.z);
    uint8_t fluid = eyeFluid ? eyeFluid : feetFluid;
    bool inLava = fluid == B_LAVA;
    bool inWater = fluid == B_WATER;
    bool inFluid = inLava || inWater;

    Vec3 fwd = cam.forward();
    Vec3 rt = cam.right();
    Vec3 mv(0, 0, 0);
    if (in.keys['W']) mv = mv + fwd;
    if (in.keys['S']) mv = mv - fwd;
    if (in.keys['D']) mv = mv + rt;
    if (in.keys['A']) mv = mv - rt;
    mv.y = 0;
    float ml = std::sqrt(mv.x * mv.x + mv.z * mv.z);
    if (ml > 1e-5f) { mv.x /= ml; mv.z /= ml; }

    // 流体里横向移动变慢（岩浆最慢）
    const float walkSpeed = inLava ? 1.6f : (inWater ? 2.7f : 4.5f);
    const float flySpeed = 20.0f;

    if (flying) {
        float speed = flySpeed * (in.keys[VK_SHIFT] ? 0.35f : 1.0f);
        float k = 1.0f - std::exp(-14.0f * dt);
        vel.x += (mv.x * speed - vel.x) * k;
        vel.z += (mv.z * speed - vel.z) * k;
        float ty = 0;
        if (in.keys[VK_SPACE]) ty = 1;
        if (in.keys[VK_SHIFT]) ty = -1;
        vel.y += (ty * speed - vel.y) * k;
        onGround = false;
    } else {
        float k = 1.0f - std::exp(-12.0f * dt);
        vel.x += (mv.x * walkSpeed - vel.x) * k;
        vel.z += (mv.z * walkSpeed - vel.z) * k;
        // 流体里重力小、下沉慢，按住空格可以上浮（岩浆里也能爬出来）
        float gravity = inLava ? 4.5f : (inWater ? 9.0f : 28.0f);
        float maxFall = inLava ? -1.2f : (inWater ? -2.5f : -40.0f);
        vel.y -= gravity * dt;
        if (vel.y < maxFall) vel.y = maxFall;
        if (in.keys[VK_SPACE] && (onGround || inFluid)) {
            vel.y = inFluid ? (inLava ? 3.2f : 4.0f) : 8.5f;
            onGround = false;
        }
    }

    // 带碰撞推进（cam.pos 是眼位；脚部 = 眼位 - 眼高）
    Vec3 p = cam.pos;
    float hw = halfWidth, hh = height;
    float feetY = p.y - eyeHeight;

    // X 轴
    p.x += vel.x * dt;
    if (collides(world, p.x - hw, feetY, p.z - hw, p.x + hw, feetY + hh, p.z + hw)) {
        if (vel.x > 0) p.x = std::floor(p.x + hw) - hw - EPS;
        else if (vel.x < 0) p.x = std::ceil(p.x - hw) + hw + EPS;
        vel.x = 0;
    }
    // Z 轴
    p.z += vel.z * dt;
    if (collides(world, p.x - hw, feetY, p.z - hw, p.x + hw, feetY + hh, p.z + hw)) {
        if (vel.z > 0) p.z = std::floor(p.z + hw) - hw - EPS;
        else if (vel.z < 0) p.z = std::ceil(p.z - hw) + hw + EPS;
        vel.z = 0;
    }
    // Y 轴
    p.y += vel.y * dt;
    feetY = p.y - eyeHeight;
    bool grounded = false;
    if (collides(world, p.x - hw, feetY, p.z - hw, p.x + hw, feetY + hh, p.z + hw)) {
        if (vel.y < 0) {
            p.y = std::ceil(feetY) + eyeHeight + EPS;
            grounded = true;
        } else if (vel.y > 0) {
            p.y = std::floor(feetY + hh) - hh + eyeHeight - EPS;
        }
        vel.y = 0;
    }
    onGround = grounded;

    // 限制在世界内（Y=0 是世界底；MC 1.18+ 主世界底为 Y=-64，见 docs/standards.md）
    if (p.y - eyeHeight < WORLD_MIN_Y) { p.y = eyeHeight + WORLD_MIN_Y; vel.y = 0; }

    cam.pos = p;
    syncEntity();
}

void Player::syncEntity() {
    ent.id = 1;                    // 玩家 ID 固定（不由 World 管理）
    ent.kind = EntityKind::Generic;
    ent.type = &ENTITY_PLAYER;
    ent.pos = Vec3(cam.pos.x, cam.pos.y - eyeHeight, cam.pos.z); // 眼睛 → 脚部中心
    ent.vel = vel;
    ent.yaw = cam.yaw;
    ent.pitch = cam.pitch;
}
