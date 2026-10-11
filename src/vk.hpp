#pragma once
#ifdef _WIN32
#include <windows.h>   // vulkan_win32.h 要 HWND/HINSTANCE，必须排在它前面
#endif
#include <vulkan/vulkan.h>
// 窗口表面扩展按平台各取一个：Win32 用 win32，Linux(X11) 用 xlib。
// volk 会按这两个头文件生成对应的函数指针（见 volk_impl.c）。
#ifdef _WIN32
// vulkan_win32.h 要 HWND/HINSTANCE，windows.h 已经排在它前面了。
// 注意：Xlib 的头**不要**在这里包含——它会定义 None/Status/Success 这类
// 宏，污染所有包含 vk.hpp 的文件（菜单枚举都被打乱）。X11 的表面头在 vk.cpp 里
// 单独包含（那里会先包含 Xlib）。
#include <vulkan/vulkan_win32.h>
#endif
#include <volk.h>
#include <cstdint>
#include <string>
#include <vector>

struct Window;

// 供菜单"显卡选择"展示的物理设备摘要（init 时填充，下标即 --gpu-index 序号）
struct GpuInfo {
    std::string name;
    std::string type;      // 简短中文类型名
    uint64_t vramMB = 0;
    bool usable = false;   // 是否支持图形 + 交换链
    bool discrete = false;
};

// 完整 Vulkan 上下文：实例、设备、交换链、同步原语。
struct VkCtx {
    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkPhysicalDevice phys = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphics = VK_NULL_HANDLE;
    VkQueue present = VK_NULL_HANDLE;
    uint32_t graphicsFamily = 0;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkFormat swapFormat = VK_FORMAT_B8G8R8A8_UNORM;
    VkExtent2D extent = {0, 0};
    std::vector<VkImage> swapImages;
    std::vector<VkImageView> swapViews;
    VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;
    VkImage depthImage = VK_NULL_HANDLE;
    VkDeviceMemory depthMem = VK_NULL_HANDLE;
    VkImageView depthView = VK_NULL_HANDLE;
    VkRenderPass renderPass = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> framebuffers;

    VkCommandPool cmdPool = VK_NULL_HANDLE;
    // 每个在飞帧一个命令缓冲。渲染器与菜单每槽提交一帧，
    // 槽 N 的栅栏发出后才重置该槽（见 acquireNext），
    // 以此让 CPU 与 GPU 重叠。
    static const int MAX_FRAMES_IN_FLIGHT = 2;
    std::vector<VkCommandBuffer> cmds;

    struct Frame {
        VkSemaphore avail = VK_NULL_HANDLE;
        VkSemaphore done = VK_NULL_HANDLE;
        VkFence fence = VK_NULL_HANDLE;
    };
    std::vector<Frame> frames; // 大小 == MAX_FRAMES_IN_FLIGHT

    bool ok = false;
    bool vsync = true;
    int desiredGpuIndex = -1; // >=0 时强制用该物理设备序号（--gpu-index / options.txt）
    std::vector<GpuInfo> gpus; // 枚举到的全部物理设备
    int activeGpuIndex = -1;   // 本次启动实际使用的设备序号
    std::string lastError;

    bool init(Window& win, int w, int h);
    void shutdown();

    bool recreateSwapchain(int w, int h);
    void destroySwapchainObjects();
    void createDepth();
    void destroyDepth();

    // 提交已录制的命令缓冲并呈现；交换链过期时返回 false。
    bool presentImage(uint32_t imageIndex, Frame& f, VkCommandBuffer cmd);
    bool acquireNext(Frame& f, uint32_t& imageIndex);

    uint32_t findMemoryType(uint32_t typeBits, VkMemoryPropertyFlags props) const;
    VkFormat pickDepthFormat();
    void selectSwapFormat();
    VkPhysicalDeviceProperties props = {};
    VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;
};
