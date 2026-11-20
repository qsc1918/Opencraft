// 菜单先在 RGBA 画布上用自带的像素字体绘制，再上传成纹理画全屏四边形。
// 绘制过程不依赖任何系统字体服务，Windows 与 Linux 得到逐字节相同的画面。
#include "menu.hpp"
#include "gfx_shim.hpp"
#include "window.hpp"
#include "png.hpp"
#include "version.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace {
const int kBtnBorder = 3; // 200x20 按钮贴图的九宫格边框宽
const int kTextScale = 2; // 12px 点阵字放大到 24px，复杂汉字笔画仍分得开

void SetPixel(std::vector<uint8_t>& canvas, int w, int h, int x, int y,
              uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (x < 0 || y < 0 || x >= w || y >= h || a == 0) return;
    uint8_t* dst = canvas.data() + ((size_t)y * w + x) * 4;
    if (a == 255) { dst[0] = r; dst[1] = g; dst[2] = b; dst[3] = 255; return; }
    int inv = 255 - a;
    dst[0] = (uint8_t)((dst[0] * inv + r * a) / 255);
    dst[1] = (uint8_t)((dst[1] * inv + g * a) / 255);
    dst[2] = (uint8_t)((dst[2] * inv + b * a) / 255);
    dst[3] = (uint8_t)std::min(255, dst[3] + a);
}

void FillRect(std::vector<uint8_t>& canvas, int cw, int ch,
              int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
    for (int yy = y; yy < y + h; yy++)
        for (int xx = x; xx < x + w; xx++) SetPixel(canvas, cw, ch, xx, yy, r, g, b, a);
}

void BlitImage(std::vector<uint8_t>& canvas, int cw, int ch,
               const std::vector<uint8_t>& src, int sw, int sh,
               int sx, int sy, int dx, int dy, int dw, int dh) {
    if (src.empty() || dw <= 0 || dh <= 0) return;
    for (int yy = 0; yy < dh; yy++) {
        int iy = sy + (int)((float)yy * sh / dh);
        if (iy < 0 || iy >= sh) continue;
        for (int xx = 0; xx < dw; xx++) {
            int ix = sx + (int)((float)xx * sw / dw);
            if (ix < 0 || ix >= sw) continue;
            const uint8_t* p = src.data() + ((size_t)iy * sw + ix) * 4;
            SetPixel(canvas, cw, ch, dx + xx, dy + yy, p[0], p[1], p[2], p[3]);
        }
    }
}

void Slice9(std::vector<uint8_t>& canvas, int cw, int ch,
            const std::vector<uint8_t>& src, int sw, int sh, int border,
            int x, int y, int w, int h) {
    int b = border;
    BlitImage(canvas, cw, ch, src, sw, sh, 0, 0, x, y, b, b);
    BlitImage(canvas, cw, ch, src, sw, sh, sw - b, 0, x + w - b, y, b, b);
    BlitImage(canvas, cw, ch, src, sw, sh, 0, sh - b, x, y + h - b, b, b);
    BlitImage(canvas, cw, ch, src, sw, sh, sw - b, sh - b, x + w - b, y + h - b, b, b);
    BlitImage(canvas, cw, ch, src, sw, sh, b, 0, x + b, y, w - 2 * b, b);
    BlitImage(canvas, cw, ch, src, sw, sh, b, sh - b, x + b, y + h - b, w - 2 * b, b);
    BlitImage(canvas, cw, ch, src, sw, sh, 0, b, x, y + b, b, h - 2 * b);
    BlitImage(canvas, cw, ch, src, sw, sh, sw - b, b, x + w - b, y + b, b, h - 2 * b);
    BlitImage(canvas, cw, ch, src, sw, sh, b, b, x + b, y + b, w - 2 * b, h - 2 * b);
}

} // namespace

