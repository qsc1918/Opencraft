#pragma once
#include "vk.hpp"
#include "vkutil.hpp"
#include <string>
#include <vector>

// 兼容派发层：renderer.cpp / menu.cpp 包含本头后，所有 vk* 调用改成 gfx* 包装函数。
// 走 Vulkan 时包装函数原样转发，主后端的行为和出图不变；走 OpenGL 时由 gfx_gl.cpp
// 用 GL 调用实现同样的语义。不要在 vk.cpp / vkutil.hpp 里包含本头。
extern bool g_gfxUseGL;

VkResult gfxCreateImage(VkDevice, const VkImageCreateInfo*, const VkAllocationCallbacks*, VkImage*);
void gfxDestroyBuffer(VkDevice, VkBuffer, const VkAllocationCallbacks*);
void gfxDestroyImage(VkDevice, VkImage, const VkAllocationCallbacks*);
void gfxGetImageMemoryRequirements(VkDevice, VkImage, VkMemoryRequirements*);
VkResult gfxAllocateMemory(VkDevice, const VkMemoryAllocateInfo*, const VkAllocationCallbacks*, VkDeviceMemory*);
void gfxFreeMemory(VkDevice, VkDeviceMemory, const VkAllocationCallbacks*);
VkResult gfxBindImageMemory(VkDevice, VkImage, VkDeviceMemory, VkDeviceSize);
VkResult gfxMapMemory(VkDevice, VkDeviceMemory, VkDeviceSize, VkDeviceSize, VkMemoryMapFlags, void**);
void gfxUnmapMemory(VkDevice, VkDeviceMemory);

VkResult gfxCreateDescriptorSetLayout(VkDevice, const VkDescriptorSetLayoutCreateInfo*, const VkAllocationCallbacks*, VkDescriptorSetLayout*);
void gfxDestroyDescriptorSetLayout(VkDevice, VkDescriptorSetLayout, const VkAllocationCallbacks*);
VkResult gfxCreateDescriptorPool(VkDevice, const VkDescriptorPoolCreateInfo*, const VkAllocationCallbacks*, VkDescriptorPool*);
void gfxDestroyDescriptorPool(VkDevice, VkDescriptorPool, const VkAllocationCallbacks*);
VkResult gfxAllocateDescriptorSets(VkDevice, const VkDescriptorSetAllocateInfo*, VkDescriptorSet*);
void gfxUpdateDescriptorSets(VkDevice, uint32_t, const VkWriteDescriptorSet*, uint32_t, const VkCopyDescriptorSet*);
VkResult gfxCreatePipelineLayout(VkDevice, const VkPipelineLayoutCreateInfo*, const VkAllocationCallbacks*, VkPipelineLayout*);
void gfxDestroyPipelineLayout(VkDevice, VkPipelineLayout, const VkAllocationCallbacks*);
void gfxDestroyPipeline(VkDevice, VkPipeline, const VkAllocationCallbacks*);

VkResult gfxCreateSampler(VkDevice, const VkSamplerCreateInfo*, const VkAllocationCallbacks*, VkSampler*);
void gfxDestroySampler(VkDevice, VkSampler, const VkAllocationCallbacks*);
VkResult gfxCreateImageView(VkDevice, const VkImageViewCreateInfo*, const VkAllocationCallbacks*, VkImageView*);
void gfxDestroyImageView(VkDevice, VkImageView, const VkAllocationCallbacks*);

VkResult gfxResetCommandBuffer(VkCommandBuffer, VkCommandBufferResetFlags);
VkResult gfxBeginCommandBuffer(VkCommandBuffer, const VkCommandBufferBeginInfo*);
VkResult gfxEndCommandBuffer(VkCommandBuffer);
void gfxCmdPipelineBarrier(VkCommandBuffer, VkPipelineStageFlags, VkPipelineStageFlags,
                           VkDependencyFlags, uint32_t, const VkMemoryBarrier*,
                           uint32_t, const VkBufferMemoryBarrier*,
                           uint32_t, const VkImageMemoryBarrier*);
void gfxCmdCopyBufferToImage(VkCommandBuffer, VkBuffer, VkImage, VkImageLayout,
                             uint32_t, const VkBufferImageCopy*);
void gfxCmdCopyImageToBuffer(VkCommandBuffer, VkImage, VkImageLayout, VkBuffer,
                             uint32_t, const VkBufferImageCopy*);
void gfxCmdBlitImage(VkCommandBuffer, VkImage, VkImageLayout, VkImage, VkImageLayout,
                     uint32_t, const VkImageBlit*, VkFilter);
