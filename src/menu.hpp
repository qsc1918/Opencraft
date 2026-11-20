#pragma once
#include "vk.hpp"
#include "save.hpp"
#include "font.hpp"
#include <string>
#include <vector>

class Window;

// 当前激活的全屏菜单；None 表示无菜单（进入游戏）。
enum class Menuscreen {
    None,
    MainMenu,
    SaveSelect,
    NewWorld,
    Options,
    VideoSettings,
    Pause,
};

// renderMenu() 返回的按钮 id。
enum {
    MENU_NONE = -1,
    MENU_SINGLEPLAYER = 1,
    MENU_OPTIONS = 2,
    MENU_QUIT = 3,
    MENU_NEWWORLD = 4,
    MENU_CREATE = 5,
    MENU_CANCEL = 6,
    MENU_VIDEO = 7,
    MENU_BACK = 8,
    MENU_VSYNC = 9,
    MENU_RENDERDIST = 10,
    MENU_GPU = 11,
    MENU_TEXSHARP = 12,
    MENU_SAVEANDTITLE = 200,
    MENU_SAVE_FIRST = 100,
    MENU_DELETE_FIRST = 500,
};

struct MenuData {
    std::vector<WorldSave> saves;
    std::string seedText;
    std::string titleText;
    bool vsync = true;
    int renderDist = 8;
    int renderDistMin = 2, renderDistMax = 32;
    int* renderDistPtr = nullptr;
    int texSharp = 16;
    int texSharpMin = 0, texSharpMax = 48;
    int* texSharpPtr = nullptr;
    std::vector<std::string> gpuNames;
    int gpuIndex = -1;
    int gpuActive = -1;
    int saveCount = 0;
};

// 菜单先在 CPU 画布上画成一张 RGBA 图，再作为全屏四边形绘制。
// 画布绘制只用自带的像素字体，不依赖 Windows GDI，因此 Windows/Linux 共用一份实现。
class Menu {
public:
    bool init(VkCtx& ctx, Window& win, const std::string& assetDir);
    void shutdown(VkCtx& ctx);

    // 渲染指定界面，返回本帧点击的按钮 id（未点击为 MENU_NONE）
    int renderMenu(VkCtx& ctx, Menuscreen screen, const MenuData& data,
                   float cx, float cy, bool mouseDown);

    void onResize(VkCtx& ctx, int w, int h);

    // 调试用：把上次渲染的菜单存成 PNG。
    bool debugSaveMenu(const std::string& path) const;

private:
    bool LoadTextures(const std::string& assetDir);
    void CreateMenuTexture(VkCtx& ctx);
    void DestroyMenuTexture(VkCtx& ctx);
    void EnsureCanvas(int w, int h);
    void RenderCanvas(Menuscreen screen, const MenuData& data, float cx, float cy);
    void UploadAndDraw(VkCtx& ctx);
    int HitTest(float cx, float cy) const;

    Font font_;
    std::vector<uint8_t> canvas_;
    int canvasW_ = 0, canvasH_ = 0;

    // 已加载贴图（自上而下 RGBA）
    std::vector<uint8_t> dirtRGBA_;
    std::vector<uint8_t> btnRGBA_, btnHiRGBA_, fieldRGBA_;
    std::vector<uint8_t> sliderRGBA_, handleRGBA_, handleHiRGBA_;
    int dirtW_ = 0, dirtH_ = 0, btnW_ = 0, btnH_ = 0;
    int fieldW_ = 0, fieldH_ = 0;
    int sliderW_ = 0, sliderH_ = 0, handleW_ = 0, handleH_ = 0;

    struct Slider {
        int id;
        float x, y, w, h;
        int minV, maxV;
        int* val;
        const char* label;
    };
    std::vector<Slider> sliders_;
    bool sliderDragging_ = false;
    int dragSlider_ = -1;

    // Vulkan 资源（GL 回退由派发层映射）。
    VkImage img_ = VK_NULL_HANDLE;
    VkDeviceMemory imgMem_ = VK_NULL_HANDLE;
    VkImageView view_ = VK_NULL_HANDLE;
    VkSampler sampler_ = VK_NULL_HANDLE;
    VkBuffer staging_ = VK_NULL_HANDLE;
    VkDeviceMemory stagingMem_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout dsl_ = VK_NULL_HANDLE;
    VkPipelineLayout layout_ = VK_NULL_HANDLE;
    VkPipeline pipe_ = VK_NULL_HANDLE;
    VkDescriptorSet set_ = VK_NULL_HANDLE;
    VkBuffer quadBuf_ = VK_NULL_HANDLE;
    VkDeviceMemory quadMem_ = VK_NULL_HANDLE;
    VkDescriptorPool mpool_ = VK_NULL_HANDLE;
    bool texReady_ = false;
    uint64_t frameIdx_ = 0;

    struct Btn { int id; float x, y, w, h; };
    std::vector<Btn> btns_;
    bool prevMouseDown_ = false;
};
