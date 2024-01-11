#include "VulkanLoop.h"

using namespace lepton2::vulkancore;

void InFlightResources::create(VulkanContext* ctx) {
    this->imageAvailableSemaphore = createGenericSemaphore(ctx);
    this->renderFinishedSemaphore = createGenericSemaphore(ctx);
    this->inFlightFence = createGenericFence(ctx, true);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = ctx->vk_command_pools.normalGraphics;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(ctx->device, &allocInfo, &commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffers.");
    }
}

void InFlightResources::destroy_back(VulkanContext* ctx) {
    vkDestroySemaphore(ctx->device, imageAvailableSemaphore, nullptr);
    vkDestroySemaphore(ctx->device, renderFinishedSemaphore, nullptr);
    vkDestroyFence(ctx->device, inFlightFence, nullptr);

    VkCommandPool owner = ctx->vk_command_pools.normalGraphics;
    vkFreeCommandBuffers(ctx->device, owner, 1, &commandBuffer);
}

VulkanLoop::VulkanLoop(RenderState* renderState, uint32_t numInFlightFrames) {
    this->renderState = renderState;
    this->numInFlightFrames = numInFlightFrames;
    this->inFlightResources.resize(numInFlightFrames);
}

void VulkanLoop::initialize() {
    for (uint32_t i = 0; i < numInFlightFrames; i++) {
        this->inFlightResources[i].create(renderState->ctx);
    }
}

bool VulkanLoop::shouldLoopTerminate() {
    return glfwWindowShouldClose(renderState->ctx->window);
}

void VulkanLoop::process() {
    glfwPollEvents();
    InFlightResources* ifr = &this->inFlightResources[this->inFlightFrameCount];
    VulkanContext* ctx = this->renderState->ctx;
    vkWaitForFences(ctx->device, 1, &ifr->inFlightFence, VK_TRUE, UINT64_MAX);
    vkResetFences(ctx->device, 1, &ifr->inFlightFence);
    SwapChainFrame swapChainFrame = ctx->swapChain.getFrame(ifr->imageAvailableSemaphore);
    vkResetCommandBuffer(ifr->commandBuffer, 0);
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0;
    beginInfo.pInheritanceInfo = nullptr;

    if (vkBeginCommandBuffer(ifr->commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to begin recording command buffer.");
    }

    ctx->swapChain.updateViewportScissor(ifr->commandBuffer);

    renderState->begin(ifr->commandBuffer, swapChainFrame);
    renderState->renderAll(ifr->commandBuffer, swapChainFrame);
    renderState->end(ifr->commandBuffer);

    if (vkEndCommandBuffer(ifr->commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to record command buffer.");
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &ifr->imageAvailableSemaphore;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &ifr->commandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &ifr->renderFinishedSemaphore;
    if (vkQueueSubmit(ctx->vk_queues.graphics, 1, &submitInfo, ifr->inFlightFence) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit command buffer");
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
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        ctx->swapChain.rebuildSwapChain();
        return;
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to present swap chain image.");
    }
    this->inFlightFrameCount++;
    if (this->inFlightFrameCount >= this->numInFlightFrames) {
        this->inFlightFrameCount %= this->numInFlightFrames;
    }
}

void VulkanLoop::terminateLoop() {
    vkDeviceWaitIdle(renderState->ctx->device);
}

void VulkanLoop::destroy_back(VulkanContext* ctx) {
    for (uint32_t i = 0; i < numInFlightFrames; i++) {
        this->inFlightResources[i].destroy(renderState->ctx);
    }
}