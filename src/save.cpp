#include "save.hpp"
#include "world.hpp"
#include "player.hpp"
#include "platform.hpp"
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <set>
#include <cstdio>

namespace fs = std::filesystem;

// 存档里的区块数来自文件，损坏/篡改的值会直接变成一次巨大分配。
// 渲染距离上限 32 → 单维度最多约 (2*34+1)^2 个区块，三个维度也远小于这个数。
static constexpr uint32_t kMaxSaveChunks = 65536;

static std::string exeDir() {
    return platform::exeDir();
}

std::string savesDir() { return platform::join(platform::exeDir(), "saves"); }

static fs::path u8(const std::string& s) {
    static_assert(sizeof(char) == sizeof(char8_t), "char8_t must be same size as char");
    return fs::path(reinterpret_cast<const char8_t*>(s.c_str()));
}

static fs::path levelPath(const std::string& name, const std::string& dir) {
    return u8(dir) / u8(name) / "level.dat";
}

// 二进制格式
// v3 文件头: magic(4) version(4) seed(4) spawnX/Y/Z(12) yaw(4) pitch(4) flying(4)
//            playerDim(4)
// v3 之后每个维度一段: dimId(4) count(4)，每区块: cx(4) cz(4) blocks[CHUNK_VOL]
// v2/v1 只有一段（等价主世界），v1 无 yaw/pitch/flying。
static constexpr uint32_t SAVE_MAGIC = 0x564D5356; // "VMSV"
static constexpr uint32_t SAVE_VERSION = 3;

std::vector<WorldSave> listSaves(const std::string& dir) {
    std::vector<WorldSave> out;
    fs::path base = u8(dir);
    if (!fs::exists(base)) return out;
    for (const auto& e : fs::directory_iterator(base)) {
        if (!e.is_directory()) continue;
        std::string name = e.path().filename().string();
        WorldSave s = saveInfo(name, dir);
        s.name = name;
        out.push_back(s);
    }
    return out;
}

WorldSave saveInfo(const std::string& name, const std::string& dir) {
    WorldSave s;
    std::ifstream f(levelPath(name, dir), std::ios::binary);
    if (!f) return s;
    uint32_t magic = 0, version = 0;
    f.read((char*)&magic, 4);
    f.read((char*)&version, 4);
    if (magic != SAVE_MAGIC) return s;
    f.read((char*)&s.seed, 4);
    f.read((char*)&s.spawnX, 4);
    f.read((char*)&s.spawnY, 4);
    f.read((char*)&s.spawnZ, 4);
    return s;
}

bool saveWorld(World& world, const Player& player, const std::string& name,
               const std::string& dir) {
    try {
        // 把内存里尚未落盘的脏块同步到 chunks/，下面枚举时才能拿全且不会过期。
        world.flushChunkEdits();
        fs::path base = u8(dir);
        fs::create_directories(base / u8(name));
        std::ofstream f(levelPath(name, dir), std::ios::binary | std::ios::trunc);
        if (!f) return false;

        // 文件头
        uint32_t magic = SAVE_MAGIC;
        uint32_t version = SAVE_VERSION;
        uint32_t seed = world.seed;
        float sx = player.cam.pos.x, sy = player.cam.pos.y, sz = player.cam.pos.z;
        float yaw = player.cam.yaw, pitch = player.cam.pitch;
        uint32_t flying = player.flying ? 1 : 0;
        uint32_t playerDim = (uint32_t)player.dim;
        f.write((char*)&magic, 4);
        f.write((char*)&version, 4);
        f.write((char*)&seed, 4);
        f.write((char*)&sx, 4);
        f.write((char*)&sy, 4);
        f.write((char*)&sz, 4);
        f.write((char*)&yaw, 4);
        f.write((char*)&pitch, 4);
        f.write((char*)&flying, 4);
        f.write((char*)&playerDim, 4);

        // 每个维度各写一段，避免把下界/末地的区块当成主世界存下来。
        // 除了内存里的区块，还要把已卸载、但改动已落盘到 chunks/ 的区块补进来，
        // 否则玩家走远后退出，那些改动就只留在临时文件里、下次读档会丢。
        struct ChunkEntry { int32_t cx, cz; std::vector<uint8_t> data; };
        const std::string edits = world.editsDir();
        for (uint32_t d = 0; d < (uint32_t)DIM_COUNT; d++) {
            std::vector<ChunkEntry> entries;
            std::set<uint64_t> seen;
            world.forEachChunkInDim((DimensionId)d, [&](std::shared_ptr<Chunk>& c, int cx, int cz) {
                if (c->state.load() < 1) return;
                entries.push_back({cx, cz,
                    std::vector<uint8_t>(c->blocks.begin(), c->blocks.end())});
                seen.insert(((uint64_t)(uint32_t)cx << 32) | (uint32_t)cz);
            });
            if (!edits.empty() && fs::exists(u8(edits))) {
                for (const auto& e : fs::directory_iterator(u8(edits))) {
                    if (!e.is_regular_file()) continue;
                    std::string fn = e.path().filename().string();
                    unsigned fd = 0; int fcx = 0, fcz = 0;
                    if (std::sscanf(fn.c_str(), "d%u_%d_%d.bin", &fd, &fcx, &fcz) != 3) continue;
                    if (fd != d) continue;
                    uint64_t key = ((uint64_t)(uint32_t)fcx << 32) | (uint32_t)fcz;
                    if (seen.count(key)) continue;   // 内存里的版本更新，以它为准
                    std::ifstream cf(e.path(), std::ios::binary);
                    if (!cf) continue;
                    std::vector<uint8_t> buf(CHUNK_VOL);
                    cf.read((char*)buf.data(), CHUNK_VOL);
                    if (cf.gcount() != (std::streamsize)CHUNK_VOL) continue;
                    entries.push_back({fcx, fcz, std::move(buf)});
                    seen.insert(key);
                }
            }
            uint32_t count = (uint32_t)entries.size();
            f.write((char*)&d, 4);
            f.write((char*)&count, 4);
            for (auto& e : entries) {
                f.write((char*)&e.cx, 4);
                f.write((char*)&e.cz, 4);
                f.write((const char*)e.data.data(), CHUNK_VOL);
            }
        }
        return true;
    } catch (...) {
        return false;
    }
}

