// ANGLE / GLES2 兜底档：用 EGL 创建 D3D11（或 SwiftShader）上下文，
// 复用 gfx_gl.cpp 的派发层与 GLSL ES 100 着色器。Vulkan 主后端完全不受影响。
#include "vk.hpp"
#include "gl_api.hpp"
#include "window.hpp"
#include "platform.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>

// gfx_gl.cpp 的后端状态，由 ANGLE 档复用其派发层。
extern bool g_gfxUseGL;
extern bool g_glLegacy;
extern bool g_gfxUseGLES;

#ifndef _WIN32
#include <dlfcn.h>
#endif

using EGLDisplay = void*;
using EGLConfig = void*;
using EGLContext = void*;
using EGLSurface = void*;
using EGLNativeWindowType = void*;
using EGLint = int;

namespace {
constexpr EGLint EGL_NONE = 0x3038;
constexpr EGLint EGL_DEFAULT_DISPLAY_VALUE = 0;
constexpr EGLint EGL_OPENGL_ES2_BIT = 0x0004;
constexpr EGLint EGL_OPENGL_ES_API = 0x30A0;
constexpr EGLint EGL_RENDERABLE_TYPE = 0x3040;
constexpr EGLint EGL_SURFACE_TYPE = 0x3033;
constexpr EGLint EGL_WINDOW_BIT = 0x0004;
constexpr EGLint EGL_RED_SIZE = 0x3024;
constexpr EGLint EGL_GREEN_SIZE = 0x3023;
constexpr EGLint EGL_BLUE_SIZE = 0x3022;
constexpr EGLint EGL_ALPHA_SIZE = 0x3021;
constexpr EGLint EGL_DEPTH_SIZE = 0x3025;
constexpr EGLint EGL_CONTEXT_CLIENT_VERSION = 0x3098;
constexpr EGLint EGL_PLATFORM_ANGLE_ANGLE = 0x3202;
constexpr EGLint EGL_PLATFORM_ANGLE_TYPE_ANGLE = 0x3203;
constexpr EGLint EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE = 0x3208;
constexpr EGLint EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE = 0x3450;
constexpr EGLint EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE = 0x3209;
constexpr EGLint EGL_PLATFORM_ANGLE_DEVICE_TYPE_SWIFTSHADER_ANGLE = 0x3487;

using EglGetProcFn = void* (*)(const char*);
using EglInitializeFn = int (*)(EGLDisplay, EGLint*, EGLint*);
using EglGetDisplayFn = EGLDisplay (*)(EGLNativeWindowType);
using EglGetPlatformDisplayFn = EGLDisplay (*)(EGLint, void*, const EGLint*);
using EglChooseConfigFn = int (*)(EGLDisplay, const EGLint*, EGLConfig*, EGLint, EGLint*);
using EglBindApiFn = int (*)(EGLint);
using EglCreateContextFn = EGLContext (*)(EGLDisplay, EGLConfig, EGLContext, const EGLint*);
using EglCreateWindowSurfaceFn = EGLSurface (*)(EGLDisplay, EGLConfig, EGLNativeWindowType, const EGLint*);
using EglMakeCurrentFn = int (*)(EGLDisplay, EGLSurface, EGLSurface, EGLContext);
using EglSwapBuffersFn = int (*)(EGLDisplay, EGLSurface);
using EglSwapIntervalFn = int (*)(EGLDisplay, int);
using EglDestroyContextFn = int (*)(EGLDisplay, EGLContext);
using EglDestroySurfaceFn = int (*)(EGLDisplay, EGLSurface);
using EglTerminateFn = int (*)(EGLDisplay);

EglGetProcFn eglProc = nullptr;
EglInitializeFn eglInitializeFn = nullptr;
EglChooseConfigFn eglChooseConfigFn = nullptr;
EglBindApiFn eglBindApiFn = nullptr;
EglCreateContextFn eglCreateContextFn = nullptr;
EglCreateWindowSurfaceFn eglCreateWindowSurfaceFn = nullptr;
EglMakeCurrentFn eglMakeCurrentFn = nullptr;
EglSwapBuffersFn eglSwapBuffersFn = nullptr;
EglDestroyContextFn eglDestroyContextFn = nullptr;
EglDestroySurfaceFn eglDestroySurfaceFn = nullptr;
EglTerminateFn eglTerminateFn = nullptr;
EGLDisplay eglDisplay = nullptr;
EGLContext eglContext = nullptr;
EGLSurface eglSurface = nullptr;

#ifdef _WIN32
HMODULE eglModule = nullptr;
HMODULE glesModule = nullptr;
#else
void* eglModule = nullptr;
void* glesModule = nullptr;
#endif

template<typename T> T GetEgl(const char* name) {
    return eglProc ? (T)eglProc(name) : nullptr;
}

gfxgl::Proc GetGlesProc(const char* name) {
    if (eglProc) {
        gfxgl::Proc p = (gfxgl::Proc)eglProc(name);
        if (p) return p;
    }
#ifdef _WIN32
    return (gfxgl::Proc)(void*)GetProcAddress(glesModule, name);
#else
    return (gfxgl::Proc)dlsym(glesModule, name);
#endif
}

void LoadEglFunctions() {
    eglInitializeFn = GetEgl<EglInitializeFn>("eglInitialize");
    eglChooseConfigFn = GetEgl<EglChooseConfigFn>("eglChooseConfig");
    eglBindApiFn = GetEgl<EglBindApiFn>("eglBindAPI");
    eglCreateContextFn = GetEgl<EglCreateContextFn>("eglCreateContext");
    eglCreateWindowSurfaceFn = GetEgl<EglCreateWindowSurfaceFn>("eglCreateWindowSurface");
    eglMakeCurrentFn = GetEgl<EglMakeCurrentFn>("eglMakeCurrent");
    eglSwapBuffersFn = GetEgl<EglSwapBuffersFn>("eglSwapBuffers");
    eglDestroyContextFn = GetEgl<EglDestroyContextFn>("eglDestroyContext");
    eglDestroySurfaceFn = GetEgl<EglDestroySurfaceFn>("eglDestroySurface");
    eglTerminateFn = GetEgl<EglTerminateFn>("eglTerminate");
}

bool IsLegacyWindows() {
#ifdef _WIN32
    // 新版 ANGLE 静态依赖 Win8 才有的 API set DLL；Win7/8/8.1 必须用随包的旧版。
    DWORD v = GetVersion();
    if (std::getenv("OPENCRAFT_ANGLE_WIN7")) return true; // 诊断用：本机强制加载旧版
    return LOBYTE(LOWORD(v)) < 10;
#else
    return false;
#endif
}

bool LoadEglLibrary() {
#ifdef _WIN32
    // DLL 与 exe 同目录；Win7/8/8.1 改从 angle-win7 子目录加载旧版 ANGLE。
    // 先设目录再 LoadLibrary，避免系统里其他 ANGLE 被误加载。
    std::string dir = platform::exeDir();
    if (IsLegacyWindows()) dir += "\\angle-win7";
    SetDllDirectoryA(dir.c_str());
    eglModule = LoadLibraryA("libEGL.dll");
    glesModule = LoadLibraryA("libGLESv2.dll");
#else
    eglModule = dlopen("libEGL.so", RTLD_NOW | RTLD_LOCAL);
    if (!eglModule) eglModule = dlopen("./libEGL.so", RTLD_NOW | RTLD_LOCAL);
    glesModule = dlopen("libGLESv2.so", RTLD_NOW | RTLD_LOCAL);
    if (!glesModule) glesModule = dlopen("./libGLESv2.so", RTLD_NOW | RTLD_LOCAL);
#endif
    if (!eglModule || !glesModule) {
        fprintf(stderr, "[angle] 找不到 libEGL / libGLESv2，请确认它们与 exe 在同一目录\n");
        return false;
    }
#ifdef _WIN32
    printf("[angle] 使用 %s ANGLE 运行库\n", IsLegacyWindows() ? "Win7 兼容版" : "当前系统版");
#endif
#ifdef _WIN32
    eglProc = (EglGetProcFn)(void*)GetProcAddress(eglModule, "eglGetProcAddress");
#else
    eglProc = (EglGetProcFn)dlsym(eglModule, "eglGetProcAddress");
#endif
    if (!eglProc) return false;
    LoadEglFunctions();
    return eglInitializeFn && eglChooseConfigFn && eglCreateContextFn &&
           eglCreateWindowSurfaceFn && eglMakeCurrentFn && eglSwapBuffersFn;
}

bool TryDisplay(Window& win, bool swiftShader) {
    EglGetPlatformDisplayFn getPlatform = GetEgl<EglGetPlatformDisplayFn>("eglGetPlatformDisplayEXT");
    if (getPlatform) {
        EGLint plat[5];
        int n = 0;
        plat[n++] = EGL_PLATFORM_ANGLE_TYPE_ANGLE;
#ifdef _WIN32
        plat[n++] = swiftShader ? EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE : EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE;
#else
        plat[n++] = EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE;
#endif
        if (swiftShader) {
            plat[n++] = EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE;
            plat[n++] = EGL_PLATFORM_ANGLE_DEVICE_TYPE_SWIFTSHADER_ANGLE;
        }
        plat[n] = EGL_NONE;
        eglDisplay = getPlatform(EGL_PLATFORM_ANGLE_ANGLE, nullptr, plat);
    }
    if (!eglDisplay) {
        EglGetDisplayFn getDisplay = GetEgl<EglGetDisplayFn>("eglGetDisplay");
        if (getDisplay) eglDisplay = getDisplay((EGLNativeWindowType)EGL_DEFAULT_DISPLAY_VALUE);
    }
    if (!eglDisplay || !eglInitializeFn(eglDisplay, nullptr, nullptr)) return false;

    const EGLint cfgAttribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 24, EGL_NONE
    };
    EGLConfig config = nullptr;
    EGLint count = 0;
    if (!eglChooseConfigFn(eglDisplay, cfgAttribs, &config, 1, &count) || count < 1) return false;
    if (!eglBindApiFn || !eglBindApiFn(EGL_OPENGL_ES_API)) return false;
    const EGLint ctxAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    eglContext = eglCreateContextFn(eglDisplay, config, nullptr, ctxAttribs);
    if (!eglContext) return false;
    eglSurface = eglCreateWindowSurfaceFn(eglDisplay, config, (EGLNativeWindowType)win.hwnd(), nullptr);
    if (!eglSurface) return false;
    return eglMakeCurrentFn(eglDisplay, eglSurface, eglSurface, eglContext) != 0;
}

