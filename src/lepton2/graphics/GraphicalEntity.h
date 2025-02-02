#pragma once

#include "../vulkancore/Descriptors.h"
#include "../vulkancore/ObjectData.h"
#include "../vulkancore/Pipelines.h"
#include "GraphicalConfigurations.h"

namespace lepton2::graphics {

namespace vkc = lepton2::vulkancore;

class GraphicalEntity : public vkc::DeletableVulkanResource {
   public:
    void initialize(GraphicalConfigurationStore* store, vkc::RenderGraphNode* node, vkc::RenderState* renderState);
    void render(vkc::RenderState* renderState, VkCommandBuffer commandBuffer, uint32_t frameIndex);

    void destroyEntityResources(vkc::VulkanContext* ctx);
    virtual void destroy_back(vkc::VulkanContext* ctx) override {
        this->destroyEntityResources(ctx);
    }

    vkc::DescriptorSetArray* dsa; // FIXME: No

   protected:
    virtual void postInit(vkc::RenderGraphNode* node, vkc::RenderState* renderState) {}
    virtual vkc::PipelineConstraints getPipelineRequirements() = 0;
    virtual void preRender(vkc::RenderState* renderState, uint32_t descriptorIndex) {}
    void setObjectData(vkc::DeviceObjectData* objectData) { this->objectData = objectData; }

   private:
    GraphicalConfigurationHandle pipelineData;
    vkc::DeviceObjectData* objectData = nullptr;
    uint32_t numInstances = 1;
};
}  // namespace lepton2::graphics