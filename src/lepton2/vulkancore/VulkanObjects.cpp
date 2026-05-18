#include "VulkanObjects.h"

#include "VulkanContext.h"

using namespace lepton2::vulkancore;

void VulkanImage::findMemory(VulkanContext* ctx, VkMemoryPropertyFlags properties) {
    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(ctx->device, this->image, &req);
    uint32_t memoryTypeIndex = findMemoryType(ctx, req.memoryTypeBits, properties);
    this->chonklet = ctx->allocManager.findMemory(ctx, req.size, req.alignment, memoryTypeIndex);
    VkDeviceSize offset = chonklet.offset + chonklet.alignmentPadding;
    vkBindImageMemory(ctx->device, this->image, this->chonklet.chonkus->memory, offset);
}

void VulkanImage::buildImageView(VulkanContext* ctx, VkImageAspectFlags aspectFlags) {
    this->imageView = createImageView(ctx, this->image, this->imageFormat, aspectFlags);
}

void VulkanImage::destroy_back(VulkanContext* ctx) {
    if (!doNotDestroyImage && this->image != VK_NULL_HANDLE) {
        vkDestroyImage(ctx->device, this->image, nullptr);
    }
    if (this->imageView != VK_NULL_HANDLE) {
        vkDestroyImageView(ctx->device, this->imageView, nullptr);
    }
    this->chonklet.destroy(ctx);
}

void VulkanBuffer::findMemory(VulkanContext* ctx, VkMemoryPropertyFlags properties) {
    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(ctx->device, this->buffer, &req);
    uint32_t memoryTypeIndex = findMemoryType(ctx, req.memoryTypeBits, properties);
    this->chonklet = ctx->allocManager.findMemory(ctx, req.size, req.alignment, memoryTypeIndex);
    VkDeviceSize offset = chonklet.offset + chonklet.alignmentPadding;
    vkBindBufferMemory(ctx->device, this->buffer, this->chonklet.chonkus->memory, offset);
}

void VulkanBuffer::destroy_back(VulkanContext* ctx) {
    if (this->buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(ctx->device, this->buffer, nullptr);
    }
    this->chonklet.destroy(ctx);
}