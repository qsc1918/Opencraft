#pragma once
#include "blocks.hpp"
#include "dimensions.hpp"

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
// 末地门 5×5 方环的几何规则（原版 EndPortalFrameBlock.getOrCreatePortalShape）
// 环上框架的 facing 必须指向环心，否则整环不算数。
// 供激活检测与自检复用，避免规则写两遍。
// ---------------------------------------------------------------------------
// 环上相对坐标 (dx,dz)（0..4）应使用的朝向；中心/四角返回 -1
inline int frameRequiredFacing(int dx, int dz) {
    bool edge = dx == 0 || dx == 4 || dz == 0 || dz == 4;
    bool isCorner = (dx == 0 || dx == 4) && (dz == 0 || dz == 4);
    if (!edge || isCorner) return -1;  // 中心 3×3 与四角都不是框架
    int ox = 2 - dx, oz = 2 - dz;      // 指向环心的方向
    if (oz == 0) return ox > 0 ? FRAME_EAST : FRAME_WEST;
    return oz > 0 ? FRAME_SOUTH : FRAME_NORTH;
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
