#include "VulkanContext.h"

using namespace lepton2::vulkancore;

#define VALIDATION_MESSAGE_TYPE_MASK (~VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT)

const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"};
const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME};

static void glfwErrorCallback(int code, const char* message) {
    printf("GLFW error, code %d: \"%s\"\n", code, message);
}

bool checkValidationLayerSupport() {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
    for (const char* layerName : validationLayers) {
        bool layerFound = false;
        for (const VkLayerProperties& layerProperties : availableLayers) {
            if (strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }
        if (!layerFound) {
            return false;
        }
    }
    return true;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {
    printf("debugUtilsMessenger callback: %s\n", pCallbackData->pMessage);
    return VK_FALSE;
}

void buildDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& msgCreateInfo) {
    msgCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    msgCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    msgCreateInfo.messageType = (VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) & VALIDATION_MESSAGE_TYPE_MASK;
    msgCreateInfo.pfnUserCallback = debugCallback;
    msgCreateInfo.pUserData = nullptr;
}

void VulkanContext::createInstance(VkApplicationInfo appInfo) {
    glfwSetErrorCallback(&glfwErrorCallback);
    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    uint32_t glfwExtensionCount;
    const char** glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    std::vector<const char*> requiredExtensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
    if (this->enable_validation_layers) requiredExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    createInfo.enabledExtensionCount = (uint32_t)requiredExtensions.size();
    createInfo.ppEnabledExtensionNames = requiredExtensions.data();
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if (this->enable_validation_layers) {
        if (!checkValidationLayerSupport()) {
            throw std::runtime_error("Not all validation layers exist.");
        }
        createInfo.enabledLayerCount = (uint32_t)validationLayers.size();
        createInfo.ppEnabledLayerNames = validationLayers.data();
        buildDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
    } else {
        createInfo.enabledLayerCount = 0;
    }
    if (vkCreateInstance(&createInfo, nullptr, &this->instance) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create vulkan instance.");
    }
    if (this->enable_validation_layers) {
        VkDebugUtilsMessengerCreateInfoEXT msgCreateInfo{};
        buildDebugMessengerCreateInfo(msgCreateInfo);
        if (EXTDoProxy(vkCreateDebugUtilsMessengerEXT, instance, &msgCreateInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create debug utils messenger.");
        }
    }
}

void VulkanContext::createSurface() {
    if (glfwCreateWindowSurface(this->instance, this->window, nullptr, &this->surface) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create window surface.");
    }
}

void printDeviceName(VkPhysicalDevice device) {
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);
    printf("%s", deviceProperties.deviceName);
}

QueueFamilyIndices VulkanContext::findQueueFamilies(VkPhysicalDevice device) {
    QueueFamilyIndices indices;
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());
    for (uint32_t i = 0; i < queueFamilies.size(); i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, this->surface, &presentSupport);
        if (presentSupport) {
            indices.presentFamily = i;
        }
        if (indices.is_complete()) {
            break;
        }
    }
    return indices;
}

bool checkDeviceExtensionSupport(VkPhysicalDevice device) {
    uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());
    std::unordered_set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());
    for (const VkExtensionProperties& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }
    return requiredExtensions.empty();
}

SwapChainSupportDetails VulkanContext::querySwapChainSupport(VkPhysicalDevice device) {
    SwapChainSupportDetails details;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, this->surface, &details.capabilities);
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, this->surface, &formatCount, nullptr);
    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, this->surface, &formatCount, details.formats.data());
    }
    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, this->surface, &presentModeCount, nullptr);
    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, this->surface, &presentModeCount, details.presentModes.data());
    }
    return details;
}

int VulkanContext::calculateDeviceScore(VkPhysicalDevice device) {
#define do_absolute_criterion(namestr, test)                                                                                                        \
    {                                                                                                                                               \
        bool result = isSuitable && (test);                                                                                                         \
        if (this->print_debug_info) printf(" --> Necessary criterion - %s: %s.\n", namestr, result ? "YES" : (isSuitable ? "NO" : "Not checking")); \
        isSuitable = isSuitable && result;                                                                                                          \
    }
#define do_score_criterion(namestr, weight, value)                                                                                                                \
    {                                                                                                                                                             \
        int result = (value);                                                                                                                                     \
        if (this->print_debug_info) printf(" --> Scored criterion, weight %f - %s: %f --> %f.\n", float(weight), namestr, float(result), float(result * weight)); \
        totalScore += result * weight;                                                                                                                            \
    }

    VkPhysicalDeviceProperties deviceProperties;
    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);
    vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
    if (this->print_debug_info) {
        printf("Considering VkPhysicalDevice \"%s\":\n", deviceProperties.deviceName);
    }
    bool isSuitable = true;
    float totalScore = 0.0;
    do_absolute_criterion("Has necessary queue family support", findQueueFamilies(device).is_complete());
    do_absolute_criterion("Has necessary extention support", checkDeviceExtensionSupport(device));
    do_absolute_criterion("Adequate swapchain support", this->querySwapChainSupport(device).has_formats_and_present_modes());
    do_absolute_criterion("Sampler anisotropy", deviceFeatures.samplerAnisotropy);
    if (isSuitable) {
        do_score_criterion("Is discrete GPU", 10000, int(deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU));
        do_score_criterion("Image dimension limits", 0.03125, (deviceProperties.limits.maxImageDimension2D));
    } else if (this->print_debug_info) {
        printf(" --> Device is not suitable. Not checking score criteria...\n");
    }
    if (this->print_debug_info) {
        printf(" --> Total device score: %d.\n\n", int(totalScore * int(isSuitable)));
    }
