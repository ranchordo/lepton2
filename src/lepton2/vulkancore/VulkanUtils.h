#pragma once

// A huge thanks to Alexander Overvoorde, and all of the contributors to github.com/Overv/VulkanTutorial,
// without whose work this library would not be remotely similar.
// Their c++ work, from which vulkancore is based, is licensed as CC0 v1.0.

#ifdef __INTELLISENSE__
#define DEBUG_ENV
#endif

#if defined(__APPLE__)
#include <MoltenVK/mvk_vulkan.h>
#endif

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <stdio.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <list>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../../external/stb_image.h"
#include "../../external/tiny_obj_loader.h"

#define EXTBuildProxyFuncptr(name) ((PFN_##name)vkGetInstanceProcAddr(instance, #name))
#define EXTDoProxy(name, ...) ((EXTBuildProxyFuncptr(name) == nullptr) ? (VK_ERROR_EXTENSION_NOT_PRESENT) : (EXTBuildProxyFuncptr(name)(__VA_ARGS__)))
#define EXTDoVoidProxy(name, ...)                \
    if (EXTBuildProxyFuncptr(name) != nullptr) { \
        EXTBuildProxyFuncptr(name)(__VA_ARGS__); \
    }

#define CHECK_DESTRUCTION(status) \
    {                             \
        if (this->destroyed) {    \
            return status;        \
        }                         \
    }
#define CHECK_DESTRUCTION_VOID() \
    {                            \
        if (this->destroyed) {   \
            return;              \
        }                        \
    }

// Don't ask.
#define SEPPUKU() \
    { *(int*)0 = 0; }

typedef std::chrono::steady_clock::time_point lepton2_time_point;

namespace lepton2::vulkancore {

class VulkanContext;
class VulkanImage;
class VulkanBuffer;
struct RenderTargetImageCreationInfo;

class DeletableVulkanResource {
   public:
    virtual void destroy_back(VulkanContext* ctx) = 0;
    void destroy(VulkanContext* ctx) {
        if (!destroyed) {
            for (int i = linked.size() - 1; i >= 0; i--) {
                std::pair<bool, DeletableVulkanResource*> pair = linked[i];
                pair.second->destroy(ctx);
                if (pair.first) {
                    delete pair.second;
                }
            }
            destroy_back(ctx);
        }
        this->destroyed = true;
    }
    void addLinkedResource(DeletableVulkanResource* linked, bool deleteptr) {
        std::pair<bool, DeletableVulkanResource*> pair(deleteptr, linked);
        this->linked.push_back(pair);
    }
    bool removeLinkedResource(DeletableVulkanResource* resource) {
        for (uint32_t i = 0; i < linked.size(); i++) {
            if (linked[i].second == resource) {
                linked.erase(linked.begin() + i);
                return true;
            }
        }
        return false;
    }
    virtual ~DeletableVulkanResource() {}

   protected:
    bool destroyed = false;
    std::vector<std::pair<bool, DeletableVulkanResource*>> linked;
};

class DeletableVulkanResourceTracker : public DeletableVulkanResource {
   public:
    void destroy_back(VulkanContext* ctx) override {}
};

enum ImageLayoutTransitionMode {
    ILTM_UNDEFINED_TO_TRANSFER_DST,
    ILTM_TRANSFER_DST_TO_SHADER_READ_ONLY,
    ILTM_UNDEFINED_TO_SHADER_READ_ONLY
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
extern void transitionImageLayout(VulkanContext* ctx, VulkanImage* image, ImageLayoutTransitionMode iltm);
extern void copyBufferToImage(VulkanContext* ctx, VulkanBuffer* buffer, VulkanImage* image, uint32_t width, uint32_t height);
extern VkImageView createImageView(VulkanContext* ctx, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
extern void copyBuffer(VulkanContext* ctx, VulkanBuffer* src, VulkanBuffer* dst, VkDeviceSize size,
                       VkDeviceSize srcOffset = 0, VkDeviceSize dstOffset = 0);
extern std::vector<char> readFile(const std::string& filename);
extern VkSemaphore createGenericSemaphore(VulkanContext* ctx);
extern VkFence createGenericFence(VulkanContext* ctx, bool signaled);
extern lepton2_time_point startTiming();
extern double getElapsedSeconds(lepton2_time_point time_point);
extern std::filesystem::path getExecutableLocation(const char* argv0, bool force_absolute);
}  // namespace lepton2::vulkancore