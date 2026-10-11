/* 包装编译单元：让 volk 实现能看到窗口平台的原生头文件，
   从而为 KHR_win32_surface（Windows）/ KHR_xlib_surface（Linux）生成函数指针。
   volk 本身会根据 _WIN32 自动选择平台扩展，但它包含的 vulkan_xlib.h 需要
   Xlib 的 Display/Window 类型，所以原生头必须排在 volk.h 之前。
   volk.c 用第三方的 third_party/volk（不是 SDK 里那份，见 CMakeLists 注释）。 */
#ifdef _WIN32
#include <windows.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>
#else
/* Xlib 有个 typedef Window，和本工程的窗口类同名；这里临时改名，
   免得 volk.h → vulkan_xlib.h 那一段被牵连。 */
#define Window XWindow
#include <X11/Xlib.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_xlib.h>
#undef Window
#endif
#include <volk.h>
#include "../../third_party/volk/volk.c"
