#pragma once

#include "Descriptors.h"
#include "Swapchain.h"
#include "VulkanMemory.h"
#include "VulkanUtils.h"

#include <optional>

namespace lepton2::vulkancore {

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsComputeFamily;
    std::optional<uint32_t> computeFamily;
    std::optional<uint32_t> presentFamily;
    bool isComplete(bool requirePresent) {
        if (!graphicsComputeFamily.has_value()) return false;
        if (!computeFamily.has_value()) return false;
        if (requirePresent && !presentFamily.has_value()) return false;
        return true;
    }
};

//! Mystery swapchain-related thing not in the swapchain file
struct SwapchainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
    bool has_formats_and_present_modes() {
        return (!formats.empty()) && (!presentModes.empty());
    }
};

//! Holds the major vulkan resources (instance, device) necessary for most operations.
/**
 * Should only be initialized once. Passing nullptr instead of GLFWwindow* will initialize the vulkan context in headless mode.
 * Direct interaction with this vulkan context class should not be necessary past the vulkancore level.
 */
class VulkanContext : public DeletableVulkanResource {
   public:
    VulkanContext(const char* argv0, bool _enableValidationLayers, bool _printDebugInfo, VkApplicationInfo appInfo, GLFWwindow* _window) {
        this->window = _window;
        this->enableValidationLayers = _enableValidationLayers;
        this->printDebugInfo = _printDebugInfo;
        this->createInstance(appInfo);
        if (_window != nullptr) this->createSurface();
        this->pickPhysicalDevice();
        this->createLogicalDevice();
        this->buildAllCommandPools();
        this->setRelativePaths(argv0);
        if (_window != nullptr) this->swapchain.querySwapchain(this);
    }
    GLFWwindow* window;
    VkInstance instance = VK_NULL_HANDLE;
    VkDevice device;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    struct {
        VkQueue graphics;
        VkQueue compute;
        VkQueue present;
    } queues;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    struct {
        VkCommandPool normalGraphicsCompute;
        VkCommandPool transientGraphicsCompute;
        VkCommandPool normalCompute;
        VkCommandPool transientCompute;
    } commandPools;
    VulkanAllocationManager allocManager;
    Swapchain swapchain;
    DescriptorPoolManager descriptorPoolManager;
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
    SwapchainSupportDetails querySwapchainSupport(VkPhysicalDevice device);
    const std::vector<const char*> getRequiredDeviceExtensions();
    void destroy_back(VulkanContext* ctx) override;

    char* buildShaderLoadPaths(const char* rel, bool compute);
    char* buildAssetLoadPath(const char* rel);

   private:
    bool enableValidationLayers;
    bool printDebugInfo;

    char* shaderSpirvLoadPath;
    char* assetsLoadPath;

    VkDebugUtilsMessengerEXT debugMessenger;
    void createInstance(VkApplicationInfo appInfo);
    void createSurface();
    int calculateDeviceScore(VkPhysicalDevice device);
    void pickPhysicalDevice();
    void createLogicalDevice();
    void buildCommandPool(VkCommandPoolCreateFlags flags, uint32_t queueFamilyIndex, VkCommandPool* commandPool);
    void buildAllCommandPools();
    void setRelativePaths(const char* argv0);
};
}  // namespace lepton2::vulkancore