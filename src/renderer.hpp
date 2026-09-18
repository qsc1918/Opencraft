#pragma once
#include "atlas.hpp"
#include "items.hpp"
#include "util.hpp"
#include "vk.hpp"
#include "window.hpp"
#include "world.hpp"
#include <string>

struct Camera;
struct Player;

struct UIVertex {
    float x, y;     // NDC 坐标
    float u, v;     // 图集 uv
    float r, g, b, a;
};

struct Buffer2 {
    VkBuffer b = VK_NULL_HANDLE;
    VkDeviceMemory m = VK_NULL_HANDLE;
    void destroy(VkDevice d) {
        if (b) vkDestroyBuffer(d, b, nullptr);
        if (m) vkFreeMemory(d, m, nullptr);
        b = VK_NULL_HANDLE; m = VK_NULL_HANDLE;
    }
};

class Renderer {
public:
    bool init(VkCtx& ctx, Window& win, const std::string& assetDir,
              const std::string& shaderDir);
    void shutdown(VkCtx& ctx);

    // 获取图像并录制提交一帧；交换链被重建时返回 false。
    bool render(VkCtx& ctx, const Camera& cam, Player& player, Input& in, float dt,
                float renderDist, bool drawUI);

    // 绑定待渲染的世界（进入新世界时会变）。
    void setWorld(World& w);

    // 区块网格的 GPU 管理（仅主线程调用）
    void destroyChunkBuffers(VkCtx& ctx, Chunk& c);

    // 等待上一帧的 GPU 工作结束。CPU 销毁或改写区块顶点/索引缓冲前必须调用，
    // 否则在飞帧可能仍在读这些缓冲，画面会闪烁/出现接缝。
    void gpuSync(VkCtx& ctx);

    void requestScreenshot(const std::string& path) { pendingShot_ = path; }
    void setTimeOfDay(float t) { timeOfDay_ = t; }

    int selectedSlot() const { return selectedSlot_; }
    uint8_t selectedBlock() const;
    // 手持杂项物品 id：选中格是打火石/末影之眼时返回对应 ItemId，否则 I_NONE。
    uint16_t heldMiscItem() const;
    float fps() const { return fps_; }
    int debugDraws() const { return debugDraws_; }

    // 物品栏（E）界面
    void setInventoryOpen(bool open);
    bool inventoryOpen() const { return invOpen_; }
    void setInventoryPage(int page) { invPage_ = page; }  // 0=方块 1=物品
    void setCursor(float x, float y) { cursorX_ = x; cursorY_ = y; }

private:
    bool createPipelines(VkCtx& ctx);
    void createAtlasTexture(VkCtx& ctx);
    void createDescriptors(VkCtx& ctx);
    void updateTerrainUBO(VkCtx& ctx, const Camera& cam, float renderDist, int slot);
    void drawChunks(VkCtx& ctx, const Camera& cam);
    void drawEntities(VkCtx& ctx, const Camera& cam);
    void drawUIOverlay(VkCtx& ctx, const Camera& cam, Input& in);
    void captureScreenshot(VkCtx& ctx, uint32_t imageIndex);
    void uploadPart(VkCtx& ctx, Chunk& c, bool opaque,
                    const std::vector<TerrainVertex>& verts,
                    const std::vector<uint32_t>& idx);
    void retireBuffer(VkBuffer b, VkDeviceMemory m, bool opaque, Chunk& c);
    void flushRetired(VkCtx& ctx, uint64_t submittedFrames);

    World* world_ = nullptr;
    VkCtx* ctxPtr_ = nullptr;
    Atlas atlas_;

    VkDescriptorSetLayout terrainDSL_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout uiDSL_ = VK_NULL_HANDLE;   // 仅采样器
    VkDescriptorSetLayout skyDSL_ = VK_NULL_HANDLE;  // 仅 UBO
    VkDescriptorPool pool_ = VK_NULL_HANDLE;

    VkPipelineLayout terrainLayout_ = VK_NULL_HANDLE;
    VkPipelineLayout skyLayout_ = VK_NULL_HANDLE;
    VkPipelineLayout uiLayout_ = VK_NULL_HANDLE;

    VkPipeline terrainPipe_ = VK_NULL_HANDLE;
    VkPipeline waterPipe_ = VK_NULL_HANDLE;
    VkPipeline skyPipe_ = VK_NULL_HANDLE;
    VkPipeline uiPipe_ = VK_NULL_HANDLE;
    VkPipeline entityPipe_ = VK_NULL_HANDLE;  // 实体盒模型管线（不剔面，写深度）

