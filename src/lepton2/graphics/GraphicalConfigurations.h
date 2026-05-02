#pragma once

#include "../vulkancore/Descriptors.h"
#include "../vulkancore/ObjectData.h"
#include "../vulkancore/Pipelines.h"
#include "../vulkancore/RenderPass.h"

namespace lepton2::graphics {

namespace vkc = lepton2::vulkancore;

class GraphicalEntity;
struct GraphicalConfiguration : public vkc::DeletableVulkanResource {
    std::vector<GraphicalEntity*> users;
    vkc::GraphicsPipeline* pipeline = nullptr;
    vkc::DescriptorSetArray* layoutReference = nullptr;

    void renderAllUsers(VkCommandBuffer commandBuffer, uint32_t scfi, uint32_t setidx);
    void preRenderAllUsers(vkc::VulkanContext* ctx, uint32_t scfi);
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
    void renderSubpassCmd(VkCommandBuffer commandBuffer, vkc::RenderPass* pass, uint32_t scfi, uint32_t setidx) override;
    void preRenderSubpass(vkc::VulkanContext* ctx, uint32_t scfi) override;

   private:
    SubpassGraphicalConfigurationStore* store;
};

class SubpassGraphicalConfigurationStore : public vkc::DeletableVulkanResource {
   public:
    SubpassGraphicalConfigurationStore(vkc::RenderGraphNode* node) : renderCallback(this) {
        this->parent = node;
    }
    GraphicalConfigurationHandle getConfiguration(vkc::VulkanContext* ctx, vkc::RenderPass* renderState,
                                                  vkc::PipelineConstraints constraints, GraphicalEntity* user);
    void dropConfigurationHandle(vkc::VulkanContext* ctx, GraphicalConfigurationHandle configuration);
    void renderAllConfigurations(VkCommandBuffer commandBuffer, uint32_t scfi, uint32_t setidx);
    void preRenderAllConfigurations(vkc::VulkanContext* ctx, uint32_t scfi);
    void destroy_back(vkc::VulkanContext* ctx) override;
    vkc::SubpassRenderCallback* getRenderCallback() { return &this->renderCallback; }

   private:
    GraphicalConfiguration* createNewConfiguration(vkc::VulkanContext* ctx, vkc::RenderPass* renderState, vkc::PipelineConstraints constraints);
    std::unordered_map<std::string, std::unordered_set<GraphicalConfiguration*>> cache;
    std::vector<GraphicalConfiguration*> allConfigs;
    uint32_t currentIdentifier;
    vkc::RenderGraphNode* parent;
    SubpassGCSRenderCallback renderCallback;
};

class GraphicalConfigurationStore : public vkc::DeletableVulkanResource {
   public:
    std::unordered_map<vkc::RenderPass*, std::vector<SubpassGraphicalConfigurationStore*>> subpassStores;
    void addAllSubpasses(vkc::RenderPass* pass);
    void destroy_back(vkc::VulkanContext* ctx) override {}
};

}  // namespace lepton2::graphics