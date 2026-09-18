#pragma once
#include <cstdint>
#include <string>
#include <vector>

// PNG 解码为紧凑排列的 8 位 RGBA；失败返回 false。
bool loadPNG(const char* path, std::vector<uint8_t>& rgba, int& w, int& h);

// 把紧凑排列的 32bpp BGRA 像素编码为 PNG 文件。
bool savePNG(const std::string& path, int w, int h, const std::vector<uint8_t>& bgra);
