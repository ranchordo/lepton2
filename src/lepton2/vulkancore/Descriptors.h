#pragma once

#include "VulkanUtils.h"

namespace lepton2::vulkancore {
    class VulkanContext;
    struct DescriptorPoolSizeTracker {
        VkDescriptorType type;
        uint32_t totalSize;
        uint32_t used;
    };

    class DescriptorSetArray;
    class DescriptorPoolManager;
    class DescriptorPool: public DeletableVulkanResource {
    public:
        DescriptorPool(VulkanContext* ctx, std::vector<DescriptorPoolSizeTracker> sizes, uint32_t totalSets);
        bool canFit(DescriptorSetArray* dsa, uint32_t quantity);
        bool allocateDescriptorSets(VulkanContext* ctx, DescriptorSetArray* dsa, uint32_t quantity);
        void destroy_back(VulkanContext* ctx) override;
        std::map<uint32_t, DescriptorPoolSizeTracker> sizeMap;
        VkDescriptorPool descriptorPool;
        uint32_t totalSets;
        uint32_t usedSets;
    };

    class DescriptorSetArray: public DeletableVulkanResource {
    public:
        struct {
            std::map<uint32_t, uint32_t> typeCounts;
            std::vector<VkDescriptorSetLayoutBinding> bindings;
            VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
        } layoutInfo;
        DescriptorPool* parent = nullptr;
        void addNewBinding(VkDescriptorType type, VkShaderStageFlags stageFlags, uint32_t num = 1);
        void buildDescriptorSetLayout(VulkanContext* ctx);
        void destroy_back(VulkanContext* ctx) override;
        std::vector<VkDescriptorSetLayout> getSingularLayout() {
            return { this->layoutInfo.descriptorSetLayout };
        }
        std::vector<VkDescriptorSet> descriptorSets;
    };

    class DescriptorPoolManager: public DeletableVulkanResource {
    public:
        void allocateDescriptorSets(VulkanContext* ctx, DescriptorSetArray* dsa, uint32_t quantity);
        void destroy_back(VulkanContext* ctx) override;
    private:
        std::vector<DescriptorPool*> pools;
    };
}