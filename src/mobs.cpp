// ===========================================================================
// mobs.cpp — 生物 AI、物理与弹射物（数值与架构对齐 MC Java 26.2）
//
// 运动一律按"刻"算（World::tickEntities 用 TICK_DT 定步长驱动），
// 所以 vel 的单位是 格/刻，直接用原版常量不再乘 dt。
// ===========================================================================
#include "mobs.hpp"
#include "blockupdate.hpp"
#include "blocks.hpp"
#include "raycast.hpp"
#include "specs.hpp"
#include <algorithm>
#include <cmath>
#include <memory>
#include <cstdio>
#include <cstdlib>

namespace {

float g_dayLight = 1.0f;          // 0..1（1=正午）
DimensionId g_dim = DIM_OVERWORLD;

inline float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
constexpr float PI = 3.14159265f;

// 全局随机源（AI 用的随机不需要存档一致）
uint64_t g_rng = 0x123456789ABCDEFULL;
inline uint32_t rnd() {
    g_rng += 0x9e3779b97f4a7c15ULL;
    uint64_t z = g_rng;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return (uint32_t)(z ^ (z >> 31));
}
inline float rndF() { return (rnd() >> 8) * (1.0f / 16777216.0f); }
inline int rndInt(int n) { return n <= 0 ? 0 : (int)(rndF() * n); }

inline bool isFluid(uint8_t b) { return b == B_WATER || b == B_LAVA; }
inline bool mobInFluid(const Mob& m) {
    return isFluid(m.world->getBlock((int)std::floor(m.pos.x),
                                     (int)std::floor(m.pos.y + 0.1f),
                                     (int)std::floor(m.pos.z)));
}

// 视线：起点到终点之间有没有不透明方块挡着
bool hasLineOfSight(const World& w, const Vec3& from, const Vec3& to) {
    Vec3 d = to - from;
    float len = length(d);
    if (len < 1e-4f) return true;
    RayHit h = raycastWorld(w, from, d / len, len);
    return !h.hit;
}

inline float wrapPi(float a) {
    while (a > PI) a -= 2 * PI;
    while (a <= -PI) a += 2 * PI;
    return a;
}

} // 匿名命名空间

