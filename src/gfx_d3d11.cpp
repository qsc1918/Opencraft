// 独立 D3D11 后端：不经过 Vulkan/GL/ANGLE，直接把 gfx* 语义映射到 D3D11。
// 运行库按需加载，显式 --renderer d3d11 才初始化，其他档位的启动路径不受影响。
#define GFX_SHIM_IMPL
#include "gfx_shim.hpp"
#include "vk.hpp"
#include "window.hpp"
#include "platform.hpp"
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <d3dcompiler.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef _MSC_VER
#define WINAPI_CC __stdcall
#else
#define WINAPI_CC __attribute__((stdcall))
#endif

namespace {

using CreateFactoryFn = HRESULT (WINAPI_CC *)(REFIID, void**);
using CreateDeviceSwapFn = HRESULT (WINAPI_CC *)(IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE, UINT,
    const D3D_FEATURE_LEVEL*, UINT, UINT, const DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**,
    ID3D11Device**, D3D_FEATURE_LEVEL*, ID3D11DeviceContext**);
using CompileFn = HRESULT (WINAPI_CC *)(LPCVOID, SIZE_T, LPCSTR, const D3D_SHADER_MACRO*,
    ID3DInclude*, LPCSTR, LPCSTR, UINT, UINT, ID3D10Blob**, ID3D10Blob**);

// 自己写 IID 避免链接 dxgi.uuid 库，进而避免 Vulkan 档被 D3D 导入表拖住。
const GUID kIidFactory1 = {0x770aae78, 0xf26f, 0x4dba, {0xa8,0x29,0x25,0x3c,0x83,0xd1,0xb3,0x87}};
const GUID kIidTexture2D = {0x6f15aaf2, 0xd208, 0x4e89, {0x9a,0xb4,0x48,0x95,0x35,0xd3,0x4f,0x9c}};

HMODULE g_d3d11Module = nullptr;
HMODULE g_dxgiModule = nullptr;
HMODULE g_compilerModule = nullptr;
CreateFactoryFn g_createFactory = nullptr;
CreateDeviceSwapFn g_createDeviceSwap = nullptr;
CompileFn g_compile = nullptr;

ID3D11Device* g_device = nullptr;
ID3D11DeviceContext* g_context = nullptr;
IDXGISwapChain* g_swap = nullptr;
ID3D11Texture2D* g_backbuffer = nullptr;
ID3D11RenderTargetView* g_rtv = nullptr;
ID3D11Texture2D* g_renderTex = nullptr;
ID3D11RenderTargetView* g_renderRtv = nullptr;
ID3D11Texture2D* g_depthTex = nullptr;
ID3D11DepthStencilView* g_dsv = nullptr;
ID3D11Texture2D* g_readback = nullptr;
ID3D11Buffer* g_originBuffer = nullptr;
ID3D11Buffer* g_uboSmall = nullptr;
ID3D11Buffer* g_uboTiles = nullptr;
VkCtx* g_ctx = nullptr;
D3D_FEATURE_LEVEL g_featureLevel = D3D_FEATURE_LEVEL_11_0;
bool g_initialized = false;
bool g_firstFrameDone = false;
uint64_t g_nextHandle = 0x1000;
VkImage g_swapImageHandle = VK_NULL_HANDLE;
VkPipeline g_boundPipeline = VK_NULL_HANDLE;
VkDescriptorSet g_boundSet = VK_NULL_HANDLE;
ID3D11Buffer* g_boundUBO = nullptr;
VkBuffer g_boundVBHandle = VK_NULL_HANDLE;

uint64_t NextHandle() { return g_nextHandle++; }

template<typename T> T* Find(std::unordered_map<uint64_t, T>& map, uint64_t key) {
    auto it = map.find(key);
    return it == map.end() ? nullptr : &it->second;
}

struct BufferRec {
    size_t size = 0;
    std::vector<uint8_t> cpu;
    ID3D11Buffer* gpu = nullptr;
    VkBufferUsageFlags usage = 0;
    bool uploaded = false;
    bool dynamic = false;
    ID3D11Buffer* floatGpu = nullptr;
    ID3D11Buffer* floatIndexGpu = nullptr;
    size_t vertexBytes = 0;
    bool converted = false;
    uint8_t signature[32] = {};
};
struct MemRec {
    size_t size = 0;
    VkBuffer buffer = VK_NULL_HANDLE;
    VkImage image = VK_NULL_HANDLE;
    std::vector<uint8_t> cpu;
};
struct ImageRec {
    int w = 0, h = 0;
    uint32_t mips = 1;
    bool srgb = false;
    DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
    ID3D11Texture2D* tex = nullptr;
    ID3D11ShaderResourceView* srv = nullptr;
    bool mipsGenerated = false;
};
struct SamplerRec { ID3D11SamplerState* state = nullptr; };
struct ViewRec { VkImage image = VK_NULL_HANDLE; };
struct DescSetRec {
    std::unordered_map<uint32_t, VkBuffer> buffers;
    std::unordered_map<uint32_t, VkImageView> images;
    std::unordered_map<uint32_t, VkSampler> samplers;
};
struct PipelineRec {
    ID3D11VertexShader* vs = nullptr;
    ID3D11PixelShader* ps = nullptr;
    ID3D11InputLayout* layout = nullptr;
    ID3D11BlendState* blend = nullptr;
    ID3D11DepthStencilState* depth = nullptr;
    ID3D11RasterizerState* raster = nullptr;
    int kind = 0; // 0=天空 1=整数顶点 2=UI
};

std::unordered_map<uint64_t, BufferRec> g_buffers;
std::unordered_map<uint64_t, MemRec> g_mems;
std::unordered_map<uint64_t, ImageRec> g_images;
std::unordered_map<uint64_t, SamplerRec> g_samplers;
std::unordered_map<uint64_t, DescSetRec> g_sets;
std::unordered_map<uint64_t, PipelineRec> g_pipelines;
std::unordered_map<uint64_t, ViewRec> g_views;
std::unordered_map<uint64_t, uint64_t> g_pipelineLayouts;

bool LoadProcs() {
    g_d3d11Module = LoadLibraryA("d3d11.dll");
    g_dxgiModule = LoadLibraryA("dxgi.dll");
    g_compilerModule = LoadLibraryA("d3dcompiler_47.dll");
    if (!g_d3d11Module || !g_dxgiModule || !g_compilerModule) {
        fprintf(stderr, "[d3d11] 系统缺少 d3d11.dll / dxgi.dll / d3dcompiler_47.dll\n");
        return false;
    }
    g_createFactory = (CreateFactoryFn)(void*)GetProcAddress(g_dxgiModule, "CreateDXGIFactory1");
    g_createDeviceSwap = (CreateDeviceSwapFn)(void*)GetProcAddress(g_d3d11Module, "D3D11CreateDeviceAndSwapChain");
    g_compile = (CompileFn)(void*)GetProcAddress(g_compilerModule, "D3DCompile");
    if (!g_createFactory || !g_createDeviceSwap || !g_compile) {
        fprintf(stderr, "[d3d11] 无法解析 D3D11 / DXGI / HLSL 编译入口\n");
        return false;
    }
    return true;
}

DXGI_FORMAT ToDxgi(VkFormat f, bool srgb) {
    if (f == VK_FORMAT_D32_SFLOAT) return DXGI_FORMAT_D32_FLOAT;
    if (srgb) return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    return DXGI_FORMAT_R8G8B8A8_UNORM;
}

void ReleaseSwapTargets() {
    if (g_context) g_context->OMSetRenderTargets(0, nullptr, nullptr);
    if (g_rtv) { g_rtv->Release(); g_rtv = nullptr; }
    if (g_renderRtv) { g_renderRtv->Release(); g_renderRtv = nullptr; }
    if (g_renderTex) { g_renderTex->Release(); g_renderTex = nullptr; }
    if (g_dsv) { g_dsv->Release(); g_dsv = nullptr; }
    if (g_depthTex) { g_depthTex->Release(); g_depthTex = nullptr; }
    if (g_backbuffer) { g_backbuffer->Release(); g_backbuffer = nullptr; }
    if (g_readback) { g_readback->Release(); g_readback = nullptr; }
}

bool RecreateTargets(int w, int h) {
    ReleaseSwapTargets();
    if (!g_swap) return false;
    if (FAILED(g_swap->ResizeBuffers(0, (UINT)w, (UINT)h, DXGI_FORMAT_UNKNOWN, 0))) return false;
    if (FAILED(g_swap->GetBuffer(0, kIidTexture2D, (void**)&g_backbuffer))) return false;
    if (FAILED(g_device->CreateRenderTargetView(g_backbuffer, nullptr, &g_rtv))) return false;

    // 渲染目标是离屏纹理：交换链后台缓冲在 Present 后内容未定义，直接拿它读回会得到全黑。
    D3D11_TEXTURE2D_DESC rtd = {};
    rtd.Width = (UINT)std::max(1, w);
    rtd.Height = (UINT)std::max(1, h);
    rtd.MipLevels = 1;
    rtd.ArraySize = 1;
    rtd.SampleDesc.Count = 1;
    rtd.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    rtd.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    rtd.Usage = D3D11_USAGE_DEFAULT;
    if (FAILED(g_device->CreateTexture2D(&rtd, nullptr, &g_renderTex))) return false;
    if (FAILED(g_device->CreateRenderTargetView(g_renderTex, nullptr, &g_renderRtv))) return false;

    D3D11_TEXTURE2D_DESC dd = {};
    dd.Width = (UINT)std::max(1, w);
    dd.Height = (UINT)std::max(1, h);
    dd.MipLevels = 1;
    dd.ArraySize = 1;
    dd.Format = DXGI_FORMAT_D32_FLOAT;
    dd.SampleDesc.Count = 1;
    dd.Usage = D3D11_USAGE_DEFAULT;
    dd.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    if (FAILED(g_device->CreateTexture2D(&dd, nullptr, &g_depthTex))) return false;
    if (FAILED(g_device->CreateDepthStencilView(g_depthTex, nullptr, &g_dsv))) return false;

    D3D11_TEXTURE2D_DESC rd = {};
    rd.Width = (UINT)std::max(1, w);
    rd.Height = (UINT)std::max(1, h);
    rd.MipLevels = 1;
    rd.ArraySize = 1;
    rd.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    rd.SampleDesc.Count = 1;
    rd.Usage = D3D11_USAGE_STAGING;
    rd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    if (FAILED(g_device->CreateTexture2D(&rd, nullptr, &g_readback))) return false;
    return true;
}

void UploadBuffer(VkBuffer handle) {
    BufferRec* b = Find(g_buffers, (uint64_t)handle);
    if (!b || !b->gpu || b->cpu.empty()) return;
    if (b->uploaded && !b->dynamic) return;
    g_context->UpdateSubresource(b->gpu, 0, nullptr, b->cpu.data(), (UINT)b->cpu.size(), 0);
    b->uploaded = true;
}

// D3D11 对整数顶点属性在部分驱动上表现不一致，这里按 GL 老设备档的做法转换成浮点属性。
void EnsureFloatFormat(BufferRec* br, size_t vertex_bytes) {
    if (!br || vertex_bytes == 0 || vertex_bytes > br->cpu.size() || vertex_bytes % 12 != 0) return;
    size_t sig_bytes = std::min<size_t>(32, br->cpu.size());
    if (br->converted && br->vertexBytes == vertex_bytes &&
        memcmp(br->cpu.data(), br->signature, sig_bytes) == 0) return;
    if (br->floatGpu) { br->floatGpu->Release(); br->floatGpu = nullptr; }
    if (br->floatIndexGpu) { br->floatIndexGpu->Release(); br->floatIndexGpu = nullptr; }
    size_t count = vertex_bytes / 12;
    std::vector<float> verts(count * 8);
    for (size_t i = 0; i < count; i++) {
        const uint8_t* src = br->cpu.data() + i * 12;
        int16_t x, y, z, w;
        memcpy(&x, src, 2); memcpy(&y, src + 2, 2);
        memcpy(&z, src + 4, 2); memcpy(&w, src + 6, 2);
        float* dst = verts.data() + i * 8;
        dst[0] = (float)x; dst[1] = (float)y; dst[2] = (float)z; dst[3] = (float)w;
        dst[4] = (float)src[8]; dst[5] = (float)src[9];
        dst[6] = (float)src[10]; dst[7] = (float)src[11];
    }
    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.ByteWidth = (UINT)std::max<size_t>(32, verts.size() * sizeof(float));
    D3D11_SUBRESOURCE_DATA sd = {};
    sd.pSysMem = verts.data();
    if (FAILED(g_device->CreateBuffer(&bd, &sd, &br->floatGpu))) return;
    size_t index_bytes = br->cpu.size() - vertex_bytes;
    if (index_bytes) {
        bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
        bd.ByteWidth = (UINT)index_bytes;
        sd.pSysMem = br->cpu.data() + vertex_bytes;
        if (FAILED(g_device->CreateBuffer(&bd, &sd, &br->floatIndexGpu))) {
            br->floatGpu->Release();
            br->floatGpu = nullptr;
            return;
        }
    }
    br->vertexBytes = vertex_bytes;
    br->converted = true;
    memset(br->signature, 0, sizeof(br->signature));
    memcpy(br->signature, br->cpu.data(), sig_bytes);
}

UINT BindFlags(VkBufferUsageFlags usage) {
    UINT f = 0;
    if (usage & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) f |= D3D11_BIND_VERTEX_BUFFER;
    if (usage & VK_BUFFER_USAGE_INDEX_BUFFER_BIT) f |= D3D11_BIND_INDEX_BUFFER;
    if (usage & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) f |= D3D11_BIND_CONSTANT_BUFFER;
    if (usage & VK_BUFFER_USAGE_TRANSFER_DST_BIT) f |= D3D11_BIND_SHADER_RESOURCE;
    return f;
}

std::string ShaderPath(const std::string& dir, const std::string& base, const char* suffix) {
    std::string p = dir + "/hlsl11/" + base + suffix + ".hlsl";
    if (!vkutil::readFile(p).empty()) return p;
    return std::string("shaders/hlsl11/") + base + suffix + ".hlsl";
}

ID3D10Blob* CompileShader(const std::string& path, const char* entry, const char* target) {
    auto src = vkutil::readFile(path);
    if (src.empty()) { fprintf(stderr, "[d3d11] 找不到着色器 %s\n", path.c_str()); return nullptr; }
    ID3D10Blob* code = nullptr;
    ID3D10Blob* error = nullptr;
    HRESULT hr = g_compile(src.data(), src.size(), path.c_str(), nullptr, nullptr,
                           entry, target, 0, 0, &code, &error);
    if (FAILED(hr)) {
        if (error) {
            fprintf(stderr, "[d3d11] %s 编译失败: %s\n", path.c_str(), (const char*)error->GetBufferPointer());
            error->Release();
        }
        if (code) code->Release();
        return nullptr;
    }
    if (error) {
        const char* msg = (const char*)error->GetBufferPointer();
        if (*msg) fprintf(stderr, "[d3d11] %s 警告: %s\n", path.c_str(), msg);
        error->Release();
    }
    return code;
}

int ClassifyShader(const std::string& base) {
    if (base.find("terrain") != std::string::npos || base.find("water") != std::string::npos ||
        base.find("entity") != std::string::npos) return 1;
    if (base.find("ui") != std::string::npos || base.find("menu") != std::string::npos) return 2;
    return 0;
}

void ApplyStates(const PipelineRec& p) {
    g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    g_context->IASetInputLayout(p.layout);
    g_context->VSSetShader(p.vs, nullptr, 0);
    g_context->PSSetShader(p.ps, nullptr, 0);
    g_context->OMSetBlendState(p.blend, nullptr, 0xffffffff);
    g_context->OMSetDepthStencilState(p.depth, 0);
    g_context->RSSetState(p.raster);
}

void ApplyDescriptorSet() {
    PipelineRec* pipe = Find(g_pipelines, (uint64_t)g_boundPipeline);
    DescSetRec* set = Find(g_sets, (uint64_t)g_boundSet);
    if (!pipe || !set) return;
    ID3D11Buffer* cb = nullptr;
    auto b0 = set->buffers.find(0);
    if (b0 != set->buffers.end()) {
        UploadBuffer(b0->second);
        BufferRec* br = Find(g_buffers, (uint64_t)b0->second);
        cb = br ? br->gpu : nullptr;
    }
    g_boundUBO = cb;
    BufferRec* ubr = Find(g_buffers, (uint64_t)(b0 == set->buffers.end() ? 0 : b0->second));
    ID3D11Buffer* vsCb = g_uboSmall;
    if (ubr && ubr->cpu.size() >= 112) g_context->UpdateSubresource(vsCb, 0, nullptr, ubr->cpu.data(), 0, 0);
    g_context->VSSetConstantBuffers(0, 1, &vsCb);
    ID3D11Buffer* tilesCb = g_uboTiles;
    if (ubr && ubr->cpu.size() >= 2192) g_context->UpdateSubresource(tilesCb, 0, nullptr, ubr->cpu.data() + 144, 2048, 0);
    g_context->VSSetConstantBuffers(1, 1, &tilesCb);
    g_context->PSSetConstantBuffers(0, 1, &cb);
    g_context->PSSetConstantBuffers(1, 1, &cb);
    if (pipe->kind != 0) {
        ID3D11Buffer* origin = g_originBuffer;
        g_context->VSSetConstantBuffers(2, 1, &origin);
    }

    ID3D11ShaderResourceView* srvs[2] = {};
    ID3D11SamplerState* samplers[2] = {};
    int count = pipe->kind == 1 ? 2 : (pipe->kind == 2 ? 1 : 0);
    int baseBinding = pipe->kind == 1 ? 1 : 0;
    for (int i = 0; i < count; i++) {
        auto im = set->images.find((uint32_t)(baseBinding + i));
        ViewRec* view = im == set->images.end() ? nullptr : Find(g_views, (uint64_t)im->second);
        ImageRec* image = view ? Find(g_images, (uint64_t)view->image) : nullptr;
        srvs[i] = image ? image->srv : nullptr;
        auto sm = set->samplers.find((uint32_t)(baseBinding + i));
        SamplerRec* sampler = sm == set->samplers.end() ? nullptr : Find(g_samplers, (uint64_t)sm->second);
        samplers[i] = sampler ? sampler->state : nullptr;
    }
    g_context->PSSetShaderResources(0, (UINT)count, srvs);
    g_context->PSSetSamplers(0, (UINT)count, samplers);
}

bool InitTargets() {
    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth = 16;
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    const float zero[4] = {};
    D3D11_SUBRESOURCE_DATA sd = {};
    sd.pSysMem = zero;
    if (FAILED(g_device->CreateBuffer(&bd, &sd, &g_originBuffer))) return false;
    D3D11_BUFFER_DESC ub = {};
    ub.Usage = D3D11_USAGE_DEFAULT;
    ub.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    ub.ByteWidth = 112;
    if (FAILED(g_device->CreateBuffer(&ub, nullptr, &g_uboSmall))) return false;
    ub.ByteWidth = 2048;
    if (FAILED(g_device->CreateBuffer(&ub, nullptr, &g_uboTiles))) return false;
    return RecreateTargets((int)g_ctx->extent.width, (int)g_ctx->extent.height);
}

bool SetupContext(VkCtx& ctx, Window& win, int w, int h, IDXGIAdapter* adapter) {
    DXGI_SWAP_CHAIN_DESC scd = {};
    scd.BufferCount = 2;
    scd.BufferDesc.Width = (UINT)w;
    scd.BufferDesc.Height = (UINT)h;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    scd.BufferDesc.RefreshRate.Numerator = 0;
    scd.BufferDesc.RefreshRate.Denominator = 1;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = (HWND)win.hwnd();
    scd.SampleDesc.Count = 1;
    scd.Windowed = TRUE;
    scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    scd.Flags = 0;

    D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0};
    HRESULT hr = g_createDeviceSwap(adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0,
                                    levels, 3, D3D11_SDK_VERSION, &scd,
                                    &g_swap, &g_device, &g_featureLevel, &g_context);
    if (FAILED(hr)) {
        fprintf(stderr, "[d3d11] 设备/交换链创建失败: 0x%x\n", (unsigned)hr);
        return false;
    }
    g_ctx = &ctx;
    ctx.extent = {(uint32_t)w, (uint32_t)h};
    if (!InitTargets()) return false;
    ctx.useGL = true;
    ctx.ok = true;
    ctx.frames.resize(VkCtx::MAX_FRAMES_IN_FLIGHT);
    ctx.cmds.assign(VkCtx::MAX_FRAMES_IN_FLIGHT, (VkCommandBuffer)(uintptr_t)0x100);
    ctx.framebuffers.assign(1, (VkFramebuffer)(uintptr_t)0x100);
    g_swapImageHandle = (VkImage)NextHandle();
    ctx.swapImages.assign(1, g_swapImageHandle);
    ctx.swapViews.assign(1, (VkImageView)(uintptr_t)0x100);
    g_gfxUseGL = true;
    g_gfxBackend = GfxBackend::D3D11;
    printf("[d3d11] 使用独立 D3D11 后端（%s）\n",
           g_featureLevel >= D3D_FEATURE_LEVEL_11_0 ? "SM5.0" : "SM4.0");
    return true;
}

