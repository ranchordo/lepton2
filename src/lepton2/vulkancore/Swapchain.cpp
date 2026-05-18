#include "Swapchain.h"

#include "Descriptors.h"
#include "RenderPass.h"
#include "VulkanContext.h"

using namespace lepton2::vulkancore;

VkSurfaceFormatKHR Swapchain::chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& available) {
    for (const VkSurfaceFormatKHR& format : available) {
        if (format.format == preferences.surfaceFormat.format && format.colorSpace == preferences.surfaceFormat.colorSpace) {
            return format;
        }
    }
    for (const VkSurfaceFormatKHR& format : available) {
        if (format.format == preferences.surfaceFormat.format) {
            printf("Swapchain::chooseSurfaceFormat: preferred surfaceFormat.colorSpace unavailable.\n");
            return format;
        }
    }
    for (const VkSurfaceFormatKHR& format : available) {
        if (format.colorSpace == preferences.surfaceFormat.colorSpace) {
            printf("SwapChain::chooseSurfaceFormat: preferred surfaceFormat.format unavailable.\n");
            return format;
        }
    }
    printf("Swapchain::chooseSurfaceFormat: all preferred surfaceFormat characteristics unavailable. Select available[0].\n");
    return available[0];
}

VkPresentModeKHR Swapchain::choosePresentMode(const std::vector<VkPresentModeKHR>& available) {
    for (const VkPresentModeKHR& mode : available) {
        if (mode == this->preferences.presentMode) return mode;
    }

    for (const VkPresentModeKHR& mode : available) {
        if (mode == VK_PRESENT_MODE_FIFO_KHR) {
            printf("Swapchain::choosePresentMode: preferred present mode unavailable, default to VK_PRESENT_MODE_FIFO_KHR.\n");
            return VK_PRESENT_MODE_FIFO_KHR;
        }
    }
    printf("Swapchain::choosePresentMode: neither preference nor default available. Select available[0].\n");
    return available[0];
}

VkExtent2D Swapchain::getGLFWExtent(VulkanContext* ctx) {
    int width, height;
    glfwGetFramebufferSize(ctx->window, &width, &height);
    return {(uint32_t)width, (uint32_t)height};
}

TerminatingSubpassConfig Swapchain::getDefaultPresentConfig() {
    return RenderSubpass::getDefaultTerminatingConfig(getImageFormat(), VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
}

static VkExtent2D chooseSwapExtent(VulkanContext* ctx, const VkSurfaceCapabilitiesKHR& capabilities, VkExtent2D actualExtent) {
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    } else {
        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
        return actualExtent;
    }
}

void Swapchain::querySwapchain(VulkanContext* ctx) {
    SwapchainSupportDetails swapchainSupport = ctx->querySwapchainSupport(ctx->physicalDevice);
    this->queryResults.surfaceFormat = this->chooseSurfaceFormat(swapchainSupport.formats);
    this->queryResults.presentMode = this->choosePresentMode(swapchainSupport.presentModes);
    this->extent = chooseSwapExtent(ctx, swapchainSupport.capabilities, Swapchain::getGLFWExtent(ctx));
    this->queryResults.currentTransform = swapchainSupport.capabilities.currentTransform;
    uint32_t imageCount = swapchainSupport.capabilities.minImageCount + 1;
    if (swapchainSupport.capabilities.maxImageCount > 0 && imageCount > swapchainSupport.capabilities.maxImageCount) {
        imageCount = swapchainSupport.capabilities.maxImageCount;
    }
    this->queryResults.imageCount = imageCount;
    this->imageFormat = queryResults.surfaceFormat.format;
}

