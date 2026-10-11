// ===========================================================================
// mobspawn.cpp — 生物刷新
//
// 原版要点（SpawnPlacements / MobSpawnSettings）：
//  * 敌对要在暗处（亮度 ≤ 7），被动要亮（天空光 ≥ 9）且脚下是草方块；
//  * 生成点在玩家 24~128 格的随机水平位置，再找该处的合法落点；
//  * 每个维度有独立数量上限（敌对 70 / 被动 10）；
//  * 离玩家超过 128 格的生物直接清除。
// 本工程没有光照引擎，亮度用 mobs::lightAt 的近似值（天空光按遮挡算 +
// 附近发光方块按距离衰减），够刷怪判定用。
// ===========================================================================
#include "mobspawn.hpp"
#include "blocks.hpp"
#include "mobs.hpp"
#include "specs.hpp"
#include "world.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

namespace {

// 刷新用的随机源（不需要存档一致）
uint64_t g_rng = 0x2545F4914F6CDD1DULL;
inline uint32_t rnd() {
    g_rng += 0x9e3779b97f4a7c15ULL;
    uint64_t z = g_rng;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return (uint32_t)(z ^ (z >> 31));
}
inline float rndF() { return (rnd() >> 8) * (1.0f / 16777216.0f); }
inline int rndInt(int n) { return n <= 0 ? 0 : (int)(rndF() * n); }

// 数量上限（原版：敌对 70、被动 10）
constexpr int kHostileCap = 70;
constexpr int kPassiveCap = 10;
// 每次尝试的生成点搜索半径（原版 spawnRadius 随机 10~128，这里取 24~128）
constexpr int kMinDist = 24;
constexpr int kMaxDist = 128;
// 离玩家超过这个距离就清除（原版 despawn 128 格）
constexpr float kDespawnDist = 128.0f;

struct SpawnEntry {
    EntityKind kind;
    bool passive;      // true=被动分组（另一套上限与亮度规则）
    int weight;        // 原版权重（同类总和约 100）
};

// 各维度的刷新表。原版下界有猪灵/恶魂/岩浆怪/凋灵骷髅，本工程没做，
// 只保留原版确实会自然刷新的那几种；末地刷末影人。
const SpawnEntry kOverworld[] = {
    {EntityKind::Pig,      true,  10}, {EntityKind::Cow,     true,  10},
    {EntityKind::Sheep,    true,  5},  {EntityKind::Chicken, true,  5},
    {EntityKind::Zombie,   false, 100}, {EntityKind::Skeleton, false, 100},
    {EntityKind::Creeper,  false, 100}, {EntityKind::Spider,  false, 100},
    {EntityKind::Enderman, false, 10},
};
const SpawnEntry kNether[] = {
    {EntityKind::Zombie,   false, 100},   // 原版下界僵尸权重低于主世界
    {EntityKind::Skeleton, false, 100},   // 普通骷髅也会在下界刷
    {EntityKind::Enderman, false, 20},    // 末影人下界常见
    // 烈焰人：原版只从下界要塞刷怪笼出，没有要塞就不刷
};
const SpawnEntry kEnd[] = {
    {EntityKind::Enderman, false, 100},   // 末地刷末影人
};

const SpawnEntry* tableFor(DimensionId d, int& n) {
    switch (d) {
    case DIM_NETHER: n = (int)(sizeof(kNether) / sizeof(SpawnEntry)); return kNether;
    case DIM_END:    n = (int)(sizeof(kEnd) / sizeof(SpawnEntry));    return kEnd;
    default:         n = (int)(sizeof(kOverworld) / sizeof(SpawnEntry)); return kOverworld;
    }
}

const char* kindId(EntityKind k) {
    switch (k) {
    case EntityKind::Pig: return "pig";
    case EntityKind::Cow: return "cow";
    case EntityKind::Sheep: return "sheep";
    case EntityKind::Chicken: return "chicken";
    case EntityKind::Zombie: return "zombie";
    case EntityKind::Skeleton: return "skeleton";
    case EntityKind::Creeper: return "creeper";
    case EntityKind::Spider: return "spider";
    case EntityKind::Blaze: return "blaze";
    case EntityKind::Enderman: return "enderman";
    default: return nullptr;
    }
}

} // 匿名命名空间

