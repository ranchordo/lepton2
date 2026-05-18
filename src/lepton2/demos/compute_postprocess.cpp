#include "../graphics/GraphicalPresets.h"
#include "../utils/LeptonUtils.h"
#include "../vulkancore/ObjectData.h"
#include "../vulkancore/Pipelines.h"
#include "../vulkancore/RenderPass.h"
#include "../vulkancore/Textures.h"
#include "../vulkancore/VulkanContext.h"
#include "../vulkancore/VulkanLoop.h"
#include "../vulkancore/VulkanMemory.h"
#include "../vulkancore/VulkanUtils.h"

using namespace lepton2::vulkancore;
using namespace lepton2::vulkancore::loopmodifiers;
using namespace lepton2::graphics::graphicalpresets;
using namespace lepton2::graphics;
using namespace lepton2::utils;

static std::vector<SimplePresetVertex> screenVertices = {
    {{-1.0f, -1.0f, 0.0f}, {0.0f, 0.0f}},
    {{+1.0f, -1.0f, 0.0f}, {1.0f, 0.0f}},
    {{+1.0f, +1.0f, 0.0f}, {1.0f, 1.0f}},
    {{-1.0f, +1.0f, 0.0f}, {0.0f, 1.0f}}};

static std::vector<uint32_t> screenIndices = {1, 0, 3, 1, 3, 2};

struct SSBO {
    int test_thing[64];
};

