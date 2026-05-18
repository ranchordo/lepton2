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

int demo_simple_subpasses(int argc, char** argv) {
    if (!glfwInit()) {
        throw std::runtime_error("Failed GLFW initialization.");
    }
    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(800, 600, "Lepton2: demo_simple_subpasses", nullptr, nullptr);
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

    TerminatingSubpassConfig termConfig = ctx->swapchain.getDefaultPresentConfig();
    RenderSubpass* node = new RenderSubpass(ctx, termConfig);

    RenderSubpass* node1 = new RenderSubpass();
    node1->addColorAttachment(defaultColorAttachmentRTIC(VK_FORMAT_R16G16B16A16_SFLOAT), true);
    node1->getColorAttachments()[0].clearValue = {0.5f, 0.5f, 0.5f, 0.0f};
    node->connectFromNode(node1, 0, 0);
    node->requestDepthAsInput(1);

    RenderPass* renderPass = new RenderPass(ctx, {node, node1}, 2);
    ctx->swapchain.buildSwapchain(ctx);
    renderPass->generateFramebuffers(ctx, &ctx->swapchain.getSwapchainImages()->images, ctx->swapchain.getKnownExtent());
    renderPass->addLinkedResource(node, true);
    renderPass->addLinkedResource(node1, true);

    node->setupSubpassDescriptorSet(ctx, renderPass, {});  // To establish input attachment descriptors

    GraphicalConfigurationStore* store = new GraphicalConfigurationStore();
    store->addAllSubpasses(renderPass);
    renderPass->addLinkedResource(store, true);

    // Main loop setup
    VulkanLoop mainLoop(renderPass->getResourceMultiplicity());
    VulkanLoopModifier* loopmod = new SimpleRenderPass(renderPass, ctx->swapchain.getSwapchainImages(), true);
    mainLoop.loopModifiers.push_back(loopmod);
    mainLoop.addLinkedResource(loopmod, true);
    mainLoop.addLinkedResource(renderPass, true);
    ctx->addLinkedResource(&mainLoop, false);

    DeletableVulkanResourceTracker sceneContainer;

    SamplerInfo defaultSamplerInfo = {};
    Texture* texture = new Texture(ctx, defaultSamplerInfo);
    texture->addTextureComponent(ctx, "demos/axion.png", VK_FORMAT_R8G8B8A8_SRGB);
    sceneContainer.addLinkedResource(texture, true);

    class : public GraphicalEntity {  // Hehe anonymous classes go brrrrr
       public:
        void _create(VulkanContext* ctx, DeviceObjectData* objectData, float rotationSpeed, float zpos, Texture* texture) {
            this->setObjectData(objectData);
            this->rotationSpeed = rotationSpeed;
            this->textureContainer = texture;
            this->zpos = zpos;
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
            GraphicsPipelineConstraints req("demos/simple_subpasses/simple_axion", dsli, simplePresetVsd);
            return req;
        }
        void postInit(VulkanContext* ctx, RenderPass* pass, RenderSubpass* node) override {
            this->start_time_point = startTiming();
        }
        void preRender(VulkanContext* ctx, SingleDescriptorSet* sds, uint32_t frameIndex) override {
            UniformBufferObject ubo{};
            float elapsedSeconds = (float)getElapsedSeconds(start_time_point);
            ubo.model = glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, zpos + sin(elapsedSeconds * 4 * this->rotationSpeed) * 0.1));
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
        float zpos;
        Texture* textureContainer;
    } rectangleEntity;

    HostObjectData* hostData = new HostObjectData(vertices.data(), vertices.size() * sizeof(SimplePresetVertex), indices);
    DeviceObjectData* devData = new DeviceObjectData(ctx, hostData);
    hostData->destroy(ctx);
    delete hostData;

    sceneContainer.addLinkedResource(devData, true);
    rectangleEntity._create(ctx, devData, glm::radians(90.0f), 0.2f, texture);
    rectangleEntity.initialize(ctx, renderPass, node1, store);
    sceneContainer.addLinkedResource(&rectangleEntity, false);

    decltype(rectangleEntity) rectangleEntity1;
    rectangleEntity1._create(ctx, devData, glm::radians(-90.0f), -0.2f, texture);
    rectangleEntity1.initialize(ctx, renderPass, node1, store);
    sceneContainer.addLinkedResource(&rectangleEntity1, false);

    StaticScreenEntity screenEntity(ctx, "demos/simple_subpasses/post_process");
    screenEntity.initialize(ctx, renderPass, node, store);
    sceneContainer.addLinkedResource(&screenEntity, false);

    mainLoop.addLinkedResource(&sceneContainer, false);

    screenEntity.getConfigurationHandle().debugPrintAllBoundDescriptors();
    rectangleEntity.getConfigurationHandle().debugPrintAllBoundDescriptors();

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