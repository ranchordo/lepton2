#pragma once

#include "VulkanObjects.h"
#include "VulkanUtils.h"

namespace lepton2::vulkancore {
class VulkanContext;
//! `VkFramebuffer` wrapper with a bundled `VkExtent2D`.
class Framebuffer : public DeletableVulkanResource {
   public:
    Framebuffer() {}
    void addImage(VulkanImage* image);
    const std::vector<VulkanImage*>& getImages() { return this->images; }
    VkFramebuffer getFramebuffer() { return this->framebuffer; }
    VkExtent2D getExtent() { return this->extent; }
    void buildFramebuffer(VulkanContext* ctx, VkRenderPass renderPass, VkExtent2D extent); //!< Build `VkFramebuffer` from collected images
    void destroy_back(VulkanContext* ctx) override;

    private:
    std::vector<VulkanImage*> images;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    VkExtent2D extent;
};
}  // namespace lepton2::vulkancore