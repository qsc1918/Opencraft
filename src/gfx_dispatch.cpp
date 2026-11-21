#define GFX_SHIM_IMPL
#include "gfx_shim.hpp"
#include "vkutil.hpp"
#include <cstdio>

GfxBackend g_gfxBackend = GfxBackend::Vulkan;

GfxDispatchTable g_gfxD3D11Table;
GfxDispatchTable g_gfxD3D12Table;
GfxDispatchTable g_gfxD3D9Table;

const GfxDispatchTable* gfxTableFor(GfxBackend backend) {
    if (backend == GfxBackend::D3D11) return &g_gfxD3D11Table;
    if (backend == GfxBackend::D3D12) return &g_gfxD3D12Table;
    if (backend == GfxBackend::D3D9) return &g_gfxD3D9Table;
    return nullptr;
}

VkResult gfxCreateImage(VkDevice a0, const VkImageCreateInfo* a1, const VkAllocationCallbacks* a2, VkImage* a3) {
    if (g_gfxBackend == GfxBackend::Vulkan) return vkCreateImage(a0, a1, a2, a3);
    if (g_gfxBackend == GfxBackend::OpenGL || g_gfxBackend == GfxBackend::Angle) return gfxgl::gfxCreateImage(a0, a1, a2, a3);
    if (const auto* table = gfxTableFor(g_gfxBackend)) if (table->createImage) return table->createImage(a0, a1, a2, a3);
    fprintf(stderr, "[gfx] 后端未实现 gfxCreateImage\n");
    return VK_ERROR_FEATURE_NOT_PRESENT;
}

void gfxDestroyBuffer(VkDevice a0, VkBuffer a1, const VkAllocationCallbacks* a2) {
    if (g_gfxBackend == GfxBackend::Vulkan) return vkDestroyBuffer(a0, a1, a2);
    if (g_gfxBackend == GfxBackend::OpenGL || g_gfxBackend == GfxBackend::Angle) return gfxgl::gfxDestroyBuffer(a0, a1, a2);
    if (const auto* table = gfxTableFor(g_gfxBackend)) if (table->destroyBuffer) return table->destroyBuffer(a0, a1, a2);
    fprintf(stderr, "[gfx] 后端未实现 gfxDestroyBuffer\n");
    return;
}

void gfxDestroyImage(VkDevice a0, VkImage a1, const VkAllocationCallbacks* a2) {
    if (g_gfxBackend == GfxBackend::Vulkan) return vkDestroyImage(a0, a1, a2);
    if (g_gfxBackend == GfxBackend::OpenGL || g_gfxBackend == GfxBackend::Angle) return gfxgl::gfxDestroyImage(a0, a1, a2);
    if (const auto* table = gfxTableFor(g_gfxBackend)) if (table->destroyImage) return table->destroyImage(a0, a1, a2);
    fprintf(stderr, "[gfx] 后端未实现 gfxDestroyImage\n");
    return;
}

void gfxGetImageMemoryRequirements(VkDevice a0, VkImage a1, VkMemoryRequirements* a2) {
    if (g_gfxBackend == GfxBackend::Vulkan) return vkGetImageMemoryRequirements(a0, a1, a2);
    if (g_gfxBackend == GfxBackend::OpenGL || g_gfxBackend == GfxBackend::Angle) return gfxgl::gfxGetImageMemoryRequirements(a0, a1, a2);
    if (const auto* table = gfxTableFor(g_gfxBackend)) if (table->getImageMemoryRequirements) return table->getImageMemoryRequirements(a0, a1, a2);
    fprintf(stderr, "[gfx] 后端未实现 gfxGetImageMemoryRequirements\n");
    return;
}

VkResult gfxAllocateMemory(VkDevice a0, const VkMemoryAllocateInfo* a1, const VkAllocationCallbacks* a2, VkDeviceMemory* a3) {
    if (g_gfxBackend == GfxBackend::Vulkan) return vkAllocateMemory(a0, a1, a2, a3);
    if (g_gfxBackend == GfxBackend::OpenGL || g_gfxBackend == GfxBackend::Angle) return gfxgl::gfxAllocateMemory(a0, a1, a2, a3);
    if (const auto* table = gfxTableFor(g_gfxBackend)) if (table->allocateMemory) return table->allocateMemory(a0, a1, a2, a3);
    fprintf(stderr, "[gfx] 后端未实现 gfxAllocateMemory\n");
    return VK_ERROR_FEATURE_NOT_PRESENT;
}

void gfxFreeMemory(VkDevice a0, VkDeviceMemory a1, const VkAllocationCallbacks* a2) {
    if (g_gfxBackend == GfxBackend::Vulkan) return vkFreeMemory(a0, a1, a2);
    if (g_gfxBackend == GfxBackend::OpenGL || g_gfxBackend == GfxBackend::Angle) return gfxgl::gfxFreeMemory(a0, a1, a2);
    if (const auto* table = gfxTableFor(g_gfxBackend)) if (table->freeMemory) return table->freeMemory(a0, a1, a2);
    fprintf(stderr, "[gfx] 后端未实现 gfxFreeMemory\n");
    return;
}

