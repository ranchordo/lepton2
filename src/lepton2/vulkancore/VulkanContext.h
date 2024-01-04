#pragma once

#include "VulkanUtils.h"
#include "VulkanMemory.h"
#include "SwapChain.h"

namespace lepton2::vulkancore {

    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;
        bool is_complete() {
            if (!graphicsFamily.has_value()) {
                return false;
            }
            if (!presentFamily.has_value()) {
                return false;
            }
            return true;
        }
    };

    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
        bool has_formats_and_present_modes() {
            return (!formats.empty()) && (!presentModes.empty());
        }
    };

    class VulkanContext: public DeletableVulkanResource {
    public:
        VulkanContext(bool _enable_validation_layers, bool _print_debug_info, VkApplicationInfo appInfo, GLFWwindow* _window):
            allocManager(this), swapChain(this) {

            this->window = _window;
            this->enable_validation_layers = _enable_validation_layers;
            this->print_debug_info = _print_debug_info;
            this->createInstance(appInfo);
            this->createSurface();
            this->pickPhysicalDevice();
            this->createLogicalDevice();
            this->swapChain.querySwapChain();
            this->buildAllCommandPools();
        }
        GLFWwindow* window;
        VkInstance instance;
        VkDevice device;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        struct {
            VkQueue graphics;
            VkQueue present;
        } vk_queues;
        VkSurfaceKHR surface;
        struct {
            VkCommandPool normalGraphics;
            VkCommandPool transientGraphics;
        } vk_command_pools;
        VulkanAllocationManager allocManager;
        SwapChain swapChain;
        QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
        SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
        void destroy_back(VulkanContext* ctx) override;
    private:
        bool enable_validation_layers;
        bool print_debug_info;

        VkDebugUtilsMessengerEXT debugMessenger;
        void createInstance(VkApplicationInfo appInfo);
        void createSurface();
        int calculateDeviceScore(VkPhysicalDevice device);
        void pickPhysicalDevice();
        void createLogicalDevice();
        void buildCommandPool(VkCommandPoolCreateFlags flags, uint32_t queueFamilyIndex, VkCommandPool* commandPool);
        void buildAllCommandPools();
    };
}