namespace mobs {

void setDayLight(float v) { g_dayLight = clamp01(v); }
float dayLight() { return g_dayLight; }
void setDimension(DimensionId d) { g_dim = d; }
DimensionId dimension() { return g_dim; }

Entity* findPlayer(World& w) {
    for (auto& e : w.entities())
        if (!e->dead && e->kind == EntityKind::Player) return e.get();
    return nullptr;
}

bool boxSolid(const World& w, float x0, float y0, float z0, float x1, float y1, float z1) {
    int bx0 = (int)std::floor(x0 + 1e-4f), bx1 = (int)std::floor(x1 - 1e-4f);
    int by0 = (int)std::floor(y0 + 1e-4f), by1 = (int)std::floor(y1 - 1e-4f);
    int bz0 = (int)std::floor(z0 + 1e-4f), bz1 = (int)std::floor(z1 - 1e-4f);
    for (int y = by0; y <= by1; y++)
        for (int x = bx0; x <= bx1; x++)
            for (int z = bz0; z <= bz1; z++)
                if (blockIsSolid(w.getBlock(x, y, z))) return true;
    return false;
}

// 头顶以上有没有不透明方块（原版 canSeeSky 的简化版）
bool skyBlocked(const World& w, int x, int y, int z) {
    for (int yy = y + 1; yy < WORLD_HEIGHT; yy++)
        if (blockIsOpaque(w.getBlock(x, yy, z))) return true;
    return false;
}

int lightAt(const World& w, int x, int y, int z) {
    int sky = 0;
    const DimensionType& dt = getDimensionType(g_dim);
    if (dt.hasSkylight && !skyBlocked(w, x, y, z)) {
        sky = (int)std::round(15.0f * (0.27f + 0.73f * g_dayLight));
    }
    int block = 0;
    const int R = 6;
    for (int dy = -R; dy <= R; dy++) {
        int yy = y + dy;
        if (yy < WORLD_MIN_Y || yy >= WORLD_HEIGHT) continue;
        for (int dx = -R; dx <= R; dx++)
            for (int dz = -R; dz <= R; dz++) {
                uint8_t b = w.getBlock(x + dx, yy, z + dz);
                if (b >= B_COUNT || b == B_AIR) continue;
                int em = BLOCK_DEFS[b].lightEmission;
                if (em <= 0) continue;
                int d = std::abs(dx) + std::abs(dy) + std::abs(dz);
                int v = em - d;
                if (v > block) block = v;
            }
    }
    return std::max(sky, std::min(block, 15));
}

void knockback(Entity& e, const Vec3& from, float strength) {
    Vec3 d = e.pos - from;
    d.y = 0;
    if (length(d) < 1e-4f) return;
    d = normalize(d) * strength;
    e.vel.x += d.x;
    e.vel.z += d.z;
    e.vel.y = std::max(e.vel.y, 0.4f);
}

void explode(World& w, const Vec3& center, float power, bool fire) {
    int r = (int)std::ceil(power);
    int cx = (int)std::floor(center.x), cy = (int)std::floor(center.y), cz = (int)std::floor(center.z);
    for (int dx = -r; dx <= r; dx++)
        for (int dy = -r; dy <= r; dy++)
            for (int dz = -r; dz <= r; dz++) {
                float d = std::sqrt((float)(dx * dx + dy * dy + dz * dz));
                if (d > power) continue;
                int x = cx + dx, y = cy + dy, z = cz + dz;
                if (y < WORLD_MIN_Y || y >= WORLD_HEIGHT) continue;
                uint8_t b = w.getBlock(x, y, z);
                if (b == B_AIR || b == B_BEDROCK || b == B_OBSIDIAN || blockIsPortalFrame(b)) continue;
                if (rndF() > 1.0f - d / power) continue;
                w.setBlock(x, y, z, fire ? B_FIRE : B_AIR);
            }
    for (size_t i = 0; i < w.entities().size(); i++) {
        Entity* e = w.entities()[i].get();
        if (!e || e->dead) continue;
        Vec3 c(e->pos.x, e->pos.y + (e->type ? e->type->height * 0.5f : 0.5f), e->pos.z);
        float d = length(c - center);
        if (d > power * 2.0f) continue;
        float f = 1.0f - d / (power * 2.0f);
        knockback(*e, center, 0.5f * f);
        e->hurt((power * 6.0f) * f);
    }
}

} // namespace mobs

// ---------------------------------------------------------------------------
// Mob：原版 serverAiStep + travel 的每整刻流程
// ---------------------------------------------------------------------------
Mob::Mob() : nav(*this) {
    nav.evaluator().setCanFloat(true);   // 原版 FloatGoal 构造时设置
}

