#include "GraphicalPresets.h"

using namespace lepton2::vulkancore;

std::vector<Vertex> screenVertices = {
    {{-1.0f, -1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
    {{+1.0f, -1.0f, 0.0f}, {1.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
    {{+1.0f, +1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
    {{-1.0f, +1.0f, 0.0f}, {0.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}};

std::vector<uint32_t> screenIndices = {1, 0, 3, 1, 3, 2};


StaticScreenEntity::StaticScreenEntity(VulkanContext* ctx, const char* shaderName, ColorAttachmentInfo* colorAttachmentInfo) {
    this->shaderName = shaderName;
    this->colorAttachmentInfo = colorAttachmentInfo;
    this->objectData = new DeviceObjectData(ctx, { screenVertices, screenIndices });
}