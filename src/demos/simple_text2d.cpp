#include "../lepton2/graphics2d/Entity2d.h"
#include "../lepton2/graphics2d/TextFont.h"
#include "../lepton2/graphics2d/Polygon2d.h"
#include "../lepton2/vulkancore/VulkanLoop.h"

using namespace lepton2::vulkancore;
using namespace lepton2::graphics;
using namespace lepton2::vulkancore::loopmodifiers;
using namespace lepton2::graphics2d;

int demo_simple_text2d(int argc, char** argv) {
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
    ctx->swapchain.buildSwapchain(ctx);

    TerminatingSubpassConfig termConfig = ctx->swapchain.getDefaultPresentConfig();
    RenderSubpass* node = new RenderSubpass(ctx, termConfig);

    RenderPass* renderPass = new RenderPass(ctx, {node}, 2);
    renderPass->generateFramebuffers(ctx, ctx->swapchain.getSwapchainImages());
    renderPass->addLinkedResource(node, true);

    VulkanLoop mainLoop(renderPass->getResourceMultiplicity());

    mainLoop.loopModifiers.push_back(new SimpleFramerateMonitor(0.5, nullptr));
    mainLoop.addLinkedResource(mainLoop.loopModifiers[mainLoop.loopModifiers.size() - 1], true);

    VulkanLoopModifier* loopmod = new SimpleRenderPass(renderPass, ctx->swapchain.getSwapchainImages(), true);
    mainLoop.loopModifiers.push_back(loopmod);
    mainLoop.addLinkedResource(loopmod, true);
    mainLoop.addLinkedResource(renderPass, true);
    ctx->addLinkedResource(&mainLoop, false);

    GraphicalConfigurationStore* store = new GraphicalConfigurationStore();
    store->addAllSubpasses(renderPass);
    renderPass->addLinkedResource(store, true);

    ConformalFrame2d* topFrame = new ConformalFrame2d(ctx, renderPass->getDepthStencilExtent());
    topFrame->initialize(ctx, renderPass, node, store);
    renderPass->addLinkedResource(topFrame, true);

    // Rectangle2d* rect = new Rectangle2d(ctx, "demos/simple_text2d/rectangle", 0, nullptr);
    // rect->initialize(ctx, renderPass, node, store);
    // rect->region = {{0.f, 0.f}, {1.0f, 1.0f}, 0.f};
    // rect->setAspectControlMode(ASPECT_CONTROL_FIT_MIN_DIM_2D);
    // rect->setHorizontalAlignment(ALIGN_HORIZ_RIGHT_2D);
    // rect->setVerticalAlignment(ALIGN_VERT_BOTTOM_2D);
    // topFrame->addLinkedResource(rect, true);
    // topFrame->addChild(rect);

    TextFont* font = new TextFont(ctx, "demos/UbuntuMono.ttf");
    ProcessedGlyph* glyph = font->getGlyph(ctx, (uint32_t)('A'));
    printf("Advance width %fem\n", glyph->advanceWidth);
    store->addLinkedResource(font, true);

    Polygon2d* poly = new Polygon2d(ctx, "demos/simple_text2d/rectangle", 0, glyph->coords);
    poly->initialize(ctx, renderPass, node, store);
    poly->region = {{0.f, 0.f}, {1.0f, 1.0f}, 0.f};
    poly->setAspectControlMode(ASPECT_CONTROL_FIT_MIN_DIM_2D);
    topFrame->addLinkedResource(poly, true);
    topFrame->addChild(poly);

    VulkanLoopModifier* logicmod = new Logic2DLoopModifier(topFrame);
    mainLoop.loopModifiers.push_back(logicmod);
    mainLoop.addLinkedResource(logicmod, true);

    mainLoop.initialize(ctx);
    while (!mainLoop.shouldLoopTerminate(ctx)) mainLoop.process(ctx);
    mainLoop.terminateLoop(ctx);

    ctx->destroy(ctx);
    delete ctx;
    return 0;
}