void gfxCmdBeginRenderPass(VkCommandBuffer, const VkRenderPassBeginInfo*, VkSubpassContents);
void gfxCmdEndRenderPass(VkCommandBuffer);
void gfxCmdSetViewport(VkCommandBuffer, uint32_t, uint32_t, const VkViewport*);
void gfxCmdSetScissor(VkCommandBuffer, uint32_t, uint32_t, const VkRect2D*);
void gfxCmdBindPipeline(VkCommandBuffer, VkPipelineBindPoint, VkPipeline);
void gfxCmdBindDescriptorSets(VkCommandBuffer, VkPipelineBindPoint, VkPipelineLayout,
                              uint32_t, uint32_t, const VkDescriptorSet*, uint32_t, const uint32_t*);
void gfxCmdBindVertexBuffers(VkCommandBuffer, uint32_t, uint32_t, const VkBuffer*, const VkDeviceSize*);
void gfxCmdBindIndexBuffer(VkCommandBuffer, VkBuffer, VkDeviceSize, VkIndexType);
void gfxCmdPushConstants(VkCommandBuffer, VkPipelineLayout, VkShaderStageFlags, uint32_t, uint32_t, const void*);
void gfxCmdDraw(VkCommandBuffer, uint32_t, uint32_t, uint32_t, uint32_t);
void gfxCmdDrawIndexed(VkCommandBuffer, uint32_t, uint32_t, uint32_t, int32_t, uint32_t);

VkResult gfxQueueSubmit(VkQueue, uint32_t, const VkSubmitInfo*, VkFence);
VkResult gfxQueueWaitIdle(VkQueue);
VkResult gfxWaitForFences(VkDevice, uint32_t, const VkFence*, VkBool32, uint64_t);
void gfxDeviceWaitIdle(VkDevice);
void gfxGetPhysicalDeviceFeatures(VkPhysicalDevice, VkPhysicalDeviceFeatures*);

// vkutil 的对应版本：Vulkan 路径直接转发，GL 路径在 gfx_gl.cpp 中实现。
namespace gfxutil {
std::vector<char> readFile(const std::string& path);
bool createBuffer(VkCtx& ctx, VkDeviceSize size, VkBufferUsageFlags usage,
                  VkMemoryPropertyFlags props, Buffer2& out);
bool createImage(VkCtx& ctx, uint32_t w, uint32_t h, VkFormat format,
                 VkImageUsageFlags usage, VkMemoryPropertyFlags props,
                 VkImage& image, VkDeviceMemory& memory);
VkImageView create2DView(VkDevice dev, VkImage image, VkFormat format);
VkPipeline makePipeline(VkCtx& ctx, VkRenderPass rp, VkPipelineLayout layout,
                        const std::string& shaderDir, const char* vsName, const char* fsName,
                        const VkVertexInputBindingDescription* bindings,
                        const VkVertexInputAttributeDescription* attrs,
                        uint32_t bindingCount, uint32_t attrCount,
                        VkPrimitiveTopology topo, VkCullModeFlags cull,
                        bool blend, bool depthTest, bool depthWrite);
} // namespace gfxutil

// 后端枚举：Vulkan 是唯一主后端，其余档位互不依赖，统一走 gfx* 派发。
enum class GfxBackend { Vulkan, OpenGL, Angle, D3D11, D3D12, D3D9 };
extern GfxBackend g_gfxBackend;

