#include "VulkanLoop.h"

using namespace lepton2::vulkancore;

void InFlightResources::create(VulkanContext* ctx) {
    this->imageAvailableSemaphore = createGenericSemaphore(ctx);
    this->renderFinishedSemaphore = createGenericSemaphore(ctx);
    this->inFlightFence = createGenericFence(ctx, true);
}

void InFlightResources::destroy_back(VulkanContext* ctx) {
    vkDestroySemaphore(ctx->device, imageAvailableSemaphore, nullptr);
    vkDestroySemaphore(ctx->device, renderFinishedSemaphore, nullptr);
    vkDestroyFence(ctx->device, inFlightFence, nullptr);
}

VulkanLoop::VulkanLoop(RenderPass* renderState, uint32_t numInFlightFrames) {
    this->renderState = renderState;
    this->numInFlightFrames = numInFlightFrames;
    this->inFlightResources.resize(numInFlightFrames);
}

void VulkanLoop::fillCommandBuffers(VulkanContext* ctx) {
    uint32_t target = ctx->swapChain.swapChainImages.size();
    uint32_t prevsize = this->commandBuffers.size();
    if (prevsize < target) {
        uint32_t growth = target - prevsize;
        this->commandBuffers.resize(target);

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = ctx->vk_command_pools.normalGraphics;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = growth;

        if (vkAllocateCommandBuffers(ctx->device, &allocInfo, commandBuffers.data() + prevsize) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate command buffers.");
        }
    }
}

void VulkanLoop::initialize(VulkanContext* ctx) {
    for (uint32_t i = 0; i < numInFlightFrames; i++) {
        this->inFlightResources[i].create(ctx);
    }
    this->fillCommandBuffers(ctx);
    this->forceRecordCommandBuffers(ctx);
}

bool VulkanLoop::shouldLoopTerminate(VulkanContext* ctx) {
    return glfwWindowShouldClose(ctx->window);
}

void VulkanLoop::forceRecordCommandBuffers(VulkanContext* ctx) {
    for (uint32_t scfi = 0; scfi < ctx->swapChain.swapChainImages.size(); scfi++) {
        VkCommandBuffer commandBuffer = commandBuffers[scfi];
        vkResetCommandBuffer(commandBuffer, 0);
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0;
        beginInfo.pInheritanceInfo = nullptr;

        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("Failed to begin recording command buffer.");
        }

        ctx->swapChain.updateViewportScissor(commandBuffer);

        for (VulkanLoopModifier* loopmod : this->loopModifiers) { loopmod->preRender(scfi); }
        renderState->begin(ctx, commandBuffer, scfi);
        renderState->renderAll(commandBuffer, scfi);
        renderState->end(commandBuffer);

        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to record command buffer.");
        }
    }
}

void VulkanLoop::process(VulkanContext* ctx) {
    glfwPollEvents();
    InFlightResources* ifr = &this->inFlightResources[this->inFlightFrameCount];
    vkWaitForFences(ctx->device, 1, &ifr->inFlightFence, VK_TRUE, UINT64_MAX);
    vkResetFences(ctx->device, 1, &ifr->inFlightFence);
    SwapChainFrame swapChainFrame = ctx->swapChain.getFrame(ctx, ifr->imageAvailableSemaphore);
    if (swapChainFrame.index == UINT32_MAX) {
        // Swapchain failure, rebuild
        ctx->swapChain.rebuildSwapChain(ctx);
        fillCommandBuffers(ctx);
        forceRecordCommandBuffers(ctx);
        if (vkQueueSubmit(ctx->vk_queues.graphics, 0, nullptr, ifr->inFlightFence) != VK_SUCCESS) {
            throw std::runtime_error("Failed to submit dummy swapchain rebuild queue.\n");
        }
        return;
    }
    VkCommandBuffer commandBuffer = this->commandBuffers[swapChainFrame.index];

    renderState->preRenderAll(ctx, swapChainFrame.index);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &ifr->imageAvailableSemaphore;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &ifr->renderFinishedSemaphore;
    if (vkQueueSubmit(ctx->vk_queues.graphics, 1, &submitInfo, ifr->inFlightFence) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit command buffer.");
    }
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &ifr->renderFinishedSemaphore;

    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &ctx->swapChain.swapChain;
    presentInfo.pImageIndices = &swapChainFrame.index;
    presentInfo.pResults = nullptr;
    VkResult result = vkQueuePresentKHR(ctx->vk_queues.present, &presentInfo);

    if (result == VK_SUCCESS) { // Explicit swapchain extent check
        VkExtent2D knownSwapChainExtent = ctx->swapChain.swapChainExtent;
        VkExtent2D extent = SwapChain::getCurrentExtent(ctx);
        if (extent.width != knownSwapChainExtent.width || extent.height != knownSwapChainExtent.height) {
            result = VK_SUBOPTIMAL_KHR;
        }
    }

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        ctx->swapChain.rebuildSwapChain(ctx);
        fillCommandBuffers(ctx);
        forceRecordCommandBuffers(ctx);
        return;
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to present swap chain image.");
    }
    this->inFlightFrameCount++;
    if (this->inFlightFrameCount >= this->numInFlightFrames) {
        this->inFlightFrameCount %= this->numInFlightFrames;
    }
}

void VulkanLoop::terminateLoop(VulkanContext* ctx) {
    vkDeviceWaitIdle(ctx->device);
}

void VulkanLoop::destroy_back(VulkanContext* ctx) {
    for (uint32_t i = 0; i < numInFlightFrames; i++) {
        this->inFlightResources[i].destroy(ctx);
    }
    if (commandBuffers.size() > 0) {
        VkCommandPool owner = ctx->vk_command_pools.normalGraphics;
        vkFreeCommandBuffers(ctx->device, owner, commandBuffers.size(), commandBuffers.data());
        commandBuffers.clear();
    }
}