void Mob::tick(World& w, float dt) {
    Entity::tick(w, dt);
    if (dead) return;
    world = &w;
    if (hurtTime > 0) hurtTime--;
    if (attackCooldown > 0) attackCooldown--;
    if (lastHurtTicks > 0) lastHurtTicks--;

    // 燃烧：每 20 刻掉 1 点（原版 fire 伤害 1 点/秒）
    if (fireTicks > 0) {
        fireTicks--;
        if (fireTicks % 20 == 0) hurt(1.0f);
        if (dead) return;
    }
    // 白天燃烧（僵尸/骷髅）
    if (burnsInDaylight() && g_dayLight > 0.55f) {
        int bx = (int)std::floor(pos.x), by = (int)std::floor(pos.y + type->height * 0.9f),
            bz = (int)std::floor(pos.z);
        bool wet = isFluid(w.getBlock(bx, (int)std::floor(pos.y), bz));
        if (!wet && !mobs::skyBlocked(w, bx, by, bz)) fireTicks = 100;
    }
    // 溺水
    uint8_t headBlock = w.getBlock((int)std::floor(pos.x),
                                   (int)std::floor(pos.y + type->height * 0.9f),
                                   (int)std::floor(pos.z));
    if (headBlock == B_WATER || headBlock == B_LAVA) {
        if (airSupply > 0) airSupply--;
        else if (((int)age * 20) % 20 == 0) hurt(headBlock == B_LAVA ? 4.0f : 2.0f);
    } else if (airSupply < 300) {
        airSupply += 4;
    }
    if (dead) return;

    // ---- 目标选择（NearestAttackableTargetGoal 等价：35 格内最近玩家）----
    if (hostile) {
        if (!target || target->dead || length(pos - target->pos) > 35.0f) {
            Entity* p = mobs::findPlayer(w);
            target = (p && length(pos - p->pos) <= 35.0f) ? p : nullptr;
        }
    }

    // ---- serverAiStep：goals → navigation → move → jump → look（原版顺序）----
    // 性能：原版靠"实体更新半径"决定远处生物是否 tick；本工程没有那套机制，
    // 用 48 格近似 —— 更远的生物只保留物理（下落/燃烧），不跑 AI/寻路。
    // 70 只怪同时寻路时这一条能把帧时间从 ~19ms 拉回 ~5ms。
    bool aiActive = true;
    {
        Entity* p = mobs::findPlayer(w);
        if (p) {
            float dx = pos.x - p->pos.x, dz = pos.z - p->pos.z;
            aiActive = dx * dx + dz * dz < 48.0f * 48.0f;
        }
    }
    if (aiActive) {
        goals.tick(*this);
        nav.tick();
        moveCtl.tick(*this);
        jumpCtl.tick(*this);
        lookCtl.tick(*this);
        aiExtra(w);
    } else {
        nav.stop();
        setSpeed(0.0f);
        zza = 0.0f;
        xxa = 0.0f;
    }
    if (dead) return;

    // ---- travel（LivingEntity.travelInAir：输入 → 移动 → 重力/阻力）----
    travel(w);

    // ---- BodyRotationControl（简化）----
    // 移动时身体就是移动朝向（MoveControl 已设 yaw），头最多偏 50°；
    // 静止且头偏离超过 15° 时身体慢慢转向头（原版 headStableTime>10 后开始）。
    float headDiff = wrapPi(yHeadRot - yaw);
    bool moving = std::fabs(vel.x) + std::fabs(vel.z) > 2.5e-4f;
    if (moving) {
        yHeadRot = yaw + std::clamp(headDiff, -50.0f * PI / 180.0f, 50.0f * PI / 180.0f);
    } else if (std::fabs(headDiff) > 15.0f * PI / 180.0f) {
        yaw += std::clamp(headDiff, -5.0f * PI / 180.0f, 5.0f * PI / 180.0f);
    }

    static int dbgTick = 0;   // age 是秒，用它取模打不出每刻日志，单开计数
    if (getenv("OPENCRAFT_MOB_DEBUG") && (dbgTick++ % 20 == 0))
        fprintf(stderr, "[mob]%s pos(%.1f,%.1f,%.1f) vel(%.3f,%.3f,%.3f) speed=%.3f zza=%.3f navDone=%d "
                        "target=%p yaw=%.2f head=%.2f onGround=%d\n",
                type->id, pos.x, pos.y, pos.z, vel.x, vel.y, vel.z,
                speed, zza, nav.isDone() ? 1 : 0, (void*)target, yaw, yHeadRot, onGround ? 1 : 0);

    // 走路动画：每走 0.3 格一个摆腿周期
    float hs = std::sqrt(vel.x * vel.x + vel.z * vel.z);
    limbSwing += hs * 21.0f;
    limbAmount += ((hs > 0.02f ? 1.0f : 0.0f) - limbAmount) * 0.25f;
}

float Mob::hurt(float dmg) {
    if (hurtTime > 0) return 0.0f;
    health -= dmg;
    hurtTime = 10;
    lastHurtTicks = 100;   // PanicGoal 的触发窗口（原版 lastDamageSource 约 40 刻）
    if (kind == EntityKind::Enderman) fireTicks = 0;
    if (health <= 0) dead = true;
    return dmg;
}