void Swapchain::buildSwapchain(VulkanContext* ctx) {
    if (this->swapchain != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(ctx->device);
        this->destroy_back(ctx);
    }

    this->querySwapchain(ctx);

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.pNext = nullptr;
    createInfo.surface = ctx->surface;
    createInfo.minImageCount = queryResults.imageCount;
    createInfo.imageFormat = queryResults.surfaceFormat.format;
    createInfo.imageColorSpace = queryResults.surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = this->preferences.usageFlags;

    QueueFamilyIndices indices = ctx->findQueueFamilies(ctx->physicalDevice);
    uint32_t queueFamilyIndices[] = {
        indices.graphicsComputeFamily.value(),
        indices.presentFamily.value()};
    if (indices.graphicsComputeFamily != indices.presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = nullptr;
    }
    createInfo.preTransform = queryResults.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = queryResults.presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;
    if (vkCreateSwapchainKHR(ctx->device, &createInfo, nullptr, &swapchain) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create swapchain.");
    }
    vkGetSwapchainImagesKHR(ctx->device, swapchain, &queryResults.imageCount, nullptr);
    std::vector<VkImage> vkImages;
    vkImages.resize(queryResults.imageCount);
    images.images.resize(queryResults.imageCount);
    vkGetSwapchainImagesKHR(ctx->device, swapchain, &queryResults.imageCount, vkImages.data());
    
    // Initialize images
    for (size_t i = 0; i < images.images.size(); i++) {
        images.images[i] = new VulkanImage();
        images.images[i]->imageView = createImageView(ctx, vkImages[i], imageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
        images.images[i]->image = vkImages[i];
        images.images[i]->doNotDestroyImage = true;
        images.images[i]->imageFormat = imageFormat;
        images.images[i]->chonklet.chonkus = nullptr;
        transitionImageLayout(ctx, images.images[i], ILTM_UNDEFINED_TO_TRANSFER_DST);
        VkCommandBuffer buffer = beginSingleTimeCommands(ctx, ctx->commandPools.transientGraphicsCompute);
        VkClearColorValue clearVal = {.float32 = {1.0f, 0.5f, 1.0f, 1.0f}}; // Light magenta
        VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vkCmdClearColorImage(buffer, images.images[i]->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearVal, 1, &range);
        endSingleTimeCommands(ctx, buffer, ctx->queues.graphics, ctx->commandPools.transientGraphicsCompute);
        transitionImageLayout(ctx, images.images[i], ILTM_UNDEFINED_TO_PRESENT_SRC_KHR);
    }
    images.extent = extent;
}

void Swapchain::destroy_back(VulkanContext* ctx) {
    for (VulkanImage* img : this->images.images) {
        img->destroy(ctx);
        delete img;
    }
    this->images.images.clear();
    vkDestroySwapchainKHR(ctx->device, this->swapchain, nullptr);
    this->swapchain = VK_NULL_HANDLE;
}

void Swapchain::updateViewportScissor(VkCommandBuffer commandBuffer) {
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)extent.width;
    viewport.height = (float)extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = extent;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
}

SwapchainFrame Swapchain::getFrameInternal(VulkanContext* ctx, VkSemaphore semaphore) {
    SwapchainFrame frame;
    frame.result = vkAcquireNextImageKHR(ctx->device, swapchain, UINT64_MAX, semaphore, VK_NULL_HANDLE, &frame.index);
    return frame;
}

SwapchainFrame Swapchain::getFrame(VulkanContext* ctx, VkSemaphore semaphore) {
    SwapchainFrame frame = this->getFrameInternal(ctx, semaphore);
    if (frame.result == VK_ERROR_OUT_OF_DATE_KHR) {
        submitDebugMessage(ctx, "Failed to acquire swapchain frame index (VK_ERROR_OUT_OF_DATE_KHR).", MSG_SEV_INFO, MSG_TYPE_GENERAL);
        return {nullptr, UINT32_MAX, frame.result};
    }
    if (frame.result != VK_SUCCESS && frame.result != VK_SUBOPTIMAL_KHR) {
        printf("(long)VkResult = %ld\n", (long)frame.result);
        throw std::runtime_error("Unexpected swapchain acquisition failure from vkAcquireNextImageKHR.");
    }
    frame.image = this->images.images[frame.index];
    return frame;
}