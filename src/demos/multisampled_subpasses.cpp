#include "../lepton2/graphics/GraphicalPresets.h"
#include "../lepton2/utils/LeptonUtils.h"
#include "../lepton2/vulkancore/ObjectData.h"
#include "../lepton2/vulkancore/Pipelines.h"
#include "../lepton2/vulkancore/RenderPass.h"
#include "../lepton2/vulkancore/Textures.h"
#include "../lepton2/vulkancore/VulkanContext.h"
#include "../lepton2/vulkancore/VulkanLoop.h"
#include "../lepton2/vulkancore/VulkanMemory.h"
#include "../lepton2/vulkancore/VulkanUtils.h"

using namespace lepton2::vulkancore;
using namespace lepton2::vulkancore::loopmodifiers;
using namespace lepton2::graphics::graphicalpresets;
using namespace lepton2::graphics;
using namespace lepton2::utils;

static std::vector<SimplePresetVertex> vertices = {
    {{-0.5f, -0.5f, 0.0f}, {0.0f, 1.0f}},
    {{+0.5f, -0.5f, 0.0f}, {1.0f, 1.0f}},
    {{+0.5f, +0.5f, 0.0f}, {1.0f, 0.0f}},
    {{-0.5f, +0.5f, 0.0f}, {0.0f, 0.0f}}};

static std::vector<uint32_t> indices = {3, 0, 1, 2, 3, 1};

struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