// ---- travel（LivingEntity.travelInAir 的移植）----
void Mob::travel(World& w) {
    // applyInput：输入衰减 0.98（原版 LivingEntity.applyInput）
    xxa *= 0.98f;
    zza *= 0.98f;

    bool inFluid = mobInFluid(*this);

    // 跳跃：JumpControl 置 jumping；水里上浮 0.04/刻，地面跳 0.42（原版 jumpFromGround）
    if (jumping) {
        if (inFluid) {
            vel.y += 0.04f;
        } else if (onGround && noJumpDelay == 0) {
            vel.y = 0.42f;
            noJumpDelay = 10;
        }
    }
    if (noJumpDelay > 0) noJumpDelay--;

    // moveRelative：getFrictionInfluencedSpeed —— 地面 = speed，液体/空中 = 0.02。
    // 注意原版 Mob.setSpeed 会把 zza 同步为 speed，所以 input 长度 ≤ 1，
    // 不会触发归一化 → 加速度 = speed²（这就是原版生物的实际加速度）。
    float spd = (onGround && !inFluid) ? speed : 0.02f;
    float len2 = xxa * xxa + zza * zza;
    if (len2 > 1e-7f) {
        float ix = xxa, iz = zza;
        if (len2 > 1.0f) {
            float inv = 1.0f / std::sqrt(len2);
            ix *= inv; iz *= inv;
        }
        float sy = std::sin(yaw), cy = std::cos(yaw);
        vel.x += ix * cy + iz * sy;
        vel.z += iz * cy - ix * sy;
    }

    moveWithCollisions(w);
    if (dead) return;

    // 重力 + 阻力（travelInAir 尾部：水平 * 摩擦*0.91，竖直 *0.98）
    vel.y -= gravityPerTick();
    if (inFluid) {
        vel.x *= 0.8f;
        vel.z *= 0.8f;
        vel.y = vel.y * 0.8f - 0.02f;
    } else {
        float f = onGround ? 0.546f : 0.91f;
        vel.x *= f;
        vel.z *= f;
        vel.y *= 0.98f;
    }
}

// travel 里的 move()：分轴碰撞 + 摔落伤害（与原版 Entity.move 等价的简化）
void Mob::moveWithCollisions(World& w) {
    float hw = type->width * 0.5f, hh = type->height;
    bool wasOnGround = onGround;
    // X
    float nx = pos.x + vel.x;
    if (!mobs::boxSolid(w, nx - hw, pos.y, pos.z - hw, nx + hw, pos.y + hh, pos.z + hw)) {
        pos.x = nx;
    } else {
        vel.x = 0;
    }
    // Z
    float nz = pos.z + vel.z;
    if (!mobs::boxSolid(w, pos.x - hw, pos.y, nz - hw, pos.x + hw, pos.y + hh, nz + hw)) {
        pos.z = nz;
    } else {
        vel.z = 0;
    }
    // Y
    float ny = pos.y + vel.y;
    if (!mobs::boxSolid(w, pos.x - hw, ny, pos.z - hw, pos.x + hw, ny + hh, pos.z + hw)) {
        if (vel.y < 0) fallDistance += -vel.y;
        pos.y = ny;
        onGround = false;
    } else if (vel.y < 0) {
        pos.y = (float)std::floor(ny) + 1.0f;
        if (!wasOnGround && takesFallDamage() && fallDistance > 3.0f)
            hurt(fallDistance - 3.0f);
        fallDistance = 0.0f;
        onGround = true;
        vel.y = 0;
    } else {
        pos.y = (float)std::floor(ny + hh) - hh - 1e-4f;   // 撞头
        vel.y = 0;
    }
    if (pos.y < WORLD_MIN_Y) { health = 0; dead = true; }
}

