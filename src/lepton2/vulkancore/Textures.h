#pragma once

#include "VulkanObjects.h"
#include "VulkanUtils.h"

namespace lepton2::vulkancore {

class VulkanContext;
class Texture;
class TextureComponent : public DeletableVulkanResource {
   public:
    TextureComponent(VulkanContext* ctx, void* imageData, uint32_t width, uint32_t height);
    TextureComponent(VulkanContext* ctx, const char* filename);
    TextureComponent(VulkanImage* image) { this->textureImage = image; }
    TextureComponent(bool useSwapChainIndexing) {
        this->useSwapChainIndexing = useSwapChainIndexing;
    }
    VulkanImage* textureImage = nullptr;
    std::vector<VulkanImage*> swapChainIndexedImages;
    void destroy_back(VulkanContext* ctx) override;
    bool useSwapChainIndexing = false;

   private:
    void createTextureImage(VulkanContext* ctx, void* imageData, uint32_t width, uint32_t height);
};

struct SamplerInfo {
    VkFilter filter = VK_FILTER_LINEAR;
    VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    VkBool32 anisotropyEnable = VK_TRUE;
    VkBorderColor borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    VkBool32 compareEnable = VK_FALSE;
    VkCompareOp compareOp = VK_COMPARE_OP_ALWAYS;
};

class Texture : public DeletableVulkanResource {
   public:
    Texture(VulkanContext* ctx, SamplerInfo samplerInfo);
    // Note: Texture components are then linked for destruction!
    void addTextureComponent(TextureComponent* component);
    void addTextureComponent(VulkanContext* ctx, const char* filename);
    void destroy_back(VulkanContext* ctx) override;
    std::vector<TextureComponent*> components;
    VkSampler textureSampler;
};

};  // namespace lepton2::vulkancore