bool Menu::init(VkCtx& ctx, Window& win, const std::string& assetDir) {
    if (!LoadTextures(assetDir)) return false;
    if (!font_.Load(assetDir + "/font/pixel.png", assetDir + "/font/pixel.fnt")) {
        fprintf(stderr, "[menu] pixel font load failed\n");
        return false;
    }
    EnsureCanvas(win.width(), win.height());
    CreateMenuTexture(ctx);

    VkDescriptorSetLayoutBinding b0 = {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
                                       VK_SHADER_STAGE_FRAGMENT_BIT};
    VkDescriptorSetLayoutCreateInfo li = {};
    li.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    li.bindingCount = 1;
    li.pBindings = &b0;
    vkCreateDescriptorSetLayout(ctx.device, &li, nullptr, &dsl_);

    VkPipelineLayoutCreateInfo pl = {};
    pl.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pl.setLayoutCount = 1;
    pl.pSetLayouts = &dsl_;
    vkCreatePipelineLayout(ctx.device, &pl, nullptr, &layout_);

    VkVertexInputBindingDescription vb = {0, 32, VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription va[3] = {};
    va[0] = {0, 0, VK_FORMAT_R32G32_SFLOAT, 0};
    va[1] = {1, 0, VK_FORMAT_R32G32_SFLOAT, 8};
    va[2] = {2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 16};

    std::string shaderDir = assetDir + "/../shaders";
    if (gfxutil::readFile(shaderDir + "/menu.vert.spv").empty()) shaderDir = "shaders";
    pipe_ = gfxutil::makePipeline(ctx, ctx.renderPass, layout_, shaderDir,
                                  "menu.vert.spv", "menu.frag.spv",
                                  &vb, va, 1, 3,
                                  VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_CULL_MODE_NONE,
                                  false, false, false);
    if (!pipe_) { fprintf(stderr, "[menu] pipeline creation failed\n"); return false; }

    VkDescriptorPoolSize ps = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4};
    VkDescriptorPoolCreateInfo poolCI = {};
    poolCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolCI.maxSets = 1;
    poolCI.poolSizeCount = 1;
    poolCI.pPoolSizes = &ps;
    VkDescriptorPool pool;
    vkCreateDescriptorPool(ctx.device, &poolCI, nullptr, &pool);
    VkDescriptorSetAllocateInfo ai = {};
    ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool = pool;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts = &dsl_;
    vkAllocateDescriptorSets(ctx.device, &ai, &set_);
    VkDescriptorImageInfo di = {sampler_, view_, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkWriteDescriptorSet wr = {};
    wr.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    wr.dstSet = set_;
    wr.dstBinding = 0;
    wr.descriptorCount = 1;
    wr.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    wr.pImageInfo = &di;
    vkUpdateDescriptorSets(ctx.device, 1, &wr, 0, nullptr);
    mpool_ = pool;

    // Vulkan 的 NDC (-1,-1) 在左上角，贴图第 0 行也在顶部，v 随屏幕向下递增。
    struct QV { float x, y, u, v, r, g, b, a; };
    QV quad[6] = {
        {-1,-1,0,0,1,1,1,1},{1,-1,1,0,1,1,1,1},{1,1,1,1,1,1,1,1},
        {-1,-1,0,0,1,1,1,1},{1,1,1,1,1,1,1,1},{-1,1,0,1,1,1,1,1},
    };
    Buffer2 qb;
    if (!gfxutil::createBuffer(ctx, sizeof(quad), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, qb)) {
        return false;
    }
    quadBuf_ = qb.b;
    quadMem_ = qb.m;
    void* qp;
    vkMapMemory(ctx.device, quadMem_, 0, sizeof(quad), 0, &qp);
    memcpy(qp, quad, sizeof(quad));
    vkUnmapMemory(ctx.device, quadMem_);
    return true;
}

bool Menu::LoadTextures(const std::string& assetDir) {
    auto load = [&](const std::string& rel, std::vector<uint8_t>& out, int& w, int& h) {
        return loadPNG((assetDir + "/" + rel).c_str(), out, w, h);
    };
    load("block/dirt.png", dirtRGBA_, dirtW_, dirtH_);
    if (!load("gui/button.png", btnRGBA_, btnW_, btnH_)) return false;
    if (!load("gui/button_highlighted.png", btnHiRGBA_, btnW_, btnH_)) return false;
    if (!load("gui/text_field.png", fieldRGBA_, fieldW_, fieldH_)) return false;
    if (!load("gui/slider.png", sliderRGBA_, sliderW_, sliderH_)) return false;
    if (!load("gui/slider_handle.png", handleRGBA_, handleW_, handleH_)) return false;
    if (!load("gui/slider_handle_highlighted.png", handleHiRGBA_, handleW_, handleH_)) return false;
    return true;
}

void Menu::CreateMenuTexture(VkCtx& ctx) {
    if (!gfxutil::createImage(ctx, (uint32_t)canvasW_, (uint32_t)canvasH_, VK_FORMAT_R8G8B8A8_SRGB,
                              VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, img_, imgMem_)) return;
    Buffer2 st;
    if (!gfxutil::createBuffer(ctx, (VkDeviceSize)canvasW_ * canvasH_ * 4,
                               VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, st)) return;
    staging_ = st.b;
    stagingMem_ = st.m;
    view_ = gfxutil::create2DView(ctx.device, img_, VK_FORMAT_R8G8B8A8_SRGB);

    VkSamplerCreateInfo sm = {};
    sm.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sm.magFilter = VK_FILTER_LINEAR;
    sm.minFilter = VK_FILTER_LINEAR;
    sm.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sm.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    vkCreateSampler(ctx.device, &sm, nullptr, &sampler_);

    if (set_ != VK_NULL_HANDLE) {
        VkDescriptorImageInfo di = {sampler_, view_, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkWriteDescriptorSet wr = {};
        wr.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        wr.dstSet = set_;
        wr.dstBinding = 0;
        wr.descriptorCount = 1;
        wr.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        wr.pImageInfo = &di;
        vkUpdateDescriptorSets(ctx.device, 1, &wr, 0, nullptr);
    }
}

void Menu::DestroyMenuTexture(VkCtx& ctx) {
    vkDeviceWaitIdle(ctx.device);
    if (view_) vkDestroyImageView(ctx.device, view_, nullptr);
    if (img_) vkDestroyImage(ctx.device, img_, nullptr);
    if (imgMem_) vkFreeMemory(ctx.device, imgMem_, nullptr);
    if (sampler_) vkDestroySampler(ctx.device, sampler_, nullptr);
    if (staging_) vkDestroyBuffer(ctx.device, staging_, nullptr);
    if (stagingMem_) vkFreeMemory(ctx.device, stagingMem_, nullptr);
    img_ = VK_NULL_HANDLE;
    view_ = VK_NULL_HANDLE;
    sampler_ = VK_NULL_HANDLE;
    staging_ = VK_NULL_HANDLE;
    imgMem_ = VK_NULL_HANDLE;
    stagingMem_ = VK_NULL_HANDLE;
    texReady_ = false;
}

void Menu::EnsureCanvas(int w, int h) {
    if (canvasW_ == w && canvasH_ == h && !canvas_.empty()) return;
    canvasW_ = w;
    canvasH_ = h;
    canvas_.assign((size_t)w * h * 4, 0);
}

void Menu::onResize(VkCtx& ctx, int w, int h) {
    DestroyMenuTexture(ctx);
    EnsureCanvas(w, h);
    CreateMenuTexture(ctx);
}

void Menu::RenderCanvas(Menuscreen screen, const MenuData& data, float cx, float cy) {
    // 泥土背景平铺，放大约 4 倍。
    int tile = dirtW_ * 4;
    if (tile > 0) {
        for (int y = 0; y < canvasH_; y += tile)
            for (int x = 0; x < canvasW_; x += tile)
                BlitImage(canvas_, canvasW_, canvasH_, dirtRGBA_, dirtW_, dirtH_, 0, 0, x, y, tile, tile);
    }
    btns_.clear();
    sliders_.clear();
    int w = canvasW_, h = canvasH_;

    auto drawText = [&](const std::string& text, int x, int y, uint32_t argb,
                        bool centered = false, int boxW = 0, int scale = kTextScale) {
        int px = x;
        if (centered) px += (boxW - (int)font_.Width(text, scale)) / 2;
        font_.DrawString(canvas_, canvasW_, canvasH_, text, px, y, argb, scale);
    };

    auto addBtn = [&](int id, float x, float y, float bw, float bh, const std::string& text) {
        bool hover = cx >= x && cx <= x + bw && cy >= y && cy <= y + bh;
        Slice9(canvas_, canvasW_, canvasH_, hover ? btnHiRGBA_ : btnRGBA_,
               btnW_, btnH_, kBtnBorder, (int)x, (int)y, (int)bw, (int)bh);
        drawText(text, (int)x, (int)y + (int)(bh - font_.LineHeight(kTextScale)) / 2,
                 0xFFFFFFFF, true, (int)bw);
        btns_.push_back({id, x, y, bw, bh});
    };

    auto addSlider = [&](int id, float x, float y, float bw, float bh,
                         int minV, int maxV, int* val, const char* label) {
        sliders_.push_back({id, x, y, bw, bh, minV, maxV, val, label});
        Slice9(canvas_, canvasW_, canvasH_, sliderRGBA_, sliderW_, sliderH_,
               kBtnBorder, (int)x, (int)y, (int)bw, (int)bh);
        int valv = *val;
        valv = std::max(minV, std::min(maxV, valv));
        float frac = (float)(valv - minV) / (float)(maxV - minV);
        frac = std::max(0.0f, std::min(1.0f, frac));
        int hx = (int)(x + frac * (bw - handleW_));
        int hy = (int)(y + (bh - handleH_) * 0.5f);
        bool hover = cx >= hx && cx <= hx + handleW_ && cy >= hy && cy <= hy + handleH_;
        BlitImage(canvas_, canvasW_, canvasH_, hover ? handleHiRGBA_ : handleRGBA_,
                  handleW_, handleH_, 0, 0, hx, hy, handleW_, handleH_);
        drawText(std::string(label) + "：" + std::to_string(valv),
                 (int)x, (int)(y + bh + 4), 0xFFFFFFFF, true, (int)bw);
    };

    if (screen == Menuscreen::MainMenu) {
        float bw = 400, bh = 40, bx = (w - bw) / 2;
        addBtn(MENU_SINGLEPLAYER, bx, 210, bw, bh, "单人游戏");
        float bw2 = 190, bx2 = (w - 2 * bw2 - 20) / 2;
        addBtn(MENU_OPTIONS, bx2, 280, bw2, bh, "选项");
        addBtn(MENU_QUIT, bx2 + bw2 + 20, 280, bw2, bh, "退出游戏");
        // 版本号右对齐。
        std::string ver = "Opencraft " + version::full();
        font_.DrawString(canvas_, canvasW_, canvasH_, ver, w - 8 - (int)font_.Width(ver, 1), h - 24,
                       0xFFD2D2D2, 1);
    } else if (screen == Menuscreen::SaveSelect) {
        float bw = 400, bh = 36, bx = (w - bw) / 2;
        addBtn(MENU_NEWWORLD, bx, 90, bw, 40, "新建世界");
        int y = 160;
        for (size_t i = 0; i < data.saves.size() && y < h - 90; i++) {
            addBtn(MENU_SAVE_FIRST + (int)i, bx, (float)y, bw, bh, data.saves[i].name);
            y += (int)bh + 8;
        }
        addBtn(MENU_BACK, bx, (float)(h - 70), bw, 40, "返回");
    } else if (screen == Menuscreen::NewWorld) {
        float bw = 400, bh = 40, bx = (w - bw) / 2;
        drawText(data.titleText, (int)bx, 90, 0xFFFFFFFF, true, (int)bw);
        drawText("世界种子", (int)bx, 183, 0xFFFFFFFF);
        float fx = bx + 120, fy = 190, fw = bw - 120, fh = 22;
        Slice9(canvas_, canvasW_, canvasH_, fieldRGBA_, fieldW_, fieldH_,
               3, (int)fx, (int)fy, (int)fw, (int)fh);
        drawText(data.seedText, (int)fx + 6, (int)fy + 3, 0xFFFFFFFF);
        addBtn(MENU_CREATE, bx, 280, bw, bh, "创建新的世界");
        addBtn(MENU_CANCEL, bx, 340, bw, bh, "取消");
    } else if (screen == Menuscreen::Options) {
        float bw = 300, bh = 40, bx = (w - bw) / 2;
        addBtn(MENU_VIDEO, bx, 260, bw, bh, "视频设置");
        addBtn(MENU_BACK, bx, (float)(h - 90), bw, bh, "返回");
    } else if (screen == Menuscreen::VideoSettings) {
        float bw = 300, bh = 40, bx = (w - bw) / 2;
        addBtn(MENU_VSYNC, bx, 150, bw, bh,
               std::string("垂直同步：") + (data.vsync ? "开" : "关"));
        float gbw = 660, gbx = (w - gbw) / 2;
        std::string gpuLabel = "显卡：";
        if (data.gpuIndex < 0 || data.gpuIndex >= (int)data.gpuNames.size())
            gpuLabel += "自动（优先独显）";
        else gpuLabel += data.gpuNames[data.gpuIndex];
        if (data.gpuIndex >= 0 && data.gpuIndex != data.gpuActive) gpuLabel += " ＊";
        addBtn(MENU_GPU, gbx, 210, gbw, bh, gpuLabel);
        drawText("切换显卡后重新启动游戏才会生效", (int)gbx, 256, 0xFFBEBEBE, true, (int)gbw);
        if (data.renderDistPtr)
            addSlider(MENU_RENDERDIST, bx, 310, bw, 20, data.renderDistMin,
                      data.renderDistMax, data.renderDistPtr, "渲染距离");
        if (data.texSharpPtr)
            addSlider(MENU_TEXSHARP, bx, 380, bw, 20, data.texSharpMin,
                      data.texSharpMax, data.texSharpPtr, "近景清晰");
        addBtn(MENU_BACK, bx, (float)(h - 90), bw, bh, "返回");
    } else if (screen == Menuscreen::Pause) {
        float bw = 400, bh = 40, bx = (w - bw) / 2;
        addBtn(MENU_SAVEANDTITLE, bx, 230, bw, bh, "保存并返回标题屏幕");
        addBtn(MENU_OPTIONS, bx, 290, bw, bh, "选项");
    }
}

int Menu::HitTest(float cx, float cy) const {
    for (auto& b : btns_)
        if (cx >= b.x && cx <= b.x + b.w && cy >= b.y && cy <= b.y + b.h) return b.id;
    return MENU_NONE;
}

void Menu::UploadAndDraw(VkCtx& ctx) {
    if (frameIdx_ > 0) {
        uint32_t pslot = (uint32_t)((frameIdx_ - 1) % (uint64_t)VkCtx::MAX_FRAMES_IN_FLIGHT);
        vkWaitForFences(ctx.device, 1, &ctx.frames[pslot].fence, VK_TRUE, UINT64_MAX);
    }
    void* p;
    if (vkMapMemory(ctx.device, stagingMem_, 0, VK_WHOLE_SIZE, 0, &p) == VK_SUCCESS) {
        memcpy(p, canvas_.data(), canvas_.size());
        vkUnmapMemory(ctx.device, stagingMem_);
    }
    uint32_t imageIndex;
    uint32_t slot = (uint32_t)(frameIdx_ % (uint64_t)VkCtx::MAX_FRAMES_IN_FLIGHT);
    VkCtx::Frame& f = ctx.frames[slot];
    if (!ctx.acquireNext(f, imageIndex)) { ctx.recreateSwapchain(canvasW_, canvasH_); return; }
    frameIdx_++;

    VkCommandBuffer cb = ctx.cmds[slot];
    vkResetCommandBuffer(cb, 0);
    VkCommandBufferBeginInfo bi = {};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(cb, &bi);

    VkImageMemoryBarrier bar = {};
    bar.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    bar.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    bar.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    bar.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bar.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bar.image = img_;
    bar.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &bar);
    VkBufferImageCopy bic = {};
    bic.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    bic.imageExtent = {(uint32_t)canvasW_, (uint32_t)canvasH_, 1};
    vkCmdCopyBufferToImage(cb, staging_, img_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bic);
    bar.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    bar.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &bar);

    VkClearValue clear = {};
    clear.color = {{0, 0, 0, 1}};
    VkRenderPassBeginInfo rp = {};
    rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp.renderPass = ctx.renderPass;
    rp.framebuffer = ctx.framebuffers[imageIndex];
    rp.renderArea = {{0, 0}, ctx.extent};
    rp.clearValueCount = 1;
    rp.pClearValues = &clear;
    vkCmdBeginRenderPass(cb, &rp, VK_SUBPASS_CONTENTS_INLINE);
    VkViewport vp = {0, 0, (float)ctx.extent.width, (float)ctx.extent.height, 0, 1};
    vkCmdSetViewport(cb, 0, 1, &vp);
    VkRect2D sc = {{0, 0}, ctx.extent};
    vkCmdSetScissor(cb, 0, 1, &sc);
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe_);
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, layout_, 0, 1, &set_, 0, nullptr);
    VkBuffer qb = quadBuf_;
    VkDeviceSize off = 0;
    vkCmdBindVertexBuffers(cb, 0, 1, &qb, &off);
    vkCmdDraw(cb, 6, 1, 0, 0);
    vkCmdEndRenderPass(cb);
    vkEndCommandBuffer(cb);
    if (!ctx.presentImage(imageIndex, f, cb)) ctx.recreateSwapchain(canvasW_, canvasH_);
}

