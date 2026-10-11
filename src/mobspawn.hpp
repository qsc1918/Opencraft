#pragma once
// ===========================================================================
// mobspawn.hpp — 生物刷新（对齐 MC 的刷新规则与数量上限）
//
// 规则按原版简化：
//  * 被动生物（猪/牛/羊/鸡）只在主世界草方块上、亮度 ≥ 9 时刷新；
//  * 敌对生物要在亮度 ≤ 7 的实心方块上（主世界/下界/末地各有各的表）；
//  * 烈焰人原版只从下界要塞的刷怪笼里出，本工程还没有要塞，所以不刷新
//    （实体与 AI 都在，用控制台 summon 可以手动放出来）。
//  * 数量上限沿用原版：被动 10、敌对 70（按维度各算各的）。
//  * 超出 128 格的生物直接消失（原版 despawn 上限）。
// ===========================================================================
#include "entity.hpp"

class World;

namespace mobspawn {

// 每帧调用（内部按 20 TPS 累积，每秒尝试若干次）
void tick(World& w, const Vec3& playerPos, int renderDistBlocks, float dt);

// 手动生成一个生物（控制台 summon 用）；kind 不合法返回 0
Entity* summon(World& w, EntityKind kind, const Vec3& at, int count = 1);

// 名字 → 类型（控制台 summon 用，英文小写）；不认识返回 false
bool kindByName(const char* name, EntityKind& out);

} // namespace mobspawn