void InitContextState(VkCtx& ctx, int w, int h) {
    gfxgl::DepthFunc(gfxgl::kLessEqual);
    gfxgl::PixelStorei(gfxgl::kPixelUnpackAlignment, 1);
    gfxgl::PixelStorei(gfxgl::kPackAlignment, 1);
    // GLSL ES 100 没有 gl_VertexID，天空用三个顶点。
    const float tri[6] = {0.0f, 0.0f, 2.0f, 0.0f, 0.0f, 2.0f};
    gfxgl::GenBuffers(1, &GfxSkyTriangle());
    gfxgl::BindBuffer(gfxgl::kArrayBuffer, GfxSkyTriangle());
    gfxgl::BufferData(gfxgl::kArrayBuffer, sizeof(tri), tri, gfxgl::kStaticDraw);
    auto interval = (EglSwapIntervalFn)eglProc("eglSwapIntervalEXT");
    if (interval) interval(eglDisplay, ctx.vsync ? 1 : 0);

    g_gfxUseGL = true;
    g_gfxUseEGL = true;
    g_glLegacy = true;
    g_gfxUseGLES = true;
    ctx.useGL = true;
    ctx.ok = true;
    ctx.extent = { (uint32_t)w, (uint32_t)h };
    ctx.swapFormat = VK_FORMAT_R8G8B8A8_SRGB;
    ctx.frames.resize(VkCtx::MAX_FRAMES_IN_FLIGHT);
    ctx.cmds.assign(VkCtx::MAX_FRAMES_IN_FLIGHT, (VkCommandBuffer)(uintptr_t)0x100);
    ctx.framebuffers.assign(1, (VkFramebuffer)(uintptr_t)0x100);
    ctx.swapImages.assign(1, (VkImage)(uintptr_t)0x100);
    ctx.swapViews.assign(1, (VkImageView)(uintptr_t)0x100);
}
} // namespace

