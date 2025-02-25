#include "GraphicalPresets.h"

#include "../vulkancore/Textures.h"

using namespace lepton2::graphics::graphicalpresets;
using namespace lepton2::graphics;
using namespace lepton2::vulkancore;

namespace lepton2::graphics::graphicalpresets {
VertexStructDescriptor simplePresetVsd = {{{offsetof(SimplePresetVertex, pos), VK_FORMAT_R32G32B32_SFLOAT},
                                           {offsetof(SimplePresetVertex, texcoord), VK_FORMAT_R32G32_SFLOAT}},
                                          sizeof(SimplePresetVertex)};
VertexStructDescriptor objLoadVsd = {{{offsetof(ObjLoadVertex, pos), VK_FORMAT_R32G32B32_SFLOAT},
                                      {offsetof(ObjLoadVertex, texcoord), VK_FORMAT_R32G32_SFLOAT},
                                      {offsetof(ObjLoadVertex, normal), VK_FORMAT_R32G32B32_SFLOAT}},
                                     sizeof(ObjLoadVertex)};
};  // namespace lepton2::graphics::graphicalpresets

std::vector<SimplePresetVertex> screenVertices = {
    {{-1.0f, -1.0f, 0.0f}, {0.0f, 0.0f}},
    {{+1.0f, -1.0f, 0.0f}, {1.0f, 0.0f}},
    {{+1.0f, +1.0f, 0.0f}, {1.0f, 1.0f}},
    {{-1.0f, +1.0f, 0.0f}, {0.0f, 1.0f}}};

std::vector<uint32_t> screenIndices = {1, 0, 3, 1, 3, 2};

std::vector<vkc::ColorAttachmentInfo*> StaticScreenEntity::fromNodeOutputs(vkc::RenderGraphNode* node) {
    std::vector<vkc::ColorAttachmentInfo*> ret;
    for (uint32_t i = 0; i < node->getColorAttachments()->size(); i++) {
        ret.push_back(&node->getColorAttachments()->at(i));
    }
    return ret;
}

PipelineConstraints StaticScreenEntity::getPipelineRequirements() {
    DescriptorSetLayoutInfo dsli;
    for (uint32_t i = 0; i < this->colorAttachmentInfo.size(); i++) {
        DescriptorInfo descInfo;
        descInfo.inputAttachmentData.colorAttachmentInfo = this->colorAttachmentInfo[i];
        descInfo.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        descInfo.shaderStages = VK_SHADER_STAGE_FRAGMENT_BIT;
        dsli.addNewBinding(descInfo);
    }
    PipelineConstraints req(this->shaderName, dsli, simplePresetVsd);
    return req;
}

StaticScreenEntity::StaticScreenEntity(VulkanContext* ctx, const char* shaderName, std::vector<ColorAttachmentInfo*> colorAttachmentInfo) {
    this->shaderName = shaderName;
    this->colorAttachmentInfo = colorAttachmentInfo;
    HostObjectData hostData(screenVertices.data(), screenVertices.size() * sizeof(SimplePresetVertex), screenIndices);
    DeviceObjectData* objectData = new DeviceObjectData(ctx, &hostData);
    this->addLinkedResource(objectData, true);
    this->setObjectData(objectData);
}

GenericEntity::GenericEntity(vkc::VulkanContext* ctx, const char* shaderName, DeviceObjectData* objectData, vkc::VertexStructDescriptor vsd) {
    if (objectData != nullptr) this->setObjectData(objectData);
    this->shaderName = shaderName;
    this->vsd = vsd;
}

PipelineConstraints GenericEntity::getPipelineRequirements() {
    DescriptorSetLayoutInfo dsli;
    {
        DescriptorInfo descInfo;
        descInfo.uniformBufferData.bufferSize = ubo_size;
        descInfo.uniformBufferData.createNewBuffer = true;
        descInfo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descInfo.shaderStages = VK_SHADER_STAGE_VERTEX_BIT;
        dsli.addNewBinding(descInfo);
    }
    if (this->texture != nullptr) {
        for (uint32_t i = 0; i < this->texture->components.size(); i++) {
            DescriptorInfo descInfo;
            descInfo.imageSamplerData.container = this->texture;
            descInfo.imageSamplerData.componentIndex = i;
            descInfo.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descInfo.shaderStages = VK_SHADER_STAGE_FRAGMENT_BIT;
            dsli.addNewBinding(descInfo);
        }
    }
    PipelineConstraints ret(this->shaderName, dsli, this->vsd);
    return ret;
}

void GenericEntity::preRender(RenderState* renderState, SingleDescriptorSet* sds, uint32_t scfi) {
    VulkanBuffer* uniformBuffer = ((descriptortypes::UniformBufferDescriptor*)(sds->instances[0]))->uniformBuffer;
    void* data = uniformBuffer->chonklet.mapMemory(renderState->ctx, 0);
    memcpy(data, ubo, ubo_size);
    uniformBuffer->chonklet.unmapMemory(renderState->ctx);
}