#pragma once

#include "VulkanObjects.h"
#include "VulkanUtils.h"

namespace lepton2::vulkancore {
class VulkanContext;
class Framebuffer : public DeletableVulkanResource {
   public:
    Framebuffer() {}
    void addImage(VulkanImage* image);
    void buildFramebuffer(VulkanContext* ctx, VkRenderPass renderPass, uint32_t width, uint32_t height);
    std::vector<VulkanImage*> images;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    void destroy_back(VulkanContext* ctx) override;
};
}  // namespace lepton2::vulkancore