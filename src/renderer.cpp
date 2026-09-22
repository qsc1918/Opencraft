#include "renderer.hpp"
#include "camera.hpp"
#include "entities.hpp"
#include "player.hpp"
#include "window.hpp"
#include "png.hpp"
#include "raycast.hpp"
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <tuple>
#include <vector>

// ---------------------------------------------------------------------------
// 辅助函数
// ---------------------------------------------------------------------------
static std::vector<char> readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    std::vector<char> data;
    if (!f) return data;
    std::streamsize size = f.tellg();
    f.seekg(0, std::ios::beg);
    data.resize((size_t)size);
    f.read(data.data(), size);
    return data;
}

static VkShaderModule loadModule(VkDevice dev, const std::string& path) {
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

static bool createBuffer(VkCtx& ctx, VkDeviceSize size, VkBufferUsageFlags usage,
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
    if (mt == UINT32_MAX) {
        if (props & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
            mt = ctx.findMemoryType(mr.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
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

static VkPipeline makePipeline(VkCtx& ctx, VkRenderPass rp, VkPipelineLayout layout,
                               const std::string& shaderDir, const char* vs, const char* fs,
                               VkVertexInputBindingDescription bindings[], VkVertexInputAttributeDescription attrs[],
                               uint32_t bindingCount, uint32_t attrCount,
                               VkPrimitiveTopology topo, VkCullModeFlags cull,
                               bool blend, bool depthTest, bool depthWrite) {
    auto vert = loadModule(ctx.device, shaderDir + "/" + vs);
    auto frag = loadModule(ctx.device, shaderDir + "/" + fs);
    if (!vert || !frag) return VK_NULL_HANDLE;

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

// ---------------------------------------------------------------------------
// 初始化 / 销毁
// ---------------------------------------------------------------------------
bool Renderer::init(VkCtx& ctx, Window& win, const std::string& assetDir,
                    const std::string& shaderDir) {
    ctxPtr_ = &ctx;
    windowW_ = win.width();
    windowH_ = win.height();

    atlas_ = buildAtlas(assetDir + "/block");
    if (!atlas_.built) { fprintf(stderr, "[renderer] atlas build failed\n"); return false; }
    createAtlasTexture(ctx);
    createDescriptors(ctx);

    // --- 管线布局 ---
    VkPushConstantRange terrPC = {VK_SHADER_STAGE_VERTEX_BIT, 0, 16};
    VkPipelineLayoutCreateInfo pl = {};
    pl.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pl.setLayoutCount = 1;
    pl.pSetLayouts = &terrainDSL_;
    pl.pushConstantRangeCount = 1;
    pl.pPushConstantRanges = &terrPC;
    vkCreatePipelineLayout(ctx.device, &pl, nullptr, &terrainLayout_);

    pl = {};
    pl.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pl.setLayoutCount = 1;
    pl.pSetLayouts = &uiDSL_;
    vkCreatePipelineLayout(ctx.device, &pl, nullptr, &uiLayout_);

    pl = {};
    pl.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pl.setLayoutCount = 1;
    pl.pSetLayouts = &skyDSL_;
    vkCreatePipelineLayout(ctx.device, &pl, nullptr, &skyLayout_);

    // --- 每帧缓冲（主机可见，仅映射一次） ---
    for (int i = 0; i < VkCtx::MAX_FRAMES_IN_FLIGHT; i++) {
        createBuffer(ctx, 256, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, terrainUBO_[i]);
        vkMapMemory(ctx.device, terrainUBO_[i].m, 0, 256, 0, &terrainUBOMap_[i]);
        createBuffer(ctx, 64, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, skyUBO_[i]);
        vkMapMemory(ctx.device, skyUBO_[i].m, 0, 64, 0, &skyUBOMap_[i]);
        createBuffer(ctx, 1 << 20, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uiBuf_[i]);
        vkMapMemory(ctx.device, uiBuf_[i].m, 0, VK_WHOLE_SIZE, 0, &uiMap_[i]);
    }

    // --- 描述符集 ---
    VkDescriptorImageInfo tii = {atlasSampler_, atlasView_, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkDescriptorImageInfo uii = {atlasSampler_, atlasView_, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    for (int i = 0; i < VkCtx::MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo tbi = {terrainUBO_[i].b, 0, 256};
        VkWriteDescriptorSet w1[2] = {};
        w1[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w1[0].dstSet = terrainSet_[i];
        w1[0].dstBinding = 0;
        w1[0].descriptorCount = 1;
        w1[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        w1[0].pBufferInfo = &tbi;
        w1[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w1[1].dstSet = terrainSet_[i];
        w1[1].dstBinding = 1;
        w1[1].descriptorCount = 1;
        w1[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w1[1].pImageInfo = &tii;
        vkUpdateDescriptorSets(ctx.device, 2, w1, 0, nullptr);

        VkDescriptorBufferInfo sbi = {skyUBO_[i].b, 0, 64};
        VkWriteDescriptorSet w2 = {};
        w2.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w2.dstSet = skySet_[i];
        w2.dstBinding = 0;
        w2.descriptorCount = 1;
        w2.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        w2.pBufferInfo = &sbi;
        vkUpdateDescriptorSets(ctx.device, 1, &w2, 0, nullptr);

        VkWriteDescriptorSet w3 = {};
        w3.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w3.dstSet = uiSet_[i];
        w3.dstBinding = 0;
        w3.descriptorCount = 1;
        w3.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w3.pImageInfo = &uii;
        vkUpdateDescriptorSets(ctx.device, 1, &w3, 0, nullptr);
    }

    // --- 管线 ---
    VkVertexInputBindingDescription terrBinding = {0, 12, VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription terrAttrs[2] = {};
    terrAttrs[0] = {0, 0, VK_FORMAT_R16G16B16A16_SINT, 0};
    terrAttrs[1] = {1, 0, VK_FORMAT_R8G8B8A8_UINT, 8};

    terrainPipe_ = makePipeline(ctx, ctx.renderPass, terrainLayout_, shaderDir,
                                "terrain.vert.spv", "terrain.frag.spv",
                                &terrBinding, terrAttrs, 1, 2,
                                VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_CULL_MODE_BACK_BIT,
                                false, true, true);
    waterPipe_ = makePipeline(ctx, ctx.renderPass, terrainLayout_, shaderDir,
                              "water.vert.spv", "water.frag.spv",
                              &terrBinding, terrAttrs, 1, 2,
                              VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_CULL_MODE_BACK_BIT,
                              true, true, false);

    skyPipe_ = makePipeline(ctx, ctx.renderPass, skyLayout_, shaderDir,
                            "sky.vert.spv", "sky.frag.spv",
                            nullptr, nullptr, 0, 0,
                            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_CULL_MODE_NONE,
                            false, false, false);

    VkVertexInputBindingDescription uiBinding = {0, 32, VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription uiAttrs[3] = {};
    uiAttrs[0] = {0, 0, VK_FORMAT_R32G32_SFLOAT, 0};
    uiAttrs[1] = {1, 0, VK_FORMAT_R32G32_SFLOAT, 8};
    uiAttrs[2] = {2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 16};
    uiPipe_ = makePipeline(ctx, ctx.renderPass, uiLayout_, shaderDir,
                           "ui.vert.spv", "ui.frag.spv",
                           &uiBinding, uiAttrs, 1, 3,
                           VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_CULL_MODE_NONE,
                           true, false, false);

    if (!terrainPipe_ || !waterPipe_ || !skyPipe_ || !uiPipe_) {
        fprintf(stderr, "[renderer] pipeline failure: %d %d %d %d\n",
                (int)(terrainPipe_ != VK_NULL_HANDLE), (int)(waterPipe_ != VK_NULL_HANDLE),
                (int)(skyPipe_ != VK_NULL_HANDLE), (int)(uiPipe_ != VK_NULL_HANDLE));
        return false;
    }
    // 实体管线：复用 terrain 布局与着色器，不剔面（盒模型内外面都可能可见）
    entityPipe_ = makePipeline(ctx, ctx.renderPass, terrainLayout_, shaderDir,
                               "terrain.vert.spv", "terrain.frag.spv",
                               &terrBinding, terrAttrs, 1, 2,
                               VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_CULL_MODE_NONE,
                               false, true, true);
    if (!entityPipe_) fprintf(stderr, "[renderer] WARNING: entity pipeline creation failed\n");

    // 实体动态顶点缓冲：CPU 每帧构建，256KB 约够 3.6K 个方块面（12 字节/顶点）
    for (int i = 0; i < VkCtx::MAX_FRAMES_IN_FLIGHT; i++) {
        createBuffer(ctx, 256 * 1024, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, entityVB_[i]);
        vkMapMemory(ctx.device, entityVB_[i].m, 0, VK_WHOLE_SIZE, 0, &entityMap_[i]);
    }

    // 截图缓冲
    shotBufReady_ = false;
    return true;
}

void Renderer::setWorld(World& w) {
    world_ = &w;
    w.onDestroyChunk = [this](Chunk& c) { destroyChunkBuffers(*ctxPtr_, c); };
}

void Renderer::createAtlasTexture(VkCtx& ctx) {
    int w = atlas_.width, h = atlas_.height;
    // 生成完整 mip 链，让远处地形采样稳定的低分辨率层，
    // 而不是在 128x128 图集上乱采样；这是远处填充率/带宽骤降的关键修复。
    uint32_t mips = 1;
    while ((1u << mips) < (uint32_t)std::max(w, h)) mips++;
    mips++;
    VkImageCreateInfo ici = {};
    ici.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ici.imageType = VK_IMAGE_TYPE_2D;
    ici.format = VK_FORMAT_R8G8B8A8_UNORM;
    ici.extent = {(uint32_t)w, (uint32_t)h, 1};
    ici.mipLevels = mips;
    ici.arrayLayers = 1;
    ici.samples = VK_SAMPLE_COUNT_1_BIT;
    ici.tiling = VK_IMAGE_TILING_OPTIMAL;
    ici.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vkCreateImage(ctx.device, &ici, nullptr, &atlasImage_) != VK_SUCCESS) {
        fprintf(stderr, "[renderer] atlas image create failed\n");
        return;
    }
    VkMemoryRequirements mr;
    vkGetImageMemoryRequirements(ctx.device, atlasImage_, &mr);
    VkMemoryAllocateInfo ai = {};
    ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize = mr.size;
    ai.memoryTypeIndex = ctx.findMemoryType(mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (ai.memoryTypeIndex == UINT32_MAX ||
        vkAllocateMemory(ctx.device, &ai, nullptr, &atlasMem_) != VK_SUCCESS) {
        fprintf(stderr, "[renderer] atlas image alloc failed\n");
        return;
    }
    vkBindImageMemory(ctx.device, atlasImage_, atlasMem_, 0);

    Buffer2 staging;
    if (!createBuffer(ctx, (VkDeviceSize)w * h * 4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging)) {
        fprintf(stderr, "[renderer] atlas staging alloc failed\n");
        return;
    }
    void* ptr;
    vkMapMemory(ctx.device, staging.m, 0, VK_WHOLE_SIZE, 0, &ptr);
    memcpy(ptr, atlas_.rgba.data(), w * h * 4);
    vkUnmapMemory(ctx.device, staging.m);

    VkCommandBuffer cb = ctx.cmds[0];
    vkResetCommandBuffer(cb, 0);
    VkCommandBufferBeginInfo bi = {};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(cb, &bi);

    VkImageMemoryBarrier b0 = {};
    b0.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    b0.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    b0.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    b0.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b0.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b0.image = atlasImage_;
    b0.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    b0.subresourceRange.levelCount = mips;
    b0.subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &b0);

    VkBufferImageCopy bic = {};
    bic.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    bic.imageSubresource.layerCount = 1;
    bic.imageExtent = {(uint32_t)w, (uint32_t)h, 1};
    vkCmdCopyBufferToImage(cb, staging.b, atlasImage_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bic);

    // 逐级生成 mip
    for (uint32_t m = 1; m < mips; m++) {
        VkImageMemoryBarrier br = {};
        br.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        br.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        br.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        br.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        br.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        br.image = atlasImage_;
        br.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        br.subresourceRange.baseMipLevel = m - 1;
        br.subresourceRange.levelCount = 1;
        br.subresourceRange.layerCount = 1;
        vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &br);
        int sw = std::max(1, w >> (m - 1)), sh = std::max(1, h >> (m - 1));
        int dw = std::max(1, w >> m), dh = std::max(1, h >> m);
        VkImageBlit blit = {};
        blit.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, m - 1, 0, 1};
        blit.srcOffsets[1] = {sw, sh, 1};
        blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, m, 0, 1};
        blit.dstOffsets[1] = {dw, dh, 1};
        vkCmdBlitImage(cb, atlasImage_, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                       atlasImage_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);
    }
    // 0..mips-2 层留在 TRANSFER_SRC（曾作为 blit 源），
    // 必须转回 TRANSFER_DST，下面的整链转换才合法。
    {
        VkImageMemoryBarrier br = {};
        br.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        br.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        br.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        br.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        br.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        br.image = atlasImage_;
        br.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        br.subresourceRange.baseMipLevel = 0;
        br.subresourceRange.levelCount = mips - 1;
        br.subresourceRange.layerCount = 1;
        vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &br);
    }
    VkImageMemoryBarrier b1 = {};
    b1.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    b1.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    b1.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    b1.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b1.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b1.image = atlasImage_;
    b1.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    b1.subresourceRange.levelCount = mips;
    b1.subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &b1);

    vkEndCommandBuffer(cb);
    VkSubmitInfo si = {};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cb;
    if (vkQueueSubmit(ctx.graphics, 1, &si, VK_NULL_HANDLE) != VK_SUCCESS)
        fprintf(stderr, "[renderer] atlas upload submit failed\n");
    vkQueueWaitIdle(ctx.graphics);
    staging.destroy(ctx.device);

    VkImageViewCreateInfo vci = {};
    vci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vci.image = atlasImage_;
    vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vci.format = VK_FORMAT_R8G8B8A8_UNORM;
    vci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    vci.subresourceRange.levelCount = mips;
    vci.subresourceRange.layerCount = 1;
    vkCreateImageView(ctx.device, &vci, nullptr, &atlasView_);

    VkSamplerCreateInfo sm = {};
    sm.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sm.magFilter = VK_FILTER_NEAREST;               // 近处保持像素风
    sm.minFilter = VK_FILTER_LINEAR;                // 缩小时走 mip，否则大片平面会出条纹走样
    sm.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;  // mip 层间插值，消除硬边条纹
    sm.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sm.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sm.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    // 各向异性过滤：大片平面斜着看时，单纯的 mip 只能在两个方向里取一个，
    // 另一方向会留下条纹状走样（下界的基岩天花板最明显）。
    sm.anisotropyEnable = VK_TRUE;
    sm.maxAnisotropy = 16.0f;
    sm.minLod = 0.0f;
    // 32px 单元里只有 16px 贴图 + 8px 边缘扩展：mip 4 时单元仅剩 2px，
    // 再往下就会把邻格颜色混进来，所以 LOD 封顶。
    sm.maxLod = (float)(mips > 5 ? 4 : (mips - 1));
    vkCreateSampler(ctx.device, &sm, nullptr, &atlasSampler_);
}

void Renderer::createDescriptors(VkCtx& ctx) {
    VkDescriptorSetLayoutBinding b0 = {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT};
    VkDescriptorSetLayoutBinding b1 = {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT};
    VkDescriptorSetLayoutBinding bs[2] = {b0, b1};
    VkDescriptorSetLayoutCreateInfo li = {};
    li.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    li.bindingCount = 2;
    li.pBindings = bs;
    vkCreateDescriptorSetLayout(ctx.device, &li, nullptr, &terrainDSL_);

    VkDescriptorSetLayoutBinding ui0 = {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT};
    li = {};
    li.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    li.bindingCount = 1;
    li.pBindings = &ui0;
    vkCreateDescriptorSetLayout(ctx.device, &li, nullptr, &uiDSL_);

    VkDescriptorSetLayoutBinding sky0 = {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT};
    li = {};
    li.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    li.bindingCount = 1;
    li.pBindings = &sky0;
    vkCreateDescriptorSetLayout(ctx.device, &li, nullptr, &skyDSL_);


    VkDescriptorPoolSize sizes[2] = {};
    sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    sizes[0].descriptorCount = 8;
    sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    sizes[1].descriptorCount = 8;
    VkDescriptorPoolCreateInfo pci = {};
    pci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pci.maxSets = 8;
    pci.poolSizeCount = 2;
    pci.pPoolSizes = sizes;
    vkCreateDescriptorPool(ctx.device, &pci, nullptr, &pool_);

    VkDescriptorSetAllocateInfo aci = {};
    aci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    aci.descriptorPool = pool_;
    aci.descriptorSetCount = 1;
    for (int i = 0; i < VkCtx::MAX_FRAMES_IN_FLIGHT; i++) {
        aci.pSetLayouts = &terrainDSL_;
        vkAllocateDescriptorSets(ctx.device, &aci, &terrainSet_[i]);
        aci.pSetLayouts = &skyDSL_;
        vkAllocateDescriptorSets(ctx.device, &aci, &skySet_[i]);
        aci.pSetLayouts = &uiDSL_;
        vkAllocateDescriptorSets(ctx.device, &aci, &uiSet_[i]);
    }
}

void Renderer::updateTerrainUBO(VkCtx& ctx, const Camera& cam, float renderDist, int slot) {
    Mat4 proj = Mat4::perspective(1.22f, (float)windowW_ / (float)windowH_, 0.02f, 512.0f);
    Mat4 vp = Mat4::mul(proj, cam.view());
    cachedVP_ = vp;

    // 昼夜循环：timeOfDay∈[0,1]，0.0=正午，0.5=午夜
    float tod = (float)(timeOfDay_);
    float sunAngle = (tod - 0.25f) * 6.2831853f; // 太阳约 0.25 升起、0.75 落下
    Vec3 sunDir(0, 0, 1);
    sunDir = Vec3(std::sin(sunAngle), std::cos(sunAngle), 0.35f);
    sunDir = normalize(sunDir);
    float dayLight = clampf(std::cos(sunAngle), 0.0f, 1.0f);
    dayLight = 0.35f + 0.65f * dayLight;

    struct UboData {
        Mat4 viewProj;
        float camX, camY, camZ, underwater;  // underwater=1 表示相机在水下
        float fogStart, fogEnd, skyR, skyG;
        float skyB, atlasPx, tilesX, tilePx;
        float dayLight, sunX, sunY, sunZ;
    } u;
    u.viewProj = vp;
    u.camX = cam.pos.x; u.camY = cam.pos.y; u.camZ = cam.pos.z;
    bool underwater = world_ &&
        world_->getBlock((int)std::floor(cam.pos.x),
                         (int)std::floor(cam.pos.y),
                         (int)std::floor(cam.pos.z)) == B_WATER;
    u.underwater = underwater ? 1.0f : 0.0f;
    u.fogStart = renderDist * (float)CHUNK_SIZE * 0.70f;
    u.fogEnd = renderDist * (float)CHUNK_SIZE * 0.94f;
    fogEndWorld_ = u.fogEnd;
    // 天空/雾颜色按维度取自 DIMENSION_TYPES（见 dimensions.hpp）
    const auto& dimType = getDimensionType(world_ ? world_->getDimension() : DIM_OVERWORLD);
    u.skyR = dimType.skyColorR / 255.0f;
    u.skyG = dimType.skyColorG / 255.0f;
    u.skyB = dimType.skyColorB / 255.0f;
    u.atlasPx = (float)atlas_.width;
    u.tilesX = (float)atlas_.tilesX;
    u.tilePx = (float)atlas_.cellSize; // shader 按 cell 布局算 tile 原点（含 8px 边距）
    u.dayLight = dayLight;
    u.sunX = sunDir.x; u.sunY = sunDir.y; u.sunZ = sunDir.z;
    memcpy(terrainUBOMap_[slot], &u, sizeof(UboData));

    struct SkyUbo { float hx, hy, hz, ha; float zx, zy, zz, za; } s;
    // 天空色按维度区分（下界/末地无昼夜循环，颜色固定）
    // dimType 已在上面 terrain UBO 处声明
    Vec3 horizon, zenith;
    if (world_ && world_->getDimension() != DIM_OVERWORLD) {
        // 下界/末地：天空色固定（环境光由维度配置 ambient_light 控制）
        float r = dimType.skyColorR / 255.0f, g = dimType.skyColorG / 255.0f, b = dimType.skyColorB / 255.0f;
        horizon = Vec3(r * 0.8f, g * 0.8f, b * 0.8f);  // 地平线压暗一点
        zenith = Vec3(r, g, b);
    } else {
        // 主世界：按昼夜循环插值
        Vec3 dayHorizon(0.60f, 0.77f, 0.90f);
        Vec3 dayZenith(0.35f, 0.55f, 0.86f);
        Vec3 nHorizon(0.10f, 0.12f, 0.22f);
        Vec3 nZenith(0.04f, 0.05f, 0.14f);
        horizon = lerp(nHorizon, dayHorizon, dayLight);
        zenith = lerp(nZenith, dayZenith, dayLight);
    }
    s.hx = horizon.x; s.hy = horizon.y; s.hz = horizon.z;
    s.ha = underwater ? 1.0f : 0.0f;   // 借用 horizon.a 传水下标记
    s.zx = zenith.x; s.zy = zenith.y; s.zz = zenith.z;
    memcpy(skyUBOMap_[slot], &s, sizeof(SkyUbo));
}

// ---------------------------------------------------------------------------
// 区块 GPU 上传
// ---------------------------------------------------------------------------
void Renderer::retireBuffer(VkBuffer b, VkDeviceMemory m, bool opaque, Chunk& c) {
    if (!b && !m) return;
    VkDeviceSize sz = opaque ? (VkDeviceSize)c.opaqueAlloc : (VkDeviceSize)c.waterAlloc;
    void* mp = opaque ? c.opaqueMap : c.waterMap;
    retired_.push_back({b, m, frameIdx_, sz, mp});
    if (opaque) c.opaqueMap = nullptr; else c.waterMap = nullptr;
}

static constexpr size_t kMaxPooledBuffers = 512;

void Renderer::uploadPart(VkCtx& ctx, Chunk& c, bool opaque,
                          const std::vector<TerrainVertex>& verts,
                          const std::vector<uint32_t>& idx) {
    uint64_t& buf = opaque ? c.opaqueBuf : c.waterBuf;
    uint64_t& mem = opaque ? c.opaqueMem : c.waterMem;
    uint64_t& alloc = opaque ? c.opaqueAlloc : c.waterAlloc;
    uint32_t& count = opaque ? c.opaqueCount : c.waterCount;
    uint64_t& vertBytes = opaque ? c.opaqueVertBytes : c.waterVertBytes;

    VkBuffer oldBuf = (VkBuffer)(uintptr_t)buf;
    VkDeviceMemory oldMem = (VkDeviceMemory)(uintptr_t)mem;
    VkDeviceSize needed = (VkDeviceSize)verts.size() * sizeof(TerrainVertex) + (VkDeviceSize)idx.size() * sizeof(uint32_t);

    if (verts.empty() && idx.empty()) {
        // 无几何：先退役旧缓冲让在飞帧用完，再清空该区块的 GPU 状态。
        retireBuffer(oldBuf, oldMem, opaque, c);
        buf = 0; mem = 0; alloc = 0; count = 0; vertBytes = 0;
        return;
    }

    // 总是新分配缓冲并退役旧的：原地改写会与正在读它的在飞帧竞争，
    // 新缓冲只从本帧起被引用，旧缓冲等安全后再释放（多帧在飞）。
    retireBuffer(oldBuf, oldMem, opaque, c);
    Buffer2 nb;
    void* ptr = nullptr;
    // 优先复用足够大的空闲缓冲，不够再新建。
    bool reused = false;
    for (size_t i = 0; i < freePool_.size(); i++) {
        if (freePool_[i].size >= needed) {
            nb.b = freePool_[i].b;
            nb.m = freePool_[i].m;
            ptr = freePool_[i].mapPtr;  // 已持久映射
            freePool_[i] = freePool_.back();
            freePool_.pop_back();
            reused = true;
            break;
        }
    }
    if (!reused) {
        if (!createBuffer(ctx, needed, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, nb))
            return;
        // 用 VK_WHOLE_SIZE 映射一次，缓冲销毁前不再解映射。
        if (vkMapMemory(ctx.device, nb.m, 0, VK_WHOLE_SIZE, 0, &ptr) != VK_SUCCESS) return;
    }
    size_t vb = verts.size() * sizeof(TerrainVertex);
    memcpy(ptr, verts.data(), vb);
    memcpy((uint8_t*)ptr + vb, idx.data(), idx.size() * sizeof(uint32_t));
    buf = (uint64_t)(uintptr_t)nb.b;
    mem = (uint64_t)(uintptr_t)nb.m;
    alloc = (uint64_t)needed;
    count = (uint32_t)idx.size();
    vertBytes = (uint64_t)vb;
    if (opaque) c.opaqueMap = ptr; else c.waterMap = ptr;
}

void Renderer::flushRetired(VkCtx& ctx, uint64_t submittedFrames) {
    // 第 r 帧退役的缓冲，要确认第 r 帧已执行完才能释放。submittedFrames 是即将
    // 产生的逻辑帧号；等过它的 acquire 栅栏后，之前 MAX_FRAMES 帧都已完成。
    uint64_t floor = submittedFrames >= (uint64_t)VkCtx::MAX_FRAMES_IN_FLIGHT
                         ? submittedFrames - (uint64_t)VkCtx::MAX_FRAMES_IN_FLIGHT : 0;
    size_t i = 0;
    while (i < retired_.size()) {
        if (retired_[i].frame < floor) {
            // 回收进池复用而非直接释放，避免流式加载时反复创建/分配；池有上限。
            if (freePool_.size() < kMaxPooledBuffers) {
                freePool_.push_back(retired_[i]);
            } else {
                if (retired_[i].b) vkDestroyBuffer(ctx.device, retired_[i].b, nullptr);
                if (retired_[i].m) vkFreeMemory(ctx.device, retired_[i].m, nullptr);
            }
            retired_[i] = retired_.back();
            retired_.pop_back();
        } else {
            i++;
        }
    }
}


void Renderer::gpuSync(VkCtx&) {
    // 多帧在飞时区块缓冲从不原地改写或释放：上传总是写新缓冲，旧缓冲等读取它的
    // 帧完成后由 flushRetired 回收。因此世界更新前不需要逐帧 GPU 屏障。
}

const uint8_t Renderer::kInvBlocks[] = {
    B_GRASS, B_STONE, B_COBBLE, B_PLANKS, B_LOG, B_DIRT, B_SAND, B_GRAVEL,
    B_GLASS, B_LEAVES, B_SNOW, B_COAL, B_IRON, B_GOLD, B_DIAMOND, B_REDSTONE,
    B_BEDROCK, B_OBSIDIAN, B_END_PORTAL_FRAME, B_NETHERRACK, B_END_STONE,
    B_CHORUS_PLANT, B_CHORUS_FLOWER, B_IRON_BARS, B_TORCH,
};
const int Renderer::kInvCount = (int)(sizeof(kInvBlocks) / sizeof(kInvBlocks[0]));

uint8_t Renderer::selectedBlock() const {
    // 选中格是方块（< kItemTag）就返回其方块 id，否则 B_AIR。
    int h = hotbar_[selectedSlot_];
    if (h >= kItemTag) {
        uint16_t item = (uint16_t)(h - kItemTag);
        const ItemDef& d = itemDef(item);
        return d.category == ITEM_BLOCK ? d.blockId : (uint8_t)B_AIR;
    }
    return (uint8_t)h;
}

uint16_t Renderer::heldMiscItem() const {
    // 选中格是打火石/末影之眼时返回其 ItemId，否则 I_NONE。
    int h = hotbar_[selectedSlot_];
    if (h < kItemTag) return I_NONE;
    uint16_t item = (uint16_t)(h - kItemTag);
    return item == I_FLINT_AND_STEEL || item == I_EYE_OF_ENDER ? item : I_NONE;
}

void Renderer::setInventoryOpen(bool open) {
    // 点击物品栏已直接改写快捷栏格子，关闭时不再覆盖。
    invOpen_ = open;
}

void Renderer::destroyChunkBuffers(VkCtx& ctx, Chunk& c) {
    // 延迟释放：在飞帧可能仍在绘制该区块，这些帧完成后由 flushRetired 回收。
    retireBuffer((VkBuffer)(uintptr_t)c.opaqueBuf, (VkDeviceMemory)(uintptr_t)c.opaqueMem, true, c);
    retireBuffer((VkBuffer)(uintptr_t)c.waterBuf, (VkDeviceMemory)(uintptr_t)c.waterMem, false, c);
    c.opaqueBuf = c.opaqueMem = c.waterBuf = c.waterMem = 0;
    c.opaqueAlloc = c.waterAlloc = c.opaqueCount = c.waterCount = 0;
    c.opaqueVertBytes = c.waterVertBytes = 0;
    (void)ctx;
}

// ---------------------------------------------------------------------------
// 渲染
// ---------------------------------------------------------------------------
bool Renderer::render(VkCtx& ctx, const Camera& cam, Player& player, Input& in, float dt,
                      float renderDist, bool drawUI) {
    windowW_ = ctx.extent.width;
    windowH_ = ctx.extent.height;
    if (in.keys['T']) timeScale_ = 40.0f / 1200.0f; // 按住 T 加速时间
    else timeScale_ = 1.0f / 1200.0f;
    timeOfDay_ += dt * timeScale_;
    if (timeOfDay_ >= 1.0f) timeOfDay_ -= 1.0f;

    if (pendingShot_.empty()) shotTaken_ = false;

    uint32_t imageIndex;
    curFrame_ = (int)(frameIdx_ % (uint64_t)VkCtx::MAX_FRAMES_IN_FLIGHT);
    VkCtx::Frame& f = ctx.frames[curFrame_];
    VkCommandBuffer cb = ctx.cmds[curFrame_];
    // acquireNext 会等待本槽的栅栏（该槽命令缓冲/UBO/描述符集的上次使用已完成），
    // 所以此刻改写它们是安全的；CPU 因此能领先 GPU 一帧而不必停顿。
    if (!ctx.acquireNext(f, imageIndex)) {
        ctx.recreateSwapchain(windowW_, windowH_);
        return false;
    }
    // 拿到本槽即说明 frameIdx_ - MAX_FRAMES_IN_FLIGHT 之前的帧已完成，可回收其区块缓冲。
    flushRetired(ctx, frameIdx_);
    updateTerrainUBO(ctx, cam, renderDist, curFrame_);
    vkResetCommandBuffer(cb, 0);
    VkCommandBufferBeginInfo bi = {};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(cb, &bi);

    VkClearValue clears[2];
    clears[0].color = {{0.35f, 0.55f, 0.86f, 1.0f}};
    clears[1].depthStencil = {1.0f, 0};
    VkRenderPassBeginInfo rp = {};
    rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp.renderPass = ctx.renderPass;
    rp.framebuffer = ctx.framebuffers[imageIndex];
    rp.renderArea = {{0, 0}, ctx.extent};
    rp.clearValueCount = 2;
    rp.pClearValues = clears;

    // 必须在 render pass 之前上传待处理网格：uploadPart 里的创建/分配不能出现在 pass 内。
    if (world_) {
        world_->snapshotChunks(snapshot_);
        for (auto& si : snapshot_) {
            Chunk* c = si.c;
            if (c->needsUpload.exchange(false)) {
                std::lock_guard<std::mutex> lk(c->meshLock);
                ChunkMeshData local = c->mesh;
                uploadPart(ctx, *c, true, local.opaqueVerts, local.opaqueIdx);
                uploadPart(ctx, *c, false, local.waterVerts, local.waterIdx);
            }
        }
    }

    vkCmdBeginRenderPass(cb, &rp, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport vp = {0, 0, (float)ctx.extent.width, (float)ctx.extent.height, 0, 1};
    vkCmdSetViewport(cb, 0, 1, &vp);
    VkRect2D sc = {{0, 0}, ctx.extent};
    vkCmdSetScissor(cb, 0, 1, &sc);

    // 天空
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, skyPipe_);
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, skyLayout_, 0, 1, &skySet_[curFrame_], 0, nullptr);
    vkCmdDraw(cb, 3, 1, 0, 0);

    drawChunks(ctx, cam);

    if (drawUI) {
        drawUIOverlay(ctx, cam, in);
    }

    vkCmdEndRenderPass(cb);

    // 截图拷贝（pass 之后，图像处于 PRESENT_SRC 布局）
    bool takeShot = !pendingShot_.empty() && !shotTaken_;
    if (takeShot) {
        if (!shotBufReady_ ||
            (uint32_t)shotW_ != ctx.extent.width || (uint32_t)shotH_ != ctx.extent.height) {
            if (shotBuf_) vkDestroyBuffer(ctx.device, shotBuf_, nullptr);
            if (shotMem_) vkFreeMemory(ctx.device, shotMem_, nullptr);
            shotW_ = ctx.extent.width;
            shotH_ = ctx.extent.height;
            Buffer2 b;
            if (createBuffer(ctx, (VkDeviceSize)shotW_ * shotH_ * 4, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, b)) {
                shotBuf_ = b.b;
                shotMem_ = b.m;
                shotBufReady_ = true;
            }
        }
        if (shotBufReady_) {
            VkImageMemoryBarrier br = {};
            br.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            br.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            br.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            br.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            br.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            br.image = ctx.swapImages[imageIndex];
            br.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            br.subresourceRange.levelCount = 1;
            br.subresourceRange.layerCount = 1;
            vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 0, 0, nullptr, 0, nullptr, 1, &br);
            VkBufferImageCopy bic = {};
            bic.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            bic.imageSubresource.layerCount = 1;
            bic.imageExtent = {(uint32_t)shotW_, (uint32_t)shotH_, 1};
            vkCmdCopyImageToBuffer(cb, ctx.swapImages[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                   shotBuf_, 1, &bic);
            br.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            br.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                 0, 0, nullptr, 0, nullptr, 1, &br);
            shotTaken_ = true;
        }
    }

    vkEndCommandBuffer(cb);
    if (!ctx.presentImage(imageIndex, f, cb)) {
        ctx.recreateSwapchain(windowW_, windowH_);
        return false;
    }
    frameIdx_++;

    if (shotTaken_ && !pendingShot_.empty()) {
        vkWaitForFences(ctx.device, 1, &f.fence, VK_TRUE, UINT64_MAX);
        std::vector<uint8_t> bgra((size_t)shotW_ * shotH_ * 4);
        void* ptr = nullptr;
        if (vkMapMemory(ctx.device, shotMem_, 0, VK_WHOLE_SIZE, 0, &ptr) == VK_SUCCESS) {
            memcpy(bgra.data(), ptr, bgra.size());
            vkUnmapMemory(ctx.device, shotMem_);
            savePNG(pendingShot_, shotW_, shotH_, bgra);
        }
        pendingShot_.clear();
    }

    frames_++;
    fpsTimer_ += dt;
    if (fpsTimer_ >= 0.5f) {
        fps_ = frames_ / fpsTimer_;
        frames_ = 0;
        fpsTimer_ = 0;
    }
    return true;
}

// 向 verts 追加一个旋转盒（6 面 × 6 顶点 = 36 verts，无索引）。
// c=中心，h=半尺寸，yaw=绕 Y 弧度，tile=图集 tile，shade=亮度基值。
static void appendBox(std::vector<TerrainVertex>& verts, Vec3 c, Vec3 h, float yaw,
                      uint8_t tile, uint8_t shade) {
    float cy = cosf(yaw), sy = sinf(yaw);
    auto rot = [&](float lx, float ly, float lz) -> Vec3 {
        return {c.x + lx * cy - lz * sy, c.y + ly, c.z + lx * sy + lz * cy};
    };
    Vec3 v[8] = {
        rot(-h.x, -h.y, -h.z), rot( h.x, -h.y, -h.z), rot( h.x,  h.y, -h.z), rot(-h.x,  h.y, -h.z),
        rot(-h.x, -h.y,  h.z), rot( h.x, -h.y,  h.z), rot( h.x,  h.y,  h.z), rot(-h.x,  h.y,  h.z),
    };
    // 面定义：[顶点索引×4] + 方向亮度系数
    struct Face { int vi[4]; uint8_t sh; };
    Face faces[6] = {
        {{1,0,3,2}, (uint8_t)(shade*255/255)},  // +X 面 PX
        {{4,5,6,7}, (uint8_t)(shade*255/255)},  // -X 面 NX
        {{3,7,6,2}, (uint8_t)(shade*255/255)},  // +Y 顶面 PY（最亮）
        {{5,4,0,1}, (uint8_t)(shade*140/255)},  // -Y 底面 NY（最暗）
        {{5,1,2,6}, (uint8_t)(shade*210/255)},  // +Z 面 PZ
        {{0,4,7,3}, (uint8_t)(shade*190/255)},  // -Z 面 NZ
    };
    for (auto& f : faces) {
        for (int t : {0,1,2, 0,2,3}) {
            Vec3 p = v[f.vi[t]];
            TerrainVertex vt;
            // 顶点位置是 1/16 格单位的 int16，越界会截断，故夹一下
            auto q16 = [](float q) {
                float s = std::floor(q * 16.0f + 0.5f);
                if (s > 32767.0f) s = 32767.0f;
                if (s < -32768.0f) s = -32768.0f;
                return (int16_t)s;
            };
            vt.x = q16(p.x); vt.y = q16(p.y); vt.z = q16(p.z);
            vt.w = 0;
            vt.u = (t < 3) ? ((t == 0 || t == 3) ? 0 : 16) : ((t == 2) ? 16 : 0);
            vt.v = (t <= 1) ? 0 : 16;
            vt.tex = tile;
            vt.shade = f.sh;
            verts.push_back(vt);
        }
    }
}

void Renderer::drawEntities(VkCtx& ctx, const Camera& cam) {
    if (!world_ || !entityPipe_) return;
    entityVerts_.clear();
    const Vec3 zero(0, 0, 0);
    for (auto& e : world_->entities()) {
        if (e->dead) continue;
        if (e->kind == EntityKind::EndCrystal) {
            // 末影水晶：两层反向旋转的立方体（外大内小）
            auto* crystal = static_cast<EndCrystal*>(e.get());
            Vec3 center = e->pos + Vec3(0, 1.0f + sinf(e->age * 1.2f) * 0.15f, 0);  // 悬浮+微浮动
            // 外层：半宽 0.8、高 1.5，绕 Y 旋转 phase
            appendBox(entityVerts_, center, Vec3(0.8f, 0.75f, 0.8f), crystal->phase,
                      T_END_CRYSTAL, 220);
            // 内层：小芯，反向旋转
            appendBox(entityVerts_, center, Vec3(0.35f, 0.45f, 0.35f), -crystal->phase * 0.7f,
                      T_END_CRYSTAL, 255);
        }
    }
    if (entityVerts_.empty()) return;
    // 上传顶点（缓冲 256KB，超了就截断，别越界写显存映射）
    size_t maxVerts = (256 * 1024) / sizeof(TerrainVertex);
    size_t n = entityVerts_.size() < maxVerts ? entityVerts_.size() : maxVerts;
    memcpy(entityMap_[curFrame_], entityVerts_.data(), n * sizeof(TerrainVertex));
    VkCommandBuffer cb = ctx.cmds[curFrame_];
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, entityPipe_);
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, terrainLayout_, 0, 1, &terrainSet_[curFrame_], 0, nullptr);
    Vec3 origin(0, 0, 0);
    vkCmdPushConstants(cb, terrainLayout_, VK_SHADER_STAGE_VERTEX_BIT, 0, 16, &origin);
    VkBuffer vb = entityVB_[curFrame_].b;
    VkDeviceSize off = 0;
    vkCmdBindVertexBuffers(cb, 0, 1, &vb, &off);
    vkCmdDraw(cb, (uint32_t)n, 1, 0, 0);
}

// drawEntities 的调用点应放在 drawChunks 结束大括号之后

void Renderer::drawChunks(VkCtx& ctx, const Camera& cam) {
    Frustum fr;
    fr.extract(cachedVP_);
    VkCommandBuffer cb = ctx.cmds[curFrame_];

    const float camX = cam.pos.x, camY = cam.pos.y, camZ = cam.pos.z;
    const float f2 = fogEndWorld_ * fogEndWorld_;
    auto chunkCulled = [&](int cx, int cz) {
        float minX = cx * (float)CHUNK_SIZE, minZ = cz * (float)CHUNK_SIZE;
        float maxX = minX + CHUNK_SIZE, maxZ = minZ + CHUNK_SIZE;
        if (!fr.testAABB(minX, 0, minZ, maxX, WORLD_HEIGHT, maxZ)) return true;
        if (f2 > 0.0f) {
            float nx = clampf(camX, minX, maxX);
            float ny = clampf(camY, 0.0f, (float)WORLD_HEIGHT);
            float nz = clampf(camZ, minZ, maxZ);
            float dx = nx - camX, dy = ny - camY, dz = nz - camZ;
            if (dx * dx + dy * dy + dz * dz > f2) return true;
        }
        return false;
    };

    // 从 render() 里的快照收集可见区块，无需加锁。
    struct VisChunk { Chunk* c; int cx; int cz; };
    std::vector<VisChunk> visible;
    visible.reserve(512);
    for (auto& si : snapshot_) {
        if (si.c->state.load() < 2) continue;
        if (chunkCulled(si.cx, si.cz)) continue;
        visible.push_back({si.c, si.cx, si.cz});
    }

    // 实体在第 1 遍（不透明）之后、第 2 遍（水）之前绘制。

    int draws = 0;
    // 第 1 遍：绘制全部不透明几何
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, terrainPipe_);
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, terrainLayout_, 0, 1, &terrainSet_[curFrame_], 0, nullptr);
    for (auto& v : visible) {
        Chunk& c = *v.c;
        if (!c.opaqueBuf || !c.opaqueCount) continue;
        draws++;
        Vec3 origin((float)(v.cx * CHUNK_SIZE), 0, (float)(v.cz * CHUNK_SIZE));
        vkCmdPushConstants(cb, terrainLayout_, VK_SHADER_STAGE_VERTEX_BIT, 0, 16, &origin);
        VkBuffer vb = (VkBuffer)(uintptr_t)c.opaqueBuf;
        VkDeviceSize off = 0;
        vkCmdBindVertexBuffers(cb, 0, 1, &vb, &off);
        vkCmdBindIndexBuffer(cb, vb, c.opaqueVertBytes, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cb, c.opaqueCount, 1, 0, 0, 1);
    }
    // 第 1.5 遍：绘制实体（必须在水之前）
    drawEntities(ctx, cam);

    // 第 2 遍：绘制全部水（半透明，必须在不透明几何之后）
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, waterPipe_);
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, terrainLayout_, 0, 1, &terrainSet_[curFrame_], 0, nullptr);
    for (auto& v : visible) {
        Chunk& c = *v.c;
        if (!c.waterBuf || !c.waterCount) continue;
        Vec3 origin((float)(v.cx * CHUNK_SIZE), 0, (float)(v.cz * CHUNK_SIZE));
        vkCmdPushConstants(cb, terrainLayout_, VK_SHADER_STAGE_VERTEX_BIT, 0, 16, &origin);
        VkBuffer vb = (VkBuffer)(uintptr_t)c.waterBuf;
        VkDeviceSize off = 0;
        vkCmdBindVertexBuffers(cb, 0, 1, &vb, &off);
        vkCmdBindIndexBuffer(cb, vb, c.waterVertBytes, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cb, c.waterCount, 1, 0, 0, 1);
    }

    debugDraws_ = draws;
}

