#pragma once

#include "RenderPass.h"
#include "VulkanUtils.h"

namespace lepton2::vulkancore {

struct InFlightResources : public DeletableVulkanResource {
    VkSemaphore imageAvailableSemaphore;
    VkSemaphore renderFinishedSemaphore;
    VkFence inFlightFence;
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
    VulkanLoop(RenderPass* renderState, uint32_t numInFlightFrames);
    void initialize(VulkanContext* ctx);
    bool shouldLoopTerminate(VulkanContext* ctx);
    void process(VulkanContext* ctx);
    void terminateLoop(VulkanContext* ctx);
    void destroy_back(VulkanContext* ctx) override;
    void addLoopModifier(VulkanLoopModifier* loopmod) { this->loopModifiers.push_back(loopmod); }

    void forceRecordCommandBuffers(VulkanContext* ctx);

   private:
    RenderPass* renderState;
    uint32_t numInFlightFrames;
    uint32_t inFlightFrameCount = 0;
    std::vector<InFlightResources> inFlightResources;
    std::vector<VkCommandBuffer> commandBuffers;
    std::vector<VulkanLoopModifier*> loopModifiers;

    void fillCommandBuffers(VulkanContext* ctx);

    friend class VulkanLoopModifier;
};
}  // namespace lepton2::vulkancore