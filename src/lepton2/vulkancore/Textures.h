#pragma once

#include "VulkanObjects.h"
#include "VulkanUtils.h"

namespace lepton2::vulkancore {

//! Represents a single component of a complex texture (e.g. albedo, normal, etc).
class TextureComponent : public DeletableVulkanResource {
   public:
    TextureComponent(VulkanContext* ctx, void* imageData, uint32_t width, uint32_t height, VkFormat format);
    TextureComponent(VulkanContext* ctx, const char* filename, VkFormat format);
    TextureComponent(VulkanImage* image) { this->image = image; }
    TextureComponent(ImageArray* imageArray) { this->imageArray = imageArray; }

    VulkanImage* image = nullptr;
    ImageArray* imageArray = nullptr;
    void destroy_back(VulkanContext* ctx) override;

   private:
    void buildImage(VulkanContext* ctx, void* imageData, uint32_t width, uint32_t height, VkFormat format);
};

struct SamplerInfo {
    VkFilter filter = VK_FILTER_LINEAR;
    VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    VkBool32 anisotropyEnable = VK_TRUE;
    VkBorderColor borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    VkBool32 compareEnable = VK_FALSE;
    VkCompareOp compareOp = VK_COMPARE_OP_ALWAYS;
};

//! A collection of texture components.
/**
 * Note: all added texture components will be automatically destroyed upon Texture::destroy(VulkanContext*).
 */
class Texture : public DeletableVulkanResource {
   public:
    Texture(VulkanContext* ctx, SamplerInfo samplerInfo);
    void addTextureComponent(TextureComponent* images);
    void addTextureComponent(VulkanContext* ctx, const char* filename, VkFormat format);
    void destroy_back(VulkanContext* ctx) override;
    std::vector<TextureComponent*> components;
    VkSampler textureSampler;
};

};  // namespace lepton2::vulkancore