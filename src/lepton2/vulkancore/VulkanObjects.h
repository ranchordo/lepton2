#pragma once

#include "VulkanUtils.h"
#include "VulkanMemory.h"

namespace lepton2::vulkancore {
    class VulkanContext;
    struct VulkanImage: public DeletableVulkanResource {
        VkImage image = VK_NULL_HANDLE;
        bool do_not_destroy_image = false;
        VkFormat imageFormat;
        VkImageView imageView = VK_NULL_HANDLE;
        MemoryChonklet chonklet;
        VulkanImage(VkImage image, VkFormat format) { this->image = image; this->imageFormat = format; }
        VulkanImage() {}
        void findMemory(VulkanContext* ctx, VkMemoryPropertyFlags properties);
        void buildImageView(VulkanContext* ctx, VkImageAspectFlags aspectFlags);
        void destroy_back(VulkanContext* ctx) override;
    };
    struct VulkanBuffer: public DeletableVulkanResource {
        VkBuffer buffer;
        MemoryChonklet chonklet;
        VulkanBuffer(VkBuffer buffer) { this->buffer = buffer; }
        VulkanBuffer() {}
        void findMemory(VulkanContext* ctx, VkMemoryPropertyFlags properties);
        void destroy_back(VulkanContext* ctx) override;
    };
}