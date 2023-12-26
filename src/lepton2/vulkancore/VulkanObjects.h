#pragma once

#include "VulkanUtils.h"
#include "VulkanMemory.h"

namespace lepton2::vulkancore {
    class VulkanContext;
    struct VulkanImage: public DeletableVulkanResource {
        VkImage image = VK_NULL_HANDLE;
        VkFormat imageFormat;
        VkImageView imageView = VK_NULL_HANDLE;
        MemoryChonklet chonklet;
        VulkanImage(VkImage image, VkFormat format) { this->image = image; this->imageFormat = format; }
        VulkanImage() {}
        void findMemory(VulkanContext* ctx, VkMemoryPropertyFlags properties);
        void buildImageView(VulkanContext* ctx, VkImageAspectFlags aspectFlags);
        void destroy(VulkanContext* ctx) override;
    };
    struct VulkanBuffer: public DeletableVulkanResource {
        VkBuffer buffer;
        MemoryChonklet chonklet;
        VulkanBuffer(VkBuffer buffer) { this->buffer = buffer; }
        VulkanBuffer() {}
        void findMemory(VulkanContext* ctx, VkMemoryPropertyFlags properties);
        void destroy(VulkanContext* ctx) override;
    };
}