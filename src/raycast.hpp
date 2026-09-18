#pragma once
#include "util.hpp"
#include "blocks.hpp"

class World;

struct RayHit {
    int x = 0, y = 0, z = 0; // 命中方块坐标
    int px = 0, py = 0, pz = 0; // 上一格（空）坐标
    int face = 0;             // 命中面编号
    bool hit = false;
};

// DDA 体素射线检测；dir 需归一化，maxDist 单位为方块。
RayHit raycastWorld(const World& w, Vec3 origin, Vec3 dir, float maxDist);
