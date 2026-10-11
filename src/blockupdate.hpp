#pragma once
// ===========================================================================
// blockupdate.hpp — 方块更新（对齐 MC Java 版的 PP 更新 / NC 更新 + 计划刻）
//
// PP 更新（Shape Update / Post-Placement Update）：方块被放置/破坏/状态变化后，
//   依次向 西、东、北、南、下、上 传播；接收方检查形状与附着性。
// NC 更新（Neighbor Changed Update）：以更新核向 西、东、下、上、北、南 传播，
//   主要服务红石元件；本工程还没做红石/流体，接收方目前只留接口。
// 顺序按原版「更新优先级」：先 NC 后 PP；传播是深度优先（见 方块更新.txt）。
//
// 本工程只实现"已有方块会受影响的"那部分（沙子/沙砾/龙蛋受重力、火把附着、
// 紫颂支撑、火），红石与流体等没有对应方块，不实现。
// ===========================================================================
#include <cstdint>

class World;

namespace bu {

// setBlock 的更新标志（对应原版 Level#setBlock 的 flag）
enum : uint32_t {
    U_NONE  = 0,
    U_NO_NC = 1u << 0,    // 不传播 NC 更新（结构/传送门批量写入）
    U_NO_PP = 1u << 1,    // 不传播 PP 更新
    U_NO_ALL = U_NO_NC | U_NO_PP,
};

// 计划刻条目（delay 单位是游戏刻，20 刻 = 1 秒）
struct TickEntry {
    int32_t x = 0, y = 0, z = 0;
    int32_t delay = 0;
    uint32_t order = 0;   // 同刻内的插入序号，保证执行顺序稳定
};

// 方块被放置/破坏/状态变化后调用：按原版顺序先 NC 后 PP。
void onBlockChanged(World& w, int x, int y, int z, uint32_t flags);

// 排一个计划刻（原版 scheduleTick：延迟若干刻后再处理一次该方块）
void scheduleTick(World& w, int x, int y, int z, int delay);

// 推进一个游戏刻的计划刻（由 World::tickEntities 在整刻时调用）
void runScheduledTicks(World& w);

// ---- 查询 ----
// 这个位置的方块能不能留着（附着/支撑检查）；不受更新影响的方块恒为 true。
bool blockCanSurvive(const World& w, int x, int y, int z);
// 紫颂植株 / 紫颂花的存活判断（mctick 与方块更新共用，等价于原版 canSurvive）
bool chorusPlantCanSurvive(const World& w, int x, int y, int z);
bool chorusFlowerCanSurvive(const World& w, int x, int y, int z);
// 受重力影响的方块（原版 falling block 列表里本工程已有的那几个）
bool isFallingBlock(uint8_t id);

} // namespace bu
