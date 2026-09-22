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

// 该方块是否触发传送（nether_portal / end_portal）。
bool isPortalBlock(uint8_t id);

// 方块被挖掉后调用：挖到传送门方块、或破坏了黑曜石框架时，整扇下界传送门破碎（同原版）。
void onBlockRemoved(World& w, int x, int y, int z, uint8_t oldId);

// 清除与 (x,y,z) 连成一片的所有下界传送门方块。
void breakNetherPortal(World& w, int x, int y, int z);

} // namespace portal
