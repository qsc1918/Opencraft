#include "world.hpp"
#include "generator.hpp"
#include "mesher.hpp"
#include <cmath>
#include <algorithm>

World::World(uint32_t seed_) : seed(seed_) {
    queue_ = std::priority_queue<WorldTask, std::vector<WorldTask>,
                                 std::function<bool(const WorldTask&, const WorldTask&)>>(
        [](const WorldTask& a, const WorldTask& b) { return worldTaskLess(a, b); });
}

World::~World() { stopWorkers(); }

void World::startWorkers(int n) {
    if (!workers_.empty()) return;
    running_.store(true);
    for (int i = 0; i < n; i++)
        workers_.emplace_back([this] { workerLoop(); });
}

void World::stopWorkers() {
    if (workers_.empty()) return;
    running_.store(false);
    queueCV_.notify_all();
    for (auto& t : workers_) t.join();
    workers_.clear();
}

// 坐标换算统一使用 specs.hpp 的 floorDiv / blockToChunkCoord / blockToChunkLocal
//（区块坐标 = floor(方块坐标/16)，负坐标向下取整 —— MC 规范语义）。

std::shared_ptr<Chunk> World::chunkAt(int cx, int cz) const {
    uint64_t key = chunkKey(cx, cz);
    auto& dim = dc();
    for (int i = 0; i < DimStorage::kCacheN; i++) {
        if (dim.cache[i].first == key && dim.cache[i].second) {
            return dim.cache[i].second;
        }
    }
    std::lock_guard<std::mutex> lk(mapLock_);
    auto it = dim.chunks.find(key);
    auto result = it == dim.chunks.end() ? nullptr : it->second;
    if (result) {
        dim.cache[dim.cacheIdx % DimStorage::kCacheN] = {key, result};
        dim.cacheIdx++;
    }
    return result;
}

std::shared_ptr<Chunk> World::chunkAtInDim(DimensionId dim, int cx, int cz) const {
    auto& ds = dims_[dim];
    uint64_t key = chunkKey(cx, cz);
    for (int i = 0; i < DimStorage::kCacheN; i++) {
        if (ds.cache[i].first == key && ds.cache[i].second) return ds.cache[i].second;
    }
    std::lock_guard<std::mutex> lk(mapLock_);
    auto it = ds.chunks.find(key);
    auto result = it == ds.chunks.end() ? nullptr : it->second;
    if (result) {
        ds.cache[ds.cacheIdx % DimStorage::kCacheN] = {key, result};
        ds.cacheIdx++;
    }
    return result;
}

uint8_t World::getBlockInDim(DimensionId dim, int x, int y, int z) const {
    if (y < WORLD_MIN_Y || y >= WORLD_HEIGHT) return B_AIR;
    int cx = blockToChunkCoord(x), cz = blockToChunkCoord(z);
    int lx = blockToChunkLocal(x), lz = blockToChunkLocal(z);
    auto c = chunkAtInDim(dim, cx, cz);
    if (!c || c->state.load() < 1) return B_AIR;
    return c->blocks[chunkIndex(lx, y, lz)];
}

bool World::setBlockInDim(DimensionId dim, int x, int y, int z, uint8_t id) {
    if (y < WORLD_MIN_Y || y >= WORLD_HEIGHT) return false;
    int cx = blockToChunkCoord(x), cz = blockToChunkCoord(z);
    auto c = chunkAtInDim(dim, cx, cz);
    if (!c || c->state.load() < 1) return false;
    int lx = blockToChunkLocal(x), lz = blockToChunkLocal(z);
    {
        std::unique_lock<std::shared_mutex> lk(blocksMutex_);
        if (c->blocks[chunkIndex(lx, y, lz)] == id) return false;
        c->blocks[chunkIndex(lx, y, lz)] = id;
    }
    auto& ds = dims_[dim];
    ds.queuedMesh.erase(chunkKey(cx, cz));
    ds.meshing.erase(chunkKey(cx, cz));
    float wx = (cx + 0.5f) * CHUNK_SIZE, wz = (cz + 0.5f) * CHUNK_SIZE;
    float dx = wx - ds.lastPx, dz = wz - ds.lastPz;
    uint64_t prio = (uint64_t)(dx * dx + dz * dz);
    std::lock_guard<std::mutex> lk(queueLock_);
    if (!ds.queuedMesh.count(chunkKey(cx, cz))) {
        ds.queuedMesh.insert(chunkKey(cx, cz));
        queue_.push(WorldTask{true, dim, cx, cz, prio, c});
        queueCV_.notify_one();
    }
    return true;
}

