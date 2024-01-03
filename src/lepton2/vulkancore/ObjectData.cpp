#include "ObjectData.h"

using namespace lepton2::vulkancore;

VkVertexInputBindingDescription Vertex::getBindingDescription() {
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return bindingDescription;
}

std::vector<VkVertexInputAttributeDescription> Vertex::getAttributeDescriptions() {
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};
    VkVertexInputAttributeDescription viad{};
    viad.binding = 0;
    viad.location = 0;
    viad.format = VK_FORMAT_R32G32B32_SFLOAT;
    viad.offset = offsetof(Vertex, pos);
    attributeDescriptions.push_back(viad);
    viad.binding = 0;
    viad.location = 1;
    viad.format = VK_FORMAT_R32G32B32_SFLOAT;
    viad.offset = offsetof(Vertex, color);
    attributeDescriptions.push_back(viad);
    viad.binding = 0;
    viad.location = 2;
    viad.format = VK_FORMAT_R32G32_SFLOAT;
    viad.offset = offsetof(Vertex, texCoord);
    attributeDescriptions.push_back(viad);
    return attributeDescriptions;
}

void DeviceObjectData::doVertexBuffer(VulkanContext* ctx, std::vector<Vertex> hostVertices) {
    VulkanBuffer stagingBuffer;
    VkDeviceSize bufferSize = sizeof(Vertex) * hostVertices.size();
    VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    createBuffer(ctx, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, properties, &stagingBuffer);
    void* data = stagingBuffer.chonklet.mapMemory(ctx, 0);
    {
        memcpy(data, hostVertices.data(), (size_t)bufferSize);
    }
    stagingBuffer.chonklet.unmapMemory(ctx);
    VkBufferUsageFlags usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    createBuffer(ctx, bufferSize, usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &this->vertexBuffer);
    copyBuffer(ctx, &stagingBuffer, &vertexBuffer, bufferSize);
    stagingBuffer.destroy(ctx);
}

void DeviceObjectData::doIndexBuffer(VulkanContext* ctx, std::vector<uint32_t> hostIndices) {
    VulkanBuffer stagingBuffer;
    VkDeviceSize bufferSize = sizeof(uint32_t) * hostIndices.size();
    VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    createBuffer(ctx, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, properties, &stagingBuffer);
    void* data = stagingBuffer.chonklet.mapMemory(ctx, 0);
    {
        memcpy(data, hostIndices.data(), (size_t)bufferSize);
    }
    stagingBuffer.chonklet.unmapMemory(ctx);
    VkBufferUsageFlags usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    createBuffer(ctx, bufferSize, usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &this->vertexBuffer);
    copyBuffer(ctx, &stagingBuffer, &vertexBuffer, bufferSize);
    stagingBuffer.destroy(ctx);
}

DeviceObjectData::DeviceObjectData(VulkanContext* ctx, HostObjectData hostData) {
    this->doVertexBuffer(ctx, hostData.vertices);
    this->doIndexBuffer(ctx, hostData.indices);
}