int demo_compute_postprocess(int argc, char** argv) {
    if (!glfwInit()) {
        throw std::runtime_error("Failed GLFW initialization.");
    }
    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(512, 512, "Lepton2: demo_compute_postprocess", nullptr, nullptr);
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pNext = nullptr;
    appInfo.pApplicationName = "Lepton2 Demo";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "lepton2";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;
#ifdef DEBUG_ENV
    VulkanContext* ctx = new VulkanContext(argv[0], true, true, appInfo, window);
#else
    VulkanContext* ctx = new VulkanContext(argv[0], false, false, appInfo, window);
#endif

    ctx->swapchain.setPresentMode(VK_PRESENT_MODE_IMMEDIATE_KHR);  // Just for debugging to avoid limiting framerate
    ctx->swapchain.setSurfaceFormat({VK_FORMAT_B8G8R8A8_UNORM, VK_COLORSPACE_SRGB_NONLINEAR_KHR});
    ctx->swapchain.setUsageFlags(VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

    TerminatingSubpassConfig termConfig = RenderSubpass::getDefaultTerminatingConfig(VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_LAYOUT_GENERAL);
    RenderSubpass* node = new RenderSubpass(ctx, termConfig);

    RenderPass* renderPass = new RenderPass(ctx, {node}, 2);
    renderPass->addLinkedResource(node, true);
    ctx->swapchain.buildSwapchain(ctx);

    ImageArray* renderContainer = new ImageArray();
    renderPass->addLinkedResource(renderContainer, true);

    VkExtent2D targetExtent = ctx->swapchain.getKnownExtent();
    RenderTargetImageCreationInfo rticInfo = defaultColorAttachmentRTIC(termConfig.attachmentDescription.format);
    rticInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT;

    renderContainer->images.resize(renderPass->getResourceMultiplicity());
    for (uint32_t i = 0; i < renderPass->getResourceMultiplicity(); i++) {
        renderContainer->images[i] = new VulkanImage();
        createRenderTarget(ctx, targetExtent, &rticInfo, renderContainer->images[i]);
    }
    renderContainer->extent = targetExtent;

    renderPass->generateFramebuffers(ctx, &renderContainer->images, targetExtent);

    GraphicalConfigurationStore* store = new GraphicalConfigurationStore();
    store->addAllSubpasses(renderPass);
    renderPass->addLinkedResource(store, true);

    VulkanLoop mainLoop(renderPass->getResourceMultiplicity());

    class : public VulkanLoopModifier {
       public:
        void _create(RenderTargetImageCreationInfo rticInfo, ImageArray* renderContainer, VkExtent2D* extent, uint32_t multiplicity) {
            this->rticInfo = rticInfo;
            this->renderContainer = renderContainer;
            this->extent = extent;
            this->multiplicity = multiplicity;
        }
        void onSwapchainRebuild(VulkanContext* ctx) override {
            for (uint32_t i = 0; i < renderContainer->images.size(); i++) {
                renderContainer->images[i]->destroy(ctx);
                delete renderContainer->images[i];
            }
            
            renderContainer->images.resize(multiplicity);
            for (uint32_t i = 0; i < multiplicity; i++) {
                renderContainer->images[i] = new VulkanImage();
                createRenderTarget(ctx, *extent, &rticInfo, renderContainer->images[i]);
            }
            renderContainer->extent = *extent;
        }

       private:
        RenderTargetImageCreationInfo rticInfo;
        ImageArray* renderContainer;
        VkExtent2D* extent;
        uint32_t multiplicity;
    } targetRebuildLoopmod;

    SimpleRenderPass renderPassLoopmod(renderPass, renderContainer, false);

    class : public VulkanLoopModifier {
       public:
        void _create(VulkanContext* ctx, ImageArray* inputContainer, VkExtent2D extent, VkExtent2D localSize) {
            this->extent = extent;
            this->localSize = localSize;
            this->inputContainer = inputContainer;
            this->swapchainContainer = ctx->swapchain.getSwapchainImages();

            DescriptorSetLayoutInfo dsli1;
            DescriptorSetLayoutInfo dsli2;
            {
                DescriptorInfo info;
                info.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                info.shaderStages = VK_SHADER_STAGE_COMPUTE_BIT;
                info.storageImageData.images = inputContainer;
                dsli1.addNewBinding(info);
            }
            {
                DescriptorInfo info;
                info.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                info.shaderStages = VK_SHADER_STAGE_COMPUTE_BIT;
                info.storageImageData.images = swapchainContainer;
                dsli2.addNewBinding(info);
            }

            this->dsa1 = new DescriptorSetArray(dsli1);
            this->addLinkedResource(dsa1, true);
            dsa1->buildDescriptorSetLayout(ctx);
            ctx->descriptorPoolManager.allocateDescriptorSets(ctx, dsa1, inputContainer->images.size());

            this->dsa2 = new DescriptorSetArray(dsli2);
            this->addLinkedResource(dsa2, true);
            dsa2->buildDescriptorSetLayout(ctx);
            ctx->descriptorPoolManager.allocateDescriptorSets(ctx, dsa2, swapchainContainer->images.size());

            std::vector<VkDescriptorSetLayout> dsl;
            dsl.push_back(dsa1->descriptorSetLayout);
            dsl.push_back(dsa2->descriptorSetLayout);

            ComputePipelineInfo pipelineInfo(dsl, "demos/compute_postprocess/postprocess");
            computePipeline = new ComputePipeline(ctx, pipelineInfo);
            this->addLinkedResource(computePipeline, true);
        }

        void renderCmd(VkCommandBuffer buffer, uint32_t frameIndex, uint32_t swapchainIndex) override {
            VkImageMemoryBarrier barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.pNext = nullptr;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = swapchainContainer->images[swapchainIndex]->image;
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = 1;
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
            vkCmdPipelineBarrier(buffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                 VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 0, nullptr, 1, &barrier);
            computePipeline->bind(buffer);
            ComputePipeline::bindDescriptorSet(buffer, computePipeline->getPipelineLayout(), dsa1, frameIndex, 0);
            ComputePipeline::bindDescriptorSet(buffer, computePipeline->getPipelineLayout(), dsa2, swapchainIndex, 1);
            uint32_t workX = (inputContainer->extent.width + localSize.width - 1) / localSize.width;
            uint32_t workY = (inputContainer->extent.height + localSize.height - 1) / localSize.height;
            vkCmdDispatch(buffer, workX, workY, 1);
            barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barrier.dstAccessMask = 0;
            barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            vkCmdPipelineBarrier(buffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                 VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 0, nullptr, 1, &barrier);
        }

        void onSwapchainRebuild(VulkanContext* ctx) override {
            dsa1->updateAllDescriptorSets(ctx);
            dsa2->updateAllDescriptorSets(ctx);
        }

       private:
        ImageArray* inputContainer;
        ImageArray* swapchainContainer;
        DescriptorSetArray* dsa1; // For default resource multiplicity
        DescriptorSetArray* dsa2; // For swapchain multiplicity
        ComputePipeline* computePipeline;
        VkExtent2D extent;
        VkExtent2D localSize;
    } postprocessLoopmod;

    targetRebuildLoopmod._create(rticInfo, renderContainer, &ctx->swapchain.getSwapchainImages()->extent, renderPass->getResourceMultiplicity());
    postprocessLoopmod._create(ctx, renderContainer, targetExtent, {16, 16});
    mainLoop.loopModifiers.push_back(&targetRebuildLoopmod);
    mainLoop.loopModifiers.push_back(&renderPassLoopmod);
    mainLoop.loopModifiers.push_back(&postprocessLoopmod);
    mainLoop.addLinkedResource(&targetRebuildLoopmod, false);
    mainLoop.addLinkedResource(&renderPassLoopmod, false);
    mainLoop.addLinkedResource(&postprocessLoopmod, false);
    mainLoop.addLinkedResource(renderPass, true);
    ctx->addLinkedResource(&mainLoop, false);

    class : public GraphicalEntity {
       public:
        void _create(VulkanContext* ctx) {
            SamplerInfo defaultSamplerInfo = {};
            this->texture = new Texture(ctx, defaultSamplerInfo);
            texture->addTextureComponent(ctx, "demos/axion.png", VK_FORMAT_R8G8B8A8_SRGB);
            this->addLinkedResource(texture, true);

            HostObjectData hostData(screenVertices.data(), screenVertices.size() * sizeof(SimplePresetVertex), screenIndices);
            DeviceObjectData* objectData = new DeviceObjectData(ctx, &hostData);
            this->addLinkedResource(objectData, true);
            this->setObjectData(objectData);
        }

        GraphicsPipelineConstraints getPipelineRequirements() override {
            DescriptorSetLayoutInfo dsli;
            DescriptorInfo descInfo;
            descInfo.imageSamplerData.container = this->texture;
            descInfo.imageSamplerData.componentIndex = 0;
            descInfo.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descInfo.shaderStages = VK_SHADER_STAGE_FRAGMENT_BIT;
            dsli.addNewBinding(descInfo, 1);
            GraphicsPipelineConstraints req("demos/compute_postprocess/simple_axion", dsli, simplePresetVsd);
            return req;
        }

       private:
        Texture* texture;
    } screen;

    screen._create(ctx);
    screen.initialize(ctx, renderPass, node, store);

    mainLoop.addLinkedResource(&screen, false);

    mainLoop.initialize(ctx);
    uint32_t frame_count = 0;
    while (!mainLoop.shouldLoopTerminate(ctx)) {
        auto time_point = startTiming();
        mainLoop.process(ctx);
        if (frame_count % 1000 == 0) {
            double fp = getElapsedSeconds(time_point);
            printf("Interval (μs): %lf\n", fp * 1000000);
            printf("Framerate (fps): %lf\n", 1 / fp);
        }
        frame_count++;
    }
    mainLoop.terminateLoop(ctx);

    ctx->destroy(ctx);
    delete ctx;
    return 0;
}