namespace mobspawn {

bool kindByName(const char* name, EntityKind& out) {
    static const EntityKind kAll[] = {
        EntityKind::Pig, EntityKind::Cow, EntityKind::Sheep, EntityKind::Chicken,
        EntityKind::Zombie, EntityKind::Skeleton, EntityKind::Creeper, EntityKind::Spider,
        EntityKind::Blaze, EntityKind::Enderman,
    };
    for (EntityKind k : kAll)
        if (std::strcmp(kindId(k), name) == 0) { out = k; return true; }
    return false;
}

Entity* summon(World& w, EntityKind kind, const Vec3& at, int count) {
    std::unique_ptr<Entity> e;
    switch (kind) {
    case EntityKind::Pig:      e = std::make_unique<Pig>(); break;
    case EntityKind::Cow:      e = std::make_unique<Cow>(); break;
    case EntityKind::Sheep:    e = std::make_unique<Sheep>(); break;
    case EntityKind::Chicken:  e = std::make_unique<Chicken>(); break;
    case EntityKind::Zombie:   e = std::make_unique<Zombie>(); break;
    case EntityKind::Skeleton: e = std::make_unique<Skeleton>(); break;
    case EntityKind::Creeper:  e = std::make_unique<Creeper>(); break;
    case EntityKind::Spider:   e = std::make_unique<Spider>(); break;
    case EntityKind::Blaze:    e = std::make_unique<Blaze>(); break;
    case EntityKind::Enderman: e = std::make_unique<Enderman>(); break;
    default: return nullptr;
    }
    e->pos = at;
    Entity* raw = e.get();
    w.spawnEntity(std::move(e));
    for (int i = 1; i < count; i++) {
        std::unique_ptr<Entity> more;
        switch (kind) {
        case EntityKind::Pig:      more = std::make_unique<Pig>(); break;
        case EntityKind::Cow:      more = std::make_unique<Cow>(); break;
        case EntityKind::Sheep:    more = std::make_unique<Sheep>(); break;
        case EntityKind::Chicken:  more = std::make_unique<Chicken>(); break;
        case EntityKind::Zombie:   more = std::make_unique<Zombie>(); break;
        case EntityKind::Skeleton: more = std::make_unique<Skeleton>(); break;
        case EntityKind::Creeper:  more = std::make_unique<Creeper>(); break;
        case EntityKind::Spider:   more = std::make_unique<Spider>(); break;
        case EntityKind::Blaze:    more = std::make_unique<Blaze>(); break;
        default:                   more = std::make_unique<Enderman>(); break;
        }
        // 散开摆，免得叠在一起（原版一次生成也是散开的）
        more->pos = at + Vec3((rndF() - 0.5f) * 4.0f, 0.0f, (rndF() - 0.5f) * 4.0f);
        w.spawnEntity(std::move(more));
    }
    return raw;
}

namespace {

// 找一个合法落点：返回 true 时 y 是脚底所在高度
bool findSpawn(World& w, int bx, int bz, bool passive, int& outY) {
    // 只在已加载的区块里找（未加载区块 getBlock 返回空气，会误判成空）
    auto c = w.chunkAt(blockToChunkCoord(bx), blockToChunkCoord(bz));
    if (!c || c->state.load() < 1) return false;

    for (int y = WORLD_HEIGHT - 2; y > WORLD_MIN_Y; y--) {
        uint8_t ground = w.getBlock(bx, y, bz);
        if (!blockIsSolid(ground)) continue;
        uint8_t above1 = w.getBlock(bx, y + 1, bz);
        if (above1 != B_AIR) continue;      // 顶上被占（比如草/水）就换地方
        // 被动生物必须站在草方块上（原版 PassiveMobSpawnRules）
        if (passive && ground != B_GRASS) return false;
        // 敌对生物要实心不透明的方块（原版是"完整实心方块"，这里用 solid 判断）
        if (!passive && !blockIsOpaque(ground)) return false;
        // 头顶 2 格要空（蜘蛛 0.9 高、鸡 0.7 高，2 格够所有小型生物）
        if (w.getBlock(bx, y + 2, bz) != B_AIR) continue;
        int light = mobs::lightAt(w, bx, y + 1, bz);
        if (passive && light < 9) return false;
        if (!passive && light > 7) return false;
        outY = y + 1;
        return true;
    }
    return false;
}

} // 匿名命名空间

void tick(World& w, const Vec3& playerPos, int renderDistBlocks, float dt) {
    // 1) 清除离玩家太远的生物（原版：128 格外直接消失）
    for (auto& up : w.entities()) {
        if (!up || up->dead) continue;
        if (up->kind == EntityKind::Player || up->kind == EntityKind::FallingBlock) continue;
        Vec3 d = up->pos - playerPos;
        if (std::fabs(d.y) > 64.0f || std::fabs(d.x) > kDespawnDist || std::fabs(d.z) > kDespawnDist)
            up->dead = true;
    }

    // 2) 每秒尝试若干次生成（原版每次 attempt 也是挑一个随机点）
    static float acc = 0.0f;
    acc += dt * (float)TICK_RATE;
    if (acc < (float)TICK_RATE) return;
    acc = 0.0f;

    int nTable = 0;
    const SpawnEntry* table = tableFor(w.getDimension(), nTable);
    if (!table || nTable <= 0) return;

    int hostile = 0, passive = 0;
    for (auto& up : w.entities()) {
        if (!up || up->dead) continue;
        switch (up->kind) {
        case EntityKind::Pig: case EntityKind::Cow: case EntityKind::Sheep:
        case EntityKind::Chicken:
            passive++; break;
        case EntityKind::Zombie: case EntityKind::Skeleton: case EntityKind::Creeper:
        case EntityKind::Spider: case EntityKind::Blaze: case EntityKind::Enderman:
            hostile++; break;
        default: break;
        }
    }

    // 渲染距离之外的区块不生成（没有地形可言）
    int maxDist = std::min(kMaxDist, std::max(kMinDist + 8, renderDistBlocks));
    const int kTries = 4;
    for (int t = 0; t < kTries; t++) {
        // 按权重挑一个种类
        int total = 0;
        for (int i = 0; i < nTable; i++) total += table[i].weight;
        int roll = rndInt(total);
        const SpawnEntry* pick = &table[0];
        for (int i = 0; i < nTable; i++) {
            roll -= table[i].weight;
            if (roll < 0) { pick = &table[i]; break; }
        }
        if (pick->passive && passive >= kPassiveCap) continue;
        if (!pick->passive && hostile >= kHostileCap) continue;

        // 24~maxDist 的随机水平位置（原版：24 格内不刷）
        float a = rndF() * 6.2831853f;
        float r = kMinDist + rndF() * (float)(maxDist - kMinDist);
        int bx = (int)std::floor(playerPos.x + std::cos(a) * r);
        int bz = (int)std::floor(playerPos.z + std::sin(a) * r);
        int y = 0;
        if (!findSpawn(w, bx, bz, pick->passive, y)) {
            // 落点不合格（没方块 / 亮度不对 / 区块没加载）就换一个点，原版也是这样反复试
            if (getenv("OPENCRAFT_SPAWN_DEBUG"))
                fprintf(stderr, "[spawn] %s (%d,%d) 没有合法落点\n", kindId(pick->kind), bx, bz);
            continue;
        }

        // 一次生成一小群（原版 passive 2~4、hostile 1~4）
        int n = pick->passive ? (2 + rndInt(3)) : (1 + rndInt(4));
        summon(w, pick->kind, Vec3(bx + 0.5f, (float)y, bz + 0.5f), n);
        if (pick->passive) passive += n; else hostile += n;
    }
}

} // namespace mobspawn