// ---------------------------------------------------------------------------
// 叠加 UI
// ---------------------------------------------------------------------------
void Renderer::drawUIOverlay(VkCtx& ctx, const Camera& cam, Input& in) {
    // 方块高亮：把目标方块的棱边投影到屏幕，用 UI 管线画细四边形。
    Vec3 fwd = cam.forward();
    RayHit hit = raycastWorld(*world_, cam.pos, fwd, 6.0f);

    Mat4 vp = cachedVP_;

    // 构建 UI 四边形
    if (in.keys['1']) selectedSlot_ = 0;
    if (in.keys['2']) selectedSlot_ = 1;
    if (in.keys['3']) selectedSlot_ = 2;
    if (in.keys['4']) selectedSlot_ = 3;
    if (in.keys['5']) selectedSlot_ = 4;
    if (in.keys['6']) selectedSlot_ = 5;
    if (in.keys['7']) selectedSlot_ = 6;
    if (in.keys['8']) selectedSlot_ = 7;
    if (in.keys['9']) selectedSlot_ = 8;
    if (in.scrollAccum > 0) selectedSlot_ = (selectedSlot_ - 1 + 9) % 9;
    if (in.scrollAccum < 0) selectedSlot_ = (selectedSlot_ + 1) % 9;

    std::vector<UIVertex> quads;
    quads.reserve(512);

    auto ndcX = [&](float px) { return px / (float)windowW_ * 2.0f - 1.0f; };
    auto ndcY = [&](float py) { return py / (float)windowH_ * 2.0f - 1.0f; };
    auto pushQuad = [&](float x0, float y0, float x1, float y1, int tile,
                        float r, float g, float b, float a) {
        float u0 = atlas_.tileU(tile, 0), v0 = atlas_.tileV(tile, 0);
        float u1 = atlas_.tileU(tile, 2), v1 = atlas_.tileV(tile, 2);
        UIVertex q[6] = {
            {ndcX(x0), ndcY(y0), u0, v0, r, g, b, a},
            {ndcX(x1), ndcY(y0), u1, v0, r, g, b, a},
            {ndcX(x1), ndcY(y1), u1, v1, r, g, b, a},
            {ndcX(x0), ndcY(y0), u0, v0, r, g, b, a},
            {ndcX(x1), ndcY(y1), u1, v1, r, g, b, a},
            {ndcX(x0), ndcY(y1), u0, v1, r, g, b, a},
        };
        for (auto& v : q) quads.push_back(v);
    };

    // 把目标方块的 12 条棱画成屏幕空间细四边形
    if (hit.hit) {
        float m[16];
        for (int i = 0; i < 16; i++) m[i] = vp.m[i];
        auto project = [&](float px, float py, float pz, float& ox, float& oy) {
            float c0 = m[0] * px + m[4] * py + m[8] * pz + m[12];
            float c1 = m[1] * px + m[5] * py + m[9] * pz + m[13];
            float c3 = m[3] * px + m[7] * py + m[11] * pz + m[15];
            if (c3 <= 0.001f) return false;
            ox = (c0 / c3 * 0.5f + 0.5f) * (float)windowW_;
            oy = (c1 / c3 * 0.5f + 0.5f) * (float)windowH_;
            return true;
        };
        // 用定向四边形（平行四边形）代替屏幕空间线段
        auto pushLineQuad = [&](float ax, float ay, float bx, float by, float t,
                                float r, float g, float b, float a) {
            float dx = bx - ax, dy = by - ay;
            float len = std::sqrt(dx * dx + dy * dy);
            if (len < 0.6f) return;
            float nx = -dy / len * t, ny = dx / len * t;
            float u0 = atlas_.tileU(T_WHITE, 0), v0 = atlas_.tileV(T_WHITE, 0);
            float u1 = atlas_.tileU(T_WHITE, 2), v1 = atlas_.tileV(T_WHITE, 2);
            UIVertex q[6] = {
                {ndcX(ax + nx), ndcY(ay + ny), u0, v0, r, g, b, a},
                {ndcX(bx + nx), ndcY(by + ny), u1, v0, r, g, b, a},
                {ndcX(bx - nx), ndcY(by - ny), u1, v1, r, g, b, a},
                {ndcX(ax + nx), ndcY(ay + ny), u0, v0, r, g, b, a},
                {ndcX(bx - nx), ndcY(by - ny), u1, v1, r, g, b, a},
                {ndcX(ax - nx), ndcY(ay - ny), u0, v1, r, g, b, a},
            };
            for (auto& v : q) quads.push_back(v);
        };
        float bx = (float)hit.x, by = (float)hit.y, bz = (float)hit.z;
        // 轮廓盒按方块形状给：末地门框架只有 13/16 高，有眼时再画中间凸起的眼块
        // （对应原版 SHAPE_EMPTY / SHAPE_FULL）。
        auto drawBoxEdges = [&](float x0, float y0, float z0, float x1, float y1, float z1) {
            Vec3 cube[8] = {
                {x0, y0, z0}, {x1, y0, z0}, {x1, y1, z0}, {x0, y1, z0},
                {x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1}};
            static const int edges[12][2] = {{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},
                                             {0,4},{1,5},{2,6},{3,7}};
            for (auto& e : edges) {
                float ax, ay, b2x, b2y;
                if (!project(cube[e[0]].x, cube[e[0]].y, cube[e[0]].z, ax, ay)) continue;
                if (!project(cube[e[1]].x, cube[e[1]].y, cube[e[1]].z, b2x, b2y)) continue;
                pushLineQuad(ax, ay, b2x, b2y, 1.0f, 0, 0, 0, 0.85f);
            }
        };
        uint8_t hitBlock = world_->getBlock(hit.x, hit.y, hit.z);
        if (blockIsPortalFrame(hitBlock)) {
            const float kFrameTop = 13.0f / 16.0f;
            drawBoxEdges(bx, by, bz, bx + 1, by + kFrameTop, bz + 1);
            if (blockFrameHasEye(hitBlock))
                drawBoxEdges(bx + 0.25f, by + kFrameTop, bz + 0.25f,
                             bx + 0.75f, by + 1.0f, bz + 0.75f);
        } else {
            drawBoxEdges(bx, by, bz, bx + 1, by + 1, bz + 1);
        }
    }

    // 准星（物品栏打开时隐藏）
    if (!invOpen_) {
        float cx = windowW_ * 0.5f, cy = windowH_ * 0.5f;
        float len = 6.0f, th = 1.0f;
        pushQuad(cx - th, cy - len, cx + th, cy + len, T_WHITE, 0, 0, 0, 0.85f);
        pushQuad(cx - len, cy - th, cx + len, cy + th, T_WHITE, 0, 0, 0, 0.85f);
    }

    // 快捷栏
    const float slotSize = 40.0f, gap = 2.0f;
    float total = 9 * slotSize + 8 * gap;
    float hx0 = (windowW_ - total) * 0.5f;
    float hy0 = windowH_ - slotSize - 8.0f;
    for (int i = 0; i < 9; i++) {
        float x0 = hx0 + i * (slotSize + gap);
        bool sel = i == selectedSlot_;
        pushQuad(x0, hy0, x0 + slotSize, hy0 + slotSize, T_WHITE,
                 sel ? 0.45f : 0.15f, sel ? 0.45f : 0.15f, sel ? 0.45f : 0.15f, 0.95f);
        int h = hotbar_[i];
        int tile = h >= kItemTag ? itemDef((uint16_t)(h - kItemTag)).iconTile
                                 : blockTile((uint8_t)h, F_PY);
        pushQuad(x0 + 5, hy0 + 5, x0 + slotSize - 5, hy0 + slotSize - 5, tile, 1, 1, 1, 1);
        if (sel) {
            pushQuad(x0, hy0, x0 + slotSize, hy0 + 1.5f, T_WHITE, 1, 1, 1, 1);
            pushQuad(x0, hy0 + slotSize - 1.5f, x0 + slotSize, hy0 + slotSize, T_WHITE, 1, 1, 1, 1);
            pushQuad(x0, hy0, x0 + 1.5f, hy0 + slotSize, T_WHITE, 1, 1, 1, 1);
            pushQuad(x0 + slotSize - 1.5f, hy0, x0 + slotSize, hy0 + slotSize, T_WHITE, 1, 1, 1, 1);
        }
    }

    // 物品栏（E）界面 —— 两个标签页：0=方块 1=物品（装备类为生存模式预留）
    if (invOpen_) {
        const float invSlot = 44.0f, invGap = 4.0f;
        int count = invPage_ == 0 ? kInvCount : (I_COUNT - 1);
        int rows = (count + kInvCols - 1) / kInvCols;
        float gridW = kInvCols * invSlot + (kInvCols - 1) * invGap;
        float gridH = rows * invSlot + (rows - 1) * invGap;
        float gx0 = (windowW_ - gridW) * 0.5f;
        float gy0 = (windowH_ - gridH) * 0.5f;
        // 半透明背景遮罩
        pushQuad(0, 0, (float)windowW_, (float)windowH_, T_WHITE, 0.05f, 0.05f, 0.05f, 0.55f);
        // 标签按钮（石头=方块页，铁镐=物品页）
        const float tabW = 64.0f, tabH = 28.0f, tabGap = 8.0f;
        float ty0 = gy0 - tabH - 10.0f;
        for (int t = 0; t < 2; t++) {
            float tx0 = gx0 + t * (tabW + tabGap);
            bool active = t == invPage_;
            pushQuad(tx0, ty0, tx0 + tabW, ty0 + tabH, T_WHITE,
                     active ? 0.45f : 0.18f, active ? 0.45f : 0.18f, active ? 0.45f : 0.18f, 0.92f);
            pushQuad(tx0 + 6, ty0 + 5, tx0 + 22, ty0 + 21, t == 0 ? T_STONE : (T_ITEM_BASE + 8), 1, 1, 1, 1);
            if (in.mouse[0] && !prevMouse0_ && cursorX_ >= tx0 && cursorX_ <= tx0 + tabW &&
                cursorY_ >= ty0 && cursorY_ <= ty0 + tabH) {
                invPage_ = t;
            }
        }
        int currentIdx = -1;
        if (invPage_ == 0) {
            // 选中格是方块时，找出它在方块页中的索引
            int cur = hotbar_[selectedSlot_];
            for (int i = 0; i < kInvCount; i++)
                if (cur < kItemTag && kInvBlocks[i] == (uint8_t)cur) { currentIdx = i; break; }
        } else {
            int cur = hotbar_[selectedSlot_];
            for (int i = 1; i < I_COUNT; i++)
                if (cur >= kItemTag && (cur - kItemTag) == (int)i) { currentIdx = i - 1; break; }
        }
        for (int i = 0; i < count; i++) {
            int col = i % kInvCols, row = i / kInvCols;
            float x0 = gx0 + col * (invSlot + invGap);
            float y0 = gy0 + row * (invSlot + invGap);
            bool sel = i == currentIdx;
            pushQuad(x0, y0, x0 + invSlot, y0 + invSlot, T_WHITE,
                     sel ? 0.50f : 0.22f, sel ? 0.50f : 0.22f, sel ? 0.50f : 0.22f, 0.92f);
            int tile = invPage_ == 0 ? blockTile(kInvBlocks[i], F_PY)
                                     : ITEM_DEFS[i + 1].iconTile;
            pushQuad(x0 + 4, y0 + 4, x0 + invSlot - 4, y0 + invSlot - 4, tile, 1, 1, 1, 1);
            if (sel) {
                pushQuad(x0, y0, x0 + invSlot, y0 + 2.0f, T_WHITE, 1, 1, 1, 1);
                pushQuad(x0, y0 + invSlot - 2.0f, x0 + invSlot, y0 + invSlot, T_WHITE, 1, 1, 1, 1);
                pushQuad(x0, y0, x0 + 2.0f, y0 + invSlot, T_WHITE, 1, 1, 1, 1);
                pushQuad(x0 + invSlot - 2.0f, y0, x0 + invSlot, y0 + invSlot, T_WHITE, 1, 1, 1, 1);
            }
        }
        // 点击选中（标签页优先，点中标签页时本帧不再选格子）
        if (in.mouse[0] && !prevMouse0_ && cursorY_ >= gy0) {
            for (int i = 0; i < count; i++) {
                int col = i % kInvCols, row = i / kInvCols;
                float x0 = gx0 + col * (invSlot + invGap);
                float y0 = gy0 + row * (invSlot + invGap);
                if (cursorX_ >= x0 && cursorX_ <= x0 + invSlot && cursorY_ >= y0 && cursorY_ <= y0 + invSlot) {
                    // 把该资源放入当前快捷栏选中格。
                    if (invPage_ == 0) hotbar_[selectedSlot_] = kInvBlocks[i];
                    else hotbar_[selectedSlot_] = kItemTag + (i + 1);
                    break;
                }
            }
        }
    }
    prevMouse0_ = in.mouse[0];

    memcpy(uiMap_[curFrame_], quads.data(), quads.size() * sizeof(UIVertex));
    VkCommandBuffer cb = ctx.cmds[curFrame_];
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, uiPipe_);
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, uiLayout_, 0, 1, &uiSet_[curFrame_], 0, nullptr);
    VkBuffer ub = uiBuf_[curFrame_].b;
    VkDeviceSize off = 0;
    vkCmdBindVertexBuffers(cb, 0, 1, &ub, &off);
    vkCmdDraw(cb, (uint32_t)quads.size(), 1, 0, 0);
}

