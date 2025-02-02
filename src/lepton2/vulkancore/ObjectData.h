#pragma once
#include "VulkanContext.h"
#include "VulkanObjects.h"
#include "VulkanUtils.h"

namespace lepton2::vulkancore {
struct VertexStructDescriptor {
    std::vector<std::pair<uint32_t, VkFormat>> members;
    size_t size;

    bool equal(const VertexStructDescriptor& other);
    VkVertexInputBindingDescription getBindingDescription();
    std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
};

struct HostObjectData : public DeletableVulkanResource {
    // Generates a copy of vertex data, _vsize is total buffer size
    HostObjectData(void* _vertices, size_t _vsize, std::vector<uint32_t> _indices) {
        this->vertices = malloc(_vsize);
        memcpy(this->vertices, _vertices, _vsize);
        this->vsize = _vsize;
        this->indices = _indices;
    }
    void destroy_back(VulkanContext* ctx) override { free(this->vertices); }
    void* vertices;
    size_t vsize;
    std::vector<uint32_t> indices;
};

class DeviceObjectData : public DeletableVulkanResource {
   public:
    DeviceObjectData(VulkanContext* ctx, HostObjectData* hostData);
    void updateDeviceData(VulkanContext* ctx, HostObjectData* hostData);
    void copyVertexBuffer(VulkanContext* ctx, void* vertices, size_t vsize);
    void copyIndexBuffer(VulkanContext* ctx, const std::vector<uint32_t>& hostIndices);
    DeviceObjectData(VulkanContext* ctx, uint32_t numVertices, uint32_t numIndices);
    uint32_t getNumIndices() { return this->numIndices; }
    void bind(VkCommandBuffer commandBuffer, VkDeviceSize offset);
    void destroy_back(VulkanContext* ctx) override;

   private:
    void createBuffers(VulkanContext* ctx, size_t vsize, uint32_t numIndices);
    VulkanBuffer vertexBuffer;
    VulkanBuffer indexBuffer;
    uint32_t numIndices;
    size_t vsize;
};
}  // namespace lepton2::vulkancore