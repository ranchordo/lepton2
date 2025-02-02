#pragma once

#include "RenderState.h"
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

class VulkanLoopModifier : public DeletableVulkanResource {
   public:
    virtual void preRender(uint32_t swapChainFrameIndex) {};
    virtual void destroy_back(VulkanContext* ctx) override {};
};

class VulkanLoop : public DeletableVulkanResource {
   public:
    VulkanLoop(RenderState* renderState, uint32_t numInFlightFrames);
    void initialize();
    bool shouldLoopTerminate();
    void process();
    void terminateLoop();
    void destroy_back(VulkanContext* ctx) override;
    void addLoopModifier(VulkanLoopModifier* loopmod) { this->loopModifiers.push_back(loopmod); }

   private:
    RenderState* renderState;
    uint32_t numInFlightFrames;
    uint32_t inFlightFrameCount = 0;
    std::vector<InFlightResources> inFlightResources;
    std::vector<VulkanLoopModifier*> loopModifiers;

    friend class VulkanLoopModifier;
};
}  // namespace lepton2::vulkancore