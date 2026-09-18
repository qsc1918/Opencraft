#pragma once
#include <cstdint>
#include <string>
#include <vector>

class World;
struct Player;

struct WorldSave {
    std::string name;
    uint32_t seed = 0;
    float spawnX = 0, spawnY = 80, spawnZ = 0;
};

std::string savesDir();
std::vector<WorldSave> listSaves(const std::string& dir);

// 保存：二进制格式，写入所有已加载区块。
bool saveWorld(World& world, const Player& player, const std::string& name,
               const std::string& dir);

// 只读文件头（种子 + 出生点），供菜单列表使用。
WorldSave saveInfo(const std::string& name, const std::string& dir);

// 从磁盘加载完整世界（区块 + 种子 + 出生点 + 玩家视角/飞行状态）。
// I/O 出错或版本不匹配时返回 false。
bool loadWorld(World& world, uint32_t& seed, float& spawnX, float& spawnY, float& spawnZ,
               float& yaw, float& pitch, bool& flying,
               const std::string& name, const std::string& dir);