void World::forEachChunk(const std::function<void(std::shared_ptr<Chunk>&, int, int)>& fn) {
    std::lock_guard<std::mutex> lk(mapLock_);
    for (auto& kv : dc().chunks) {
        int cx = (int)(int32_t)(kv.first >> 32);
        int cz = (int)(int32_t)kv.first;
        fn(kv.second, cx, cz);
    }
}

void World::snapshotChunks(std::vector<ChunkInfo>& out) {
    std::lock_guard<std::mutex> lk(mapLock_);
    out.clear();
    out.reserve(dc().chunks.size());
    for (auto& kv : dc().chunks) {
        int cx = (int)(int32_t)(kv.first >> 32);
        int cz = (int)(int32_t)kv.first;
        out.push_back({kv.second.get(), cx, cz});
    }
}

uint8_t World::getBlock(int x, int y, int z) const {
    if (y < WORLD_MIN_Y || y >= WORLD_HEIGHT) return B_AIR;
    int cx = blockToChunkCoord(x), cz = blockToChunkCoord(z);
    int lx = blockToChunkLocal(x), lz = blockToChunkLocal(z);
    auto c = chunkAt(cx, cz);
    if (!c || c->state.load() < 1) return B_AIR;
    return c->blocks[chunkIndex(lx, y, lz)];
}

bool World::setBlock(int x, int y, int z, uint8_t id) {
    if (y < WORLD_MIN_Y || y >= WORLD_HEIGHT) return false;
    int cx = blockToChunkCoord(x), cz = blockToChunkCoord(z);
    auto c = chunkAt(cx, cz);
    if (!c || c->state.load() < 1) return false;
    int lx = blockToChunkLocal(x), lz = blockToChunkLocal(z);
    {
        std::unique_lock<std::shared_mutex> lk(blocksMutex_);
        if (c->blocks[chunkIndex(lx, y, lz)] == id) return false;
        c->blocks[chunkIndex(lx, y, lz)] = id;
    }
    editLog_.push_back({x, y, z, (int)id});
    // The owning chunk AND its edge neighbours must be re-meshed: a block edit on a
    // chunk boundary changes the neighbour's boundary face culling/AO, and failing to
    // rebuild it leaves stale faces that z-fight (flicker / see-through seams).
    markDirty(cx, cz);
    markDirty(cx + 1, cz);
    markDirty(cx - 1, cz);
    markDirty(cx, cz + 1);
    markDirty(cx, cz - 1);
    return true;
}

void World::markDirty(int cx, int cz) {
    scheduleMesh(currentDim_, cx, cz);
}

void World::loadChunkFromDisk(int cx, int cz, std::istream& f) {
    uint64_t key = chunkKey(cx, cz);
    auto& dim = dc();
    std::shared_ptr<Chunk> c;
    {
        std::lock_guard<std::mutex> lk(mapLock_);
        auto it = dim.chunks.find(key);
        if (it == dim.chunks.end()) { c = std::make_shared<Chunk>(); dim.chunks[key] = c; }
        else c = it->second;
    }
    f.read((char*)c->blocks.data(), CHUNK_VOL);
    c->state.store(1);
    c->dirty.store(true);
}

