#pragma once

#include "GraphicalEntity.h"
#include "RenderState.h"

namespace lepton2::vulkancore {

class StaticScreenEntity : public GraphicalEntity {
   public:
    StaticScreenEntity(VulkanContext* ctx, const char* shaderName, ColorAttachmentInfo* colorAttachmentInfo);
    PipelineInfo getPipelineRequirements() override {
        DescriptorInfo descInfo;
        descInfo.colorAttachmentInfo = this->colorAttachmentInfo;
        descInfo.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        DescriptorSetLayoutInfo dsli;
        dsli.addNewBinding(descInfo, VK_SHADER_STAGE_FRAGMENT_BIT, 1);
        PipelineInfo req(this->shaderName, dsli, nullptr, VK_SAMPLE_COUNT_1_BIT, VK_FALSE, {}, VK_POLYGON_MODE_FILL, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_CULL_MODE_NONE);
        return req;
    }

    void destroy_back(VulkanContext* ctx) override {
        this->objectData->destroy(ctx);
        delete this->objectData;
        this->destroyEntityResources(ctx);
    }

   private:
    ColorAttachmentInfo* colorAttachmentInfo;
    const char* shaderName;
};
};  // namespace lepton2::vulkancore