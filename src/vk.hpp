#pragma once
#include <windows.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>
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
