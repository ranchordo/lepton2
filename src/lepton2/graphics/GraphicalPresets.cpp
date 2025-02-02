#include "GraphicalPresets.h"

using namespace lepton2::graphics::graphicalpresets;
using namespace lepton2::graphics;
using namespace lepton2::vulkancore;

VertexStructDescriptor simplePresetVsd = {{{offsetof(SimplePresetVertex, pos), VK_FORMAT_R32G32B32_SFLOAT},
                                           {offsetof(SimplePresetVertex, color), VK_FORMAT_R32G32B32_SFLOAT},
                                           {offsetof(SimplePresetVertex, texcoord), VK_FORMAT_R32G32_SFLOAT}},
                                          sizeof(SimplePresetVertex)};

std::vector<SimplePresetVertex> screenVertices = {
    {{-1.0f, -1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
    {{+1.0f, -1.0f, 0.0f}, {1.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
    {{+1.0f, +1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
    {{-1.0f, +1.0f, 0.0f}, {0.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}};

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