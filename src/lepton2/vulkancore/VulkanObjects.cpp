#include "VulkanObjects.h"
#include "VulkanContext.h"

using namespace lepton2::vulkancore;

void VulkanImage::findMemory(VulkanContext* ctx, VkMemoryPropertyFlags properties) {
    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(ctx->device, this->image, &req);
    uint32_t memoryTypeIndex = findMemoryType(ctx, req.memoryTypeBits, properties);
    this->chonklet = ctx->allocManager.findMemory(req.size, memoryTypeIndex);
    vkBindImageMemory(ctx->device, this->image, this->chonklet.chonkus->memory, this->chonklet.offset);
}

void VulkanImage::buildImageView(VulkanContext* ctx, VkImageAspectFlags aspectFlags) {
    this->imageView = createImageView(ctx, this->image, this->imageFormat, aspectFlags);
}

void VulkanImage::destroy(VulkanContext* ctx) {
    if (this->image != VK_NULL_HANDLE) {
        vkDestroyImage(ctx->device, this->image, nullptr);
        this->image = VK_NULL_HANDLE;
    }
    if (this->imageView != VK_NULL_HANDLE) {
        vkDestroyImageView(ctx->device, this->imageView, nullptr);
        this->imageView = VK_NULL_HANDLE;
    }
    this->chonklet.destroy(ctx);
}

void VulkanBuffer::findMemory(VulkanContext* ctx, VkMemoryPropertyFlags properties) {
    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(ctx->device, this->buffer, &req);
    uint32_t memoryTypeIndex = findMemoryType(ctx, req.memoryTypeBits, properties);
    this->chonklet = ctx->allocManager.findMemory(req.size, memoryTypeIndex);
    vkBindBufferMemory(ctx->device, this->buffer, this->chonklet.chonkus->memory, this->chonklet.offset);
}

void VulkanBuffer::destroy(VulkanContext* ctx) {
    if (this->buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(ctx->device, this->buffer, nullptr);
        this->buffer = VK_NULL_HANDLE;
    }
    this->chonklet.destroy(ctx);
}