void World::forceMeshChunk(int cx, int cz) {
    uint64_t key = chunkKey(cx, cz);
    auto& dim = dc();
    std::shared_ptr<Chunk> c;
    {
        std::lock_guard<std::mutex> lk(mapLock_);
        auto it = dim.chunks.find(key);
        if (it == dim.chunks.end()) return;
        c = it->second;
    }
    if (c->state.load() < 1) return;
    c->dirty.store(true);
    {
        std::lock_guard<std::mutex> lk(queueLock_);
        dim.queuedMesh.erase(key);
        dim.meshing.erase(key);
    }
    float wx = (cx + 0.5f) * CHUNK_SIZE, wz = (cz + 0.5f) * CHUNK_SIZE;
    float dx = wx - dim.lastPx, dz = wz - dim.lastPz;
    uint64_t prio = (uint64_t)(dx * dx + dz * dz);
    std::lock_guard<std::mutex> lk(queueLock_);
    dim.queuedMesh.insert(key);
    queue_.push(WorldTask{true, currentDim_, cx, cz, prio, c});
    queueCV_.notify_one();
}

void World::forceGenerateChunk(int cx, int cz) {
    uint64_t key = chunkKey(cx, cz);
    auto& dim = dc();
    std::shared_ptr<Chunk> c;
    {
        std::lock_guard<std::mutex> lk(mapLock_);
        auto it = dim.chunks.find(key);
        if (it == dim.chunks.end()) { c = std::make_shared<Chunk>(); dim.chunks[key] = c; }
        else c = it->second;
    }
    if (c->state.load() >= 1) return;
    gen::generateForDim(currentDim_, seed, cx, cz, c->blocks.data());
    c->state.store(1);
    c->dirty.store(true);
    scheduleMesh(currentDim_, cx, cz);
}

void World::scheduleMesh(DimensionId dim, int cx, int cz) {
    auto& d = dims_[dim];
    std::shared_ptr<Chunk> c;
    {
        std::lock_guard<std::mutex> lk(mapLock_);
        auto it = d.chunks.find(chunkKey(cx, cz));
        if (it == d.chunks.end()) return;
        c = it->second;
    }
    if (c->state.load() < 1) return;
    c->dirty.store(true);
    uint64_t key = chunkKey(cx, cz);
    {
        std::lock_guard<std::mutex> lk(queueLock_);
        if (d.queuedMesh.count(key) || d.meshing.count(key)) return;
        d.queuedMesh.insert(key);
        float wx = (cx + 0.5f) * CHUNK_SIZE, wz = (cz + 0.5f) * CHUNK_SIZE;
        float dx = wx - d.lastPx, dz = wz - d.lastPz;
        uint64_t prio = (uint64_t)(dx * dx + dz * dz);
        queue_.push(WorldTask{true, dim, cx, cz, prio, c});
        queueCV_.notify_one();
    }
}

void World::enqueue(DimensionId dim, bool isMesh, int cx, int cz, uint64_t prio) {
    auto& d = dims_[dim];
    std::shared_ptr<Chunk> c;
    {
        std::lock_guard<std::mutex> lk(mapLock_);
        auto it = d.chunks.find(chunkKey(cx, cz));
        if (it == d.chunks.end()) return;
        c = it->second;
    }
    std::lock_guard<std::mutex> lk(queueLock_);
    if (isMesh) {
        if (d.queuedMesh.count(chunkKey(cx, cz)) || d.meshing.count(chunkKey(cx, cz))) return;
        d.queuedMesh.insert(chunkKey(cx, cz));
    } else {
        if (d.queuedGen.count(chunkKey(cx, cz)) || d.generating.count(chunkKey(cx, cz))) return;
        d.queuedGen.insert(chunkKey(cx, cz));
    }
    queue_.push(WorldTask{isMesh, dim, cx, cz, prio, c});
    queueCV_.notify_one();
}

bool World::popTask(WorldTask& out) {
    std::unique_lock<std::mutex> lk(queueLock_);
    while (true) {
        if (!running_.load() && queue_.empty()) return false;
        if (!queue_.empty()) break;
        queueCV_.wait(lk);
    }
    out = queue_.top();
    queue_.pop();
    uint64_t key = chunkKey(out.cx, out.cz);
    auto& d = dims_[out.dim];
    if (out.isMesh) {
        d.queuedMesh.erase(key);
        d.meshing.insert(key);
    } else {
        d.queuedGen.erase(key);
        d.generating.insert(key);
    }
    return true;
}

