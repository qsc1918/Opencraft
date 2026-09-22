#include "portal.hpp"
#include "world.hpp"
#include <array>
#include <cmath>
#include <vector>

namespace portal {

bool isPortalBlock(uint8_t id) {
    return id == B_NETHER_PORTAL || id == B_END_PORTAL;
}

// ---------------------------------------------------------------------------
// 下界传送门：黑曜石矩形框识别（对齐 MC PortalShape，简化为无需朝向）
// 从空位 (x,y,z) 出发，在水平面内找 2..21 宽、3..21 高的黑曜石框。
// 返回框左下角/宽/高/轴；找不到返回 false。
// ---------------------------------------------------------------------------
struct Frame {
    int x0, y0, z0;     // 内腔左下角（内腔第一个方块的下界）
    int w, h;           // 内腔宽（沿轴）、高
    int axis;           // 0=X 向横向, 1=Z 向横向
};

static bool isObsidian(const World& w, int x, int y, int z) {
    return w.getBlock(x, y, z) == B_OBSIDIAN;
}

// 从 (x,y,z) 沿 dir（±1 水平方向）找右侧黑曜石边界。
// 调用者保证 y 是内腔最底行（每格的 below 都是黑曜石地板）。
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
    // (fx,fy,fz) 是玩家用打火石瞄准的位面（门洞内或瞄准格）；MC 允许点门洞内任意空位。
    // 先向下找内腔最底行：below 为黑曜石（地板）的那一行。
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

        // 在底行 (fx,by,fz) 上向左找左黑曜石、向右找右黑曜石，确定内腔宽。
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
        // startX/startZ 是内腔最左（贴着左黑曜石）的格；右扫确定宽度。
        int width = scanWidthUntilObsidian(w, startX, by, startZ, dx, dz, 21);
        if (width < 2 || width > 21) continue;
        // 左边界（startX 左侧）必须确实是黑曜石
        if (!isObsidian(w, startX - dx, by, startZ - dz)) continue;

        // 高度：从 by 向上，逐层要求左右黑曜石 + 内部空，直到顶框。
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
            // 顶框检查：本行上方整行都是黑曜石
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

    // 先把门洞覆盖到的区块生成出来：跨区块的门若目标区块还没生成，
    // setBlock 会直接失败，导致只填上一部分格子。
    {
        int adx = best.axis == 0 ? 1 : 0;
        int adz = best.axis == 0 ? 0 : 1;
        int minX = best.x0, maxX = best.x0 + adx * (best.w - 1);
        int minZ = best.z0, maxZ = best.z0 + adz * (best.w - 1);
        for (int cx = blockToChunkCoord(minX); cx <= blockToChunkCoord(maxX); cx++)
            for (int cz = blockToChunkCoord(minZ); cz <= blockToChunkCoord(maxZ); cz++)
                w.forceGenerateChunk(cx, cz);
    }

