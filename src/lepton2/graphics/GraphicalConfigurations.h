#pragma once

#include "../vulkancore/Descriptors.h"
#include "../vulkancore/ObjectData.h"
#include "../vulkancore/Pipelines.h"
#include "../vulkancore/RenderPass.h"

namespace lepton2::graphics {

namespace vkc = lepton2::vulkancore;

class GraphicalEntity;
class SubpassGraphicalConfigurationStore;
class GraphicalConfiguration;

//! Holds a link between a GraphicalEntity and the GraphicalConfiguration it uses.
struct GraphicalConfigurationHandle {
    GraphicalConfiguration* config;
    GraphicalEntity* user;
    SubpassGraphicalConfigurationStore* store;

    void debugPrintAllBoundDescriptors();
};

//! The graphics submodule representation of a graphical pipeline, collected with the entities that use it.
class GraphicalConfiguration : public vkc::DeletableVulkanResource {
   public:
    void renderAllUsers(VkCommandBuffer commandBuffer, uint32_t frameIndex, uint32_t setidx);
    void preRenderAllUsers(vkc::VulkanContext* ctx, uint32_t frameIndex);
    void destroy_back(vkc::VulkanContext* ctx) override;

    vkc::DescriptorSetArray* getLayoutReference() { return this->layoutReference; }
    vkc::GraphicsPipeline* getPipeline() { return this->pipeline; }

   private:
    std::vector<GraphicalEntity*> users;
    vkc::GraphicsPipeline* pipeline = nullptr;
    vkc::DescriptorSetArray* layoutReference = nullptr;

    friend class SubpassGraphicalConfigurationStore;
    friend void GraphicalConfigurationHandle::debugPrintAllBoundDescriptors();
};

//! Render callback to inject into vulkancore render subpass.
class SubpassGCSRenderCallback : public vkc::SubpassRenderCallback {
   public:
    SubpassGCSRenderCallback(SubpassGraphicalConfigurationStore* store) {
        this->store = store;
    }
    void renderSubpassCmd(VkCommandBuffer commandBuffer, vkc::RenderPass* pass, uint32_t frameIndex, uint32_t setidx) override;
    void preRenderSubpass(vkc::VulkanContext* ctx, uint32_t frameIndex) override;

   private:
    SubpassGraphicalConfigurationStore* store;
};

//! Set of candidate graphical configurations registered within a render subpass.
class SubpassGraphicalConfigurationStore : public vkc::DeletableVulkanResource {
   public:
    SubpassGraphicalConfigurationStore(vkc::RenderSubpass* node, vkc::RenderPass* renderPass) : renderCallback(this) {
        this->parent = node;
        this->renderPass = renderPass;
    }
    GraphicalConfigurationHandle getConfiguration(vkc::VulkanContext* ctx, vkc::GraphicsPipelineConstraints constraints, GraphicalEntity* user);
    void dropConfigurationHandle(vkc::VulkanContext* ctx, GraphicalConfigurationHandle configuration);
    void renderAllConfigurations(VkCommandBuffer commandBuffer, uint32_t frameIndex, uint32_t setidx);
    void preRenderAllConfigurations(vkc::VulkanContext* ctx, uint32_t frameIndex);
    void destroy_back(vkc::VulkanContext* ctx) override;
    vkc::SubpassRenderCallback* getRenderCallback() { return &this->renderCallback; }

   private:
    GraphicalConfiguration* createNewConfiguration(vkc::VulkanContext* ctx, vkc::RenderPass* renderState, vkc::GraphicsPipelineConstraints constraints);
    std::unordered_map<std::string, std::unordered_set<GraphicalConfiguration*>> cache;
    std::vector<GraphicalConfiguration*> allConfigs;
    uint32_t currentIdentifier;
    vkc::RenderSubpass* parent;
    vkc::RenderPass* renderPass;
    SubpassGCSRenderCallback renderCallback;

    friend void GraphicalConfigurationHandle::debugPrintAllBoundDescriptors();
};

//! Collection of render passes, where each subpass is associated with a SubpassGraphicalConfigurationStore.
class GraphicalConfigurationStore : public vkc::DeletableVulkanResource {
   public:
    std::unordered_map<vkc::RenderPass*, std::vector<SubpassGraphicalConfigurationStore*>> subpassStores;
    void addAllSubpasses(vkc::RenderPass* pass);
    void destroy_back(vkc::VulkanContext* ctx) override {}
};

}  // namespace lepton2::graphics