#include "lepton2/vulkancore/GraphicalEntity.h"
#include "lepton2/vulkancore/GraphicalPresets.h"
#include "lepton2/vulkancore/ObjectData.h"
#include "lepton2/vulkancore/Pipelines.h"
#include "lepton2/vulkancore/RenderState.h"
#include "lepton2/vulkancore/Textures.h"
#include "lepton2/vulkancore/VulkanContext.h"
#include "lepton2/vulkancore/VulkanLoop.h"
#include "lepton2/vulkancore/VulkanMemory.h"
#include "lepton2/vulkancore/VulkanUtils.h"

using namespace lepton2::vulkancore;

// TODO: Investigate push_back and see if we can switch to static sizing in some places (namely RenderState et al.)
// TODO: Investigate persistent mapping
// TODO: See if it works with unicode paths

VulkanContext* ctx;

struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 texcoord;
};

VertexStructDescriptor vsd = {{{offsetof(Vertex, pos), VK_FORMAT_R32G32B32_SFLOAT},
                               {offsetof(Vertex, color), VK_FORMAT_R32G32B32_SFLOAT},
                               {offsetof(Vertex, texcoord), VK_FORMAT_R32G32_SFLOAT}},
                              sizeof(Vertex)};

std::vector<Vertex> vertices = {
    {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
    {{+0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
    {{+0.5f, +0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
    {{-0.5f, +0.5f, 0.0f}, {0.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}};

std::vector<uint32_t> indices = {3, 0, 1, 2, 3, 1};

struct ModelUniformBufferObject {
    glm::mat4 model;
};

struct SubpassUniformBufferObject {
    glm::mat4 view;
    glm::mat4 proj;
};

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
    GLFWwindow* window = glfwCreateWindow(800, 600, "Lepton2 test", nullptr, nullptr);
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Lepton2 test";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "lepton2";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;
#ifdef DEBUG_ENV
    ctx = new VulkanContext(argv[0], true, true, appInfo, window);
#else
    ctx = new VulkanContext(argv[0], false, false, appInfo, window);
#endif

    RenderGraph renderGraph(ctx);
    RenderGraphNode* node = renderGraph.buildPresentingNode();

    // RenderGraphNode* node1 = renderGraph.buildNewNode();
    // RenderTargetImageCreationInfo rticInfo{};
    // rticInfo.use_presenter = false;
    // rticInfo.imageTiling = VK_IMAGE_TILING_OPTIMAL;
    // rticInfo.memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    // rticInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    // rticInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
    // rticInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    // rticInfo.aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
    // node1->addColorAttachment(rticInfo, true);
    // node->connectFromNode(node1, 0, 0);

    RenderState* renderState = renderGraph.buildRenderState();
    renderState->addLinkedResource(node, true);
    // renderState->addLinkedResource(node1, true);
    renderState->addLinkedResource(&renderGraph, false);
    // ctx->swapChain.swapChainQueryResults.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
    ctx->swapChain.buildSwapChain(renderState);

    VulkanLoop mainLoop(renderState, 2);

    DeletableVulkanResourceTracker sceneContainer;

    SamplerInfo defaultSamplerInfo = {};
    Texture* texture = new Texture(ctx, defaultSamplerInfo);
    texture->addTextureComponent(ctx, "texture.png");
    sceneContainer.addLinkedResource(texture, true);

    class : public GraphicalEntity {  // Hehe anonymous classes go brrrrr
       public:
        void _create(VulkanContext* ctx, DeviceObjectData* objectData, float rotationSpeed, float zpos, Texture* texture) {
            this->setObjectData(objectData);
            this->rotationSpeed = rotationSpeed;
            this->textureContainer = texture;
            this->zpos = zpos;
        }
        PipelineConstraints getPipelineRequirements() override {
            DescriptorSetLayoutInfo dsli;
            {
                DescriptorInfo descInfo;
                descInfo.uniformBufferData.bufferSize = sizeof(ModelUniformBufferObject);
                descInfo.uniformBufferData.createNewBuffer = true;
                descInfo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                descInfo.shaderStages = VK_SHADER_STAGE_VERTEX_BIT;
                dsli.addNewBinding(descInfo);
            }
            {
                DescriptorInfo descInfo;
                descInfo.imageSamplerData.container = this->textureContainer;
                descInfo.imageSamplerData.componentIndex = 0;
                descInfo.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                descInfo.shaderStages = VK_SHADER_STAGE_FRAGMENT_BIT;
                dsli.addNewBinding(descInfo);
            }
            PipelineConstraints req("shader", dsli, vsd);
            return req;
        }
        void postInit(RenderGraphNode* node, RenderState* renderState) override {
            this->start_time_point = startTiming();
        }
        void preRender(RenderState* renderState, uint32_t descriptorIndex) override {
            ModelUniformBufferObject ubo{};
            float elapsedSeconds = (float)getElapsedSeconds(start_time_point);
            ubo.model = glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, zpos + sin(elapsedSeconds * 4 * this->rotationSpeed) * 0.1));
            ubo.model = glm::rotate(ubo.model, elapsedSeconds * this->rotationSpeed, glm::vec3(0, 0, 1));
            VulkanBuffer* uniformBuffer = ((descriptortypes::UniformBufferDescriptor*)(dsa->singleDescriptorSets[descriptorIndex].instances[0]))->uniformBuffer;
            void* data = uniformBuffer->chonklet.mapMemory(ctx, 0);
            memcpy(data, &ubo, sizeof(ModelUniformBufferObject));
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

    DescriptorSetLayoutInfo subpassDSLI;
    {
        DescriptorInfo descInfo;
        descInfo.uniformBufferData.bufferSize = sizeof(SubpassUniformBufferObject);
        descInfo.uniformBufferData.createNewBuffer = true;
        descInfo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descInfo.shaderStages = VK_SHADER_STAGE_VERTEX_BIT;
        subpassDSLI.addNewBinding(descInfo);
    }
    node->setupSubpassDescriptorSet(ctx, subpassDSLI);

    HostObjectData* hostData = new HostObjectData(vertices.data(), vertices.size() * sizeof(Vertex), indices);
    sceneContainer.addLinkedResource(hostData, true);
    DeviceObjectData* devData = new DeviceObjectData(ctx, hostData);
    sceneContainer.addLinkedResource(devData, true);
    rectangleEntity._create(ctx, devData, glm::radians(90.0f), 0.2f, texture);
    rectangleEntity.initialize(node, renderState);
    sceneContainer.addLinkedResource(&rectangleEntity, false);

    decltype(rectangleEntity) rectangleEntity1;
    rectangleEntity1._create(ctx, devData, glm::radians(-90.0f), -0.2f, texture);
    rectangleEntity1.initialize(node, renderState);
    sceneContainer.addLinkedResource(&rectangleEntity1, false);

    // graphicalpresets::StaticScreenEntity screenEntity(ctx, "prshader", &node1->getColorAttachments()->at(0));
    // screenEntity.initialize(node, renderState);
    // sceneContainer.addLinkedResource(&screenEntity, false);

    SubpassUniformBufferObject subo{};
    subo.view = glm::lookAt(glm::vec3(1, 1, 0.5), glm::vec3(0, 0, 0), glm::vec3(0, 0, 1));
    subo.proj = glm::perspective(glm::radians(45.0f), ctx->swapChain.swapChainExtent.width / (float)ctx->swapChain.swapChainExtent.height, 0.1f, 10.0f);
    subo.proj[1][1] *= -1;

    // Setup loop modifier
    class : public VulkanLoopModifier {
       public:
        void _init(SubpassUniformBufferObject* subo, RenderGraphNode* subpass) {
            this->subo = subo;
            this->subpass = subpass;
        }
        void preRender(uint32_t scfi) override {
            VulkanBuffer* uniformBuffer = ((descriptortypes::UniformBufferDescriptor*)(subpass->getSubpassDsa()->singleDescriptorSets[scfi].instances[0]))->uniformBuffer;
            void* data = uniformBuffer->chonklet.mapMemory(ctx, 0);
            memcpy(data, subo, sizeof(SubpassUniformBufferObject));
            uniformBuffer->chonklet.unmapMemory(ctx);
        }

       private:
        SubpassUniformBufferObject* subo;
        RenderGraphNode* subpass;
    } suboloopmodifier;

    suboloopmodifier._init(&subo, node);
    mainLoop.addLinkedResource(&sceneContainer, false);
    mainLoop.addLoopModifier(&suboloopmodifier);

    glfwSetInputMode(ctx->window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    mainLoop.initialize();
    uint32_t frame_count = 0;
    while (!mainLoop.shouldLoopTerminate()) {
        auto time_point = startTiming();
        mainLoop.process();
        double fp = getElapsedSeconds(time_point);
        if (frame_count % 1000 == 0) {
            printf("Interval (μs): %lf\n", fp * 1000000);
            printf("Framerate (fps): %lf\n", 1 / fp);
        }
        frame_count++;
    }
    mainLoop.terminateLoop();

    mainLoop.destroy(ctx);
    renderState->destroy(ctx);
    delete renderState;
    ctx->destroy(ctx);
    delete ctx;
    return 0;
}