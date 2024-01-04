#pragma once

// A huge thanks to Alexander Overvoorde, and all of the contributors to github.com/Overv/VulkanTutorial,
// without whose work this library would not be remotely similar.
// Their c++ work, from which vulkancore is based, is licensed as CC0 v1.0.

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <vulkan/vulkan.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <stdio.h>
#include <vector>

#define EXTBuildProxyFuncptr(name) ((PFN_##name)vkGetInstanceProcAddr(instance, #name))
#define EXTDoProxy(name, ...) ((EXTBuildProxyFuncptr(name) == nullptr) ? (VK_ERROR_EXTENSION_NOT_PRESENT) : (EXTBuildProxyFuncptr(name)(__VA_ARGS__)))
#define EXTDoVoidProxy(name, ...)                \
    if (EXTBuildProxyFuncptr(name) != nullptr) { \
        EXTBuildProxyFuncptr(name)(__VA_ARGS__); \
    }

#define CHECK_DESTRUCTION(status) { if (this->destroyed) { return status; }}
#define CHECK_DESTRUCTION_VOID() { if (this->destroyed) { return; }}

// Don't ask.
#define SEPPUKU() { *(int*)0 = 0; }

namespace lepton2::vulkancore {

    class VulkanContext;
    class VulkanImage;
    class VulkanBuffer;
    struct RenderTargetImageCreationInfo;

    class DeletableVulkanResource {
    public:
        virtual void destroy_back(VulkanContext* ctx) = 0;
        void destroy(VulkanContext* ctx) {
            if (!destroyed) destroy_back(ctx);
            this->destroyed = true;
        }
    protected:
        bool destroyed = false;
    };

    enum ImageLayoutTransitionMode {
        ILTM_UNDEF_2_XFER_DST,
        ILTM_XFER_DST_2_FRAG_RO
    };

    extern uint32_t findMemoryType(VulkanContext* ctx, uint32_t typeFilter, VkMemoryPropertyFlags properties);
    extern void createBuffer(VulkanContext* ctx, VkDeviceSize size, VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties, VulkanBuffer* buffer);
    extern void createImage(VulkanContext* ctx, uint32_t width, uint32_t height,
        VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkSampleCountFlagBits samples,
        VkMemoryPropertyFlags properties, VkImageAspectFlags aspectFlags, VulkanImage* image);
    extern void createRenderTarget(VulkanContext* ctx, VkExtent2D extent,
        RenderTargetImageCreationInfo* rticInfo, VulkanImage* image);
    extern VkCommandBuffer beginSingleTimeCommands(VulkanContext* ctx);
    extern void endSingleTimeCommands(VulkanContext* ctx, VkCommandBuffer commandBuffer);
    extern void transitionImageLayout(VulkanContext* ctx, VkImage image, VkFormat format, VkImageLayout oldLayout,
        VkImageLayout newLayout, ImageLayoutTransitionMode iltm);
    extern void copyBufferToImage(VulkanContext* ctx, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
    extern VkImageView createImageView(VulkanContext* ctx, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
    extern void copyBuffer(VulkanContext* ctx, VulkanBuffer* src, VulkanBuffer* dst, VkDeviceSize size,
        VkDeviceSize srcOffset = 0, VkDeviceSize dstOffset = 0);
    extern std::vector<char> readFile(const std::string& filename);
    extern VkSemaphore createGenericSemaphore(VulkanContext* ctx);
    extern VkFence createGenericFence(VulkanContext* ctx, bool signaled);
}