    // 填充 nether_portal 方块
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
// 布局（5×5，F=框架位置，. = 中心 portal 区）：
//   .FFF.
//   F...F
//   F...F
//   F...F
//   .FFF.
// 每个框架还要朝向正确：原版 BlockPattern 要求上边朝南、下边朝北、左边朝东、
// 右边朝西（即都指向环心），否则不激活。
// ---------------------------------------------------------------------------
// 该框架位置应该朝向哪里：南=+Z、西=-X、北=-Z、东=+X
static int frameRequiresFacing(int dx, int dz) {
    if (dz == 0) return dx > 0 ? FRAME_EAST : FRAME_WEST;
    return dz > 0 ? FRAME_SOUTH : FRAME_NORTH;
}

// 检查 (x0,cy,z0) 为基准的 5×5 方环是否满足激活条件
static bool allFramesHaveEye(const World& w, int x0, int cy, int z0) {
    for (int dx = 0; dx < 5; dx++) {
        for (int dz = 0; dz < 5; dz++) {
            bool edge = dx == 0 || dx == 4 || dz == 0 || dz == 4;
            bool isCorner = (dx == 0 || dx == 4) && (dz == 0 || dz == 4);
            if (!edge || isCorner) continue;  // 中心 3×3 与四角都不是框架
            uint8_t b = w.getBlock(x0 + dx, cy, z0 + dz);
            if (!blockIsPortalFrame(b) || !blockFrameHasEye(b)) return false;
            if (blockFrameFacing(b) != frameRequiresFacing(2 - dx, 2 - dz)) return false;
        }
    }
    return true;
}

uint8_t frameIdForPlacement(float yaw) {
    // 原版 getStateForPlacement 用 context.getHorizontalDirection().getOpposite()：
    // 玩家朝哪边看，框架就背对他。
    // 本工程相机 forward = (sin yaw, *, cos yaw)：yaw=0 看 +Z（南）、π/2 看 +X（东）。
    float a = std::fmod(yaw, 6.2831853f);
    if (a < 0) a += 6.2831853f;
    int facing;
    if (a < 0.7853982f || a >= 5.4977871f)      facing = FRAME_NORTH; // 看南 → 朝北
    else if (a < 2.3561945f)                    facing = FRAME_WEST;  // 看东 → 朝西
    else if (a < 3.9269908f)                    facing = FRAME_SOUTH; // 看北 → 朝南
    else                                        facing = FRAME_EAST;  // 看西 → 朝东
    return frameId(facing, false);
}

bool tryPlaceEyeOfEnder(World& w, int fx, int fy, int fz) {
    uint8_t b = w.getBlock(fx, fy, fz);
    if (!blockIsPortalFrame(b) || blockFrameHasEye(b)) return false;
    // 变成有眼框架（保留原朝向）
    w.setBlock(fx, fy, fz, frameId(blockFrameFacing(b), true));

    // 检测：以 (fx,fz) 为环上某格，反推 5×5 方环的左上角基准
    for (int relX = 0; relX < 5; relX++) {
        for (int relZ = 0; relZ < 5; relZ++) {
            bool edge = relX == 0 || relX == 4 || relZ == 0 || relZ == 4;
            bool isCorner = (relX == 0 || relX == 4) && (relZ == 0 || relZ == 4);
            if (!edge || isCorner) continue;
            int x0 = fx - relX, z0 = fz - relZ;
            if (allFramesHaveEye(w, x0, fy, z0)) {
                // 激活中心 3×3 的 portal
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

// 洪水填充：清掉与起点相连的全部下界传送门方块（原版里门是整体破碎的）
void breakNetherPortal(World& w, int x, int y, int z) {
    static const int d[6][3] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
    if (w.getBlock(x, y, z) != B_NETHER_PORTAL) return;
    std::vector<std::array<int, 3>> stack;
    stack.push_back({x, y, z});
    w.setBlock(x, y, z, B_AIR);
    while (!stack.empty()) {
        std::array<int, 3> p = stack.back();
        stack.pop_back();
        for (const auto& dd : d) {
            int nx = p[0] + dd[0], ny = p[1] + dd[1], nz = p[2] + dd[2];
            if (w.getBlock(nx, ny, nz) == B_NETHER_PORTAL) {
                w.setBlock(nx, ny, nz, B_AIR);
                stack.push_back({nx, ny, nz});
            }
        }
    }
}

void onBlockRemoved(World& w, int x, int y, int z, uint8_t oldId) {
    static const int d[6][3] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
    if (oldId == B_NETHER_PORTAL) {
        // 起点可能已经被挖成空气了，所以连自己和六个邻格一起试着扩散
        if (w.getBlock(x, y, z) == B_NETHER_PORTAL) breakNetherPortal(w, x, y, z);
        for (const auto& dd : d) {
            int nx = x + dd[0], ny = y + dd[1], nz = z + dd[2];
            if (w.getBlock(nx, ny, nz) == B_NETHER_PORTAL) breakNetherPortal(w, nx, ny, nz);
        }
        return;
    }
    if (oldId != B_OBSIDIAN) return;
    // 破坏框架方块：贴着它的整扇门一起消失
    for (const auto& dd : d) {
        int nx = x + dd[0], ny = y + dd[1], nz = z + dd[2];
        if (w.getBlock(nx, ny, nz) == B_NETHER_PORTAL) {
            breakNetherPortal(w, nx, ny, nz);
            return;
        }
    }
}

} // namespace portal
