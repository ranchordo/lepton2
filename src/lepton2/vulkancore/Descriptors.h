#pragma once

#include "VulkanUtils.h"

namespace lepton2::vulkancore {

class VulkanContext;
class ColorAttachmentInfo;
class DepthAttachmentInfo;
class Texture;

//! The necessary information to create any type of lepton2-managed descriptor.
/**
 * When submitting a DescriptorInfo to create a new lepton2-managed descriptor, the `descriptorType`
 * and `shaderStages` fields will always be used. During creation, descriptorType is used to select
 * the created descriptor type, and only the relevant substruct from DescriptorInfo will be accessed.
 * \sa DescriptorSetLayoutInfo and descriptortypes::DescriptorType
 */
struct DescriptorInfo {
    VkDescriptorType descriptorType;
    struct {
        VkDeviceSize bufferSize = 0;
        bool createNewBuffer = true;
        VulkanBuffer* bufferReference = nullptr;  //!< If not creating a new buffer, exact buffer to use, otherwise `nullptr`.
    } uniformBufferData;
    struct {
        VkDeviceSize bufferSize = 0;
        bool createNewBuffer = true;
        VulkanBuffer* bufferReference = nullptr;  //!< If not creating a new buffer, exact buffer to use, otherwise `nullptr`.
    } storageBufferData;
    struct {
        ImageArray* images = nullptr;
    } storageImageData;
    struct {
        ColorAttachmentInfo* colorAttachmentInfo = nullptr;  //<! Exactly one of `colorAttachmentInfo`/`depthAttachmentInfo` must be non-null.
        DepthAttachmentInfo* depthAttachmentInfo = nullptr;  //<! Exactly one of `colorAttachmentInfo`/`depthAttachmentInfo` must be non-null.
    } inputAttachmentData;
    struct {
        Texture* container;
        uint32_t componentIndex; //!< Which component of `container` to use
    } imageSamplerData;
    VkShaderStageFlags shaderStages = VK_SHADER_STAGE_ALL_GRAPHICS;
};

//! Holds a `VkWriteDescriptorSet` and associated buffer/image info.
/**
 * Used to keep `bufferInfo`/`imageInfo` alive and associated with `writeInfo` until
 * `vkWriteDescriptorSets` is called.
 * \sa descriptortypes::DescriptorType::getWriteInfo(VulkanContext*, VkDescriptorSet, uint32_t)
 */
struct DescriptorWriteInfoContainer {
    VkDescriptorBufferInfo bufferInfo;
    VkDescriptorImageInfo imageInfo;
    VkWriteDescriptorSet writeInfo;
};

namespace descriptortypes {
class DescriptorType;
}

//! Host-side objects representing allocated descriptor instances.
namespace descriptortypes {

//! Generic descriptor type
/**
 * Every class inheriting from `DescriptorType` represents a different `VkDescriptorType` value.
 * \sa DescriptorPool::allocateDescriptorSets(VulkanContext*, DescriptorSetArray*, uint32_t)
 */
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

class StorageBufferDescriptor : public DescriptorType {
   public:
    StorageBufferDescriptor(VulkanContext* ctx, DescriptorInfo info, uint32_t index);
    VkDescriptorType getDescriptorType() override { return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; }
    DescriptorWriteInfoContainer getWriteInfo(VulkanContext* ctx, VkDescriptorSet dstSet, uint32_t dstBinding) override;
    void destroy_back(VulkanContext* ctx) override;

    VulkanBuffer* storageBuffer;
    VkDeviceSize storageBufferSize;
};

class StorageImageDescriptor : public DescriptorType {
   public:
    StorageImageDescriptor(VulkanContext* ctx, DescriptorInfo info, uint32_t index);
    VkDescriptorType getDescriptorType() override { return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE; }
    DescriptorWriteInfoContainer getWriteInfo(VulkanContext* ctx, VkDescriptorSet dstSet, uint32_t dstBinding) override;
    void destroy_back(VulkanContext* ctx) override {}

    ImageArray* images;
    uint32_t index;
};

class ImageSamplerDescriptor : public DescriptorType {
   public:
    ImageSamplerDescriptor(VulkanContext* ctx, DescriptorInfo info, uint32_t index);
    VkDescriptorType getDescriptorType() override { return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; }
    DescriptorWriteInfoContainer getWriteInfo(VulkanContext* ctx, VkDescriptorSet dstSet, uint32_t dstBinding) override;
    void destroy_back(VulkanContext* ctx) override {}