// gfx* 实现：签名与 gfx_shim.hpp 一一对应，函数名首字母大写供派发表注册。
VkResult CreateImage(VkDevice, const VkImageCreateInfo* ci, const VkAllocationCallbacks*, VkImage* out) {
    ImageRec rec;
    rec.w = (int)ci->extent.width;
    rec.h = (int)ci->extent.height;
    rec.mips = ci->mipLevels ? ci->mipLevels : 1;
    // D3D11 不像 Vulkan 那样容忍超出最大层数的 mipCount，必须夹紧到 1+floor(log2(max(w,h)))。
    unsigned maxMips = 1;
    while ((1u << maxMips) <= (unsigned)std::max(rec.w, rec.h)) maxMips++;
    rec.mips = std::min(rec.mips, maxMips);
    rec.srgb = ci->format == VK_FORMAT_R8G8B8A8_SRGB || ci->format == VK_FORMAT_B8G8R8A8_SRGB;
    rec.format = ToDxgi(ci->format, rec.srgb);
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = (UINT)rec.w;
    td.Height = (UINT)rec.h;
    td.MipLevels = rec.mips;
    td.ArraySize = 1;
    td.Format = rec.format;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    HRESULT hr = g_device->CreateTexture2D(&td, nullptr, &rec.tex);
    if (FAILED(hr)) { fprintf(stderr, "[d3d11] 纹理创建失败: 0x%x (%dx%d mips=%u fmt=%d)\n", (unsigned)hr, rec.w, rec.h, rec.mips, (int)rec.format); return VK_ERROR_OUT_OF_HOST_MEMORY; }
    D3D11_SHADER_RESOURCE_VIEW_DESC vd = {};
    vd.Format = rec.format;
    vd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    vd.Texture2D.MipLevels = rec.mips;
    if (FAILED(g_device->CreateShaderResourceView(rec.tex, &vd, &rec.srv))) {
        rec.tex->Release();
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    *out = (VkImage)NextHandle();
    g_images[(uint64_t)*out] = rec;
    return VK_SUCCESS;
}
void DestroyBuffer(VkDevice, VkBuffer b, const VkAllocationCallbacks*) {
    BufferRec* r = Find(g_buffers, (uint64_t)b);
    if (r && r->gpu) r->gpu->Release();
    g_buffers.erase((uint64_t)b);
}
void DestroyImage(VkDevice, VkImage i, const VkAllocationCallbacks*) {
    ImageRec* r = Find(g_images, (uint64_t)i);
    if (r) {
        if (r->srv) r->srv->Release();
        if (r->tex) r->tex->Release();
    }
    g_images.erase((uint64_t)i);
}
void GetImageMemoryRequirements(VkDevice, VkImage i, VkMemoryRequirements* mr) {
    ImageRec* r = Find(g_images, (uint64_t)i);
    *mr = {};
    mr->size = r ? (size_t)r->w * r->h * 4 : 4;
    mr->alignment = 4;
}
VkResult AllocateMemory(VkDevice, const VkMemoryAllocateInfo* ai, const VkAllocationCallbacks*, VkDeviceMemory* out) {
    MemRec rec;
    rec.size = (size_t)ai->allocationSize;
    *out = (VkDeviceMemory)NextHandle();
    g_mems[(uint64_t)*out] = rec;
    return VK_SUCCESS;
}
void FreeMemory(VkDevice, VkDeviceMemory m, const VkAllocationCallbacks*) { g_mems.erase((uint64_t)m); }
VkResult BindImageMemory(VkDevice, VkImage i, VkDeviceMemory m, VkDeviceSize) {
    MemRec* mr = Find(g_mems, (uint64_t)m);
    if (mr) mr->image = i;
    return VK_SUCCESS;
}
VkResult MapMemory(VkDevice, VkDeviceMemory m, VkDeviceSize off, VkDeviceSize size, VkMemoryMapFlags, void** out) {
    MemRec* mr = Find(g_mems, (uint64_t)m);
    if (!mr) return VK_ERROR_MEMORY_MAP_FAILED;
    if (mr->buffer) {
        BufferRec* br = Find(g_buffers, (uint64_t)mr->buffer);
        if (br && br->cpu.size() >= off) { *out = br->cpu.data() + off; return VK_SUCCESS; }
    }
    size_t need = (size_t)((size == VK_WHOLE_SIZE) ? mr->size : (off + size));
    if (mr->cpu.size() < need) mr->cpu.resize(need);
    *out = need ? mr->cpu.data() + off : nullptr;
    return VK_SUCCESS;
}
void UnmapMemory(VkDevice, VkDeviceMemory m) {
    MemRec* mr = Find(g_mems, (uint64_t)m);
    if (mr && mr->buffer) {
        BufferRec* br = Find(g_buffers, (uint64_t)mr->buffer);
        if (br) br->uploaded = false;
    }
}
VkResult CreateDescriptorSetLayout(VkDevice, const VkDescriptorSetLayoutCreateInfo*, const VkAllocationCallbacks*, VkDescriptorSetLayout* out) {
    *out = (VkDescriptorSetLayout)NextHandle(); return VK_SUCCESS;
}
void DestroyDescriptorSetLayout(VkDevice, VkDescriptorSetLayout, const VkAllocationCallbacks*) {}
VkResult CreateDescriptorPool(VkDevice, const VkDescriptorPoolCreateInfo*, const VkAllocationCallbacks*, VkDescriptorPool* out) {
    *out = (VkDescriptorPool)NextHandle(); return VK_SUCCESS;
}
void DestroyDescriptorPool(VkDevice, VkDescriptorPool, const VkAllocationCallbacks*) {}
VkResult AllocateDescriptorSets(VkDevice, const VkDescriptorSetAllocateInfo* ai, VkDescriptorSet* out) {
    for (uint32_t i = 0; i < ai->descriptorSetCount; i++) out[i] = (VkDescriptorSet)NextHandle();
    g_sets[(uint64_t)out[0]] = DescSetRec();
    return VK_SUCCESS;
}
void UpdateDescriptorSets(VkDevice, uint32_t n, const VkWriteDescriptorSet* writes, uint32_t, const VkCopyDescriptorSet*) {
    for (uint32_t i = 0; i < n; i++) {
        const VkWriteDescriptorSet& w = writes[i];
        DescSetRec* set = Find(g_sets, (uint64_t)w.dstSet);
        if (!set) continue;
        if (w.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER && w.pBufferInfo)
            set->buffers[w.dstBinding] = w.pBufferInfo->buffer;
        if ((w.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
             w.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE) && w.pImageInfo) {
            set->images[w.dstBinding] = w.pImageInfo->imageView;
            set->samplers[w.dstBinding] = w.pImageInfo->sampler;
        }
    }
}
VkResult CreatePipelineLayout(VkDevice, const VkPipelineLayoutCreateInfo*, const VkAllocationCallbacks*, VkPipelineLayout* out) {
    *out = (VkPipelineLayout)NextHandle(); return VK_SUCCESS;
}
void DestroyPipelineLayout(VkDevice, VkPipelineLayout, const VkAllocationCallbacks*) {}
void DestroyPipeline(VkDevice, VkPipeline p, const VkAllocationCallbacks*) {
    PipelineRec* r = Find(g_pipelines, (uint64_t)p);
    if (r) {
        if (r->vs) r->vs->Release();
        if (r->ps) r->ps->Release();
        if (r->layout) r->layout->Release();
        if (r->blend) r->blend->Release();
        if (r->depth) r->depth->Release();
        if (r->raster) r->raster->Release();
    }
    g_pipelines.erase((uint64_t)p);
}
VkResult CreateSampler(VkDevice, const VkSamplerCreateInfo* ci, const VkAllocationCallbacks*, VkSampler* out) {
    D3D11_SAMPLER_DESC sd = {};
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.Filter = ci->magFilter == VK_FILTER_NEAREST ? D3D11_FILTER_MIN_MAG_MIP_POINT : D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    if (ci->magFilter == VK_FILTER_NEAREST && ci->minFilter == VK_FILTER_LINEAR)
        sd.Filter = D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT;
    if (ci->minFilter == VK_FILTER_NEAREST && ci->mipmapMode == VK_SAMPLER_MIPMAP_MODE_LINEAR)
        sd.Filter = D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR;
    sd.MaxAnisotropy = ci->anisotropyEnable ? (UINT)std::min(16.0f, ci->maxAnisotropy) : 1;
    if (sd.MaxAnisotropy > 1) sd.Filter = D3D11_FILTER_ANISOTROPIC;
    sd.MinLOD = ci->minLod;
    sd.MaxLOD = ci->maxLod;
    sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
    SamplerRec rec;
    if (FAILED(g_device->CreateSamplerState(&sd, &rec.state))) return VK_ERROR_OUT_OF_HOST_MEMORY;
    *out = (VkSampler)NextHandle();
    g_samplers[(uint64_t)*out] = rec;
    return VK_SUCCESS;
}
void DestroySampler(VkDevice, VkSampler s, const VkAllocationCallbacks*) {
    SamplerRec* r = Find(g_samplers, (uint64_t)s);
    if (r && r->state) r->state->Release();
    g_samplers.erase((uint64_t)s);
}
VkResult CreateImageView(VkDevice, const VkImageViewCreateInfo* ci, const VkAllocationCallbacks*, VkImageView* out) {
    ViewRec rec;
    rec.image = ci->image;
    *out = (VkImageView)NextHandle();
    g_views[(uint64_t)*out] = rec;
    return VK_SUCCESS;
}
void DestroyImageView(VkDevice, VkImageView v, const VkAllocationCallbacks*) { g_views.erase((uint64_t)v); }
VkResult ResetCommandBuffer(VkCommandBuffer, VkCommandBufferResetFlags) { return VK_SUCCESS; }
VkResult BeginCommandBuffer(VkCommandBuffer, const VkCommandBufferBeginInfo*) { g_firstFrameDone = true; return VK_SUCCESS; }
VkResult EndCommandBuffer(VkCommandBuffer) { return VK_SUCCESS; }
void CmdPipelineBarrier(VkCommandBuffer, VkPipelineStageFlags, VkPipelineStageFlags, VkDependencyFlags,
                        uint32_t, const VkMemoryBarrier*, uint32_t, const VkBufferMemoryBarrier*,
                        uint32_t, const VkImageMemoryBarrier*) {}
void CmdCopyBufferToImage(VkCommandBuffer, VkBuffer src, VkImage img, VkImageLayout,
                          uint32_t n, const VkBufferImageCopy* copies) {
    BufferRec* br = Find(g_buffers, (uint64_t)src);
    ImageRec* ir = Find(g_images, (uint64_t)img);
    if (!br || !ir || !ir->tex) return;
    size_t offset = 0;
    for (uint32_t i = 0; i < n; i++) {
        UINT w = copies[i].imageExtent.width, h = copies[i].imageExtent.height;
        D3D11_BOX box = {};
        box.left = copies[i].imageOffset.x;
        box.top = copies[i].imageOffset.y;
        box.front = 0;
        box.right = box.left + w;
        box.bottom = box.top + h;
        box.back = 1;
        g_context->UpdateSubresource(ir->tex, copies[i].imageSubresource.mipLevel, &box,
                                     br->cpu.data() + offset, w * 4, (size_t)w * h * 4);
        offset += (size_t)w * h * 4;
    }
}
void CmdCopyImageToBuffer(VkCommandBuffer, VkImage img, VkImageLayout, VkBuffer dst,
                          uint32_t n, const VkBufferImageCopy* copies) {
    BufferRec* br = Find(g_buffers, (uint64_t)dst);
    if (!br || !n || !g_readback || img != g_swapImageHandle) return;
    int w = (int)copies[0].imageExtent.width, h = (int)copies[0].imageExtent.height;
    g_context->OMSetRenderTargets(0, nullptr, nullptr);
    g_context->CopyResource(g_readback, g_renderTex);
    D3D11_MAPPED_SUBRESOURCE map = {};
    if (FAILED(g_context->Map(g_readback, 0, D3D11_MAP_READ, 0, &map))) return;
    const uint8_t* src = (const uint8_t*)map.pData;
    for (int y = 0; y < h; y++) {
        const uint8_t* row = src + (size_t)y * map.RowPitch;
        uint8_t* dst = br->cpu.data() + (size_t)y * w * 4;
        for (int x = 0; x < w; x++) {
            dst[x * 4 + 0] = row[x * 4 + 2];
            dst[x * 4 + 1] = row[x * 4 + 1];
            dst[x * 4 + 2] = row[x * 4 + 0];
            dst[x * 4 + 3] = row[x * 4 + 3];
        }
    }
    g_context->Unmap(g_readback, 0);
}
void CmdBlitImage(VkCommandBuffer, VkImage src, VkImageLayout, VkImage dst, VkImageLayout,
                  uint32_t, const VkImageBlit*, VkFilter) {
    if (src != dst) return;
    ImageRec* ir = Find(g_images, (uint64_t)src);
    if (ir && ir->srv && ir->mips > 1 && !ir->mipsGenerated) {
        g_context->GenerateMips(ir->srv);
        ir->mipsGenerated = true;
    }
}
void CmdBeginRenderPass(VkCommandBuffer, const VkRenderPassBeginInfo* rp, VkSubpassContents) {
    if (!g_renderRtv || !g_dsv) return;
    const float* c = rp->clearValueCount > 0 ? rp->pClearValues[0].color.float32 : nullptr;
    float clear[4] = {0, 0, 0, 1};
    if (c) memcpy(clear, c, sizeof(clear));
    g_context->ClearRenderTargetView(g_renderRtv, clear);
    if (rp->clearValueCount > 1) g_context->ClearDepthStencilView(g_dsv, D3D11_CLEAR_DEPTH,
                                                                   rp->pClearValues[1].depthStencil.depth, 0);
    g_context->OMSetRenderTargets(1, &g_renderRtv, g_dsv);
}
void CmdEndRenderPass(VkCommandBuffer) {}
void CmdSetViewport(VkCommandBuffer, uint32_t, uint32_t, const VkViewport* vp) {
    D3D11_VIEWPORT d3dvp = {};
    d3dvp.TopLeftX = vp->x; d3dvp.TopLeftY = vp->y;
    d3dvp.Width = vp->width; d3dvp.Height = vp->height;
    d3dvp.MinDepth = vp->minDepth; d3dvp.MaxDepth = vp->maxDepth;
    g_context->RSSetViewports(1, &d3dvp);
}
void CmdSetScissor(VkCommandBuffer, uint32_t, uint32_t, const VkRect2D* r) {
    D3D11_RECT rect = {};
    rect.left = r->offset.x; rect.top = r->offset.y;
    rect.right = r->offset.x + (LONG)r->extent.width;
    rect.bottom = r->offset.y + (LONG)r->extent.height;
    g_context->RSSetScissorRects(1, &rect);
}
void CmdBindPipeline(VkCommandBuffer, VkPipelineBindPoint, VkPipeline p) {
    g_boundPipeline = p;
    PipelineRec* rec = Find(g_pipelines, (uint64_t)p);
    if (rec) ApplyStates(*rec);
}
void CmdBindDescriptorSets(VkCommandBuffer, VkPipelineBindPoint, VkPipelineLayout,
                           uint32_t, uint32_t, const VkDescriptorSet* sets, uint32_t, const uint32_t*) {
    if (!sets || !sets[0]) return;
    g_boundSet = sets[0];
    ApplyDescriptorSet();
}
void CmdBindVertexBuffers(VkCommandBuffer, uint32_t first, uint32_t n, const VkBuffer* bs, const VkDeviceSize* offs) {
    if (!n || !bs) return;
    g_boundVBHandle = bs[0];
    UploadBuffer(bs[0]);
    BufferRec* br = Find(g_buffers, (uint64_t)bs[0]);
    if (!br || !br->gpu) return;
    PipelineRec* pipe = Find(g_pipelines, (uint64_t)g_boundPipeline);
    UINT stride = pipe && pipe->kind == 2 ? 32 : 12;
    ID3D11Buffer* buf = br->gpu;
    if (pipe && pipe->kind == 1 && !(br->usage & VK_BUFFER_USAGE_INDEX_BUFFER_BIT)) {
        EnsureFloatFormat(br, br->size);
        if (br->floatGpu) { buf = br->floatGpu; stride = 32; }
    }
    UINT offset = offs ? (UINT)offs[0] : 0;
    g_context->IASetVertexBuffers(first, 1, &buf, &stride, &offset);
}
void CmdBindIndexBuffer(VkCommandBuffer, VkBuffer b, VkDeviceSize off, VkIndexType t) {
    UploadBuffer(b);
    BufferRec* br = Find(g_buffers, (uint64_t)b);
    if (!br || !br->gpu) return;
    if (br->usage & VK_BUFFER_USAGE_INDEX_BUFFER_BIT) {
        EnsureFloatFormat(br, (size_t)off);
        if (br->floatGpu) {
            BufferRec* vbr = Find(g_buffers, (uint64_t)g_boundVBHandle);
            if (vbr) {
                UINT stride = 32;
                UINT zero = 0;
                ID3D11Buffer* vb = vbr->floatGpu ? vbr->floatGpu : vbr->gpu;
                g_context->IASetVertexBuffers(0, 1, &vb, &stride, &zero);
            }
            if (br->floatIndexGpu) {
                g_context->IASetIndexBuffer(br->floatIndexGpu,
                    t == VK_INDEX_TYPE_UINT16 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT, 0);
                return;
            }
        }
    }
    g_context->IASetIndexBuffer(br->gpu,
        t == VK_INDEX_TYPE_UINT16 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT, (UINT)off);
}
void CmdPushConstants(VkCommandBuffer, VkPipelineLayout, VkShaderStageFlags, uint32_t, uint32_t size, const void* data) {
    if (!g_originBuffer || size < 16) return;
    g_context->UpdateSubresource(g_originBuffer, 0, nullptr, data, 16, 0);
}
void CmdDraw(VkCommandBuffer, uint32_t vc, uint32_t, uint32_t first, uint32_t) {
    g_context->Draw(vc, first);
}
void CmdDrawIndexed(VkCommandBuffer, uint32_t ic, uint32_t, uint32_t first, int32_t, uint32_t) {
    // MinGW 的 d3d11.h 把 DrawIndexed 写成 3 参数，直接调会破坏 stdcall 栈平衡。
    g_context->DrawIndexedInstanced(ic, 1, first, 0, 0);
}
VkResult QueueSubmit(VkQueue, uint32_t, const VkSubmitInfo*, VkFence) { return VK_SUCCESS; }
VkResult QueueWaitIdle(VkQueue) { if (g_context) g_context->Flush(); return VK_SUCCESS; }
VkResult WaitForFences(VkDevice, uint32_t, const VkFence*, VkBool32, uint64_t) { return VK_SUCCESS; }
void DeviceWaitIdle(VkDevice) { if (g_context) g_context->Flush(); }
void GetPhysicalDeviceFeatures(VkPhysicalDevice, VkPhysicalDeviceFeatures* f) { if (f) f->samplerAnisotropy = VK_TRUE; }

std::vector<char> ReadFileShim(const std::string& path) { return vkutil::readFile(path); }
bool CreateBuffer(VkCtx&, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags, Buffer2& out) {
    BufferRec rec;
    rec.size = (size_t)size;
    rec.cpu.assign(rec.size, 0);
    rec.usage = usage;
    rec.dynamic = (usage & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) != 0 || !g_firstFrameDone;
    UINT flags = BindFlags(usage);
    if (flags) {
        D3D11_BUFFER_DESC bd = {};
        bd.ByteWidth = (UINT)std::max<size_t>(16, rec.size);
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.BindFlags = flags;
        if (FAILED(g_device->CreateBuffer(&bd, nullptr, &rec.gpu))) return false;
    }
    out.b = (VkBuffer)NextHandle();
    out.m = (VkDeviceMemory)NextHandle();
    g_buffers[(uint64_t)out.b] = rec;
    MemRec mem;
    mem.size = rec.size;
    mem.buffer = out.b;
    g_mems[(uint64_t)out.m] = mem;
    return true;
}
bool CreateImageUtil(VkCtx& ctx, uint32_t w, uint32_t h, VkFormat fmt, VkImageUsageFlags,
                 VkMemoryPropertyFlags, VkImage& image, VkDeviceMemory& mem) {
    VkImageCreateInfo ci = {};
    ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ci.extent = {w, h, 1};
    ci.format = fmt;
    ci.mipLevels = 1;
    if (CreateImage(VK_NULL_HANDLE, &ci, nullptr, &image) != VK_SUCCESS) return false;
    VkMemoryRequirements mr = {};
    GetImageMemoryRequirements(VK_NULL_HANDLE, image, &mr);
    VkMemoryAllocateInfo ai = {};
    ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize = mr.size;
    if (AllocateMemory(VK_NULL_HANDLE, &ai, nullptr, &mem) != VK_SUCCESS) return false;
    BindImageMemory(VK_NULL_HANDLE, image, mem, 0);
    (void)ctx;
    return true;
}
VkImageView Create2DView(VkDevice, VkImage image, VkFormat) {
    VkImageViewCreateInfo ci = {};
    ci.image = image;
    ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    VkImageView out;
    CreateImageView(VK_NULL_HANDLE, &ci, nullptr, &out);
    return out;
}
VkPipeline MakePipeline(VkCtx&, VkRenderPass, VkPipelineLayout,
                        const std::string& shaderDir, const char* vsName, const char* fsName,
                        const VkVertexInputBindingDescription*, const VkVertexInputAttributeDescription*,
                        uint32_t, uint32_t, VkPrimitiveTopology, VkCullModeFlags cull,
                        bool blend, bool depthTest, bool depthWrite) {
    std::string vbase(vsName), fbase(fsName);
    for (auto* s : {&vbase, &fbase}) {
        size_t dot = s->find('.');
        if (dot != std::string::npos) s->resize(dot);
    }
    const char* model = g_featureLevel >= D3D_FEATURE_LEVEL_11_0 ? "_5_0" : "_4_0";
    std::string vst = std::string("vs") + model, pst = std::string("ps") + model;
    ID3D10Blob* vsBlob = CompileShader(ShaderPath(shaderDir, vbase, "_VS"), "VSMain", vst.c_str());
    ID3D10Blob* psBlob = CompileShader(ShaderPath(shaderDir, fbase, "_PS"), "PSMain", pst.c_str());
    if (!vsBlob || !psBlob) {
        if (vsBlob) vsBlob->Release();
        if (psBlob) psBlob->Release();
        return VK_NULL_HANDLE;
    }
    PipelineRec rec;
    rec.kind = ClassifyShader(vbase);
    if (FAILED(g_device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &rec.vs)) ||
        FAILED(g_device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &rec.ps))) {
        vsBlob->Release(); psBlob->Release();
        return VK_NULL_HANDLE;
    }
    if (rec.kind != 0) {
        D3D11_INPUT_ELEMENT_DESC elems[3];
        UINT count = 0;
        if (rec.kind == 1) {
            elems[0] = {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0};
            elems[1] = {"TEXCOORD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0};
            count = 2;
        } else {
            elems[0] = {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0};
            elems[1] = {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0};
            elems[2] = {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0};
            count = 3;
        }
        if (FAILED(g_device->CreateInputLayout(elems, count, vsBlob->GetBufferPointer(),
                                                vsBlob->GetBufferSize(), &rec.layout))) {
            vsBlob->Release(); psBlob->Release();
            return VK_NULL_HANDLE;
        }
    }
    vsBlob->Release();
    psBlob->Release();

    D3D11_BLEND_DESC bd = {};
    bd.RenderTarget[0].BlendEnable = blend ? TRUE : FALSE;
    bd.RenderTarget[0].SrcBlend = bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
    bd.RenderTarget[0].DestBlend = bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    bd.RenderTarget[0].BlendOp = bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    g_device->CreateBlendState(&bd, &rec.blend);

    D3D11_DEPTH_STENCIL_DESC dd = {};
    dd.DepthEnable = depthTest ? TRUE : FALSE;
    dd.DepthWriteMask = depthWrite ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
    dd.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    g_device->CreateDepthStencilState(&dd, &rec.depth);

    D3D11_RASTERIZER_DESC rd = {};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = cull == VK_CULL_MODE_NONE ? D3D11_CULL_NONE : D3D11_CULL_BACK;
    rd.FrontCounterClockwise = TRUE;
    rd.DepthClipEnable = TRUE;
    g_device->CreateRasterizerState(&rd, &rec.raster);

    VkPipeline out = (VkPipeline)NextHandle();
    g_pipelines[(uint64_t)out] = rec;
    return out;
}