bool gfxAngleInit(VkCtx& ctx, Window& win, int w, int h) {
    if (!LoadEglLibrary()) return false;
    // 环境变量便于无 GPU 的机器直接走 SwiftShader 软件光栅化。
    bool preferSoftware = std::getenv("OPENCRAFT_ANGLE_SWIFTSHADER") != nullptr;
    if (preferSoftware || !TryDisplay(win, false)) {
        fprintf(stderr, "[angle] 改用 SwiftShader 软件后端\n");
        if (eglDisplay) {
            if (eglSurface) eglDestroySurfaceFn(eglDisplay, eglSurface);
            if (eglContext) eglDestroyContextFn(eglDisplay, eglContext);
            eglTerminateFn(eglDisplay);
            eglDisplay = nullptr; eglContext = nullptr; eglSurface = nullptr;
        }
        if (!TryDisplay(win, true)) {
            fprintf(stderr, "[angle] ANGLE 上下文创建失败\n");
            return false;
        }
    }
    if (!gfxgl::LoadFunctions(GetGlesProc, true)) return false;
    InitContextState(ctx, w, h);
    printf("[angle] 使用 ANGLE / OpenGL ES 2.0 兜底后端\n");
    return true;
}

void gfxAngleSwap() {
    if (eglDisplay && eglSurface && eglSwapBuffersFn) eglSwapBuffersFn(eglDisplay, eglSurface);
}

void gfxAngleShutdown() {
    if (eglDisplay) {
        if (eglSurface) eglDestroySurfaceFn(eglDisplay, eglSurface);
        if (eglContext) eglDestroyContextFn(eglDisplay, eglContext);
        eglTerminateFn(eglDisplay);
    }
    eglDisplay = nullptr;
    eglContext = nullptr;
    eglSurface = nullptr;
#ifdef _WIN32
    if (eglModule) FreeLibrary(eglModule);
    if (glesModule) FreeLibrary(glesModule);
#else
    if (eglModule) dlclose(eglModule);
    if (glesModule) dlclose(glesModule);
#endif
    eglModule = nullptr;
    glesModule = nullptr;
}
