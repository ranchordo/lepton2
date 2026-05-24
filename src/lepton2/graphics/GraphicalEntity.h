#pragma once

#include "../vulkancore/Descriptors.h"
#include "../vulkancore/ObjectData.h"
#include "../vulkancore/Pipelines.h"
#include "GraphicalConfigurations.h"

namespace lepton2::graphics {

namespace vkc = lepton2::vulkancore;

//! Entity object under the 3d graphics submodule, rendered through its registered configuration.
class GraphicalEntity : public vkc::DeletableVulkanResource {
   public:
    void initialize(vkc::VulkanContext* ctx, vkc::RenderPass* renderState, vkc::RenderSubpass* node, GraphicalConfigurationStore* store);
    void render(VkCommandBuffer commandBuffer, uint32_t frameIndex, uint32_t setidx);
    void doPreRender(vkc::VulkanContext* ctx, uint32_t frameIndex) {
        this->preRender(ctx, &dsa->singleDescriptorSets[frameIndex], frameIndex);
    }

    GraphicalConfigurationHandle& getConfigurationHandle() { return pipelineData; }

    void destroyEntityResources(vkc::VulkanContext* ctx);
    virtual void destroy_back(vkc::VulkanContext* ctx) override {
        this->destroyEntityResources(ctx);
    }

   protected:
    virtual void postInit(vkc::VulkanContext* ctx, vkc::RenderPass* renderState, vkc::RenderSubpass* node) {}
    virtual vkc::GraphicsPipelineConstraints getPipelineRequirements() = 0;
    virtual void preRender(vkc::VulkanContext* ctx, vkc::SingleDescriptorSet* sds, uint32_t frameIndex) {} //!< Typically for manipulation of entity data on each frame.
    virtual void preRenderCmd(VkCommandBuffer commandBuffer, uint32_t frameIndex, uint32_t setidx) {}
    void setObjectData(vkc::DeviceObjectData* objectData) { this->objectData = objectData; }

   private:
    GraphicalConfigurationHandle pipelineData;
    vkc::DeviceObjectData* objectData = nullptr;
    uint32_t numInstances = 1;
    vkc::DescriptorSetArray* dsa;

    friend void GraphicalConfigurationHandle::debugPrintAllBoundDescriptors();
};

}  // namespace lepton2::graphics