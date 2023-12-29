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

    struct MemoryChonkus: public DeletableVulkanResource {
        VkDeviceMemory memory = VK_NULL_HANDLE;
        uint32_t memory_type;
        VkDeviceSize allocationSize;
        MemoryChonkletEntry* entry;
        bool is_null() { return (memory == VK_NULL_HANDLE); }
        void destroy_back(VulkanContext* ctx) override;
    };

    struct MemoryChonklet: public DeletableVulkanResource {
        MemoryChonkus* chonkus = nullptr;
        VkDeviceSize offset;
        VkDeviceSize size;
        bool is_null() { return (chonkus == nullptr) || chonkus->is_null(); }
        void destroy_back(VulkanContext* ctx) override;
    };

    class VulkanAllocationManager: public DeletableVulkanResource {
    public:
        VulkanAllocationManager(VulkanContext* ctx) { this->ctx = ctx; }
        MemoryChonklet findMemory(VkDeviceSize size, uint32_t memoryTypeIndex);
        void freeChonklet(MemoryChonklet chonklet);
        void destroy_back(VulkanContext* ctx) override;
    private:
        MemoryChonkus buildChonkus(VkDeviceSize size, uint32_t memoryTypeIndex);
        MemoryChonkletEntry* findAvailableEntry(MemoryChonkus* chonkus, VkDeviceSize size);
        void deleteEntry(MemoryChonkus* chonkus, MemoryChonkletEntry* entry);
        void checkChonkusDeletion(MemoryChonkus* chonkus);
        std::map<uint32_t, std::vector<MemoryChonkus>> chonki;
        VulkanContext* ctx;
    };
}