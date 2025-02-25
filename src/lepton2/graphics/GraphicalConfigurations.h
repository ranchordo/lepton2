#pragma once

#include "../vulkancore/Descriptors.h"
#include "../vulkancore/ObjectData.h"
#include "../vulkancore/Pipelines.h"
#include "../vulkancore/RenderState.h"

namespace lepton2::graphics {

namespace vkc = lepton2::vulkancore;

class GraphicalEntity;
struct GraphicalConfiguration : public vkc::DeletableVulkanResource {
    std::unordered_set<GraphicalEntity*> users;
    vkc::GraphicsPipeline* pipeline = nullptr;
    vkc::DescriptorSetArray* layoutReference = nullptr;
    void renderAllUsers(vkc::RenderState* renderState, VkCommandBuffer commandBuffer, uint32_t frameIndex);
    void destroy_back(vkc::VulkanContext* ctx) override;
};

class SubpassGraphicalConfigurationStore;
struct GraphicalConfigurationHandle {
    GraphicalConfiguration* config;
    GraphicalEntity* user;
    SubpassGraphicalConfigurationStore* store;
};

class SubpassGCSRenderCallback : public vkc::SubpassRenderCallback {
   public:
    SubpassGCSRenderCallback(SubpassGraphicalConfigurationStore* store) {
        this->store = store;
    }
    void renderSubpassCmd(VkCommandBuffer commandBuffer, vkc::RenderState* pass, uint32_t swapChainFrameIndex) override;

   private:
    SubpassGraphicalConfigurationStore* store;
};

class SubpassGraphicalConfigurationStore : public vkc::DeletableVulkanResource {
   public:
    SubpassGraphicalConfigurationStore(vkc::RenderGraphNode* node) : renderCallback(this) {
        this->parent = node;
    }
    GraphicalConfigurationHandle getConfiguration(vkc::RenderState* renderState, vkc::PipelineConstraints constraints, GraphicalEntity* user);
    void freeConfiguration(vkc::VulkanContext* ctx, GraphicalConfigurationHandle configuration);
    void renderAllConfigurations(vkc::RenderState* renderState, VkCommandBuffer commandBuffer, uint32_t frameIndex);
    void destroy_back(vkc::VulkanContext* ctx) override;
    vkc::SubpassRenderCallback* getRenderCallback() { return &this->renderCallback; }

   private:
    GraphicalConfiguration* createNewConfiguration(vkc::RenderState* renderState, vkc::PipelineConstraints constraints);
    std::unordered_map<std::string, std::unordered_set<GraphicalConfiguration*>> cache;
    uint32_t currentIdentifier;
    vkc::RenderGraphNode* parent;
    SubpassGCSRenderCallback renderCallback;
};

class GraphicalConfigurationStore : public vkc::DeletableVulkanResource {
   public:
    std::unordered_map<vkc::RenderState*, std::vector<SubpassGraphicalConfigurationStore*>> subpassStores;
    void addAllSubpasses(vkc::RenderState* pass);
    void destroy_back(vkc::VulkanContext* ctx) override {}
};

}  // namespace lepton2::graphics