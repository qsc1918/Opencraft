#pragma once
// ===========================================================================
// mobs.hpp — 生物（被动/敌对）与玩家实体
//
// AI 架构与物理都是原版移植（见 mobai.hpp/mobpath.hpp）：
//   每整刻顺序 = 原版 serverAiStep + travel：
//     goalSelector → navigation → moveControl → jumpControl → lookControl
//     → applyInput → moveRelative → move(碰撞) → 重力/阻力
//   关键点：原版 Mob.setSpeed(speed) 会同时把前进输入 zza 设为 speed，
//   所以地面加速度 = speed²（摩擦 0.6 下稳态位移 ≈ 2.2*speed² 格/刻，
//   猪 0.25 → ≈2.8 m/s，与原版一致）。
// 特殊行为（苦力怕引信、末影人瞬移、烈焰人火球等）走 aiExtra 钩子。
// ===========================================================================
#include "dimensions.hpp"
#include "entity.hpp"
#include "mobai.hpp"
#include "world.hpp"
#include <memory>
#include <vector>

// 生物共用的基类
struct Mob : Entity {
    // ---- 属性（原版 Attributes）----
    float moveSpeed = 0.10f;     // generic.movement_speed
    bool  hostile = false;       // 是否敌对（决定目标选择）
    float damage = 3.0f;         // 近战伤害
    float fallDistance = 0.0f;
    int   airSupply = 300;
    float fuse = -1.0f;          // 苦力怕引信：<0 未点燃
    int   teleportTimer = 0;     // 末影人瞬移冷却
    int   angerTicks = 0;        // 末影人被注视后的愤怒时间

    // ---- 原版运行时字段 ----
    float speed = 0.0f;          // Mob.speed：加速度基准
    float zza = 0.0f, xxa = 0.0f;
    // 原版 Mob.setSpeed：速度同时写进"前进输入" zza —— 这是移动的关键
    // （地面加速度 = speed * zza = speed²，忘了这一步生物就只会转身不走路）
    void setSpeed(float s) { speed = s; zza = s; }
    float yHeadRot = 0.0f;       // 头部绝对朝向（弧度，本引擎 yaw 约定）
    bool  jumping = false;
    int   noJumpDelay = 0;
    int   lastHurtTicks = 0;     // 受击后 PanicGoal 的触发窗口
    float maxUpStep = 0.6f;
    Entity* target = nullptr;    // 敌对目标（玩家）
    float limbSwing = 0.0f;      // 走路动画相位
    float limbAmount = 0.0f;
    int   attackCooldown = 0;
    World* world = nullptr;      // 每刻由 tick 填（Goal 用来找玩家/读方块）

    // ---- AI 组件 ----
    ai::GoalSelector goals;
    ai::PathNavigation nav;
    ai::MoveControl moveCtl;
    ai::JumpControl jumpCtl;
    ai::LookControl lookCtl;
    std::vector<std::unique_ptr<ai::Goal>> goalStorage_;

    Mob();
    void tick(World& w, float dt) override;
    float hurt(float dmg) override;

    template<typename T, typename... A>
    void addGoal(int prio, A&&... a) {
        auto g = std::make_unique<T>(*this, std::forward<A>(a)...);
        goals.add(prio, g.get());
        goalStorage_.push_back(std::move(g));
    }

    virtual void registerGoals() {}                 // 构造时调用一次
    virtual void aiExtra(World& w) {}               // 特殊行为钩子（每刻）
    virtual float gravityPerTick() const { return 0.08f; }
    virtual bool takesFallDamage() const { return true; }
    virtual bool burnsInDaylight() const { return false; }

private:
    void travel(World& w);              // 原版 LivingEntity.travelInAir
    void moveWithCollisions(World& w);  // travel 里的 move()：碰撞 + 摔落
};

// 被动生物：原版 registerGoals（Float/Panic/Stroll/LookAt/RandomLook）
struct PassiveMob : Mob {
    void registerGoals() override;
};

// 敌对生物：近战追击 + 巡逻
struct HostileMob : Mob {
    void registerGoals() override;
};

struct Pig : PassiveMob { Pig(); };
struct Cow : PassiveMob { Cow(); };
struct Sheep : PassiveMob { Sheep(); };
struct Chicken : PassiveMob {
    Chicken();
    bool takesFallDamage() const override { return false; }  // 原版鸡免疫摔落
    void aiExtra(World& w) override;                          // 慢速下落（每刻 *0.6）
};

struct Zombie : HostileMob {
    Zombie();
    bool burnsInDaylight() const override { return true; }
};
struct Skeleton : HostileMob {
    Skeleton();
    bool burnsInDaylight() const override { return true; }
    void registerGoals() override;   // 远程射箭 goal
};
struct Creeper : HostileMob {
    Creeper();
    void aiExtra(World& w) override;   // 引信 + 爆炸
};
struct Spider : HostileMob { Spider(); };
struct Blaze : HostileMob {
    Blaze();
    float gravityPerTick() const override { return 0.0f; }   // 漂浮
    bool takesFallDamage() const override { return false; }
    void aiExtra(World& w) override;                          // 悬浮 + 火球
};
struct Enderman : HostileMob {
    Enderman();
    void aiExtra(World& w) override;   // 被注视才敌对 + 随机瞬移 + 怕水
    void teleportRandom(World& w);
};

// ---- 弹射物 ----
struct Arrow : Entity {         // 箭：受重力，命中造成伤害与击退
    Arrow();
    void tick(World& w, float dt) override;
};
struct Fireball : Entity {      // 小火球：不落，命中点火并造成伤害
    Fireball();
    void tick(World& w, float dt) override;
};

// ---- 玩家实体 ----
struct PlayerEntity : Entity {
    int regenTimer = 0;
    int hurtFlash = 0;
    PlayerEntity();
    void tick(World& w, float dt) override;
    float hurt(float dmg) override;
};

namespace mobs {

void setDayLight(float v);
float dayLight();
void setDimension(DimensionId d);
DimensionId dimension();

// 最近的存活玩家（没有返回 nullptr）
Entity* findPlayer(World& w);

bool skyBlocked(const World& w, int x, int y, int z);
int lightAt(const World& w, int x, int y, int z);
bool boxSolid(const World& w, float x0, float y0, float z0, float x1, float y1, float z1);
void knockback(Entity& e, const Vec3& from, float strength);
void explode(World& w, const Vec3& center, float power, bool fire);

} // namespace mobs
