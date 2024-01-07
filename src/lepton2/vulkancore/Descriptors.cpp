#include "Descriptors.h"

#include "RenderState.h"
#include "VulkanContext.h"

using namespace lepton2::vulkancore;
using namespace lepton2::vulkancore::descriptortypes;

DescriptorType* DescriptorType::constructDescriptorInstance(VulkanContext* ctx, DescriptorInfo info, uint32_t index) {
    switch (info.descriptorType) {
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            return new UniformBufferDescriptor(ctx, info, index);
        case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
            return new InputAttachmentDescriptor(ctx, info, index);
        default:
            throw std::runtime_error("Unsupported descriptor type.");
    }
    return nullptr;
}

UniformBufferDescriptor::UniformBufferDescriptor(VulkanContext* ctx, DescriptorInfo info, uint32_t index) {
    VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    this->uniformBuffer = new VulkanBuffer();
    createBuffer(ctx, info.bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, properties, this->uniformBuffer);
    this->uniformBufferSize = info.bufferSize;
}

DescriptorWriteInfoContainer UniformBufferDescriptor::getWriteInfo(VulkanContext* ctx, VkDescriptorSet dstSet, uint32_t dstBinding) {
    DescriptorWriteInfoContainer ret;
    ret.bufferInfo.buffer = uniformBuffer->buffer;
    ret.bufferInfo.offset = 0;
    ret.bufferInfo.range = this->uniformBufferSize;

    ret.writeInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    ret.writeInfo.dstSet = dstSet;
    ret.writeInfo.dstBinding = dstBinding;
    ret.writeInfo.dstArrayElement = 0;
    ret.writeInfo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ret.writeInfo.descriptorCount = 1;
    ret.writeInfo.pNext = nullptr;

    return ret;
}

void UniformBufferDescriptor::destroy_back(VulkanContext* ctx) {
    this->uniformBuffer->destroy(ctx);
    delete this->uniformBuffer;
}

InputAttachmentDescriptor::InputAttachmentDescriptor(VulkanContext* ctx, DescriptorInfo info, uint32_t index) {
    this->colorAttachmentInfo = info.colorAttachmentInfo;
    this->arrayIndex = index;
}

DescriptorWriteInfoContainer InputAttachmentDescriptor::getWriteInfo(VulkanContext* ctx, VkDescriptorSet dstSet, uint32_t dstBinding) {
    if (this->updateInfo == nullptr) {
        this->updateInfo = new DescriptorSetUpdateInfo();
        this->updateInfo->alive = true;
        this->updateInfo->instance = this;
        this->updateInfo->dstSet = dstSet;
        this->updateInfo->dstBinding = dstBinding;
        ctx->swapChain.descriptorUpdates.insert(this->updateInfo);
    }
    DescriptorWriteInfoContainer ret;
    ret.imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    ret.imageInfo.imageView = this->colorAttachmentInfo->swapChainCreations[this->arrayIndex]->imageView;

    ret.writeInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    ret.writeInfo.dstSet = dstSet;
    ret.writeInfo.dstBinding = dstBinding;
    ret.writeInfo.dstArrayElement = 0;
    ret.writeInfo.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    ret.writeInfo.descriptorCount = 1;
    ret.writeInfo.pNext = nullptr;

    return ret;
}

void InputAttachmentDescriptor::destroy_back(VulkanContext* ctx) {
    this->updateInfo->alive = false;
    // UpdateInfo will be cleaned by the swapChain on a recreation or deletion.
}

