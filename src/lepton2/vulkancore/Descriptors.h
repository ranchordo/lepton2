#pragma once

#include "VulkanUtils.h"

namespace lepton2::vulkancore {

class VulkanContext;
class ColorAttachmentInfo;
struct DescriptorSetUpdateInfo;
class Texture;
class TextureComponent;
struct DescriptorInfo {
    VkDescriptorType descriptorType;
    struct {
        VkDeviceSize bufferSize = 0;
        bool createNewBuffer = true;
        VulkanBuffer* bufferReference = nullptr;
    } uniformBufferData;
    struct {
        ColorAttachmentInfo* colorAttachmentInfo;
    } inputAttachmentData;
    struct {
        Texture* container;
        uint32_t componentIndex;
    } imageSamplerData;
    VkShaderStageFlags shaderStages = VK_SHADER_STAGE_ALL_GRAPHICS;
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

class ImageSamplerDescriptor : public DescriptorType {
   public:
    ImageSamplerDescriptor(VulkanContext* ctx, DescriptorInfo info, uint32_t index);
    VkDescriptorType getDescriptorType() override { return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; }
    DescriptorWriteInfoContainer getWriteInfo(VulkanContext* ctx, VkDescriptorSet dstSet, uint32_t dstBinding) override;
    void destroy_back(VulkanContext* ctx) override;

   private:
    Texture* container;
    uint32_t componentIndex;
    uint32_t imageIndex;
    DescriptorSetUpdateInfo* updateInfo = nullptr;
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

struct DescriptorSetLayoutInfo {
    std::unordered_map<uint32_t, uint32_t> typeCounts;
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    std::vector<DescriptorInfo> descInfo;
    void addNewBinding(DescriptorInfo descriptorInfo, uint32_t num = 1);
};

class DescriptorSetArray : public DeletableVulkanResource {
   public:
    DescriptorSetArray(DescriptorSetLayoutInfo layoutInfo);
    DescriptorSetArray(DescriptorSetArray* layoutReference);
    DescriptorSetLayoutInfo layoutInfo;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    DescriptorPool* parent = nullptr;
    static bool isLayoutCompatible(std::vector<VkDescriptorSetLayoutBinding> a, std::vector<VkDescriptorSetLayoutBinding> b);
    void buildDescriptorSetLayout(VulkanContext* ctx);
    void updateAllDescriptorSets(VulkanContext* ctx);
    void destroy_back(VulkanContext* ctx) override;
    std::vector<SingleDescriptorSet> singleDescriptorSets;

   private:
    bool externalLayout = false;
};

class DescriptorPoolManager : public DeletableVulkanResource {
   public:
    void allocateDescriptorSets(VulkanContext* ctx, DescriptorSetArray* dsa, uint32_t quantity);
    void destroy_back(VulkanContext* ctx) override;

   private:
    std::vector<DescriptorPool*> pools;
};
}  // namespace lepton2::vulkancore