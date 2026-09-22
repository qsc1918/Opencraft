#include "camera.hpp"
#include "endgen.hpp"
#include "entities.hpp"
#include "mctick.hpp"
#include "player.hpp"
#include "portal.hpp"
#include "raycast.hpp"
#include "renderer.hpp"
#include "save.hpp"
#include "menu.hpp"
#include "vk.hpp"
#include "window.hpp"
#include "world.hpp"
#include "version.hpp"
#include <windows.h>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <thread>
#include <vector>
#include <memory>

struct Args {
    uint32_t seed = 1337;
    std::string screenshot;
    std::string posStr;
    std::string breakBlock;
    std::vector<std::string> placeBlocks; // --place x,y,z,id（调试，可多次）
    float yaw = 0.0f, pitch = -0.35f;
    float timeArg = -1.0f;
    int renderDist = 8;
    int threads = 0;
    int frames = 0;
    bool renderDistSet = false;
    bool noUI = false;
    bool drive = false;
    bool noVsync = false;
    bool invStart = false;
    int invPage = 0;
    int gpuIndex = -1;
    std::string crystalPos;
    std::string dimArg;
    std::string menuShot;
    int menuScreen = 1; // 默认主菜单（Menuscreen::MainMenu）
    bool spawnPortal = false; // 调试：在出生点生成已点燃的下界门
    bool showVersion = false; // --version：打印版本号后退出
};

static Args parseArgs(int argc, char** argv) {
    Args a;
    // 留一个核给主线程，否则生成/构网格会把渲染线程挤掉（进新维度时卡顿）
    a.threads = (int)std::thread::hardware_concurrency() - 1;
    if (a.threads < 1) a.threads = 1;
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        auto next = [&]() -> std::string { return i + 1 < argc ? argv[++i] : std::string(); };
        if (arg == "--seed") a.seed = (uint32_t)std::stoul(next());
        else if (arg == "--screenshot") a.screenshot = next();
        else if (arg == "--pos") a.posStr = next();
        else if (arg == "--yaw") a.yaw = std::stof(next());
        else if (arg == "--pitch") a.pitch = std::stof(next());
        else if (arg == "--render-dist") { a.renderDist = std::stoi(next()); a.renderDistSet = true; }
        else if (arg == "--threads") a.threads = std::stoi(next());
        else if (arg == "--frames") a.frames = std::stoi(next());
        else if (arg == "--no-ui") a.noUI = true;
        else if (arg == "--break") a.breakBlock = next();
        else if (arg == "--place") a.placeBlocks.push_back(next());
        else if (arg == "--time") a.timeArg = std::stof(next());
        else if (arg == "--drive") a.drive = true;
        else if (arg == "--no-vsync") a.noVsync = true;
        else if (arg == "--inventory") a.invStart = true;
        else if (arg == "--inv-page") a.invPage = std::stoi(next());
        else if (arg == "--crystal") a.crystalPos = next();
        else if (arg == "--dim") a.dimArg = next();
        else if (arg == "--gpu-index") a.gpuIndex = std::stoi(next());
        else if (arg == "--menu-shot") a.menuShot = next();
        else if (arg == "--menu-screen") a.menuScreen = std::stoi(next());
        else if (arg == "--spawn-portal") a.spawnPortal = true;
        else if (arg == "--version") a.showVersion = true;
        else if (arg == "--help") {
            printf("Usage: opencraft [--seed N] [--render-dist N] [--threads N] [--pos x,y,z]\n"
                   "  [--screenshot out.png] [--frames N] [--no-vsync] [--no-ui]\n"
                   "  [--time f] [--drive] [--break x,y,z] [--inventory] [--gpu-index N]\n"
                   "  [--version] 打印版本号后退出\n");
        }
    }
    return a;
}

static std::string exeDir() {
    char buf[MAX_PATH];
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    std::string p(buf);
    size_t pos = p.find_last_of("\\/");
    return pos == std::string::npos ? "." : p.substr(0, pos);
}

// 持久化设置：保存到 exe 同目录的 options.txt。
struct Options {
    bool vsync = false;       // 默认关闭垂直同步
    int renderDist = 8;       // 默认渲染距离（区块）
};

static std::string optionsPath() { return exeDir() + "\\options.txt"; }

static Options loadOptions() {
    Options o;
    std::ifstream f(optionsPath());
    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind("vsync:", 0) == 0) {
            o.vsync = (line.substr(6) == "on");
        } else if (line.rfind("renderDist:", 0) == 0) {
            int v = std::atoi(line.c_str() + 11);
            if (v >= 2 && v <= 32) o.renderDist = v;
        }
    }
    return o;
}

