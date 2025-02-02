#include "ObjectData.h"

using namespace lepton2::vulkancore;

bool VertexStructDescriptor::equal(const VertexStructDescriptor& other) {
    if (this->size != other.size) return false;
    for (uint32_t i = 0; i < members.size(); i++) {
        if (members[i].first != other.members[i].first) return false;
        if (members[i].second != other.members[i].second) return false;
    }
    return true;
}

VkVertexInputBindingDescription VertexStructDescriptor::getBindingDescription() {
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = (uint32_t)this->size;
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return bindingDescription;
}

std::vector<VkVertexInputAttributeDescription> VertexStructDescriptor::getAttributeDescriptions() {
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
    VkVertexInputAttributeDescription viad{};
    for (uint32_t i = 0; i < this->members.size(); i++) {
        viad.binding = 0;
        viad.location = i;
        viad.format = this->members[i].second;
        viad.offset = this->members[i].first;
        attributeDescriptions.push_back(viad);
    }
    return attributeDescriptions;
}

void DeviceObjectData::copyVertexBuffer(VulkanContext* ctx, void* vertices, size_t vsize) {
    if (vsize != this->vsize) {
        throw std::runtime_error("Must keep consistent vertex size when updating device data.");
    }
    VulkanBuffer stagingBuffer;
    VkDeviceSize bufferSize = vsize;
    VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    createBuffer(ctx, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, properties, &stagingBuffer);
    void* data = stagingBuffer.chonklet.mapMemory(ctx, 0);
    {
        memcpy(data, vertices, (size_t)bufferSize);
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

void DeviceObjectData::createBuffers(VulkanContext* ctx, size_t vsize, uint32_t numIndices) {
    VkDeviceSize vertexBufferSize = vsize;
    VkDeviceSize indexBufferSize = sizeof(uint32_t) * numIndices;
    VkBufferUsageFlags vertexBufferUsage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    createBuffer(ctx, vertexBufferSize, vertexBufferUsage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &this->vertexBuffer);
    VkBufferUsageFlags indexBufferUsage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    createBuffer(ctx, indexBufferSize, indexBufferUsage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &this->indexBuffer);
    this->numIndices = numIndices;
    this->vsize = vsize;
}

void DeviceObjectData::updateDeviceData(VulkanContext* ctx, HostObjectData* hostData) {
    this->copyVertexBuffer(ctx, hostData->vertices, hostData->vsize);
    this->copyIndexBuffer(ctx, hostData->indices);
}

DeviceObjectData::DeviceObjectData(VulkanContext* ctx, HostObjectData* hostData) {
    this->createBuffers(ctx, hostData->vsize, hostData->indices.size());
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