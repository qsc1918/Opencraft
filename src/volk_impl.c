/* 包装编译单元：让 volk 实现能看到 Win32 平台头文件，
   从而为 KHR_win32_surface 扩展生成函数指针。 */
#include <windows.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>
#include <volk.h>
#include <volk.c>
