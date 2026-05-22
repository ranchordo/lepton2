#pragma once

#include "../utils/LeptonUtils.h"
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
class ImageArraySwapchainRebuild : public VulkanLoopModifier {
   public:
    ImageArraySwapchainRebuild(RenderTargetImageCreationInfo rticInfo, ImageArray* array, VkExtent2D* extentPtr, uint32_t mult) {
        this->rticInfo = rticInfo;
        this->array = array;
        this->extentPtr = extentPtr;
        this->multiplicity = mult;
    }
    void onSwapchainRebuild(VulkanContext*) override;

   private:
    RenderTargetImageCreationInfo rticInfo;
    ImageArray* array;
    VkExtent2D* extentPtr;
    uint32_t multiplicity;
};
class SimpleComputePass : public VulkanLoopModifier {
   public:
    SimpleComputePass(VulkanContext* ctx, const char* shaderName, ImageArray* inputContainer, ImageArray* outputContainer, VkExtent2D localSize,
                      VkImageLayout outputLayout, std::vector<VkDescriptorSetLayout> otherDsls, DescriptorSetLayoutInfo initDsli);
    virtual void preRender(VulkanContext* ctx, uint32_t frameIndex) override {};
    void renderCmd(VkCommandBuffer buffer, uint32_t frameIndex, uint32_t swapchainIndex) override;
    void onSwapchainRebuild(VulkanContext* ctx) override;
    virtual void destroy_back(VulkanContext* ctx) override {};

   protected:
    DescriptorSetArray* dsa1 = nullptr;  // For input multiplicity
    DescriptorSetArray* dsa2 = nullptr;  // For output multiplicity (if applicable)
    uint32_t setidx;

   private:
    ImageArray* inputContainer;
    ImageArray* outputContainer;
    ComputePipeline* computePipeline;
    VkExtent2D localSize;
    VkImageLayout outputLayout;
    bool swapchainIndexing;
};
class SimpleFramerateMonitor : public VulkanLoopModifier {
   public:
    SimpleFramerateMonitor(double updatePeriodSeconds, double* frameratePtr) {
        this->period = updatePeriodSeconds;
        this->ptr = frameratePtr;
    }
    void preRender(VulkanContext* ctx, uint32_t frameIndex) override;
    void setUpdatePeriod(double period) { this->period = period; }
    void setFrameratePtr(double* ptr) { this->ptr = ptr; }

   private:
    double period;
    uint32_t frameCount = UINT32_MAX;
    utils::lepton2_time_point timePoint;
    double* ptr;
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

    uint32_t getNumFramesInFlight() { return inFlightResources.size(); }

    std::vector<loopmodifiers::VulkanLoopModifier*> loopModifiers;

   private:
    uint32_t inFlightFrameCount = 0;
    std::vector<InFlightResources> inFlightResources;

    friend class VulkanLoopModifier;
};
}  // namespace lepton2::vulkancore