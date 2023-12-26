#pragma once

#include "VulkanUtils.h"
#include "VulkanObjects.h"

namespace lepton2::vulkancore {
    // This is all awful. Framebuffers need to actually have vkFramebuffers, and they're based on imageViews.
    class VulkanContext;
    class Framebuffer: public DeletableVulkanResource {
    public:
        Framebuffer() {}
        void addColorImage(VulkanImage image);
        void setDepthImage(VulkanImage image);
        VulkanImage depthImage;
        std::vector<VulkanImage> colorImages;
        void destroy(VulkanContext* ctx) override;
    };
}