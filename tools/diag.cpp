// build/diag.cpp - temporary diagnostic tool (not tracked)
#include "atlas.hpp"
#include "generator.hpp"
#include "png.hpp"
#include "blocks.hpp"
#include "specs.hpp"
#include "dimensions.hpp"
#include <chrono>
#include <cstdio>
#include <vector>

int main() {
    Atlas a = buildAtlas("assets/block");
    printf("atlas %dx%d tilesX=%d tilesY=%d cell=%d pad=%d\n",
           a.width, a.height, a.tilesX, a.tilesY, a.cellSize, a.tilePad);
    // 直接按 tile 采样内存里的图集（避开 PNG 落盘）
    for (int t = 0; t < 8; t++) {
        int tx = t % a.tilesX, ty = t / a.tilesX;
        int px = tx * a.cellSize + a.tilePad + 8;
        int py = ty * a.cellSize + a.tilePad + 8;
        size_t o = ((size_t)py * a.width + px) * 4;
        printf("tile %2d RGB=%3d,%3d,%3d a=%3d\n", t, a.rgba[o], a.rgba[o + 1], a.rgba[o + 2], a.rgba[o + 3]);
    }
    {   // 对照：单独加载 oak_log.png
        std::vector<uint8_t> img; int w = 0, h = 0;
        loadPNG("assets/block/oak_log.png", img, w, h);
        size_t o = ((size_t)(h / 2) * w + w / 2) * 4;
        printf("direct oak_log.png %dx%d center RGB=%d,%d,%d\n", w, h, img[o], img[o + 1], img[o + 2]);
    }
    {   // 打印 tile 7 的 4x4 抽样，看是不是整格都是黄的
        int tx = 7, ty = 0;
        for (int y = 0; y < 32; y += 8) {
            printf("t7 row%2d:", y);
            for (int x = 0; x < 32; x += 8) {
                size_t o = ((size_t)(ty * a.cellSize + y) * a.width + (tx * a.cellSize + x)) * 4;
                printf(" %3d,%3d,%3d", a.rgba[o], a.rgba[o + 1], a.rgba[o + 2]);
            }
            printf("\n");
        }
    }
    {   // 打印 tile 4（基岩）每行的前 16 个像素灰度
        int tx = 4, ty = 0;
        for (int y = 0; y < 16; y++) {
            printf("bedrock row%2d:", y);
            for (int x = 0; x < 16; x++) {
                size_t o = ((size_t)(ty * a.cellSize + a.tilePad + y) * a.width +
                            (tx * a.cellSize + a.tilePad + x)) * 4;
                printf(" %3d", a.rgba[o]);
            }
            printf("\n");
        }
    }
    {   // 对照：直接加载 bedrock.png 的 16x16 灰度
        std::vector<uint8_t> img; int w = 0, h = 0;
        loadPNG("assets/block/bedrock.png", img, w, h);
        for (int y = 0; y < 16 && y < h; y++) {
            printf("file  row%2d:", y);
            for (int x = 0; x < 16 && x < w; x++) printf(" %3d", img[((size_t)y * w + x) * 4]);
            printf("\n");
        }
    }

    std::vector<uint8_t> b(CHUNK_VOL);
    long long cnt[B_COUNT] = {0};
    static long long band[8][B_COUNT];
    for (int cx = -2; cx <= 2; cx++)
        for (int cz = -2; cz <= 2; cz++) {
            gen::generateForDim(DIM_NETHER, 1337, cx, cz, b.data());
            for (int y = 0; y < WORLD_HEIGHT; y++)
                for (int z = 0; z < 16; z++)
                    for (int x = 0; x < 16; x++) {
                        uint8_t v = b[x + (z << 4) + (y << 8)];
                        cnt[v]++;
                        band[y / 16][v]++;
                    }
        }
    printf("--- nether census (5x5 chunks) ---\n");
    long long total = 0;
    for (int i = 0; i < B_COUNT; i++) total += cnt[i];
    for (int i = 0; i < B_COUNT; i++)
        if (cnt[i]) printf("%3d %-14s %8lld  %5.1f%%\n", i, BLOCK_DEFS[i].name, cnt[i], 100.0 * cnt[i] / total);
    printf("--- bands: netherrack / air / lava ---\n");
    for (int yy = 0; yy < 8; yy++)
        printf("y=%3d..%3d  rack=%7lld air=%7lld lava=%7lld\n", yy * 16, yy * 16 + 15,
               band[yy][B_NETHERRACK], band[yy][B_AIR], band[yy][B_LAVA]);
    printf("--- column profiles at (0,0) and (8,8) ---\n");
    gen::generateForDim(DIM_NETHER, 1337, 0, 0, b.data());
    for (int pass = 0; pass < 2; pass++) {
        int lx = pass == 0 ? 0 : 8, lz = pass == 0 ? 0 : 8;
        printf("col (%d,%d): ", lx, lz);
        int y = 0;
        while (y < WORLD_HEIGHT) {
            uint8_t v = b[lx + (lz << 4) + (y << 8)];
            int y2 = y;
            while (y2 + 1 < WORLD_HEIGHT && b[lx + (lz << 4) + ((y2 + 1) << 8)] == v) y2++;
            printf("[%d-%d]%s ", y, y2, BLOCK_DEFS[v].name);
            y = y2 + 1;
        }
        printf("\n");
    }
    {
        auto t0 = std::chrono::steady_clock::now();
        for (int cx = -9; cx <= 9; cx++)
            for (int cz = -9; cz <= 9; cz++) gen::generateForDim(DIM_NETHER, 1337, cx, cz, b.data());
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::steady_clock::now() - t0).count();
        printf("19x19 nether chunks: %lld ms, %.2f ms/chunk\n", (long long)ms, ms / 361.0);
        t0 = std::chrono::steady_clock::now();
        for (int cx = -9; cx <= 9; cx++)
            for (int cz = -9; cz <= 9; cz++) gen::generateForDim(DIM_OVERWORLD, 1337, cx, cz, b.data());
        ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                 std::chrono::steady_clock::now() - t0).count();
        printf("19x19 overworld chunks: %lld ms, %.2f ms/chunk\n", (long long)ms, ms / 361.0);
    }
    return 0;
}
