#include "GraphicalEntity.h"

#include "../vulkancore/RenderPass.h"

using namespace lepton2::vulkancore;
using namespace lepton2::graphics;

void GraphicalEntity::initialize(VulkanContext* ctx, RenderPass* renderState, RenderGraphNode* node, GraphicalConfigurationStore* store) {
    PipelineConstraints req = this->getPipelineRequirements();
    SubpassGraphicalConfigurationStore* subpassStore = store->subpassStores[renderState].at(node->getSubpassIndex());
    this->pipelineData = subpassStore->getConfiguration(ctx, renderState, req, this);
    this->pipelineData.config->users.push_back(this);
    this->dsa = new DescriptorSetArray(this->pipelineData.config->layoutReference);
    uint32_t numDescriptors = ctx->swapChain.swapChainImages.size();
    ctx->descriptorPoolManager.allocateDescriptorSets(ctx, this->dsa, numDescriptors);
    this->postInit(ctx, renderState, node);
}

void GraphicalEntity::render(VkCommandBuffer commandBuffer, uint32_t scfi, uint32_t setidx) {
    if (this->numInstances == 0) {
        return;
    }
    VkPipelineLayout pipelineLayout = this->pipelineData.config->pipeline->getPipelineLayout();
    this->pipelineData.config->pipeline->bindDescriptorSet(commandBuffer, pipelineLayout, this->dsa, scfi, setidx);
    this->objectData->bind(commandBuffer, 0);
    vkCmdDrawIndexed(commandBuffer, this->objectData->getNumIndices(), this->numInstances, 0, 0, 0);
}

void GraphicalEntity::destroyEntityResources(VulkanContext* ctx) {
    this->dsa->destroy(ctx);
    delete this->dsa;
    this->pipelineData.store->dropConfigurationHandle(ctx, this->pipelineData);
}