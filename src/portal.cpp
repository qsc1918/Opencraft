#include "portal.hpp"
#include "world.hpp"
#include <algorithm>
#include <array>
#include <climits>
#include <cmath>
#include <cstdlib>
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
// 检查 (x0,cy,z0) 为基准的 5×5 方环是否满足激活条件
static bool allFramesHaveEye(const World& w, int x0, int cy, int z0) {
    return isCompleteRingAt([&](int x, int y, int z) { return w.getBlock(x, y, z); }, x0, cy, z0);
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

    // 方环可能跨 2×2 区块；先把 5×5 范围内涉及的区块生成出来，
    // 否则 setBlock 对未生成的区块直接失败，门只会填上一部分（原版没这个问题）。
    for (int rx = -4; rx <= 4; rx++)
        for (int rz = -4; rz <= 4; rz++)
            w.forceGenerateChunk(blockToChunkCoord(fx + rx), blockToChunkCoord(fz + rz));

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
                // 激活中心 3×3 的 portal（对应原版 EnderEyeItem 的 frontTopLeft + (-3,0,-3)）
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

// ---------------------------------------------------------------------------
// 下界门目的地：对齐原版 PortalForcer（26.2）
// 原版流程：按坐标比例算出近似出口 → 在附近找已有的门（POI 方形搜索），
// 找不到就 createPortal 造一扇：33×33 方形螺旋找一列"底下实心、上方 4×4 可替换"的
// 空腔，放黑曜石框 + 2×3 传送门方块；落点取门内横向中点、底排。
// 本实现没有 POI 索引，只在"已生成的区块"里找已有门（回程时原门通常还在内存里）。
// ---------------------------------------------------------------------------
namespace {

// 能不能被门/门框替换（原版 canBeReplaced() && fluidState.isEmpty()：水/岩浆不算空位）
inline bool portalReplaceable(uint8_t id) {
    return id == B_AIR || id == B_FIRE;
}

// 顶面高度：最上层"阻挡运动"的方块之上的第一格（原版 MOTION_BLOCKING；水/火不算阻挡）
inline int motionBlockingHeight(const World& w, int x, int z) {
    for (int y = WORLD_HEIGHT - 1; y >= 0; y--) {
        uint8_t b = w.getBlock(x, y, z);
        if (b != B_AIR && b != B_FIRE && b != B_WATER) return y + 1;
    }
    return 0;
}

// 门内落点：横向取中点（2 宽正好落在两格缝上，与原版一致）、纵向取底排。
inline Vec3 portalLandingPos(const World& w, int px, int py, int pz, int axis) {
    int wx = axis == 0 ? 1 : 0, wz = axis == 0 ? 0 : 1;
    int x0 = px, z0 = pz, y0 = py;
    while (w.getBlock(x0 - wx, py, z0 - wz) == B_NETHER_PORTAL) { x0 -= wx; z0 -= wz; }
    while (y0 - 1 >= WORLD_MIN_Y && w.getBlock(x0, y0 - 1, z0) == B_NETHER_PORTAL) y0--;
    int width = 1;
    while (width < 21 && w.getBlock(x0 + wx * width, py, z0 + wz * width) == B_NETHER_PORTAL) width++;
    return Vec3((float)x0 + (axis == 0 ? width * 0.5f : 0.5f), (float)y0,
                (float)z0 + (axis == 0 ? 0.5f : width * 0.5f));
}

} // 匿名命名空间

int inferPortalAxis(const World& w, int x, int y, int z) {
    if (w.getBlock(x, y, z) != B_NETHER_PORTAL) return -1;
    // 门宽方向那一侧一定有同门方块；另一侧是黑曜石框
    bool xSide = w.getBlock(x + 1, y, z) == B_NETHER_PORTAL ||
                 w.getBlock(x - 1, y, z) == B_NETHER_PORTAL;
    bool zSide = w.getBlock(x, y, z + 1) == B_NETHER_PORTAL ||
                 w.getBlock(x, y, z - 1) == B_NETHER_PORTAL;
    if (xSide == zSide) return 0;  // 1 格宽等歧义情况按 X 处理
    return xSide ? 0 : 1;
}

bool findExistingNetherPortal(World& w, int tx, int ty, int tz, int radiusBlocks, Vec3& outPos) {
    int bestD2 = INT_MAX, bx = 0, by = 0, bz = 0;
    // 只扫已生成的区块：跨维度回程时目的地的区块往往还没生成，
    // 这时就当没找到、直接造新门（原版有 POI 索引，本实现没有）。
    w.forEachChunk([&](std::shared_ptr<Chunk>& c, int cx, int cz) {
        int minX = cx * CHUNK_SIZE, minZ = cz * CHUNK_SIZE;
        if (minX > tx + radiusBlocks || minX + CHUNK_SIZE - 1 < tx - radiusBlocks) return;
        if (minZ > tz + radiusBlocks || minZ + CHUNK_SIZE - 1 < tz - radiusBlocks) return;
        if (c->state.load() < 1) return;
        const uint8_t* blocks = c->blocks.data();
        for (int y = 0; y < WORLD_HEIGHT; y++)
            for (int z = 0; z < CHUNK_SIZE; z++)
                for (int x = 0; x < CHUNK_SIZE; x++) {
                    if (blocks[chunkIndex(x, y, z)] != B_NETHER_PORTAL) continue;
                    int wx = minX + x, wz = minZ + z;
                    int dx = wx - tx, dy = y - ty, dz = wz - tz;
                    int d2 = dx * dx + dy * dy + dz * dz;
                    if (d2 < bestD2) { bestD2 = d2; bx = wx; by = y; bz = wz; }
                }
    });
    if (bestD2 == INT_MAX) return false;
    int axis = inferPortalAxis(w, bx, by, bz);
    outPos = portalLandingPos(w, bx, by, bz, axis < 0 ? 0 : axis);
    return true;
}

bool createNetherPortal(World& w, int tx, int ty, int tz, int axis, Vec3& outPos) {
    const int dirX = axis == 0 ? 1 : 0, dirZ = axis == 0 ? 0 : 1;   // 门宽方向
    const int cwX = -dirZ, cwZ = dirX;                              // 原版 getClockWise
    const int maxY = WORLD_HEIGHT - 1;

    auto canReplace = [&](int x, int y, int z) { return portalReplaceable(w.getBlock(x, y, z)); };
    auto solidAt = [&](int x, int y, int z) { return blockIsSolid(w.getBlock(x, y, z)); };
    // 原版 canHostFrame：4 宽 × 4 高（底下那排要实心、上面 4 排要可替换）
    auto canHostFrame = [&](int bx, int by, int bz, int offset) {
        for (int width = -1; width < 3; width++)
            for (int height = -1; height < 4; height++) {
                int x = bx + dirX * width + cwX * offset;
                int z = bz + dirZ * width + cwZ * offset;
                int y = by + height;
                if (y < WORLD_MIN_Y || y >= WORLD_HEIGHT) return false;
                if (height < 0) { if (!solidAt(x, y, z)) return false; }
                else if (!canReplace(x, y, z)) return false;
            }
        return true;
    };

    // 螺旋扫 33×33 列（原版 spiralAround(origin, 16)），优先"两侧都能放"的完整位置
    bool haveFull = false, havePart = false;
    int fx = 0, fy = 0, fz = 0, px = 0, py = 0, pz = 0;
    double fullD2 = 0, partD2 = 0;
    auto scanColumn = [&](int colX, int colZ) {
        int height = std::min(maxY, motionBlockingHeight(w, colX, colZ));
        for (int y = height; y >= WORLD_MIN_Y; y--) {
            if (!canReplace(colX, y, colZ)) continue;
            int firstEmptyY = y, bottom = y;
            while (bottom > WORLD_MIN_Y && canReplace(colX, bottom - 1, colZ)) bottom--;
            if (bottom + 4 > maxY) continue;
            int deltaY = firstEmptyY - bottom;   // 头顶到柱顶的距离
            if (!(deltaY <= 0 || deltaY >= 3)) continue;
            if (!canHostFrame(colX, bottom, colZ, 0)) continue;
            double d2 = (double)(colX - tx) * (colX - tx) + (double)(bottom - ty) * (bottom - ty) +
                        (double)(colZ - tz) * (colZ - tz);
            if (canHostFrame(colX, bottom, colZ, -1) && canHostFrame(colX, bottom, colZ, 1) &&
                (!haveFull || d2 < fullD2)) {
                haveFull = true; fullD2 = d2;
                fx = colX; fy = bottom; fz = colZ;
            }
            if (!haveFull && (!havePart || d2 < partD2)) {
                havePart = true; partD2 = d2;
                px = colX; py = bottom; pz = colZ;
            }
        }
    };
    const int kRadius = 16;
    for (int r = 0; r <= kRadius; r++) {
        if (r == 0) { scanColumn(tx, tz); continue; }
        for (int i = -r; i <= r; i++) { scanColumn(tx + i, tz - r); scanColumn(tx + i, tz + r); }
        for (int i = -r + 1; i <= r - 1; i++) { scanColumn(tx - r, tz + i); scanColumn(tx + r, tz + i); }
    }
    if (!haveFull && havePart) { haveFull = true; fx = px; fy = py; fz = pz; }

    if (!haveFull) {
        // 兜底：硬凿一个口袋（原版同款），保证一定能造出门
        int minStartY = std::max(WORLD_MIN_Y + 1, 70), maxStartY = maxY - 9;
        if (maxStartY < minStartY) return false;
        fx = tx - dirX; fz = tz - dirZ;
        fy = ty < minStartY ? minStartY : (ty > maxStartY ? maxStartY : ty);
        for (int box = -1; box < 2; box++)
            for (int width = 0; width < 2; width++)
                for (int height = -1; height < 3; height++) {
                    int x = fx + dirX * width + cwX * box, z = fz + dirZ * width + cwZ * box;
                    w.setBlock(x, fy + height, z, height < 0 ? B_OBSIDIAN : B_AIR);
                }
    }

    // 黑曜石框（4 宽 × 5 层里的边框）
    for (int width = -1; width < 3; width++)
        for (int height = -1; height < 4; height++) {
            if (width != -1 && width != 2 && height != -1 && height != 3) continue;
            w.setBlock(fx + dirX * width, fy + height, fz + dirZ * width, B_OBSIDIAN);
        }
    // 门内 2×3
    for (int width = 0; width < 2; width++)
        for (int height = 0; height < 3; height++)
            w.setBlock(fx + dirX * width, fy + height, fz + dirZ * width, B_NETHER_PORTAL);

    outPos = Vec3((float)fx + (axis == 0 ? 1.0f : 0.5f), (float)fy,
                  (float)fz + (axis == 0 ? 0.5f : 1.0f));
    return true;
}

bool findOrCreateNetherPortal(World& w, int tx, int ty, int tz, int axis, int radiusBlocks,
                              Vec3& outPos) {
    if (findExistingNetherPortal(w, tx, ty, tz, radiusBlocks, outPos)) return true;
    return createNetherPortal(w, tx, ty, tz, axis, outPos);
}

} // namespace portal
