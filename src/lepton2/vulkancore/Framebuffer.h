#pragma once

#include "VulkanUtils.h"
#include "VulkanObjects.h"

namespace lepton2::vulkancore {
    class VulkanContext;
    class Framebuffer: public DeletableVulkanResource {
    public:
        Framebuffer() {}
        void addColorImage(VulkanImage* image);
        void setDepthImage(VulkanImage* image);
        void buildFramebuffer(VkRenderPass renderPass, uint32_t width, uint32_t height);
        VulkanImage* depthImage;
        std::vector<VulkanImage*> colorImages;
        VkFramebuffer framebuffer = VK_NULL_HANDLE;
        void destroy_back(VulkanContext* ctx) override;
    };
}