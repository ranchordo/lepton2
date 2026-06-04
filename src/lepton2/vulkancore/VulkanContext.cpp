#include "VulkanContext.h"

#include <filesystem>

#include "../utils/LeptonUtils.h"

#define EXTBuildProxyFuncptr(name) ((PFN_##name)vkGetInstanceProcAddr(instance, #name))
#define EXTDoProxy(name, ...) ((EXTBuildProxyFuncptr(name) == nullptr) ? (VK_ERROR_EXTENSION_NOT_PRESENT) : (EXTBuildProxyFuncptr(name)(__VA_ARGS__)))
#define EXTDoVoidProxy(name, ...)                \
    if (EXTBuildProxyFuncptr(name) != nullptr) { \
        EXTBuildProxyFuncptr(name)(__VA_ARGS__); \
    }

using namespace lepton2::vulkancore;

const std::vector<const char*> validationLayers = {"VK_LAYER_KHRONOS_validation"};

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

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT sev,
                                                    VkDebugUtilsMessageTypeFlagsEXT type,
                                                    const VkDebugUtilsMessengerCallbackDataEXT* data, void* pUserData) {
    printf("[vk_debug_utils] ");
    if (sev & MSG_SEV_ERROR) printf("[sev_error] ");
    if (sev & MSG_SEV_WARN) printf("[sev_warning] ");
    if (sev & MSG_SEV_INFO) printf("[sev_info] ");
    if (sev & MSG_SEV_VERBOSE) printf("[sev_verbose] ");
    if (type & MSG_TYPE_GENERAL) printf("[type_general] ");
    if (type & MSG_TYPE_VALIDATION) printf("[type_validation] ");
    if (type & MSG_TYPE_PERFORMANCE) printf("[type_performance] ");
    printf("%s\n", data->pMessage);
    return VK_FALSE;
}

void buildDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& msgCreateInfo) {
    msgCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    msgCreateInfo.pNext = nullptr;
    msgCreateInfo.messageSeverity = MSG_SEV_INFO | MSG_SEV_WARN | MSG_SEV_ERROR;
    msgCreateInfo.messageType = MSG_TYPE_GENERAL | MSG_TYPE_VALIDATION | MSG_TYPE_PERFORMANCE;
    msgCreateInfo.pfnUserCallback = debugCallback;
    msgCreateInfo.pUserData = nullptr;
}