VkResult gfxBindImageMemory(VkDevice a0, VkImage a1, VkDeviceMemory a2, VkDeviceSize a3) {
    if (g_gfxBackend == GfxBackend::Vulkan) return vkBindImageMemory(a0, a1, a2, a3);
    if (g_gfxBackend == GfxBackend::OpenGL || g_gfxBackend == GfxBackend::Angle) return gfxgl::gfxBindImageMemory(a0, a1, a2, a3);
    if (const auto* table = gfxTableFor(g_gfxBackend)) if (table->bindImageMemory) return table->bindImageMemory(a0, a1, a2, a3);
    fprintf(stderr, "[gfx] 后端未实现 gfxBindImageMemory\n");
    return VK_ERROR_FEATURE_NOT_PRESENT;
}

VkResult gfxMapMemory(VkDevice a0, VkDeviceMemory a1, VkDeviceSize a2, VkDeviceSize a3, VkMemoryMapFlags a4, void** a5) {
    if (g_gfxBackend == GfxBackend::Vulkan) return vkMapMemory(a0, a1, a2, a3, a4, a5);
    if (g_gfxBackend == GfxBackend::OpenGL || g_gfxBackend == GfxBackend::Angle) return gfxgl::gfxMapMemory(a0, a1, a2, a3, a4, a5);
    if (const auto* table = gfxTableFor(g_gfxBackend)) if (table->mapMemory) return table->mapMemory(a0, a1, a2, a3, a4, a5);
    fprintf(stderr, "[gfx] 后端未实现 gfxMapMemory\n");
    return VK_ERROR_FEATURE_NOT_PRESENT;
}

void gfxUnmapMemory(VkDevice a0, VkDeviceMemory a1) {
    if (g_gfxBackend == GfxBackend::Vulkan) return vkUnmapMemory(a0, a1);
    if (g_gfxBackend == GfxBackend::OpenGL || g_gfxBackend == GfxBackend::Angle) return gfxgl::gfxUnmapMemory(a0, a1);
    if (const auto* table = gfxTableFor(g_gfxBackend)) if (table->unmapMemory) return table->unmapMemory(a0, a1);
    fprintf(stderr, "[gfx] 后端未实现 gfxUnmapMemory\n");
    return;
}

VkResult gfxCreateDescriptorSetLayout(VkDevice a0, const VkDescriptorSetLayoutCreateInfo* a1, const VkAllocationCallbacks* a2, VkDescriptorSetLayout* a3) {
    if (g_gfxBackend == GfxBackend::Vulkan) return vkCreateDescriptorSetLayout(a0, a1, a2, a3);
    if (g_gfxBackend == GfxBackend::OpenGL || g_gfxBackend == GfxBackend::Angle) return gfxgl::gfxCreateDescriptorSetLayout(a0, a1, a2, a3);
    if (const auto* table = gfxTableFor(g_gfxBackend)) if (table->createDescriptorSetLayout) return table->createDescriptorSetLayout(a0, a1, a2, a3);
    fprintf(stderr, "[gfx] 后端未实现 gfxCreateDescriptorSetLayout\n");
    return VK_ERROR_FEATURE_NOT_PRESENT;
}

void gfxDestroyDescriptorSetLayout(VkDevice a0, VkDescriptorSetLayout a1, const VkAllocationCallbacks* a2) {
    if (g_gfxBackend == GfxBackend::Vulkan) return vkDestroyDescriptorSetLayout(a0, a1, a2);
    if (g_gfxBackend == GfxBackend::OpenGL || g_gfxBackend == GfxBackend::Angle) return gfxgl::gfxDestroyDescriptorSetLayout(a0, a1, a2);
    if (const auto* table = gfxTableFor(g_gfxBackend)) if (table->destroyDescriptorSetLayout) return table->destroyDescriptorSetLayout(a0, a1, a2);
    fprintf(stderr, "[gfx] 后端未实现 gfxDestroyDescriptorSetLayout\n");
    return;
}

VkResult gfxCreateDescriptorPool(VkDevice a0, const VkDescriptorPoolCreateInfo* a1, const VkAllocationCallbacks* a2, VkDescriptorPool* a3) {
    if (g_gfxBackend == GfxBackend::Vulkan) return vkCreateDescriptorPool(a0, a1, a2, a3);
    if (g_gfxBackend == GfxBackend::OpenGL || g_gfxBackend == GfxBackend::Angle) return gfxgl::gfxCreateDescriptorPool(a0, a1, a2, a3);
    if (const auto* table = gfxTableFor(g_gfxBackend)) if (table->createDescriptorPool) return table->createDescriptorPool(a0, a1, a2, a3);
    fprintf(stderr, "[gfx] 后端未实现 gfxCreateDescriptorPool\n");
    return VK_ERROR_FEATURE_NOT_PRESENT;
}