namespace gfxgl {
VkResult gfxCreateImage(VkDevice, const VkImageCreateInfo*, const VkAllocationCallbacks*, VkImage*);
void gfxDestroyBuffer(VkDevice, VkBuffer, const VkAllocationCallbacks*);
void gfxDestroyImage(VkDevice, VkImage, const VkAllocationCallbacks*);
void gfxGetImageMemoryRequirements(VkDevice, VkImage, VkMemoryRequirements*);
VkResult gfxAllocateMemory(VkDevice, const VkMemoryAllocateInfo*, const VkAllocationCallbacks*, VkDeviceMemory*);
void gfxFreeMemory(VkDevice, VkDeviceMemory, const VkAllocationCallbacks*);
VkResult gfxBindImageMemory(VkDevice, VkImage, VkDeviceMemory, VkDeviceSize);
VkResult gfxMapMemory(VkDevice, VkDeviceMemory, VkDeviceSize, VkDeviceSize, VkMemoryMapFlags, void**);
void gfxUnmapMemory(VkDevice, VkDeviceMemory);
VkResult gfxCreateDescriptorSetLayout(VkDevice, const VkDescriptorSetLayoutCreateInfo*, const VkAllocationCallbacks*, VkDescriptorSetLayout*);
void gfxDestroyDescriptorSetLayout(VkDevice, VkDescriptorSetLayout, const VkAllocationCallbacks*);
VkResult gfxCreateDescriptorPool(VkDevice, const VkDescriptorPoolCreateInfo*, const VkAllocationCallbacks*, VkDescriptorPool*);
void gfxDestroyDescriptorPool(VkDevice, VkDescriptorPool, const VkAllocationCallbacks*);
VkResult gfxAllocateDescriptorSets(VkDevice, const VkDescriptorSetAllocateInfo*, VkDescriptorSet*);
void gfxUpdateDescriptorSets(VkDevice, uint32_t, const VkWriteDescriptorSet*, uint32_t, const VkCopyDescriptorSet*);
VkResult gfxCreatePipelineLayout(VkDevice, const VkPipelineLayoutCreateInfo*, const VkAllocationCallbacks*, VkPipelineLayout*);
void gfxDestroyPipelineLayout(VkDevice, VkPipelineLayout, const VkAllocationCallbacks*);
void gfxDestroyPipeline(VkDevice, VkPipeline, const VkAllocationCallbacks*);
VkResult gfxCreateSampler(VkDevice, const VkSamplerCreateInfo*, const VkAllocationCallbacks*, VkSampler*);
void gfxDestroySampler(VkDevice, VkSampler, const VkAllocationCallbacks*);
VkResult gfxCreateImageView(VkDevice, const VkImageViewCreateInfo*, const VkAllocationCallbacks*, VkImageView*);
void gfxDestroyImageView(VkDevice, VkImageView, const VkAllocationCallbacks*);
VkResult gfxResetCommandBuffer(VkCommandBuffer, VkCommandBufferResetFlags);
VkResult gfxBeginCommandBuffer(VkCommandBuffer, const VkCommandBufferBeginInfo*);
VkResult gfxEndCommandBuffer(VkCommandBuffer);
void gfxCmdPipelineBarrier(VkCommandBuffer, VkPipelineStageFlags, VkPipelineStageFlags, VkDependencyFlags, uint32_t, const VkMemoryBarrier*, uint32_t, const VkBufferMemoryBarrier*, uint32_t, const VkImageMemoryBarrier*);
void gfxCmdCopyBufferToImage(VkCommandBuffer, VkBuffer, VkImage, VkImageLayout, uint32_t, const VkBufferImageCopy*);
void gfxCmdCopyImageToBuffer(VkCommandBuffer, VkImage, VkImageLayout, VkBuffer, uint32_t, const VkBufferImageCopy*);
void gfxCmdBlitImage(VkCommandBuffer, VkImage, VkImageLayout, VkImage, VkImageLayout, uint32_t, const VkImageBlit*, VkFilter);
void gfxCmdBeginRenderPass(VkCommandBuffer, const VkRenderPassBeginInfo*, VkSubpassContents);
void gfxCmdEndRenderPass(VkCommandBuffer);
void gfxCmdSetViewport(VkCommandBuffer, uint32_t, uint32_t, const VkViewport*);
void gfxCmdSetScissor(VkCommandBuffer, uint32_t, uint32_t, const VkRect2D*);
void gfxCmdBindPipeline(VkCommandBuffer, VkPipelineBindPoint, VkPipeline);
void gfxCmdBindDescriptorSets(VkCommandBuffer, VkPipelineBindPoint, VkPipelineLayout, uint32_t, uint32_t, const VkDescriptorSet*, uint32_t, const uint32_t*);
void gfxCmdBindVertexBuffers(VkCommandBuffer, uint32_t, uint32_t, const VkBuffer*, const VkDeviceSize*);
void gfxCmdBindIndexBuffer(VkCommandBuffer, VkBuffer, VkDeviceSize, VkIndexType);
void gfxCmdPushConstants(VkCommandBuffer, VkPipelineLayout, VkShaderStageFlags, uint32_t, uint32_t, const void*);
void gfxCmdDraw(VkCommandBuffer, uint32_t, uint32_t, uint32_t, uint32_t);
void gfxCmdDrawIndexed(VkCommandBuffer, uint32_t, uint32_t, uint32_t, int32_t, uint32_t);
VkResult gfxQueueSubmit(VkQueue, uint32_t, const VkSubmitInfo*, VkFence);
VkResult gfxQueueWaitIdle(VkQueue);
VkResult gfxWaitForFences(VkDevice, uint32_t, const VkFence*, VkBool32, uint64_t);
void gfxDeviceWaitIdle(VkDevice);
void gfxGetPhysicalDeviceFeatures(VkPhysicalDevice, VkPhysicalDeviceFeatures*);
namespace gfxutil {
std::vector<char> readFile(const std::string& path);
bool createBuffer(VkCtx& ctx, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, Buffer2& out);
bool createImage(VkCtx& ctx, uint32_t w, uint32_t h, VkFormat format, VkImageUsageFlags usage, VkMemoryPropertyFlags props, VkImage& image, VkDeviceMemory& memory);
VkImageView create2DView(VkDevice dev, VkImage image, VkFormat format);
VkPipeline makePipeline(VkCtx& ctx, VkRenderPass rp, VkPipelineLayout layout,
                        const std::string& shaderDir, const char* vsName, const char* fsName,
                        const VkVertexInputBindingDescription* bindings,
                        const VkVertexInputAttributeDescription* attrs,
                        uint32_t bindingCount, uint32_t attrCount,
                        VkPrimitiveTopology topo, VkCullModeFlags cull,
                        bool blend, bool depthTest, bool depthWrite);
} // namespace gfxutil
} // namespace gfxgl

