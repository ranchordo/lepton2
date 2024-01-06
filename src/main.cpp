#include "lepton2/vulkancore/ObjectData.h"
#include "lepton2/vulkancore/Pipelines.h"
#include "lepton2/vulkancore/RenderState.h"
#include "lepton2/vulkancore/VulkanContext.h"
#include "lepton2/vulkancore/VulkanMemory.h"
#include "lepton2/vulkancore/VulkanUtils.h"

using namespace lepton2::vulkancore;

// TODO: Investigate push_back and see if we can switch to static sizing in some places (namely RenderState et al.)
// TODO: Investigate persistent mapping
// TODO: See if it works with unicode paths

VulkanContext* ctx;

std::vector<Vertex> vertices = {
    { { -0.5f, -0.5f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f } },
    { { +0.5f, -0.5f, 0.0f }, { 1.0f, 0.0f, 1.0f }, { 1.0f, 0.0f } },
    { { +0.5f, +0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 1.0f } },
    { { -0.5f, +0.5f, 0.0f }, { 0.0f, 1.0f, 1.0f }, { 0.0f, 1.0f } }
};

std::vector<uint32_t> indices = {
    3, 0, 1, 2, 3, 1
};

HostObjectData hostData = { vertices, indices };

VkCommandBuffer commandBuffer;

struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
} ubo;

int main(int argc, char** argv) {
#ifdef DEBUG_ENV
#pragma message "Building in debug mode."
    printf("Launching main in test mode...\n\n");
#endif
    if (!glfwInit()) {
        throw std::runtime_error("Failed GLFW initialization.");
    }
    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(800, 600, "Vulkan main", nullptr, nullptr);
    VkApplicationInfo appInfo {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Vulkan test of some sort";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "None yet";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;
#ifdef DEBUG_ENV
    ctx = new VulkanContext(true, true, appInfo, window);
#else
    ctx = new VulkanContext(false, false, appInfo, window);
#endif

    // Relative shader loading
    std::filesystem::path shader_location_path = getExecutableLocation(argv[0], false).append("shaders");
    const char* shader_location = shader_location_path.c_str();
    char* new_shader_location = (char*)malloc(strlen(shader_location) + 1);
    strcpy(new_shader_location, shader_location);
    shaders_spirv_load_dir = new_shader_location;

    RenderGraph renderGraph(ctx);
    RenderGraphNode* node = renderGraph.getTerminatingNode();
    // Graph ends here, nothing to attach

    RenderState* renderState = renderGraph.buildRenderState();
    ctx->swapChain.buildSwapChain(renderState);

    DescriptorSetArray* dsa = new DescriptorSetArray();
    dsa->addNewBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 1);
    dsa->buildDescriptorSetLayout(ctx);
    ctx->descriptorPoolManager.allocateDescriptorSets(ctx, dsa, 1);

    PipelineInfo pipelineInfo("shader", node, dsa->getSingularLayout());
    renderState->addPipeline("shader", pipelineInfo);

    DeviceObjectData devData(ctx, hostData);

    VkSemaphore imageAvailableSemaphore = createGenericSemaphore(ctx);
    VkSemaphore renderFinishedSemaphore = createGenericSemaphore(ctx);
    VkFence inFlightFence = createGenericFence(ctx, true);

    VulkanBuffer uniformBuffer;
    VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    createBuffer(ctx, sizeof(UniformBufferObject), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, properties, &uniformBuffer);

    VkDescriptorBufferInfo bufferInfo {};
    bufferInfo.buffer = uniformBuffer.buffer;
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(UniformBufferObject);
    VkWriteDescriptorSet descriptorWrites {};
    descriptorWrites.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites.dstSet = dsa->descriptorSets[0];
    descriptorWrites.dstBinding = 0;
    descriptorWrites.dstArrayElement = 0;
    descriptorWrites.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites.descriptorCount = 1;
    descriptorWrites.pBufferInfo = &bufferInfo;

    vkUpdateDescriptorSets(ctx->device, 1, &descriptorWrites, 0, nullptr);

    {
        VkCommandBufferAllocateInfo allocInfo {};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = ctx->vk_command_pools.normalGraphics;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        if (vkAllocateCommandBuffers(ctx->device, &allocInfo, &commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate command buffers.");
        }
        uint32_t frame_count = 0;
        auto start_time_point = startTiming();
        while (!glfwWindowShouldClose(ctx->window)) {
            auto time_point = startTiming();
            glfwPollEvents();
            vkWaitForFences(ctx->device, 1, &inFlightFence, VK_TRUE, UINT64_MAX);
            vkResetFences(ctx->device, 1, &inFlightFence);
            SwapChainFrame swapChainFrame = ctx->swapChain.getFrame(imageAvailableSemaphore);
            vkResetCommandBuffer(commandBuffer, 0);
            VkCommandBufferBeginInfo beginInfo {};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = 0;
            beginInfo.pInheritanceInfo = nullptr;

            if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
                throw std::runtime_error("Failed to begin recording command buffer.");
            }

            renderState->bind(commandBuffer, swapChainFrame);
            renderState->getPipeline("shader")->bind(commandBuffer);
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, renderState->getPipeline("shader")->getPipelineLayout(), 0, 1, dsa->descriptorSets.data(), 0, nullptr);

            devData.bind(commandBuffer, 0);

            ctx->swapChain.updateViewportScissor(commandBuffer);

            vkCmdDrawIndexed(commandBuffer, (uint32_t)indices.size(), 1, 0, 0, 0);
            vkCmdEndRenderPass(commandBuffer);

            if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
                throw std::runtime_error("Failed to record command buffer.");
            }

            UniformBufferObject ubo {};
            ubo.model = glm::rotate(glm::mat4(1.0f), (float)getElapsedSeconds(start_time_point) * glm::radians(90.0f), glm::vec3(0, 0, 1));
            ubo.view = glm::lookAt(glm::vec3(1, 1, 1), glm::vec3(0, 0, 0), glm::vec3(0, 0, 1));
            ubo.proj = glm::perspective(glm::radians(45.0f), ctx->swapChain.swapChainExtent.width / (float)ctx->swapChain.swapChainExtent.height, 0.1f, 10.0f);
            ubo.proj[1][1] *= -1;
            void* data = uniformBuffer.chonklet.mapMemory(ctx, 0);
            memcpy(data, &ubo, sizeof(UniformBufferObject));
            uniformBuffer.chonklet.unmapMemory(ctx);

            VkSubmitInfo submitInfo {};
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
            VkPresentInfoKHR presentInfo {};
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
            if (frame_count % 1000 == 0) {
                double fp = getElapsedSeconds(time_point);
                printf("Interval (μs): %lf\n", fp * 1000000);
                printf("Framerate (fps): %lf\n", 1 / fp);
            }
            frame_count++;
        }
        vkDeviceWaitIdle(ctx->device);
    }
    uniformBuffer.destroy(ctx);
    dsa->destroy(ctx);
    delete dsa;
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