void gfxDestroyDescriptorPool(VkDevice a0, VkDescriptorPool a1, const VkAllocationCallbacks* a2) {
    if (g_gfxBackend == GfxBackend::Vulkan) return vkDestroyDescriptorPool(a0, a1, a2);
    if (g_gfxBackend == GfxBackend::OpenGL || g_gfxBackend == GfxBackend::Angle) return gfxgl::gfxDestroyDescriptorPool(a0, a1, a2);
    if (const auto* table = gfxTableFor(g_gfxBackend)) if (table->destroyDescriptorPool) return table->destroyDescriptorPool(a0, a1, a2);
    fprintf(stderr, "[gfx] 后端未实现 gfxDestroyDescriptorPool\n");
    return;
}

VkResult gfxAllocateDescriptorSets(VkDevice a0, const VkDescriptorSetAllocateInfo* a1, VkDescriptorSet* a2) {
    if (g_gfxBackend == GfxBackend::Vulkan) return vkAllocateDescriptorSets(a0, a1, a2);
    if (g_gfxBackend == GfxBackend::OpenGL || g_gfxBackend == GfxBackend::Angle) return gfxgl::gfxAllocateDescriptorSets(a0, a1, a2);
    if (const auto* table = gfxTableFor(g_gfxBackend)) if (table->allocateDescriptorSets) return table->allocateDescriptorSets(a0, a1, a2);
    fprintf(stderr, "[gfx] 后端未实现 gfxAllocateDescriptorSets\n");
    return VK_ERROR_FEATURE_NOT_PRESENT;
}

void gfxUpdateDescriptorSets(VkDevice a0, uint32_t a1, const VkWriteDescriptorSet* a2, uint32_t a3, const VkCopyDescriptorSet* a4) {
    if (g_gfxBackend == GfxBackend::Vulkan) return vkUpdateDescriptorSets(a0, a1, a2, a3, a4);
    if (g_gfxBackend == GfxBackend::OpenGL || g_gfxBackend == GfxBackend::Angle) return gfxgl::gfxUpdateDescriptorSets(a0, a1, a2, a3, a4);
    if (const auto* table = gfxTableFor(g_gfxBackend)) if (table->updateDescriptorSets) return table->updateDescriptorSets(a0, a1, a2, a3, a4);
    fprintf(stderr, "[gfx] 后端未实现 gfxUpdateDescriptorSets\n");
    return;
}

VkResult gfxCreatePipelineLayout(VkDevice a0, const VkPipelineLayoutCreateInfo* a1, const VkAllocationCallbacks* a2, VkPipelineLayout* a3) {
    if (g_gfxBackend == GfxBackend::Vulkan) return vkCreatePipelineLayout(a0, a1, a2, a3);
    if (g_gfxBackend == GfxBackend::OpenGL || g_gfxBackend == GfxBackend::Angle) return gfxgl::gfxCreatePipelineLayout(a0, a1, a2, a3);
    if (const auto* table = gfxTableFor(g_gfxBackend)) if (table->createPipelineLayout) return table->createPipelineLayout(a0, a1, a2, a3);
    fprintf(stderr, "[gfx] 后端未实现 gfxCreatePipelineLayout\n");
    return VK_ERROR_FEATURE_NOT_PRESENT;
}

void gfxDestroyPipelineLayout(VkDevice a0, VkPipelineLayout a1, const VkAllocationCallbacks* a2) {
    if (g_gfxBackend == GfxBackend::Vulkan) return vkDestroyPipelineLayout(a0, a1, a2);
    if (g_gfxBackend == GfxBackend::OpenGL || g_gfxBackend == GfxBackend::Angle) return gfxgl::gfxDestroyPipelineLayout(a0, a1, a2);
    if (const auto* table = gfxTableFor(g_gfxBackend)) if (table->destroyPipelineLayout) return table->destroyPipelineLayout(a0, a1, a2);
    fprintf(stderr, "[gfx] 后端未实现 gfxDestroyPipelineLayout\n");
    return;
}

void gfxDestroyPipeline(VkDevice a0, VkPipeline a1, const VkAllocationCallbacks* a2) {
    if (g_gfxBackend == GfxBackend::Vulkan) return vkDestroyPipeline(a0, a1, a2);
    if (g_gfxBackend == GfxBackend::OpenGL || g_gfxBackend == GfxBackend::Angle) return gfxgl::gfxDestroyPipeline(a0, a1, a2);
    if (const auto* table = gfxTableFor(g_gfxBackend)) if (table->destroyPipeline) return table->destroyPipeline(a0, a1, a2);
    fprintf(stderr, "[gfx] 后端未实现 gfxDestroyPipeline\n");
    return;
}

VkResult gfxCreateSampler(VkDevice a0, const VkSamplerCreateInfo* a1, const VkAllocationCallbacks* a2, VkSampler* a3) {
    if (g_gfxBackend == GfxBackend::Vulkan) return vkCreateSampler(a0, a1, a2, a3);
    if (g_gfxBackend == GfxBackend::OpenGL || g_gfxBackend == GfxBackend::Angle) return gfxgl::gfxCreateSampler(a0, a1, a2, a3);
    if (const auto* table = gfxTableFor(g_gfxBackend)) if (table->createSampler) return table->createSampler(a0, a1, a2, a3);
    fprintf(stderr, "[gfx] 后端未实现 gfxCreateSampler\n");
    return VK_ERROR_FEATURE_NOT_PRESENT;
}