// 非 OpenGL 后端在初始化时把实现填进自己的表；未实现的调用返回错误而不是崩。
struct GfxDispatchTable {
    decltype(&gfxCreateImage) createImage = nullptr;
    decltype(&gfxDestroyBuffer) destroyBuffer = nullptr;
    decltype(&gfxDestroyImage) destroyImage = nullptr;
    decltype(&gfxGetImageMemoryRequirements) getImageMemoryRequirements = nullptr;
    decltype(&gfxAllocateMemory) allocateMemory = nullptr;
    decltype(&gfxFreeMemory) freeMemory = nullptr;
    decltype(&gfxBindImageMemory) bindImageMemory = nullptr;
    decltype(&gfxMapMemory) mapMemory = nullptr;
    decltype(&gfxUnmapMemory) unmapMemory = nullptr;
    decltype(&gfxCreateDescriptorSetLayout) createDescriptorSetLayout = nullptr;
    decltype(&gfxDestroyDescriptorSetLayout) destroyDescriptorSetLayout = nullptr;
    decltype(&gfxCreateDescriptorPool) createDescriptorPool = nullptr;
    decltype(&gfxDestroyDescriptorPool) destroyDescriptorPool = nullptr;
    decltype(&gfxAllocateDescriptorSets) allocateDescriptorSets = nullptr;
    decltype(&gfxUpdateDescriptorSets) updateDescriptorSets = nullptr;
    decltype(&gfxCreatePipelineLayout) createPipelineLayout = nullptr;
    decltype(&gfxDestroyPipelineLayout) destroyPipelineLayout = nullptr;
    decltype(&gfxDestroyPipeline) destroyPipeline = nullptr;
    decltype(&gfxCreateSampler) createSampler = nullptr;
    decltype(&gfxDestroySampler) destroySampler = nullptr;
    decltype(&gfxCreateImageView) createImageView = nullptr;
    decltype(&gfxDestroyImageView) destroyImageView = nullptr;
    decltype(&gfxResetCommandBuffer) resetCommandBuffer = nullptr;
    decltype(&gfxBeginCommandBuffer) beginCommandBuffer = nullptr;
    decltype(&gfxEndCommandBuffer) endCommandBuffer = nullptr;
    decltype(&gfxCmdPipelineBarrier) cmdPipelineBarrier = nullptr;
    decltype(&gfxCmdCopyBufferToImage) cmdCopyBufferToImage = nullptr;
    decltype(&gfxCmdCopyImageToBuffer) cmdCopyImageToBuffer = nullptr;
    decltype(&gfxCmdBlitImage) cmdBlitImage = nullptr;
    decltype(&gfxCmdBeginRenderPass) cmdBeginRenderPass = nullptr;
    decltype(&gfxCmdEndRenderPass) cmdEndRenderPass = nullptr;
    decltype(&gfxCmdSetViewport) cmdSetViewport = nullptr;
    decltype(&gfxCmdSetScissor) cmdSetScissor = nullptr;
    decltype(&gfxCmdBindPipeline) cmdBindPipeline = nullptr;
    decltype(&gfxCmdBindDescriptorSets) cmdBindDescriptorSets = nullptr;
    decltype(&gfxCmdBindVertexBuffers) cmdBindVertexBuffers = nullptr;
    decltype(&gfxCmdBindIndexBuffer) cmdBindIndexBuffer = nullptr;
    decltype(&gfxCmdPushConstants) cmdPushConstants = nullptr;
    decltype(&gfxCmdDraw) cmdDraw = nullptr;
    decltype(&gfxCmdDrawIndexed) cmdDrawIndexed = nullptr;
    decltype(&gfxQueueSubmit) queueSubmit = nullptr;
    decltype(&gfxQueueWaitIdle) queueWaitIdle = nullptr;
    decltype(&gfxWaitForFences) waitForFences = nullptr;
    decltype(&gfxDeviceWaitIdle) deviceWaitIdle = nullptr;
    decltype(&gfxGetPhysicalDeviceFeatures) getPhysicalDeviceFeatures = nullptr;
    decltype(&gfxutil::readFile) utilReadFile = nullptr;
    decltype(&gfxutil::createBuffer) utilCreateBuffer = nullptr;
    decltype(&gfxutil::createImage) utilCreateImage = nullptr;
    decltype(&gfxutil::create2DView) utilCreate2DView = nullptr;
    decltype(&gfxutil::makePipeline) utilMakePipeline = nullptr;
};
extern GfxDispatchTable g_gfxD3D11Table;
extern GfxDispatchTable g_gfxD3D12Table;
extern GfxDispatchTable g_gfxD3D9Table;
const GfxDispatchTable* gfxTableFor(GfxBackend backend);

