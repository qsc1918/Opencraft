#pragma once
#include "blocks.hpp"
#include "dimensions.hpp"
#include "entity.hpp"
#include "noise.hpp"
#include "specs.hpp"
#include "util.hpp"
#include <atomic>
#include <array>
#include <condition_variable>
#include <climits>
#include <cstdint>
#include <cstring>
#include <functional>
#include <istream>
#include <memory>
#include <mutex>
#include <queue>
#include <shared_mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// 世界常量与坐标换算函数都在 specs.hpp，属世界结构规范，别在本文件重定义。
// 见 docs/standards.md。
inline int chunkIndex(int x, int y, int z) { return x + (z << 4) + (y << 8); }

// 地形/水面网格共用的 8 字节紧凑顶点。
struct TerrainVertex {
    int8_t  x, y, z;
    uint8_t pad;
    uint8_t u, v;     // 图块内 0..15
    uint8_t tex;      // 图集图块下标
    uint8_t shade;    // 0..255 预烘焙亮度（面光 * AO）
};
static_assert(sizeof(TerrainVertex) == 8, "vertex must be 8 bytes");

inline bool operator==(const TerrainVertex& a, const TerrainVertex& b) {
    return std::memcmp(&a, &b, sizeof(TerrainVertex)) == 0;
}

struct ChunkMeshData {
    std::vector<TerrainVertex> opaqueVerts;
    std::vector<uint32_t>      opaqueIdx;
    std::vector<TerrainVertex> waterVerts;
    std::vector<uint32_t>      waterIdx;
};

struct Chunk {
    std::array<uint8_t, CHUNK_VOL> blocks{};
    std::atomic<int> state{0};        // 0=空 1=已生成 2=已构网格
    std::atomic<bool> dirty{false};
    std::atomic<bool> needsUpload{false};
    std::mutex meshLock;
    ChunkMeshData mesh;

    uint64_t opaqueBuf = 0;   // 存原始 VkBuffer 句柄，避免头文件依赖 Vulkan
    uint64_t opaqueMem = 0;
    uint64_t waterBuf = 0;
    uint64_t waterMem = 0;
    uint32_t opaqueCount = 0; // 索引数量
    uint32_t waterCount = 0;
    uint64_t opaqueAlloc = 0; // 已分配字节数
    uint64_t waterAlloc = 0;
    uint64_t opaqueVertBytes = 0; // 顶点数据字节数（索引缓冲区偏移）
    uint64_t waterVertBytes = 0;
    void*    opaqueMap = nullptr; // 常驻映射指针（HOST_COHERENT，无需手动刷新）
    void*    waterMap = nullptr;
};

inline uint64_t chunkKey(int cx, int cz) {
    return ((uint64_t)(uint32_t)cx << 32) | (uint32_t)cz;
}

struct WorldTask {
    bool isMesh = false;
    DimensionId dim = DIM_OVERWORLD;
    int32_t cx = 0, cz = 0;
    uint64_t prio = 0;
    std::shared_ptr<Chunk> chunk;
};

inline bool worldTaskLess(const WorldTask& a, const WorldTask& b) {
    if (a.prio != b.prio) return a.prio > b.prio;
    return a.cx != b.cx ? a.cx > b.cx : a.cz > b.cz;
}

// 构网格用的本地副本：本区块 + 四邻，18x18x128。
// 本地坐标 x,z ∈ [-1,16]、y ∈ [0,WORLD_HEIGHT)，索引用 (x+1)+(z+1)*18+y*18*18，
// 使 -1..16 恰好映射到 0..17，无需越界判断。
struct MeshView {
    std::array<uint8_t, 18 * 18 * WORLD_HEIGHT> blocks{};
    inline uint8_t at(int x, int y, int z) const {
        if (x < -1 || x > 16 || y < 0 || y >= WORLD_HEIGHT || z < -1 || z > 16) return B_AIR;
        return blocks[(x + 1) + (z + 1) * 18 + y * 18 * 18];
    }
    inline void set(int x, int y, int z, uint8_t v) {
        blocks[(x + 1) + (z + 1) * 18 + y * 18 * 18] = v;
    }
};

