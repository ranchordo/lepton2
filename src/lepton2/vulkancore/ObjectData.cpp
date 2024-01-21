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

void DeviceObjectData::copyVertexBuffer(VulkanContext* ctx, const std::vector<Vertex>& hostVertices) {
    if (hostVertices.size() != this->numVertices) {
        throw std::runtime_error("Must keep consistent vertex size when updating device data.");
    }
    VulkanBuffer stagingBuffer;
    VkDeviceSize bufferSize = sizeof(Vertex) * hostVertices.size();
    VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    createBuffer(ctx, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, properties, &stagingBuffer);
    void* data = stagingBuffer.chonklet.mapMemory(ctx, 0);
    {
        memcpy(data, hostVertices.data(), (size_t)bufferSize);
    }
    stagingBuffer.chonklet.unmapMemory(ctx);
    copyBuffer(ctx, &stagingBuffer, &vertexBuffer, bufferSize);
    stagingBuffer.destroy(ctx);
}

void DeviceObjectData::copyIndexBuffer(VulkanContext* ctx, const std::vector<uint32_t>& hostIndices) {
    if (hostIndices.size() != this->numIndices) {
        throw std::runtime_error("Must keep consistent index size when updating device data");
    }
    VulkanBuffer stagingBuffer;
    VkDeviceSize bufferSize = sizeof(uint32_t) * hostIndices.size();
    VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    createBuffer(ctx, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, properties, &stagingBuffer);
    void* data = stagingBuffer.chonklet.mapMemory(ctx, 0);
    {
        memcpy(data, hostIndices.data(), (size_t)bufferSize);
    }
    stagingBuffer.chonklet.unmapMemory(ctx);
    copyBuffer(ctx, &stagingBuffer, &indexBuffer, bufferSize);
    stagingBuffer.destroy(ctx);
}

void DeviceObjectData::createBuffers(VulkanContext* ctx, uint32_t numVertices, uint32_t numIndices) {
    VkDeviceSize vertexBufferSize = sizeof(Vertex) * numVertices;
    VkDeviceSize indexBufferSize = sizeof(uint32_t) * numIndices;
    VkBufferUsageFlags vertexBufferUsage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    createBuffer(ctx, vertexBufferSize, vertexBufferUsage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &this->vertexBuffer);
    VkBufferUsageFlags indexBufferUsage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    createBuffer(ctx, indexBufferSize, indexBufferUsage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &this->indexBuffer);
    this->numIndices = numIndices;
    this->numVertices = numVertices;
}

void DeviceObjectData::updateDeviceData(VulkanContext* ctx, const HostObjectData& hostData) {
    this->copyVertexBuffer(ctx, hostData.vertices);
    this->copyIndexBuffer(ctx, hostData.indices);
}

DeviceObjectData::DeviceObjectData(VulkanContext* ctx, const HostObjectData& hostData) {
    this->createBuffers(ctx, hostData.vertices.size(), hostData.indices.size());
    this->updateDeviceData(ctx, hostData);
}

DeviceObjectData::DeviceObjectData(VulkanContext* ctx, uint32_t numVertices, uint32_t numIndices) {
    this->createBuffers(ctx, numVertices, numIndices);
}

void DeviceObjectData::destroy_back(VulkanContext* ctx) {
    this->vertexBuffer.destroy(ctx);
    this->indexBuffer.destroy(ctx);
}

void DeviceObjectData::bind(VkCommandBuffer commandBuffer, VkDeviceSize offset) {
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &this->vertexBuffer.buffer, &offset);
    vkCmdBindIndexBuffer(commandBuffer, this->indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
}