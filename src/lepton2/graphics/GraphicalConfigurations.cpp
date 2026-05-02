#include "GraphicalConfigurations.h"

#include "../vulkancore/RenderPass.h"
#include "GraphicalEntity.h"

using namespace lepton2::graphics;
using namespace lepton2::vulkancore;

void GraphicalConfiguration::renderAllUsers(VkCommandBuffer commandBuffer, uint32_t scfi, uint32_t setidx) {
    this->pipeline->bind(commandBuffer);
    for (GraphicalEntity* entity : this->users) {
        entity->render(commandBuffer, scfi, setidx);
    }
}

void GraphicalConfiguration::preRenderAllUsers(VulkanContext* ctx, uint32_t scfi) {
    for (GraphicalEntity* entity : this->users) {
        entity->doPreRender(ctx, scfi);
    }
}

void GraphicalConfiguration::destroy_back(VulkanContext* ctx) {
    this->pipeline->destroy(ctx);
    delete this->pipeline;
    this->layoutReference->destroy(ctx);
    delete this->layoutReference;
}

GraphicalConfiguration* SubpassGraphicalConfigurationStore::createNewConfiguration(VulkanContext* ctx, RenderPass* renderState, PipelineConstraints constraints) {
    GraphicalConfiguration* newConfiguration = new GraphicalConfiguration();
    newConfiguration->layoutReference = new DescriptorSetArray(constraints.layoutInfo);
    newConfiguration->layoutReference->buildDescriptorSetLayout(ctx);

    std::vector<VkDescriptorSetLayout> dsl(renderState->getSuperpassLayouts());
    // Add pass DSL
    if (renderState->getPassDsa() != nullptr) {
        dsl.push_back(renderState->getPassDsa()->descriptorSetLayout);
    }
    // Add subpass DSL
    if (parent->getSubpassDsa() != nullptr) {
        dsl.push_back(parent->getSubpassDsa()->descriptorSetLayout);
    }
    // Add pipeline DSL
    dsl.push_back(newConfiguration->layoutReference->descriptorSetLayout);
    PipelineInfo newPipelineInfo(dsl, constraints);
    newConfiguration->pipeline = new GraphicsPipeline(ctx, this->parent, renderState->renderPass, newPipelineInfo);
    return newConfiguration;
}

GraphicalConfigurationHandle SubpassGraphicalConfigurationStore::getConfiguration(VulkanContext* ctx, RenderPass* renderState,
                                                                                  PipelineConstraints constraints, GraphicalEntity* user) {
    std::unordered_set<GraphicalConfiguration*>* selectedVector = nullptr;
    if (this->cache.count(constraints.shaderName) == 0) {
        std::unordered_set<GraphicalConfiguration*> newTempVector;
        this->cache[constraints.shaderName] = newTempVector;
        selectedVector = &this->cache[constraints.shaderName];
    } else {
        std::unordered_set<GraphicalConfiguration*>& candidates = this->cache[constraints.shaderName];
        for (GraphicalConfiguration* candidate : candidates) {
            if (!constraints.compatible(candidate->pipeline->creationConstraints)) continue;
            GraphicalConfigurationHandle ret;
            ret.user = user;
            ret.config = candidate;
            ret.store = this;
            return ret;
        }
        selectedVector = &candidates;
    }
    GraphicalConfiguration* newConfiguration = this->createNewConfiguration(ctx, renderState, constraints);
    selectedVector->insert(newConfiguration);
    this->allConfigs.push_back(newConfiguration);
    GraphicalConfigurationHandle ret;
    ret.user = user;
    ret.config = newConfiguration;
    ret.store = this;
    return ret;
}

void SubpassGCSRenderCallback::renderSubpassCmd(VkCommandBuffer commandBuffer, RenderPass* pass, uint32_t scfi, uint32_t setidx) {
    this->store->renderAllConfigurations(commandBuffer, scfi, setidx);
}

void SubpassGCSRenderCallback::preRenderSubpass(VulkanContext* ctx, uint32_t scfi) {
    this->store->preRenderAllConfigurations(ctx, scfi);
}

void SubpassGraphicalConfigurationStore::dropConfigurationHandle(VulkanContext* ctx, GraphicalConfigurationHandle handle) {
    CHECK_DESTRUCTION();
    if (this->cache.count(handle.config->pipeline->creationConstraints.shaderName) == 0) {
        return;
    }

    auto elem = std::find(handle.config->users.begin(), handle.config->users.end(), handle.user);
    if (elem != handle.config->users.end()) {
        std::iter_swap(elem, handle.config->users.end() - 1);
        handle.config->users.pop_back();
    }
    if (handle.config->users.size() == 0) {
        std::unordered_set<GraphicalConfiguration*>& configurations = cache[handle.config->pipeline->creationConstraints.shaderName];
        configurations.erase(handle.config);
        auto elem2 = std::find(allConfigs.begin(), allConfigs.end(), handle.config);
        if (elem2 != allConfigs.end()) {
            std::iter_swap(elem2, allConfigs.end() - 1);
            allConfigs.pop_back();
        }
        handle.config->destroy(ctx);
        delete handle.config;
    }
}

void SubpassGraphicalConfigurationStore::renderAllConfigurations(VkCommandBuffer commandBuffer, uint32_t scfi, uint32_t setidx) {
    for (GraphicalConfiguration* config : allConfigs) {
        config->renderAllUsers(commandBuffer, scfi, setidx);
    }
}

void SubpassGraphicalConfigurationStore::preRenderAllConfigurations(VulkanContext* ctx, uint32_t scfi) {
    for (GraphicalConfiguration* config : allConfigs) {
        config->preRenderAllUsers(ctx, scfi);
    }
}

void SubpassGraphicalConfigurationStore::destroy_back(VulkanContext* ctx) {
    for (std::pair<std::string, std::unordered_set<GraphicalConfiguration*>> const& pair : this->cache) {
        for (GraphicalConfiguration* config : pair.second) {
            config->destroy(ctx);
            delete config;
        }
    }
    this->cache.clear();
}

void GraphicalConfigurationStore::addAllSubpasses(RenderPass* pass) {
    if (this->subpassStores.count(pass) > 0) return;
    std::vector<SubpassGraphicalConfigurationStore*> stores(pass->numSubpasses());
    for (uint32_t i = 0; i < pass->numSubpasses(); i++) {
        SubpassGraphicalConfigurationStore* store = new SubpassGraphicalConfigurationStore(pass->getNode(i));
        this->addLinkedResource(store, true);
        stores[i] = store;
        pass->getNode(i)->setRenderCallback(store->getRenderCallback());
    }
    subpassStores[pass] = stores;
}