   private:
    Texture* container;
    uint32_t componentIndex;
    uint32_t imageIndex;
};

class InputAttachmentDescriptor : public DescriptorType {
   public:
    InputAttachmentDescriptor(VulkanContext* ctx, DescriptorInfo info, uint32_t index);
    VkDescriptorType getDescriptorType() override { return VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT; }
    DescriptorWriteInfoContainer getWriteInfo(VulkanContext* ctx, VkDescriptorSet dstSet, uint32_t dstBinding) override;
    void destroy_back(VulkanContext* ctx) override {}

   private:
    ColorAttachmentInfo* colorAttachmentInfo;
    DepthAttachmentInfo* depthAttachmentInfo;
    uint32_t arrayIndex;
};
}  // namespace descriptortypes

//! Tracks descriptor allocations for each `VkDescriptorType`.
struct DescriptorPoolSizeTracker {
    VkDescriptorType type;
    uint32_t totalSize;
    uint32_t used;
};

class DescriptorSetArray;
class DescriptorPoolManager;
//! `VkDescriptorPool` wrapper with additional CPU-side allocation tracking.
class DescriptorPool : public DeletableVulkanResource {
   public:
    DescriptorPool(VulkanContext* ctx, std::vector<DescriptorPoolSizeTracker> sizes, uint32_t totalSets);
    bool canFit(DescriptorSetArray* dsa, uint32_t quantity);
    bool allocateDescriptorSets(VulkanContext* ctx, DescriptorSetArray* dsa, uint32_t quantity); //!< Allocate from descriptor pools and create descriptortype::DescriptorType instances.
    void destroy_back(VulkanContext* ctx) override;
    std::unordered_map<uint32_t, DescriptorPoolSizeTracker> sizeMap;
    VkDescriptorPool descriptorPool;
    uint32_t totalSets;
    uint32_t usedSets;
};

//! Descriptor set (both Vulkan- and lepton2-managed) corresponding to a single resource index.
struct SingleDescriptorSet {
    VkDescriptorSet descriptorSet;
    std::vector<descriptortypes::DescriptorType*> instances;
};

//! Information for creating a DescriptorSetArray.
struct DescriptorSetLayoutInfo {
    std::unordered_map<uint32_t, uint32_t> typeCounts;   //!< How many of each `VkDescriptorType` will be used (used to track pool capacities).
    std::vector<VkDescriptorSetLayoutBinding> bindings;  //!< Vulkan-side descriptor set layout information.
    std::vector<DescriptorInfo> descInfo;                //!< lepton2-side descriptor set layout information.
    void addNewBinding(DescriptorInfo descriptorInfo, uint32_t num = 1);
};

//! Holds an array of descriptor sets which all have identical layouts
/**
 * DescriptorSetArrays are lepton2's primary interface for allocating, building, and updating descriptor sets.
 * They are typically used in conjunction with other resource multiplicities. For example, each descriptor set
 * in the array might correspond to a different frame in flight or render framebuffer index.
 * 
 * Size-1 arrays are also perfectly valid for cases where only single descriptor sets are needed.
 */
class DescriptorSetArray : public DeletableVulkanResource {
   public:
    DescriptorSetArray(const DescriptorSetLayoutInfo& layoutInfo);
    DescriptorSetArray(DescriptorSetArray* layoutReference);
    DescriptorSetLayoutInfo layoutInfo;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    DescriptorPool* parent = nullptr;
    static bool isLayoutCompatible(std::vector<VkDescriptorSetLayoutBinding> a, std::vector<VkDescriptorSetLayoutBinding> b);
    void buildDescriptorSetLayout(VulkanContext* ctx); //!< Use `LayoutInfo` to build descriptorSetLayout.
    void updateAllDescriptorSets(VulkanContext* ctx);
    void destroy_back(VulkanContext* ctx) override;
    std::vector<SingleDescriptorSet> singleDescriptorSets;

   private:
    bool externalLayout = false;
};

//! Holds descriptor pools and creates new ones as they fill up.
class DescriptorPoolManager : public DeletableVulkanResource {
   public:
    void allocateDescriptorSets(VulkanContext* ctx, DescriptorSetArray* dsa, uint32_t quantity);
    void destroy_back(VulkanContext* ctx) override;

   private:
    std::vector<DescriptorPool*> pools;
};
}  // namespace lepton2::vulkancore