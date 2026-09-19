#pragma once
#include "blocks.hpp"
#include "dimensions.hpp"

class World;

namespace portal {

// ---------------------------------------------------------------------------
// 传送门机制（对齐 Minecraft）。方块模型已简化（uint8 id，无朝向/状态），
// 故做如下归一化：
//  - 下界门：黑曜石矩形框（宽 2..21，高 3..21），用打火石点内部空位生成
//    nether_portal；走入门块即跨维度传送（1:8 缩放）。
//  - 末地门：12 个 end_portal_frame 围成 3×3 外圈。末影之眼点框架使其变为
//    B_END_PORTAL_FRAME_EYE，12 个都有眼则中心 3×3 生成 end_portal。
// ---------------------------------------------------------------------------

// 在下界门内点火：识别到黑曜石框架则填满 nether_portal 并返回 true。
bool tryLightNetherPortal(World& w, int frameX, int frameY, int frameZ);

// 在末地框架上放末影之眼：该框架变为有眼状态，12 个齐了则激活中心 3×3。
bool tryPlaceEyeOfEnder(World& w, int frameX, int frameY, int frameZ);

// 该方块是否触发传送（nether_portal / end_portal）。
bool isPortalBlock(uint8_t id);

// 方块被挖掉后调用：挖到传送门方块、或破坏了黑曜石框架时，整扇下界传送门破碎（同原版）。
void onBlockRemoved(World& w, int x, int y, int z, uint8_t oldId);

// 清除与 (x,y,z) 连成一片的所有下界传送门方块。
void breakNetherPortal(World& w, int x, int y, int z);

} // namespace portal
