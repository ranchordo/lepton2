#include "GraphicalEntity.h"

#include "../vulkancore/RenderPass.h"
#include "../vulkancore/VulkanContext.h"

using namespace lepton2::vulkancore;
using namespace lepton2::graphics;

void GraphicalEntity::initialize(VulkanContext* ctx, RenderPass* renderState, RenderSubpass* node, GraphicalConfigurationStore* store) {
    GraphicsPipelineConstraints req = this->getPipelineRequirements();
    if (req.shaderName != nullptr) {
        SubpassGraphicalConfigurationStore* subpassStore = store->subpassStores[renderState].at(node->getSubpassIndex());
        this->pipelineData = subpassStore->getConfiguration(ctx, req, this);
        this->dsa = new DescriptorSetArray(this->pipelineData.config->getLayoutReference());
        uint32_t numDescriptors = renderState->getResourceMultiplicity();
        ctx->descriptorPoolManager.allocateDescriptorSets(ctx, this->dsa, numDescriptors);
    }
    this->postInit(ctx, renderState, node, store);
}

void GraphicalEntity::render(VkCommandBuffer commandBuffer, uint32_t frameIndex, uint32_t setidx) {
    if (!active) return;
    this->preRenderCmd(commandBuffer, frameIndex, setidx);
    if (this->numInstances == 0) {
        return;
    }
    VkPipelineLayout pipelineLayout = this->pipelineData.config->getPipeline()->getPipelineLayout();
    if (this->dsa->layoutInfo.bindings.size() > 0) {
        this->pipelineData.config->getPipeline()->bindDescriptorSet(commandBuffer, pipelineLayout, this->dsa, frameIndex, setidx);
    }
    this->objectData->bind(commandBuffer, 0);
    vkCmdDrawIndexed(commandBuffer, this->objectData->getNumIndices(), this->numInstances, 0, 0, 0);
}

void GraphicalEntity::destroyEntityResources(VulkanContext* ctx) {
    if (this->dsa != nullptr) {
        this->dsa->destroy(ctx);
        delete this->dsa;
    }
    if (this->pipelineData.store != nullptr) {
        this->pipelineData.store->dropConfigurationHandle(ctx, this->pipelineData);
    }
}