void gfxDestroySampler(VkDevice a0, VkSampler a1, const VkAllocationCallbacks* a2) {
    if (g_gfxBackend == GfxBackend::Vulkan) return vkDestroySampler(a0, a1, a2);
    if (g_gfxBackend == GfxBackend::OpenGL || g_gfxBackend == GfxBackend::Angle) return gfxgl::gfxDestroySampler(a0, a1, a2);
    if (const auto* table = gfxTableFor(g_gfxBackend)) if (table->destroySampler) return table->destroySampler(a0, a1, a2);
    fprintf(stderr, "[gfx] 后端未实现 gfxDestroySampler\n");
    return;
}

VkResult gfxCreateImageView(VkDevice a0, const VkImageViewCreateInfo* a1, const VkAllocationCallbacks* a2, VkImageView* a3) {
    if (g_gfxBackend == GfxBackend::Vulkan) return vkCreateImageView(a0, a1, a2, a3);
    if (g_gfxBackend == GfxBackend::OpenGL || g_gfxBackend == GfxBackend::Angle) return gfxgl::gfxCreateImageView(a0, a1, a2, a3);
    if (const auto* table = gfxTableFor(g_gfxBackend)) if (table->createImageView) return table->createImageView(a0, a1, a2, a3);
    fprintf(stderr, "[gfx] 后端未实现 gfxCreateImageView\n");
    return VK_ERROR_FEATURE_NOT_PRESENT;
}

void gfxDestroyImageView(VkDevice a0, VkImageView a1, const VkAllocationCallbacks* a2) {
    if (g_gfxBackend == GfxBackend::Vulkan) return vkDestroyImageView(a0, a1, a2);
    if (g_gfxBackend == GfxBackend::OpenGL || g_gfxBackend == GfxBackend::Angle) return gfxgl::gfxDestroyImageView(a0, a1, a2);
    if (const auto* table = gfxTableFor(g_gfxBackend)) if (table->destroyImageView) return table->destroyImageView(a0, a1, a2);
    fprintf(stderr, "[gfx] 后端未实现 gfxDestroyImageView\n");
    return;
}

VkResult gfxResetCommandBuffer(VkCommandBuffer a0, VkCommandBufferResetFlags a1) {
    if (g_gfxBackend == GfxBackend::Vulkan) return vkResetCommandBuffer(a0, a1);
    if (g_gfxBackend == GfxBackend::OpenGL || g_gfxBackend == GfxBackend::Angle) return gfxgl::gfxResetCommandBuffer(a0, a1);
    if (const auto* table = gfxTableFor(g_gfxBackend)) if (table->resetCommandBuffer) return table->resetCommandBuffer(a0, a1);
    fprintf(stderr, "[gfx] 后端未实现 gfxResetCommandBuffer\n");
    return VK_ERROR_FEATURE_NOT_PRESENT;
}

VkResult gfxBeginCommandBuffer(VkCommandBuffer a0, const VkCommandBufferBeginInfo* a1) {
    if (g_gfxBackend == GfxBackend::Vulkan) return vkBeginCommandBuffer(a0, a1);
    if (g_gfxBackend == GfxBackend::OpenGL || g_gfxBackend == GfxBackend::Angle) return gfxgl::gfxBeginCommandBuffer(a0, a1);
    if (const auto* table = gfxTableFor(g_gfxBackend)) if (table->beginCommandBuffer) return table->beginCommandBuffer(a0, a1);
    fprintf(stderr, "[gfx] 后端未实现 gfxBeginCommandBuffer\n");
    return VK_ERROR_FEATURE_NOT_PRESENT;
}

VkResult gfxEndCommandBuffer(VkCommandBuffer a0) {
    if (g_gfxBackend == GfxBackend::Vulkan) return vkEndCommandBuffer(a0);
    if (g_gfxBackend == GfxBackend::OpenGL || g_gfxBackend == GfxBackend::Angle) return gfxgl::gfxEndCommandBuffer(a0);
    if (const auto* table = gfxTableFor(g_gfxBackend)) if (table->endCommandBuffer) return table->endCommandBuffer(a0);
    fprintf(stderr, "[gfx] 后端未实现 gfxEndCommandBuffer\n");
    return VK_ERROR_FEATURE_NOT_PRESENT;
}

void gfxCmdPipelineBarrier(VkCommandBuffer a0, VkPipelineStageFlags a1, VkPipelineStageFlags a2, VkDependencyFlags a3, uint32_t a4, const VkMemoryBarrier* a5, uint32_t a6, const VkBufferMemoryBarrier* a7, uint32_t a8, const VkImageMemoryBarrier* a9) {
    if (g_gfxBackend == GfxBackend::Vulkan) return vkCmdPipelineBarrier(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9);
    if (g_gfxBackend == GfxBackend::OpenGL || g_gfxBackend == GfxBackend::Angle) return gfxgl::gfxCmdPipelineBarrier(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9);
    if (const auto* table = gfxTableFor(g_gfxBackend)) if (table->cmdPipelineBarrier) return table->cmdPipelineBarrier(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9);
    fprintf(stderr, "[gfx] 后端未实现 gfxCmdPipelineBarrier\n");
    return;
}