void RegisterTable() {
    g_gfxD3D11Table.createImage = &CreateImage;
    g_gfxD3D11Table.destroyBuffer = &DestroyBuffer;
    g_gfxD3D11Table.destroyImage = &DestroyImage;
    g_gfxD3D11Table.getImageMemoryRequirements = &GetImageMemoryRequirements;
    g_gfxD3D11Table.allocateMemory = &AllocateMemory;
    g_gfxD3D11Table.freeMemory = &FreeMemory;
    g_gfxD3D11Table.bindImageMemory = &BindImageMemory;
    g_gfxD3D11Table.mapMemory = &MapMemory;
    g_gfxD3D11Table.unmapMemory = &UnmapMemory;
    g_gfxD3D11Table.createDescriptorSetLayout = &CreateDescriptorSetLayout;
    g_gfxD3D11Table.destroyDescriptorSetLayout = &DestroyDescriptorSetLayout;
    g_gfxD3D11Table.createDescriptorPool = &CreateDescriptorPool;
    g_gfxD3D11Table.destroyDescriptorPool = &DestroyDescriptorPool;
    g_gfxD3D11Table.allocateDescriptorSets = &AllocateDescriptorSets;
    g_gfxD3D11Table.updateDescriptorSets = &UpdateDescriptorSets;
    g_gfxD3D11Table.createPipelineLayout = &CreatePipelineLayout;
    g_gfxD3D11Table.destroyPipelineLayout = &DestroyPipelineLayout;
    g_gfxD3D11Table.destroyPipeline = &DestroyPipeline;
    g_gfxD3D11Table.createSampler = &CreateSampler;
    g_gfxD3D11Table.destroySampler = &DestroySampler;
    g_gfxD3D11Table.createImageView = &CreateImageView;
    g_gfxD3D11Table.destroyImageView = &DestroyImageView;
    g_gfxD3D11Table.resetCommandBuffer = &ResetCommandBuffer;
    g_gfxD3D11Table.beginCommandBuffer = &BeginCommandBuffer;
    g_gfxD3D11Table.endCommandBuffer = &EndCommandBuffer;
    g_gfxD3D11Table.cmdPipelineBarrier = &CmdPipelineBarrier;
    g_gfxD3D11Table.cmdCopyBufferToImage = &CmdCopyBufferToImage;
    g_gfxD3D11Table.cmdCopyImageToBuffer = &CmdCopyImageToBuffer;
    g_gfxD3D11Table.cmdBlitImage = &CmdBlitImage;
    g_gfxD3D11Table.cmdBeginRenderPass = &CmdBeginRenderPass;
    g_gfxD3D11Table.cmdEndRenderPass = &CmdEndRenderPass;
    g_gfxD3D11Table.cmdSetViewport = &CmdSetViewport;
    g_gfxD3D11Table.cmdSetScissor = &CmdSetScissor;
    g_gfxD3D11Table.cmdBindPipeline = &CmdBindPipeline;
    g_gfxD3D11Table.cmdBindDescriptorSets = &CmdBindDescriptorSets;
    g_gfxD3D11Table.cmdBindVertexBuffers = &CmdBindVertexBuffers;
    g_gfxD3D11Table.cmdBindIndexBuffer = &CmdBindIndexBuffer;
    g_gfxD3D11Table.cmdPushConstants = &CmdPushConstants;
    g_gfxD3D11Table.cmdDraw = &CmdDraw;
    g_gfxD3D11Table.cmdDrawIndexed = &CmdDrawIndexed;
    g_gfxD3D11Table.queueSubmit = &QueueSubmit;
    g_gfxD3D11Table.queueWaitIdle = &QueueWaitIdle;
    g_gfxD3D11Table.waitForFences = &WaitForFences;
    g_gfxD3D11Table.deviceWaitIdle = &DeviceWaitIdle;
    g_gfxD3D11Table.getPhysicalDeviceFeatures = &GetPhysicalDeviceFeatures;
    g_gfxD3D11Table.utilReadFile = &ReadFileShim;
    g_gfxD3D11Table.utilCreateBuffer = &CreateBuffer;
    g_gfxD3D11Table.utilCreateImage = &CreateImageUtil;
    g_gfxD3D11Table.utilCreate2DView = &Create2DView;
    g_gfxD3D11Table.utilMakePipeline = &MakePipeline;
}

} // namespace

