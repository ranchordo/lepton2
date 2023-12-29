#include "SwapChain.h"

#include "VulkanContext.h"

using namespace lepton2::vulkancore;

VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    for (const VkSurfaceFormatKHR& availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }
    return availableFormats[0]; // TODO: Better ranking system.
}

VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
    for (const VkPresentModeKHR& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return availablePresentMode;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D chooseSwapExtent(VulkanContext* ctx, const VkSurfaceCapabilitiesKHR& capabilities) {
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    } else {
        int width, height;
        glfwGetFramebufferSize(ctx->window, &width, &height);

        VkExtent2D actualExtent = {
            (uint32_t)width,
            (uint32_t)height
        };
        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
        return actualExtent;
    }
}

void SwapChain::createSwapChain() {
    SwapChainSupportDetails swapChainSupport = ctx->querySwapChainSupport(ctx->physicalDevice);
    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = chooseSwapExtent(ctx, swapChainSupport.capabilities);
    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }
    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = ctx->surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1; // For stereoscopy, maybe 2?
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    QueueFamilyIndices indices = ctx->findQueueFamilies(ctx->physicalDevice);
    uint32_t queueFamilyIndices[] = {
        indices.graphicsFamily.value(),
        indices.presentFamily.value()
    };
    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = nullptr;
    }
    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // TODO: Figure out proper alpha compositing.
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;
    if (vkCreateSwapchainKHR(ctx->device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create swapchain.");
    }
    vkGetSwapchainImagesKHR(ctx->device, swapChain, &imageCount, nullptr);
    swapChainImages.resize(imageCount);
    std::vector<VkImage> swapChainVkImages;
    swapChainVkImages.resize(imageCount);
    swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(ctx->device, swapChain, &imageCount, swapChainVkImages.data());
    swapChainImageFormat = surfaceFormat.format;
    swapChainExtent = extent;
    // Create image views
    for (size_t i = 0; i < swapChainImages.size(); i++) {
        swapChainImages[i] = new VulkanImage();
        swapChainImages[i]->imageView = createImageView(ctx, swapChainVkImages[i], swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
        swapChainImages[i]->image = swapChainVkImages[i];
        swapChainImages[i]->do_not_destroy_image = true;
        swapChainImages[i]->imageFormat = swapChainImageFormat;
        swapChainImages[i]->chonklet.chonkus = nullptr;
    }

    // Create default depth image
    this->defaultDepthImage = new VulkanImage();
    this->defaultDepthImage->imageFormat = VK_FORMAT_D32_SFLOAT_S8_UINT;
    createImage(this->ctx, swapChainExtent.width, swapChainExtent.height, defaultDepthImage->imageFormat,
        VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        VK_IMAGE_ASPECT_DEPTH_BIT, defaultDepthImage);

    this->swapChainFramebuffers.resize(this->swapChainImages.size());
    for (uint32_t i = 0; i < this->swapChainImages.size(); i++) {
        Framebuffer* nfb = new Framebuffer();
        nfb->addColorImage(this->swapChainImages.at(i));
        nfb->setDepthImage(defaultDepthImage);
        // Don't actually build yet, need a renderPass object
        // But we can still set these up as framebuffer structural references.
        this->swapChainFramebuffers[i] = nfb;
    }
}

void SwapChain::destroy_back(VulkanContext* ctx) {
    for (VulkanImage* img : this->swapChainImages) {
        img->destroy(ctx);
        delete img;
    }
    this->defaultDepthImage->destroy(ctx);
    delete this->defaultDepthImage;
    for (Framebuffer* fb : this->swapChainFramebuffers) {
        fb->destroy(ctx);
        delete fb;
    }
    vkDestroySwapchainKHR(ctx->device, this->swapChain, nullptr);
}

void SwapChain::rebuildSwapChain() {
    int width = 0, height = 0;
    glfwGetFramebufferSize(ctx->window, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(ctx->window, &width, &height);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(ctx->device);
    this->destroy_back(this->ctx);
    this->createSwapChain();
}

void SwapChain::createFramebuffers(VkRenderPass renderPass, VulkanImage* depthImage) {
    for (uint32_t i = 0; i < this->swapChainImages.size(); i++) {
        swapChainFramebuffers.at(i)->buildFramebuffer(renderPass, swapChainExtent.width, swapChainExtent.height);
    }
}