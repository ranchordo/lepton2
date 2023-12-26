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
        std::vector<VulkanImage> swapChainImages;
        std::vector<Framebuffer> swapChainFramebuffers;
        VkFormat swapChainImageFormat;
        VkExtent2D swapChainExtent;
        void destroy(VulkanContext* ctx) override;
        void rebuildSwapChain();
        void createSwapChain();
    private:
        void createFramebuffers();
    };
}