void World::generateChunk(DimensionId dim, int cx, int cz, const std::shared_ptr<Chunk>& c) {
    auto& d = dims_[dim];
    gen::generateForDim(dim, seed, cx, cz, c->blocks.data());
    c->state.store(1);
    scheduleMesh(dim, cx, cz);
    scheduleMesh(dim, cx + 1, cz);
    scheduleMesh(dim, cx - 1, cz);
    scheduleMesh(dim, cx, cz + 1);
    scheduleMesh(dim, cx, cz - 1);
    {
        std::lock_guard<std::mutex> lk(queueLock_);
        d.generating.erase(chunkKey(cx, cz));
    }
}

void World::meshChunk(DimensionId dim, int cx, int cz, const std::shared_ptr<Chunk>& c) {
    auto& d = dims_[dim];
    MeshView view;
    view.blocks.fill(B_AIR);

    // Collect all neighbor chunks under a single mapLock_ acquisition.
    std::shared_ptr<Chunk> neighbors[4]; // +x, -x, +z, -z
    std::shared_ptr<Chunk> corners[4];   // ++, -+, +-, --
    {
        std::lock_guard<std::mutex> mlk(mapLock_);
        auto get = [&](int ncx, int ncz) -> std::shared_ptr<Chunk> {
            auto it = d.chunks.find(chunkKey(ncx, ncz));
            return (it != d.chunks.end() && it->second->state.load() >= 1) ? it->second : nullptr;
        };
        neighbors[0] = get(cx + 1, cz);
        neighbors[1] = get(cx - 1, cz);
        neighbors[2] = get(cx, cz + 1);
        neighbors[3] = get(cx, cz - 1);
        corners[0] = get(cx + 1, cz + 1);
        corners[1] = get(cx - 1, cz + 1);
        corners[2] = get(cx + 1, cz - 1);
        corners[3] = get(cx - 1, cz - 1);
    }

    {
        std::shared_lock<std::shared_mutex> lk(blocksMutex_);

        // self
        for (int y = 0; y < WORLD_HEIGHT; y++)
            for (int z = 0; z < 16; z++)
                for (int x = 0; x < 16; x++)
                    view.set(x, y, z, c->blocks[chunkIndex(x, y, z)]);

        // +x neighbor
        if (neighbors[0]) {
            auto& nc = neighbors[0];
            for (int y = 0; y < WORLD_HEIGHT; y++)
                for (int z = 0; z < 16; z++)
                    view.set(16, y, z, nc->blocks[chunkIndex(0, y, z)]);
        }
        // -x neighbor
        if (neighbors[1]) {
            auto& nc = neighbors[1];
            for (int y = 0; y < WORLD_HEIGHT; y++)
                for (int z = 0; z < 16; z++)
                    view.set(-1, y, z, nc->blocks[chunkIndex(15, y, z)]);
        }
        // +z neighbor
        if (neighbors[2]) {
            auto& nc = neighbors[2];
            for (int y = 0; y < WORLD_HEIGHT; y++)
                for (int x = 0; x < 16; x++)
                    view.set(x, y, 16, nc->blocks[chunkIndex(x, y, 0)]);
        }
        // -z neighbor
        if (neighbors[3]) {
            auto& nc = neighbors[3];
            for (int y = 0; y < WORLD_HEIGHT; y++)
                for (int x = 0; x < 16; x++)
                    view.set(x, y, -1, nc->blocks[chunkIndex(x, y, 15)]);
        }
        // corners
        auto copyCorner = [&](const std::shared_ptr<Chunk>& nc, int vx, int vz) {
            if (!nc) return;
            int nx = vx == 16 ? 0 : 15;
            int nz = vz == 16 ? 0 : 15;
            for (int y = 0; y < WORLD_HEIGHT; y++)
                view.set(vx, y, vz, nc->blocks[chunkIndex(nx, y, nz)]);
        };
        copyCorner(corners[0], 16, 16);
        copyCorner(corners[1], -1, 16);
        copyCorner(corners[2], 16, -1);
        copyCorner(corners[3], -1, -1);
    }

    ChunkMeshData mesh = buildChunkMesh(view);
    {
        std::lock_guard<std::mutex> lk(c->meshLock);
        c->mesh = std::move(mesh);
        c->needsUpload.store(true);
        c->dirty.store(false);
        c->state.store(2);
    }
    {
        std::lock_guard<std::mutex> lk(queueLock_);
        d.meshing.erase(chunkKey(cx, cz));
    }
}