// ---------------------------------------------------------------------------
// Goal 注册（各生物的 registerGoals 抄自原版同名方法，没实现的 Goal 略去）
// ---------------------------------------------------------------------------
void PassiveMob::registerGoals() {
    // 猪/牛/羊/鸡：Sheep.registerGoals（Pig 相同，略去 Breed/Tempt/FollowParent/EatBlock）
    addGoal<ai::FloatGoal>(0);
    addGoal<ai::PanicGoal>(1, 1.25);
    addGoal<ai::WaterAvoidingRandomStrollGoal>(6, 1.0);
    addGoal<ai::LookAtPlayerGoal>(7, 6.0f);
    addGoal<ai::RandomLookAroundGoal>(8);
}

void HostileMob::registerGoals() {
    // Zombie.registerGoals：Float/MeleeAttack/Stroll/LookAt/RandomLook
    addGoal<ai::FloatGoal>(0);
    addGoal<ai::MeleeAttackGoal>(1, 1.0);
    addGoal<ai::WaterAvoidingRandomStrollGoal>(8, 1.0);
    addGoal<ai::LookAtPlayerGoal>(8, 8.0f);
    addGoal<ai::RandomLookAroundGoal>(9);
}

// 骷髅：远程射箭（RangedBowAttackGoal 的精简版）
namespace {
class SkeletonBowGoal : public ai::Goal {
public:
    explicit SkeletonBowGoal(Mob& m) : m_(m) { setFlags({ai::Flag::MOVE, ai::Flag::LOOK}); }
    bool canUse() override { return m_.target && !m_.target->dead; }
    bool canContinueToUse() override { return m_.target && !m_.target->dead; }
    void stop() override { m_.nav.stop(); }
    void tick() override {
        Entity* t = m_.target;
        float dx = t->pos.x - m_.pos.x, dz = t->pos.z - m_.pos.z;
        float d = std::sqrt(dx * dx + dz * dz);
        m_.lookCtl.setLookAt(t->pos.x, t->pos.y + 1.0f, t->pos.z, 40.0f * PI / 180.0f,
                             50.0f * PI / 180.0f);
        if (d > 10.0f) {
            m_.nav.moveToEntity(*t, 1.0);          // 拉开就追
        } else if (d < 6.0f) {
            // 太近就后退（原版 RangedBowAttackGoal 的后撤分支）
            m_.nav.moveTo(m_.pos.x - dx / d * 5.0f, m_.pos.y, m_.pos.z - dz / d * 5.0f, 1.0);
        } else {
            m_.nav.stop();                          // 距离合适站定射箭
        }
        if (m_.attackCooldown == 0 && d < 15.0f) {
            Vec3 from(m_.pos.x, m_.pos.y + m_.type->eyeHeight, m_.pos.z);
            Vec3 to(t->pos.x, t->pos.y + 1.0f, t->pos.z);
            if (hasLineOfSight(*m_.world, from, to)) {
                auto a = std::make_unique<Arrow>();
                a->pos = from;
                Vec3 dir = normalize(to - from);
                a->vel = dir * 1.5f;
                a->yaw = std::atan2(dir.x, dir.z);
                a->pitch = std::asin(clampf(dir.y, -1.0f, 1.0f));
                m_.world->spawnEntity(std::move(a));
                m_.attackCooldown = 30;
            }
        }
    }
private:
    Mob& m_;
};
} // 匿名命名空间

void Skeleton::registerGoals() {
    addGoal<ai::FloatGoal>(0);
    addGoal<SkeletonBowGoal>(1);
    addGoal<ai::WaterAvoidingRandomStrollGoal>(8, 1.0);
    addGoal<ai::LookAtPlayerGoal>(8, 8.0f);
    addGoal<ai::RandomLookAroundGoal>(9);
}

