#pragma once
#include "blocks.hpp"
#include "dimensions.hpp"
#include "util.hpp"

class World;

namespace portal {

// ---------------------------------------------------------------------------
// 传送门机制（对齐 Minecraft）。方块存储是 uint8 id、没有状态位，
// 末地框架的朝向被展开成 8 个 id（见 blocks.hpp），所以这里做如下归一化：
//  - 下界门：黑曜石矩形框（宽 2..21，高 3..21），用打火石点内部空位生成
//    nether_portal；走入门块即跨维度传送（1:8 缩放）。
//  - 末地门：12 个 end_portal_frame 围成 5×5 外圈（四角无框架、中心 3×3 空）。
//    每个框架的 facing 必须指向环心（原版 BlockPattern 要求），
//    末影之眼点框架使其变成有眼状态，12 个都朝向正确且有眼才激活中心 3×3。
// ---------------------------------------------------------------------------

// 在下界门内点火：识别到黑曜石框架则填满 nether_portal 并返回 true。
bool tryLightNetherPortal(World& w, int frameX, int frameY, int frameZ);

// 在末地框架上放末影之眼：该框架变为有眼状态，12 个朝向正确且都有眼则激活中心 3×3。
bool tryPlaceEyeOfEnder(World& w, int frameX, int frameY, int frameZ);

// 玩家放置末地传送门框架时该用的 id：朝向 = 玩家水平朝向的反方向（原版一致）。
uint8_t frameIdForPlacement(float yaw);

// ---------------------------------------------------------------------------
// 下界门目的地（对齐原版 PortalForcer）
// 调用前请先把 World 切到目标维度，并保证目标点附近区块已生成。
// ---------------------------------------------------------------------------
// 门方块的横向轴：0 = 沿 X 展开，1 = 沿 Z 展开；不是门方块返回 -1。
int inferPortalAxis(const World& w, int x, int y, int z);

// 目标点附近已有门的落点（门内 2 宽取中、底排）；找不到返回 false。
// radiusBlocks 是方形搜索半径（原版：去下界 16、回主世界 128）。
bool findExistingNetherPortal(World& w, int tx, int ty, int tz, int radiusBlocks, Vec3& outPos);

// 按原版 PortalForcer.createPortal 造一扇新门（螺旋找 4×4 空腔 + 黑曜石框 + 点燃），
// 成功返回门内落点。axis 是源门的横向轴（新门沿用）。
bool createNetherPortal(World& w, int tx, int ty, int tz, int axis, Vec3& outPos);

// 先找已有门、找不到就造一扇；返回落点。
bool findOrCreateNetherPortal(World& w, int tx, int ty, int tz, int axis, int radiusBlocks,
                              Vec3& outPos);

// ---------------------------------------------------------------------------
// 末地门 5×5 方环的几何规则（原版 EndPortalFrameBlock.getOrCreatePortalShape
// 的等价判定，见 portal.cpp 的说明）。
// 环上框架的 facing 必须指向环心，否则整环不算数。
// 供激活检测与自检复用，避免规则写两遍。
// ---------------------------------------------------------------------------
// 环上相对坐标 (dx,dz)（0..4）应使用的朝向；中心/四角返回 -1。
// 原版图案（要塞 StrongholdPieces 的实证布局）要求：北边朝南、南边朝北、
// 西边朝东、东边朝西 —— 也就是每个框架都指向环心。
inline int frameRequiredFacing(int dx, int dz) {
    bool edge = dx == 0 || dx == 4 || dz == 0 || dz == 4;
    bool isCorner = (dx == 0 || dx == 4) && (dz == 0 || dz == 4);
    if (!edge || isCorner) return -1;  // 中心 3×3 与四角都不是框架
    if (dx == 0) return FRAME_EAST;    // 西边 → 朝东
    if (dx == 4) return FRAME_WEST;    // 东边 → 朝西
    if (dz == 0) return FRAME_SOUTH;   // 北边 → 朝南
    return FRAME_NORTH;                // 南边 → 朝北
}

// 以 (x0,z0) 为左上角基准的 5×5 方环是否满足激活条件。
// get 返回 (x,y,z) 处的方块 id。
template <typename GetBlock>
bool isCompleteRingAt(GetBlock get, int x0, int y, int z0) {
    for (int dx = 0; dx < 5; dx++) {
        for (int dz = 0; dz < 5; dz++) {
            int need = frameRequiredFacing(dx, dz);
            if (need < 0) continue;
            uint8_t b = get(x0 + dx, y, z0 + dz);
            if (!blockIsPortalFrame(b) || !blockFrameHasEye(b)) return false;
            if (blockFrameFacing(b) != need) return false;
        }
    }
    return true;
}

// 该方块是否触发传送（nether_portal / end_portal）。
bool isPortalBlock(uint8_t id);

// 方块被挖掉后调用：挖到传送门方块、或破坏了黑曜石框架时，整扇下界传送门破碎（同原版）。
void onBlockRemoved(World& w, int x, int y, int z, uint8_t oldId);

// 清除与 (x,y,z) 连成一片的所有下界传送门方块。
void breakNetherPortal(World& w, int x, int y, int z);

} // namespace portal
