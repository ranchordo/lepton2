#pragma once

#include "GraphicalEntity.h"
#include "RenderState.h"

namespace lepton2::vulkancore::graphicalpresets {

struct SimplePresetVertex {
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 texcoord;
};

// extern VertexStructDescriptor simplePresetVsd;

class StaticScreenEntity : public GraphicalEntity {
   public:
    StaticScreenEntity(VulkanContext* ctx, const char* shaderName, ColorAttachmentInfo* colorAttachmentInfo);
    PipelineConstraints getPipelineRequirements() override;

    void destroy_back(VulkanContext* ctx) override {
        this->destroyEntityResources(ctx);
    }

   private:
    ColorAttachmentInfo* colorAttachmentInfo;
    const char* shaderName;
};
};  // namespace lepton2::vulkancore::graphicalpresets