// ---------------------------------------------------------------------------
// 各生物构造与特殊行为
// ---------------------------------------------------------------------------
Pig::Pig() {
    kind = EntityKind::Pig; type = &ENTITY_PIG;
    maxHealth = health = 10.0f; moveSpeed = 0.25f; hostile = false;
    yHeadRot = yaw; registerGoals();
}
Cow::Cow() {
    kind = EntityKind::Cow; type = &ENTITY_COW;
    maxHealth = health = 10.0f; moveSpeed = 0.20f; hostile = false;
    yHeadRot = yaw; registerGoals();
}
Sheep::Sheep() {
    kind = EntityKind::Sheep; type = &ENTITY_SHEEP;
    maxHealth = health = 8.0f; moveSpeed = 0.23f; hostile = false;
    yHeadRot = yaw; registerGoals();
}
Chicken::Chicken() {
    kind = EntityKind::Chicken; type = &ENTITY_CHICKEN;
    maxHealth = health = 4.0f; moveSpeed = 0.25f; hostile = false;
    yHeadRot = yaw; registerGoals();
}
void Chicken::aiExtra(World&) {
    // 原版鸡下落慢：竖直速度每刻乘 0.6
    if (!onGround && vel.y < 0.0f) vel.y *= 0.6f;
}

Zombie::Zombie() {
    kind = EntityKind::Zombie; type = &ENTITY_ZOMBIE;
    maxHealth = health = 20.0f; moveSpeed = 0.23f; hostile = true; damage = 3.0f;
    yHeadRot = yaw; registerGoals();
}
Skeleton::Skeleton() {
    kind = EntityKind::Skeleton; type = &ENTITY_SKELETON;
    maxHealth = health = 20.0f; moveSpeed = 0.25f; hostile = true; damage = 2.0f;
    yHeadRot = yaw; registerGoals();
}
Creeper::Creeper() {
    kind = EntityKind::Creeper; type = &ENTITY_CREEPER;
    maxHealth = health = 20.0f; moveSpeed = 0.23f; hostile = true; damage = 0.0f;
    yHeadRot = yaw; registerGoals();
}
void Creeper::aiExtra(World& w) {
    // 原版 SwellGoal：3 格内点燃引信，30 刻爆炸；退到 7 格外取消。
    // 引信期间导航停止（站定不动）。
    if (target && !target->dead) {
        float dx = target->pos.x - pos.x, dz = target->pos.z - pos.z;
        float d2 = dx * dx + dz * dz;
        if (d2 < 9.0f) {
            if (fuse < 0) fuse = 0.0f;
            fuse += 1.0f;
            nav.stop();
            if (fuse >= 30.0f) {
                Vec3 c(pos.x, pos.y + 0.5f, pos.z);
                dead = true;
                mobs::explode(w, c, 3.0f, false);
            }
            return;
        }
        if (d2 > 49.0f && fuse >= 0) fuse -= 1.0f;
    }
    if (fuse >= 0) {
        fuse -= 1.0f;
        if (fuse < 0) fuse = -1.0f;
    }
}

