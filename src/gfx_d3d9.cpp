// D3D9 后端（实现中）：当前仅提供入口占位，显式选择时返回失败。
#include "vk.hpp"
#include <cstdio>

bool gfxD3D9Init(VkCtx&, Window&, int, int) {
    fprintf(stderr, "[d3d9] 后端尚未实现\n");
    return false;
}
void gfxD3D9Swap() {}
void gfxD3D9Shutdown(VkCtx&) {}
