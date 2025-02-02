#include "GraphicalEntity.h"

#include "../vulkancore/RenderState.h"

using namespace lepton2::vulkancore;
using namespace lepton2::graphics;

void GraphicalEntity::initialize(GraphicalConfigurationStore* store, RenderGraphNode* node, RenderState* renderState) {
    PipelineConstraints req = this->getPipelineRequirements();
    SubpassGraphicalConfigurationStore* subpassStore = store->subpassStores[renderState].at(node->getSubpassIndex());
    this->pipelineData = subpassStore->getConfiguration(renderState, req, this);
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