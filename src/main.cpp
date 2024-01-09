#include "lepton2/vulkancore/GraphicalEntity.h"
#include "lepton2/vulkancore/GraphicalPresets.h"
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
    {{-0.5f, -0.5f, +0.2f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
    {{+0.5f, -0.5f, +0.2f}, {1.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
    {{+0.5f, +0.5f, +0.2f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
    {{-0.5f, +0.5f, +0.2f}, {0.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}};

std::vector<Vertex> morevertices = {
    {{-0.5f, -0.5f, -0.2f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
    {{+0.5f, -0.5f, -0.2f}, {1.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
    {{+0.5f, +0.5f, -0.2f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
    {{-0.5f, +0.5f, -0.2f}, {0.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}};

std::vector<uint32_t> indices = {3, 0, 1, 2, 3, 1};

std::vector<Vertex> vertices1 = {
    {{-1.0f, -1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
    {{+1.0f, -1.0f, 0.0f}, {1.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
    {{+1.0f, +1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
    {{-1.0f, +1.0f, 0.0f}, {0.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}};

std::vector<uint32_t> indices1 = {1, 0, 3, 1, 3, 2};

HostObjectData hostData1 = {vertices1, indices1};

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
    VkApplicationInfo appInfo{};
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

    RenderGraphNode* node1 = renderGraph.buildNewNode();
    RenderTargetImageCreationInfo rticInfo{};
    rticInfo.use_swapchain = false;
    rticInfo.imageTiling = VK_IMAGE_TILING_OPTIMAL;
    rticInfo.memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    rticInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    rticInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
    rticInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    rticInfo.aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
    node1->addColorAttachment(rticInfo, true);
    node->connectFromNode(node1, 0, 0);

    RenderState* renderState = renderGraph.buildRenderState();
    ctx->swapChain.buildSwapChain(renderState);

    class : public GraphicalEntity {  // Hehe anonymous classes go brrrrr
       public:
        void _create(VulkanContext* ctx, const HostObjectData& hostData, float rotationSpeed) {
            this->objectData = new DeviceObjectData(ctx, hostData);
            this->rotationSpeed = rotationSpeed;
        }
        PipelineInfo getPipelineRequirements() override {
            DescriptorInfo descInfo;
            descInfo.bufferSize = sizeof(UniformBufferObject);
            descInfo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            DescriptorSetLayoutInfo dsli;
            dsli.addNewBinding(descInfo, VK_SHADER_STAGE_VERTEX_BIT, 1);
            PipelineInfo req("shader", dsli, nullptr, VK_SAMPLE_COUNT_1_BIT, VK_FALSE, {}, VK_POLYGON_MODE_FILL, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_CULL_MODE_NONE);
            return req;
        }
        void postInit(RenderGraphNode* node, RenderState* renderState) override {
            this->start_time_point = startTiming();
        }
        void preRender(RenderState* renderState, uint32_t descriptorIndex) override {
            UniformBufferObject ubo{};
            float elapsedSeconds = (float)getElapsedSeconds(start_time_point);
            ubo.model = glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, sin(elapsedSeconds * 4 * this->rotationSpeed) * 0.1));
            ubo.model = glm::rotate(ubo.model, elapsedSeconds * this->rotationSpeed, glm::vec3(0, 0, 1));
            ubo.view = glm::lookAt(glm::vec3(1, 1, 0.5), glm::vec3(0, 0, 0), glm::vec3(0, 0, 1));
            ubo.proj = glm::perspective(glm::radians(45.0f), ctx->swapChain.swapChainExtent.width / (float)ctx->swapChain.swapChainExtent.height, 0.1f, 10.0f);
            ubo.proj[1][1] *= -1;
            VulkanBuffer* uniformBuffer = ((descriptortypes::UniformBufferDescriptor*)(dsa->singleDescriptorSets[descriptorIndex].instances[0]))->uniformBuffer;
            void* data = uniformBuffer->chonklet.mapMemory(ctx, 0);
            memcpy(data, &ubo, sizeof(UniformBufferObject));
            uniformBuffer->chonklet.unmapMemory(ctx);
        }
        void destroy_back(VulkanContext* ctx) override {
            this->objectData->destroy(ctx);
            delete this->objectData;
            this->destroyEntityResources(ctx);
        }

       private:
        lepton2_time_point start_time_point;
        float rotationSpeed;
    } rectangleEntity;

    rectangleEntity._create(ctx, { vertices, indices }, glm::radians(90.0f));
    rectangleEntity.initialize(node1, renderState, ctx->swapChain.swapChainImages.size());

    decltype(rectangleEntity) rectangleEntity1;
    rectangleEntity1._create(ctx, { morevertices, indices }, glm::radians(-90.0f));
    rectangleEntity1.initialize(node1, renderState, ctx->swapChain.swapChainImages.size());

    StaticScreenEntity screenEntity(ctx, "prshader", &node1->getColorAttachments()->at(0));
    screenEntity.initialize(node, renderState, ctx->swapChain.swapChainImages.size());

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
        uint32_t frame_count = 0;
        auto start_time_point = startTiming();
        while (!glfwWindowShouldClose(ctx->window)) {
            auto time_point = startTiming();
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

            ctx->swapChain.updateViewportScissor(commandBuffer);

            renderState->begin(commandBuffer, swapChainFrame);
            renderState->renderAll(commandBuffer, swapChainFrame);
            renderState->end(commandBuffer);

            if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
                throw std::runtime_error("Failed to record command buffer.");
            }

            VkSubmitInfo submitInfo{};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT};
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
                continue;
            } else if (result != VK_SUCCESS) {
                printf("%d\n", (int)result);
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
    rectangleEntity.destroy(ctx);
    rectangleEntity1.destroy(ctx);
    vkDestroySemaphore(ctx->device, imageAvailableSemaphore, nullptr);
    vkDestroySemaphore(ctx->device, renderFinishedSemaphore, nullptr);
    vkDestroyFence(ctx->device, inFlightFence, nullptr);
    renderState->destroy(ctx);
    delete renderState;
    // node1->destroy(ctx);
    // delete node1;
    node->destroy(ctx);
    delete node;
    renderGraph.destroy(ctx);
    ctx->destroy(ctx);
    delete ctx;
    return 0;
}