Spider::Spider() {
    kind = EntityKind::Spider; type = &ENTITY_SPIDER;
    maxHealth = health = 16.0f; moveSpeed = 0.30f; hostile = true; damage = 3.0f;
    yHeadRot = yaw; registerGoals();
}
Blaze::Blaze() {
    kind = EntityKind::Blaze; type = &ENTITY_BLAZE;
    maxHealth = health = 20.0f; moveSpeed = 0.23f; hostile = true; damage = 6.0f;
    yHeadRot = yaw;
}
void Blaze::aiExtra(World& w) {
    // 原版 Blaze：悬浮在玩家上方，间隔发射 3 发火球（FlyingMoveControl，不用地面寻路）
    Entity* p = mobs::findPlayer(w);
    if (!p) {
        vel.y += std::sin(age * 2.0f) * 0.01f;
        return;
    }
    lookCtl.setLookAt(p->pos.x, p->pos.y + 1.2f, p->pos.z, 40.0f * PI / 180.0f, 50.0f * PI / 180.0f);
    float dx = p->pos.x - pos.x, dz = p->pos.z - pos.z;
    float d = std::sqrt(dx * dx + dz * dz);
    if (d > 16.0f) return;
    // 保持高度：目标 = 玩家上方 2 格
    float want = p->pos.y + 2.0f;
    vel.y += clampf((want - pos.y) * 0.05f, -0.05f, 0.05f);
    if (d > 4.0f) {
        vel.x += (dx / d) * moveSpeed * 0.05f;
        vel.z += (dz / d) * moveSpeed * 0.05f;
    }
    if (attackCooldown == 0) {
        Vec3 from(pos.x, pos.y + type->height * 0.6f, pos.z);
        Vec3 to(p->pos.x, p->pos.y + 1.2f, p->pos.z);
        Vec3 dir = normalize(to - from);
        for (int i = 0; i < 3; i++) {
            auto f = std::make_unique<Fireball>();
            f->pos = from;
            Vec3 d2 = normalize(dir + Vec3((i - 1) * 0.08f, (i - 1) * 0.05f, 0));
            f->vel = d2 * 0.5f;
            w.spawnEntity(std::move(f));
        }
        attackCooldown = 40 + rndInt(40);
    }
}
Enderman::Enderman() {
    kind = EntityKind::Enderman; type = &ENTITY_ENDERMAN;
    maxHealth = health = 40.0f; moveSpeed = 0.30f; hostile = true; damage = 7.0f;
    yHeadRot = yaw; registerGoals();
}
void Enderman::aiExtra(World& w) {
    // 怕水：碰到水就受伤并瞬移走（原版 waterSensitivity）
    uint8_t feet = w.getBlock((int)std::floor(pos.x), (int)std::floor(pos.y + 0.1f),
                              (int)std::floor(pos.z));
    if (feet == B_WATER) {
        if (hurtTime == 0) hurt(1.0f);
        if (teleportTimer == 0) { teleportRandom(w); teleportTimer = 20; }
        return;
    }
    if (teleportTimer > 0) teleportTimer--;

    // 被玩家注视才敌对（原版 EndermanEndermiteTargeting/StareGoal）
    Entity* p = mobs::findPlayer(w);
    if (p) {
        float dx = p->pos.x - pos.x, dz = p->pos.z - pos.z;
        float d = std::sqrt(dx * dx + dz * dz);
        if (d < 64.0f) {
            Vec3 eye(p->pos.x, p->pos.y + 1.62f, p->pos.z);
            Vec3 head(pos.x, pos.y + type->eyeHeight, pos.z);
            Vec3 look(std::sin(p->yaw) * std::cos(p->pitch), std::sin(p->pitch),
                      std::cos(p->yaw) * std::cos(p->pitch));
            Vec3 toHead = normalize(head - eye);
            if (dot(look, toHead) > 0.90f && hasLineOfSight(w, eye, head)) {
                angerTicks = 600;   // 原版 DATA_ANGRY 持续到不再被激怒
                target = p;
            }
        }
    }
    if (angerTicks > 0) {
        angerTicks--;
        if (angerTicks == 0) target = nullptr;
    } else if (lastHurtTicks <= 0) {
        target = nullptr;
    }
}

// 末影人瞬移（原版 teleport 的简化：周围 16 格随机找落点）
void Enderman::teleportRandom(World& w) {
    for (int i = 0; i < 16; i++) {
        int nx = (int)std::floor(pos.x) + rndInt(17) - 8;
        int ny = (int)std::floor(pos.y) + rndInt(9) - 4;
        int nz = (int)std::floor(pos.z) + rndInt(17) - 8;
        if (ny < WORLD_MIN_Y || ny >= WORLD_HEIGHT - 2) continue;
        if (mobs::boxSolid(w, (float)nx, (float)ny, (float)nz,
                           (float)nx + 0.6f, (float)ny + type->height, (float)nz + 0.6f)) continue;
        if (mobs::boxSolid(w, (float)nx, (float)ny - 1.0f, (float)nz,
                           (float)nx + 0.6f, (float)ny, (float)nz + 0.6f)) continue;
        pos = Vec3((float)nx + 0.5f, (float)ny, (float)nz + 0.5f);
        fallDistance = 0.0f;
        nav.stop();
        return;
    }
}