const std::vector<DescriptorPoolSizeTracker> defaultSizes = {
    {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 256, 0},
    {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 256, 0},
    {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 256, 0}};

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
        return false;  // Not enough space
    }
    for (auto const& pair : dsa->layoutInfo.typeCounts) {
        if (this->sizeMap.count(pair.first) == 0) {
            throw std::runtime_error("This descriptor pool doesn't contain a required type.");
        }
        DescriptorPoolSizeTracker& tracker = this->sizeMap[pair.first];
        if (tracker.used + (quantity * pair.second) > tracker.totalSize) {
            return false;  // Not enough space
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
    dsa->singleDescriptorSets.resize(quantity);
    std::vector<VkDescriptorSet> descriptorSets;
    descriptorSets.resize(quantity);
    if (vkAllocateDescriptorSets(ctx->device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate descriptor sets.");
    }
    for (uint32_t i = 0; i < quantity; i++) {
        dsa->singleDescriptorSets[i].descriptorSet = descriptorSets[i];
        dsa->singleDescriptorSets[i].instances.resize(dsa->layoutInfo.descInfo.size());
        for (uint32_t j = 0; j < dsa->layoutInfo.descInfo.size(); j++) {
            DescriptorType* instance;
            instance = DescriptorType::constructDescriptorInstance(ctx, dsa->layoutInfo.descInfo[j], i);
            dsa->singleDescriptorSets[i].instances[j] = instance;
        }
    }
    descriptorSets.clear();
    dsa->updateAllDescriptorSets(ctx);
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

void DescriptorSetArray::addNewBinding(DescriptorInfo descriptorInfo, VkShaderStageFlags stageFlags, uint32_t num) {
    uint32_t uint_type = (uint32_t)descriptorInfo.descriptorType;
    if (this->layoutInfo.typeCounts.count(uint_type) == 0) {
        this->layoutInfo.typeCounts[uint_type] = 0;
    }
    this->layoutInfo.typeCounts[uint_type] += num;
    for (uint32_t i = 0; i < num; i++) {
        VkDescriptorSetLayoutBinding binding{};
        binding.binding = this->layoutInfo.bindings.size();
        binding.descriptorType = descriptorInfo.descriptorType;
        binding.descriptorCount = 1;
        binding.stageFlags = stageFlags;
        binding.pImmutableSamplers = nullptr;
        this->layoutInfo.bindings.push_back(binding);
        this->layoutInfo.descInfo.push_back(descriptorInfo);
    }
}

bool DescriptorSetArray::isLayoutCompatible(DescriptorSetArray* other) {
    if (this->layoutInfo.bindings.size() != other->layoutInfo.bindings.size()) {
        return false;
    }
    for (uint32_t i = 0; i < this->layoutInfo.bindings.size(); i++) {
        VkDescriptorSetLayoutBinding a = this->layoutInfo.bindings[i];
        VkDescriptorSetLayoutBinding b = other->layoutInfo.bindings[i];
        if (a.binding != b.binding) {
            return false;
        }
        if (a.descriptorType != b.descriptorType) {
            return false;
        }
        if (a.stageFlags != b.stageFlags) {
            return false;
        }
    }
    return true;
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

void DescriptorSetArray::updateAllDescriptorSets(VulkanContext* ctx) {
    std::vector<VkDescriptorImageInfo> imageInfos;
    std::vector<VkDescriptorBufferInfo> bufferInfos;
    std::vector<VkWriteDescriptorSet> writeInfos;
    for (SingleDescriptorSet& sds : this->singleDescriptorSets) {
        for (uint32_t i = 0; i < sds.instances.size(); i++) {
            DescriptorWriteInfoContainer dwic = sds.instances[i]->getWriteInfo(ctx, sds.descriptorSet, i);
            imageInfos.push_back(dwic.imageInfo);
            bufferInfos.push_back(dwic.bufferInfo);
            writeInfos.push_back(dwic.writeInfo);
        }
    }
    for (uint32_t i = 0; i < writeInfos.size(); i++) {
        writeInfos[i].pImageInfo = &imageInfos[i]; // Must keep alive
        writeInfos[i].pBufferInfo = &bufferInfos[i]; // Must keep alive
    }
    vkUpdateDescriptorSets(ctx->device, writeInfos.size(), writeInfos.data(), 0, nullptr);
}

void DescriptorSetArray::destroy_back(VulkanContext* ctx) {
    if (this->layoutInfo.descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(ctx->device, this->layoutInfo.descriptorSetLayout, nullptr);
    }
    if (this->singleDescriptorSets.size() > 0) {
        std::vector<VkDescriptorSet> descriptorSets;
        for (SingleDescriptorSet& sds : this->singleDescriptorSets) {
            descriptorSets.push_back(sds.descriptorSet);
            for (DescriptorType* instance : sds.instances) {
                instance->destroy(ctx);
                delete instance;
            }
            sds.instances.clear();
        }
        this->singleDescriptorSets.clear();
        vkFreeDescriptorSets(ctx->device, parent->descriptorPool, descriptorSets.size(), descriptorSets.data());
        // Keep tallies
        parent->usedSets -= this->singleDescriptorSets.size();
        for (auto const& pair : this->layoutInfo.typeCounts) {
            parent->sizeMap[pair.first].used -= (this->singleDescriptorSets.size() * pair.second);
        }
    }
    this->singleDescriptorSets.clear();
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