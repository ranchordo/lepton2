#include "../graphics/GraphicalPresets.h"
#include "../vulkancore/ObjectData.h"
#include "../vulkancore/Pipelines.h"
#include "../vulkancore/RenderPass.h"
#include "../vulkancore/Textures.h"
#include "../vulkancore/VulkanContext.h"
#include "../vulkancore/VulkanLoop.h"
#include "../vulkancore/VulkanMemory.h"
#include "../vulkancore/VulkanUtils.h"
#include "../utils/LeptonUtils.h"

using namespace lepton2::vulkancore;
using namespace lepton2::vulkancore::loopmodifiers;
using namespace lepton2::graphics::graphicalpresets;
using namespace lepton2::graphics;
using namespace lepton2::utils;

#define NUM_GROUPS 32768

// Defined in shader
#define LOCAL_SIZE_X 256

struct SSBO {
    int atomic_total_invocations = 0;
    int atomic_total_primes = 0;
    int test_thing[LOCAL_SIZE_X * NUM_GROUPS];
};

int demo_headless_compute(int argc, char** argv) {
    // GLFW initialization would normally go here, but we're running headless.
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pNext = nullptr;
    appInfo.pApplicationName = "Lepton2 Demo";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "lepton2";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;
#ifdef DEBUG_ENV
    VulkanContext* ctx = new VulkanContext(argv[0], true, true, appInfo, nullptr);
#else
    VulkanContext* ctx = new VulkanContext(argv[0], false, false, appInfo, nullptr);
#endif

    // Set up SSBO for this compute shader launch
    DescriptorSetLayoutInfo dsli;
    DescriptorInfo descInfo;
    descInfo.storageBufferData.bufferSize = sizeof(SSBO);
    descInfo.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descInfo.shaderStages = VK_SHADER_STAGE_COMPUTE_BIT;
    dsli.addNewBinding(descInfo, 1);

    // Allocate/instantiate descriptor instances
    DescriptorSetArray* dsa = new DescriptorSetArray(dsli);
    dsa->buildDescriptorSetLayout(ctx);
    ctx->descriptorPoolManager.allocateDescriptorSets(ctx, dsa, 1);

    SSBO* ssbo = new SSBO(); // Create large CPU-side buffer on the heap

    // Copy to device
    VulkanBuffer* buf = ((descriptortypes::StorageBufferDescriptor*)dsa->singleDescriptorSets[0].instances[0])->storageBuffer;
    void* data = buf->chonklet.mapMemory(ctx, 0);
    memcpy(data, ssbo, sizeof(SSBO));
    buf->chonklet.unmapMemory(ctx);

    // Create and configure compute pipeline
    std::vector<VkDescriptorSetLayout> dsl;
    dsl.push_back(dsa->descriptorSetLayout);
    ComputePipelineInfo pipelineInfo(dsl, "demos/headless_compute/single_compute");
    ComputePipeline* computePipeline = new ComputePipeline(ctx, pipelineInfo);
    
    // Dispatch compute shader through transient compute queue
    VkCommandBuffer commandBuffer = beginSingleTimeCommands(ctx, ctx->commandPools.transientCompute);
    computePipeline->bind(commandBuffer);
    ComputePipeline::bindDescriptorSet(commandBuffer, computePipeline->getPipelineLayout(), dsa, 0, 0);
    vkCmdDispatch(commandBuffer, NUM_GROUPS, 1, 1);
    printf("Submitting compute dispatch...\n");
    lepton2_time_point start = startTiming();
    // Utility function waits internally for submitted queue to finish
    endSingleTimeCommands(ctx, commandBuffer, ctx->queues.compute, ctx->commandPools.transientCompute);
    printf("Finished compute routine in %lg seconds.\n", getElapsedSeconds(start));

    // Get data back from the device
    data = buf->chonklet.mapMemory(ctx, 0);
    memcpy(ssbo, data, sizeof(SSBO));
    buf->chonklet.unmapMemory(ctx);

    // Print results
    printf("Prime numbers from %d-%d are as follows:\n", LOCAL_SIZE_X * (NUM_GROUPS - 16), LOCAL_SIZE_X * NUM_GROUPS);
    bool first_elem = true;
    for (int i = LOCAL_SIZE_X * (NUM_GROUPS - 16); i < LOCAL_SIZE_X * NUM_GROUPS; i++) {
        if (ssbo->test_thing[i] < 0) continue;
        if (!first_elem) printf(", ");
        printf("%d", ssbo->test_thing[i]);
        first_elem = false;
    }
    printf("\n");

    printf("atomics_test: Ran %u total threads, found %u total primes\n", ssbo->atomic_total_invocations, ssbo->atomic_total_primes);

    computePipeline->destroy(ctx);
    dsa->destroy(ctx);

    ctx->destroy(ctx);
    delete ctx;
    return 0;
}