// ---------------------------------------------------------------------------
// 玩家实体 / 弹射物（与之前一致）
// ---------------------------------------------------------------------------
PlayerEntity::PlayerEntity() {
    kind = EntityKind::Player;
    type = &ENTITY_PLAYER;
    maxHealth = health = 20.0f;
}
void PlayerEntity::tick(World& w, float dt) {
    Entity::tick(w, dt);
    if (hurtTime > 0) hurtTime--;
    if (hurtFlash > 0) hurtFlash--;
    if (fireTicks > 0) {
        fireTicks--;
        if (fireTicks % 20 == 0) hurt(1.0f);
    }
    if (hurtTime > 0 || health >= maxHealth) { regenTimer = 0; return; }
    if (++regenTimer >= 100) {
        regenTimer = 60;
        health = health + 1.0f > maxHealth ? maxHealth : health + 1.0f;
    }
}
float PlayerEntity::hurt(float dmg) {
    if (hurtTime > 0) return 0.0f;
    health -= dmg;
    hurtTime = 10;
    hurtFlash = 10;
    if (health <= 0.0f) { health = 0.0f; }   // 死亡处理在主循环（回出生点）
    return dmg;
}

Arrow::Arrow() {
    kind = EntityKind::Arrow;
    type = &ENTITY_ARROW;
    vel.y = -0.05f;
}
void Arrow::tick(World& w, float dt) {
    (void)dt;
    age += 0.05f;
    Vec3 next = pos + vel;
    // 命中方块：钉住消失
    if (mobs::boxSolid(w, next.x - 0.05f, next.y - 0.05f, next.z - 0.05f,
                       next.x + 0.05f, next.y + 0.05f, next.z + 0.05f)) {
        dead = true;
        return;
    }
    pos = next;
    vel.y -= 0.05f;
    vel.x *= 0.99f;
    vel.z *= 0.99f;
    if (age > 60.0f) dead = true;
    // 命中实体
    for (auto& e : w.entities()) {
        if (!e || e->dead || e.get() == this) continue;
        if (e->kind == EntityKind::Player || e->kind == EntityKind::Zombie ||
            e->kind == EntityKind::Skeleton || e->kind == EntityKind::Creeper ||
            e->kind == EntityKind::Pig || e->kind == EntityKind::Cow ||
            e->kind == EntityKind::Sheep || e->kind == EntityKind::Chicken) {
            float hw = e->type->width * 0.5f;
            if (pos.x > e->pos.x - hw && pos.x < e->pos.x + hw &&
                pos.z > e->pos.z - hw && pos.z < e->pos.z + hw &&
                pos.y > e->pos.y && pos.y < e->pos.y + e->type->height) {
                e->hurt(3.0f);
                mobs::knockback(*e, pos - vel, 0.3f);
                dead = true;
                return;
            }
        }
    }
}

Fireball::Fireball() {
    kind = EntityKind::Fireball;
    type = &ENTITY_FIREBALL;
}
void Fireball::tick(World& w, float dt) {
    (void)dt;
    age += 0.05f;
    Vec3 next = pos + vel;
    if (mobs::boxSolid(w, next.x - 0.2f, next.y - 0.2f, next.z - 0.2f,
                       next.x + 0.2f, next.y + 0.2f, next.z + 0.2f)) {
        mobs::explode(w, pos, 1.0f, true);
        dead = true;
        return;
    }
    pos = next;
    if (age > 60.0f) dead = true;
    for (auto& e : w.entities()) {
        if (!e || e->dead || e.get() == this) continue;
        if (e->kind == EntityKind::Player) {
            float hw = e->type->width * 0.5f;
            if (pos.x > e->pos.x - hw && pos.x < e->pos.x + hw &&
                pos.z > e->pos.z - hw && pos.z < e->pos.z + hw &&
                pos.y > e->pos.y && pos.y < e->pos.y + e->type->height) {
                e->hurt(5.0f);
                mobs::explode(w, pos, 1.0f, true);
                dead = true;
                return;
            }
        }
    }
}
