#pragma once

#include "VulkanUtils.h"
#include "VulkanObjects.h"
#include "Framebuffer.h"

namespace lepton2::vulkancore {
    class VulkanContext;

    class SwapChain: public DeletableVulkanResource {
    public:
        SwapChain(VulkanContext* ctx) { this->ctx = ctx; }
        VkSwapchainKHR swapChain;
        VulkanContext* ctx;
        std::vector<VulkanImage*> swapChainImages;
        std::vector<Framebuffer*> swapChainFramebuffers;
        VkFormat swapChainImageFormat;
        VkExtent2D swapChainExtent;
        VulkanImage* defaultDepthImage;
        void destroy_back(VulkanContext* ctx) override;
        void rebuildSwapChain();
        void createSwapChain();
        void createFramebuffers(VkRenderPass renderPass, VulkanImage* depthImage);
    };
}