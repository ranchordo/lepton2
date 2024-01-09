#pragma once

#include "Descriptors.h"
#include "VulkanUtils.h"

namespace lepton2::vulkancore {
extern const char* shaders_spirv_load_dir;
class RenderGraphNode;

// You have become the very thing you swore to destroy.
// But I mean we do have these sweet sweet default params
struct PipelineInfo {
    PipelineInfo(const char* shaderName,
                 DescriptorSetLayoutInfo dsaLayout,
                 DescriptorSetArray* dsaLayoutReference,
                 VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT,
                 VkBool32 useStencilTesting = VK_FALSE,
                 VkStencilOpState stencilState = {},
                 VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL,
                 VkFrontFace frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
                 VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT);
    PipelineInfo(const PipelineInfo& other);
    const char* shaderName;
    DescriptorSetLayoutInfo dsaLayout;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkSampleCountFlagBits samples;
    VkBool32 useStencilTesting;
    VkStencilOpState stencilState;
    VkPolygonMode polygonMode;
    VkFrontFace frontFace;
    VkCullModeFlags cullMode;
};

class GraphicsPipeline : public DeletableVulkanResource {
   public:
    GraphicsPipeline(VulkanContext* ctx, uint32_t subpassIndex,
                     VkRenderPass renderPass, PipelineInfo cInfo);
    void bind(VkCommandBuffer commandBuffer);
    VkPipeline getPipeline() { return this->pipeline; }
    void bindDescriptorSet(VkCommandBuffer commandBuffer, DescriptorSetArray* dsa, uint32_t index);
    void destroy_back(VulkanContext* ctx) override;
    PipelineInfo creationInfo;
    VkPipelineLayout getPipelineLayout() {
        return this->pipelineLayout;
    }

   private:
    VkShaderModule buildShaderModule(VulkanContext* ctx,
                                     const std::vector<char>& code);
    VkShaderModule vertexShaderModule;
    VkShaderModule fragmentShaderModule;
    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;
};
}  // namespace lepton2::vulkancore