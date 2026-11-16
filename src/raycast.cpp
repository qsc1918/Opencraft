#include "raycast.hpp"
#include "world.hpp"
#include "blocks.hpp"
#include <algorithm>
#include <cmath>

namespace {

// 射线与格子内 AABB 的相交（slab 法）。返回进入距离，起点在盒内时返回 0；
// 不相交返回 -1。outAxis 是进入面对应的轴，供调用方定 face。
float rayBox(Vec3 orig, Vec3 dir, Vec3 bmin, Vec3 bmax, int& outAxis) {
    float tmin = -1.0e30f, tmax = 1.0e30f;
    int axis = -1;
    const float o[3] = {orig.x, orig.y, orig.z};
    const float d[3] = {dir.x, dir.y, dir.z};
    const float lo[3] = {bmin.x, bmin.y, bmin.z};
    const float hi[3] = {bmax.x, bmax.y, bmax.z};
    for (int a = 0; a < 3; a++) {
        if (std::abs(d[a]) < 1e-9f) {
            if (o[a] < lo[a] || o[a] > hi[a]) return -1.0f;
            continue;
        }
        float inv = 1.0f / d[a];
        float t1 = (lo[a] - o[a]) * inv;
        float t2 = (hi[a] - o[a]) * inv;
        float nearT = std::min(t1, t2), farT = std::max(t1, t2);
        if (nearT > tmin) { tmin = nearT; axis = a; }
        if (farT < tmax) tmax = farT;
        if (tmin > tmax) return -1.0f;
    }
    if (tmax < 0.0f) return -1.0f;
    outAxis = axis;
    return tmin > 0.0f ? tmin : 0.0f;
}

} // namespace

RayHit raycastWorld(const World& w, Vec3 origin, Vec3 dir, float maxDist) {
    RayHit out;
    float dx = dir.x, dy = dir.y, dz = dir.z;

    int x = (int)std::floor(origin.x);
    int y = (int)std::floor(origin.y);
    int z = (int)std::floor(origin.z);

    int stepX = dx > 0 ? 1 : -1;
    int stepY = dy > 0 ? 1 : -1;
    int stepZ = dz > 0 ? 1 : -1;

    float tDeltaX = std::abs(dx) > 1e-9f ? std::abs(1.0f / dx) : 1e30f;
    float tDeltaY = std::abs(dy) > 1e-9f ? std::abs(1.0f / dy) : 1e30f;
    float tDeltaZ = std::abs(dz) > 1e-9f ? std::abs(1.0f / dz) : 1e30f;

    float tMaxX = dx != 0 ? (dx > 0 ? (x + 1 - origin.x) / dx : (origin.x - x) / -dx) : 1e30f;
    float tMaxY = dy != 0 ? (dy > 0 ? (y + 1 - origin.y) / dy : (origin.y - y) / -dy) : 1e30f;
    float tMaxZ = dz != 0 ? (dz > 0 ? (z + 1 - origin.z) / dz : (origin.z - z) / -dz) : 1e30f;

    int lastX = x, lastY = y, lastZ = z;
    float t = 0;
    while (t <= maxDist) {
        uint8_t id = w.getBlock(x, y, z);
        // 流体不可选中：水下/岩浆里准星要穿过流体打到后面的实体方块。
        if (id != B_AIR && id != B_WATER && id != B_LAVA) {
            // 下界传送门是薄板，朝向由邻居推断（原版是方块状态 AXIS，本工程按邻域算）。
            int axis = 0;
            if (id == B_NETHER_PORTAL) {
                bool xSide = w.getBlock(x + 1, y, z) == B_NETHER_PORTAL ||
                             w.getBlock(x - 1, y, z) == B_NETHER_PORTAL;
                bool zSide = w.getBlock(x, y, z + 1) == B_NETHER_PORTAL ||
                             w.getBlock(x, y, z - 1) == B_NETHER_PORTAL;
                axis = (xSide == zSide) ? 0 : (xSide ? 0 : 1);
            }
            BlockBox16 s = blockShape16(id, axis);
            const float k = 1.0f / 16.0f;
            Vec3 bmin((float)x + s.x0 * k, (float)y + s.y0 * k, (float)z + s.z0 * k);
            Vec3 bmax((float)x + s.x1 * k, (float)y + s.y1 * k, (float)z + s.z1 * k);
            int hitAxis = -1;
            float tHit = rayBox(origin, dir, bmin, bmax, hitAxis);
            // 原版 BlockGetter.clip 逐格按 VoxelShape 求交：起点在形状内会 t=0 命中
            // （站在门板里准星恒命中门）；同格但不在板内、射线没穿过板就跳过，
            // 继续打到后面的方块。按整格判断会把这两种情况混为一谈。
            if (tHit >= 0.0f && tHit <= maxDist) {
                out.hit = true;
                out.x = x; out.y = y; out.z = z;
                out.px = lastX; out.py = lastY; out.pz = lastZ;
                if (hitAxis == 0) out.face = dx > 0 ? F_NX : F_PX;
                else if (hitAxis == 1) out.face = dy > 0 ? F_NY : F_PY;
                else if (hitAxis == 2) out.face = dz > 0 ? F_NZ : F_PZ;
                return out;
            }
        }
        lastX = x; lastY = y; lastZ = z;
        if (tMaxX < tMaxY && tMaxX < tMaxZ) {
            x += stepX;
            t = tMaxX;
            tMaxX += tDeltaX;
        } else if (tMaxY < tMaxZ) {
            y += stepY;
            t = tMaxY;
            tMaxY += tDeltaY;
        } else {
            z += stepZ;
            t = tMaxZ;
            tMaxZ += tDeltaZ;
        }
    }
    return out;
}
