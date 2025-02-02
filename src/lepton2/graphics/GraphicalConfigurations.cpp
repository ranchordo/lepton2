#include "GraphicalConfigurations.h"

#include "../vulkancore/RenderState.h"
#include "GraphicalEntity.h"

using namespace lepton2::graphics;
using namespace lepton2::vulkancore;

void GraphicalConfiguration::renderAllUsers(RenderState* renderState, VkCommandBuffer commandBuffer, uint32_t frameIndex) {
    this->pipeline->bind(commandBuffer);
    // FIXME: Bind multiple descriptor sets with one command
    for (uint32_t i = 0; i < renderState->dsaQueue.size(); i++) {
        this->pipeline->bindDescriptorSet(commandBuffer, renderState->dsaQueue[i].second,
                                          frameIndex, renderState->dsaQueue[i].first);
    }
    renderState->dsaQueue.clear();
    for (GraphicalEntity* entity : this->users) {
        entity->render(renderState, commandBuffer, frameIndex);
    }
}

void GraphicalConfiguration::destroy_back(VulkanContext* ctx) {
    this->pipeline->destroy(ctx);
    delete this->pipeline;
    this->layoutReference->destroy(ctx);
    delete this->layoutReference;
}

GraphicalConfiguration* SubpassGraphicalConfigurationStore::createNewConfiguration(RenderState* renderState, PipelineConstraints constraints) {
    GraphicalConfiguration* newConfiguration = new GraphicalConfiguration();
    newConfiguration->layoutReference = new DescriptorSetArray(constraints.layoutInfo);
    newConfiguration->layoutReference->buildDescriptorSetLayout(renderState->ctx);

    std::vector<VkDescriptorSetLayout> dsl;
    dsl.push_back(newConfiguration->layoutReference->descriptorSetLayout);
    // Add subpass DSL
    if (parent->getSubpassDsa() != nullptr) {
        dsl.push_back(parent->getSubpassDsa()->descriptorSetLayout);
    }
    // Add pass DSL
    if (renderState->getPassDsa() != nullptr) {
        dsl.push_back(renderState->getPassDsa()->descriptorSetLayout);
    }
    PipelineInfo newPipelineInfo(dsl, constraints);
    newConfiguration->pipeline = new GraphicsPipeline(renderState->ctx, parent->getSubpassIndex(), renderState->renderPass, newPipelineInfo);
    return newConfiguration;
}

GraphicalConfigurationHandle SubpassGraphicalConfigurationStore::getConfiguration(RenderState* renderState, PipelineConstraints constraints, GraphicalEntity* user) {
    std::unordered_set<GraphicalConfiguration*>* selectedVector = nullptr;
    if (this->cache.count(constraints.shaderName) == 0) {
        std::unordered_set<GraphicalConfiguration*> newTempVector;
        this->cache[constraints.shaderName] = newTempVector;
        selectedVector = &this->cache[constraints.shaderName];
    } else {
        std::unordered_set<GraphicalConfiguration*>& candidates = this->cache[constraints.shaderName];
        for (GraphicalConfiguration* candidate : candidates) {
            if (!constraints.compatible(candidate->pipeline->creationConstraints)) continue;
            candidate->users.insert(user);
            GraphicalConfigurationHandle ret;
            ret.user = user;
            ret.config = candidate;
            ret.store = this;
            return ret;
        }
        selectedVector = &candidates;
    }
    GraphicalConfiguration* newConfiguration = this->createNewConfiguration(renderState, constraints);
    newConfiguration->users.insert(user);
    selectedVector->insert(newConfiguration);
    GraphicalConfigurationHandle ret;
    ret.user = user;
    ret.config = newConfiguration;
    ret.store = this;
    return ret;
}

void SubpassGCSRenderCallback::renderSubpassCmd(VkCommandBuffer commandBuffer, vkc::RenderState* pass, uint32_t swapChainFrameIndex) {
    this->store->renderAllConfigurations(pass, commandBuffer, swapChainFrameIndex);
}

void SubpassGraphicalConfigurationStore::freeConfiguration(VulkanContext* ctx, GraphicalConfigurationHandle configuration) {
    CHECK_DESTRUCTION();
    if (this->cache.count(configuration.config->pipeline->creationConstraints.shaderName) == 0) {
        return;
    }
    configuration.config->users.erase(configuration.user);
    if (configuration.config->users.size() == 0) {
        std::unordered_set<GraphicalConfiguration*>& configurations = cache[configuration.config->pipeline->creationConstraints.shaderName];
        configurations.erase(configuration.config);
        configuration.config->destroy(ctx);
        delete configuration.config;
    }
}

void SubpassGraphicalConfigurationStore::renderAllConfigurations(RenderState* renderState, VkCommandBuffer commandBuffer, uint32_t frameIndex) {
    for (std::pair<std::string, std::unordered_set<GraphicalConfiguration*>> const& pair : this->cache) {
        for (GraphicalConfiguration* config : pair.second) {
            config->renderAllUsers(renderState, commandBuffer, frameIndex);
        }
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

void GraphicalConfigurationStore::addPass(RenderState* pass) {
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