void Renderer::shutdown(VkCtx& ctx) {
    if (shotBuf_) vkDestroyBuffer(ctx.device, shotBuf_, nullptr);
    if (shotMem_) vkFreeMemory(ctx.device, shotMem_, nullptr);
    for (int i = 0; i < VkCtx::MAX_FRAMES_IN_FLIGHT; i++) {
        if (uiMap_[i]) vkUnmapMemory(ctx.device, uiBuf_[i].m);
        uiBuf_[i].destroy(ctx.device);
        if (terrainUBOMap_[i]) vkUnmapMemory(ctx.device, terrainUBO_[i].m);
        terrainUBO_[i].destroy(ctx.device);
        if (skyUBOMap_[i]) vkUnmapMemory(ctx.device, skyUBO_[i].m);
        skyUBO_[i].destroy(ctx.device);
    }
    // GPU 已空闲，释放所有延迟回收的区块缓冲。
    for (auto& r : retired_) {
        if (r.mapPtr) vkUnmapMemory(ctx.device, r.m);
        if (r.b) vkDestroyBuffer(ctx.device, r.b, nullptr);
        if (r.m) vkFreeMemory(ctx.device, r.m, nullptr);
    }
    retired_.clear();
    for (auto& r : freePool_) {
        if (r.mapPtr) vkUnmapMemory(ctx.device, r.m);
        if (r.b) vkDestroyBuffer(ctx.device, r.b, nullptr);
        if (r.m) vkFreeMemory(ctx.device, r.m, nullptr);
    }
    freePool_.clear();
    // 实体动态缓冲
    for (int i = 0; i < VkCtx::MAX_FRAMES_IN_FLIGHT; i++) {
        if (entityMap_[i]) vkUnmapMemory(ctx.device, entityVB_[i].m);
        entityVB_[i].destroy(ctx.device);
    }
    if (pool_) vkDestroyDescriptorPool(ctx.device, pool_, nullptr);
    if (terrainDSL_) vkDestroyDescriptorSetLayout(ctx.device, terrainDSL_, nullptr);
    if (uiDSL_) vkDestroyDescriptorSetLayout(ctx.device, uiDSL_, nullptr);
    if (skyDSL_) vkDestroyDescriptorSetLayout(ctx.device, skyDSL_, nullptr);
    if (terrainPipe_) vkDestroyPipeline(ctx.device, terrainPipe_, nullptr);
    if (waterPipe_) vkDestroyPipeline(ctx.device, waterPipe_, nullptr);
    if (skyPipe_) vkDestroyPipeline(ctx.device, skyPipe_, nullptr);
    if (uiPipe_) vkDestroyPipeline(ctx.device, uiPipe_, nullptr);
    if (entityPipe_) vkDestroyPipeline(ctx.device, entityPipe_, nullptr);
    if (terrainLayout_) vkDestroyPipelineLayout(ctx.device, terrainLayout_, nullptr);
    if (skyLayout_) vkDestroyPipelineLayout(ctx.device, skyLayout_, nullptr);
    if (uiLayout_) vkDestroyPipelineLayout(ctx.device, uiLayout_, nullptr);
    if (atlasView_) vkDestroyImageView(ctx.device, atlasView_, nullptr);
    if (atlasSampler_) vkDestroySampler(ctx.device, atlasSampler_, nullptr);
    if (atlasImage_) vkDestroyImage(ctx.device, atlasImage_, nullptr);
    if (atlasMem_) vkFreeMemory(ctx.device, atlasMem_, nullptr);
}








