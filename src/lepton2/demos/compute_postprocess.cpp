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
using namespace lepton2::vulkancore::descriptortypes;
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

struct UniformBufferObject {
    uint maxdisp;
    uint seed = 0;
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
    ctx->swapchain.buildSwapchain(ctx);

    // Render pass structure:

    TerminatingSubpassConfig termConfig = RenderSubpass::getDefaultTerminatingConfig(VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_LAYOUT_GENERAL);
    RenderSubpass* node = new RenderSubpass(ctx, termConfig);

    RenderPass* renderPass = new RenderPass(ctx, {node}, 2);
    renderPass->addLinkedResource(node, true);

    ImageArray* renderContainer1 = new ImageArray();  // To store render output
    ImageArray* renderContainer2 = new ImageArray();  // To store x-blurred step
    renderPass->addLinkedResource(renderContainer1, true);
    renderPass->addLinkedResource(renderContainer2, true);

    // Main loop setup:

    VulkanLoop mainLoop(renderPass->getResourceMultiplicity());

    // Framerate monitor
    mainLoop.loopModifiers.push_back(new SimpleFramerateMonitor(0.5, nullptr));
    mainLoop.addLinkedResource(mainLoop.loopModifiers[mainLoop.loopModifiers.size() - 1], true);

    // Loop modifiers to handle rendering and re-creation of render targets:
    RenderTargetImageCreationInfo rticInfo = defaultColorAttachmentRTIC(termConfig.attachmentDescription.format);
    rticInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    ImageArraySwapchainRebuild targetRebuildLoopmod1(rticInfo, renderContainer1, &ctx->swapchain.getSwapchainImages()->extent,
                                                     renderPass->getResourceMultiplicity());
    mainLoop.loopModifiers.push_back(&targetRebuildLoopmod1);

    ImageArraySwapchainRebuild targetRebuildLoopmod2(rticInfo, renderContainer2, &ctx->swapchain.getSwapchainImages()->extent,
                                                     renderPass->getResourceMultiplicity());
    mainLoop.loopModifiers.push_back(&targetRebuildLoopmod2);

    SimpleRenderPass renderPassLoopmod(renderPass, renderContainer1, false);
    mainLoop.loopModifiers.push_back(&renderPassLoopmod);

    targetRebuildLoopmod1.onSwapchainRebuild(ctx);  // Shortcuts for framebuffer init
    targetRebuildLoopmod2.onSwapchainRebuild(ctx);
    renderPassLoopmod.onSwapchainRebuild(ctx);

    // Compute pass with a uniform buffer:
    class ModifiedComputePass : public SimpleComputePass {
       public:
        ModifiedComputePass(VulkanContext* ctx, const char* name, ImageArray* in, ImageArray* out, VkImageLayout layout, UniformBufferObject* ubo)
            : SimpleComputePass(ctx, name, in, out, {16, 16}, layout, {}, getInitialLayoutInfo()) {
            this->ubo = ubo;
        }

        DescriptorSetLayoutInfo getInitialLayoutInfo() {
            DescriptorSetLayoutInfo dsli;
            DescriptorInfo descInfo;
            descInfo.uniformBufferData.bufferSize = sizeof(UniformBufferObject);
            descInfo.uniformBufferData.createNewBuffer = true;
            descInfo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descInfo.shaderStages = VK_SHADER_STAGE_COMPUTE_BIT;
            dsli.addNewBinding(descInfo);
            return dsli;
        }
        void preRender(VulkanContext* ctx, uint32_t frameIndex) override {
            VulkanBuffer* buf = ((UniformBufferDescriptor*)(dsa1->singleDescriptorSets[frameIndex].instances[0]))->uniformBuffer;
            void* data = buf->chonklet.mapMemory(ctx, 0);
            memcpy(data, ubo, sizeof(UniformBufferObject));
            buf->chonklet.unmapMemory(ctx);
        }

       private:
        UniformBufferObject* ubo; // Both passes will share the same UniformBufferObject
    };

    // Make the values in the UBO dance CPU-side with another loop modifier:
    UniformBufferObject ubo;
    class : public VulkanLoopModifier {
       public:
        void _create(UniformBufferObject* ubo, VkExtent2D* windowSize) {
            this->ubo = ubo;
            ubo->seed = 0;
            this->start = startTiming();
            this->windowSize = windowSize;
        }
        void preRender(VulkanContext* ctx, uint32_t frameIndex) override {
            float disp_multiplier = (2.f + cosf(2.f * M_PI * getElapsedSeconds(start))) / 3.f;
            ubo->maxdisp = 2 * (uint)(0.1f * windowSize->height * disp_multiplier);
            ubo->seed++;
        }

       private:
        UniformBufferObject* ubo;
        lepton2_time_point start;
        VkExtent2D* windowSize;
    } dynamicsLoopmod;

    // Add boring CPU-side dynamics modifier:
    dynamicsLoopmod._create(&ubo, &ctx->swapchain.getSwapchainImages()->extent);
    mainLoop.loopModifiers.push_back(&dynamicsLoopmod);

    // X-blurring pass from renderContainer1->images to renderContainer2->images
    ModifiedComputePass postprocessLoopmod1(ctx, "demos/compute_postprocess/blur_x", renderContainer1,
                                            renderContainer2, VK_IMAGE_LAYOUT_GENERAL, &ubo);
    mainLoop.loopModifiers.push_back(&postprocessLoopmod1);
    
    // Y-blurring pass from renderContainer2->images directly to swapchain
    ModifiedComputePass postprocessLoopmod2(ctx, "demos/compute_postprocess/blur_y", renderContainer2,
                                            ctx->swapchain.getSwapchainImages(), VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, &ubo);
    mainLoop.loopModifiers.push_back(&postprocessLoopmod2);

    // Mainly unnecessary, as these do not override destroy_back(VulkanContext*), but good practice:
    mainLoop.addLinkedResource(&targetRebuildLoopmod1, false);
    mainLoop.addLinkedResource(&targetRebuildLoopmod2, false);
    mainLoop.addLinkedResource(&renderPassLoopmod, false);
    mainLoop.addLinkedResource(&dynamicsLoopmod, false);
    mainLoop.addLinkedResource(&postprocessLoopmod1, false);
    mainLoop.addLinkedResource(&postprocessLoopmod2, false);
    mainLoop.addLinkedResource(renderPass, true);

    // Ok, this one is necessary:
    ctx->addLinkedResource(&mainLoop, false);

    // Normal graphics submodule initialization:
    GraphicalConfigurationStore* store = new GraphicalConfigurationStore();
    store->addAllSubpasses(renderPass);
    renderPass->addLinkedResource(store, true);

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

    // Loop!
    mainLoop.initialize(ctx);
    while (!mainLoop.shouldLoopTerminate(ctx)) mainLoop.process(ctx);
    mainLoop.terminateLoop(ctx);

    ctx->destroy(ctx);
    delete ctx;
    return 0;
}