void gfxCmdCopyBufferToImage(VkCommandBuffer a0, VkBuffer a1, VkImage a2, VkImageLayout a3, uint32_t a4, const VkBufferImageCopy* a5) {
    if (g_gfxBackend == GfxBackend::Vulkan) return vkCmdCopyBufferToImage(a0, a1, a2, a3, a4, a5);
    if (g_gfxBackend == GfxBackend::OpenGL || g_gfxBackend == GfxBackend::Angle) return gfxgl::gfxCmdCopyBufferToImage(a0, a1, a2, a3, a4, a5);
    if (const auto* table = gfxTableFor(g_gfxBackend)) if (table->cmdCopyBufferToImage) return table->cmdCopyBufferToImage(a0, a1, a2, a3, a4, a5);
    fprintf(stderr, "[gfx] 后端未实现 gfxCmdCopyBufferToImage\n");
    return;
}

void gfxCmdCopyImageToBuffer(VkCommandBuffer a0, VkImage a1, VkImageLayout a2, VkBuffer a3, uint32_t a4, const VkBufferImageCopy* a5) {
    if (g_gfxBackend == GfxBackend::Vulkan) return vkCmdCopyImageToBuffer(a0, a1, a2, a3, a4, a5);
    if (g_gfxBackend == GfxBackend::OpenGL || g_gfxBackend == GfxBackend::Angle) return gfxgl::gfxCmdCopyImageToBuffer(a0, a1, a2, a3, a4, a5);
    if (const auto* table = gfxTableFor(g_gfxBackend)) if (table->cmdCopyImageToBuffer) return table->cmdCopyImageToBuffer(a0, a1, a2, a3, a4, a5);
    fprintf(stderr, "[gfx] 后端未实现 gfxCmdCopyImageToBuffer\n");
    return;
}

void gfxCmdBlitImage(VkCommandBuffer a0, VkImage a1, VkImageLayout a2, VkImage a3, VkImageLayout a4, uint32_t a5, const VkImageBlit* a6, VkFilter a7) {
    if (g_gfxBackend == GfxBackend::Vulkan) return vkCmdBlitImage(a0, a1, a2, a3, a4, a5, a6, a7);
    if (g_gfxBackend == GfxBackend::OpenGL || g_gfxBackend == GfxBackend::Angle) return gfxgl::gfxCmdBlitImage(a0, a1, a2, a3, a4, a5, a6, a7);
    if (const auto* table = gfxTableFor(g_gfxBackend)) if (table->cmdBlitImage) return table->cmdBlitImage(a0, a1, a2, a3, a4, a5, a6, a7);
    fprintf(stderr, "[gfx] 后端未实现 gfxCmdBlitImage\n");
    return;
}

void gfxCmdBeginRenderPass(VkCommandBuffer a0, const VkRenderPassBeginInfo* a1, VkSubpassContents a2) {
    if (g_gfxBackend == GfxBackend::Vulkan) return vkCmdBeginRenderPass(a0, a1, a2);
    if (g_gfxBackend == GfxBackend::OpenGL || g_gfxBackend == GfxBackend::Angle) return gfxgl::gfxCmdBeginRenderPass(a0, a1, a2);
    if (const auto* table = gfxTableFor(g_gfxBackend)) if (table->cmdBeginRenderPass) return table->cmdBeginRenderPass(a0, a1, a2);
    fprintf(stderr, "[gfx] 后端未实现 gfxCmdBeginRenderPass\n");
    return;
}

void gfxCmdEndRenderPass(VkCommandBuffer a0) {
    if (g_gfxBackend == GfxBackend::Vulkan) return vkCmdEndRenderPass(a0);
    if (g_gfxBackend == GfxBackend::OpenGL || g_gfxBackend == GfxBackend::Angle) return gfxgl::gfxCmdEndRenderPass(a0);
    if (const auto* table = gfxTableFor(g_gfxBackend)) if (table->cmdEndRenderPass) return table->cmdEndRenderPass(a0);
    fprintf(stderr, "[gfx] 后端未实现 gfxCmdEndRenderPass\n");
    return;
}

void gfxCmdSetViewport(VkCommandBuffer a0, uint32_t a1, uint32_t a2, const VkViewport* a3) {
    if (g_gfxBackend == GfxBackend::Vulkan) return vkCmdSetViewport(a0, a1, a2, a3);
    if (g_gfxBackend == GfxBackend::OpenGL || g_gfxBackend == GfxBackend::Angle) return gfxgl::gfxCmdSetViewport(a0, a1, a2, a3);
    if (const auto* table = gfxTableFor(g_gfxBackend)) if (table->cmdSetViewport) return table->cmdSetViewport(a0, a1, a2, a3);
    fprintf(stderr, "[gfx] 后端未实现 gfxCmdSetViewport\n");
    return;
}

