// build/atlascheck.cpp - 检查有眼框架图块是否合成为"框架顶 + 中间眼"
#include "atlas.hpp"
#include "blocks.hpp"
#include <cstdio>

int main() {
    Atlas a = buildAtlas("assets/block");
    auto px = [&](int tile, int x, int y) -> const uint8_t* {
        int tx = tile % a.tilesX, ty = tile / a.tilesX;
        return &a.rgba[((size_t)(ty * a.cellSize + a.tilePad + y) * a.width +
                        (tx * a.cellSize + a.tilePad + x)) * 4];
    };
    const uint8_t* f00 = px(T_END_PORTAL_FRAME, 0, 0);
    const uint8_t* f88 = px(T_END_PORTAL_FRAME, 8, 8);
    const uint8_t* e00 = px(T_END_PORTAL_FRAME_EYE, 0, 0);
    const uint8_t* e88 = px(T_END_PORTAL_FRAME_EYE, 8, 8);
    const uint8_t* s00 = px(T_END_PORTAL_FRAME_SIDE, 0, 0);   // 第 0 行是透明区
    const uint8_t* s40 = px(T_END_PORTAL_FRAME_SIDE, 0, 4);   // 第 4 行应不透明
    printf("frame_top corner=%d,%d,%d  eye_tile corner=%d,%d,%d  (应相同)\n",
           f00[0], f00[1], f00[2], e00[0], e00[1], e00[2]);
    printf("frame_top center=%d,%d,%d  eye_tile center=%d,%d,%d  (应不同)\n",
           f88[0], f88[1], f88[2], e88[0], e88[1], e88[2]);
    printf("side row0 alpha=%d (应 0), row4 alpha=%d (应 255)\n", s00[3], s40[3]);
    bool ok = e00[0] == f00[0] && e00[1] == f00[1] && e00[2] == f00[2] &&
              (e88[0] != f88[0] || e88[1] != f88[1] || e88[2] != f88[2]) &&
              s00[3] == 0 && s40[3] == 255;
    printf(ok ? "OK\n" : "FAIL\n");
    return ok ? 0 : 1;
}
