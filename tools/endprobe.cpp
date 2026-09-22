// endprobe.cpp — 末地生成调试工具：打印指定半径内的高度图/方块统计
// 用法: endprobe [seed] [半径区块数]
#include "blocks.hpp"
#include "endgen.hpp"
#include "specs.hpp"
#include <cstdio>
#include <cstdlib>
#include <vector>

int main(int argc, char** argv) {
    uint32_t seed = argc > 1 ? (uint32_t)strtoul(argv[1], nullptr, 10) : 12345u;
    int radius = argc > 2 ? atoi(argv[2]) : 6;

    std::vector<uint8_t> buf(CHUNK_VOL);
    int counts[B_COUNT] = {0};
    for (int cz = -radius; cz <= radius; cz++) {
        for (int cx = -radius; cx <= radius; cx++) {
            endgen::generateEnd(seed, cx, cz, buf.data());
            // 只统计地形（末地石），结构方块单独看
            for (size_t i = 0; i < buf.size(); i++) counts[buf[i]]++;
        }
    }
    printf("seed=%u radius=%d chunks=%d\n", seed, radius, (2 * radius + 1) * (2 * radius + 1));
    for (int i = 0; i < B_COUNT; i++)
        if (counts[i]) printf("  %-24s %d\n", BLOCK_DEFS[i].id, counts[i]);

    // 沿 +X 轴的末地石顶面高度剖面（忽略传送门/平台等结构方块）
    printf("\n沿 +X / +Z 轴末地石顶面高度:\n");
    printf("     x:");
    for (int x = 0; x <= radius * 16; x += 8) printf("%6d", x);
    printf("\n  z=0 :");
    for (int x = 0; x <= radius * 16; x += 8) {
        endgen::generateEnd(seed, blockToChunkCoord(x), 0, buf.data());
        int lx = blockToChunkLocal(x);
        int top = -1;
        for (int y = WORLD_HEIGHT - 1; y >= 0; y--)
            if (buf[lx + 0 + (y << 8)] == B_END_STONE) { top = y; break; }
        printf("%6d", top);
    }
    printf("\n  z=8 :");
    for (int x = 0; x <= radius * 16; x += 8) {
        endgen::generateEnd(seed, blockToChunkCoord(x), 0, buf.data());
        int lx = blockToChunkLocal(x);
        int top = -1;
        for (int y = WORLD_HEIGHT - 1; y >= 0; y--)
            if (buf[lx + (8 << 4) + (y << 8)] == B_END_STONE) { top = y; break; }
        printf("%6d", top);
    }
    printf("\n");
    printf("\n沿 +X 轴（每 8 格）:\n");
    for (int x = -8; x <= radius * 16; x += 8)
        printf("  x=%4d  islandH=%7.1f  base3D(y=20)=%9.5f base3D(y=64)=%9.5f base3D(y=126)=%9.5f\n",
               x, endgen::islandHeightValue(x, 0),
               endgen::debugBase3D(seed, x, 20, 0),
               endgen::debugBase3D(seed, x, 64, 0),
               endgen::debugBase3D(seed, x, 126, 0));
    fprintf(stderr, "\n原始密度对照 (0,0) 列 y=120..127:\n");
    endgen::debugDumpColumn(seed, 0, 0, 0, 0);
    return 0;
}