void World::workerLoop() {
    WorldTask t;
    while (popTask(t)) {
        if (t.isMesh)
            meshChunk(t.dim, t.cx, t.cz, t.chunk);
        else
            generateChunk(t.dim, t.cx, t.cz, t.chunk);
    }
}

// ---- 实体管理（主线程；见 docs/standards.md §实体）----

Entity* World::spawnEntity(std::unique_ptr<Entity> e) {
    if (!e) return nullptr;
    e->id = nextEntityId_++;
    Entity* raw = e.get();
    entities_.push_back(std::move(e));
    return raw;
}

void World::tickEntities(float dt) {
    for (auto& e : entities_) e->tick(*this, dt);
    entities_.erase(std::remove_if(entities_.begin(), entities_.end(),
                                   [](const std::unique_ptr<Entity>& e) { return e->dead; }),
                    entities_.end());
}

// 射线 vs 实体 AABB（slab 法）；实体碰撞箱: 宽 type->width，高 type->height，
// 底面在 pos.y（脚部中心语义）。
static bool rayAABB(Vec3 o, Vec3 d, Vec3 mn, Vec3 mx, float maxDist, float* outT) {
    float t0 = 0.0f, t1 = maxDist;
    float lo[3] = {mn.x, mn.y, mn.z}, hi[3] = {mx.x, mx.y, mx.z};
    float oo[3] = {o.x, o.y, o.z}, dd[3] = {d.x, d.y, d.z};
    for (int i = 0; i < 3; i++) {
        if (std::fabs(dd[i]) < 1e-9f) {
            if (oo[i] < lo[i] || oo[i] > hi[i]) return false;
            continue;
        }
        float inv = 1.0f / dd[i];
        float ta = (lo[i] - oo[i]) * inv, tb = (hi[i] - oo[i]) * inv;
        if (ta > tb) { float tmp = ta; ta = tb; tb = tmp; }
        t0 = ta > t0 ? ta : t0;
        t1 = tb < t1 ? tb : t1;
        if (t0 > t1) return false;
    }
    *outT = t0;
    return true;
}

Entity* World::raycastEntity(Vec3 origin, Vec3 dir, float maxDist, Vec3* hit) {
    Entity* best = nullptr;
    float bestT = maxDist;
    Vec3 d = length(dir) > 1e-9f ? normalize(dir) : Vec3(0, 0, 0);
    for (auto& e : entities_) {
        if (e->dead) continue;
        float hw = e->type ? e->type->width * 0.5f : 0.5f;
        float hh = e->type ? e->type->height : 1.0f;
        Vec3 mn(e->pos.x - hw, e->pos.y, e->pos.z - hw);
        Vec3 mx(e->pos.x + hw, e->pos.y + hh, e->pos.z + hw);
        float t = 0;
        if (rayAABB(origin, d, mn, mx, bestT, &t)) {
            bestT = t;
            best = e.get();
            if (hit) *hit = origin + d * t;
        }
    }
    return best;
}

