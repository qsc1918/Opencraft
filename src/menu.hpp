#pragma once
#include "vk.hpp"
#include "save.hpp"
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
    MENU_GPU = 11,             // 显卡设备选择（切换后下次启动生效）
    MENU_TEXSHARP = 12,        // 近景清晰距离滑块
    MENU_SAVEANDTITLE = 200,
    MENU_SAVE_FIRST = 100,     // 存档按钮 100, 101, ...
    MENU_DELETE_FIRST = 500,   // 删除按钮 500+i
};

struct MenuData {
    std::vector<WorldSave> saves;
    std::string seedText;
    std::string titleText;          // 如“新建世界”等标题文本
    bool vsync = true;
    int renderDist = 8;             // 渲染距离（区块）
    int renderDistMin = 2, renderDistMax = 32;
    int* renderDistPtr = nullptr;   // 非空时，滑块把新值写回此处
    // 近景清晰距离（格）：此距离内方块不做 mip 模糊；0=关闭
    int texSharp = 16;
    int texSharpMin = 0, texSharpMax = 48;
    int* texSharpPtr = nullptr;
    // 显卡选择：gpuIndex=-1 表示自动（独显优先），否则为 ctx.gpus 下标
    std::vector<std::string> gpuNames;
    int gpuIndex = -1;
    int gpuActive = -1;             // 本次启动实际使用的设备序号
    int saveCount = 0;
};

// 全屏菜单渲染器：用 GDI 把菜单（泥土背景、九宫格按钮、系统字体）
// 画进纹理，再作为全屏四边形绘制。
class Menu {
public:
    bool init(VkCtx& ctx, Window& win, const std::string& assetDir);
    void shutdown(VkCtx& ctx);

    // 渲染指定界面，返回本帧点击的按钮 id（未点击为 MENU_NONE）。
    // cx,cy 为客户端光标坐标；mouseDown 表示本次刚按下。
    int renderMenu(VkCtx& ctx, Menuscreen screen, const MenuData& data,
                   float cx, float cy, bool mouseDown);

    void onResize(VkCtx& ctx, int w, int h);

    // 调试用：把上次渲染的菜单（DIB）存成 PNG。
    bool debugSaveMenu(const std::string& path) const;

private:
    bool loadTextures(const std::string& assetDir);
    void createMenuTexture(VkCtx& ctx);
    void destroyMenuTexture(VkCtx& ctx);
    void ensureDIB(int w, int h);
    void renderToDIB(Menuscreen screen, const MenuData& data, float cx, float cy);
    void uploadAndDraw(VkCtx& ctx);
    int  hitTest(float cx, float cy) const;

    // GDI 资源（只用 void* 存，这样非 Windows 也能过编译；实际只有 Win32 实现会用）
    void* dibBits_ = nullptr;      // 屏幕 DIB 像素缓冲
    void* dibBmp_ = nullptr;
    void* dibDC_ = nullptr;
    int     diw_ = 0, dih_ = 0;
    void* font_ = nullptr;

    // 已加载贴图（RGBA 与绘制用 DIB/HDC）
    std::vector<uint8_t> dirtRGBA_;
    std::vector<uint8_t> btnRGBA_, btnHiRGBA_, fieldRGBA_;
    std::vector<uint8_t> sliderRGBA_, handleRGBA_, handleHiRGBA_;
    int dirtW_ = 0, dirtH_ = 0, btnW_ = 0, btnH_ = 0;
    int sliderW_ = 0, sliderH_ = 0, handleW_ = 0, handleH_ = 0;
    // 一行一个声明：`void* a = nullptr, b = nullptr;` 在 MSVC 下会被
    // 当成函数声明（最令人烦恼的解析），别那么写。
    void* dirtDC_ = nullptr;
    void* btnDC_ = nullptr;
    void* btnHiDC_ = nullptr;
    void* fieldDC_ = nullptr;
    void* sliderDC_ = nullptr;
    void* handleDC_ = nullptr;
    void* handleHiDC_ = nullptr;
    void* dirtBmp_ = nullptr;
    void* btnBmp_ = nullptr;
    void* btnHiBmp_ = nullptr;
    void* fieldBmp_ = nullptr;
    void* sliderBmp_ = nullptr;
    void* handleBmp_ = nullptr;
    void* handleHiBmp_ = nullptr;

    // 滑块状态（每帧重建，可同时有多个：渲染距离/近景清晰）
    struct Slider {
        int id;                // 拖动时返回的按钮 id
        float x, y, w, h;
        int minV, maxV;
        int* val;              // 写回目标
        const char* label;     // 数值标签前缀
    };
    std::vector<Slider> sliders_;
    bool sliderDragging_ = false;
    int  dragSlider_ = -1;     // 正在拖动的滑块下标

    // Vulkan 资源
    VkImage    img_ = VK_NULL_HANDLE;
    VkDeviceMemory imgMem_ = VK_NULL_HANDLE;
    VkImageView view_ = VK_NULL_HANDLE;
    VkSampler  sampler_ = VK_NULL_HANDLE;
    VkBuffer   staging_ = VK_NULL_HANDLE;
    VkDeviceMemory stagingMem_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout dsl_ = VK_NULL_HANDLE;
    VkPipelineLayout layout_ = VK_NULL_HANDLE;
    VkPipeline  pipe_ = VK_NULL_HANDLE;
    VkDescriptorSet set_ = VK_NULL_HANDLE;
    VkBuffer   quadBuf_ = VK_NULL_HANDLE;
    VkDeviceMemory quadMem_ = VK_NULL_HANDLE;
    VkDescriptorPool mpool_ = VK_NULL_HANDLE;
    bool texReady_ = false;
    uint64_t frameIdx_ = 0;

    // 按钮矩形，每帧填充，用于命中测试
    struct Btn { int id; float x, y, w, h; };
    std::vector<Btn> btns_;
    bool prevMouseDown_ = false;
};
