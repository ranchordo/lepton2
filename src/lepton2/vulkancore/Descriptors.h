#pragma once

#include "VulkanUtils.h"

namespace lepton2::vulkancore {

class VulkanContext;
class ColorAttachmentInfo;
struct DescriptorSetUpdateInfo;
struct DescriptorInfo {
    VkDescriptorType descriptorType;
    VkDeviceSize bufferSize;
    ColorAttachmentInfo* colorAttachmentInfo;
};

struct DescriptorWriteInfoContainer {
    VkDescriptorBufferInfo bufferInfo;
    VkDescriptorImageInfo imageInfo;
    VkWriteDescriptorSet writeInfo;
};

namespace descriptortypes {

class DescriptorType : public DeletableVulkanResource {
   public:
    virtual VkDescriptorType getDescriptorType() = 0;
    virtual DescriptorWriteInfoContainer getWriteInfo(VulkanContext* ctx, VkDescriptorSet dstSet, uint32_t dstBinding) = 0;
    static DescriptorType* constructDescriptorInstance(VulkanContext* ctx, DescriptorInfo info, uint32_t index);
    virtual ~DescriptorType() {}
};

class UniformBufferDescriptor : public DescriptorType {
   public:
    UniformBufferDescriptor(VulkanContext* ctx, DescriptorInfo info, uint32_t index);
    VkDescriptorType getDescriptorType() override { return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; }
    DescriptorWriteInfoContainer getWriteInfo(VulkanContext* ctx, VkDescriptorSet dstSet, uint32_t dstBinding) override;
    void destroy_back(VulkanContext* ctx) override;

    VulkanBuffer* uniformBuffer;
    VkDeviceSize uniformBufferSize;
};

class InputAttachmentDescriptor : public DescriptorType {
   public:
    InputAttachmentDescriptor(VulkanContext* ctx, DescriptorInfo info, uint32_t index);
    VkDescriptorType getDescriptorType() override { return VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT; }
    DescriptorWriteInfoContainer getWriteInfo(VulkanContext* ctx, VkDescriptorSet dstSet, uint32_t dstBinding) override;
    void destroy_back(VulkanContext* ctx) override;

   private:
    ColorAttachmentInfo* colorAttachmentInfo;
    uint32_t arrayIndex;
    DescriptorSetUpdateInfo* updateInfo = nullptr;
};
}  // namespace descriptortypes

struct DescriptorPoolSizeTracker {
    VkDescriptorType type;
    uint32_t totalSize;
    uint32_t used;
};

class DescriptorSetArray;
class DescriptorPoolManager;
class DescriptorPool : public DeletableVulkanResource {
   public:
    DescriptorPool(VulkanContext* ctx, std::vector<DescriptorPoolSizeTracker> sizes, uint32_t totalSets);
    bool canFit(DescriptorSetArray* dsa, uint32_t quantity);
    bool allocateDescriptorSets(VulkanContext* ctx, DescriptorSetArray* dsa, uint32_t quantity);
    void destroy_back(VulkanContext* ctx) override;
    std::unordered_map<uint32_t, DescriptorPoolSizeTracker> sizeMap;
    VkDescriptorPool descriptorPool;
    uint32_t totalSets;
    uint32_t usedSets;
};

struct DescriptorSetUpdateInfo {
    VkDescriptorSet dstSet;
    uint32_t dstBinding;
    descriptortypes::DescriptorType* instance;
    bool alive = false;
};

struct SingleDescriptorSet {
    VkDescriptorSet descriptorSet;
    std::vector<descriptortypes::DescriptorType*> instances;
};

class DescriptorSetArray : public DeletableVulkanResource {
   public:
    struct {
        std::unordered_map<uint32_t, uint32_t> typeCounts;
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        std::vector<DescriptorInfo> descInfo;
        VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    } layoutInfo;
    DescriptorPool* parent = nullptr;
    void addNewBinding(DescriptorInfo descriptorInfo, VkShaderStageFlags stageFlags, uint32_t num = 1);
    bool isLayoutCompatible(DescriptorSetArray* other);
    void buildDescriptorSetLayout(VulkanContext* ctx);
    void updateAllDescriptorSets(VulkanContext* ctx);
    void destroy_back(VulkanContext* ctx) override;
    std::vector<VkDescriptorSetLayout> getSingularLayout() {
        return {this->layoutInfo.descriptorSetLayout};
    }
    std::vector<SingleDescriptorSet> singleDescriptorSets;
};

class DescriptorPoolManager : public DeletableVulkanResource {
   public:
    void allocateDescriptorSets(VulkanContext* ctx, DescriptorSetArray* dsa, uint32_t quantity);
    void destroy_back(VulkanContext* ctx) override;

   private:
    std::vector<DescriptorPool*> pools;
};
}  // namespace lepton2::vulkancore