void World::update(float px, float pz, int renderDist) {
    auto& dim = dc();
    dim.lastPx = px;
    dim.lastPz = pz;
    int ccx = blockToChunkCoord((int)std::floor(px));
    int ccz = blockToChunkCoord((int)std::floor(pz));

    // Skip the expensive ensure+queue loop when the player has not moved to a
    // new chunk since the last call and no chunks are stuck in state 0 (meaning
    // all previously queued generations have been picked up).
    bool posChanged = (ccx != dim.lastCCX || ccz != dim.lastCCZ || renderDist != dim.lastRenderDist);
    dim.lastCCX = ccx;
    dim.lastCCZ = ccz;
    dim.lastRenderDist = renderDist;

    // Ensure chunks exist + queue generation.
    // Skip the expensive scan when the player has not moved to a new chunk — all
    // previously created chunks were already queued and will be picked up by workers.
    if (posChanged) {
        // Step 1: collect chunks needing generation under a single mapLock_ hold
        // (previously acquired/released per chunk — 289 lock ops per frame).
        struct NeedGen { int cx, cz; uint64_t prio; };
        std::vector<NeedGen> toGen;
        {
            std::lock_guard<std::mutex> lk(mapLock_);
            for (int r = 0; r <= renderDist; r++) {
                int x0 = ccx - r, x1 = ccx + r;
                int z0 = ccz - r, z1 = ccz + r;
                for (int cx = x0; cx <= x1; cx++) {
                    for (int cz = z0; cz <= z1; cz++) {
                        if (r > 0 && cx > x0 && cx < x1 && cz > z0 && cz < z1) continue;
                        uint64_t key = chunkKey(cx, cz);
                        auto it = dim.chunks.find(key);
                        if (it == dim.chunks.end()) {
                            auto c = std::make_shared<Chunk>();
                            dim.chunks[key] = c;
                            float wx = (cx + 0.5f) * CHUNK_SIZE, wz = (cz + 0.5f) * CHUNK_SIZE;
                            float dx = wx - px, dz = wz - pz;
                            toGen.push_back({cx, cz, (uint64_t)(dx * dx + dz * dz)});
                        } else if (it->second->state.load() == 0) {
                            float wx = (cx + 0.5f) * CHUNK_SIZE, wz = (cz + 0.5f) * CHUNK_SIZE;
                            float dx = wx - px, dz = wz - pz;
                            toGen.push_back({cx, cz, (uint64_t)(dx * dx + dz * dz)});
                        }
                    }
                }
            }
        }
        // Step 2: enqueue generation (acquires queueLock_ separately, no nesting)
        for (auto& g : toGen) {
            uint64_t key = chunkKey(g.cx, g.cz);
            {
                std::lock_guard<std::mutex> qlk(queueLock_);
                if (dim.queuedGen.count(key) || dim.generating.count(key)) continue;
            }
            enqueue(currentDim_, false, g.cx, g.cz, g.prio);
        }
    }

    // unload far chunks — throttle to once every 15 frames to avoid iterating
    // the entire chunk map + acquiring queueLock per candidate every frame.
    if (++dim.unloadCounter >= 15) {
    dim.unloadCounter = 0;
    std::vector<uint64_t> toErase;
    {
        std::lock_guard<std::mutex> lk(mapLock_);
        for (auto& kv : dim.chunks) {
            int cx = (int)(int32_t)(kv.first >> 32);
            int cz = (int)(int32_t)kv.first;
            int dist = std::max(std::abs(cx - ccx), std::abs(cz - ccz));
            auto& c = kv.second;
            if (dist > renderDist + 2 && c->state.load() >= 2 && !c->dirty.load()) {
                uint64_t key = kv.first;
                {
                    std::lock_guard<std::mutex> qlk(queueLock_);
                    if (dim.generating.count(key) || dim.meshing.count(key) || dim.queuedMesh.count(key) || dim.queuedGen.count(key))
                        continue;
                }
                toErase.push_back(key);
            }
        }
        for (uint64_t key : toErase) {
            auto it = dim.chunks.find(key);
            if (it != dim.chunks.end()) {
                if (onDestroyChunk) onDestroyChunk(*it->second);
                dim.chunks.erase(it);
            }
        }
    }
    } // unload throttle
}