static void saveOptions(const Options& o) {
    std::ofstream f(optionsPath(), std::ios::trunc);
    if (!f) return;
    f << "vsync:" << (o.vsync ? "on" : "off") << "\n";
    f << "renderDist:" << o.renderDist << "\n";
}

int main(int argc, char** argv) {
    Args a = parseArgs(argc, argv);
    SetProcessDPIAware();

    if (a.showVersion) {
        printf("Opencraft %s\n", version::full().c_str());
        return 0;
    }

    // 先读设置再建 Vulkan 上下文：vsync 会影响交换链。
    Options opts = loadOptions();

    Window win;
    std::string title = "Opencraft " + version::full();
    if (!win.init(1280, 720, title.c_str())) { return 1; }

    VkCtx ctx;
    ctx.vsync = a.noVsync ? false : opts.vsync;
    ctx.desiredGpuIndex = a.gpuIndex;
    if (!ctx.init(win, 1280, 720)) { printf("Vulkan init failed: %s\n", ctx.lastError.c_str()); return 1; }

    Renderer renderer;
    Menu menu;
    if (!renderer.init(ctx, win, exeDir() + "\\assets", exeDir() + "\\shaders")) { printf("Renderer init failed\n"); return 1; }
    
    if (!menu.init(ctx, win, exeDir() + "\\assets")) { printf("Menu init failed\n"); return 1; }
    

    // ---- 游戏状态 ----
    enum class GS { MainMenu, SaveSelect, NewWorld, Options, Video, Pause, Play };
    const bool startDirect = !a.screenshot.empty() || a.drive || !a.breakBlock.empty()
                             || a.frames > 0 || !a.posStr.empty() || a.invStart;
    GS gs = startDirect ? GS::Play : GS::MainMenu;

    std::unique_ptr<World> world;
    Player player;
    std::string worldName;
    std::string seedText;
    std::vector<WorldSave> saves;
    Menuscreen optionsReturnTo = Menuscreen::MainMenu;
    int frame = 0;
    int lastW = 0, lastH = 0;
    int renderDist = (a.renderDistSet ? a.renderDist : opts.renderDist); // 视频设置里可运行时调整
    bool escPrev = false, ePrev = false, in_prevL = false, in_prevR = false;
    auto last = std::chrono::steady_clock::now();

    auto screenFromGS = [](GS g) -> Menuscreen {
        switch (g) {
            case GS::MainMenu: return Menuscreen::MainMenu;
            case GS::SaveSelect: return Menuscreen::SaveSelect;
            case GS::NewWorld: return Menuscreen::NewWorld;
            case GS::Options: return Menuscreen::Options;
            case GS::Video: return Menuscreen::VideoSettings;
            case GS::Pause: return Menuscreen::Pause;
            default: return Menuscreen::None;
        }
    };

    auto applyFirst = [&]() {
        if (a.timeArg >= 0.0f) renderer.setTimeOfDay(a.timeArg);
        if (a.invStart) renderer.setInventoryOpen(true);
        renderer.setInventoryPage(a.invPage);
    };
    applyFirst();

    // 进入世界。loadFrom 是存档名，空表示新建世界。
    auto enterWorld = [&](uint32_t seed, const std::string& name, const std::string& loadFrom) {
        world = std::make_unique<World>(seed);
        world->startWorkers(a.threads);
        renderer.setWorld(*world);
        worldName = name;
        player = Player();
        if (a.dimArg == "nether") { player.dim = DIM_NETHER; world->setDimension(DIM_NETHER); }
        else if (a.dimArg == "end") { player.dim = DIM_END; world->setDimension(DIM_END); }
        player.cam.yaw = a.yaw;
        player.cam.pitch = a.pitch;
        player.cam.markDirty();

        if (!loadFrom.empty()) {
            // 从磁盘载入已保存区块（二进制格式：各维度分别一段）。
            float savedSpawnX = 8.5f, savedSpawnY = 80.0f, savedSpawnZ = 8.5f;
            float savedYaw = 0.0f, savedPitch = -0.1f;
            bool savedFlying = false;
            DimensionId savedDim = DIM_OVERWORLD;
            loadWorld(*world, seed, savedSpawnX, savedSpawnY, savedSpawnZ,
                      savedYaw, savedPitch, savedFlying, savedDim, loadFrom, savesDir());
            // 网格不持久化（只存方块数据），载入后必须重建全部已载入区块，
            // 否则残留网格与邻块边界面会不一致。要逐维度重建。
            for (int d = 0; d < DIM_COUNT; d++) {
                world->setDimension((DimensionId)d);
                std::vector<std::pair<int,int>> loadedChunks;
                world->forEachChunk([&](std::shared_ptr<Chunk>& c, int cx, int cz) {
                    if (c->state.load() >= 1) loadedChunks.push_back({cx, cz});
                });
                for (auto& [cx, cz] : loadedChunks) {
                    world->forceMeshChunk(cx, cz);
                    world->forceMeshChunk(cx + 1, cz);
                    world->forceMeshChunk(cx - 1, cz);
                    world->forceMeshChunk(cx, cz + 1);
                    world->forceMeshChunk(cx, cz - 1);
                }
            }
            // 恢复存档里玩家所在的维度（否则会站在主世界看下界的区块）
            player.dim = savedDim;
            world->setDimension(savedDim);
            if (!a.posStr.empty()) {
                float x = 0, y = 80, z = 0;
                if (sscanf(a.posStr.c_str(), "%f,%f,%f", &x, &y, &z) >= 1) player.cam.pos = Vec3(x, y, z);
            } else {
                player.cam.pos = Vec3(savedSpawnX, savedSpawnY, savedSpawnZ);
            }
            // 恢复存档的视角与飞行状态，避免读档后朝向默认或从空中坠落。
            player.cam.yaw = savedYaw;
            player.cam.pitch = savedPitch;
            player.cam.markDirty();
            player.flying = savedFlying;
        } else {
            // 新世界：围绕出生点生成区块。
            if (!a.posStr.empty()) {
                float x = 0, y = 80, z = 0;
                if (sscanf(a.posStr.c_str(), "%f,%f,%f", &x, &y, &z) >= 1) player.cam.pos = Vec3(x, y, z);
            } else {
                auto hasHeadroom = [&](int x, int z, int surfaceY, int needed) {
                    for (int y = surfaceY + 1; y <= surfaceY + needed; y++) {
                        if (y >= WORLD_HEIGHT) return false;
                        uint8_t b = world->getBlock(x, y, z);
                        if (b != B_AIR && b != B_WATER) return false;
                    }
                    return true;
                };
                // 区块坐标换算统一走 specs.hpp（floor 语义，负坐标正确）。
                // 出生锚点：区块 (0,0) 中心方块，与 MC 出生搜索一致。
                const int anchor = CHUNK_SIZE / 2;
                int spawnX = anchor, spawnZ = anchor, spawnTopY = 0;
                bool found = false;
                auto columnTop = [&](int x, int z, bool groundOnly, int& yOut) {
                    for (int y = WORLD_HEIGHT - 1; y >= 0; y--) {
                        uint8_t b = world->getBlock(x, y, z);
                        if (b == B_AIR || b == B_WATER) continue;
                        if (groundOnly && (b == B_LEAVES || b == B_LOG)) continue;
                        yOut = y;
                        return b;
                    }
                    return (uint8_t)0;
                };
                for (int radius = 0; radius <= 128 && !found; radius++) {
                    for (int dx = -radius; dx <= radius && !found; dx++) {
                        for (int dz = -radius; dz <= radius && !found; dz++) {
                            if (radius > 0 && std::abs(dx) != radius && std::abs(dz) != radius) continue;
                            int x = anchor + dx, z = anchor + dz;
                            world->forceGenerateChunk(blockToChunkCoord(x), blockToChunkCoord(z));
                            int sY = -1;
                            uint8_t topBlock = columnTop(x, z, true, sY);
                            if (sY < 0) continue;
                            if (topBlock == B_WATER) continue;
                            if (sY < SEA_LEVEL - 1) continue;
                            int checkFrom = sY;
                            for (int y = sY + 1; y < WORLD_HEIGHT; y++) {
                                uint8_t b = world->getBlock(x, y, z);
                                if (b == B_AIR) { checkFrom = y; break; }
                                if (b != B_WATER && b != B_LEAVES && b != B_LOG) { checkFrom = y; break; }
                            }
                            if (!hasHeadroom(x, z, checkFrom, 3)) continue;
                            spawnX = x; spawnZ = z; spawnTopY = sY;
                            found = true;
                        }
                    }
                }
                if (!found) {
                    int bestFloor = -1, bestX = anchor, bestZ = anchor;
                    for (int radius = 0; radius <= 12; radius++) {
                        for (int dx = -radius; dx <= radius; dx++) {
                            for (int dz = -radius; dz <= radius; dz++) {
                                if (radius > 0 && std::abs(dx) != radius && std::abs(dz) != radius) continue;
                                int x = anchor + dx, z = anchor + dz;
                                world->forceGenerateChunk(blockToChunkCoord(x), blockToChunkCoord(z));
                                int sY = -1;
                                columnTop(x, z, false, sY);
                                if (sY < 0) continue;
                                if (sY > bestFloor) { bestFloor = sY; bestX = x; bestZ = z; }
                            }
                        }
                        if (bestFloor > SEA_LEVEL - 12) break;
                    }
                    spawnX = bestX; spawnZ = bestZ; spawnTopY = bestFloor > 0 ? bestFloor : 0;
                    found = true;
                    if (spawnTopY <= 0) { spawnX = anchor; spawnZ = anchor; }
                }
                if (spawnTopY <= 0) player.cam.pos = Vec3(anchor + 0.5f, 80.0f, anchor + 0.5f);
                else player.cam.pos = Vec3((float)spawnX + 0.5f, (float)spawnTopY + 1.0f + player.eyeHeight,
                                           (float)spawnZ + 0.5f);
            }
        }
        // 调试/演示：在玩家前方 3 格生成并点燃一个竖直下界门（两种出生路径都可用）
        if (a.spawnPortal) {
            int pxi = (int)std::floor(player.cam.pos.x);
            int pzi = (int)std::floor(player.cam.pos.z) + 3;
            int baseY = (int)std::floor(player.cam.pos.y - player.eyeHeight);
            if (baseY < 4) baseY = 4;
            // 门洞对准玩家 x：覆盖 pxi..pxi+1，往前走即可触发
            int fx0 = pxi - 1;
            for (int x = fx0; x <= fx0 + 3; x++) {
                world->forceGenerateChunk(blockToChunkCoord(x), blockToChunkCoord(pzi));
                world->setBlock(x, baseY - 1, pzi, B_OBSIDIAN);
                world->setBlock(x, baseY + 3, pzi, B_OBSIDIAN);
            }
            for (int y = baseY - 1; y <= baseY + 3; y++) {
                world->setBlock(fx0, y, pzi, B_OBSIDIAN);
                world->setBlock(fx0 + 3, y, pzi, B_OBSIDIAN);
            }
            // 清空门洞（玩家实际建造时也是先挖空再点火）
            for (int x = fx0 + 1; x <= fx0 + 2; x++)
                for (int y = baseY; y <= baseY + 2; y++)
                    world->setBlock(x, y, pzi, B_AIR);
            // 用打火石那套逻辑点燃（顺便实测点燃逻辑）
            bool lit = portal::tryLightNetherPortal(*world, fx0 + 1, baseY, pzi);
            fprintf(stderr, "[main] demo nether portal at (%d,%d) lit=%d\n", pxi, pzi, (int)lit);
        }
        if (!a.invStart) renderer.setInventoryOpen(false);
        win.setCapture(true);
        ShowCursor(FALSE);
        gs = GS::Play;
    };

    // 等玩家周围区块就绪，保证开局流畅
    auto settle = [&]() {
        auto t0 = std::chrono::steady_clock::now();
        while (true) {
            renderer.gpuSync(ctx);
            world->update(player.cam.pos.x, player.cam.pos.z, renderDist);
            int ready = 0;
            world->forEachChunk([&](std::shared_ptr<Chunk>& c, int, int) { if (c->state.load() >= 2) ready++; });
            if (ready >= (2 * renderDist + 1) * (2 * renderDist + 1)) break;
            if (std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - t0).count() > 20000) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    };

    // 维度传送：切 player.dim 与 world 维度，再设玩家位置。
    // 下界门：主世界与下界互传，坐标按下界缩放系数 8.0 换算。
    // 末地门：主世界与末地互传（回程回主世界）。
    float portalCooldown = 0.0f; // 防止出入口来回反复传送
    auto performTeleport = [&](DimensionId toDim) {
        DimensionId fromDim = player.dim;
        float scale = getDimensionType(toDim).coordinateScale;
        float inv = 1.0f / scale;
        float px = player.cam.pos.x, py = player.cam.pos.y, pz = player.cam.pos.z;
        if (toDim == DIM_NETHER) { px *= inv; pz *= inv; }
        else if (toDim == DIM_OVERWORLD && fromDim == DIM_NETHER) { px *= scale; pz *= scale; }
        // 原版进末地一律落在中央的 obsidian 平台上（return portal 留着以后做）
        if (toDim == DIM_END) {
            px = endgen::END_SPAWN_X;
            pz = endgen::END_SPAWN_Z;
            py = endgen::END_SPAWN_Y + 1.0f;
        }
        // Y 按两边的世界高度映射，并把下界落点限制在洞穴带内：
        // 直接沿用原 Y 会把玩家送到贴着基岩天花板的位置（整片天花板怼脸）。
        {
            const DimensionType& toT = getDimensionType(toDim);
            const DimensionType& fromT = getDimensionType(fromDim);
            float ratio = (py - fromT.minY) / (float)fromT.height;
            if (ratio < 0.0f) ratio = 0.0f;
            if (ratio > 1.0f) ratio = 1.0f;
            py = toT.minY + ratio * toT.height;
            if (toDim == DIM_NETHER) {
                if (py < 32.0f) py = 32.0f;
                if (py > 96.0f) py = 96.0f;
            }
        }
        player.dim = toDim;
        world->setDimension(toDim);
        // 落点安全化：新维度地形可能是实心的，先把目标区块生成出来，
        // 再向上找到有 2 格净空的位置，避免玩家直接卡在方块里。
        world->forceGenerateChunk(blockToChunkCoord((int)std::floor(px)),
                                  blockToChunkCoord((int)std::floor(pz)));
        {
            int bx = (int)std::floor(px), bz = (int)std::floor(pz);
            auto open = [&](uint8_t b) {
                return b == B_AIR || b == B_WATER || b == B_LAVA || b == B_NETHER_PORTAL;
            };
            // 可站立：脚与头两格净空，且下方有实心块
            auto standable = [&](int y) {
                if (y < 2 || y >= WORLD_HEIGHT - 1) return false;
                if (!open(world->getBlock(bx, y, bz)) || !open(world->getBlock(bx, y + 1, bz)))
                    return false;
                uint8_t below = world->getBlock(bx, y - 1, bz);
                return blockIsSolid(below) || below == B_NETHER_PORTAL;
            };
            // 从目标高度向上下找最近的可站立空位（先往下，避免贴在天花板上）
            int sy = (int)std::floor(py);
            if (sy < 2) sy = 2;
            if (sy > WORLD_HEIGHT - 2) sy = WORLD_HEIGHT - 2;
            int found = -1;
            for (int d = 0; d < WORLD_HEIGHT; d++) {
                int down = sy - d, up = sy + d;
                if (down >= 2 && standable(down)) { found = down; break; }
                if (up < WORLD_HEIGHT - 1 && standable(up)) { found = up; break; }
            }
            if (found > 0) py = (float)found;
        }
        player.cam.pos = Vec3(px, py, pz);
        player.cam.markDirty();
        player.vel = Vec3(0, 0, 0);
        portalCooldown = 1.5f;
    };

    // 每帧检测：脚下方块是传送门就切维度。主循环与截图分支共用。
    auto checkTeleport = [&]() {
        if (portalCooldown > 0.0f) { portalCooldown -= 1.0f / 60.0f; return; }
        float feetY = player.cam.pos.y - player.eyeHeight;
        bool foundPortal = false;
        for (int sy = 0; sy < 2 && !foundPortal; sy++) {
            int bx = (int)std::floor(player.cam.pos.x);
            int by = (int)std::floor(feetY + sy);
            int bz = (int)std::floor(player.cam.pos.z);
            uint8_t pb = world->getBlock(bx, by, bz);
            if (portal::isPortalBlock(pb)) {
                DimensionId curDim = world->getDimension();
                DimensionId toDim = curDim;
                if (pb == B_NETHER_PORTAL) {
                    // 下界门：主世界与下界双向。
                    toDim = (curDim == DIM_NETHER) ? DIM_OVERWORLD : DIM_NETHER;
                } else if (pb == B_END_PORTAL) {
                    // 末地门：主世界与末地双向。
                    toDim = (curDim == DIM_END) ? DIM_OVERWORLD : DIM_END;
                }
                if (toDim != curDim) {
                    performTeleport(toDim);
                }
                foundPortal = true;
            }
        }
    };

    if (gs == GS::Play) {
        enterWorld(a.seed, "world", std::string());
        
        settle();
        
        if (!a.breakBlock.empty()) {
            int bx = 0, by = 0, bz = 0;
            sscanf(a.breakBlock.c_str(), "%d,%d,%d", &bx, &by, &bz);
            fprintf(stderr, "[main] break (%d,%d,%d)\n", bx, by, bz);
            uint8_t old = world->getBlock(bx, by, bz);
            portal::onBlockRemoved(*world, bx, by, bz, old);
            world->setBlock(bx, by, bz, B_AIR);
        }
        // 调试：放置方块，格式 x,y,z,id（可多次）
        for (const std::string& p : a.placeBlocks) {
            int px = 0, py = 0, pz = 0, pid = 0;
            if (sscanf(p.c_str(), "%d,%d,%d,%d", &px, &py, &pz, &pid) == 4) {
                world->forceGenerateChunk(blockToChunkCoord(px), blockToChunkCoord(pz));
                world->setBlock(px, py, pz, (uint8_t)pid);
                fprintf(stderr, "[main] place (%d,%d,%d)=%d\n", px, py, pz, pid);
            }
        }
        if (!a.crystalPos.empty()) {
            float x = 12.5f, y = 72.0f, z = 8.5f;
            if (sscanf(a.crystalPos.c_str(), "%f,%f,%f", &x, &y, &z) >= 1) {
                auto crystal = std::make_unique<EndCrystal>();
                crystal->pos = Vec3(x, y, z);
                world->spawnEntity(std::move(crystal));
                fprintf(stderr, "[main] spawned EndCrystal at (%.1f, %.1f, %.1f)\n", x, y, z);
            }
        }
        if (!a.screenshot.empty()) {
            int totalFrames = a.drive ? 400 : 30;
            for (int i = 0; i < totalFrames && win.pump(); i++) {
                if (i == (a.drive ? 380 : 20)) renderer.requestScreenshot(a.screenshot);
                Input& in = win.input();
                if (a.drive) { in.keys['W'] = true; player.cam.pitch = -0.1f; player.cam.markDirty(); }
                player.update(in, *world, 1.0f / 60.0f);
                world->tickEntities(1.0f / 60.0f);
                world->update(player.cam.pos.x, player.cam.pos.z, renderDist);
                checkTeleport();
                renderer.render(ctx, player.cam, player, in, 1.0f / 60.0f, (float)renderDist, !a.noUI);
                win.endFrame();
            }
            {
                int ready = 0;
                world->forEachChunk([&](std::shared_ptr<Chunk>& c, int, int) { if (c->state.load() >= 2) ready++; });
                fprintf(stderr, "[main] screenshot: %d chunks meshed\n", ready);
            }
            menu.shutdown(ctx);
            renderer.shutdown(ctx);
            world->stopWorkers();
            ctx.shutdown();
            win.shutdown();
            return 0;
        }
    }

    // 种子输入解析
    auto seedFromText = [&]() -> uint32_t {
        if (seedText.empty()) return (uint32_t)std::chrono::steady_clock::now().time_since_epoch().count();
        try {
            size_t pos;
            unsigned long long v = std::stoull(seedText, &pos);
            if (pos == seedText.size()) return (uint32_t)v;
        } catch (...) {}
        // 非纯数字：哈希成种子
        uint32_t h = 2166136261u;
        for (char ch : seedText) { h ^= (uint8_t)ch; h *= 16777619u; }
        return h;
    };

    // 生成不重名的世界名
    int saveSeq = 0;
    auto newWorldName = [&]() -> std::string {
        std::string n = "世界" + std::to_string(++saveSeq);
        for (auto& s : saves) if (s.name == n) n = "世界" + std::to_string(++saveSeq);
        return n;
    };

    if (!a.menuShot.empty()) {
        Menuscreen ms = (Menuscreen)a.menuScreen;
        MenuData md;
        md.saves = listSaves(savesDir());
        md.vsync = ctx.vsync;
        md.renderDist = renderDist;
        md.seedText = "123456";
        menu.renderMenu(ctx, ms, md, 640.0f, 300.0f, false);
        menu.debugSaveMenu(a.menuShot);
        menu.shutdown(ctx);
        renderer.shutdown(ctx);
        ctx.shutdown();
        win.shutdown();
        return 0;
    }

    while (true) {
        if (!win.pump()) break;
        RECT rc;
        GetClientRect((HWND)win.hwnd(), &rc);
        int cw = rc.right - rc.left, ch = rc.bottom - rc.top;
        if (cw <= 0 || ch <= 0) { std::this_thread::sleep_for(std::chrono::milliseconds(16)); continue; }
        if (cw != lastW || ch != lastH) { ctx.recreateSwapchain(cw, ch); menu.onResize(ctx, cw, ch); lastW = cw; lastH = ch; }

        Input& in = win.input();
        auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now - last).count();
        last = now;
        if (dt > 0.1f) dt = 0.1f;

        bool esc = in.keys[VK_ESCAPE], escEdge = esc && !escPrev;
        bool eKey = in.keys['E'], eEdge = eKey && !ePrev;
        escPrev = esc;
        ePrev = eKey;

        if (gs == GS::Play) {
            if (escEdge) {
                if (renderer.inventoryOpen()) { renderer.setInventoryOpen(false); win.setCapture(true); ShowCursor(FALSE); }
                else { gs = GS::Pause; win.setCapture(false); ShowCursor(TRUE); }
            }
            if (eEdge) {
                bool o = !renderer.inventoryOpen();
                renderer.setInventoryOpen(o);
                if (o) { win.setCapture(false); ShowCursor(TRUE); }
                else { win.setCapture(true); ShowCursor(FALSE); }
            }
            if (a.drive) { in.keys['W'] = true; player.cam.pitch = -0.1f; player.cam.markDirty(); }

            if (renderer.inventoryOpen()) {
                float ccx, ccy; win.cursorPos(ccx, ccy); renderer.setCursor(ccx, ccy);
            } else {
                float mx, my;
                win.pollMouse(mx, my);
                player.cam.addYaw(-mx * 0.003f);
                player.cam.addPitch(-my * 0.003f);
                
                player.update(in, *world, dt);
                world->tickEntities(dt);
                // 方块随机刻（紫颂花长成植株等）
                mctick::tickRandomBlocks(*world, player.cam.pos.x, player.cam.pos.z, dt);

                // ---- 传送检测：脚下方块是传送门就切维度 ----
                checkTeleport();

                RayHit hit = raycastWorld(*world, player.cam.pos, player.cam.forward(), 6.0f);
                if (in.mouse[0] && !in_prevL) {
                    // 先测实体（龙/水晶），再测方块
                    Vec3 entityHit;
                    Entity* entHit = world->raycastEntity(player.cam.pos, player.cam.forward(), 6.0f, &entityHit);
                    if (entHit) {
                        entHit->hurt(10.0f); // 攻击水晶造成伤害
                    } else if (hit.hit) {
                        uint8_t t = world->getBlock(hit.x, hit.y, hit.z);
                        // 水与岩浆不可挖（原版流体不可破坏）
                        if (t != B_AIR && t != B_WATER && t != B_LAVA) {
                            // 先处理传送门破碎，再真正挖掉（否则门方块已被空气替换）
                            portal::onBlockRemoved(*world, hit.x, hit.y, hit.z, t);
                            world->setBlock(hit.x, hit.y, hit.z, B_AIR);
                        }
                    }
                }
                if (in.mouse[1] && !in_prevR) {
                    uint16_t held = renderer.heldMiscItem();
                    if (held == I_FLINT_AND_STEEL) {
                        // 打火石：先试着点燃下界门，点不着就在点击面的空位放火。
                        int tx = hit.hit ? hit.px : (int)std::floor(player.cam.pos.x);
                        int ty = hit.hit ? hit.py : ((int)std::floor(player.cam.pos.y) - 1);
                        int tz = hit.hit ? hit.pz : (int)std::floor(player.cam.pos.z);
                        if (!portal::tryLightNetherPortal(*world, tx, ty, tz) && hit.hit) {
                            if (world->getBlock(tx, ty, tz) == B_AIR)
                                world->setBlock(tx, ty, tz, B_FIRE);
                        }
                    } else if (held == I_EYE_OF_ENDER && hit.hit) {
                        // 末影之眼：用在末地传送门框架上（空框架才吃眼）。
                        uint8_t bt = world->getBlock(hit.x, hit.y, hit.z);
                        if (blockIsPortalFrame(bt))
                            portal::tryPlaceEyeOfEnder(*world, hit.x, hit.y, hit.z);
                    } else if (hit.hit) {
                        int px = hit.px, py = hit.py, pz = hit.pz;
                        uint8_t b = renderer.selectedBlock();
                        // 末地传送门框架：按玩家水平朝向取反决定 facing（原版一致）
                        if (blockIsPortalFrame(b))
                            b = portal::frameIdForPlacement(player.cam.yaw);
                        float feet = player.cam.pos.y - player.eyeHeight;
                        bool inside = !(px + 1 <= player.cam.pos.x - player.halfWidth ||
                                        px >= player.cam.pos.x + player.halfWidth ||
                                        py + 1 <= feet || py >= feet + player.height ||
                                        pz + 1 <= player.cam.pos.z - player.halfWidth ||
                                        pz >= player.cam.pos.z + player.halfWidth);
                        if (!inside) world->setBlock(px, py, pz, b);
                    }
                }
                in_prevL = in.mouse[0];
                in_prevR = in.mouse[1];
            }

            renderer.gpuSync(ctx);
            world->update(player.cam.pos.x, player.cam.pos.z, renderDist);
            renderer.render(ctx, player.cam, player, in, dt, (float)renderDist, !a.noUI);
        } else {
            // ---- 菜单界面 ----
            if (escEdge) {
                switch (gs) {
                    case GS::SaveSelect: gs = GS::MainMenu; break;
                    case GS::NewWorld: gs = GS::SaveSelect; break;
                    case GS::Options: gs = optionsReturnTo == Menuscreen::Pause ? GS::Pause : GS::MainMenu; break;
                    case GS::Video: gs = GS::Options; break;
                    case GS::Pause: win.setCapture(true); ShowCursor(FALSE); gs = GS::Play; break;
                    case GS::MainMenu: default: break;
                }
            }

            // 种子输入：边沿触发，一次按键输入一个字符
            if (gs == GS::NewWorld) {
                for (UINT k : {0x30u,0x31u,0x32u,0x33u,0x34u,0x35u,0x36u,0x37u,0x38u,0x39u,
                               (UINT)'A',(UINT)'B',(UINT)'C',(UINT)'D',(UINT)'E',(UINT)'F'})
                    if (in.pressed[k]) seedText += (char)k;
                if (in.pressed[VK_SPACE]) seedText += ' ';
                if (in.pressed[VK_OEM_MINUS]) seedText += '-';
                if (in.pressed[VK_BACK] && !seedText.empty()) seedText.pop_back();
            }

            float cx, cy; win.cursorPos(cx, cy);
            MenuData md;
            md.saves = saves;
            md.seedText = seedText;
            md.vsync = ctx.vsync;
            md.renderDist = renderDist;
            md.renderDistPtr = &renderDist;
            md.titleText = "新建世界";
            int clicked = menu.renderMenu(ctx, screenFromGS(gs), md, cx, cy, in.mouse[0]);

            switch (clicked) {
                case MENU_SINGLEPLAYER: saves = listSaves(savesDir()); gs = GS::SaveSelect; break;
                case MENU_QUIT: goto shutdown_all;
                case MENU_OPTIONS:
                    optionsReturnTo = (gs == GS::Pause) ? Menuscreen::Pause : Menuscreen::MainMenu;
                    gs = GS::Options; break;
                case MENU_NEWWORLD: seedText.clear(); gs = GS::NewWorld; break;
                case MENU_SAVEANDTITLE:
                    saveWorld(*world, player, worldName, savesDir());
                    world->stopWorkers();
                    world.reset();
                    gs = GS::MainMenu; win.setCapture(false); ShowCursor(TRUE);
                    break;
                case MENU_CREATE: {
                    uint32_t seed = seedFromText();
                    std::string name = newWorldName();
                    enterWorld(seed, name, std::string());
                    settle();
                    saveWorld(*world, player, name, savesDir()); // 写入存档条目
                    break;
                }
                case MENU_CANCEL: gs = GS::SaveSelect; break;
                case MENU_VIDEO: gs = GS::Video; break;
                case MENU_BACK:
                    if (gs == GS::SaveSelect) gs = GS::MainMenu;
                    else if (gs == GS::Options) gs = optionsReturnTo == Menuscreen::Pause ? GS::Pause : GS::MainMenu;
                    else if (gs == GS::Video) gs = GS::Options;
                    break;
                case MENU_VSYNC:
                    ctx.vsync = !ctx.vsync;
                    ctx.recreateSwapchain(cw, ch);
                    saveOptions(Options{ctx.vsync, renderDist});
                    break;
                case MENU_RENDERDIST:
                    // 滑块已把新值写入 renderDist，这里只需保存。
                    saveOptions(Options{ctx.vsync, renderDist});
                    break;
                default:
                    // 存档列表按钮
                    if (clicked >= MENU_SAVE_FIRST && clicked < MENU_SAVE_FIRST + (int)saves.size() && gs == GS::SaveSelect) {
                        int i = clicked - MENU_SAVE_FIRST;
                        enterWorld(saves[i].seed, saves[i].name, saves[i].name);
                        settle();
                    }
                    break;
            }
        }

        if (frame % 10 == 0) {
            const wchar_t* mode = (gs == GS::Play) ? L"Game" : L"Menu";
            wchar_t title[128];
            int ifps = (int)(renderer.fps() + 0.5f);
            swprintf(title, 128, L"Opencraft - %s - FPS: %d", mode, ifps);
            SetWindowTextW((HWND)win.hwnd(), title);
        }
        win.endFrame();
        if (a.frames > 0 && ++frame >= a.frames) break;
    }

shutdown_all:
    win.setCapture(false);
    if (world) world->stopWorkers();
    menu.shutdown(ctx);
    renderer.shutdown(ctx);
    ctx.shutdown();
    win.shutdown();
    return 0;
}
