#pragma once

#include "Descriptors.h"
#include "ObjectData.h"
#include "Pipelines.h"
#include "RenderState.h"

namespace lepton2::vulkancore {

class GraphicalEntity;
struct GraphicalConfiguration {
    std::vector<GraphicalEntity*> users;
    GraphicsPipeline* pipeline = nullptr;
    DescriptorSetArray* layoutReference = nullptr;
};

class GraphicalConfigurationStore : public DeletableVulkanResource {
   public:
    GraphicalConfigurationStore(RenderGraphNode* node) {
        this->parent = node;
    }
    GraphicalConfiguration* getConfiguration(RenderState* renderState, PipelineInfo pipelineInfo, GraphicalEntity* user);
    void freeConfiguration(VulkanContext* ctx, GraphicalConfiguration* configuration);
    void destroy_back(VulkanContext* ctx) override;

   private:
    std::unordered_map<std::string, std::vector<GraphicalConfiguration*>> cache;
    uint32_t currentIdentifier;
    RenderGraphNode* parent;
};

class GraphicalEntity : public DeletableVulkanResource {
   public:
    DescriptorSetArray* dsa;
    GraphicalConfiguration* pipelineData;
    DeviceObjectData* objectData = nullptr;
    virtual PipelineInfo getRequirements() = 0;
    virtual void onRender(uint32_t descriptorIndex) {}
    bool doRender = true;
    void destroyEntityResources(VulkanContext* ctx);
    virtual void destroy_back(VulkanContext* ctx) override {
        this->destroyEntityResources(ctx);
    }
};
}  // namespace lepton2::vulkancore