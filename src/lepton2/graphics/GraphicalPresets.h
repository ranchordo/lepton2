#pragma once

#include "GraphicalEntity.h"
#include "../vulkancore/RenderState.h"

namespace lepton2::graphics::graphicalpresets {

namespace vkc = lepton2::vulkancore;

struct SimplePresetVertex {
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 texcoord;
};

// extern VertexStructDescriptor simplePresetVsd;

class StaticScreenEntity : public GraphicalEntity {
   public:
    StaticScreenEntity(vkc::VulkanContext* ctx, const char* shaderName, vkc::ColorAttachmentInfo* colorAttachmentInfo);
    vkc::PipelineConstraints getPipelineRequirements() override;

    void destroy_back(vkc::VulkanContext* ctx) override {
        this->destroyEntityResources(ctx);
    }

   private:
    vkc::ColorAttachmentInfo* colorAttachmentInfo;
    const char* shaderName;
};
};  // namespace lepton2::vulkancore::graphicalpresets