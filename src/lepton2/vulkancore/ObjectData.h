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
    DeviceObjectData(VulkanContext* ctx, HostObjectData hostData);
    void bind(VkCommandBuffer commandBuffer, VkDeviceSize offset);
    void destroy_back(VulkanContext* ctx) override;

   private:
    void doVertexBuffer(VulkanContext* ctx, std::vector<Vertex> hostVertices);
    void doIndexBuffer(VulkanContext* ctx, std::vector<uint32_t> hostIndices);
    VulkanBuffer vertexBuffer;
    VulkanBuffer indexBuffer;
};
}  // namespace lepton2::vulkancore