#pragma once

#include "VulkanObjects.h"
#include "VulkanUtils.h"

namespace lepton2::vulkancore {
//! Formats and sizes of whatever structure corresponds to a vertex.
struct VertexStructDescriptor {
    std::vector<std::pair<uint32_t, VkFormat>> members;
    size_t size;

    bool equal(const VertexStructDescriptor& other);
    VkVertexInputBindingDescription getBindingDescription();
    std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
};

struct HostObjectData : public DeletableVulkanResource {
    // Will delete internal vertex buffer ptr on destruction.
    HostObjectData(void* _vertices, size_t _vsize, std::vector<uint32_t> _indices, bool gen_copy = true) {
        if (gen_copy) {
            this->vertices = malloc(_vsize);
            memcpy(this->vertices, _vertices, _vsize);
        } else {
            this->vertices = _vertices;
        }
        this->vsize = _vsize;
        this->indices = _indices;
    }
    void destroy_back(VulkanContext* ctx) override { free(this->vertices); }
    void* vertices;
    size_t vsize;
    std::vector<uint32_t> indices;
};

//! Managed vertex and index buffers for a graphical object, stored device-side.
class DeviceObjectData : public DeletableVulkanResource {
   public:
    DeviceObjectData(VulkanContext* ctx, HostObjectData* hostData); //!< Generates and populates device-side data.
    void updateDeviceData(VulkanContext* ctx, HostObjectData* hostData); //!< Refreshes device-side data, does not reallocate.
    void copyVertexBuffer(VulkanContext* ctx, void* vertices, size_t vsize); //!< Transfer vertex data to device.
    void copyIndexBuffer(VulkanContext* ctx, const std::vector<uint32_t>& hostIndices); //!< Transfer index data to device.
    DeviceObjectData(VulkanContext* ctx, uint32_t numVertices, uint32_t numIndices); //!< Generates device-side vertex and index buffers, but does not populate them.
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