#ifndef GFX_SHIM_IMPL
#define vkCreateImage gfxCreateImage
#define vkDestroyBuffer gfxDestroyBuffer
#define vkDestroyImage gfxDestroyImage
#define vkGetImageMemoryRequirements gfxGetImageMemoryRequirements
#define vkAllocateMemory gfxAllocateMemory
#define vkFreeMemory gfxFreeMemory
#define vkBindImageMemory gfxBindImageMemory
#define vkMapMemory gfxMapMemory
#define vkUnmapMemory gfxUnmapMemory
#define vkCreateDescriptorSetLayout gfxCreateDescriptorSetLayout
#define vkDestroyDescriptorSetLayout gfxDestroyDescriptorSetLayout
#define vkCreateDescriptorPool gfxCreateDescriptorPool
#define vkDestroyDescriptorPool gfxDestroyDescriptorPool
#define vkAllocateDescriptorSets gfxAllocateDescriptorSets
#define vkUpdateDescriptorSets gfxUpdateDescriptorSets
#define vkCreatePipelineLayout gfxCreatePipelineLayout
#define vkDestroyPipelineLayout gfxDestroyPipelineLayout
#define vkDestroyPipeline gfxDestroyPipeline
#define vkCreateSampler gfxCreateSampler
#define vkDestroySampler gfxDestroySampler
#define vkCreateImageView gfxCreateImageView
#define vkDestroyImageView gfxDestroyImageView
#define vkResetCommandBuffer gfxResetCommandBuffer
#define vkBeginCommandBuffer gfxBeginCommandBuffer
#define vkEndCommandBuffer gfxEndCommandBuffer
#define vkCmdPipelineBarrier gfxCmdPipelineBarrier
#define vkCmdCopyBufferToImage gfxCmdCopyBufferToImage
#define vkCmdCopyImageToBuffer gfxCmdCopyImageToBuffer
#define vkCmdBlitImage gfxCmdBlitImage
#define vkCmdBeginRenderPass gfxCmdBeginRenderPass
#define vkCmdEndRenderPass gfxCmdEndRenderPass
#define vkCmdSetViewport gfxCmdSetViewport
#define vkCmdSetScissor gfxCmdSetScissor
#define vkCmdBindPipeline gfxCmdBindPipeline
#define vkCmdBindDescriptorSets gfxCmdBindDescriptorSets
#define vkCmdBindVertexBuffers gfxCmdBindVertexBuffers
#define vkCmdBindIndexBuffer gfxCmdBindIndexBuffer
#define vkCmdPushConstants gfxCmdPushConstants
#define vkCmdDraw gfxCmdDraw
#define vkCmdDrawIndexed gfxCmdDrawIndexed
#define vkQueueSubmit gfxQueueSubmit
#define vkQueueWaitIdle gfxQueueWaitIdle
#define vkWaitForFences gfxWaitForFences
#define vkDeviceWaitIdle gfxDeviceWaitIdle
#define vkGetPhysicalDeviceFeatures gfxGetPhysicalDeviceFeatures
#endif
