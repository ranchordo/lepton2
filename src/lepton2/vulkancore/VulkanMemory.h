#pragma once

#include "VulkanUtils.h"

namespace lepton2::vulkancore {
class VulkanContext;

struct MemoryChonkletEntry {
    VkDeviceSize offset;
    VkDeviceSize size;
    MemoryChonkletEntry* next;
    MemoryChonkletEntry* prev;
    MemoryChonkletEntry(VkDeviceSize offset, VkDeviceSize size, MemoryChonkletEntry* next, MemoryChonkletEntry* prev) {
        this->offset = offset;
        this->size = size;
        this->next = next;
        this->prev = prev;
    }
};

struct MemoryChonkus : public DeletableVulkanResource {
    VkDeviceMemory memory = VK_NULL_HANDLE;
    uint32_t memory_type;
    VkDeviceSize allocationSize;
    MemoryChonkletEntry* entry;
    bool is_null() { return (memory == VK_NULL_HANDLE); }
    void destroy_back(VulkanContext* ctx) override;
};

struct MemoryChonklet : public DeletableVulkanResource {
    MemoryChonkus* chonkus = nullptr;
    VkDeviceSize offset;
    VkDeviceSize size;
    VkDeviceSize alignmentPadding;
    void* mapMemory(VulkanContext* ctx, VkMemoryMapFlags flags);
    void unmapMemory(VulkanContext* ctx);
    bool is_null() { return (chonkus == nullptr) || chonkus->is_null(); }
    void destroy_back(VulkanContext* ctx) override;
};

class VulkanAllocationManager : public DeletableVulkanResource {
   public:
    MemoryChonklet findMemory(VulkanContext* ctx, VkDeviceSize size, VkDeviceSize alignment, uint32_t memoryTypeIndex);
    void freeChonklet(VulkanContext* ctx, MemoryChonklet chonklet);
    void destroy_back(VulkanContext* ctx) override;

   private:
    MemoryChonkus* buildChonkus(VulkanContext* ctx, VkDeviceSize size, uint32_t memoryTypeIndex);
    MemoryChonkletEntry* findAvailableEntry(MemoryChonkus* chonkus, VkDeviceSize size, VkDeviceSize alignment);
    void deleteEntry(MemoryChonkus* chonkus, MemoryChonkletEntry* entry);
    void checkChonkusDeletion(VulkanContext* ctx, MemoryChonkus* chonkus);
    std::unordered_map<uint32_t, std::vector<MemoryChonkus*>> chonki;
};
}  // namespace lepton2::vulkancore