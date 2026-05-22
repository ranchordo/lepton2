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

int demo_render_to_texture(int argc, char** argv) {
    if (!glfwInit()) {
        throw std::runtime_error("Failed GLFW initialization.");
    }
    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(800, 800, "Lepton2: demo_render_to_texture", nullptr, nullptr);
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
    ctx->swapchain.buildSwapchain(ctx);

    // Set up render pass structure
    TerminatingSubpassConfig firstTermConfig = RenderSubpass::getDefaultTerminatingConfig(VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    RenderSubpass* node = new RenderSubpass(ctx, firstTermConfig);

    RenderSubpass* node1 = new RenderSubpass();
    node1->addColorAttachment(defaultColorAttachmentRTIC(VK_FORMAT_R16G16B16A16_SFLOAT), true);
    node1->getColorAttachments()[0].clearValue = {0.5f, 0.5f, 0.5f, 0.0f};
    node->connectFromNode(node1, 0, 0);
    node->requestDepthAsInput(1);

    RenderPass* renderPass = new RenderPass(ctx, {node, node1}, 2);

    // Render targets - fixed resolution (otherwise you need an ImageArraySwapchainRebuild modifier)
    RenderTargetImageCreationInfo renderTargetInfo = defaultColorAttachmentRTIC(VK_FORMAT_R16G16B16A16_SFLOAT);
    renderTargetInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    ImageArray* renderContainer = new ImageArray();
    renderPass->addLinkedResource(renderContainer, true);
    VkExtent2D firstRenderExtent = {256, 256};
    fillRenderTargetArray(ctx, &renderTargetInfo, renderContainer, firstRenderExtent, renderPass->getResourceMultiplicity());

    renderPass->generateFramebuffers(ctx, &renderContainer->images, firstRenderExtent);
    renderPass->addLinkedResource(node, true);
    renderPass->addLinkedResource(node1, true);
    node->setupSubpassDescriptorSet(ctx, renderPass, {});  // To establish input attachment descriptors

    VulkanLoop mainLoop(renderPass->getResourceMultiplicity());

    // Framerate monitor
    mainLoop.loopModifiers.push_back(new SimpleFramerateMonitor(0.5, nullptr));
    mainLoop.addLinkedResource(mainLoop.loopModifiers[mainLoop.loopModifiers.size() - 1], true);

    // First rendering step - passing nullptr to targets will just stop framebuffer reinit on swapchain rebuild
    VulkanLoopModifier* loopmod = new SimpleRenderPass(renderPass, nullptr, false);
    mainLoop.loopModifiers.push_back(loopmod);
    mainLoop.addLinkedResource(loopmod, true);
    mainLoop.addLinkedResource(renderPass, true);
    ctx->addLinkedResource(&mainLoop, false);

    // Set up the demo_simple_subpasses scene inside the first render pass:
    DeletableVulkanResourceTracker sceneContainer;

    GraphicalConfigurationStore* store = new GraphicalConfigurationStore();
    store->addAllSubpasses(renderPass);
    renderPass->addLinkedResource(store, true);

    SamplerInfo defaultSamplerInfo = {};
    Texture* axionTexture = new Texture(ctx, defaultSamplerInfo);
    axionTexture->addTextureComponent(ctx, "demos/axion.png", VK_FORMAT_R8G8B8A8_SRGB);
    sceneContainer.addLinkedResource(axionTexture, true);

    class : public GraphicalEntity {
       public:
        void _create(VulkanContext* ctx, DeviceObjectData* objectData, float rotationSpeed, float zpos, Texture* texture, const char* shader) {
            this->setObjectData(objectData);
            this->rotationSpeed = rotationSpeed;
            this->textureContainer = texture;
            this->zpos = zpos;
            this->shaderName = shader;
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
            GraphicsPipelineConstraints req(shaderName, dsli, simplePresetVsd);
            return req;
        }
        void postInit(VulkanContext* ctx, RenderPass* pass, RenderSubpass* node) override {
            this->start_time_point = startTiming();
        }
        void preRender(VulkanContext* ctx, SingleDescriptorSet* sds, uint32_t frameIndex) override {
            UniformBufferObject ubo{};
            float elapsedSeconds = (float)getElapsedSeconds(start_time_point);
            ubo.model = glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, zpos + sin(elapsedSeconds * 4 * this->rotationSpeed) * 0.1));
            ubo.model = glm::rotate(ubo.model, elapsedSeconds * this->rotationSpeed + glm::radians(135.0f), glm::vec3(0, 0, 1));
            ubo.view = glm::lookAt(glm::vec3(1, 1, 0.5), glm::vec3(0, 0, 0), glm::vec3(0, 0, 1));
            ubo.proj = glm::perspective(glm::radians(45.0f), 1.0f, 0.1f, 10.0f);
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
        const char* shaderName;
    } rectangleEntity;

    HostObjectData* hostData = new HostObjectData(vertices.data(), vertices.size() * sizeof(SimplePresetVertex), indices);
    DeviceObjectData* devData = new DeviceObjectData(ctx, hostData);
    hostData->destroy(ctx);
    delete hostData;

    sceneContainer.addLinkedResource(devData, true);
    rectangleEntity._create(ctx, devData, glm::radians(90.0f), 0.2f, axionTexture, "demos/render_to_texture/simple_axion");
    rectangleEntity.initialize(ctx, renderPass, node1, store);
    sceneContainer.addLinkedResource(&rectangleEntity, false);

    decltype(rectangleEntity) rectangleEntity1;
    rectangleEntity1._create(ctx, devData, glm::radians(-90.0f), -0.2f, axionTexture, "demos/render_to_texture/simple_axion");
    rectangleEntity1.initialize(ctx, renderPass, node1, store);
    sceneContainer.addLinkedResource(&rectangleEntity1, false);

    StaticScreenEntity screenEntity(ctx, "demos/render_to_texture/post_process_1");
    screenEntity.initialize(ctx, renderPass, node, store);
    sceneContainer.addLinkedResource(&screenEntity, false);

    mainLoop.addLinkedResource(&sceneContainer, false);

    // Set up the second render pass (directly to swapchain)
    TerminatingSubpassConfig swapchainTermConfig = ctx->swapchain.getDefaultPresentConfig();
    RenderSubpass* pass2_node = new RenderSubpass(ctx, swapchainTermConfig);
    RenderPass* renderPass2 = new RenderPass(ctx, {pass2_node}, renderPass->getResourceMultiplicity());
    renderPass2->addLinkedResource(pass2_node, true);
    renderPass2->generateFramebuffers(ctx, &ctx->swapchain.getSwapchainImages()->images, ctx->swapchain.getKnownExtent());

    VulkanLoopModifier* loopmod2 = new SimpleRenderPass(renderPass2, ctx->swapchain.getSwapchainImages(), true);
    mainLoop.loopModifiers.push_back(loopmod2);
    mainLoop.addLinkedResource(loopmod2, true);
    mainLoop.addLinkedResource(renderPass2, true);

    // Build a texture from renderContainer image array
    Texture* renderTexture = new Texture(ctx, defaultSamplerInfo);
    renderTexture->addTextureComponent(new TextureComponent(renderContainer));
    sceneContainer.addLinkedResource(renderTexture, true);

    // Graphics submodule stuff for renderPass2 and pass2_node
    store->addAllSubpasses(renderPass2);

    decltype(rectangleEntity) rectangleEntity_pass2;
    rectangleEntity_pass2._create(ctx, devData, glm::radians(9.0f), 0.0f, renderTexture, "demos/render_to_texture/post_process_2");
    rectangleEntity_pass2.initialize(ctx, renderPass2, pass2_node, store);
    sceneContainer.addLinkedResource(&rectangleEntity_pass2, false);

    // Loop!
    mainLoop.initialize(ctx);
    while (!mainLoop.shouldLoopTerminate(ctx)) mainLoop.process(ctx);
    mainLoop.terminateLoop(ctx);

    ctx->destroy(ctx);
    delete ctx;
    return 0;
}