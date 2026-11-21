// OpenGL 回退后端。只在 Vulkan 不可用时启用；Vulkan 路径的行为完全不受影响。
// 这里的 gfx* 函数与 gfx_shim.hpp 声明一一对应，Vulkan 分支原样转发到 volk，
// 其余分支用最小的 GL 3.3 调用实现同样的渲染语义。
#define GFX_SHIM_IMPL
#include "gfx_shim.hpp"
#include "gl_api.hpp"
#include "window.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#define Window XWindow
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#undef Window
#include <GL/glx.h>
#endif

bool g_gfxUseGL = false;
bool g_gfxUseEGL = false; // 当前上下文来自 EGL/ANGLE
bool g_glLegacy = false; // 老设备档：GLSL 120 + 顶点属性转 float
bool g_gfxUseGLES = false; // ANGLE 档：EGL 上下文 + GLSL ES 100

namespace {

struct BufferRec {
    size_t size = 0;
    std::vector<uint8_t> cpu;
    uint32_t gl = 0;
    // 老设备档：12 字节整数顶点转成 32 字节浮点顶点，索引另存一份。
    uint32_t legacyGl = 0;
    uint32_t legacyIndexGl = 0;
    size_t legacyVertexBytes = 0;
    // 缓冲会被 freePool_ 复用并原地改写，签名变化时必须重做转换，否则画出旧几何。
    uint8_t signature[32] = {};
};
struct ImageRec {
    int w = 0, h = 0;
    bool srgb = false;
    uint32_t mips = 1;
    uint32_t gl = 0;
};
struct MemRec {
    size_t size = 0;
    std::vector<uint8_t> cpu;
    VkImage image = VK_NULL_HANDLE;
    VkBuffer buffer = VK_NULL_HANDLE;
};
struct SamplerRec {
    bool nearest = false; // min/mag 都是最近邻，且锁 mip0
    bool mip = true;      // 允许 mipmap
    bool magLinear = false;
};
struct DescSetRec {
    std::unordered_map<uint32_t, VkBuffer> buffers;
    std::unordered_map<uint32_t, uint64_t> images;  // binding → VkImageView 句柄
    std::unordered_map<uint32_t, VkSampler> samplers; // binding → VkSampler
};
struct PipelineRec {
    uint32_t program = 0;
    int vertexKind = 0; // 0=无顶点属性 1=ivec4+uvec4 2=两 vec2+vec4
    bool blend = false, depthTest = false, depthWrite = false, cull = false;
    std::unordered_map<std::string, int> locs;
    int Loc(const char* name) {
        auto it = locs.find(name);
        if (it != locs.end()) return it->second;
        int l = gfxgl::GetUniformLocation(program, name);
        locs[name] = l;
        return l;
    }
};
struct ViewRec {
    uint32_t texture = 0;
};

std::unordered_map<uint64_t, BufferRec> g_buffers;
std::unordered_map<uint64_t, MemRec> g_mems;
std::unordered_map<uint64_t, ImageRec> g_images;
std::unordered_map<uint64_t, SamplerRec> g_samplers;
std::unordered_map<uint64_t, DescSetRec> g_sets;
std::unordered_map<uint64_t, PipelineRec> g_pipelines;
std::unordered_map<uint64_t, ViewRec> g_views;

uint32_t g_skyTriangle = 0; // 老设备档没有 gl_VertexID，天空改用三个顶点
VkBuffer g_boundVB = VK_NULL_HANDLE;
VkBuffer g_boundIB = VK_NULL_HANDLE;
VkIndexType g_indexType = VK_INDEX_TYPE_UINT32;
VkDeviceSize g_indexOffset = 0;
size_t g_legacyVertexBytes = 0; // 老设备档记录原始顶点区大小，用于惰性转换
VkPipeline g_boundPipe = VK_NULL_HANDLE;
VkDescriptorSet g_boundSet = VK_NULL_HANDLE;
VkPipelineLayout g_boundLayout = VK_NULL_HANDLE;
float g_origin[4] = {0, 0, 0, 0};
uint64_t g_handle = 0x1000;

uint64_t NextHandle() { return g_handle++; }

template<typename T> T* Lookup(std::unordered_map<uint64_t, T>& m, uint64_t key) {
    auto it = m.find(key);
    return it == m.end() ? nullptr : &it->second;
}

void ApplySampler(uint32_t unit, VkSampler sampler) {
    gfxgl::ActiveTexture(gfxgl::kTexture0 + unit);
    SamplerRec* sr = Lookup(g_samplers, (uint64_t)sampler);
    bool nearest = sr && sr->nearest;
    bool mip = !sr || sr->mip;
    gfxgl::TexParameteri(gfxgl::kTexture2D, gfxgl::kTexMinFilter,
                         nearest ? gfxgl::kNearest : (mip ? gfxgl::kLinearMipmapLinear : gfxgl::kLinear));
    gfxgl::TexParameteri(gfxgl::kTexture2D, gfxgl::kTexMagFilter,
                         (sr && sr->magLinear) ? gfxgl::kLinear : gfxgl::kNearest);
    gfxgl::TexParameteri(gfxgl::kTexture2D, gfxgl::kTexWrapS, gfxgl::kClampToEdge);
    gfxgl::TexParameteri(gfxgl::kTexture2D, gfxgl::kTexWrapT, gfxgl::kClampToEdge);
}

void BindTextureUnit(uint32_t unit, uint64_t imageView, VkSampler sampler) {
    ViewRec* v = Lookup(g_views, imageView);
    gfxgl::ActiveTexture(gfxgl::kTexture0 + unit);
    gfxgl::BindTexture(gfxgl::kTexture2D, v ? v->texture : 0);
    ApplySampler(unit, sampler);
}

void UploadBuffer(VkBuffer buffer) {
    BufferRec* b = Lookup(g_buffers, (uint64_t)buffer);
    if (!b || !b->gl || b->cpu.empty()) return;
    gfxgl::BindBuffer(gfxgl::kArrayBuffer, b->gl);
    gfxgl::BufferSubData(gfxgl::kArrayBuffer, 0, (gfxgl::Sizeiptr)b->cpu.size(), b->cpu.data());
}

// 老设备档把 12 字节整数顶点（4×int16 + 4×uint8）转成两个 vec4 浮点属性。
void EnsureLegacyFormat(VkBuffer buffer, size_t vertexBytes) {
    BufferRec* b = Lookup(g_buffers, (uint64_t)buffer);
    if (!b || vertexBytes == 0 || vertexBytes > b->cpu.size()) return;
    size_t sigBytes = std::min<size_t>(32, b->cpu.size());
    if (b->legacyGl && b->legacyVertexBytes == vertexBytes &&
        memcmp(b->cpu.data(), b->signature, sigBytes) == 0) return;
    if (b->legacyGl) { gfxgl::DeleteBuffers(1, &b->legacyGl); b->legacyGl = 0; }
    if (b->legacyIndexGl) { gfxgl::DeleteBuffers(1, &b->legacyIndexGl); b->legacyIndexGl = 0; }
    size_t count = vertexBytes / 12;
    std::vector<float> converted(count * 8);
    for (size_t i = 0; i < count; i++) {
        const uint8_t* p = b->cpu.data() + i * 12;
        int16_t x, y, z, w;
        memcpy(&x, p, 2); memcpy(&y, p + 2, 2);
        memcpy(&z, p + 4, 2); memcpy(&w, p + 6, 2);
        float* out = converted.data() + i * 8;
        out[0] = (float)x; out[1] = (float)y; out[2] = (float)z; out[3] = (float)w;
        out[4] = (float)p[8]; out[5] = (float)p[9];
        out[6] = (float)p[10]; out[7] = (float)p[11];
    }
    gfxgl::GenBuffers(1, &b->legacyGl);
    gfxgl::BindBuffer(gfxgl::kArrayBuffer, b->legacyGl);
    gfxgl::BufferData(gfxgl::kArrayBuffer, (gfxgl::Sizeiptr)(converted.size() * sizeof(float)),
                      converted.data(), gfxgl::kStaticDraw);
    size_t indexBytes = b->cpu.size() - vertexBytes;
    if (indexBytes) {
        gfxgl::GenBuffers(1, &b->legacyIndexGl);
        gfxgl::BindBuffer(gfxgl::kElementBuffer, b->legacyIndexGl);
        gfxgl::BufferData(gfxgl::kElementBuffer, (gfxgl::Sizeiptr)indexBytes,
                          b->cpu.data() + vertexBytes, gfxgl::kStaticDraw);
    }
    b->legacyVertexBytes = vertexBytes;
    memset(b->signature, 0, sizeof(b->signature));
    memcpy(b->signature, b->cpu.data(), sigBytes);
}

void SetUniforms(PipelineRec& p, int kind, DescSetRec* set) {
    if (!set) return;
    if (kind == 0) {
        // 天空：UBO 只有 horizon / zenith 两个 vec4。
        for (auto& kv : set->buffers) {
            BufferRec* b = Lookup(g_buffers, (uint64_t)kv.second);
            if (!b || b->cpu.size() < 64) continue;
            const float* f = (const float*)b->cpu.data();
            int l = p.Loc("horizon"); if (l >= 0) gfxgl::Uniform4f(l, f[0], f[1], f[2], f[3]);
            l = p.Loc("zenith"); if (l >= 0) gfxgl::Uniform4f(l, f[4], f[5], f[6], f[7]);
            break;
        }
        return;
    }
    if (kind == 1) {
        // 地形/水/实体共用同一份 std140 镜像：前 36 float 是公共参数，后面是 tileRect。
        for (auto& kv : set->buffers) {
            BufferRec* b = Lookup(g_buffers, (uint64_t)kv.second);
            if (!b || b->cpu.size() < 36 * 4) continue;
            const float* f = (const float*)b->cpu.data();
            int l = p.Loc("viewProj"); if (l >= 0) gfxgl::UniformMatrix4fv(l, 1, gfxgl::kFalse, f);
            l = p.Loc("camPos"); if (l >= 0) gfxgl::Uniform4fv(l, 1, f + 16);
            l = p.Loc("fogParams"); if (l >= 0) gfxgl::Uniform4fv(l, 1, f + 20);
            l = p.Loc("misc"); if (l >= 0) gfxgl::Uniform4fv(l, 1, f + 24);
            l = p.Loc("day"); if (l >= 0) gfxgl::Uniform4fv(l, 1, f + 28);
            l = p.Loc("texSharp"); if (l >= 0) gfxgl::Uniform4fv(l, 1, f + 32);
            l = p.Loc("tileRect");
            if (l >= 0 && b->cpu.size() >= 548 * 4) gfxgl::Uniform4fv(l, 128, f + 36);
            break;
        }
        int l = p.Loc("pcOrigin");
        if (l >= 0) gfxgl::Uniform4f(l, g_origin[0], g_origin[1], g_origin[2], g_origin[3]);
        return;
    }
}

void PrepareDraw(VkPipeline pipe, VkDescriptorSet set) {
    PipelineRec* pr = Lookup(g_pipelines, (uint64_t)pipe);
    if (!pr) return;
    DescSetRec* ds = Lookup(g_sets, (uint64_t)set);
    SetUniforms(*pr, pr->vertexKind, ds);
    if (!ds) return;
    if (pr->vertexKind == 1) {
        auto it1 = ds->images.find(1);
        auto it2 = ds->images.find(2);
        auto s1 = ds->samplers.find(1);
        auto s2 = ds->samplers.find(2);
        BindTextureUnit(0, it1 == ds->images.end() ? 0 : it1->second,
                        s1 == ds->samplers.end() ? VK_NULL_HANDLE : s1->second);
        BindTextureUnit(1, it2 == ds->images.end() ? 0 : it2->second,
                        s2 == ds->samplers.end() ? VK_NULL_HANDLE : s2->second);
        int l = pr->Loc("atlas"); if (l >= 0) gfxgl::Uniform1i(l, 0);
        l = pr->Loc("atlasSharp"); if (l >= 0) gfxgl::Uniform1i(l, 1);
    } else if (pr->vertexKind == 2) {
        auto it = ds->images.find(0);
        auto st = ds->samplers.find(0);
        BindTextureUnit(0, it == ds->images.end() ? 0 : it->second,
                        st == ds->samplers.end() ? VK_NULL_HANDLE : st->second);
        int l = pr->Loc("atlas"); if (l >= 0) gfxgl::Uniform1i(l, 0);
        l = pr->Loc("menu"); if (l >= 0) gfxgl::Uniform1i(l, 0);
    }
}

std::string ReadText(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// GL 着色器与 Vulkan 着色器分开存放，Vulkan 的 450 版本保持不动。
std::string GlShaderPath(const std::string& spvDir, const std::string& name) {
    std::string base = name;
    size_t dot = base.find(".spv");
    if (dot != std::string::npos) base.resize(dot);
    // 老设备档用 GLSL 120，完整档用 330；两者都是从 SPV 文件名派生。
    const char* tier = g_gfxUseGLES ? "gles100" : (g_glLegacy ? "gl120" : "gl330");
    std::string p = spvDir + "/" + tier + "/" + base + ".glsl";
    if (!ReadText(p).empty()) return p;
    std::string exeRel = std::string("shaders/") + tier + "/" + base + ".glsl";
    if (!ReadText(exeRel).empty()) return exeRel;
    return p;
}

uint32_t CompileShader(unsigned int type, const std::string& src, const char* label) {
    uint32_t s = gfxgl::CreateShader(type);
    const char* ptr = src.c_str();
    gfxgl::ShaderSource(s, 1, &ptr, nullptr);
    gfxgl::CompileShader(s);
    int ok = 0;
    gfxgl::GetShaderiv(s, gfxgl::kCompileStatus, &ok);
    if (!ok) {
        char log[2048] = {};
        gfxgl::GetShaderInfoLog(s, sizeof(log), nullptr, log);
        fprintf(stderr, "[gl] %s compile failed: %s\n", label, log);
        gfxgl::DeleteShader(s);
        return 0;
    }
    return s;
}

int ClassifyVertex(const std::string& vsName) {
    if (vsName.find("terrain") != std::string::npos ||
        vsName.find("water") != std::string::npos ||
        vsName.find("entity") != std::string::npos) return 1;
    if (vsName.find("ui") != std::string::npos ||
        vsName.find("menu") != std::string::npos) return 2;
    return 0;
}

} // namespace

namespace gfxgl {

Proc (*getProc)(const char*) = nullptr;

#define GLF(name) name##Fn name = nullptr
GLF(GetIntegerv); GLF(GetString); GLF(Viewport); GLF(Scissor);
GLF(ClearColor); GLF(ClearDepth); GLF(Clear); GLF(Enable); GLF(Disable);
GLF(DepthFunc); GLF(DepthMask); GLF(BlendFunc); GLF(CullFace); GLF(FrontFace);
GLF(PixelStorei); GLF(Flush); GLF(Finish); GLF(GetError);
GLF(GenBuffers); GLF(DeleteBuffers); GLF(BindBuffer); GLF(BufferData); GLF(BufferSubData);
GLF(GenVertexArrays); GLF(DeleteVertexArrays); GLF(BindVertexArray);
GLF(GenTextures); GLF(DeleteTextures); GLF(BindTexture); GLF(ActiveTexture);
GLF(TexImage2D); GLF(TexSubImage2D); GLF(GenerateMipmap); GLF(TexParameteri); GLF(TexParameterf);
GLF(CreateShader); GLF(DeleteShader); GLF(ShaderSource); GLF(CompileShader);
GLF(GetShaderiv); GLF(GetShaderInfoLog); GLF(CreateProgram); GLF(DeleteProgram);
GLF(AttachShader); GLF(LinkProgram); GLF(GetProgramiv); GLF(GetProgramInfoLog);
GLF(UseProgram); GLF(BindAttribLocation); GLF(GetUniformLocation);
GLF(Uniform1i); GLF(Uniform4f); GLF(Uniform1fv); GLF(Uniform2fv); GLF(Uniform4fv); GLF(UniformMatrix4fv);
GLF(VertexAttribPointer); GLF(VertexAttribIPointer); GLF(EnableVertexAttribArray);
GLF(DisableVertexAttribArray); GLF(DrawArrays); GLF(DrawElements); GLF(ReadPixels);
#undef GLF

namespace {
// GL 1.1 的函数不在 wglGetProcAddress 里，必须从 opengl32.dll 直接取。
void* LoadCoreFunction(const char* name) {
#ifdef _WIN32
    static HMODULE mod = GetModuleHandleA("opengl32.dll");
    return mod ? (void*)GetProcAddress(mod, name) : nullptr;
#else
    (void)name;
    return nullptr;
#endif
}
} // namespace

bool LoadFunctions(Proc (*gp)(const char*), bool legacy) {
    getProc = gp;
#define LOAD_IMPL(name, required) \
    name = reinterpret_cast<decltype(name)>(getProc("gl" #name)); \
    if (!name) name = reinterpret_cast<decltype(name)>(LoadCoreFunction("gl" #name)); \
    if (!name && required) { fprintf(stderr, "[gl] 缺少函数 gl%s\n", #name); return false; }
#define LOAD(name) LOAD_IMPL(name, true)
    LOAD(GetIntegerv); LOAD(GetString); LOAD(Viewport); LOAD(Scissor);
    LOAD(ClearColor); LOAD(ClearDepth); LOAD(Clear); LOAD(Enable); LOAD(Disable);
    LOAD(DepthFunc); LOAD(DepthMask); LOAD(BlendFunc); LOAD(CullFace); LOAD(FrontFace);
    LOAD(PixelStorei); LOAD(Flush); LOAD(Finish); LOAD(GetError);
    LOAD(GenBuffers); LOAD(DeleteBuffers); LOAD(BindBuffer); LOAD(BufferData); LOAD(BufferSubData);
    LOAD_IMPL(GenVertexArrays, !legacy);
    LOAD_IMPL(DeleteVertexArrays, !legacy);
    LOAD_IMPL(BindVertexArray, !legacy);
    LOAD(GenTextures); LOAD(DeleteTextures); LOAD(BindTexture); LOAD(ActiveTexture);
    LOAD(TexImage2D); LOAD(TexSubImage2D); LOAD(GenerateMipmap); LOAD(TexParameteri); LOAD(TexParameterf);
    LOAD(CreateShader); LOAD(DeleteShader); LOAD(ShaderSource); LOAD(CompileShader);
    LOAD(GetShaderiv); LOAD(GetShaderInfoLog); LOAD(CreateProgram); LOAD(DeleteProgram);
    LOAD(AttachShader); LOAD(LinkProgram); LOAD(GetProgramiv); LOAD(GetProgramInfoLog);
    LOAD(UseProgram); LOAD(BindAttribLocation); LOAD(GetUniformLocation);
    LOAD(Uniform1i); LOAD(Uniform4f); LOAD(Uniform1fv); LOAD(Uniform2fv); LOAD(Uniform4fv);
    LOAD(UniformMatrix4fv);
    LOAD(VertexAttribPointer);
    LOAD_IMPL(VertexAttribIPointer, !legacy);
    LOAD(EnableVertexAttribArray); LOAD(DisableVertexAttribArray);
    LOAD(DrawArrays); LOAD(DrawElements); LOAD(ReadPixels);
#undef LOAD
#undef LOAD_IMPL
    return true;
}

void ClearFunctions() { getProc = nullptr; }


// ---------------------------------------------------------------------------
// 上下文：Win32 用 WGL，Linux 用 GLX。都只创建一次，进程退出时销毁。
// ---------------------------------------------------------------------------
namespace {

struct GlContext {
#ifdef _WIN32
    HWND hwnd = nullptr;
    HDC dc = nullptr;
    HGLRC rc = nullptr;
#else
    Display* dpy = nullptr;
    ::XWindow win = 0;
    GLXContext ctx = nullptr;
#endif
    uint32_t vao = 0;
    bool valid = false;
    bool legacy = false; // OpenGL 2.1 / GLSL 120 老设备档
};
GlContext g_glctx;

void SetVsync(bool on) {
#ifdef _WIN32
    using SwapIntervalFn = int (WINAPI*)(int);
    auto fn = (SwapIntervalFn)gfxgl::getProc("wglSwapIntervalEXT");
    if (fn) fn(on ? 1 : 0);
#else
    auto fn = (int (*)(Display*, ::XWindow, int))gfxgl::getProc("glXSwapIntervalEXT");
    if (fn) fn(g_glctx.dpy, g_glctx.win, on ? 1 : 0);
#endif
}

bool QueryVersion(int& major, int& minor) {
    const char* v = (const char*)gfxgl::GetString(gfxgl::kVersion);
    if (!v || sscanf(v, "%d.%d", &major, &minor) != 2) return false;
    printf("[gl] OpenGL %s (%s)\n", v, (const char*)gfxgl::GetString(gfxgl::kRenderer));
    return true;
}

bool AtLeast(int major, int minor, int wantMajor, int wantMinor) {
    return major > wantMajor || (major == wantMajor && minor >= wantMinor);
}

void DestroyGlContext() {
#ifdef _WIN32
    if (g_glctx.rc) { wglMakeCurrent(nullptr, nullptr); wglDeleteContext(g_glctx.rc); }
    if (g_glctx.dc) ReleaseDC(g_glctx.hwnd, g_glctx.dc);
#else
    if (g_glctx.ctx) glXDestroyContext(g_glctx.dpy, g_glctx.ctx);
    if (g_glctx.dpy) XCloseDisplay(g_glctx.dpy);
#endif
    g_glctx = GlContext{};
}

bool CreateGlContext(Window& win, bool core) {
#ifdef _WIN32
    g_glctx.hwnd = (HWND)win.hwnd();
    g_glctx.dc = GetDC(g_glctx.hwnd);
    if (!g_glctx.dc) return false;
    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    int fmt = ChoosePixelFormat(g_glctx.dc, &pfd);
    if (!fmt || !SetPixelFormat(g_glctx.dc, fmt, &pfd)) return false;
    // 临时上下文只为取 wglCreateContextAttribsARB；老设备档直接用它。
    HGLRC dummy = wglCreateContext(g_glctx.dc);
    if (!dummy) return false;
    wglMakeCurrent(g_glctx.dc, dummy);
    if (core) {
        using CreateAttribsFn = HGLRC (WINAPI*)(HDC, HGLRC, const int*);
        auto createAttribs = (CreateAttribsFn)wglGetProcAddress("wglCreateContextAttribsARB");
        if (createAttribs) {
            const int attribs[] = {
                0x2091, 3, 0x2092, 3,        // MAJOR / MINOR
                0
            };
            g_glctx.rc = createAttribs(g_glctx.dc, nullptr, attribs);
        }
        if (g_glctx.rc) { wglMakeCurrent(nullptr, nullptr); wglDeleteContext(dummy); }
        else { wglMakeCurrent(nullptr, nullptr); wglDeleteContext(dummy); return false; }
    } else {
        g_glctx.rc = dummy;
    }
    return wglMakeCurrent(g_glctx.dc, g_glctx.rc) != FALSE;
#else
    (void)core; // GLX 这条路径本来就是兼容上下文，按实际版本分档即可。
    g_glctx.dpy = XOpenDisplay(nullptr);
    g_glctx.win = (::XWindow)(uintptr_t)win.hwnd();
    if (!g_glctx.dpy || !g_glctx.win) return false;
    int attrs[] = { GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None };
    XVisualInfo* vi = glXChooseVisual(g_glctx.dpy, DefaultScreen(g_glctx.dpy), attrs);
    if (!vi) return false;
    g_glctx.ctx = glXCreateContext(g_glctx.dpy, vi, nullptr, True);
    XFree(vi);
    return g_glctx.ctx && glXMakeCurrent(g_glctx.dpy, g_glctx.win, g_glctx.ctx);
#endif
}

gfxgl::Proc (*GetContextProc()) (const char*) {
#ifdef _WIN32
    return (gfxgl::Proc (*)(const char*))wglGetProcAddress;
#else
    return (gfxgl::Proc (*)(const char*))glXGetProcAddress;
#endif
}

} // namespace

uint32_t& GfxSkyTriangle() { return g_skyTriangle; }

bool gfxGLInit(VkCtx& ctx, Window& win, int w, int h, bool forceLegacy) {
    // 先试 3.3 完整档；驱动太老或显式要求时退到 2.1 兼容档。
    for (int attempt = 0; attempt < 2; attempt++) {
        bool core = !forceLegacy && attempt == 0;
        if (!CreateGlContext(win, core)) { DestroyGlContext(); continue; }
        if (!gfxgl::LoadFunctions(GetContextProc(), !core)) { DestroyGlContext(); continue; }
        int major = 0, minor = 0;
        if (!QueryVersion(major, minor)) { DestroyGlContext(); continue; }
        if (core && !AtLeast(major, minor, 3, 3)) { DestroyGlContext(); continue; }
        if (!core && !AtLeast(major, minor, 2, 1)) { DestroyGlContext(); continue; }
        g_glctx.legacy = !core;
        g_glLegacy = !core;
        break;
    }
    if (!gfxgl::getProc) {
        fprintf(stderr, "[gl] 找不到可用的 OpenGL 上下文（3.3 / 2.1 均失败）\n");
        return false;
    }
    if (!g_glctx.legacy) {
        gfxgl::GenVertexArrays(1, &g_glctx.vao);
        gfxgl::BindVertexArray(g_glctx.vao);
    } else {
        const float tri[6] = {0.0f, 0.0f, 2.0f, 0.0f, 0.0f, 2.0f};
        gfxgl::GenBuffers(1, &g_skyTriangle);
        gfxgl::BindBuffer(gfxgl::kArrayBuffer, g_skyTriangle);
        gfxgl::BufferData(gfxgl::kArrayBuffer, sizeof(tri), tri, gfxgl::kStaticDraw);
    }
    gfxgl::DepthFunc(gfxgl::kLessEqual);
    // 3.3 档由 GL 做 sRGB 编码；2.1 档在着色器里手写 pow(1/2.2)。
    if (!g_glctx.legacy) gfxgl::Enable(gfxgl::kFramebufferSrgb);
    gfxgl::PixelStorei(gfxgl::kPixelUnpackAlignment, 1);
    gfxgl::PixelStorei(gfxgl::kPackAlignment, 1);
    SetVsync(ctx.vsync);

    g_gfxUseGL = true;
    g_gfxBackend = GfxBackend::OpenGL;
    ctx.useGL = true;
    ctx.ok = true;
    ctx.extent = { (uint32_t)w, (uint32_t)h };
    ctx.swapFormat = VK_FORMAT_R8G8B8A8_SRGB;
    ctx.frames.resize(VkCtx::MAX_FRAMES_IN_FLIGHT);
    ctx.cmds.assign(VkCtx::MAX_FRAMES_IN_FLIGHT, (VkCommandBuffer)(uintptr_t)0x100);
    // Renderer 仍按 Vulkan 的流程用 imageIndex 下标访问这些数组；GL 路径不使用句柄，
    // 但数组必须有一项，否则截图/渲染 pass 会越界。
    ctx.framebuffers.assign(1, (VkFramebuffer)(uintptr_t)0x100);
    ctx.swapImages.assign(1, (VkImage)(uintptr_t)0x100);
    ctx.swapViews.assign(1, (VkImageView)(uintptr_t)0x100);
    g_glctx.valid = true;
    printf("[gl] 使用 OpenGL %s 回退后端（Vulkan 不可用）\n", g_glctx.legacy ? "2.1 老设备档" : "3.3 完整档");
    return true;
}

void gfxGLSwap(VkCtx& ctx) {
    // ANGLE 上下文不走 WGL，不能用 g_glctx.valid 判断，否则 eglSwapBuffers 永远不会被调用。
    if (g_gfxUseEGL) { gfxAngleSwap(); return; }
    if (!g_glctx.valid) return;
#ifdef _WIN32
    SwapBuffers(g_glctx.dc);
#else
    glXSwapBuffers(g_glctx.dpy, g_glctx.win);
#endif
    (void)ctx;
}

void gfxGLShutdown(VkCtx& ctx) {
    if (!g_gfxUseGL) return;
    for (auto& kv : g_buffers) {
        BufferRec& b = kv.second;
        if (b.gl) gfxgl::DeleteBuffers(1, &b.gl);
        if (b.legacyGl) gfxgl::DeleteBuffers(1, &b.legacyGl);
        if (b.legacyIndexGl) gfxgl::DeleteBuffers(1, &b.legacyIndexGl);
    }
    if (g_skyTriangle) gfxgl::DeleteBuffers(1, &g_skyTriangle);
    for (auto& kv : g_images) if (kv.second.gl) gfxgl::DeleteTextures(1, &kv.second.gl);
    for (auto& kv : g_pipelines) if (kv.second.program) gfxgl::DeleteProgram(kv.second.program);
    g_buffers.clear(); g_mems.clear(); g_images.clear(); g_samplers.clear();
    g_sets.clear(); g_pipelines.clear(); g_views.clear();
    if (g_glctx.vao) gfxgl::DeleteVertexArrays(1, &g_glctx.vao);
    if (g_gfxUseEGL) gfxAngleShutdown();
    else DestroyGlContext();
    g_gfxUseGL = false;
    g_gfxUseEGL = false;
    g_glLegacy = false;
    g_gfxUseGLES = false;
    ctx.useGL = false;
    ctx.ok = false;
}

// ---------------------------------------------------------------------------
// gfx* 派发：Vulkan 原样转发，OpenGL 分支实现相同语义。
// ---------------------------------------------------------------------------
VkResult gfxCreateImage(VkDevice d, const VkImageCreateInfo* ci, const VkAllocationCallbacks* a, VkImage* out) {
    ImageRec rec;
    rec.w = (int)ci->extent.width;
    rec.h = (int)ci->extent.height;
    rec.mips = ci->mipLevels ? ci->mipLevels : 1;
    rec.srgb = ci->format == VK_FORMAT_R8G8B8A8_SRGB || ci->format == VK_FORMAT_B8G8R8A8_SRGB;
    gfxgl::GenTextures(1, &rec.gl);
    gfxgl::BindTexture(gfxgl::kTexture2D, rec.gl);
    // 必须先用 glTexImage2D 分配存储；只有 glTexSubImage2D 会因纹理不完整而上传失败。
    // 老设备档用普通 RGBA8，sRGB 转换在 GLSL 120 里手写。
    int internalFormat = (rec.srgb && !g_glLegacy) ? gfxgl::kSrgb8Alpha8 : gfxgl::kRgba8;
    gfxgl::TexImage2D(gfxgl::kTexture2D, 0, internalFormat,
                      rec.w, rec.h, 0, gfxgl::kRgba, gfxgl::kUnsignedByte, nullptr);
    gfxgl::TexParameteri(gfxgl::kTexture2D, gfxgl::kTextureMaxLevel, (int)rec.mips - 1);
    *out = (VkImage)(uintptr_t)NextHandle();
    g_images[(uint64_t)*out] = rec;
    return VK_SUCCESS;
}
void gfxDestroyBuffer(VkDevice d, VkBuffer b, const VkAllocationCallbacks* a) {
    BufferRec* r = Lookup(g_buffers, (uint64_t)b);
    if (r) {
        if (r->gl) gfxgl::DeleteBuffers(1, &r->gl);
        if (r->legacyGl) gfxgl::DeleteBuffers(1, &r->legacyGl);
        if (r->legacyIndexGl) gfxgl::DeleteBuffers(1, &r->legacyIndexGl);
    }
    g_buffers.erase((uint64_t)b);
}
void gfxDestroyImage(VkDevice d, VkImage i, const VkAllocationCallbacks* a) {
    ImageRec* r = Lookup(g_images, (uint64_t)i);
    if (r && r->gl) gfxgl::DeleteTextures(1, &r->gl);
    g_images.erase((uint64_t)i);
}
void gfxGetImageMemoryRequirements(VkDevice d, VkImage i, VkMemoryRequirements* mr) {
    ImageRec* r = Lookup(g_images, (uint64_t)i);
    size_t bytes = r ? (size_t)r->w * r->h * 4 : 4;
    *mr = {};
    mr->size = bytes;
    mr->alignment = 4;
}
VkResult gfxAllocateMemory(VkDevice d, const VkMemoryAllocateInfo* ai, const VkAllocationCallbacks* a, VkDeviceMemory* out) {
    MemRec rec;
    rec.size = (size_t)ai->allocationSize;
    *out = (VkDeviceMemory)(uintptr_t)NextHandle();
    g_mems[(uint64_t)*out] = rec;
    return VK_SUCCESS;
}
void gfxFreeMemory(VkDevice d, VkDeviceMemory m, const VkAllocationCallbacks* a) {
    g_mems.erase((uint64_t)m);
}
VkResult gfxBindImageMemory(VkDevice d, VkImage i, VkDeviceMemory m, VkDeviceSize off) {
    MemRec* mr = Lookup(g_mems, (uint64_t)m);
    if (mr) mr->image = i;
    return VK_SUCCESS;
}
VkResult gfxMapMemory(VkDevice d, VkDeviceMemory m, VkDeviceSize off, VkDeviceSize size, VkMemoryMapFlags flags, void** out) {
    MemRec* mr = Lookup(g_mems, (uint64_t)m);
    if (!mr) return VK_ERROR_MEMORY_MAP_FAILED;
    // 缓冲的 CPU 镜像只有一份：缓冲记录和它的内存记录共享同一块数据，
    // 否则上传/读回会与 vkMapMemory 拿到的指针不一致。
    if (mr->buffer) {
        BufferRec* br = Lookup(g_buffers, (uint64_t)mr->buffer);
        if (br && br->cpu.size() >= mr->size) {
            *out = br->cpu.data() + off;
            return VK_SUCCESS;
        }
    }
    size_t need = (size_t)((size == VK_WHOLE_SIZE) ? mr->size : (off + size));
    if (mr->cpu.size() < need) mr->cpu.resize(need);
    *out = mr->cpu.data() + off;
    return VK_SUCCESS;
}
void gfxUnmapMemory(VkDevice d, VkDeviceMemory m) {
}

VkResult gfxCreateDescriptorSetLayout(VkDevice d, const VkDescriptorSetLayoutCreateInfo* ci, const VkAllocationCallbacks* a, VkDescriptorSetLayout* out) {
    *out = (VkDescriptorSetLayout)(uintptr_t)NextHandle(); return VK_SUCCESS;
}
void gfxDestroyDescriptorSetLayout(VkDevice d, VkDescriptorSetLayout l, const VkAllocationCallbacks* a) {
}
VkResult gfxCreateDescriptorPool(VkDevice d, const VkDescriptorPoolCreateInfo* ci, const VkAllocationCallbacks* a, VkDescriptorPool* out) {
    *out = (VkDescriptorPool)(uintptr_t)NextHandle(); return VK_SUCCESS;
}
void gfxDestroyDescriptorPool(VkDevice d, VkDescriptorPool p, const VkAllocationCallbacks* a) {
}
VkResult gfxAllocateDescriptorSets(VkDevice d, const VkDescriptorSetAllocateInfo* ai, VkDescriptorSet* out) {
    for (uint32_t i = 0; i < ai->descriptorSetCount; i++) {
        out[i] = (VkDescriptorSet)(uintptr_t)NextHandle();
        g_sets[(uint64_t)out[i]] = DescSetRec{};
    }
    return VK_SUCCESS;
}
void gfxUpdateDescriptorSets(VkDevice d, uint32_t n, const VkWriteDescriptorSet* w, uint32_t c, const VkCopyDescriptorSet* cp) {
    for (uint32_t i = 0; i < n; i++) {
        DescSetRec* ds = Lookup(g_sets, (uint64_t)w[i].dstSet);
        if (!ds) continue;
        if (w[i].descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER && w[i].pBufferInfo)
            ds->buffers[w[i].dstBinding] = w[i].pBufferInfo->buffer;
        if (w[i].descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER && w[i].pImageInfo) {
            ds->images[w[i].dstBinding] = (uint64_t)w[i].pImageInfo->imageView;
            ds->samplers[w[i].dstBinding] = w[i].pImageInfo->sampler;
        }
    }
}
VkResult gfxCreatePipelineLayout(VkDevice d, const VkPipelineLayoutCreateInfo* ci, const VkAllocationCallbacks* a, VkPipelineLayout* out) {
    *out = (VkPipelineLayout)(uintptr_t)NextHandle(); return VK_SUCCESS;
}
void gfxDestroyPipelineLayout(VkDevice d, VkPipelineLayout l, const VkAllocationCallbacks* a) {
}
void gfxDestroyPipeline(VkDevice d, VkPipeline p, const VkAllocationCallbacks* a) {
    PipelineRec* r = Lookup(g_pipelines, (uint64_t)p);
    if (r && r->program) gfxgl::DeleteProgram(r->program);
    g_pipelines.erase((uint64_t)p);
}

VkResult gfxCreateSampler(VkDevice d, const VkSamplerCreateInfo* ci, const VkAllocationCallbacks* a, VkSampler* out) {
    SamplerRec rec;
    rec.nearest = ci->minFilter == VK_FILTER_NEAREST && ci->magFilter == VK_FILTER_NEAREST;
    rec.mip = ci->maxLod > 0.5f && ci->mipmapMode != VK_SAMPLER_MIPMAP_MODE_NEAREST;
    rec.magLinear = ci->magFilter == VK_FILTER_LINEAR;
    *out = (VkSampler)(uintptr_t)NextHandle();
    g_samplers[(uint64_t)*out] = rec;
    return VK_SUCCESS;
}
void gfxDestroySampler(VkDevice d, VkSampler s, const VkAllocationCallbacks* a) {
    g_samplers.erase((uint64_t)s);
}
VkResult gfxCreateImageView(VkDevice d, const VkImageViewCreateInfo* ci, const VkAllocationCallbacks* a, VkImageView* out) {
    ImageRec* r = Lookup(g_images, (uint64_t)ci->image);
    *out = (VkImageView)(uintptr_t)NextHandle();
    g_views[(uint64_t)*out] = ViewRec{ r ? r->gl : 0 };
    return VK_SUCCESS;
}
void gfxDestroyImageView(VkDevice d, VkImageView v, const VkAllocationCallbacks* a) {
    g_views.erase((uint64_t)v);
}

VkResult gfxResetCommandBuffer(VkCommandBuffer cb, VkCommandBufferResetFlags f) {
    return g_gfxUseGL ? VK_SUCCESS : vkResetCommandBuffer(cb, f);
}
VkResult gfxBeginCommandBuffer(VkCommandBuffer cb, const VkCommandBufferBeginInfo* bi) {
    return g_gfxUseGL ? VK_SUCCESS : vkBeginCommandBuffer(cb, bi);
}
VkResult gfxEndCommandBuffer(VkCommandBuffer cb) {
    return g_gfxUseGL ? VK_SUCCESS : vkEndCommandBuffer(cb);
}
void gfxCmdPipelineBarrier(VkCommandBuffer cb, VkPipelineStageFlags s1, VkPipelineStageFlags s2,
                           VkDependencyFlags dep, uint32_t a, const VkMemoryBarrier* mb,
                           uint32_t b, const VkBufferMemoryBarrier* bb,
                           uint32_t c, const VkImageMemoryBarrier* ib) {
}
void gfxCmdCopyBufferToImage(VkCommandBuffer cb, VkBuffer src, VkImage img, VkImageLayout,
                             uint32_t n, const VkBufferImageCopy* copies) {
    BufferRec* br = Lookup(g_buffers, (uint64_t)src);
    ImageRec* ir = Lookup(g_images, (uint64_t)img);
    if (!br || !ir || !ir->gl) return;
    gfxgl::BindTexture(gfxgl::kTexture2D, ir->gl);
    size_t offset = 0;
    for (uint32_t i = 0; i < n; i++) {
        int w = (int)copies[i].imageExtent.width, h = (int)copies[i].imageExtent.height;
        gfxgl::TexSubImage2D(gfxgl::kTexture2D, (int)copies[i].imageSubresource.mipLevel,
                             (int)copies[i].imageOffset.x, (int)copies[i].imageOffset.y,
                             w, h, gfxgl::kRgba, gfxgl::kUnsignedByte, br->cpu.data() + offset);
        offset += (size_t)w * h * 4;
    }
    if (ir->mips > 1) gfxgl::GenerateMipmap(gfxgl::kTexture2D);
}
void gfxCmdCopyImageToBuffer(VkCommandBuffer cb, VkImage img, VkImageLayout layout, VkBuffer dst,
                             uint32_t n, const VkBufferImageCopy* copies) {
    BufferRec* br = Lookup(g_buffers, (uint64_t)dst);
    if (!br || !n) return;
    int w = (int)copies[0].imageExtent.width, h = (int)copies[0].imageExtent.height;
    // glReadPixels 自下而上，Vulkan 的 copyImageToBuffer 自上而下；翻过来才能
    // 让 savePNG 得到与 Vulkan 相同方向的截图。
    std::vector<uint8_t> tmp((size_t)w * h * 4);
    gfxgl::ReadPixels(0, 0, w, h, gfxgl::kBgra, gfxgl::kUnsignedByte, tmp.data());
    for (int y = 0; y < h; y++)
        memcpy(br->cpu.data() + (size_t)y * w * 4, tmp.data() + (size_t)(h - 1 - y) * w * 4, (size_t)w * 4);
}
void gfxCmdBlitImage(VkCommandBuffer cb, VkImage src, VkImageLayout srcLayout, VkImage dst, VkImageLayout dstLayout,
                     uint32_t n, const VkImageBlit* blits, VkFilter f) {
}
void gfxCmdBeginRenderPass(VkCommandBuffer cb, const VkRenderPassBeginInfo* rp, VkSubpassContents sc) {
    const VkClearValue* cv = rp->pClearValues;
    if (rp->clearValueCount > 0) {
        gfxgl::ClearColor(cv[0].color.float32[0], cv[0].color.float32[1],
                          cv[0].color.float32[2], cv[0].color.float32[3]);
    }
    if (rp->clearValueCount > 1) gfxgl::ClearDepth(cv[1].depthStencil.depth);
    // glClear 的深度写入受 DepthMask 影响，上一帧若以 UI/水收尾会留下 false。
    gfxgl::DepthMask(gfxgl::kTrue);
    gfxgl::Clear(gfxgl::kColorBufferBit | gfxgl::kDepthBufferBit);
}
void gfxCmdEndRenderPass(VkCommandBuffer cb) {
}
void gfxCmdSetViewport(VkCommandBuffer cb, uint32_t f, uint32_t n, const VkViewport* vp) {
    gfxgl::Viewport((int)vp->x, (int)vp->y, (int)vp->width, (int)vp->height);
}
void gfxCmdSetScissor(VkCommandBuffer cb, uint32_t f, uint32_t n, const VkRect2D* r) {
    gfxgl::Scissor(r->offset.x, r->offset.y, (int)r->extent.width, (int)r->extent.height);
}
void gfxCmdBindPipeline(VkCommandBuffer cb, VkPipelineBindPoint bp, VkPipeline p) {
    g_boundPipe = p;
    PipelineRec* pr = Lookup(g_pipelines, (uint64_t)p);
    if (!pr) return;
    gfxgl::UseProgram(pr->program);
    if (pr->blend) { gfxgl::Enable(gfxgl::kBlend); gfxgl::BlendFunc(gfxgl::kSrcAlpha, gfxgl::kOneMinusSrcAlpha); }
    else gfxgl::Disable(gfxgl::kBlend);
    if (pr->depthTest) { gfxgl::Enable(gfxgl::kDepthTest); gfxgl::DepthMask(pr->depthWrite ? gfxgl::kTrue : gfxgl::kFalse); }
    else { gfxgl::Disable(gfxgl::kDepthTest); gfxgl::DepthMask(gfxgl::kFalse); }
    // 着色器里已经翻转 gl_Position.y 以对齐 Vulkan 的朝下 Y 轴，绕序保持 CCW。
    if (pr->cull) { gfxgl::Enable(gfxgl::kCullFace); gfxgl::CullFace(gfxgl::kBack); gfxgl::FrontFace(gfxgl::kCcw); }
    else gfxgl::Disable(gfxgl::kCullFace);
}
void gfxCmdBindDescriptorSets(VkCommandBuffer cb, VkPipelineBindPoint bp, VkPipelineLayout l,
                              uint32_t f, uint32_t n, const VkDescriptorSet* s, uint32_t d, const uint32_t* o) {
    if (n) g_boundSet = s[0];
    g_boundLayout = l;
}
void gfxCmdBindVertexBuffers(VkCommandBuffer cb, uint32_t f, uint32_t n, const VkBuffer* b, const VkDeviceSize* o) {
    if (n) { g_boundVB = b[0]; UploadBuffer(b[0]); }
}
void gfxCmdBindIndexBuffer(VkCommandBuffer cb, VkBuffer b, VkDeviceSize off, VkIndexType t) {
    g_boundIB = b;
    g_indexType = t;
    g_indexOffset = off;
    BufferRec* br = Lookup(g_buffers, (uint64_t)b);
    if (!br) return;
    if (g_glLegacy) {
        g_legacyVertexBytes = (size_t)off;
        EnsureLegacyFormat(b, g_legacyVertexBytes);
        g_indexOffset = 0;
        gfxgl::BindBuffer(gfxgl::kElementBuffer, br->legacyIndexGl ? br->legacyIndexGl : br->gl);
    } else {
        gfxgl::BindBuffer(gfxgl::kElementBuffer, br->gl);
    }
}
void gfxCmdPushConstants(VkCommandBuffer cb, VkPipelineLayout l, VkShaderStageFlags st,
                         uint32_t off, uint32_t size, const void* data) {
    memcpy(g_origin, data, std::min<size_t>(size, sizeof(g_origin)));
}
void gfxCmdDraw(VkCommandBuffer cb, uint32_t vc, uint32_t ic, uint32_t vf, uint32_t fi) {
    PipelineRec* pr = Lookup(g_pipelines, (uint64_t)g_boundPipe);
    if (!pr) return;
    PrepareDraw(g_boundPipe, g_boundSet);
    BufferRec* br = Lookup(g_buffers, (uint64_t)g_boundVB);
    if (g_glLegacy) {
        if (pr->vertexKind == 0) {
            // GLSL 120 没有 gl_VertexID，用 CPU 生成的三个顶点画全屏三角形。
            gfxgl::DisableVertexAttribArray(1);
            gfxgl::DisableVertexAttribArray(2);
            gfxgl::BindBuffer(gfxgl::kArrayBuffer, g_skyTriangle);
            gfxgl::VertexAttribPointer(0, 2, gfxgl::kFloat, gfxgl::kFalse, 8, (const void*)0);
            gfxgl::EnableVertexAttribArray(0);
            gfxgl::DrawArrays(gfxgl::kTriangles, 0, 3);
            return;
        }
        if (pr->vertexKind == 1 && br) {
            EnsureLegacyFormat(g_boundVB, (size_t)(fi + vc) * 12);
            gfxgl::BindBuffer(gfxgl::kArrayBuffer, br->legacyGl ? br->legacyGl : br->gl);
            gfxgl::VertexAttribPointer(0, 4, gfxgl::kFloat, gfxgl::kFalse, 32, (const void*)0);
            gfxgl::VertexAttribPointer(1, 4, gfxgl::kFloat, gfxgl::kFalse, 32, (const void*)16);
            gfxgl::DisableVertexAttribArray(2);
            gfxgl::EnableVertexAttribArray(0);
            gfxgl::EnableVertexAttribArray(1);
            gfxgl::DrawArrays(gfxgl::kTriangles, (int)fi, (int)vc);
            return;
        }
    }
    if (!pr->vertexKind) {
        // 天空用 gl_VertexID 生成三角形，必须关掉上一轮残留的属性数组。
        for (int i = 0; i < 3; i++) gfxgl::DisableVertexAttribArray(i);
    }
    if (pr->vertexKind && br) {
        gfxgl::BindBuffer(gfxgl::kArrayBuffer, br->gl);
        if (pr->vertexKind == 1) {
            // 与 Vulkan 顶点输入一致：inPos 是 4×int16，inMeta 是 4×uint8，
            // 共 12 字节；GL 的整数属性会自动扩展到 ivec4/uvec4。
            gfxgl::VertexAttribIPointer(0, 4, gfxgl::kShort, 12, (const void*)0);
            gfxgl::VertexAttribIPointer(1, 4, gfxgl::kUnsignedByte, 12, (const void*)8);
        } else {
            gfxgl::VertexAttribPointer(0, 2, gfxgl::kFloat, gfxgl::kFalse, 32, (const void*)0);
            gfxgl::VertexAttribPointer(1, 2, gfxgl::kFloat, gfxgl::kFalse, 32, (const void*)8);
            gfxgl::VertexAttribPointer(2, 4, gfxgl::kFloat, gfxgl::kFalse, 32, (const void*)16);
        }
        for (int i = 0; i < (pr->vertexKind == 1 ? 2 : 3); i++) gfxgl::EnableVertexAttribArray(i);
    }
    gfxgl::DrawArrays(gfxgl::kTriangles, (int)fi, (int)vc);
}
void gfxCmdDrawIndexed(VkCommandBuffer cb, uint32_t ic, uint32_t inst, uint32_t fi, int32_t vi, uint32_t fInst) {
    PipelineRec* pr = Lookup(g_pipelines, (uint64_t)g_boundPipe);
    if (!pr) return;
    PrepareDraw(g_boundPipe, g_boundSet);
    BufferRec* br = Lookup(g_buffers, (uint64_t)g_boundVB);
    if (g_glLegacy && br) {
        EnsureLegacyFormat(g_boundVB, g_legacyVertexBytes);
        gfxgl::BindBuffer(gfxgl::kArrayBuffer, br->legacyGl ? br->legacyGl : br->gl);
        gfxgl::VertexAttribPointer(0, 4, gfxgl::kFloat, gfxgl::kFalse, 32, (const void*)0);
        gfxgl::VertexAttribPointer(1, 4, gfxgl::kFloat, gfxgl::kFalse, 32, (const void*)16);
        gfxgl::EnableVertexAttribArray(0);
        gfxgl::EnableVertexAttribArray(1);
    } else if (br) {
        gfxgl::BindBuffer(gfxgl::kArrayBuffer, br->gl);
        gfxgl::VertexAttribIPointer(0, 4, gfxgl::kShort, 12, (const void*)0);
        gfxgl::VertexAttribIPointer(1, 4, gfxgl::kUnsignedByte, 12, (const void*)8);
        gfxgl::EnableVertexAttribArray(0);
        gfxgl::EnableVertexAttribArray(1);
    }
    size_t stride = g_indexType == VK_INDEX_TYPE_UINT16 ? 2 : 4;
    gfxgl::DrawElements(gfxgl::kTriangles, (int)ic,
                        g_indexType == VK_INDEX_TYPE_UINT16 ? 0x1403 : gfxgl::kUnsignedInt,
                        (const void*)(uintptr_t)(g_indexOffset + fi * stride));
}
VkResult gfxQueueSubmit(VkQueue q, uint32_t n, const VkSubmitInfo* si, VkFence f) {
    gfxgl::Flush();
    return VK_SUCCESS;
}
VkResult gfxQueueWaitIdle(VkQueue q) {
    gfxgl::Finish();
    return VK_SUCCESS;
}
VkResult gfxWaitForFences(VkDevice d, uint32_t n, const VkFence* f, VkBool32 wait, uint64_t t) {
    return VK_SUCCESS;
}
void gfxDeviceWaitIdle(VkDevice d) {
    gfxgl::Finish();
}
void gfxGetPhysicalDeviceFeatures(VkPhysicalDevice p, VkPhysicalDeviceFeatures* f) {
    if (f) *f = {};
}

// gfxutil：Vulkan 分支完全等价于 vkutil，GL 分支用最小 GL 资源实现。
namespace gfxutil {
std::vector<char> readFile(const std::string& path) { return vkutil::readFile(path); }

bool createBuffer(VkCtx& ctx, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, Buffer2& out) {
    BufferRec rec;
    rec.size = (size_t)size;
    rec.cpu.assign(rec.size, 0);
    gfxgl::GenBuffers(1, &rec.gl);
    gfxgl::BindBuffer(gfxgl::kArrayBuffer, rec.gl);
    gfxgl::BufferData(gfxgl::kArrayBuffer, (gfxgl::Sizeiptr)rec.size, nullptr, gfxgl::kDynamicDraw);
    out.b = (VkBuffer)(uintptr_t)NextHandle();
    out.m = (VkDeviceMemory)(uintptr_t)NextHandle();
    MemRec mem;
    mem.size = rec.size;
    mem.buffer = out.b;
    mem.cpu.resize(rec.size);
    g_buffers[(uint64_t)out.b] = rec;
    g_mems[(uint64_t)out.m] = mem;
    return true;
}
bool createImage(VkCtx& ctx, uint32_t w, uint32_t h, VkFormat fmt, VkImageUsageFlags usage,
                 VkMemoryPropertyFlags props, VkImage& image, VkDeviceMemory& mem) {
    VkImageCreateInfo ci = {};
    ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ci.extent = { w, h, 1 };
    ci.format = fmt;
    ci.mipLevels = 1;
    if (gfxgl::gfxCreateImage(ctx.device, &ci, nullptr, &image) != VK_SUCCESS) return false;
    VkMemoryRequirements mr = {};
    gfxgl::gfxGetImageMemoryRequirements(ctx.device, image, &mr);
    VkMemoryAllocateInfo ai = {};
    ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize = mr.size;
    if (gfxgl::gfxAllocateMemory(ctx.device, &ai, nullptr, &mem) != VK_SUCCESS) return false;
    gfxgl::gfxBindImageMemory(ctx.device, image, mem, 0);
    return true;
}
VkImageView create2DView(VkDevice dev, VkImage image, VkFormat format) {
    VkImageViewCreateInfo ci = {};
    ci.image = image;
    ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    VkImageView out;
    gfxgl::gfxCreateImageView(VK_NULL_HANDLE, &ci, nullptr, &out);
    return out;
}
VkPipeline makePipeline(VkCtx& ctx, VkRenderPass rp, VkPipelineLayout layout,
                        const std::string& shaderDir, const char* vsName, const char* fsName,
                        const VkVertexInputBindingDescription* bindings, const VkVertexInputAttributeDescription* attrs,
                        uint32_t bindingCount, uint32_t attrCount, VkPrimitiveTopology topo, VkCullModeFlags cull,
                        bool blend, bool depthTest, bool depthWrite) {
    std::string path = GlShaderPath(shaderDir, vsName);
    std::string vsSrc = ReadText(path);
    if (vsSrc.empty()) { fprintf(stderr, "[gl] 找不到着色器 %s\n", path.c_str()); return VK_NULL_HANDLE; }
    std::string fsSrc = ReadText(GlShaderPath(shaderDir, fsName));
    if (fsSrc.empty()) { fprintf(stderr, "[gl] 找不到着色器 %s\n", fsName); return VK_NULL_HANDLE; }
    uint32_t v = ::CompileShader(gfxgl::kVertShader, vsSrc, vsName);
    uint32_t f = ::CompileShader(gfxgl::kFragShader, fsSrc, fsName);
    if (!v || !f) { if (v) gfxgl::DeleteShader(v); if (f) gfxgl::DeleteShader(f); return VK_NULL_HANDLE; }
    uint32_t p = gfxgl::CreateProgram();
    gfxgl::AttachShader(p, v);
    gfxgl::AttachShader(p, f);
    gfxgl::LinkProgram(p);
    int ok = 0;
    gfxgl::GetProgramiv(p, gfxgl::kLinkStatus, &ok);
    if (!ok) {
        char log[2048] = {};
        gfxgl::GetProgramInfoLog(p, sizeof(log), nullptr, log);
        fprintf(stderr, "[gl] link failed: %s\n", log);
        gfxgl::DeleteProgram(p);
        return VK_NULL_HANDLE;
    }
    gfxgl::DeleteShader(v);
    gfxgl::DeleteShader(f);
    PipelineRec rec;
    rec.program = p;
    rec.vertexKind = ClassifyVertex(vsName);
    rec.blend = blend;
    rec.depthTest = depthTest;
    rec.depthWrite = depthWrite;
    rec.cull = cull != VK_CULL_MODE_NONE;
    VkPipeline out = (VkPipeline)(uintptr_t)NextHandle();
    g_pipelines[(uint64_t)out] = rec;
    return out;
}
} // namespace gfxutil
} // namespace gfxgl

bool gfxGLInit(VkCtx& ctx, Window& win, int w, int h, bool forceLegacy) { return gfxgl::gfxGLInit(ctx, win, w, h, forceLegacy); }
void gfxGLSwap(VkCtx& ctx) { gfxgl::gfxGLSwap(ctx); }
void gfxGLShutdown(VkCtx& ctx) { gfxgl::gfxGLShutdown(ctx); }
uint32_t& GfxSkyTriangle() { return gfxgl::GfxSkyTriangle(); }
