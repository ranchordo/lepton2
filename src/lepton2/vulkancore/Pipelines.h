#pragma once

#include "VulkanUtils.h"
namespace lepton2::vulkancore {
    extern const char* shaders_spirv_load_dir;
    class RenderGraphNode;

    // You have become the very thing you swore to destroy.
    // But I mean we do have these sweet sweet default params
    struct PipelineInfo {
        PipelineInfo(const char* shaderName, RenderGraphNode* node,
            std::vector<VkDescriptorSetLayout> descriptorSetLayouts,
            VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT,
            VkBool32 useStencilTesting = VK_FALSE,
            VkStencilOpState stencilState = {},
            VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL,
            VkFrontFace frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
            VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT);
        const char* shaderName;
        uint32_t subpassIndex;
        std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
        VkSampleCountFlagBits samples;
        VkBool32 useStencilTesting;
        VkStencilOpState stencilState;
        VkPolygonMode polygonMode;
        VkFrontFace frontFace;
        VkCullModeFlags cullMode;
    };

    class GraphicsPipeline: public DeletableVulkanResource {
    public:
        GraphicsPipeline(VulkanContext* ctx, VkRenderPass renderPass, PipelineInfo cInfo);
        void bind(VkCommandBuffer commandBuffer);
        void destroy_back(VulkanContext* ctx) override;
    private:
        VkShaderModule buildShaderModule(VulkanContext* ctx, const std::vector<char>& code);
        VkShaderModule vertexShaderModule;
        VkShaderModule fragmentShaderModule;
        VkPipeline pipeline;
        VkPipelineLayout pipelineLayout;
    };
}