bool gfxD3D11Init(VkCtx& ctx, Window& win, int w, int h) {
    // 原生 D3D11 后端尚未通过画面验证，常规路径先走 ANGLE 的 D3D11 上下文。
    // 设 OPENCRAFT_D3D11_NATIVE=1 才会尝试下面尚未完成的原生实现。
    if (!std::getenv("OPENCRAFT_D3D11_NATIVE")) {
        if (!gfxAngleInit(ctx, win, w, h)) return false;
        printf("[d3d11] 使用 ANGLE 的 D3D11 后端\n");
        return true;
    }
    if (!LoadProcs()) return false;
    IDXGIFactory1* factory = nullptr;
    if (FAILED(g_createFactory(kIidFactory1, (void**)&factory))) {
        fprintf(stderr, "[d3d11] CreateDXGIFactory1 失败\n");
        return false;
    }
    IDXGIAdapter1* chosen = nullptr;
    IDXGIAdapter1* adapter = nullptr;
    SIZE_T bestVram = 0;
    ctx.gpus.clear();
    for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; i++) {
        DXGI_ADAPTER_DESC1 desc = {};
        adapter->GetDesc1(&desc);
        GpuInfo info;
        info.name = "DXGI adapter";
        info.usable = !(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE);
        info.discrete = info.usable;
        info.vramMB = desc.DedicatedVideoMemory >> 20;
        ctx.gpus.push_back(info);
        bool want = ctx.desiredGpuIndex == (int)i ||
                    (ctx.desiredGpuIndex < 0 && info.usable && desc.DedicatedVideoMemory > bestVram);
        if (want) {
            if (chosen) chosen->Release();
            chosen = adapter;
            bestVram = desc.DedicatedVideoMemory;
            ctx.activeGpuIndex = (int)i;
        } else adapter->Release();
        adapter = nullptr;
    }
    if (!chosen && FAILED(factory->EnumAdapters1(0, (IDXGIAdapter1**)&chosen))) chosen = nullptr;
    factory->Release();
    if (!chosen) { fprintf(stderr, "[d3d11] 找不到可用显卡\n"); return false; }
    bool ok = SetupContext(ctx, win, w, h, chosen);
    chosen->Release();
    if (!ok) return false;
    RegisterTable();
    g_initialized = true;
    return true;
}

