#include "lepton2/graphics/GraphicalEntity.h"
#include "lepton2/graphics/GraphicalPresets.h"
#include "lepton2/utils/InputHandler.h"
#include "lepton2/utils/LeptonUtils.h"
#include "lepton2/vulkancore/ObjectData.h"
#include "lepton2/vulkancore/Pipelines.h"
#include "lepton2/vulkancore/RenderState.h"
#include "lepton2/vulkancore/Textures.h"
#include "lepton2/vulkancore/VulkanContext.h"
#include "lepton2/vulkancore/VulkanLoop.h"
#include "lepton2/vulkancore/VulkanMemory.h"
#include "lepton2/vulkancore/VulkanUtils.h"

using namespace lepton2::vulkancore;
using namespace lepton2::utils;
using namespace lepton2::graphics;
using namespace lepton2::graphics::graphicalpresets;

// TODO: Investigate push_back and see if we can switch to static sizing in some places (namely RenderState et al.)
// TODO: Investigate persistent mapping
// TODO: See if it works with unicode paths

VulkanContext* ctx;

struct SpinorVertex {
    glm::vec4 pos;
    glm::vec2 texcoord;
    glm::vec4 normal;
};

VertexStructDescriptor spinorvsd = {{{offsetof(SpinorVertex, pos), VK_FORMAT_R32G32B32A32_SFLOAT},
                                     {offsetof(SpinorVertex, texcoord), VK_FORMAT_R32G32_SFLOAT},
                                     {offsetof(SpinorVertex, normal), VK_FORMAT_R32G32B32A32_SFLOAT}},
                                    sizeof(SpinorVertex)};

struct ModelUniformBufferObject {
    glm::quat model_rot;
    glm::vec3 model_trans;
};

struct SubpassUniformBufferObject {
    glm::mat4 proj;
    glm::mat4 view_post;
    glm::quat view_rot;
    glm::vec3 view_trans;
    float phase_thresh;
};

glm::vec2 zmult(glm::vec2 a, glm::vec2 b) {
    return glm::vec2(a.x * b.x - a.y * b.y, a.x * b.y + a.y * b.x);
}

