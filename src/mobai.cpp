// ===========================================================================
// mobai.cpp — 原版 AI 架构移植（对照 26.2：GoalSelector/MoveControl/LookControl/
// JumpControl/PathNavigation 与常用 Goal）
// ===========================================================================
#include "mobai.hpp"
#include "mobs.hpp"
#include "blocks.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstdio>

namespace ai {

static const float PI = 3.14159265f;
static const float DEG2RAD = PI / 180.0f;

// 实体脚下是否在液体里
static bool inFluid(const Mob& m) {
    if (!m.world) return false;
    uint8_t b = m.world->getBlock((int)std::floor(m.pos.x),
                                  (int)std::floor(m.pos.y + 0.1f),
                                  (int)std::floor(m.pos.z));
    return b == B_WATER || b == B_LAVA;
}

// Mth.wrapDegrees 的弧度版：把差值折到 (-π, π]
static float wrapPi(float a) {
    while (a > PI) a -= 2 * PI;
    while (a <= -PI) a += 2 * PI;
    return a;
}
// Mth.rotateIfNecessary：把 base 朝 target 转，最多 maxDiff
static float rotateIfNecessary(float base, float target, float maxDiff) {
    float d = wrapPi(target - base);
    return base + std::clamp(d, -maxDiff, maxDiff);
}

// ---- GoalSelector（GoalSelector.java）----
void GoalSelector::add(int prio, Goal* g) {
    goals_.push_back({prio, g, false});
}
void GoalSelector::tick(Mob& mob) {
    (void)mob;
    // cleanup：正在跑但条件不再满足的停掉
    for (auto& w : goals_) {
        if (w.running && !w.goal->canContinueToUse()) {
            w.goal->stop();
            w.running = false;
        }
    }
    // 锁定的标志位如果对应 goal 已经不在跑，就解锁
    for (int f = 0; f <= (int)Flag::TARGET; f++)
        if (locked_[f]) {
            bool stillRunning = false;
            for (auto& w : goals_)
                if (w.goal == locked_[f] && w.running) stillRunning = true;
            if (!stillRunning) locked_[f] = nullptr;
        }
    // update：能启动的启动（标志位被占用则不让路）
    for (auto& w : goals_) {
        if (w.running) continue;
        bool flagsFree = true;
        for (int f = 0; f <= (int)Flag::TARGET; f++) {
            if (!w.goal->hasFlag((Flag)f)) continue;
            Goal* holder = locked_[f];
            if (holder && holder != w.goal) flagsFree = false;
        }
        if (flagsFree && w.goal->canUse()) {
            for (int f = 0; f <= (int)Flag::TARGET; f++) {
                if (!w.goal->hasFlag((Flag)f)) continue;
                if (locked_[f] && locked_[f] != w.goal) locked_[f]->stop();
                locked_[f] = w.goal;
            }
            w.goal->start();
            w.running = true;
        }
    }
    // tick 所有正在跑的（原版 tickRunningGoals(true)）
    for (auto& w : goals_)
        if (w.running) w.goal->tick();
}

// ---- JumpControl ----
void JumpControl::tick(Mob& mob) {
    mob.jumping = want_;
    want_ = false;
}

// ---- MoveControl（MoveControl.java）----
void MoveControl::setWantedPosition(double x, double y, double z, double speedModifier) {
    wx_ = x; wy_ = y; wz_ = z;
    speedModifier_ = speedModifier;
    if (op_ != Op::JUMPING) op_ = Op::MOVE_TO;
}
float MoveControl::rotlerp(float a, float b, float max) {
    float diff = wrapPi(b - a);
    diff = std::clamp(diff, -max, max);
    return a + diff;
}
void MoveControl::tick(Mob& mob) {
    if (op_ == Op::MOVE_TO) {
        op_ = Op::WAIT;
        double xd = wx_ - mob.pos.x;
        double zd = wz_ - mob.pos.z;
        double yd = wy_ - mob.pos.y;
        double dd = xd * xd + yd * yd + zd * zd;
        if (dd < 2.5e-7) { mob.zza = 0.0f; return; }
        // 目标朝向（本引擎约定：yaw=atan2(dx,dz)，0=+Z）
        float yRotD = std::atan2((float)xd, (float)zd);
        mob.yaw = rotlerp(mob.yaw, yRotD, 90.0f * DEG2RAD);   // MAX_TURN = 90°/刻
        mob.setSpeed((float)(speedModifier_ * mob.moveSpeed));   // 同时写 zza
        // 需要跳上去就跳（原版：yd > maxUpStep 且水平距离很近）
        if (yd > mob.maxUpStep && xd * xd + zd * zd < std::max(1.0, (double)mob.type->width)) {
            mob.jumpCtl.jump();
            op_ = Op::JUMPING;
        }
    } else if (op_ == Op::JUMPING) {
        mob.setSpeed((float)(speedModifier_ * mob.moveSpeed));
        if (mob.onGround || inFluid(mob)) op_ = Op::WAIT;
    } else {
        mob.zza = 0.0f;
    }
}

// ---- LookControl（LookControl.java）----
void LookControl::setLookAt(double x, double y, double z, float yMaxRotSpeed, float xMaxRotAngle) {
    wx_ = x; wy_ = y; wz_ = z;
    yMaxRotSpeed_ = yMaxRotSpeed;
    xMaxRotAngle_ = xMaxRotAngle;
    cooldown_ = 2;
}
void LookControl::tick(Mob& mob) {
    if (cooldown_ > 0) {
        cooldown_--;
        double xd = wx_ - mob.pos.x, zd = wz_ - mob.pos.z;
        if (std::abs(zd) > 1e-5 || std::abs(xd) > 1e-5) {
            // getYRotD：目标水平角
            float yRotD = std::atan2((float)zd, (float)xd);
            mob.yHeadRot = rotateIfNecessary(mob.yHeadRot, yRotD, yMaxRotSpeed_);
        }
        double yd = wy_ - (mob.pos.y + mob.type->height * 0.9f);
        double sd = std::sqrt(xd * xd + zd * zd);
        if (std::abs(yd) > 1e-5 || std::abs(sd) > 1e-5) {
            // getXRotD：正 = 低头（与模型 xRot 同号）
            float xRotD = std::atan2((float)yd, (float)sd);
            mob.pitch += std::clamp(wrapPi(xRotD - mob.pitch),
                                    -xMaxRotAngle_, xMaxRotAngle_);
            mob.pitch = std::clamp(mob.pitch, -1.5f, 1.5f);
        }
    } else {
        mob.yHeadRot = rotateIfNecessary(mob.yHeadRot, mob.yaw, 10.0f * DEG2RAD);
    }
    // clampHeadRotationToBody：走路时头相对身体最多 ±50°（getMaxHeadYRot）
    if (mob.nav.isInProgress())
        mob.yHeadRot = rotateIfNecessary(mob.yHeadRot, mob.yaw, 50.0f * DEG2RAD);
}

// ---- PathNavigation（PathNavigation.java + GroundPathNavigation.java）----
PathNavigation::PathNavigation(Mob& mob) : mob_(mob) {
    // 陆地生物可下水（配合 FloatGoal 上浮）；WATER 扣分默认 8 但可通行
}
bool PathNavigation::isDone() const { return path_.isDone(); }
void PathNavigation::stop() {
    path_.nodes.clear();
    path_.next = 0;
}
bool PathNavigation::moveTo(double x, double y, double z, double speedModifier) {
    // 原版 moveTo(createPath(x,y,z,1), speed)：maxPathLength = FOLLOW_RANGE(32)
    if (!mob_.world) return false;
    std::vector<mobpath::BlockPos3> targets;
    targets.push_back({(int)std::floor(x), (int)std::floor(y), (int)std::floor(z)});
    eval_.prepare(*mob_.world, mob_);
    mobpath::Path p = mobpath::PathFinder(512).findPath(*mob_.world, mob_, targets, 32.0f, 1, eval_);
    if (getenv("OPENCRAFT_PATH_DEBUG")) {
        auto* st = eval_.getStart();
        fprintf(stderr, "[path] 目标(%d,%d,%d) 起点(%d,%d,%d) type=%d malus=%.1f 节点数=%zu\n",
                targets[0].x, targets[0].y, targets[0].z,
                st ? st->x : -999, st ? st->y : -999, st ? st->z : -999,
                st ? (int)st->type : -1, st ? st->costMalus : -9.0f, p.nodes.size());
        if (st) {
            mobpath::Node* nb[32];
            int n = eval_.getNeighbors(nb, st);
            fprintf(stderr, "[path]   起点邻居数=%d", n);
            for (int i = 0; i < n && i < 4; i++)
                fprintf(stderr, " (%d,%d,%d)t%d", nb[i]->x, nb[i]->y, nb[i]->z, (int)nb[i]->type);
            fprintf(stderr, "\n");
        }
    }
    eval_.done();
    if (p.nodes.empty()) { stop(); return false; }
    path_ = p;
    speedModifier_ = speedModifier;
    return true;
}
bool PathNavigation::moveToEntity(const Entity& t, double speedModifier) {
    return moveTo(t.pos.x, t.pos.y, t.pos.z, speedModifier);
}
void PathNavigation::tick() {
    tick_++;
    if (isDone()) return;
    bool canUpdate = mob_.onGround || inFluid(mob_);
    if (canUpdate) {
        // followThePath：够近就推进到下一个节点
        float bbW = mob_.type->width;
        float maxDist = bbW > 0.75f ? bbW * 0.5f : 0.75f - bbW * 0.5f;
        int nx, ny, nz;
        path_.getNextNodePos(nx, ny, nz);
        float xDist = std::abs((float)mob_.pos.x - (nx + 0.5f));
        float yDist = std::abs((float)mob_.pos.y - ny);
        float zDist = std::abs((float)mob_.pos.z - (nz + 0.5f));
        if (xDist < maxDist && zDist < maxDist && yDist < 1.0f) path_.advance();
        doStuckDetection(mob_, (float)mob_.pos.x, (float)mob_.pos.z);
    }
    if (!isDone()) {
        float tx, ty, tz;
        path_.getNextEntityPos(mob_.type->width, tx, ty, tz);
        // getGroundY：本工程方块都是整格碰撞，直接用节点 y
        mob_.moveCtl.setWantedPosition(tx, (double)ty, tz, speedModifier_);
    }
}
void PathNavigation::doStuckDetection(Mob& mob, float px, float pz) {
    if (tick_ - lastStuckCheck_ > 100) {
        float eff = mob.speed >= 1.0f ? mob.speed : mob.speed * mob.speed;
        float threshold = eff * 100.0f * 0.25f;
        float dx = px - lastStuckX_, dz = pz - lastStuckZ_;
        if (dx * dx + dz * dz < threshold * threshold) stop();
        lastStuckCheck_ = tick_;
        lastStuckX_ = px;
        lastStuckZ_ = pz;
    }
    if (!isDone()) {
        int nx, ny, nz;
        path_.getNextNodePos(nx, ny, nz);
        long now = (long)tick_;
        if (nx == timeoutCachedX_ && ny == timeoutCachedY_ && nz == timeoutCachedZ_) {
            timeoutTimer_ += now - lastTimeoutCheck_;
        } else {
            timeoutCachedX_ = nx; timeoutCachedY_ = ny; timeoutCachedZ_ = nz;
            float dx = (float)mob.pos.x - (nx + 0.5f), dz = (float)mob.pos.z - (nz + 0.5f);
            double distToNode = std::sqrt(dx * dx + dz * dz);
            timeoutLimit_ = mob.speed > 0.0f ? distToNode / mob.speed * 20.0 : 0.0;
        }
        if (timeoutLimit_ > 0.0 && timeoutTimer_ > timeoutLimit_ * 3.0) stop();
        lastTimeoutCheck_ = now;
    }
}

// ---- 随机位置工具（DefaultRandomPos / RandomPos 的等效实现）----
// 在 mob 周围找可站立点：随机方向 × 距离，向下/向上找地面，类型合法才要。
static bool findGroundPos(Mob& m, float maxDist, int tries, bool avoidWater,
                          double& ox, double& oy, double& oz) {
    World& w = *m.world;
    for (int i = 0; i < tries; i++) {
        float a = (float)(rand() % 6283) / 1000.0f;
        float d = 1.0f + (float)(rand() % 1000) / 1000.0f * (maxDist - 1.0f);
        float bx = m.pos.x + std::sin(a) * d;
        float bz = m.pos.z + std::cos(a) * d;
        int ix = (int)std::floor(bx), iz = (int)std::floor(bz);
        int iy = (int)std::floor(m.pos.y);
        // 目标附近的地面：从脚下高度向上/向下扫 ±3 格
        for (int yy = iy + 3; yy >= iy - 3; yy--) {
            uint8_t below = w.getBlock(ix, yy - 1, iz);
            if (below == B_AIR || below == B_WATER || below == B_LAVA) continue;
            if (!blockIsSolid(below)) continue;
            uint8_t at = w.getBlock(ix, yy, iz);
            uint8_t head = w.getBlock(ix, yy + 1, iz);
            if (at != B_AIR || head != B_AIR) continue;
            ox = bx; oy = (double)yy; oz = bz;
            return true;
        }
    }
    return false;
}

// ---- FloatGoal（FloatGoal.java）----
bool FloatGoal::canUse() {
    if (!m_.world) return false;
    uint8_t atFeet = m_.world->getBlock((int)std::floor(m_.pos.x),
                                        (int)std::floor(m_.pos.y + 0.1f),
                                        (int)std::floor(m_.pos.z));
    return atFeet == B_WATER || atFeet == B_LAVA;
}
void FloatGoal::tick() {
    if ((rand() % 5) != 0) m_.jumpCtl.jump();   // 原版 0.8 概率
}

// ---- PanicGoal（PanicGoal.java）----
bool PanicGoal::canUse() {
    if (m_.lastHurtTicks <= 0) return false;
    return findRandomPosition();
}
bool PanicGoal::findRandomPosition() {
    double x, y, z;
    if (!findGroundPos(m_, 5.0f, 10, true, x, y, z)) return false;
    px_ = x; py_ = y; pz_ = z;
    return true;
}
void PanicGoal::start() { m_.nav.moveTo(px_, py_, pz_, speed_); }
bool PanicGoal::canContinueToUse() { return m_.nav.isInProgress(); }
void PanicGoal::stop() { m_.nav.stop(); }

// ---- RandomStrollGoal（RandomStrollGoal.java）----
bool RandomStrollGoal::canUse() {
    if (rand() % (interval_ / 2 > 0 ? interval_ / 2 : 1) != 0) return false;   // reducedTickDelay
    bool ok = getPosition(wx_, wy_, wz_);
    if (getenv("OPENCRAFT_STROLL_DEBUG"))
        fprintf(stderr, "[stroll] %s 找点=%d (%.1f,%.1f,%.1f)\n", m_.type->id, ok ? 1 : 0, wx_, wy_, wz_);
    return ok;
}
bool RandomStrollGoal::getPosition(double& x, double& y, double& z) {
    return findGroundPos(m_, 10.0f, 10, false, x, y, z);
}
void RandomStrollGoal::start() {
    bool ok = m_.nav.moveTo(wx_, wy_, wz_, speed_);
    if (getenv("OPENCRAFT_STROLL_DEBUG"))
        fprintf(stderr, "[stroll] %s 开始移动=%d 目标(%.1f,%.1f,%.1f)\n", m_.type->id, ok ? 1 : 0, wx_, wy_, wz_);
}
bool RandomStrollGoal::canContinueToUse() { return m_.nav.isInProgress(); }
void RandomStrollGoal::stop() { m_.nav.stop(); }

// ---- WaterAvoidingRandomStrollGoal ----
bool WaterAvoidingRandomStrollGoal::getPosition(double& x, double& y, double& z) {
    return findGroundPos(m_, 10.0f, 10, true, x, y, z);
}

// ---- LookAtPlayerGoal（LookAtPlayerGoal.java）----
bool LookAtPlayerGoal::canUse() {
    if ((float)(rand() % 1000) / 1000.0f >= prob_) return false;
    if (m_.target) { lookAt_ = m_.target; return true; }
    if (!m_.world) return false;
    Entity* p = mobs::findPlayer(*m_.world);
    if (p && length(m_.pos - p->pos) < dist_) { lookAt_ = p; return true; }
    lookAt_ = nullptr;
    return false;
}
bool LookAtPlayerGoal::canContinueToUse() {
    if (!lookAt_ || lookAt_->dead) return false;
    return length(m_.pos - lookAt_->pos) <= dist_ && lookTime_ > 0;
}
void LookAtPlayerGoal::start() { lookTime_ = 40 + rand() % 40; }
void LookAtPlayerGoal::stop() { lookAt_ = nullptr; }
void LookAtPlayerGoal::tick() {
    if (!lookAt_) return;
    m_.lookCtl.setLookAt(lookAt_->pos.x,
                         lookAt_->pos.y + lookAt_->type->height * 0.9f,
                         lookAt_->pos.z, 40.0f * DEG2RAD, 50.0f * DEG2RAD);
    lookTime_--;
}

// ---- RandomLookAroundGoal（RandomLookAroundGoal.java）----
bool RandomLookAroundGoal::canUse() {
    return (float)(rand() % 1000) / 1000.0f < 0.02f;
}
void RandomLookAroundGoal::start() {
    double rnd = 2 * PI * (double)rand() / (double)RAND_MAX;
    relX_ = std::cos(rnd);
    relZ_ = std::sin(rnd);
    lookTime_ = 20 + rand() % 20;
}
void RandomLookAroundGoal::tick() {
    lookTime_--;
    m_.lookCtl.setLookAt(m_.pos.x + relX_, m_.pos.y + m_.type->height * 0.9f,
                         m_.pos.z + relZ_, 40.0f * DEG2RAD, 50.0f * DEG2RAD);
}

// ---- MeleeAttackGoal（MeleeAttackGoal.java 精简版）----
bool MeleeAttackGoal::canUse() {
    Entity* t = m_.target;
    if (!t || t->dead) return false;
    return length(m_.pos - t->pos) < 35.0f;   // FOLLOW_RANGE
}
bool MeleeAttackGoal::canContinueToUse() {
    Entity* t = m_.target;
    return t && !t->dead && length(m_.pos - t->pos) < 35.0f;
}
void MeleeAttackGoal::stop() {
    m_.nav.stop();
    m_.target = nullptr;
}
void MeleeAttackGoal::tick() {
    Entity* t = m_.target;
    if (!t || t->dead) return;
    float d = length(m_.pos - t->pos);
    // 每 10 刻重算路径（原版 adjustedTickDelay(20)≈10 刻一调）
    if (--recalibrate_ <= 0 || m_.nav.isDone()) {
        recalibrate_ = 10;
        m_.nav.moveToEntity(*t, speed_);
    }
    // 进入攻击距离就挥击（原版 isWithinMeleeAttackRange：平方比较 + 20 刻冷却）
    float reachSqr = m_.type->width * 2.0f * (m_.type->width * 2.0f) + t->type->width;
    if (d * d <= reachSqr && m_.attackCooldown <= 0) {
        m_.attackCooldown = 20;
        t->hurt(m_.damage);
        mobs::knockback(*t, m_.pos, 0.4f);
    }
}

} // namespace ai
