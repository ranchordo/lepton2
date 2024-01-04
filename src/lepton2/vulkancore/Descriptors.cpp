#include "Descriptors.h"
#include "VulkanContext.h"

using namespace lepton2::vulkancore;

const std::vector<DescriptorPoolSizeTracker> defaultSizes = {
    { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 256, 0 },
    { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 256, 0 }
};

const uint32_t defaultTotalSets = 256;

DescriptorPool::DescriptorPool(VulkanContext* ctx, std::vector<DescriptorPoolSizeTracker> sizes, uint32_t totalSets) {
    std::vector<VkDescriptorPoolSize> sizeInfos;
    for (DescriptorPoolSizeTracker tracker : sizes) {
        VkDescriptorPoolSize sizeInfo{};
        sizeInfo.type = tracker.type;
        sizeInfo.descriptorCount = tracker.totalSize;
        sizeInfos.push_back(sizeInfo);
        tracker.used = 0;
        this->sizeMap[(uint32_t)(tracker.type)] = tracker;
    }
    this->totalSets = totalSets;
    this->usedSets = 0;
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = sizeInfos.size();
    poolInfo.pPoolSizes = sizeInfos.data();
    poolInfo.maxSets = totalSets;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

    if (vkCreateDescriptorPool(ctx->device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor pool.");
    }
}

bool DescriptorPool::canFit(DescriptorSetArray* dsa, uint32_t quantity) {
    if (this->usedSets + quantity > this->totalSets) {
        return false; // Not enough space
    }
    for (auto const& pair : dsa->layoutInfo.typeCounts) {
        if (this->sizeMap.count(pair.first) == 0) {
            throw std::runtime_error("This descriptor pool doesn't contain a required type.");
        }
        DescriptorPoolSizeTracker& tracker = this->sizeMap[pair.first];
        if (tracker.used + (quantity * pair.second) > tracker.totalSize) {
            return false; // Not enough space
        }
    }
    return true;
}

bool DescriptorPool::allocateDescriptorSets(VulkanContext* ctx, DescriptorSetArray* dsa, uint32_t quantity) {
    if (!canFit(dsa, quantity)) {
        return false;
    }
    std::vector<VkDescriptorSetLayout> layouts(quantity, dsa->layoutInfo.descriptorSetLayout);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = quantity;
    allocInfo.pSetLayouts = layouts.data();
    dsa->descriptorSets.resize(quantity);
    if (vkAllocateDescriptorSets(ctx->device, &allocInfo, dsa->descriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate descriptor sets.");
    }
    // Keep tallies
    this->usedSets += quantity;
    for (auto const& pair : dsa->layoutInfo.typeCounts) {
        this->sizeMap[pair.first].used += (quantity * pair.second);
    }
    dsa->parent = this;
    return true;
}

void DescriptorPool::destroy_back(VulkanContext* ctx) {
    if (this->descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(ctx->device, descriptorPool, nullptr);
    }
}

void DescriptorSetArray::addNewBinding(VkDescriptorType type, VkShaderStageFlags stageFlags, uint32_t num) {
    if (this->layoutInfo.typeCounts.count((uint32_t)type) == 0) {
        this->layoutInfo.typeCounts[(uint32_t)type] = 0;
    }
    this->layoutInfo.typeCounts[(uint32_t)type] += num;
    for (uint32_t i = 0; i < num; i++) {
        VkDescriptorSetLayoutBinding binding{};
        binding.binding = this->layoutInfo.bindings.size();
        binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        binding.descriptorCount = 1;
        binding.stageFlags = stageFlags;
        binding.pImmutableSamplers = nullptr;
        this->layoutInfo.bindings.push_back(binding);
    }
}

void DescriptorSetArray::buildDescriptorSetLayout(VulkanContext* ctx) {
    VkDescriptorSetLayoutCreateInfo layoutCreateInfo{};
    layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutCreateInfo.bindingCount = this->layoutInfo.bindings.size();
    layoutCreateInfo.pBindings = this->layoutInfo.bindings.data();

    if (vkCreateDescriptorSetLayout(ctx->device, &layoutCreateInfo, nullptr, &layoutInfo.descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor set layout.");
    }

    this->layoutInfo.bindings.clear();
    // We don't clear layoutInfo.typeCounts because we need it for pool logic
}

void DescriptorSetArray::destroy_back(VulkanContext* ctx) {
    if (this->layoutInfo.descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(ctx->device, this->layoutInfo.descriptorSetLayout, nullptr);
    }
    if (this->descriptorSets.size() > 0) {
        vkFreeDescriptorSets(ctx->device, parent->descriptorPool, descriptorSets.size(), descriptorSets.data());
        // Keep tallies
        parent->usedSets -= this->descriptorSets.size();
        for (auto const& pair : this->layoutInfo.typeCounts) {
            parent->sizeMap[pair.first].used -= pair.second;
        }
    }
    this->descriptorSets.clear();
}

void DescriptorPoolManager::allocateDescriptorSets(VulkanContext* ctx, DescriptorSetArray* dsa, uint32_t quantity) {
    for (DescriptorPool* descriptorPool : this->pools) {
        if (descriptorPool->allocateDescriptorSets(ctx, dsa, quantity)) {
            return;
        }
    }
    DescriptorPool* newPool = new DescriptorPool(ctx, defaultSizes, defaultTotalSets);
    if (!newPool->allocateDescriptorSets(ctx, dsa, quantity)) {
        throw std::runtime_error("Cannot fit this DescriptorSetArray into a brand-new DescriptorPool. Is something too big?");
    }
    this->pools.push_back(newPool);
}

void DescriptorPoolManager::destroy_back(VulkanContext* ctx) {
    for (DescriptorPool* descriptorPool : this->pools) {
        descriptorPool->destroy(ctx);
        delete descriptorPool;
    }
    this->pools.clear();
}