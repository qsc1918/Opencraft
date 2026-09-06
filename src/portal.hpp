#pragma once
#include "blocks.hpp"
#include "dimensions.hpp"

class World;

namespace portal {

// ---------------------------------------------------------------------------
// 传送门机制（对齐 Minecraft；参考 26.2 源码的 PortalShape / EndPortalFrameBlock
// / NetherPortalBlock / EnderEyeItem）。为适配 VoxMine 的简化方块模型（uint8 id，
// 无朝向/状态），做了如下归一化：
//  - 下界传送门：黑曜石矩形框（宽 2..21，高 3..21，内空），用打火石点内部空位
//    生成 nether_portal 块。玩家走入 nether_portal 触发跨维度传送（1:8 缩放）。
//  - 末地传送门：12 个 end_portal_frame 围成 3×3 外圈（+中心 3×3 空间）。用末影
//    之眼点框架 -> 该框架变成 B_END_PORTAL_FRAME_EYE。12 个都变有眼 -> 中心 3×3
//    生成 end_portal。玩家走入 end_portal 触发到末地的传送。
// ---------------------------------------------------------------------------

// 在下界传送门内点火（玩家站在空位用打火石右键）。
// 若 (x,y,z) 附近能识别出黑曜石框架，填满 nether_portal 并返回 true。
bool tryLightNetherPortal(World& w, int frameX, int frameY, int frameZ);

// 在末地传送门框架上放末影之眼（玩家用末影之眼右键框架）。
// 把该框架变成 B_END_PORTAL_FRAME_EYE；若 12 个框架齐了，激活中心 3×3 并返回 true。
bool tryPlaceEyeOfEnder(World& w, int frameX, int frameY, int frameZ);

// 判定一个方块是否触发传送（nether_portal / end_portal）。
bool isPortalBlock(uint8_t id);

} // namespace portal
