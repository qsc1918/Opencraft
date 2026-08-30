#pragma once
// ===========================================================================
// entity.hpp — 实体规范（对齐 Minecraft 实体模型）
//
// 实体 (Entity) = 拥有命名空间 ID、按 tick 模拟、带类型碰撞箱的动态世界对象。
// 完整规范见 docs/standards.md §实体。
//
// 与 Minecraft 的对应关系（为未来存档/联机预留的字段语义）：
//   Entity::pos    ↔ MC NBT "Pos"      —— 脚部中心坐标（不是眼睛！）
//   Entity::vel    ↔ MC NBT "Motion"   —— 速度
//   Entity::yaw/pitch ↔ MC NBT "Rotation" —— MC 用角度制，本项目内部用弧度制
//   EntityType::id ↔ MC NBT "id"       —— 命名空间 ID，如 "voxmine:player"
// ===========================================================================
#include "util.hpp"
#include <cstdint>

// 运行时实体实例 ID。0 = 无效。持久化时应改用 UUID（MC 规范）。
using EntityId = uint32_t;

// 实体类型：决定碰撞箱与基础行为。类型本身静态注册（MC 注册表模式）。
struct EntityType {
    const char* id;    // 命名空间 ID（Resource location）
    float width;       // 碰撞箱宽（X/Z 方向，方块单位）
    float height;      // 碰撞箱高（脚→头）
    float eyeHeight;   // 眼睛相对脚部的高度
};

// 与 Minecraft 对齐的实体类型（MC player: 0.6×1.8、眼高 1.62）。
inline constexpr EntityType ENTITY_PLAYER{"voxmine:player", 0.6f, 1.8f, 1.62f};

// 实体实例公共字段。当前仅玩家使用；未来掉落物/生物等在此之上扩展。
struct Entity {
    EntityId id = 0;                   // 实例 ID
    const EntityType* type = nullptr;  // 类型（决定碰撞箱）
    Vec3 pos{};                        // 脚部中心（MC "Pos" 语义）
    Vec3 vel{};                        // 速度（MC "Motion" 语义）
    float yaw = 0.0f;                  // 朝向（弧度；MC "Rotation" 为角度制）
    float pitch = 0.0f;                // 俯仰（弧度）
};
