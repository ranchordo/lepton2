#pragma once
#include "VulkanContext.h"
#include "VulkanObjects.h"
#include "VulkanUtils.h"

namespace lepton2::vulkancore {
struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 texCoord;

    static VkVertexInputBindingDescription getBindingDescription();
    static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
};

struct HostObjectData {
    HostObjectData(std::vector<Vertex> _vertices, std::vector<uint32_t> _indices) {
        this->vertices = _vertices;
        this->indices = _indices;
    }
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
};

class DeviceObjectData : public DeletableVulkanResource {
   public:
    DeviceObjectData(VulkanContext* ctx, const HostObjectData& hostData);
    void updateDeviceData(VulkanContext* ctx, const HostObjectData& hostData);
    void copyVertexBuffer(VulkanContext* ctx, const std::vector<Vertex>& hostVertices);
    void copyIndexBuffer(VulkanContext* ctx, const std::vector<uint32_t>& hostIndices);
    DeviceObjectData(VulkanContext* ctx, uint32_t numVertices, uint32_t numIndices);
    uint32_t getNumIndices() { return this->numIndices; }
    void bind(VkCommandBuffer commandBuffer, VkDeviceSize offset);
    void destroy_back(VulkanContext* ctx) override;

   private:
    void createBuffers(VulkanContext* ctx, uint32_t numVertices, uint32_t numIndices);
    VulkanBuffer vertexBuffer;
    VulkanBuffer indexBuffer;
    uint32_t numIndices;
    uint32_t numVertices;
};
}  // namespace lepton2::vulkancore