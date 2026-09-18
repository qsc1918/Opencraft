#pragma once
// ===========================================================================
// entity.hpp — 实体规范（对齐 Minecraft 实体模型）
// 实体 = 带命名空间 ID、按 tick 模拟、有类型碰撞箱的动态对象。
// 字段为存档/联机预留，对应 MC NBT：Pos=pos、Motion=vel、Rotation=yaw/pitch。
// 注意 pos 是脚部中心；MC 用角度制，这里内部用弧度制。
// ===========================================================================
#include "util.hpp"
#include <cstdint>

// 运行时实例 ID，0 为无效；持久化应改用 UUID（MC 规范）。
using EntityId = uint32_t;

// 实体类型：决定碰撞箱与行为，静态注册（MC 注册表模式）。
struct EntityType {
    const char* id;    // 命名空间 ID
    float width;       // 碰撞箱宽（X/Z，方块单位）
    float height;      // 碰撞箱高（脚→头）
    float eyeHeight;   // 眼高（相对脚部）
};

// 与 MC 对齐的实体类型（MC player: 0.6×1.8、眼高 1.62）。
inline constexpr EntityType ENTITY_PLAYER{"opencraft:player", 0.6f, 1.8f, 1.62f};

class World;

// 实体种类，用于渲染/AI 分派
enum class EntityKind : uint8_t {
    Generic = 0,   // 无渲染实体的默认种类（玩家等）
    EndCrystal,    // 末影水晶
    EnderDragon,   // 末影龙
};

// 与 MC 对齐的实体类型表（规范 ID 见 docs/standards.md §实体）
inline constexpr EntityType ENTITY_END_CRYSTAL{"opencraft:end_crystal", 2.0f, 2.0f, 1.0f};
inline constexpr EntityType ENTITY_ENDER_DRAGON{"opencraft:ender_dragon", 13.0f, 4.0f, 2.0f};

// 实体基类，按 tick 模拟（由 World::tickEntities 在主线程驱动）。
// pos=脚部中心、vel=速度、yaw/pitch=朝向（弧度）。
struct Entity {
    EntityId id = 0;                   // 实例 ID
    EntityKind kind = EntityKind::Generic;
    const EntityType* type = nullptr;  // 类型（决定碰撞箱）
    Vec3 pos{};                        // 脚部中心（MC Pos）
    Vec3 vel{};                        // 速度（MC Motion）
    float yaw = 0.0f;                  // 朝向（弧度；MC 用角度制）
    float pitch = 0.0f;                // 俯仰（弧度）
    bool dead = false;                 // 标记后由 World::tickEntities 移除
    float age = 0.0f;                  // 存在时长（秒）

    virtual ~Entity() = default;
    virtual void tick(World& w, float dt) { age += dt; }
    // 返回实际伤害；<=0 表示免疫（MC 龙的免疫规则见规范文档）
    virtual float hurt(float damage) { return damage; }
};