    VkImage atlasImage_ = VK_NULL_HANDLE;
    VkDeviceMemory atlasMem_ = VK_NULL_HANDLE;
    VkImageView atlasView_ = VK_NULL_HANDLE;
    VkSampler atlasSampler_ = VK_NULL_HANDLE;

    Buffer2 terrainUBO_[VkCtx::MAX_FRAMES_IN_FLIGHT];
    void* terrainUBOMap_[VkCtx::MAX_FRAMES_IN_FLIGHT] = {};
    VkDescriptorSet terrainSet_[VkCtx::MAX_FRAMES_IN_FLIGHT] = {};

    Buffer2 skyUBO_[VkCtx::MAX_FRAMES_IN_FLIGHT];
    void* skyUBOMap_[VkCtx::MAX_FRAMES_IN_FLIGHT] = {};
    VkDescriptorSet skySet_[VkCtx::MAX_FRAMES_IN_FLIGHT] = {};

    Buffer2 uiBuf_[VkCtx::MAX_FRAMES_IN_FLIGHT];
    void* uiMap_[VkCtx::MAX_FRAMES_IN_FLIGHT] = {};
    VkDescriptorSet uiSet_[VkCtx::MAX_FRAMES_IN_FLIGHT] = {};

    // 实体动态缓冲：每帧 CPU 构建盒模型，仅主线程绘制
    Buffer2 entityVB_[VkCtx::MAX_FRAMES_IN_FLIGHT];
    void* entityMap_[VkCtx::MAX_FRAMES_IN_FLIGHT] = {};

    // 已无引用的区块缓冲：在飞帧可能还在读，需等若干帧后才可释放。
    struct RetiredBuf { VkBuffer b; VkDeviceMemory m; uint64_t frame; VkDeviceSize size; void* mapPtr; };
    std::vector<RetiredBuf> retired_;
    // 退役缓冲安全后移入此池，供后续区块上传复用，避免流式加载时
    // 反复 vkCreateBuffer/vkAllocateMemory；池设上限以限制内存增长。
    std::vector<RetiredBuf> freePool_;

    int curFrame_ = 0;   // 当前录制的帧槽（0..MAX_FRAMES_IN_FLIGHT-1）

    VkBuffer shotBuf_ = VK_NULL_HANDLE;
    VkDeviceMemory shotMem_ = VK_NULL_HANDLE;
    bool shotBufReady_ = false;
    std::string pendingShot_;
    bool shotTaken_ = false;

    int windowW_ = 0, windowH_ = 0;
    float fps_ = 0.0f;
    float fpsTimer_ = 0.0f;
    int frames_ = 0;

    int selectedSlot_ = 0;
    float slotAnim_ = 0.0f;
    int lastWheel_ = 0;
    uint64_t frameIdx_ = 0;
    int shotW_ = 0, shotH_ = 0;
    int debugDraws_ = 0;
    Mat4 cachedVP_;
    float fogEndWorld_ = 0.0f;   // 雾完全遮蔽区块的世界空间距离
    float timeOfDay_ = 0.25f;
    float timeScale_ = 1.0f / 1200.0f; // 20 分钟走完一个昼夜循环

    bool invOpen_ = false;
    int invPage_ = 0;              // 0=方块页 1=物品页
    float cursorX_ = 0.0f, cursorY_ = 0.0f;
    bool prevMouse0_ = false;
    // 快捷栏每格是一个"手持资源"：< kItemTag 为方块 id（blockTile/放置），
    // >= kItemTag 为物品（kItemTag + ItemId，按 iconTile 渲染、右键使用）。
    static constexpr int kItemTag = 0x100;
    int hotbar_[9] = {B_GRASS, B_STONE, B_COBBLE, B_PLANKS, B_LOG,
                      B_DIRT, B_SAND, B_GRAVEL, B_GLASS};
    // 物品栏内容（全部可放置方块）
    static const uint8_t kInvBlocks[];
    static const int kInvCount;
    static const int kInvCols = 8;

    // 世界区块的裸指针快照，复用以避免每帧拷贝 shared_ptr。
    std::vector<World::ChunkInfo> snapshot_;
    std::vector<TerrainVertex> entityVerts_;  // 每帧构建的实体盒模型顶点
};