glm::vec4 to_spinor(glm::vec3 vec3, float phase) {
    float phi = atan2f(vec3.y, vec3.x);
    float xymag = sqrtf(vec3.x * vec3.x + vec3.y * vec3.y);
    float theta = atan2f(xymag, vec3.z);
    glm::vec2 xi1 = glm::vec2(cosf(theta / 2), 0);
    glm::vec2 xi2 = sinf(theta / 2) * glm::vec2(cosf(phi), sinf(phi));
    glm::vec2 phasor = glm::vec2(cosf(phase), sinf(phase));
    return glm::length(vec3) * glm::vec4(zmult(phasor, xi1), zmult(phasor, xi2));
}

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
    GLFWwindow* window = glfwCreateWindow(1600, 1200, "Lepton2 test", nullptr, nullptr);
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

    GraphicalConfigurationStore* store = new GraphicalConfigurationStore();
    renderState->addLinkedResource(store, true);
    store->addPass(renderState);

    VulkanLoop mainLoop(renderState, 2);

    DeletableVulkanResourceTracker sceneContainer;

    SamplerInfo defaultSamplerInfo = {};
    Texture* texture = new Texture(ctx, defaultSamplerInfo);
    texture->addTextureComponent(ctx, "sphere-test.png");
    sceneContainer.addLinkedResource(texture, true);

    HostObjectData* objmodel = loadObjFile(ctx, "sphere-test.obj");
    ObjLoadVertex* vertices = (ObjLoadVertex*)objmodel->vertices;
    uint32_t num_vertices = objmodel->vsize / sizeof(ObjLoadVertex);

    SpinorVertex* sverts = (SpinorVertex*)malloc(num_vertices * sizeof(SpinorVertex));

    for (uint32_t i = 0; i < num_vertices; i++) {
        ObjLoadVertex vert = vertices[i];
        SpinorVertex svert{};
        float phase = -atan2(vert.pos.y, vert.pos.x);
        svert.pos = to_spinor(vert.pos, phase);
        svert.normal = to_spinor(vert.normal, 0.0);
        svert.texcoord = vert.texcoord;
        sverts[i] = svert;
    }

    HostObjectData* model = new HostObjectData(sverts, num_vertices * sizeof(SpinorVertex), objmodel->indices, false);
    objmodel->destroy(ctx);
    delete objmodel;

    DeviceObjectData* objectData = new DeviceObjectData(ctx, model);
    model->destroy(ctx);  // Deletes sverts pointer
    sverts = nullptr;
    delete model;

    ModelUniformBufferObject model_ubo;

    GenericSinglyTextured entity(ctx, "shader", objectData, texture, spinorvsd);
    entity.set_ubo(&model_ubo, sizeof(ModelUniformBufferObject));
    entity.addLinkedResource(objectData, true);

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

    entity.initialize(store, node, renderState);
    sceneContainer.addLinkedResource(&entity, false);

    model_ubo.model_trans = glm::vec3(0.0f);
    model_ubo.model_rot = glm::quat(1.0f, 0, 0, 0);//glm::rotate(glm::quat(1.0f, 0, 0, 0), glm::radians(90.0f), glm::vec3(1, 0, 0));

    SubpassUniformBufferObject subo{};
    subo.view_trans = -glm::vec3(1, 0, 0);
    subo.view_rot = glm::quat(1, 0, 0, 0);
    subo.view_post = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1, 0, 0));
    subo.phase_thresh = 0;
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

    InputHandler input(ctx->window);

    glm::vec3 ppos = glm::vec3(0, 0, 0.01f);
    double phi = 0.0;
    double theta = 0.0;

    glfwSetInputMode(ctx->window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    glfwPollEvents();
    MousePosition mpos = input.getMousePosition();

    mainLoop.initialize();
    uint32_t frame_count = 0;

    while (!mainLoop.shouldLoopTerminate()) {
        auto time_point = startTiming();
        mainLoop.process();
        double fp = getElapsedSeconds(time_point);
        {  // Movement
            MousePosition mpos2 = input.getMousePosition();
            subo.view_rot = glm::rotate(glm::rotate(glm::quat(1, 0, 0, 0), (float)(-theta), glm::vec3(1, 0, 0)), (float)(-phi), glm::vec3(0, 0, 1));
            subo.view_trans = -ppos;
            subo.phase_thresh = -(phi + M_PI) / 2;
            theta += (mpos.y - mpos2.y) * 0.005;
            phi += (mpos.x - mpos2.x) * 0.005;
            if (theta > M_PI_2) theta = M_PI_2;
            if (theta < -M_PI_2) theta = -M_PI_2;
            mpos = mpos2;
            glm::mat3 movframe = glm::rotate(glm::mat4(1.0f), (float)(phi), glm::vec3(0, 0, 1));
            float movspeed = 1.5 * fp * (1.0 + 1.2 * input.keyPressed(GLFW_KEY_LEFT_CONTROL));
            glm::vec3 up = movframe * glm::vec3(0, 0, movspeed);
            glm::vec3 forward = movframe * glm::vec3(0, movspeed, 0);
            glm::vec3 right = movframe * glm::vec3(movspeed, 0, 0);
            if (input.keyPressed(GLFW_KEY_W)) ppos += forward;
            if (input.keyPressed(GLFW_KEY_S)) ppos -= forward;
            if (input.keyPressed(GLFW_KEY_A)) ppos -= right;
            if (input.keyPressed(GLFW_KEY_D)) ppos += right;
            if (input.keyPressed(GLFW_KEY_SPACE)) ppos += up;
            if (input.keyPressed(GLFW_KEY_LEFT_SHIFT)) ppos -= up;
        }
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