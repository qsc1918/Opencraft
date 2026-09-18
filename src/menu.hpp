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

    // GDI 资源
    void* dibBits_ = nullptr;      // 屏幕 DIB 像素缓冲
    HBITMAP dibBmp_ = nullptr;
    HDC     dibDC_ = nullptr;
    int     diw_ = 0, dih_ = 0;
    HFONT   font_ = nullptr;

    // 已加载贴图（RGBA 与绘制用 DIB/HDC）
    std::vector<uint8_t> dirtRGBA_;
    std::vector<uint8_t> btnRGBA_, btnHiRGBA_, fieldRGBA_;
    std::vector<uint8_t> sliderRGBA_, handleRGBA_, handleHiRGBA_;
    int dirtW_ = 0, dirtH_ = 0, btnW_ = 0, btnH_ = 0;
    int sliderW_ = 0, sliderH_ = 0, handleW_ = 0, handleH_ = 0;
    HDC dirtDC_ = nullptr, btnDC_ = nullptr, btnHiDC_ = nullptr, fieldDC_ = nullptr;
    HDC sliderDC_ = nullptr, handleDC_ = nullptr, handleHiDC_ = nullptr;
    HBITMAP dirtBmp_ = nullptr, btnBmp_ = nullptr, btnHiBmp_ = nullptr, fieldBmp_ = nullptr;
    HBITMAP sliderBmp_ = nullptr, handleBmp_ = nullptr, handleHiBmp_ = nullptr;

    // 渲染距离滑块状态（几何每帧刷新）
    bool sliderActive_ = false;
    bool sliderDragging_ = false;
    float sliderMinX_ = 0, sliderMaxX_ = 0, sliderY_ = 0, sliderGH_ = 0;
    int sliderMinV_ = 2, sliderMaxV_ = 32;

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