void gfxCmdSetScissor(VkCommandBuffer a0, uint32_t a1, uint32_t a2, const VkRect2D* a3) {
    if (g_gfxBackend == GfxBackend::Vulkan) return vkCmdSetScissor(a0, a1, a2, a3);
    if (g_gfxBackend == GfxBackend::OpenGL || g_gfxBackend == GfxBackend::Angle) return gfxgl::gfxCmdSetScissor(a0, a1, a2, a3);
    if (const auto* table = gfxTableFor(g_gfxBackend)) if (table->cmdSetScissor) return table->cmdSetScissor(a0, a1, a2, a3);
    fprintf(stderr, "[gfx] 后端未实现 gfxCmdSetScissor\n");
    return;
}

void gfxCmdBindPipeline(VkCommandBuffer a0, VkPipelineBindPoint a1, VkPipeline a2) {
    if (g_gfxBackend == GfxBackend::Vulkan) return vkCmdBindPipeline(a0, a1, a2);
    if (g_gfxBackend == GfxBackend::OpenGL || g_gfxBackend == GfxBackend::Angle) return gfxgl::gfxCmdBindPipeline(a0, a1, a2);
    if (const auto* table = gfxTableFor(g_gfxBackend)) if (table->cmdBindPipeline) return table->cmdBindPipeline(a0, a1, a2);
    fprintf(stderr, "[gfx] 后端未实现 gfxCmdBindPipeline\n");
    return;
}

void gfxCmdBindDescriptorSets(VkCommandBuffer a0, VkPipelineBindPoint a1, VkPipelineLayout a2, uint32_t a3, uint32_t a4, const VkDescriptorSet* a5, uint32_t a6, const uint32_t* a7) {
    if (g_gfxBackend == GfxBackend::Vulkan) return vkCmdBindDescriptorSets(a0, a1, a2, a3, a4, a5, a6, a7);
    if (g_gfxBackend == GfxBackend::OpenGL || g_gfxBackend == GfxBackend::Angle) return gfxgl::gfxCmdBindDescriptorSets(a0, a1, a2, a3, a4, a5, a6, a7);
    if (const auto* table = gfxTableFor(g_gfxBackend)) if (table->cmdBindDescriptorSets) return table->cmdBindDescriptorSets(a0, a1, a2, a3, a4, a5, a6, a7);
    fprintf(stderr, "[gfx] 后端未实现 gfxCmdBindDescriptorSets\n");
    return;
}

void gfxCmdBindVertexBuffers(VkCommandBuffer a0, uint32_t a1, uint32_t a2, const VkBuffer* a3, const VkDeviceSize* a4) {
    if (g_gfxBackend == GfxBackend::Vulkan) return vkCmdBindVertexBuffers(a0, a1, a2, a3, a4);
    if (g_gfxBackend == GfxBackend::OpenGL || g_gfxBackend == GfxBackend::Angle) return gfxgl::gfxCmdBindVertexBuffers(a0, a1, a2, a3, a4);
    if (const auto* table = gfxTableFor(g_gfxBackend)) if (table->cmdBindVertexBuffers) return table->cmdBindVertexBuffers(a0, a1, a2, a3, a4);
    fprintf(stderr, "[gfx] 后端未实现 gfxCmdBindVertexBuffers\n");
    return;
}

void gfxCmdBindIndexBuffer(VkCommandBuffer a0, VkBuffer a1, VkDeviceSize a2, VkIndexType a3) {
    if (g_gfxBackend == GfxBackend::Vulkan) return vkCmdBindIndexBuffer(a0, a1, a2, a3);
    if (g_gfxBackend == GfxBackend::OpenGL || g_gfxBackend == GfxBackend::Angle) return gfxgl::gfxCmdBindIndexBuffer(a0, a1, a2, a3);
    if (const auto* table = gfxTableFor(g_gfxBackend)) if (table->cmdBindIndexBuffer) return table->cmdBindIndexBuffer(a0, a1, a2, a3);
    fprintf(stderr, "[gfx] 后端未实现 gfxCmdBindIndexBuffer\n");
    return;
}

void gfxCmdPushConstants(VkCommandBuffer a0, VkPipelineLayout a1, VkShaderStageFlags a2, uint32_t a3, uint32_t a4, const void* a5) {
    if (g_gfxBackend == GfxBackend::Vulkan) return vkCmdPushConstants(a0, a1, a2, a3, a4, a5);
    if (g_gfxBackend == GfxBackend::OpenGL || g_gfxBackend == GfxBackend::Angle) return gfxgl::gfxCmdPushConstants(a0, a1, a2, a3, a4, a5);
    if (const auto* table = gfxTableFor(g_gfxBackend)) if (table->cmdPushConstants) return table->cmdPushConstants(a0, a1, a2, a3, a4, a5);
    fprintf(stderr, "[gfx] 后端未实现 gfxCmdPushConstants\n");
    return;
}

