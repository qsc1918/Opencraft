#pragma once
// ===========================================================================
// entities.hpp — 具体实体类型（末影水晶 / 末影龙等）
//
// 实体都继承 Entity（entity.hpp）：tick() 每帧主线程模拟，hurt() 定受击规则；
// 渲染由 Renderer 按 kind 拼盒模型。规范见 docs/standards.md §实体，
// 末地机制参数见 §10/B5 阶段说明。
// ===========================================================================
#include "entity.hpp"
#include "specs.hpp"
#include "world.hpp"
#include <cmath>

// ---------------------------------------------------------------------------
// 末影水晶（MC 最新机制）:
//  - 悬在柱顶 bedrock 上方约 1 格，持续自旋并轻微上下浮动
//  - 对 32 格内的末影龙发射治疗光束: 0.5 HP/tick = 10 HP/秒
//  - 受击立即爆炸（爆炸由攻击方触发；这里只负责消失）
// ---------------------------------------------------------------------------
struct EndCrystal : Entity {
    float phase = 0.0f;   // 自旋相位（弧度）
    bool  caged = false;  // 是否被铁栏杆笼罩住（末地最高的两根柱子）

    EndCrystal() {
        kind = EntityKind::EndCrystal;
        type = &ENTITY_END_CRYSTAL;
    }
    void tick(World& w, float dt) override {
        age += dt;
        phase += dt * 1.5f;
    }
    float hurt(float dmg) override {
        dead = true;   // 受击即毁（爆炸特效/方块破坏由攻击调用方做）
        return dmg;
    }
};

// ---------------------------------------------------------------------------
// 下落的方块（MC FallingBlock）：沙子/沙砾/龙蛋失去支撑后变成的实体。
// 运动参数照抄原版：重力 -0.04 格/刻²，顺序是「加速度 → 位置 → 阻力」，
// 阻力 0.98（见 实体.txt 的运动表）。只判竖直碰撞：落到第一个实心方块顶面为止。
// ---------------------------------------------------------------------------
struct FallingBlock : Entity {
    uint8_t block = B_SAND;   // 落地后还原成哪个方块
    int ticksAlive = 0;       // 存在刻数；超 600 刻原版会变掉落物，这里直接消失

    FallingBlock() {
        kind = EntityKind::FallingBlock;
        type = &ENTITY_FALLING_BLOCK;
    }

    void tick(World& w, float dt) override {
        Entity::tick(w, dt);
        if (dead) return;
        if (++ticksAlive > 600) { dead = true; return; }

        // 区块没加载时不模拟（否则会穿过未生成的地形掉到世界底）
        int cx = blockToChunkCoord((int)std::floor(pos.x));
        int cz = blockToChunkCoord((int)std::floor(pos.z));
        auto c = w.chunkAt(cx, cz);
        if (!c || c->state.load() < 1) return;

        vel.y -= 0.04f;                      // 原版重力加速度（格/刻²）
        int bx = (int)std::floor(pos.x), bz = (int)std::floor(pos.z);

        // 找落点：脚下第一个实心方块的顶面（一路扫到世界底）
        int landY = WORLD_MIN_Y;
        for (int y = (int)std::floor(pos.y) - 1; y >= WORLD_MIN_Y; y--) {
            if (blockIsSolid(w.getBlock(bx, y, bz))) { landY = y + 1; break; }
        }

        float ny = pos.y + vel.y;
        if (ny <= (float)landY) {
            pos.y = (float)landY;
            uint8_t cur = w.getBlock(bx, landY, bz);
            if (cur == B_AIR || cur == B_WATER || cur == B_LAVA) {
                w.setBlock(bx, landY, bz, block);
            }
            // 目标被非固体方块占用时原版掉落为物品，本工程还没有物品实体
            dead = true;
            return;
        }
        pos.y = ny;
        vel.x *= 0.98f; vel.y *= 0.98f; vel.z *= 0.98f;   // 阻力
    }
};
