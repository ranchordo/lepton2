#pragma once

#include "RenderPass.h"
#include "VulkanUtils.h"

namespace lepton2::vulkancore {

struct InFlightResources : public DeletableVulkanResource {
    VkSemaphore imageAvailableSemaphore;
    VkSemaphore renderFinishedSemaphore;
    VkFence inFlightFence;
    VkCommandBuffer commandBuffer;
    void create(VulkanContext* ctx);
    void destroy_back(VulkanContext* ctx) override;
};

namespace loopmodifiers {
class VulkanLoopModifier : public DeletableVulkanResource {
   public:
    virtual void preRender(VulkanContext* ctx, uint32_t frameIndex) {};
    virtual void renderCmd(VkCommandBuffer buffer, uint32_t frameIndex, uint32_t swapchainIndex) {};
    virtual void postRenderCmd(VkCommandBuffer buffer, uint32_t frameIndex, uint32_t swapchainIndex) {};
    virtual void onSwapchainRebuild(VulkanContext* ctx) {};
    virtual void destroy_back(VulkanContext* ctx) override {};
};
class SimpleRenderPass : public VulkanLoopModifier {
   public:
    SimpleRenderPass(RenderPass* pass, ImageArray* targets, bool useSwapchainFramebufferIndex) {
        this->renderPass = pass;
        this->targetContainer = targets;
        this->swapchainIndexing = useSwapchainFramebufferIndex;
    }
    void preRender(VulkanContext* ctx, uint32_t frameIndex) override;
    void renderCmd(VkCommandBuffer buffer, uint32_t frameIndex, uint32_t swapchainIndex) override;
    void onSwapchainRebuild(VulkanContext* ctx) override;

   private:
    RenderPass* renderPass;
    ImageArray* targetContainer;
    bool swapchainIndexing;
};
}  // namespace loopmodifiers

class VulkanLoop : public DeletableVulkanResource {
   public:
    VulkanLoop(uint32_t inFlightFrames);
    void initialize(VulkanContext* ctx);
    bool shouldLoopTerminate(VulkanContext* ctx);
    void process(VulkanContext* ctx);
    void terminateLoop(VulkanContext* ctx);
    void destroy_back(VulkanContext* ctx) override;

    std::vector<loopmodifiers::VulkanLoopModifier*> loopModifiers;

   private:
    uint32_t inFlightFrameCount = 0;
    std::vector<InFlightResources> inFlightResources;

    friend class VulkanLoopModifier;
};
}  // namespace lepton2::vulkancore