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

class VulkanLoop : public DeletableVulkanResource {
   public:
    VulkanLoop(RenderState* renderState, uint32_t numInFlightFrames);
    void initialize();
    bool shouldLoopTerminate();
    void process();
    void terminateLoop();
    void destroy_back(VulkanContext* ctx) override;

   private:
    RenderState* renderState;
    uint32_t numInFlightFrames;
    uint32_t inFlightFrameCount = 0;
    std::vector<InFlightResources> inFlightResources;
};
}  // namespace lepton2::vulkancore