#pragma once
// vkutil.hpp — Vulkan 样板封装：着色器加载、缓冲/图像创建、图形管线
// renderer/menu/vk 三处原本各抄了一份，抽到这里共用。改管线默认值必须同步看
// shaders/ 与 PROJECT_STATE.md §4.11（sRGB 工作流）。
#include "vk.hpp"

#include <cstdio>
#include <string>
#include <vector>

// 缓冲 + 显存句柄对，destroy 后自动置空（重复 destroy 安全）。
// 兼容派发层的释放入口：Vulkan 时原样转发，OpenGL 回退时释放 GL 资源。
// 声明放在这里是因为 Buffer2::destroy 是内联的，GL 模式下绝不能调用空的 volk 指针。
void gfxDestroyBuffer(VkDevice device, VkBuffer buffer, const VkAllocationCallbacks* callbacks);
void gfxFreeMemory(VkDevice device, VkDeviceMemory memory, const VkAllocationCallbacks* callbacks);

struct Buffer2 {
    VkBuffer b = VK_NULL_HANDLE;
    VkDeviceMemory m = VK_NULL_HANDLE;
    void destroy(VkDevice d) {
        if (b) gfxDestroyBuffer(d, b, nullptr);
        if (m) gfxFreeMemory(d, m, nullptr);
        b = VK_NULL_HANDLE;
        m = VK_NULL_HANDLE;
    }
};

namespace vkutil {
inline std::vector<char> readFile(const std::string& path) {
    std::vector<char> data;
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return data;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return data; }
    fseek(f, 0, SEEK_SET);
    data.resize((size_t)sz);
    // fread 可能短读，必须按实际读到的字节数收缩，否则 codeSize 会大于缓冲区。
    size_t got = fread(data.data(), 1, (size_t)sz, f);
    fclose(f);
    data.resize(got);
    return data;
}

inline VkShaderModule loadShaderModule(VkDevice dev, const std::string& path) {
    auto data = readFile(path);
    if (data.empty()) return VK_NULL_HANDLE;
    VkShaderModuleCreateInfo ci = {};
    ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = data.size();
    ci.pCode = (const uint32_t*)data.data();
    VkShaderModule m;
    if (vkCreateShaderModule(dev, &ci, nullptr, &m) != VK_SUCCESS) return VK_NULL_HANDLE;
    return m;
}

