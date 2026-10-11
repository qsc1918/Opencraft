#pragma once
// ===========================================================================
// mobai.hpp — 原版生物 AI 架构移植（net/minecraft/world/entity/ai/*）
//
// 对照 26.2 反编译源码：GoalSelector/Goal、MoveControl/LookControl/JumpControl、
// GroundPathNavigation，以及被动生物常用的那几个 Goal。
// Mob（mobs.hpp）持有这些控制器，每整刻按原版顺序 tick：
//   goalSelector → navigation → moveControl → jumpControl → lookControl → travel
// ===========================================================================
#include "mobpath.hpp"
#include <vector>

struct Mob;   // mobs.hpp（避免循环包含）

namespace ai {

// ---- Goal（Goal.java）----
enum class Flag : uint8_t { MOVE, LOOK, JUMP, TARGET };

class Goal {
public:
    virtual ~Goal() = default;
    virtual bool canUse() = 0;
    virtual bool canContinueToUse() { return canUse(); }
    virtual void start() {}
    virtual void stop() {}
    virtual void tick() {}
    virtual bool requiresUpdateEveryTick() const { return false; }
    bool hasFlag(Flag f) const { return flags_ & (1u << (uint8_t)f); }
    void setFlags(std::initializer_list<Flag> fs) {
        flags_ = 0;
        for (Flag f : fs) flags_ |= 1u << (uint8_t)f;
    }
    uint32_t flags_ = 0;
};

// ---- GoalSelector（GoalSelector.java：优先级 + 互斥标志）----
class GoalSelector {
public:
    void add(int prio, Goal* g);
    void tick(Mob& mob);
private:
    struct Wrapped {
        int prio;
        Goal* goal;
        bool running = false;
    };
    std::vector<Wrapped> goals_;
    Goal* locked_[(int)Flag::TARGET + 1] = {};
};

// ---- JumpControl（JumpControl.java）----
class JumpControl {
public:
    void jump() { want_ = true; }
    void tick(Mob& mob);
private:
    bool want_ = false;
};

// ---- MoveControl（MoveControl.java）----
class MoveControl {
public:
    void setWantedPosition(double x, double y, double z, double speedModifier);
    bool hasWanted() const { return op_ == Op::MOVE_TO; }
    void tick(Mob& mob);
    double speedModifier() const { return speedModifier_; }
private:
    enum class Op { WAIT, MOVE_TO, JUMPING };
    static float rotlerp(float a, float b, float max);   // 角度插值（度）
    double wx_ = 0, wy_ = 0, wz_ = 0;
    double speedModifier_ = 1.0;
    Op op_ = Op::WAIT;
};

// ---- LookControl（LookControl.java）----
class LookControl {
public:
    void setLookAt(double x, double y, double z, float yMaxRotSpeed, float xMaxRotAngle);
    void tick(Mob& mob);
private:
    double wx_ = 0, wy_ = 0, wz_ = 0;
    float yMaxRotSpeed_ = 40.0f, xMaxRotAngle_ = 50.0f;
    int cooldown_ = 0;
};

// ---- GroundPathNavigation（PathNavigation.java + GroundPathNavigation.java）----
class PathNavigation {
public:
    explicit PathNavigation(Mob& mob);
    // 到指定点（自动算路径）；找不到路径返回 false
    bool moveTo(double x, double y, double z, double speedModifier);
    bool moveToEntity(const Entity& target, double speedModifier);
    void stop();
    bool isDone() const;
    bool isInProgress() const { return !isDone(); }
    void tick();
    mobpath::NodeEvaluator& evaluator() { return eval_; }
    double speedModifier() const { return speedModifier_; }

private:
    void followThePath(Mob& mob);
    void doStuckDetection(Mob& mob, float px, float pz);
    Mob& mob_;
    mobpath::Path path_;
    double speedModifier_ = 1.0;
    int tick_ = 0;
    int lastStuckCheck_ = 0;
    float lastStuckX_ = 0, lastStuckZ_ = 0;
    int timeoutCachedX_ = 0, timeoutCachedY_ = 0, timeoutCachedZ_ = 0;
    long timeoutTimer_ = 0, lastTimeoutCheck_ = 0;
    double timeoutLimit_ = 0;
    mobpath::NodeEvaluator eval_;
};

// ---- 常用 Goal（各自的 Java 对应文件已逐条对照）----

// FloatGoal：在水里就上浮（0 优先级）
class FloatGoal : public Goal {
public:
    explicit FloatGoal(Mob& m) : m_(m) { setFlags({Flag::JUMP}); }
    bool canUse() override;
    bool requiresUpdateEveryTick() const override { return true; }
    void tick() override;
private:
    Mob& m_;
};

// PanicGoal：受击后找随机点逃窜（PanicGoal.java，1.25 倍速）
class PanicGoal : public Goal {
public:
    PanicGoal(Mob& m, double speedModifier) : m_(m), speed_(speedModifier) { setFlags({Flag::MOVE}); }
    bool canUse() override;
    bool canContinueToUse() override;
    void start() override;
    void stop() override;
private:
    bool findRandomPosition();
    Mob& m_;
    double speed_;
    double px_ = 0, py_ = 0, pz_ = 0;
};

// RandomStrollGoal：每 120 刻掷骰，随机找 10 格内的可走点走过去
class RandomStrollGoal : public Goal {
public:
    RandomStrollGoal(Mob& m, double speedModifier, int interval = 120)
        : m_(m), speed_(speedModifier), interval_(interval) { setFlags({Flag::MOVE}); }
    bool canUse() override;
    bool canContinueToUse() override;
    void start() override;
    void stop() override;
protected:
    virtual bool getPosition(double& x, double& y, double& z);
    Mob& m_;
    double speed_;
    int interval_;
    double wx_ = 0, wy_ = 0, wz_ = 0;
};

// WaterAvoidingRandomStrollGoal：陆生生物优先找不落水的随机点
class WaterAvoidingRandomStrollGoal : public RandomStrollGoal {
public:
    WaterAvoidingRandomStrollGoal(Mob& m, double speedModifier)
        : RandomStrollGoal(m, speedModifier) {}
protected:
    bool getPosition(double& x, double& y, double& z) override;
};

// LookAtPlayerGoal：偶尔看向 8 格内的玩家（LookAtPlayerGoal.java）
class LookAtPlayerGoal : public Goal {
public:
    LookAtPlayerGoal(Mob& m, float lookDistance, float probability = 0.02f)
        : m_(m), dist_(lookDistance), prob_(probability) { setFlags({Flag::LOOK}); }
    bool canUse() override;
    bool canContinueToUse() override;
    void start() override;
    void stop() override;
    void tick() override;
private:
    Mob& m_;
    float dist_;
    float prob_;
    Entity* lookAt_ = nullptr;
    int lookTime_ = 0;
};

// RandomLookAroundGoal：站着时随机看四周（RandomLookAroundGoal.java）
class RandomLookAroundGoal : public Goal {
public:
    explicit RandomLookAroundGoal(Mob& m) : m_(m) { setFlags({Flag::MOVE, Flag::LOOK}); }
    bool canUse() override;
    bool canContinueToUse() override { return lookTime_ >= 0; }
    void start() override;
    bool requiresUpdateEveryTick() const override { return true; }
    void tick() override;
private:
    Mob& m_;
    double relX_ = 0, relZ_ = 0;
    int lookTime_ = 0;
};

// MeleeAttackGoal（MeleeAttackGoal.java 的精简版）：追到目标身边打
class MeleeAttackGoal : public Goal {
public:
    MeleeAttackGoal(Mob& m, double speedModifier, bool mustSee = true)
        : m_(m), speed_(speedModifier) { setFlags({Flag::MOVE, Flag::TARGET}); }
    bool canUse() override;
    bool canContinueToUse() override;
    void stop() override;
    bool requiresUpdateEveryTick() const override { return false; }
    void tick() override;
private:
    Mob& m_;
    double speed_;
    int recalibrate_ = 0;
};

} // namespace ai