void VulkanContext::createInstance(VkApplicationInfo appInfo) {
    if (this->window != nullptr) {
        glfwSetErrorCallback(&glfwErrorCallback);
    }
    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.pApplicationInfo = &appInfo;

    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = nullptr;
    if (this->window != nullptr) {
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    }
    std::vector<const char*> requiredExtensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
    if (this->enableValidationLayers) requiredExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    requiredExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    createInfo.enabledExtensionCount = (uint32_t)requiredExtensions.size();
    createInfo.ppEnabledExtensionNames = requiredExtensions.data();

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if (this->enableValidationLayers) {
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
    if (this->enableValidationLayers) {
        VkDebugUtilsMessengerCreateInfoEXT msgCreateInfo{};
        buildDebugMessengerCreateInfo(msgCreateInfo);
        if (EXTDoProxy(vkCreateDebugUtilsMessengerEXT, instance, &msgCreateInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create debug utils messenger.");
        }

        // Test validation layers
        submitDebugMessage(this, "Lepton2 internal: Debug messenger callback active.\n", MSG_SEV_WARN, MSG_TYPE_VALIDATION);
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
        if ((queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT)) {
            indices.graphicsComputeFamily = i;
        } else if (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
            indices.computeFamily = i;  // Dedicated compute
        }
        VkBool32 presentSupport = false;
        if (this->surface != VK_NULL_HANDLE) {
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, this->surface, &presentSupport);
        }
        if (presentSupport) {
            indices.presentFamily = i;
        }
        if (indices.isComplete(this->window != nullptr)) {
            break;
        }
    }
    if (!indices.computeFamily.has_value()) {  // Fallback - asynchronous compute not supported
        indices.computeFamily = indices.graphicsComputeFamily;
    }
    return indices;
}

const std::vector<const char*> VulkanContext::getRequiredDeviceExtensions() {
    std::vector<const char*> result;
    result.push_back(VK_KHR_SHADER_ATOMIC_INT64_EXTENSION_NAME);
    if (this->window != nullptr) {
        result.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    }
    return result;
}

bool checkDeviceExtensionSupport(VkPhysicalDevice device, const std::vector<const char*> deviceExtensions) {
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

SwapchainSupportDetails VulkanContext::querySwapchainSupport(VkPhysicalDevice device) {
    SwapchainSupportDetails details;
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
#define do_absolute_criterion(namestr, test)                                                                                                      \
    {                                                                                                                                             \
        bool result = isSuitable && (test);                                                                                                       \
        if (this->printDebugInfo) printf(" --> Necessary criterion - %s: %s.\n", namestr, result ? "YES" : (isSuitable ? "NO" : "Not checking")); \
        isSuitable = isSuitable && result;                                                                                                        \
    }
#define do_score_criterion(namestr, weight, value)                                                                                                                    \
    {                                                                                                                                                                 \
        int result = (value);                                                                                                                                         \
        if (this->printDebugInfo) printf(" --> Scored criterion, weight %.2f - %s: %.2f --> %.2f.\n", float(weight), namestr, float(result), float(result * weight)); \
        totalScore += result * weight;                                                                                                                                \
    }

    VkPhysicalDeviceProperties deviceProperties;
    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);
    vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
    if (this->printDebugInfo) {
        printf("Considering VkPhysicalDevice \"%s\":\n", deviceProperties.deviceName);
    }
    bool isSuitable = true;
    float totalScore = 0.0;
    QueueFamilyIndices qfis = findQueueFamilies(device);
    do_absolute_criterion("Adequate queue family support", qfis.isComplete(this->window != nullptr));
    do_absolute_criterion("Adequate extention support", checkDeviceExtensionSupport(device, getRequiredDeviceExtensions()));
    if (this->window != nullptr) {
        do_absolute_criterion("Adequate swapchain support", this->querySwapchainSupport(device).has_formats_and_present_modes());
    }
    do_absolute_criterion("Sampler anisotropy", deviceFeatures.samplerAnisotropy);
    do_absolute_criterion("Independent blending support", deviceFeatures.independentBlend);
    if (isSuitable) {
        do_score_criterion("Is discrete GPU", 10000, int(deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU));
        do_score_criterion("Image dimension limit", 0.05, (deviceProperties.limits.maxImageDimension2D));
        do_score_criterion("Dedicated asynchronous compute queue", 2048, int(qfis.isComplete(false) && (qfis.computeFamily != qfis.graphicsComputeFamily)))
    } else if (this->printDebugInfo) {
        printf(" --> Device is not suitable. Not checking score criteria.\n");
    }
    if (this->printDebugInfo) {
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
    if (this->printDebugInfo) {
        printf("Picked physical device \"");
        printDeviceName(physicalDevice);
        printf("\".\n");
    }
}

static void addUniqueIndex(std::vector<uint32_t>& vec, uint32_t idx) {
    for (uint32_t i = 0; i < vec.size(); i++) {
        if (vec[i] == idx) return;
    }
    vec.push_back(idx);
}

void VulkanContext::createLogicalDevice() {
    QueueFamilyIndices indices = this->findQueueFamilies(this->physicalDevice);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::vector<uint32_t> uniqueQueueFamilies;  // Use vector instead of unordered_set because it's so small
    if (indices.graphicsComputeFamily.has_value()) addUniqueIndex(uniqueQueueFamilies, indices.graphicsComputeFamily.value());
    if (indices.computeFamily.has_value()) addUniqueIndex(uniqueQueueFamilies, indices.computeFamily.value());
    if (indices.presentFamily.has_value()) addUniqueIndex(uniqueQueueFamilies, indices.presentFamily.value());
    float queuePriorities[1] = {1.0f};
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.pNext = nullptr;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = queuePriorities;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.samplerAnisotropy = VK_TRUE;
    deviceFeatures.sampleRateShading = VK_TRUE;
    deviceFeatures.fillModeNonSolid = VK_TRUE;
    deviceFeatures.independentBlend = VK_TRUE;
    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.queueCreateInfoCount = (uint32_t)queueCreateInfos.size();
    createInfo.pEnabledFeatures = &deviceFeatures;

    const std::vector<const char*> deviceExtensions = this->getRequiredDeviceExtensions();

    createInfo.enabledExtensionCount = (uint32_t)deviceExtensions.size();
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();
    if (this->enableValidationLayers) {
        createInfo.enabledLayerCount = (uint32_t)validationLayers.size();
        createInfo.ppEnabledLayerNames = validationLayers.data();
    } else {
        createInfo.enabledLayerCount = 0;
    }
    if (vkCreateDevice(this->physicalDevice, &createInfo, nullptr, &this->device) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create logical device.");
    }
    vkGetDeviceQueue(this->device, indices.graphicsComputeFamily.value(), 0, &this->queues.graphics);
    if (indices.graphicsComputeFamily.value() != indices.computeFamily.value()) {
        vkGetDeviceQueue(this->device, indices.computeFamily.value(), 0, &this->queues.compute);
    } else {
        this->queues.compute = this->queues.graphics;
    }
    if (indices.presentFamily.has_value()) {
        vkGetDeviceQueue(this->device, indices.presentFamily.value(), 0, &this->queues.present);
    }
}

void VulkanContext::buildCommandPool(VkCommandPoolCreateFlags flags, uint32_t queueFamilyIndex, VkCommandPool* commandPool) {
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.pNext = nullptr;
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
        uint32_t queueFamilyIndex = indices.graphicsComputeFamily.value();
        this->buildCommandPool(flags, queueFamilyIndex, &this->commandPools.normalGraphicsCompute);
    }
    // Transient graphics
    {
        VkCommandPoolCreateFlags flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        uint32_t queueFamilyIndex = indices.graphicsComputeFamily.value();
        this->buildCommandPool(flags, queueFamilyIndex, &this->commandPools.transientGraphicsCompute);
    }

    if (indices.graphicsComputeFamily.value() != indices.computeFamily.value()) {
        // Normal compute
        {
            VkCommandPoolCreateFlags flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            uint32_t queueFamilyIndex = indices.computeFamily.value();
            this->buildCommandPool(flags, queueFamilyIndex, &this->commandPools.normalCompute);
        }
        // Transient compute
        {
            VkCommandPoolCreateFlags flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
            uint32_t queueFamilyIndex = indices.computeFamily.value();
            this->buildCommandPool(flags, queueFamilyIndex, &this->commandPools.transientCompute);
        }
    } else {
        this->commandPools.normalCompute = this->commandPools.normalGraphicsCompute;
        this->commandPools.transientCompute = this->commandPools.transientGraphicsCompute;
    }
}

void VulkanContext::setRelativePaths(const char* argv0) {
    std::filesystem::path shader_location_path = lepton2::utils::getExecutableLocation(argv0, false).append("shaders");
    std::string shader_location = shader_location_path.string();
    this->shaderSpirvLoadPath = (char*)malloc(shader_location.length() + 1);
    strcpy(this->shaderSpirvLoadPath, shader_location.c_str());

    std::filesystem::path asset_location_path = lepton2::utils::getExecutableLocation(argv0, false).append("assets");
    std::string asset_location = asset_location_path.string();
    this->assetsLoadPath = (char*)malloc(asset_location.length() + 1);
    strcpy(this->assetsLoadPath, asset_location.c_str());
}

char* VulkanContext::buildShaderLoadPaths(const char* rel, bool compute) {
    if (compute) {
        size_t len = snprintf(nullptr, 0, "%s/%s.comp.spv", this->shaderSpirvLoadPath, rel);
        char* buf = (char*)malloc(sizeof(char) * (len + 1));
        snprintf(buf, len + 1, "%s/%s.comp.spv", this->shaderSpirvLoadPath, rel);
        return buf;
    }
    size_t combined_length = snprintf(nullptr, 0, "%s/%s.____.spv", this->shaderSpirvLoadPath, rel);
    char* buf = (char*)malloc(sizeof(char) * 2 * (combined_length + 1));
    snprintf(buf, combined_length + 1, "%s/%s.vert.spv", this->shaderSpirvLoadPath, rel);
    snprintf(buf + combined_length + 1, combined_length + 1, "%s/%s.frag.spv", this->shaderSpirvLoadPath, rel);
    return buf;
}

char* VulkanContext::buildAssetLoadPath(const char* rel) {
    size_t combined_length = snprintf(nullptr, 0, "%s/%s", this->assetsLoadPath, rel);
    char* buf = (char*)malloc(sizeof(char) * (combined_length + 1));
    snprintf(buf, combined_length + 1, "%s/%s", this->assetsLoadPath, rel);
    return buf;
}

void VulkanContext::destroy_back(VulkanContext* ctx) {
    if (this->window != nullptr) {
        this->swapchain.destroy(ctx);
    }
    this->descriptorPoolManager.destroy(ctx);
    this->allocManager.destroy(ctx);
    if (this->commandPools.normalGraphicsCompute != VK_NULL_HANDLE) {
        vkDestroyCommandPool(this->device, commandPools.normalGraphicsCompute, nullptr);
    }
    if (this->commandPools.transientGraphicsCompute != VK_NULL_HANDLE) {
        vkDestroyCommandPool(this->device, commandPools.transientGraphicsCompute, nullptr);
    }
    if (commandPools.normalCompute != VK_NULL_HANDLE && commandPools.normalCompute != commandPools.normalGraphicsCompute) {
        vkDestroyCommandPool(this->device, commandPools.normalCompute, nullptr);
    }
    if (commandPools.transientCompute != VK_NULL_HANDLE && commandPools.transientCompute != commandPools.transientGraphicsCompute) {
        vkDestroyCommandPool(this->device, commandPools.transientCompute, nullptr);
    }

    if (this->device != VK_NULL_HANDLE) {
        vkDestroyDevice(this->device, nullptr);
    }
    if (enableValidationLayers && debugMessenger != VK_NULL_HANDLE) {
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
    free(this->shaderSpirvLoadPath);
    free(this->assetsLoadPath);
}