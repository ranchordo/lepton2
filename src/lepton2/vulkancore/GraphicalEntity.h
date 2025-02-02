#pragma once

#include "Descriptors.h"
#include "ObjectData.h"
#include "Pipelines.h"

namespace lepton2::vulkancore {

class RenderState;
class RenderGraphNode;
class GraphicalEntity;
struct GraphicalConfiguration : public DeletableVulkanResource {
    std::unordered_set<GraphicalEntity*> users;
    GraphicsPipeline* pipeline = nullptr;
    DescriptorSetArray* layoutReference = nullptr;
    void renderAllUsers(RenderState* renderState, VkCommandBuffer commandBuffer, uint32_t frameIndex);
    void destroy_back(VulkanContext* ctx) override;
};

class GraphicalConfigurationStore;
struct GraphicalConfigurationHandle {
    GraphicalConfiguration* config;
    GraphicalEntity* user;
    GraphicalConfigurationStore* store;
};

class GraphicalConfigurationStore : public DeletableVulkanResource {
   public:
    GraphicalConfigurationStore(RenderGraphNode* node) {
        this->parent = node;
    }
    GraphicalConfigurationHandle getConfiguration(RenderState* renderState, PipelineConstraints constraints, GraphicalEntity* user);
    void freeConfiguration(VulkanContext* ctx, GraphicalConfigurationHandle configuration);
    void renderAllConfigurations(RenderState* renderState, VkCommandBuffer commandBuffer, uint32_t frameIndex);
    void destroy_back(VulkanContext* ctx) override;

   private:
    GraphicalConfiguration* createNewConfiguration(RenderState* renderState, PipelineConstraints constraints);
    std::unordered_map<std::string, std::unordered_set<GraphicalConfiguration*>> cache;
    uint32_t currentIdentifier;
    RenderGraphNode* parent;
};

class GraphicalEntity : public DeletableVulkanResource {
   public:
    void initialize(RenderGraphNode* node, RenderState* renderState);
    void render(RenderState* renderState, VkCommandBuffer commandBuffer, uint32_t frameIndex);

    void destroyEntityResources(VulkanContext* ctx);
    virtual void destroy_back(VulkanContext* ctx) override {
        this->destroyEntityResources(ctx);
    }

    DescriptorSetArray* dsa; // FIXME: No

   protected:
    virtual void postInit(RenderGraphNode* node, RenderState* renderState) {}
    virtual PipelineConstraints getPipelineRequirements() = 0;
    virtual void preRender(RenderState* renderState, uint32_t descriptorIndex) {}
    void setObjectData(DeviceObjectData* objectData) { this->objectData = objectData; }

   private:
    GraphicalConfigurationHandle pipelineData;
    DeviceObjectData* objectData = nullptr;
    uint32_t numInstances = 1;
};
}  // namespace lepton2::vulkancore