int Menu::renderMenu(VkCtx& ctx, Menuscreen screen, const MenuData& data,
                     float cx, float cy, bool mouseDown) {
    if ((int)ctx.extent.width != canvasW_ || (int)ctx.extent.height != canvasH_) {
        DestroyMenuTexture(ctx);
        EnsureCanvas((int)ctx.extent.width, (int)ctx.extent.height);
        CreateMenuTexture(ctx);
    }
    RenderCanvas(screen, data, cx, cy);
    UploadAndDraw(ctx);

    if (!sliders_.empty()) {
        for (size_t i = 0; i < sliders_.size(); i++) {
            const Slider& s = sliders_[i];
            bool over = cx >= s.x && cx <= s.x + s.w && cy >= s.y && cy <= s.y + s.h;
            if (mouseDown && (over || dragSlider_ == (int)i)) {
                float frac = (cx - s.x) / s.w;
                frac = std::max(0.0f, std::min(1.0f, frac));
                int val = s.minV + (int)std::lround(frac * (s.maxV - s.minV));
                if (s.val) *s.val = std::max(s.minV, std::min(s.maxV, val));
                sliderDragging_ = true;
                dragSlider_ = (int)i;
                prevMouseDown_ = mouseDown;
                return s.id;
            }
        }
    }
    sliderDragging_ = false;
    dragSlider_ = -1;
    if (mouseDown && !prevMouseDown_) { prevMouseDown_ = true; return HitTest(cx, cy); }
    prevMouseDown_ = mouseDown;
    return MENU_NONE;
}

