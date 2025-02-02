#pragma once

#include "Framebuffer.h"
#include "VulkanObjects.h"
#include "VulkanUtils.h"

namespace lepton2::vulkancore {
class VulkanContext;
class RenderState;
struct DescriptorSetUpdateInfo;

struct SwapChainFrame {
    Framebuffer* framebuffer;
    VulkanImage* image;
    uint32_t index;
    VkResult result;
};

class SwapChain : public DeletableVulkanResource {
   public:
    SwapChain(VulkanContext* ctx) { this->ctx = ctx; }
    void updateViewportScissor(VkCommandBuffer commandBuffer);
    SwapChainFrame getFrame(VkSemaphore semaphore);
    VkSwapchainKHR swapChain;
    VulkanContext* ctx;
    RenderState* renderState;
    std::vector<VulkanImage*> swapChainImages;
    std::vector<Framebuffer*> swapChainFramebuffers;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    struct {
        VkSurfaceFormatKHR surfaceFormat;
        VkPresentModeKHR presentMode;
        VkSurfaceTransformFlagBitsKHR currentTransform;
        uint32_t imageCount;
    } swapChainQueryResults;
    bool clearSwapChain = true;  // Change whether swapchain images are cleared on render
    void destroy_back(VulkanContext* ctx) override;
    void querySwapChain();
    void buildSwapChain(RenderState* renderState);
    // Only used for manual rebuilding
    void deinitSwapChain();
    void rebuildSwapChain();
    std::unordered_set<DescriptorSetUpdateInfo*> descriptorUpdates;

   private:
    SwapChainFrame getFrameInternal(VkSemaphore semaphore);
    std::vector<VulkanImage*> renderTargetImages;
    std::vector<std::vector<VulkanImage*>*> swapChainCreationTrackers;
};
}  // namespace lepton2::vulkancore