// 每维度存储：区块表、缓存、任务追踪集合（共享同一个 worker 池）
struct DimStorage {
    std::unordered_map<uint64_t, std::shared_ptr<Chunk>> chunks;
    static constexpr int kCacheN = 8;
    mutable std::array<std::pair<uint64_t, std::shared_ptr<Chunk>>, kCacheN> cache{};
    mutable uint32_t cacheIdx = 0;
    std::unordered_set<uint64_t> queuedGen, queuedMesh, generating, meshing;
    float lastPx = 0, lastPz = 0;
    int lastCCX = INT_MIN, lastCCZ = INT_MIN, lastRenderDist = -1;
    uint32_t unloadCounter = 0;
};

class World {
public:
    explicit World(uint32_t seed);
    ~World();

    void startWorkers(int n);
    void stopWorkers();

    // ---- 维度切换 ----
    void setDimension(DimensionId dim) { currentDim_ = dim; }
    DimensionId getDimension() const { return currentDim_; }

    // 主线程调度：确保 (px,pz) 周围区块已创建并入队。
    void update(float px, float pz, int renderDist);

    // 读方块（主线程，当前维度）；未生成的区块按空气处理。
    uint8_t getBlock(int x, int y, int z) const;
    // 玩家改方块（当前维度）；真正改变才返回 true。
    bool setBlock(int x, int y, int z, uint8_t id);

    // 跨维度读写方块（传送门用；不改 currentDim_）。
    uint8_t getBlockInDim(DimensionId dim, int x, int y, int z) const;
    bool setBlockInDim(DimensionId dim, int x, int y, int z, uint8_t id);

    std::shared_ptr<Chunk> chunkAt(int cx, int cz) const;

    // 遍历全部区块（主线程，当前维度）。
    void forEachChunk(const std::function<void(std::shared_ptr<Chunk>&, int, int)>& fn);

    // 裸指针快照（当前维度），免得渲染侧持有 shared_ptr。
    struct ChunkInfo { Chunk* c; int cx; int cz; };
    void snapshotChunks(std::vector<ChunkInfo>& out);

    void markDirty(int cx, int cz);

    const std::vector<std::array<int, 4>>& editLog() const { return editLog_; }
    void clearEditLog() { editLog_.clear(); }

    void forceGenerateChunk(int cx, int cz);
    void loadChunkFromDisk(int cx, int cz, std::istream& f);
    void forceMeshChunk(int cx, int cz);

    size_t chunkCount() const {
        std::lock_guard<std::mutex> lk(mapLock_);
        return dims_[currentDim_].chunks.size();
    }

    int workerCount() const { return (int)workers_.size(); }

    uint32_t seed;

    // ---- 实体管理（主线程模拟；渲染线程只读）----
    Entity* spawnEntity(std::unique_ptr<Entity> e);
    const std::vector<std::unique_ptr<Entity>>& entities() const { return entities_; }
    void tickEntities(float dt);
    Entity* raycastEntity(Vec3 origin, Vec3 dir, float maxDist, Vec3* hit = nullptr);

    std::function<void(Chunk&)> onDestroyChunk;

private:
    void workerLoop();
    void generateChunk(DimensionId dim, int cx, int cz, const std::shared_ptr<Chunk>& c);
    void meshChunk(DimensionId dim, int cx, int cz, const std::shared_ptr<Chunk>& c);
    void scheduleMesh(DimensionId dim, int cx, int cz);
    void enqueue(DimensionId dim, bool isMesh, int cx, int cz, uint64_t prio);
    bool popTask(WorldTask& out);

    // 按维度访问快捷方式
    DimStorage& dc() { return dims_[currentDim_]; }
    const DimStorage& dc() const { return dims_[currentDim_]; }

    // 指定维度查区块（跨维度传送门用）。
    std::shared_ptr<Chunk> chunkAtInDim(DimensionId dim, int cx, int cz) const;

    mutable std::mutex mapLock_;
    std::array<DimStorage, DIM_COUNT> dims_;
    DimensionId currentDim_ = DIM_OVERWORLD;
    mutable std::shared_mutex blocksMutex_;

    std::priority_queue<WorldTask, std::vector<WorldTask>, std::function<bool(const WorldTask&, const WorldTask&)>> queue_;
    std::mutex queueLock_;
    std::condition_variable queueCV_;

    std::vector<std::thread> workers_;
    std::atomic<bool> running_{false};

    std::vector<std::array<int, 4>> editLog_;

    std::vector<std::unique_ptr<Entity>> entities_;
    EntityId nextEntityId_ = 1;
};
