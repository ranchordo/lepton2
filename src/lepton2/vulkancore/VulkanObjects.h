#pragma once

#include "VulkanMemory.h"
#include "VulkanUtils.h"

namespace lepton2::vulkancore {
class VulkanContext;
class VulkanImage : public DeletableVulkanResource {
   public:
    VkImage image = VK_NULL_HANDLE;
    bool doNotDestroyImage = false;
    VkFormat imageFormat;
    VkImageView imageView = VK_NULL_HANDLE;
    MemoryChonklet chonklet;
    VulkanImage(VkImage image, VkFormat format) {
        this->image = image;
        this->imageFormat = format;
    }
    VulkanImage() {}
    void findMemory(VulkanContext* ctx, VkMemoryPropertyFlags properties);
    void buildImageView(VulkanContext* ctx, VkImageAspectFlags aspectFlags);
    void destroy_back(VulkanContext* ctx) override;
};
class VulkanBuffer : public DeletableVulkanResource {
   public:
    VkBuffer buffer;
    MemoryChonklet chonklet;
    VulkanBuffer(VkBuffer buffer) { this->buffer = buffer; }
    VulkanBuffer() {}
    void findMemory(VulkanContext* ctx, VkMemoryPropertyFlags properties);
    void destroy_back(VulkanContext* ctx) override;
};
}  // namespace lepton2::vulkancore