bool loadWorld(World& world, uint32_t& seed, float& spawnX, float& spawnY, float& spawnZ,
               float& yaw, float& pitch, bool& flying, DimensionId& playerDim,
               const std::string& name, const std::string& dir) {
    std::ifstream f(levelPath(name, dir), std::ios::binary);
    if (!f) return false;

    uint32_t magic = 0, version = 0;
    f.read((char*)&magic, 4);
    f.read((char*)&version, 4);
    if (magic != SAVE_MAGIC) return false;

    f.read((char*)&seed, 4);
    f.read((char*)&spawnX, 4);
    f.read((char*)&spawnY, 4);
    f.read((char*)&spawnZ, 4);

    // v2 起才写玩家视角/飞行状态；v1 存档没有这 12 字节。
    yaw = 0.0f;
    pitch = -0.1f;
    flying = false;
    playerDim = DIM_OVERWORLD;
    if (version >= 2) {
        f.read((char*)&yaw, 4);
        f.read((char*)&pitch, 4);
        uint32_t fly = 0;
        f.read((char*)&fly, 4);
        flying = fly != 0;
    }

    if (version >= 3) {
        // 分维度读取
        uint32_t pdim = 0;
        f.read((char*)&pdim, 4);
        playerDim = pdim < (uint32_t)DIM_COUNT ? (DimensionId)pdim : DIM_OVERWORLD;
        for (int i = 0; i < DIM_COUNT; i++) {
            uint32_t dimId = 0, count = 0;
            f.read((char*)&dimId, 4);
            f.read((char*)&count, 4);
            if (!f || dimId >= (uint32_t)DIM_COUNT) return false;
            // count 来自文件，损坏/篡改的存档会给出一个巨大值导致 OOM；
            // 按世界可容纳的区块数封顶（渲染距离上限 32 → 半径约 34 区块）。
            if (count > kMaxSaveChunks) return false;
            for (uint32_t k = 0; k < count; k++) {
                int32_t cx = 0, cz = 0;
                f.read((char*)&cx, 4);
                f.read((char*)&cz, 4);
                if (!f) return false;
                world.loadChunkFromDiskInDim((DimensionId)dimId, cx, cz, f);
                if (!f) return false;
            }
        }
        return true;
    }

    // v1/v2：只有一段，按主世界读取
    uint32_t numChunks = 0;
    f.read((char*)&numChunks, 4);
    if (!f || numChunks > kMaxSaveChunks) return false;
    for (uint32_t i = 0; i < numChunks; i++) {
        int32_t cx, cz;
        f.read((char*)&cx, 4);
        f.read((char*)&cz, 4);
        if (!f) return false;
        world.loadChunkFromDiskInDim(DIM_OVERWORLD, cx, cz, f);
        if (!f) return false;
    }
    return true;
}
