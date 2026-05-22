#pragma once

#include "Framebuffer.h"
#include "RenderPass.h"
#include "VulkanObjects.h"
#include "VulkanUtils.h"

namespace lepton2::vulkancore {
class VulkanContext;
class RenderPass;

struct SwapchainFrame {
    VulkanImage* image;
    uint32_t index;
    VkResult result;
};

class Swapchain : public DeletableVulkanResource {
   public:    
    SwapchainFrame getFrame(VulkanContext* ctx, VkSemaphore semaphore);

    void querySwapchain(VulkanContext* ctx);
    void buildSwapchain(VulkanContext* ctx);

    VkFormat getImageFormat() { return this->imageFormat; }
    VkExtent2D getKnownExtent() { return extent; }
    uint32_t getQueriedImageCount() { return this->queryResults.imageCount; }
    VkSwapchainKHR* getSwapchain() { return &this->swapchain; }
    ImageArray* getSwapchainImages() { return &this->images; }

    TerminatingSubpassConfig getDefaultPresentConfig();

    // Configuration
    void setPresentMode(VkPresentModeKHR presentMode) { this->preferences.presentMode = presentMode; }
    void setSurfaceFormat(VkSurfaceFormatKHR surfaceFormat) { this->preferences.surfaceFormat = surfaceFormat; }
    void setUsageFlags(VkImageUsageFlags flags) { this->preferences.usageFlags = flags; }

    void destroy_back(VulkanContext* ctx) override;

   private:
    SwapchainFrame getFrameInternal(VulkanContext* ctx, VkSemaphore semaphore);
    VkPresentModeKHR choosePresentMode(const std::vector<VkPresentModeKHR>& available);
    VkSurfaceFormatKHR chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& available);
    static VkExtent2D getGLFWExtent(VulkanContext* ctx);
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    ImageArray images;
    VkFormat imageFormat;
    VkExtent2D extent;
    struct {
        VkSurfaceFormatKHR surfaceFormat;
        VkPresentModeKHR presentMode;
        VkSurfaceTransformFlagBitsKHR currentTransform;
        uint32_t imageCount = 0;
    } queryResults;

    struct {
        VkPresentModeKHR presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
        VkSurfaceFormatKHR surfaceFormat = {VK_FORMAT_B8G8R8A8_SRGB, VK_COLORSPACE_SRGB_NONLINEAR_KHR};
        VkImageUsageFlags usageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    } preferences;
};
}  // namespace lepton2::vulkancore