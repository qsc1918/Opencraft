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