void gfxCmdDraw(VkCommandBuffer a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
    if (g_gfxBackend == GfxBackend::Vulkan) return vkCmdDraw(a0, a1, a2, a3, a4);
    if (g_gfxBackend == GfxBackend::OpenGL || g_gfxBackend == GfxBackend::Angle) return gfxgl::gfxCmdDraw(a0, a1, a2, a3, a4);
    if (const auto* table = gfxTableFor(g_gfxBackend)) if (table->cmdDraw) return table->cmdDraw(a0, a1, a2, a3, a4);
    fprintf(stderr, "[gfx] 后端未实现 gfxCmdDraw\n");
    return;
}

void gfxCmdDrawIndexed(VkCommandBuffer a0, uint32_t a1, uint32_t a2, uint32_t a3, int32_t a4, uint32_t a5) {
    if (g_gfxBackend == GfxBackend::Vulkan) return vkCmdDrawIndexed(a0, a1, a2, a3, a4, a5);
    if (g_gfxBackend == GfxBackend::OpenGL || g_gfxBackend == GfxBackend::Angle) return gfxgl::gfxCmdDrawIndexed(a0, a1, a2, a3, a4, a5);
    if (const auto* table = gfxTableFor(g_gfxBackend)) if (table->cmdDrawIndexed) return table->cmdDrawIndexed(a0, a1, a2, a3, a4, a5);
    fprintf(stderr, "[gfx] 后端未实现 gfxCmdDrawIndexed\n");
    return;
}

VkResult gfxQueueSubmit(VkQueue a0, uint32_t a1, const VkSubmitInfo* a2, VkFence a3) {
    if (g_gfxBackend == GfxBackend::Vulkan) return vkQueueSubmit(a0, a1, a2, a3);
    if (g_gfxBackend == GfxBackend::OpenGL || g_gfxBackend == GfxBackend::Angle) return gfxgl::gfxQueueSubmit(a0, a1, a2, a3);
    if (const auto* table = gfxTableFor(g_gfxBackend)) if (table->queueSubmit) return table->queueSubmit(a0, a1, a2, a3);
    fprintf(stderr, "[gfx] 后端未实现 gfxQueueSubmit\n");
    return VK_ERROR_FEATURE_NOT_PRESENT;
}

VkResult gfxQueueWaitIdle(VkQueue a0) {
    if (g_gfxBackend == GfxBackend::Vulkan) return vkQueueWaitIdle(a0);
    if (g_gfxBackend == GfxBackend::OpenGL || g_gfxBackend == GfxBackend::Angle) return gfxgl::gfxQueueWaitIdle(a0);
    if (const auto* table = gfxTableFor(g_gfxBackend)) if (table->queueWaitIdle) return table->queueWaitIdle(a0);
    fprintf(stderr, "[gfx] 后端未实现 gfxQueueWaitIdle\n");
    return VK_ERROR_FEATURE_NOT_PRESENT;
}

VkResult gfxWaitForFences(VkDevice a0, uint32_t a1, const VkFence* a2, VkBool32 a3, uint64_t a4) {
    if (g_gfxBackend == GfxBackend::Vulkan) return vkWaitForFences(a0, a1, a2, a3, a4);
    if (g_gfxBackend == GfxBackend::OpenGL || g_gfxBackend == GfxBackend::Angle) return gfxgl::gfxWaitForFences(a0, a1, a2, a3, a4);
    if (const auto* table = gfxTableFor(g_gfxBackend)) if (table->waitForFences) return table->waitForFences(a0, a1, a2, a3, a4);
    fprintf(stderr, "[gfx] 后端未实现 gfxWaitForFences\n");
    return VK_ERROR_FEATURE_NOT_PRESENT;
}

void gfxDeviceWaitIdle(VkDevice a0) {
    if (g_gfxBackend == GfxBackend::Vulkan) { vkDeviceWaitIdle(a0); return; }
    if (g_gfxBackend == GfxBackend::OpenGL || g_gfxBackend == GfxBackend::Angle) return gfxgl::gfxDeviceWaitIdle(a0);
    if (const auto* table = gfxTableFor(g_gfxBackend)) if (table->deviceWaitIdle) return table->deviceWaitIdle(a0);
    fprintf(stderr, "[gfx] 后端未实现 gfxDeviceWaitIdle\n");
    return;
}

void gfxGetPhysicalDeviceFeatures(VkPhysicalDevice a0, VkPhysicalDeviceFeatures* a1) {
    if (g_gfxBackend == GfxBackend::Vulkan) return vkGetPhysicalDeviceFeatures(a0, a1);
    if (g_gfxBackend == GfxBackend::OpenGL || g_gfxBackend == GfxBackend::Angle) return gfxgl::gfxGetPhysicalDeviceFeatures(a0, a1);
    if (const auto* table = gfxTableFor(g_gfxBackend)) if (table->getPhysicalDeviceFeatures) return table->getPhysicalDeviceFeatures(a0, a1);
    fprintf(stderr, "[gfx] 后端未实现 gfxGetPhysicalDeviceFeatures\n");
    return;
}

