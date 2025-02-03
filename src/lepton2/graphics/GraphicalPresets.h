#pragma once

#include "../vulkancore/RenderState.h"
#include "GraphicalEntity.h"

namespace lepton2::graphics::graphicalpresets {

namespace vkc = lepton2::vulkancore;

struct SimplePresetVertex {
    glm::vec3 pos;
    glm::vec2 texcoord;

    bool operator==(const SimplePresetVertex& other) const {
        return pos == other.pos && texcoord == other.texcoord;
    }
};

extern vkc::VertexStructDescriptor simplePresetVsd;

class StaticScreenEntity : public GraphicalEntity {
   public:
    StaticScreenEntity(vkc::VulkanContext* ctx, const char* shaderName, vkc::ColorAttachmentInfo* colorAttachmentInfo);
    vkc::PipelineConstraints getPipelineRequirements() override;

    void destroy_back(vkc::VulkanContext* ctx) override { this->destroyEntityResources(ctx); }

   private:
    vkc::ColorAttachmentInfo* colorAttachmentInfo;
    const char* shaderName;
};

class GenericSinglyTextured : public GraphicalEntity {
   public:
    GenericSinglyTextured(vkc::VulkanContext* ctx, const char* shaderName, vkc::DeviceObjectData* objectData, vkc::Texture* texture);
    vkc::PipelineConstraints getPipelineRequirements() override;
    void preRender(vkc::RenderState* renderState, vkc::SingleDescriptorSet* sds, uint32_t scfi) override;
    virtual void logic() {};

    struct {
        glm::mat4 model = glm::mat4(1.0f);
    } ubo;
    

    void destroy_back(vkc::VulkanContext* ctx) override { this->destroyEntityResources(ctx); }

   private:
    const char* shaderName;
    vkc::Texture* texture;
};

};  // namespace lepton2::graphics::graphicalpresets

namespace std {
    template<> struct hash<lepton2::graphics::graphicalpresets::SimplePresetVertex> {
        size_t operator()(lepton2::graphics::graphicalpresets::SimplePresetVertex const& vertex) const {
            return hash<glm::vec3>()(vertex.pos) ^ (hash<glm::vec2>()(vertex.texcoord) << 1);
        }
    };
} // namespace std