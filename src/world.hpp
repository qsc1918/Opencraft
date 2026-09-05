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

// 区块/世界常量（CHUNK_SIZE、WORLD_HEIGHT、SEA_LEVEL、SECTION_*、光照/刻/
// 世界边界/坐标换算函数）已收编到 specs.hpp —— 它们是世界结构规范的一部分，
// 不要在本文件重新定义。见 docs/standards.md。
inline int chunkIndex(int x, int y, int z) { return x + (z << 4) + (y << 8); }

// 8-byte packed vertex for terrain/water meshes.
struct TerrainVertex {
    int8_t  x, y, z;
    uint8_t pad;
    uint8_t u, v;     // 0..15 within tile
    uint8_t tex;      // atlas tile index
    uint8_t shade;    // 0..255 baked brightness (face light * AO)
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
    std::atomic<int> state{0};        // 0 empty, 1 generated, 2 meshed
    std::atomic<bool> dirty{false};
    std::atomic<bool> needsUpload{false};
    std::mutex meshLock;
    ChunkMeshData mesh;

    uint64_t opaqueBuf = 0;   // VkBuffer handles stored as raw
    uint64_t opaqueMem = 0;
    uint64_t waterBuf = 0;
    uint64_t waterMem = 0;
    uint32_t opaqueCount = 0; // index count
    uint32_t waterCount = 0;
    uint64_t opaqueAlloc = 0; // bytes allocated
    uint64_t waterAlloc = 0;
    uint64_t opaqueVertBytes = 0; // vertex data byte size (index buffer offset)
    uint64_t waterVertBytes = 0;
    void*    opaqueMap = nullptr; // persistent vkMapMemory pointer (HOST_COHERENT)
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

// Stores a local 18x18x128 copy of a chunk plus its 4 neighbors for meshing.
// Indexed by LOCAL block coordinates x,z in [-1,16] and y in [0,WORLD_HEIGHT).
// The array is 18 wide per axis; index = (x+1) + (z+1)*18 + y*18*18 so the
// -1..16 local range maps cleanly onto 0..17 without out-of-bounds access.
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

    // Main-thread scheduling: ensure chunks around (px,pz) exist & are queued.
    void update(float px, float pz, int renderDist);

    // Block read (main thread, current dimension). Returns air for ungenerated.
    uint8_t getBlock(int x, int y, int z) const;
    // Player edit (current dimension). Returns true if changed.
    bool setBlock(int x, int y, int z, uint8_t id);

    std::shared_ptr<Chunk> chunkAt(int cx, int cz) const;

    // Iterate all chunks (main thread, current dimension).
    void forEachChunk(const std::function<void(std::shared_ptr<Chunk>&, int, int)>& fn);

    // Raw-pointer snapshot (current dimension).
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