bool Menu::debugSaveMenu(const std::string& path) const {
    if (canvas_.empty()) return false;
    std::vector<uint8_t> bgra(canvas_.size());
    for (size_t i = 0; i < canvas_.size(); i += 4) {
        bgra[i] = canvas_[i + 2];
        bgra[i + 1] = canvas_[i + 1];
        bgra[i + 2] = canvas_[i];
        bgra[i + 3] = canvas_[i + 3];
    }
    return savePNG(path, canvasW_, canvasH_, bgra);
}

void Menu::shutdown(VkCtx& ctx) {
    DestroyMenuTexture(ctx);
    vkDeviceWaitIdle(ctx.device);
    if (quadBuf_) vkDestroyBuffer(ctx.device, quadBuf_, nullptr);
    if (quadMem_) vkFreeMemory(ctx.device, quadMem_, nullptr);
    if (mpool_) vkDestroyDescriptorPool(ctx.device, mpool_, nullptr);
    if (dsl_) vkDestroyDescriptorSetLayout(ctx.device, dsl_, nullptr);
    if (layout_) vkDestroyPipelineLayout(ctx.device, layout_, nullptr);
    if (pipe_) vkDestroyPipeline(ctx.device, pipe_, nullptr);
    quadBuf_ = VK_NULL_HANDLE;
    quadMem_ = VK_NULL_HANDLE;
    mpool_ = VK_NULL_HANDLE;
    dsl_ = VK_NULL_HANDLE;
    layout_ = VK_NULL_HANDLE;
    pipe_ = VK_NULL_HANDLE;
}
