#include "portal.hpp"
#include "world.hpp"
#include <cmath>

namespace portal {

bool isPortalBlock(uint8_t id) {
    return id == B_NETHER_PORTAL || id == B_END_PORTAL;
}

// ---------------------------------------------------------------------------
// 下界传送门：黑曜石矩形框识别（对齐 MC PortalShape，简化到无需朝向）
// 从空位 (x,y,z) 出发，在水平面内找一个 2..21 宽、3..21 高的黑曜石框。
// 返回框左下角/宽/高/轴；找不到返回 false。
// ---------------------------------------------------------------------------
struct Frame {
    int x0, y0, z0;     // 内腔左下角（确定内腔第一个方块的下界）
    int w, h;           // 内腔宽（沿轴）、高
    int axis;           // 0=X 向横向, 1=Z 向横向
};

static bool isObsidian(const World& w, int x, int y, int z) {
    return w.getBlock(x, y, z) == B_OBSIDIAN;
}

// 从 (x,y,z) 沿 dir（±1 水平方向）找右侧黑曜石边界。
// 调用者保证 y 是内腔最底行（内腔每个格的 below 都是黑曜石地板）。
// 返回内腔宽（不含两侧黑曜石）；失败返回 -1。
static int scanWidthUntilObsidian(const World& w, int x, int y, int z, int dx, int dz, int maxW) {
    for (int i = 1; i <= maxW; i++) {
        int nx = x + dx * i, nz = z + dz * i;
        uint8_t b = w.getBlock(nx, y, nz);
        if (isObsidian(w, nx, y, nz)) return i;          // 右边界
        if (b != B_AIR && b != B_WATER && b != B_NETHER_PORTAL) return -1;
        if (!isObsidian(w, nx, y - 1, nz)) return -1;    // 底部必须是黑曜石地板
    }
    return -1;
}

bool tryLightNetherPortal(World& w, int fx, int fy, int fz) {
    // (fx,fy,fz) 是玩家用打火石瞄准的位面（门洞内/或瞄准格）。MC 允许点门洞内任意空位。
    // 先向下找内腔最底行：below 是黑曜石（地板）的那一行。
    int baseY = fy;
    while (baseY > WORLD_MIN_Y) {
        uint8_t below = w.getBlock(fx, baseY - 1, fz);
        if (isObsidian(w, fx, baseY - 1, fz)) break;      // 地板
        uint8_t cur = w.getBlock(fx, baseY, fz);
        if (cur != B_AIR && cur != B_WATER && cur != B_NETHER_PORTAL) return false;
        baseY--;
    }
    int by = baseY;

    int bestScore = -1;
    Frame best{};
    for (int axis = 0; axis < 2; axis++) {
        int dx = axis == 0 ? 1 : 0;
        int dz = axis == 0 ? 0 : 1;

        // 在底行 (fx,by,fz) 上：向左找左黑曜石，向右找右黑曜石，确定内腔宽。
        int leftEdge = 0;
        for (; leftEdge <= 21; leftEdge++) {
            int nx = fx - dx * leftEdge, nz = fz - dz * leftEdge;
            uint8_t b = w.getBlock(nx, by, nz);
            if (isObsidian(w, nx, by, nz)) break;
            if (b != B_AIR && b != B_WATER && b != B_NETHER_PORTAL) { leftEdge = -1; break; }
            if (!isObsidian(w, nx, by - 1, nz)) { leftEdge = -1; break; }
        }
        if (leftEdge <= 0) continue;
        int startX = fx - dx * leftEdge + dx, startZ = fz - dz * leftEdge + dz;
        // startX/startZ 现在是内腔最左（贴着左黑曜石）的格。
        // 右扫确定宽度。
        int width = scanWidthUntilObsidian(w, startX, by, startZ, dx, dz, 21);
        if (width < 2 || width > 21) continue;
        // 需要左边界确实有黑曜石（startX 左侧）
        if (!isObsidian(w, startX - dx, by, startZ - dz)) continue;

        // 高度：从 by 向上，每层左右黑曜石 + 内部空，直到顶框。
        int height = 0;
        for (int hy = by; hy <= by + 21; hy++) {
            if (hy >= WORLD_HEIGHT) break;
            bool leftSide = isObsidian(w, startX - dx, hy, startZ - dz);
            bool rightSide = isObsidian(w, startX + dx * width, hy, startZ + dz * width);
            if (!leftSide || !rightSide) break;
            bool allEmpty = true;
            for (int i = 0; i < width; i++) {
                uint8_t b = w.getBlock(startX + dx * i, hy, startZ + dz * i);
                if (b != B_AIR && b != B_WATER && b != B_NETHER_PORTAL) { allEmpty = false; break; }
            }
            if (!allEmpty) break;
            height++;
            // 顶框检查：本行上方一整行黑曜石
            bool topFrame = true;
            for (int i = 0; i < width; i++)
                if (!isObsidian(w, startX + dx * i, hy + 1, startZ + dz * i)) { topFrame = false; break; }
            if (topFrame) break;
        }
        if (height < 3 || height > 21) continue;
        int score = width * height;
        if (score > bestScore) {
            bestScore = score;
            best = {startX, by, startZ, width, height, axis};
        }
    }
    if (bestScore < 0) return false;

    // 填充 nether_portal
    int dx = best.axis == 0 ? 1 : 0;
    int dz = best.axis == 0 ? 0 : 1;
    for (int i = 0; i < best.w; i++)
        for (int j = 0; j < best.h; j++) {
            int px = best.x0 + dx * i, pz = best.z0 + dz * i, py = best.y0 + j;
            if (w.getBlock(px, py, pz) != B_NETHER_PORTAL)
                w.setBlock(px, py, pz, B_NETHER_PORTAL);
        }
    return true;
}

// ---------------------------------------------------------------------------
// 末地传送门：12 个框架围成 5×5 方环（四角无框架），中心 3×3 为 portal。
// 检测以 (cx,cy,cz) 为框架之一的完整形状是否全是有眼框架。
// 找到则激活中心 3×3 为 end_portal。返回左上角 5×5 基准（用于生成 portal）。
// 布局（5×5，F=框架位置，. = 中心 portal 区）：
//   .FFF.
//   F...F
//   F...F
//   F...F
//   .FFF.
// ---------------------------------------------------------------------------
static bool isFrame(const World& w, int x, int y, int z) {
    uint8_t b = w.getBlock(x, y, z);
    return b == B_END_PORTAL_FRAME || b == B_END_PORTAL_FRAME_EYE;
}

static bool allFramesHaveEye(const World& w, int x0, int cy, int z0) {
    // 从 (x0,z0) 是 5×5 方环的左上角基准；y 统一用 cy。
    for (int dx = 0; dx < 5; dx++) {
        for (int dz = 0; dz < 5; dz++) {
            bool edge = dx == 0 || dx == 4 || dz == 0 || dz == 4;
            bool isCorner = (dx == 0 || dx == 4) && (dz == 0 || dz == 4);
            if (!edge) continue;        // 中心 3×3 不是框架
            if (isCorner) continue;     // 四角无框架
            if (w.getBlock(x0 + dx, cy, z0 + dz) != B_END_PORTAL_FRAME_EYE) return false;
        }
    }
    return true;
}

bool tryPlaceEyeOfEnder(World& w, int fx, int fy, int fz) {
    uint8_t b = w.getBlock(fx, fy, fz);
    if (b != B_END_PORTAL_FRAME || b == B_END_PORTAL_FRAME_EYE) return false;
    // 变有眼
    w.setBlock(fx, fy, fz, B_END_PORTAL_FRAME_EYE);

    // 检测：以 (fx,fz) 为框架，向四个方向试 5×5 方环的左上角基准。
    // 五个可能的基准偏移（该框架在环上的相对位置）。
    // 直观做法：检查该框架在 5×5 环外圈的哪个位置，反推基准。
    for (int relX = 0; relX < 5; relX++) {
        for (int relZ = 0; relZ < 5; relZ++) {
            bool edge = relX == 0 || relX == 4 || relZ == 0 || relZ == 4;
            bool isCorner = (relX == 0 || relX == 4) && (relZ == 0 || relZ == 4);
            if (!edge || isCorner) continue;
            int x0 = fx - relX, z0 = fz - relZ;
            if (allFramesHaveEye(w, x0, fy, z0)) {
                // 激活中心 3×3 portal
                for (int i = 1; i <= 3; i++)
                    for (int j = 1; j <= 3; j++) {
                        int px = x0 + i, pz = z0 + j;
                        if (w.getBlock(px, fy, pz) != B_END_PORTAL)
                            w.setBlock(px, fy, pz, B_END_PORTAL);
                    }
                return true;
            }
        }
    }
    return false;
}

} // namespace portal
