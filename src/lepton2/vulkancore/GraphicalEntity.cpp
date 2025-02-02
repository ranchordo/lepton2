#include "GraphicalEntity.h"

#include "RenderState.h"

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

GraphicalConfiguration* GraphicalConfigurationStore::createNewConfiguration(RenderState* renderState, PipelineConstraints constraints) {
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

GraphicalConfigurationHandle GraphicalConfigurationStore::getConfiguration(RenderState* renderState, PipelineConstraints constraints, GraphicalEntity* user) {
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

void GraphicalConfigurationStore::freeConfiguration(VulkanContext* ctx, GraphicalConfigurationHandle configuration) {
    CHECK_DESTRUCTION_VOID();
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

void GraphicalConfigurationStore::renderAllConfigurations(RenderState* renderState, VkCommandBuffer commandBuffer, uint32_t frameIndex) {
    for (std::pair<std::string, std::unordered_set<GraphicalConfiguration*>> const& pair : this->cache) {
        for (GraphicalConfiguration* config : pair.second) {
            config->renderAllUsers(renderState, commandBuffer, frameIndex);
        }
    }
}

void GraphicalConfigurationStore::destroy_back(VulkanContext* ctx) {
    for (std::pair<std::string, std::unordered_set<GraphicalConfiguration*>> const& pair : this->cache) {
        for (GraphicalConfiguration* config : pair.second) {
            config->destroy(ctx);
            delete config;
        }
    }
    this->cache.clear();
}

void GraphicalEntity::initialize(RenderGraphNode* node, RenderState* renderState) {
    PipelineConstraints req = this->getPipelineRequirements();
    this->pipelineData = node->configurationStore.getConfiguration(renderState, req, this);
    this->dsa = new DescriptorSetArray(this->pipelineData.config->layoutReference);
    uint32_t numDescriptors = renderState->ctx->swapChain.swapChainImages.size();
    renderState->ctx->descriptorPoolManager.allocateDescriptorSets(renderState->ctx, this->dsa, numDescriptors);
    this->postInit(node, renderState);
}

void GraphicalEntity::render(RenderState* renderState, VkCommandBuffer commandBuffer, uint32_t frameIndex) {
    if (this->numInstances == 0) {
        return;
    }
    this->preRender(renderState, frameIndex);
    this->pipelineData.config->pipeline->bindDescriptorSet(commandBuffer, this->dsa, frameIndex, 0);
    this->objectData->bind(commandBuffer, 0);
    vkCmdDrawIndexed(commandBuffer, this->objectData->getNumIndices(), this->numInstances, 0, 0, 0);
}

void GraphicalEntity::destroyEntityResources(VulkanContext* ctx) {
    this->dsa->destroy(ctx);
    delete this->dsa;
    this->pipelineData.store->freeConfiguration(ctx, this->pipelineData);
}