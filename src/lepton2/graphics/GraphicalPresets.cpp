#include "GraphicalPresets.h"

#include "../vulkancore/Textures.h"

using namespace lepton2::graphics::graphicalpresets;
using namespace lepton2::graphics;
using namespace lepton2::vulkancore;

namespace lepton2::graphics::graphicalpresets {
VertexStructDescriptor simplePresetVsd = {{{offsetof(SimplePresetVertex, pos), VK_FORMAT_R32G32B32_SFLOAT},
                                           {offsetof(SimplePresetVertex, texcoord), VK_FORMAT_R32G32_SFLOAT}},
                                          sizeof(SimplePresetVertex)};
};  // namespace lepton2::graphics::graphicalpresets

std::vector<SimplePresetVertex> screenVertices = {
    {{-1.0f, -1.0f, 0.0f}, {0.0f, 0.0f}},
    {{+1.0f, -1.0f, 0.0f}, {1.0f, 0.0f}},
    {{+1.0f, +1.0f, 0.0f}, {1.0f, 1.0f}},
    {{-1.0f, +1.0f, 0.0f}, {0.0f, 1.0f}}};

std::vector<uint32_t> screenIndices = {1, 0, 3, 1, 3, 2};

PipelineConstraints StaticScreenEntity::getPipelineRequirements() {
    DescriptorInfo descInfo;
    descInfo.inputAttachmentData.colorAttachmentInfo = this->colorAttachmentInfo;
    descInfo.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    descInfo.shaderStages = VK_SHADER_STAGE_FRAGMENT_BIT;
    DescriptorSetLayoutInfo dsli;
    dsli.addNewBinding(descInfo);
    PipelineConstraints req(this->shaderName, dsli, simplePresetVsd);
    return req;
}

StaticScreenEntity::StaticScreenEntity(VulkanContext* ctx, const char* shaderName, ColorAttachmentInfo* colorAttachmentInfo) {
    this->shaderName = shaderName;
    this->colorAttachmentInfo = colorAttachmentInfo;
    HostObjectData hostData(screenVertices.data(), screenVertices.size() * sizeof(SimplePresetVertex), screenIndices);
    DeviceObjectData* objectData = new DeviceObjectData(ctx, &hostData);
    this->addLinkedResource(objectData, true);
    this->setObjectData(objectData);
}

GenericSinglyTextured::GenericSinglyTextured(vkc::VulkanContext* ctx, const char* shaderName, DeviceObjectData* objectData, Texture* texture) {
    if (objectData != nullptr) this->setObjectData(objectData);
    this->shaderName = shaderName;
    this->texture = texture;
}

PipelineConstraints GenericSinglyTextured::getPipelineRequirements() {
    DescriptorSetLayoutInfo dsli;
    {
        DescriptorInfo descInfo;
        descInfo.uniformBufferData.bufferSize = sizeof(ubo);
        descInfo.uniformBufferData.createNewBuffer = true;
        descInfo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descInfo.shaderStages = VK_SHADER_STAGE_VERTEX_BIT;
        dsli.addNewBinding(descInfo);
    }
    for (uint32_t i = 0; i < this->texture->components.size(); i++) {
        DescriptorInfo descInfo;
        descInfo.imageSamplerData.container = this->texture;
        descInfo.imageSamplerData.componentIndex = i;
        descInfo.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descInfo.shaderStages = VK_SHADER_STAGE_FRAGMENT_BIT;
        dsli.addNewBinding(descInfo);
    }
    return PipelineConstraints(this->shaderName, dsli, simplePresetVsd);
}

void GenericSinglyTextured::preRender(RenderState* renderState, SingleDescriptorSet* sds, uint32_t scfi) {
    VulkanBuffer* uniformBuffer = ((descriptortypes::UniformBufferDescriptor*)(sds->instances[0]))->uniformBuffer;
    void* data = uniformBuffer->chonklet.mapMemory(renderState->ctx, 0);
    memcpy(data, &ubo, sizeof(ubo));
    uniformBuffer->chonklet.unmapMemory(renderState->ctx);
}