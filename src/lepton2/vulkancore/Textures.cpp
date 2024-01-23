#include "Textures.h"

#include "VulkanContext.h"

#define STB_IMAGE_IMPLEMENTATION
#include "../../external/stb_image.h"

using namespace lepton2::vulkancore;

TextureComponent::TextureComponent(VulkanContext* ctx, void* imageData, uint32_t width, uint32_t height) {
    this->createTextureImage(ctx, imageData, width, height);
}

TextureComponent::TextureComponent(VulkanContext* ctx, const char* filename) {
    size_t combined_length = snprintf(nullptr, 0, "%s/%s", ctx->assets_load_path, filename);
    char filename_buffer[combined_length + 1];
    snprintf(filename_buffer, combined_length + 1, "%s/%s", ctx->assets_load_path, filename);
    int width, height, channels;
    stbi_uc* imageData = stbi_load(filename_buffer, &width, &height, &channels, STBI_rgb_alpha);
    if (!imageData) {
        throw std::runtime_error("Failed to load a texture file.");
    }
    this->createTextureImage(ctx, imageData, (uint32_t)width, (uint32_t)height);
    stbi_image_free(imageData);
}

void TextureComponent::createTextureImage(VulkanContext* ctx, void* imageData, uint32_t width, uint32_t height) {
    VkDeviceSize size = width * height * 4;
    VulkanBuffer staging;
    createBuffer(ctx, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &staging);
    void* stagingMemory = staging.chonklet.mapMemory(ctx, 0);
    memcpy(stagingMemory, imageData, (size_t)size);
    staging.chonklet.unmapMemory(ctx);

    this->textureImage = new VulkanImage();
    createImage(ctx, width, height, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_SAMPLE_COUNT_1_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                VK_IMAGE_ASPECT_COLOR_BIT, textureImage);
    transitionImageLayout(ctx, textureImage, ILTM_UNDEFINED_TO_TRANSFER_DST);
    copyBufferToImage(ctx, &staging, textureImage, width, height);
    transitionImageLayout(ctx, textureImage, ILTM_TRANSFER_DST_TO_SHADER_READ_ONLY);
    staging.destroy(ctx);
}

void TextureComponent::destroy_back(VulkanContext* ctx) {
    if (this->textureImage != nullptr) {
        this->textureImage->destroy(ctx);
        delete this->textureImage;
    }
}

Texture::Texture(VulkanContext* ctx, SamplerInfo samplerInfo) {
    VkSamplerCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    createInfo.magFilter = samplerInfo.filter;
    createInfo.minFilter = samplerInfo.filter;
    createInfo.addressModeU = samplerInfo.addressMode;
    createInfo.addressModeV = samplerInfo.addressMode;
    createInfo.addressModeW = samplerInfo.addressMode;
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(ctx->physicalDevice, &properties);
    createInfo.anisotropyEnable = VK_TRUE;
    createInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
    createInfo.borderColor = samplerInfo.borderColor;
    createInfo.unnormalizedCoordinates = VK_FALSE;
    createInfo.compareEnable = samplerInfo.compareEnable;
    createInfo.compareOp = samplerInfo.compareOp;
    createInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    createInfo.mipLodBias = 0.0f;
    createInfo.minLod = 0.0f;
    createInfo.maxLod = 0.0f;

    if (vkCreateSampler(ctx->device, &createInfo, nullptr, &this->textureSampler) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create texture sampler.");
    }
}

void Texture::addTextureComponent(TextureComponent* component) {
    this->components.push_back(component);
}

void Texture::addTextureComponent(VulkanContext* ctx, const char* filename) {
    this->addTextureComponent(new TextureComponent(ctx, filename));
}

void Texture::destroy_back(VulkanContext* ctx) {
    vkDestroySampler(ctx->device, this->textureSampler, nullptr);
    for (TextureComponent* component : this->components) {
        component->destroy(ctx);
        delete component;
    }
}