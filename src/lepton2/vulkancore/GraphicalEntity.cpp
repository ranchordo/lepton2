#include "GraphicalEntity.h"

#include "RenderState.h"

using namespace lepton2::vulkancore;

void GraphicalConfiguration::renderAllUsers(RenderState* renderState, VkCommandBuffer commandBuffer, uint32_t frameIndex) {
    this->pipeline->bind(commandBuffer);
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

GraphicalConfiguration* GraphicalConfigurationStore::createNewConfiguration(RenderState* renderState, PipelineInfo pipelineInfo) {
    GraphicalConfiguration* newConfiguration = new GraphicalConfiguration();
    newConfiguration->layoutReference = new DescriptorSetArray(pipelineInfo.dsaLayout);
    newConfiguration->layoutReference->buildDescriptorSetLayout(renderState->ctx);

    PipelineInfo newPipelineInfo(pipelineInfo);
    newPipelineInfo.descriptorSetLayout = newConfiguration->layoutReference->descriptorSetLayout;
    newConfiguration->pipeline = new GraphicsPipeline(renderState->ctx, parent->getSubpassIndex(), renderState->renderPass, newPipelineInfo);
    return newConfiguration;
}

GraphicalConfigurationHandle GraphicalConfigurationStore::getConfiguration(RenderState* renderState, PipelineInfo pipelineInfo, GraphicalEntity* user) {
    std::unordered_set<GraphicalConfiguration*>* selectedVector = nullptr;
    if (this->cache.count(pipelineInfo.shaderName) == 0) {
        std::unordered_set<GraphicalConfiguration*> newTempVector;
        this->cache[pipelineInfo.shaderName] = newTempVector;
        selectedVector = &this->cache[pipelineInfo.shaderName];
    } else {
        std::unordered_set<GraphicalConfiguration*>& candidates = this->cache[pipelineInfo.shaderName];
        for (GraphicalConfiguration* candidate : candidates) {
            if (!DescriptorSetArray::isLayoutCompatible(pipelineInfo.dsaLayout.bindings, candidate->pipeline->creationInfo.dsaLayout.bindings)) {
                continue;
            }
            if (pipelineInfo.samples != candidate->pipeline->creationInfo.samples) {
                continue;
            }
            if (pipelineInfo.useStencilTesting != candidate->pipeline->creationInfo.useStencilTesting) {
                continue;
            }
            if (pipelineInfo.useStencilTesting) {
                throw std::runtime_error("Can't compare stencil test states yet.");
            }
            if (pipelineInfo.polygonMode != candidate->pipeline->creationInfo.polygonMode) {
                continue;
            }
            if (pipelineInfo.frontFace != candidate->pipeline->creationInfo.frontFace) {
                continue;
            }
            if (pipelineInfo.cullMode != candidate->pipeline->creationInfo.cullMode) {
                continue;
            }
            candidate->users.insert(user);
            GraphicalConfigurationHandle ret;
            ret.user = user;
            ret.config = candidate;
            ret.store = this;
            return ret;
        }
        selectedVector = &candidates;
    }
    GraphicalConfiguration* newConfiguration = this->createNewConfiguration(renderState, pipelineInfo);
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
    if (this->cache.count(configuration.config->pipeline->creationInfo.shaderName) == 0) {
        return;
    }
    configuration.config->users.erase(configuration.user);
    if (configuration.config->users.size() == 0) {
        std::unordered_set<GraphicalConfiguration*>& configurations = cache[configuration.config->pipeline->creationInfo.shaderName];
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

void GraphicalEntity::initialize(RenderGraphNode* node, RenderState* renderState, uint32_t numDescriptors) {
    PipelineInfo req = this->getPipelineRequirements();
    this->pipelineData = node->configurationStore.getConfiguration(renderState, req, this);
    this->dsa = new DescriptorSetArray(this->pipelineData.config->layoutReference);
    renderState->ctx->descriptorPoolManager.allocateDescriptorSets(renderState->ctx, this->dsa, numDescriptors);
    this->postInit(node, renderState);
}

void GraphicalEntity::render(RenderState* renderState, VkCommandBuffer commandBuffer, uint32_t frameIndex) {
    if (this->numInstances == 0) {
        return;
    }
    this->preRender(renderState, frameIndex);
    this->pipelineData.config->pipeline->bindDescriptorSet(commandBuffer, this->dsa, frameIndex);
    this->objectData->bind(commandBuffer, 0);
    vkCmdDrawIndexed(commandBuffer, this->objectData->getNumIndices(), this->numInstances, 0, 0, 0);
}

void GraphicalEntity::destroyEntityResources(VulkanContext* ctx) {
    this->dsa->destroy(ctx);
    delete this->dsa;
    this->pipelineData.store->freeConfiguration(ctx, this->pipelineData);
}