#undef do_absolute_criterion
#undef do_score_criterion
    return int(totalScore * int(isSuitable));
}

void VulkanContext::pickPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(this->instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        throw std::runtime_error("Failed to find any physical devices.");
    }
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(this->instance, &deviceCount, devices.data());
    int bestDeviceScore = 0;
    for (const VkPhysicalDevice device : devices) {
        int score = this->calculateDeviceScore(device);
        if (score > bestDeviceScore) {
            bestDeviceScore = score;
            this->physicalDevice = device;
        }
    }
    if (this->physicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("No suitable GPU found.");
    }
    if (this->print_debug_info) {
        printf("Picked physical device \"");
        printDeviceName(physicalDevice);
        printf("\".\n");
    }
}

void VulkanContext::createLogicalDevice() {
    QueueFamilyIndices indices = this->findQueueFamilies(this->physicalDevice);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::unordered_set<uint32_t> uniqueQueueFamilies = {
        indices.graphicsFamily.value(),
        indices.presentFamily.value()};
    float queuePriorities[1] = {1.0f};
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = queuePriorities;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.samplerAnisotropy = VK_TRUE;
    deviceFeatures.fillModeNonSolid = VK_TRUE;
    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.queueCreateInfoCount = (uint32_t)queueCreateInfos.size();
    createInfo.pEnabledFeatures = &deviceFeatures;

    createInfo.enabledExtensionCount = (uint32_t)deviceExtensions.size();
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();
    if (this->enable_validation_layers) {
        createInfo.enabledLayerCount = (uint32_t)validationLayers.size();
        createInfo.ppEnabledLayerNames = validationLayers.data();
    } else {
        createInfo.enabledLayerCount = 0;
    }
    if (vkCreateDevice(this->physicalDevice, &createInfo, nullptr, &this->device) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create logical device.");
    }
    vkGetDeviceQueue(this->device, indices.graphicsFamily.value(), 0, &this->vk_queues.graphics);
    vkGetDeviceQueue(this->device, indices.presentFamily.value(), 0, &this->vk_queues.present);
}

void VulkanContext::buildCommandPool(VkCommandPoolCreateFlags flags, uint32_t queueFamilyIndex, VkCommandPool* commandPool) {
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = flags;
    poolInfo.queueFamilyIndex = queueFamilyIndex;
    if (vkCreateCommandPool(this->device, &poolInfo, nullptr, commandPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create command pool.");
    }
}

void VulkanContext::buildAllCommandPools() {
    QueueFamilyIndices indices = this->findQueueFamilies(this->physicalDevice);
    // Normal graphics
    {
        VkCommandPoolCreateFlags flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        uint32_t queueFamilyIndex = indices.graphicsFamily.value();
        this->buildCommandPool(flags, queueFamilyIndex, &this->vk_command_pools.normalGraphics);
    }
    // Transient graphics
    {
        VkCommandPoolCreateFlags flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        uint32_t queueFamilyIndex = indices.graphicsFamily.value();
        this->buildCommandPool(flags, queueFamilyIndex, &this->vk_command_pools.transientGraphics);
    }
}

void VulkanContext::destroy_back(VulkanContext* ctx) {
    this->swapChain.destroy(ctx);
    this->descriptorPoolManager.destroy(ctx);
    this->allocManager.destroy(ctx);
    if (this->vk_command_pools.normalGraphics != VK_NULL_HANDLE) {
        vkDestroyCommandPool(this->device, vk_command_pools.normalGraphics, nullptr);
    }
    if (this->vk_command_pools.transientGraphics != VK_NULL_HANDLE) {
        vkDestroyCommandPool(this->device, vk_command_pools.transientGraphics, nullptr);
    }

    if (this->device != VK_NULL_HANDLE) {
        vkDestroyDevice(this->device, nullptr);
    }
    if (enable_validation_layers && debugMessenger != VK_NULL_HANDLE) {
        EXTDoVoidProxy(vkDestroyDebugUtilsMessengerEXT, instance, debugMessenger, nullptr);
    }
    if (this->surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(this->instance, surface, nullptr);
    }
    if (this->instance != VK_NULL_HANDLE) {
        vkDestroyInstance(this->instance, nullptr);
    }
    if (this->window != nullptr) {
        glfwDestroyWindow(window);
    }
}