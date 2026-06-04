#pragma once

#include "../utils/LeptonUtils.h"
#include "VulkanUtils.h"

namespace lepton2::vulkancore {
class VulkanContext;

//! Internal representation of a free chonkus subregion from which chonklets may be allocated.
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

//! A large allocation of vulkan memory from which other resources may be allocated.
struct MemoryChonkus : public DeletableVulkanResource {
    VkDeviceMemory memory = VK_NULL_HANDLE;
    uint32_t memory_type;
    VkDeviceSize allocationSize;
    MemoryChonkletEntry* entry;
    lepton2::utils::lepton2_time_point lastUseTime;
    bool is_null() { return (memory == VK_NULL_HANDLE); }
    void destroy_back(VulkanContext* ctx) override;
};

//! A minor chunk of memory belonging to a MemoryChonkus which is used for a single object.
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

//! Simple linked list memory allocator which returns chonklets when requested.
class VulkanAllocationManager : public DeletableVulkanResource {
   public:
    MemoryChonklet findMemory(VulkanContext* ctx, VkDeviceSize size, VkDeviceSize alignment, uint32_t memoryTypeIndex);
    void freeChonklet(VulkanContext* ctx, MemoryChonklet chonklet);
    void purgeUnusedChonki(VulkanContext* ctx, bool overrideTimeout = true);
    void destroy_back(VulkanContext* ctx) override;

    void setCleanupTimeout(double timeout) { this->cleanupTimeout = timeout; }

   private:
    MemoryChonkus* buildChonkus(VulkanContext* ctx, VkDeviceSize size, uint32_t memoryTypeIndex);
    MemoryChonkletEntry* findAvailableEntry(MemoryChonkus* chonkus, VkDeviceSize size, VkDeviceSize alignment);
    void deleteEntry(MemoryChonkus* chonkus, MemoryChonkletEntry* entry);
    void checkChonkusUnused(VulkanContext* ctx, MemoryChonkus* chonkus);
    void doChonkusDeletion(VulkanContext* ctx, MemoryChonkus* chonkus);
    std::unordered_map<uint32_t, std::vector<MemoryChonkus*>> chonki;
    double cleanupTimeout = 0.1; //!< Inactivity cleanup after which to allow chonkus deletion. Negative values immediate.
    std::vector<MemoryChonkus*> unusedChonki;
};
}  // namespace lepton2::vulkancore