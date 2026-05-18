#include "VulkanLoop.h"

#include "VulkanContext.h"

using namespace lepton2::vulkancore;
using namespace lepton2::vulkancore::loopmodifiers;

void InFlightResources::create(VulkanContext* ctx) {
    this->imageAvailableSemaphore = createGenericSemaphore(ctx);
    this->renderFinishedSemaphore = createGenericSemaphore(ctx);
    this->inFlightFence = createGenericFence(ctx, true);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.pNext = nullptr;
    allocInfo.commandPool = ctx->commandPools.normalGraphicsCompute;
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

    VkCommandPool owner = ctx->commandPools.normalGraphicsCompute;
    vkFreeCommandBuffers(ctx->device, owner, 1, &commandBuffer);
}

// Loop modifiers:
void SimpleRenderPass::preRender(VulkanContext* ctx, uint32_t frameIndex) {
    this->renderPass->preRenderAll(ctx, frameIndex);
}
void SimpleRenderPass::renderCmd(VkCommandBuffer buffer, uint32_t frameIndex, uint32_t swapchainIndex) {
    if (this->renderPass->getNumFramebuffers() == 0) {
        throw std::runtime_error("Trying to render a RenderPass with no framebuffers. Please initialize them.");
    }
    uint32_t fbIdx = swapchainIndexing ? swapchainIndex : frameIndex;
    this->renderPass->begin(buffer, fbIdx);
    this->renderPass->renderAll(buffer, frameIndex, fbIdx);
    this->renderPass->end(buffer);
}
void SimpleRenderPass::onSwapchainRebuild(VulkanContext* ctx) {
    this->renderPass->destroyFramebuffers(ctx);
    this->renderPass->generateFramebuffers(ctx, &targetContainer->images, targetContainer->extent);
}

VulkanLoop::VulkanLoop(uint32_t inFlightFrames) {
    this->inFlightResources.resize(inFlightFrames);
}

void VulkanLoop::initialize(VulkanContext* ctx) {
    for (uint32_t i = 0; i < inFlightResources.size(); i++) {
        this->inFlightResources[i].create(ctx);
    }
}

bool VulkanLoop::shouldLoopTerminate(VulkanContext* ctx) {
    return glfwWindowShouldClose(ctx->window);
}

void VulkanLoop::process(VulkanContext* ctx) {
    glfwPollEvents();
    InFlightResources* ifr = &this->inFlightResources[inFlightFrameCount];
    vkWaitForFences(ctx->device, 1, &ifr->inFlightFence, VK_TRUE, UINT64_MAX);
    vkResetFences(ctx->device, 1, &ifr->inFlightFence);
    SwapchainFrame swapchainFrame = ctx->swapchain.getFrame(ctx, ifr->imageAvailableSemaphore);
    if (swapchainFrame.index == UINT32_MAX) {
        // Swapchain failure, rebuild
        ctx->swapchain.buildSwapchain(ctx);
        for (VulkanLoopModifier* mod : this->loopModifiers) {
            mod->onSwapchainRebuild(ctx);
        }
        if (vkQueueSubmit(ctx->queues.graphics, 0, nullptr, ifr->inFlightFence) != VK_SUCCESS) {
            throw std::runtime_error("Failed to submit dummy swapchain rebuild queue.\n");
        }
        return;
    }

    // Pre-render phase
    for (VulkanLoopModifier* loopmod : this->loopModifiers) {
        loopmod->preRender(ctx, inFlightFrameCount);
    }

    // RenderCmd phase
    VkCommandBuffer commandBuffer = ifr->commandBuffer;
    vkResetCommandBuffer(commandBuffer, 0);
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.pNext = nullptr;
    beginInfo.flags = 0;
    beginInfo.pInheritanceInfo = nullptr;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to begin recording command buffer.");
    }

    ctx->swapchain.updateViewportScissor(commandBuffer);

    for (VulkanLoopModifier* loopmod : this->loopModifiers) {
        loopmod->renderCmd(commandBuffer, inFlightFrameCount, swapchainFrame.index);
    }
    for (VulkanLoopModifier* loopmod : this->loopModifiers) {
        loopmod->postRenderCmd(commandBuffer, inFlightFrameCount, swapchainFrame.index);
    }

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to record command buffer.");
    }

    // Submit/present phase
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pNext = nullptr;
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &ifr->imageAvailableSemaphore;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &ifr->renderFinishedSemaphore;
    if (vkQueueSubmit(ctx->queues.graphics, 1, &submitInfo, ifr->inFlightFence) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit command buffer.");
    }
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = nullptr;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &ifr->renderFinishedSemaphore;

    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = ctx->swapchain.getSwapchain();
    presentInfo.pImageIndices = &swapchainFrame.index;
    presentInfo.pResults = nullptr;
    VkResult result = vkQueuePresentKHR(ctx->queues.present, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        ctx->swapchain.buildSwapchain(ctx);
        for (VulkanLoopModifier* mod : this->loopModifiers) {
            mod->onSwapchainRebuild(ctx);
        }
        return;
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to present swap chain image.");
    }
    inFlightFrameCount++;
    if (inFlightFrameCount >= inFlightResources.size()) {
        inFlightFrameCount %= inFlightResources.size();
    }
}

void VulkanLoop::terminateLoop(VulkanContext* ctx) {
    vkDeviceWaitIdle(ctx->device);
}

void VulkanLoop::destroy_back(VulkanContext* ctx) {
    for (uint32_t i = 0; i < inFlightResources.size(); i++) {
        this->inFlightResources[i].destroy(ctx);
    }
}