int demo_multisampled_subpasses(int argc, char** argv) {
    if (!glfwInit()) {
        throw std::runtime_error("Failed GLFW initialization.");
    }
    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
    GLFWwindow* window = glfwCreateWindow(800, 600, "Lepton2: demo_multisampled_subpasses", nullptr, nullptr);

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
    ctx->swapchain.setSurfaceFormat({VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR});
    ctx->swapchain.buildSwapchain(ctx);

    // Set up render pass structure
    TerminatingSubpassConfig termConfig = ctx->swapchain.getDefaultPresentConfig();
    termConfig.attachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    RenderSubpass* node = new RenderSubpass();
    node->addColorAttachment(defaultColorAttachmentRTIC(termConfig.attachmentDescription.format, VK_SAMPLE_COUNT_4_BIT), true);
    node->addResolveAttachment(termConfig, 0);

    RenderSubpass* node1 = new RenderSubpass();
    node1->addColorAttachment(defaultColorAttachmentRTIC(VK_FORMAT_R16G16B16A16_SFLOAT, VK_SAMPLE_COUNT_4_BIT), true);
    node1->getColorAttachments()[0].clearValue = {1.0f, 1.0f, 1.0f, 1.0f};
    node->connectFromNode(node1, 0, 0);

    RenderPass* renderPass = new RenderPass(ctx, {node, node1}, 2, VK_SAMPLE_COUNT_4_BIT);
    renderPass->generateFramebuffers(ctx, ctx->swapchain.getSwapchainImages());
    renderPass->addLinkedResource(node, true);
    renderPass->addLinkedResource(node1, true);

    node->setupSubpassDescriptorSet(ctx, renderPass, {});  // To establish input attachment descriptors

    VulkanLoop mainLoop(renderPass->getResourceMultiplicity());

    // Framerate monitor
    mainLoop.loopModifiers.push_back(new SimpleFramerateMonitor(0.5, nullptr));
    mainLoop.addLinkedResource(mainLoop.loopModifiers[mainLoop.loopModifiers.size() - 1], true);

    // Main rendering step
    VulkanLoopModifier* loopmod = new SimpleRenderPass(renderPass, ctx->swapchain.getSwapchainImages(), true);
    mainLoop.loopModifiers.push_back(loopmod);
    mainLoop.addLinkedResource(loopmod, true);
    mainLoop.addLinkedResource(renderPass, true);
    ctx->addLinkedResource(&mainLoop, false);

    // Graphics submodule boilerplate
    GraphicalConfigurationStore* store = new GraphicalConfigurationStore();
    store->addAllSubpasses(renderPass);
    renderPass->addLinkedResource(store, true);

    // Behavior for single spinning square entity:
    class : public GraphicalEntity {  // Hehe anonymous classes go brrrrr
       public:
        void _create(VulkanContext* ctx, float rotationSpeed) {
            HostObjectData* hostData = new HostObjectData(vertices.data(), vertices.size() * sizeof(SimplePresetVertex), indices);
            DeviceObjectData* devData = new DeviceObjectData(ctx, hostData);
            hostData->destroy(ctx);
            delete hostData;
            this->addLinkedResource(devData, true);

            this->setObjectData(devData);
            this->rotationSpeed = rotationSpeed;
            SamplerInfo defaultSamplerInfo = {};
            this->textureContainer = new Texture(ctx, defaultSamplerInfo);
            this->textureContainer->addTextureComponent(ctx, "demos/axion.png", VK_FORMAT_R8G8B8A8_SRGB);
            this->addLinkedResource(this->textureContainer, true);
        }
        GraphicsPipelineConstraints getPipelineRequirements() override {
            DescriptorSetLayoutInfo dsli;
            {
                DescriptorInfo descInfo;
                descInfo.uniformBufferData.bufferSize = sizeof(UniformBufferObject);
                descInfo.uniformBufferData.createNewBuffer = true;
                descInfo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                descInfo.shaderStages = VK_SHADER_STAGE_VERTEX_BIT;
                dsli.addNewBinding(descInfo, 1);
            }
            {
                DescriptorInfo descInfo;
                descInfo.imageSamplerData.container = this->textureContainer;
                descInfo.imageSamplerData.componentIndex = 0;
                descInfo.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                descInfo.shaderStages = VK_SHADER_STAGE_FRAGMENT_BIT;
                dsli.addNewBinding(descInfo, 1);
            }
            GraphicsPipelineConstraints req("demos/multisampled_subpasses/simple_axion", dsli, simplePresetVsd);
            return req;
        }
        void postInit(VulkanContext* ctx, RenderPass* pass, RenderSubpass* node) override {
            this->start_time_point = startTiming();
        }
        void preRender(VulkanContext* ctx, SingleDescriptorSet* sds, uint32_t frameIndex) override {
            UniformBufferObject ubo{};
            float elapsedSeconds = (float)getElapsedSeconds(start_time_point);
            ubo.model = glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, 0));
            ubo.model = glm::rotate(ubo.model, elapsedSeconds * this->rotationSpeed, glm::vec3(0, 0, 1));
            ubo.view = glm::lookAt(glm::vec3(1, 1, 0.5), glm::vec3(0, 0, 0), glm::vec3(0, 0, 1));
            VkExtent2D extent = ctx->swapchain.getKnownExtent();
            ubo.proj = glm::perspective(glm::radians(45.0f), extent.width / (float)extent.height, 0.1f, 10.0f);
            ubo.proj[1][1] *= -1;
            VulkanBuffer* uniformBuffer = ((descriptortypes::UniformBufferDescriptor*)(sds->instances[0]))->uniformBuffer;
            void* data = uniformBuffer->chonklet.mapMemory(ctx, 0);
            memcpy(data, &ubo, sizeof(UniformBufferObject));
            uniformBuffer->chonklet.unmapMemory(ctx);
        }
        void destroy_back(VulkanContext* ctx) override {
            this->destroyEntityResources(ctx);
        }

       private:
        lepton2_time_point start_time_point;
        float rotationSpeed;
        Texture* textureContainer;
    } rectangleEntity;

    rectangleEntity._create(ctx, glm::radians(15.0f));
    rectangleEntity.initialize(ctx, renderPass, node1, store);
    mainLoop.addLinkedResource(&rectangleEntity, false);

    // Screen for the second subpass
    StaticScreenEntity screenEntity(ctx, "demos/multisampled_subpasses/post_process");
    screenEntity.initialize(ctx, renderPass, node, store);
    mainLoop.addLinkedResource(&screenEntity, false);

    // Loop!
    mainLoop.initialize(ctx);
    while (!mainLoop.shouldLoopTerminate(ctx)) mainLoop.process(ctx);
    mainLoop.terminateLoop(ctx);

    ctx->destroy(ctx);
    delete ctx;
    return 0;
}