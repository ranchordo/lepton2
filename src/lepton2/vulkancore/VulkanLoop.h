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
//! Adds additional functionality to a VulkanLoop.
/**
 * The VulkanLoop main rendering system takes place in three passes: `preRender`, `renderCmd` and `postRenderCmd`.
 * For each pass, all attached vulkan loop modifiers will have corresponding methods called in the order in which
 * they are placed into the VulkanLoop's internal modifiers list.
 */
class VulkanLoopModifier : public DeletableVulkanResource {
   public:
    virtual void preRender(VulkanContext* ctx, uint32_t frameIndex) {};
    virtual void renderCmd(VkCommandBuffer buffer, uint32_t frameIndex, uint32_t swapchainIndex) {};
    virtual void postRenderCmd(VkCommandBuffer buffer, uint32_t frameIndex, uint32_t swapchainIndex) {};
    virtual void onSwapchainRebuild(VulkanContext* ctx) {};
    virtual void destroy_back(VulkanContext* ctx) override {};
};
//! Run a render pass directly to an ImageArray* of render targets.
class SimpleRenderPass : public VulkanLoopModifier {
   public:
    //! \param targets If nullptr, the render pass framebuffers will not be rebuilt during `onSwapchainRebuild`.
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
//! Refill an array of render targets on every swapchain rebuild using a RenderTargetImageCreationInfo and a VkExtent2D reference.
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
//! Use a compute shader to transfer data between two image arrays.
/**
 * If the two image arrays have different multiplicities, the outputContainer will be separated into its own storage image descriptor
 * set and be accessed using swapchain frame indexing. Otherwise, both the initDsli and input/outputContainers will be bound into the
 * same descriptor set accessed by `frameIndex`. A set of other descriptor set layouts may be passed which must be bound outside the
 * compute pass.
 */
class SimpleComputePass : public VulkanLoopModifier {
   public:
    SimpleComputePass(VulkanContext* ctx, const char* shaderName, ImageArray* inputContainer, ImageArray* outputContainer, VkExtent2D localSize,
                      VkImageLayout outputLayout, std::vector<VkDescriptorSetLayout> otherDsls, const DescriptorSetLayoutInfo& initDsli);
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
//! Print out the average framerate (in Hz) periodically and store it to a pointer if not null.
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