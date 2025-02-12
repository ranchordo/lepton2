#pragma once

#include "../vulkancore/RenderState.h"
#include "GraphicalEntity.h"

namespace lepton2::graphics::graphicalpresets {

namespace vkc = lepton2::vulkancore;

struct SimplePresetVertex {
    glm::vec3 pos;
    glm::vec2 texcoord;
};

struct ObjLoadVertex {
    glm::vec3 pos;
    glm::vec2 texcoord;
    glm::vec3 normal;

    bool operator==(const ObjLoadVertex& other) const {
        return pos == other.pos && texcoord == other.texcoord && normal == other.normal;
    }
};

extern vkc::VertexStructDescriptor simplePresetVsd;
extern vkc::VertexStructDescriptor objLoadVsd;

class StaticScreenEntity : public GraphicalEntity {
   public:
    StaticScreenEntity(vkc::VulkanContext* ctx, const char* shaderName, vkc::ColorAttachmentInfo* colorAttachmentInfo);
    vkc::PipelineConstraints getPipelineRequirements() override;

    void destroy_back(vkc::VulkanContext* ctx) override { this->destroyEntityResources(ctx); }

   private:
    vkc::ColorAttachmentInfo* colorAttachmentInfo;
    const char* shaderName;
};

class GenericEntity : public GraphicalEntity {
   public:
    GenericEntity(vkc::VulkanContext* ctx, const char* shaderName, vkc::DeviceObjectData* objectData, vkc::VertexStructDescriptor vsd = objLoadVsd);
    vkc::PipelineConstraints getPipelineRequirements() override;
    void preRender(vkc::RenderState* renderState, vkc::SingleDescriptorSet* sds, uint32_t scfi) override;
    void setTexture(vkc::Texture* texture) { this->texture = texture; }
    virtual void logic() {};

    void set_ubo(void* _ubo, size_t _ubo_size) {
        this->ubo = _ubo;
        this->ubo_size = _ubo_size;
    }

    struct {
        glm::mat4 model = glm::mat4(1.0f);
    } base_ubo;

    void destroy_back(vkc::VulkanContext* ctx) override { this->destroyEntityResources(ctx); }

   private:
    const char* shaderName;
    vkc::Texture* texture = nullptr;
    vkc::VertexStructDescriptor vsd;
    void* ubo = &this->base_ubo;
    size_t ubo_size = sizeof(base_ubo);
};

};  // namespace lepton2::graphics::graphicalpresets

namespace std {
template <>
struct hash<lepton2::graphics::graphicalpresets::ObjLoadVertex> {
    size_t operator()(lepton2::graphics::graphicalpresets::ObjLoadVertex const& vertex) const {
        return hash<glm::vec3>()(vertex.pos) ^
               ((hash<glm::vec2>()(vertex.texcoord) ^
                 (hash<glm::vec3>()(vertex.normal) << 1))
                << 1);
    }
};
}  // namespace std