namespace gfxutil {
std::vector<char> readFile(const std::string& a0) {
    if (g_gfxBackend == GfxBackend::Vulkan) return vkutil::readFile(a0);
    if (g_gfxBackend == GfxBackend::OpenGL || g_gfxBackend == GfxBackend::Angle) return gfxgl::gfxutil::readFile(a0);
    if (const auto* table = gfxTableFor(g_gfxBackend)) if (table->utilReadFile) return table->utilReadFile(a0);
    fprintf(stderr, "[gfx] 后端未实现 readFile\n");
    return {};
}
} // namespace gfxutil

namespace gfxutil {
bool createBuffer(VkCtx& a0, VkDeviceSize a1, VkBufferUsageFlags a2, VkMemoryPropertyFlags a3, Buffer2& a4) {
    if (g_gfxBackend == GfxBackend::Vulkan) return vkutil::createBuffer(a0, a1, a2, a3, a4);
    if (g_gfxBackend == GfxBackend::OpenGL || g_gfxBackend == GfxBackend::Angle) return gfxgl::gfxutil::createBuffer(a0, a1, a2, a3, a4);
    if (const auto* table = gfxTableFor(g_gfxBackend)) if (table->utilCreateBuffer) return table->utilCreateBuffer(a0, a1, a2, a3, a4);
    fprintf(stderr, "[gfx] 后端未实现 createBuffer\n");
    return false;
}
} // namespace gfxutil

namespace gfxutil {
bool createImage(VkCtx& a0, uint32_t a1, uint32_t a2, VkFormat a3, VkImageUsageFlags a4, VkMemoryPropertyFlags a5, VkImage& a6, VkDeviceMemory& a7) {
    if (g_gfxBackend == GfxBackend::Vulkan) return vkutil::createImage(a0, a1, a2, a3, a4, a5, a6, a7);
    if (g_gfxBackend == GfxBackend::OpenGL || g_gfxBackend == GfxBackend::Angle) return gfxgl::gfxutil::createImage(a0, a1, a2, a3, a4, a5, a6, a7);
    if (const auto* table = gfxTableFor(g_gfxBackend)) if (table->utilCreateImage) return table->utilCreateImage(a0, a1, a2, a3, a4, a5, a6, a7);
    fprintf(stderr, "[gfx] 后端未实现 createImage\n");
    return false;
}
} // namespace gfxutil

namespace gfxutil {
VkImageView create2DView(VkDevice a0, VkImage a1, VkFormat a2) {
    if (g_gfxBackend == GfxBackend::Vulkan) return vkutil::create2DView(a0, a1, a2);
    if (g_gfxBackend == GfxBackend::OpenGL || g_gfxBackend == GfxBackend::Angle) return gfxgl::gfxutil::create2DView(a0, a1, a2);
    if (const auto* table = gfxTableFor(g_gfxBackend)) if (table->utilCreate2DView) return table->utilCreate2DView(a0, a1, a2);
    fprintf(stderr, "[gfx] 后端未实现 create2DView\n");
    return VK_NULL_HANDLE;
}
} // namespace gfxutil

namespace gfxutil {
VkPipeline makePipeline(VkCtx& a0, VkRenderPass a1, VkPipelineLayout a2, const std::string& a3, const char* a4, const char* a5, const VkVertexInputBindingDescription* a6, const VkVertexInputAttributeDescription* a7, uint32_t a8, uint32_t a9, VkPrimitiveTopology a10, VkCullModeFlags a11, bool a12, bool a13, bool a14) {
    if (g_gfxBackend == GfxBackend::Vulkan) return vkutil::makePipeline(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14);
    if (g_gfxBackend == GfxBackend::OpenGL || g_gfxBackend == GfxBackend::Angle) return gfxgl::gfxutil::makePipeline(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14);
    if (const auto* table = gfxTableFor(g_gfxBackend)) if (table->utilMakePipeline) return table->utilMakePipeline(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14);
    fprintf(stderr, "[gfx] 后端未实现 makePipeline\n");
    return VK_NULL_HANDLE;
}
} // namespace gfxutil

void gfxSwap(VkCtx& ctx) {
    if (g_gfxBackend == GfxBackend::OpenGL || g_gfxBackend == GfxBackend::Angle) { gfxGLSwap(ctx); return; }
    if (g_gfxBackend == GfxBackend::D3D11) { gfxD3D11Swap(); return; }
    if (g_gfxBackend == GfxBackend::D3D12) { gfxD3D12Swap(); return; }
    if (g_gfxBackend == GfxBackend::D3D9) { gfxD3D9Swap(); return; }
}
void gfxShutdown(VkCtx& ctx) {
    if (g_gfxBackend == GfxBackend::OpenGL || g_gfxBackend == GfxBackend::Angle) { gfxGLShutdown(ctx); return; }
    if (g_gfxBackend == GfxBackend::D3D11) { gfxD3D11Shutdown(ctx); return; }
    if (g_gfxBackend == GfxBackend::D3D12) { gfxD3D12Shutdown(ctx); return; }
    if (g_gfxBackend == GfxBackend::D3D9) { gfxD3D9Shutdown(ctx); return; }
}
void gfxOnResize(VkCtx& ctx, int w, int h) {
    (void)ctx;
    if (g_gfxBackend == GfxBackend::D3D11) gfxD3D11Resize(w, h);
}
