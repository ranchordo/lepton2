#include "SwapChain.h"

#include "Descriptors.h"
#include "RenderState.h"
#include "VulkanContext.h"

using namespace lepton2::vulkancore;

VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    for (const VkSurfaceFormatKHR& availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }
    return availableFormats[0];  // TODO: Better ranking system.
}

VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
    return VK_PRESENT_MODE_IMMEDIATE_KHR;  // FIXME: Stop this madness!
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
            (uint32_t)height};
        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
        return actualExtent;
    }
}

void SwapChain::querySwapChain() {
    SwapChainSupportDetails swapChainSupport = ctx->querySwapChainSupport(ctx->physicalDevice);
    this->swapChainQueryResults.surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    this->swapChainQueryResults.presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    this->swapChainExtent = chooseSwapExtent(ctx, swapChainSupport.capabilities);
    this->swapChainQueryResults.currentTransform = swapChainSupport.capabilities.currentTransform;
    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }
    this->swapChainQueryResults.imageCount = imageCount;
    this->swapChainImageFormat = swapChainQueryResults.surfaceFormat.format;
}

void SwapChain::buildSwapChain(RenderState* renderState) {
    this->renderState = renderState;

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = ctx->surface;
    createInfo.minImageCount = swapChainQueryResults.imageCount;
    createInfo.imageFormat = swapChainQueryResults.surfaceFormat.format;
    createInfo.imageColorSpace = swapChainQueryResults.surfaceFormat.colorSpace;
    createInfo.imageExtent = swapChainExtent;
    createInfo.imageArrayLayers = 1;  // For stereoscopy, maybe 2?
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    QueueFamilyIndices indices = ctx->findQueueFamilies(ctx->physicalDevice);
    uint32_t queueFamilyIndices[] = {
        indices.graphicsFamily.value(),
        indices.presentFamily.value()};
    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = nullptr;
    }
    createInfo.preTransform = swapChainQueryResults.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;  // TODO: Figure out proper alpha compositing.
    createInfo.presentMode = swapChainQueryResults.presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;
    if (vkCreateSwapchainKHR(ctx->device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create swapchain.");
    }
    vkGetSwapchainImagesKHR(ctx->device, swapChain, &swapChainQueryResults.imageCount, nullptr);
    swapChainImages.resize(swapChainQueryResults.imageCount);
    std::vector<VkImage> swapChainVkImages;
    swapChainVkImages.resize(swapChainQueryResults.imageCount);
    swapChainImages.resize(swapChainQueryResults.imageCount);
    vkGetSwapchainImagesKHR(ctx->device, swapChain, &swapChainQueryResults.imageCount, swapChainVkImages.data());
    // Create image views
    for (size_t i = 0; i < swapChainImages.size(); i++) {
        swapChainImages[i] = new VulkanImage();
        swapChainImages[i]->imageView = createImageView(ctx, swapChainVkImages[i], swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
        swapChainImages[i]->image = swapChainVkImages[i];
        swapChainImages[i]->do_not_destroy_image = true;
        swapChainImages[i]->imageFormat = swapChainImageFormat;
        swapChainImages[i]->chonklet.chonkus = nullptr;
    }

    this->swapChainFramebuffers.resize(this->swapChainImages.size());
    for (uint32_t i = 0; i < this->swapChainImages.size(); i++) {
        Framebuffer* nfb = new Framebuffer();
        for (RenderTargetImageCreationInfo rticInfo : renderState->rticInfos) {
            if (rticInfo.use_swapchain) {
                nfb->addImage(swapChainImages[i]);
            } else {
                VulkanImage* rtImage = new VulkanImage();
                createRenderTarget(ctx, swapChainExtent, &rticInfo, rtImage);
                nfb->addImage(rtImage);
                this->renderTargetImages.push_back(rtImage);
                if (rticInfo.creationTracker != nullptr) {
                    rticInfo.creationTracker->push_back(rtImage);
                    this->swapChainCreationTrackers.push_back(rticInfo.creationTracker);
                }
            }
        }
        nfb->buildFramebuffer(ctx, renderState->renderPass, swapChainExtent.width, swapChainExtent.height);
        this->swapChainFramebuffers[i] = nfb;
    }

    // Filter and trigger descriptor updates:
    std::unordered_set<DescriptorSetUpdateInfo*> filteredDescriptorUpdates;
    for (DescriptorSetUpdateInfo* updateInfo : this->descriptorUpdates) {
        if (updateInfo->alive) {
            filteredDescriptorUpdates.insert(updateInfo);
        } else {
            delete updateInfo;
        }
    }
    this->descriptorUpdates = filteredDescriptorUpdates;
    std::vector<VkDescriptorImageInfo> imageInfos;
    std::vector<VkDescriptorBufferInfo> bufferInfos;
    std::vector<VkWriteDescriptorSet> writeInfos;
    for (DescriptorSetUpdateInfo* updateInfo : this->descriptorUpdates) {
        DescriptorWriteInfoContainer dwic = updateInfo->instance->getWriteInfo(this->ctx, updateInfo->dstSet, updateInfo->dstBinding);
        imageInfos.push_back(dwic.imageInfo);
        bufferInfos.push_back(dwic.bufferInfo);
        writeInfos.push_back(dwic.writeInfo);
    }
    for (uint32_t i = 0; i < writeInfos.size(); i++) {
        writeInfos[i].pImageInfo = &imageInfos[i]; // Must keep alive
        writeInfos[i].pBufferInfo = &bufferInfos[i]; // Must keep alive
    }
    vkUpdateDescriptorSets(this->ctx->device, writeInfos.size(), writeInfos.data(), 0, nullptr);
}

void SwapChain::destroy_back(VulkanContext* ctx) {
    for (VulkanImage* img : this->swapChainImages) {
        img->destroy(ctx);
        delete img;
    }
    this->swapChainImages.clear();
    for (std::vector<VulkanImage*>* creationTracker : this->swapChainCreationTrackers) {
        creationTracker->clear();
    }
    this->swapChainCreationTrackers.clear();
    // Free dead descriptor updates:
    std::unordered_set<DescriptorSetUpdateInfo*> filteredDescriptorUpdates;
    for (DescriptorSetUpdateInfo* updateInfo : this->descriptorUpdates) {
        if (updateInfo->alive) {
            filteredDescriptorUpdates.insert(updateInfo);
        } else {
            delete updateInfo;
        }
    }
    this->descriptorUpdates = filteredDescriptorUpdates;
    for (VulkanImage* img : this->renderTargetImages) {
        img->destroy(ctx);
        delete img;
    }
    this->renderTargetImages.clear();
    for (Framebuffer* fb : this->swapChainFramebuffers) {
        fb->destroy(ctx);
        delete fb;
    }
    this->swapChainFramebuffers.clear();
    vkDestroySwapchainKHR(ctx->device, this->swapChain, nullptr);
}

void SwapChain::rebuildSwapChain() {
    if (this->renderState == nullptr) {
        throw std::runtime_error("Can't rebuild swap chain without building it first.");
    }
    int width = 0, height = 0;
    glfwGetFramebufferSize(ctx->window, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(ctx->window, &width, &height);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(ctx->device);
    this->destroy_back(this->ctx);
    this->querySwapChain();
    this->buildSwapChain(this->renderState);
}

void SwapChain::updateViewportScissor(VkCommandBuffer commandBuffer) {
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)swapChainExtent.width;
    viewport.height = (float)swapChainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapChainExtent;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
}

SwapChainFrame SwapChain::getFrameInternal(VkSemaphore semaphore) {
    SwapChainFrame frame;
    frame.result = vkAcquireNextImageKHR(ctx->device, swapChain, UINT64_MAX, semaphore, VK_NULL_HANDLE, &frame.index);
    return frame;
}

SwapChainFrame SwapChain::getFrame(VkSemaphore semaphore) {
    SwapChainFrame frame = this->getFrameInternal(semaphore);
    if (frame.result == VK_ERROR_OUT_OF_DATE_KHR) {
        this->rebuildSwapChain();
        frame = this->getFrameInternal(semaphore);
    }
    if (frame.result != VK_SUCCESS && frame.result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Failed to grab swapchain index.");
    }
    frame.framebuffer = this->swapChainFramebuffers[frame.index];
    frame.image = this->swapChainImages[frame.index];
    return frame;
}