void gfxD3D11Resize(int w, int h) {
    if (g_initialized && g_ctx && (int)g_ctx->extent.width == w && (int)g_ctx->extent.height == h) return;
    if (g_initialized) RecreateTargets(w, h);
}

void gfxD3D11Swap() {
    if (!g_initialized || !g_swap || !g_renderTex) return;
    ID3D11Texture2D* back = nullptr;
    if (SUCCEEDED(g_swap->GetBuffer(0, kIidTexture2D, (void**)&back)) && back) {
        g_context->CopyResource(back, g_renderTex);
        back->Release();
    }
    g_swap->Present(g_ctx && g_ctx->vsync ? 1 : 0, 0);
}

void gfxD3D11Shutdown(VkCtx& ctx) {
    if (!g_initialized) return;
    for (auto& kv : g_buffers) if (kv.second.gpu) kv.second.gpu->Release();
    for (auto& kv : g_images) {
        if (kv.second.srv) kv.second.srv->Release();
        if (kv.second.tex) kv.second.tex->Release();
    }
    for (auto& kv : g_samplers) if (kv.second.state) kv.second.state->Release();
    for (auto& kv : g_pipelines) {
        PipelineRec& p = kv.second;
        if (p.vs) p.vs->Release();
        if (p.ps) p.ps->Release();
        if (p.layout) p.layout->Release();
        if (p.blend) p.blend->Release();
        if (p.depth) p.depth->Release();
        if (p.raster) p.raster->Release();
    }
    g_buffers.clear(); g_mems.clear(); g_images.clear(); g_samplers.clear();
    g_sets.clear(); g_pipelines.clear(); g_views.clear();
    for (auto& kv : g_buffers) {
        if (kv.second.floatGpu) kv.second.floatGpu->Release();
        if (kv.second.floatIndexGpu) kv.second.floatIndexGpu->Release();
    }
    if (g_originBuffer) g_originBuffer->Release();
    if (g_uboSmall) g_uboSmall->Release();
    if (g_uboTiles) g_uboTiles->Release();
    ReleaseSwapTargets();
    if (g_swap) g_swap->Release();
    if (g_context) g_context->Release();
    if (g_device) g_device->Release();
    if (g_d3d11Module) FreeLibrary(g_d3d11Module);
    if (g_dxgiModule) FreeLibrary(g_dxgiModule);
    if (g_compilerModule) FreeLibrary(g_compilerModule);
    g_device = nullptr; g_context = nullptr; g_swap = nullptr; g_originBuffer = nullptr;
    g_uboSmall = nullptr; g_uboTiles = nullptr;
    g_d3d11Module = g_dxgiModule = g_compilerModule = nullptr;
    g_initialized = false;
    ctx.ok = false;
    ctx.useGL = false;
    g_gfxUseGL = false;
    g_gfxBackend = GfxBackend::Vulkan;
}
