#pragma once

#include "VulkanUtils.h"
#include "VulkanObjects.h"
#include "Framebuffer.h"

namespace lepton2::vulkancore {
    class VulkanContext;
    class RenderState;

    struct SwapChainFrame {
        Framebuffer* framebuffer;
        VulkanImage* image;
        uint32_t index;
        VkResult result;
    };

    class SwapChain: public DeletableVulkanResource {
    public:
        SwapChain(VulkanContext* ctx) { this->ctx = ctx; }
        void updateViewportScissor(VkCommandBuffer commandBuffer);
        SwapChainFrame getFrame(VkSemaphore semaphore);
        VkSwapchainKHR swapChain;
        VulkanContext* ctx;
        RenderState* renderState;
        std::vector<VulkanImage*> swapChainImages;
        std::vector<VulkanImage*> renderTargetImages;
        std::vector<Framebuffer*> swapChainFramebuffers;
        VkFormat swapChainImageFormat;
        VkExtent2D swapChainExtent;
        struct {
            VkSurfaceFormatKHR surfaceFormat;
            VkPresentModeKHR presentMode;
            VkSurfaceTransformFlagBitsKHR currentTransform;
            uint32_t imageCount;
        } swapChainQueryResults;
        bool clearSwapChain = true; // Change whether swapchain images are cleared on render
        void destroy_back(VulkanContext* ctx) override;
        void querySwapChain();
        void buildSwapChain(RenderState* renderState);
        void rebuildSwapChain();
    private:
        SwapChainFrame getFrameInternal(VkSemaphore semaphore);
    };
}