inline bool createBuffer(VkCtx& ctx, VkDeviceSize size, VkBufferUsageFlags usage,
                           VkMemoryPropertyFlags props, Buffer2& out) {
    VkBufferCreateInfo ci = {};
    ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    ci.size = size;
    ci.usage = usage;
    ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(ctx.device, &ci, nullptr, &out.b) != VK_SUCCESS) return false;
    VkMemoryRequirements mr;
    vkGetBufferMemoryRequirements(ctx.device, out.b, &mr);
    uint32_t mt = ctx.findMemoryType(mr.memoryTypeBits, props);
    // DEVICE_LOCAL 在某些设备上不可单独满足，退回 host visible 至少能跑。
    if (mt == UINT32_MAX && (props & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
        mt = ctx.findMemoryType(mr.memoryTypeBits,
                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    }
    if (mt == UINT32_MAX) return false;
    VkMemoryAllocateInfo ai = {};
    ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize = mr.size;
    ai.memoryTypeIndex = mt;
    if (vkAllocateMemory(ctx.device, &ai, nullptr, &out.m) != VK_SUCCESS) return false;
    vkBindBufferMemory(ctx.device, out.b, out.m, 0);
    return true;
}

// 创建 2D 图像并绑定显存；失败时句柄保持 VK_NULL_HANDLE。
inline bool createImage(VkCtx& ctx, uint32_t w, uint32_t h, VkFormat format,
                          VkImageUsageFlags usage, VkMemoryPropertyFlags props,
                          VkImage& image, VkDeviceMemory& mem) {
    VkImageCreateInfo ci = {};
    ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ci.imageType = VK_IMAGE_TYPE_2D;
    ci.format = format;
    ci.extent = {w, h, 1};
    ci.mipLevels = 1;
    ci.arrayLayers = 1;
    ci.samples = VK_SAMPLE_COUNT_1_BIT;
    ci.tiling = VK_IMAGE_TILING_OPTIMAL;
    ci.usage = usage;
    if (vkCreateImage(ctx.device, &ci, nullptr, &image) != VK_SUCCESS) return false;
    VkMemoryRequirements mr;
    vkGetImageMemoryRequirements(ctx.device, image, &mr);
    uint32_t mt = ctx.findMemoryType(mr.memoryTypeBits, props);
    if (mt == UINT32_MAX) return false;
    VkMemoryAllocateInfo ai = {};
    ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize = mr.size;
    ai.memoryTypeIndex = mt;
    if (vkAllocateMemory(ctx.device, &ai, nullptr, &mem) != VK_SUCCESS) return false;
    vkBindImageMemory(ctx.device, image, mem, 0);
    return true;
}

// 2D 着色器图像视图（单 mip、单层）。
inline VkImageView create2DView(VkDevice dev, VkImage image, VkFormat format) {
    VkImageViewCreateInfo ci = {};
    ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    ci.image = image;
    ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ci.format = format;
    ci.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    VkImageView v;
    if (vkCreateImageView(dev, &ci, nullptr, &v) != VK_SUCCESS) return VK_NULL_HANDLE;
    return v;
}

// 图形管线。视口/裁剪设为动态，深度比较固定 LESS_OR_EQUAL（与深度附件一致），
// 混合/剔除/深度测试写入由调用方按需开关。
inline VkPipeline makePipeline(VkCtx& ctx, VkRenderPass rp, VkPipelineLayout layout,
                                 const std::string& shaderDir, const char* vsName, const char* fsName,
                                 const VkVertexInputBindingDescription* bindings,
                                 const VkVertexInputAttributeDescription* attrs,
                                 uint32_t bindingCount, uint32_t attrCount,
                                 VkPrimitiveTopology topo, VkCullModeFlags cull,
                                 bool blend, bool depthTest, bool depthWrite) {
    VkShaderModule vert = loadShaderModule(ctx.device, shaderDir + "/" + vsName);
    VkShaderModule frag = loadShaderModule(ctx.device, shaderDir + "/" + fsName);
    if (!vert || !frag) {
        if (vert) vkDestroyShaderModule(ctx.device, vert, nullptr);
        if (frag) vkDestroyShaderModule(ctx.device, frag, nullptr);
        return VK_NULL_HANDLE;
    }

    VkPipelineShaderStageCreateInfo stages[2] = {};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vert;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = frag;
    stages[1].pName = "main";

    VkPipelineVertexInputStateCreateInfo vi = {};
    vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vi.vertexBindingDescriptionCount = bindingCount;
    vi.pVertexBindingDescriptions = bindings;
    vi.vertexAttributeDescriptionCount = attrCount;
    vi.pVertexAttributeDescriptions = attrs;

    VkPipelineInputAssemblyStateCreateInfo ia = {};
    ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = topo;

    VkPipelineViewportStateCreateInfo vp = {};
    vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp.viewportCount = 1;
    vp.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rs = {};
    rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = cull;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms = {};
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo ds = {};
    ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.depthTestEnable = depthTest ? VK_TRUE : VK_FALSE;
    ds.depthWriteEnable = depthWrite ? VK_TRUE : VK_FALSE;
    ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

    VkPipelineColorBlendAttachmentState ba = {};
    ba.colorWriteMask = 0xF;
    if (blend) {
        ba.blendEnable = VK_TRUE;
        ba.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        ba.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        ba.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        ba.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    }
    VkPipelineColorBlendStateCreateInfo cb = {};
    cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.attachmentCount = 1;
    cb.pAttachments = &ba;

    VkDynamicState dyn[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynCI = {};
    dynCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynCI.dynamicStateCount = 2;
    dynCI.pDynamicStates = dyn;

    VkGraphicsPipelineCreateInfo pi = {};
    pi.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pi.stageCount = 2;
    pi.pStages = stages;
    pi.pVertexInputState = &vi;
    pi.pInputAssemblyState = &ia;
    pi.pViewportState = &vp;
    pi.pRasterizationState = &rs;
    pi.pMultisampleState = &ms;
    pi.pDepthStencilState = &ds;
    pi.pColorBlendState = &cb;
    pi.pDynamicState = &dynCI;
    pi.layout = layout;
    pi.renderPass = rp;
    pi.subpass = 0;

    VkPipeline p;
    VkResult r = vkCreateGraphicsPipelines(ctx.device, VK_NULL_HANDLE, 1, &pi, nullptr, &p);
    vkDestroyShaderModule(ctx.device, vert, nullptr);
    vkDestroyShaderModule(ctx.device, frag, nullptr);
    return r == VK_SUCCESS ? p : VK_NULL_HANDLE;
}

} // namespace vkutil
