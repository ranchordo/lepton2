#include "GraphicalEntity.h"

using namespace lepton2::vulkancore;

GraphicalConfiguration* GraphicalConfigurationStore::getConfiguration(RenderState* renderState, PipelineInfo pipelineInfo, GraphicalEntity* user) {
    if (this->cache.count(pipelineInfo.shaderName) == 0) {
        GraphicalConfiguration* newConfiguration = new GraphicalConfiguration();
        newConfiguration->users.push_back(user);
        newConfiguration->pipeline = new GraphicsPipeline(renderState->ctx, parent->getSubpassIndex(), renderState->renderPass, pipelineInfo);
        newConfiguration->layoutReference = new DescriptorSetArray(pipelineInfo.dsaLayout);
        newConfiguration->layoutReference->buildDescriptorSetLayout(renderState->ctx);
    }
}