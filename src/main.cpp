#include "lepton2/vulkancore/VulkanUtils.h"
#include "lepton2/vulkancore/VulkanMemory.h"
#include "lepton2/vulkancore/VulkanContext.h"
#include "lepton2/vulkancore/RenderState.h"
#include "lepton2/vulkancore/Pipelines.h"
#include "lepton2/vulkancore/ObjectData.h"

using namespace lepton2::vulkancore;

// TODO: Investigate push_back and see if we can switch to static sizing in some places (namely RenderState et al.)

VulkanContext* ctx;

std::vector<Vertex> vertices = {
    {{ -0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
    {{ +0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
    {{ +0.5f, +0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
    {{ -0.5f, +0.5f, 0.0f}, {0.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}
};

std::vector<uint32_t> indices = {
    1, 0, 3, 1, 3, 2
};

HostObjectData hostData = { vertices, indices };

VkCommandBuffer commandBuffer;

int main() {
    if (!glfwInit()) {
        throw std::runtime_error("Failed GLFW initialization.");
    }
    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(800, 600, "Vulkan main", nullptr, nullptr);
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Vulkan test of some sort";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "None yet";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;
    ctx = new VulkanContext(true, true, appInfo, window);

    RenderGraph renderGraph(ctx);
    RenderGraphNode* node = renderGraph.getTerminatingNode();
    // Graph ends here, nothing to attach

    RenderState* renderState = renderGraph.buildRenderState();
    ctx->swapChain.buildSwapChain(renderState);

    PipelineInfo pipelineInfo("shader", node, {}, VK_SAMPLE_COUNT_1_BIT, VK_FALSE, {}, VK_POLYGON_MODE_FILL, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_CULL_MODE_NONE);
    renderState->addPipeline("shader", pipelineInfo);

    DeviceObjectData devData(ctx, hostData);

    VkSemaphore imageAvailableSemaphore = createGenericSemaphore(ctx);
    VkSemaphore renderFinishedSemaphore = createGenericSemaphore(ctx);
    VkFence inFlightFence = createGenericFence(ctx, true);

    {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = ctx->vk_command_pools.normalGraphics;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        if (vkAllocateCommandBuffers(ctx->device, &allocInfo, &commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate command buffers.");
        }

        while (!glfwWindowShouldClose(ctx->window)) {
            glfwPollEvents();
            vkWaitForFences(ctx->device, 1, &inFlightFence, VK_TRUE, UINT64_MAX);
            vkResetFences(ctx->device, 1, &inFlightFence);
            SwapChainFrame swapChainFrame = ctx->swapChain.getFrame(imageAvailableSemaphore);
            vkResetCommandBuffer(commandBuffer, 0);
            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = 0;
            beginInfo.pInheritanceInfo = nullptr;

            if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
                throw std::runtime_error("Failed to begin recording command buffer.");
            }

            renderState->bind(commandBuffer, swapChainFrame);
            renderState->getPipeline("shader")->bind(commandBuffer);

            devData.bind(commandBuffer, 0);

            ctx->swapChain.updateViewportScissor(commandBuffer);

            vkCmdDrawIndexed(commandBuffer, (uint32_t)indices.size(), 1, 0, 0, 0);
            vkCmdEndRenderPass(commandBuffer);

            if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
                throw std::runtime_error("Failed to record command buffer.");
            }

            VkSubmitInfo submitInfo{};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT };
            submitInfo.waitSemaphoreCount = 1;
            submitInfo.pWaitSemaphores = &imageAvailableSemaphore;
            submitInfo.pWaitDstStageMask = waitStages;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &commandBuffer;
            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores = &renderFinishedSemaphore;
            if (vkQueueSubmit(ctx->vk_queues.graphics, 1, &submitInfo, inFlightFence) != VK_SUCCESS) {
                throw std::runtime_error("Failed to submit command buffer");
            }
            VkPresentInfoKHR presentInfo{};
            presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            presentInfo.waitSemaphoreCount = 1;
            presentInfo.pWaitSemaphores = &renderFinishedSemaphore;

            presentInfo.swapchainCount = 1;
            presentInfo.pSwapchains = &ctx->swapChain.swapChain;
            presentInfo.pImageIndices = &swapChainFrame.index;
            presentInfo.pResults = nullptr;
            VkResult result = vkQueuePresentKHR(ctx->vk_queues.present, &presentInfo);
            if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
                ctx->swapChain.rebuildSwapChain();
            } else if (result != VK_SUCCESS) {
                throw std::runtime_error("Failed to present swap chain image.");
            }
        }
        vkDeviceWaitIdle(ctx->device);
    }
    vkDestroySemaphore(ctx->device, imageAvailableSemaphore, nullptr);
    vkDestroySemaphore(ctx->device, renderFinishedSemaphore, nullptr);
    vkDestroyFence(ctx->device, inFlightFence, nullptr);
    devData.destroy(ctx);
    renderState->destroy(ctx);
    delete renderState;
    node->destroy(ctx);
    delete node;
    renderGraph.destroy(ctx);
    ctx->destroy(ctx);
    delete ctx;
    return 0;
}