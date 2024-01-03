#pragma once

#include "VulkanUtils.h"
#include "RenderState.h"

namespace lepton2::vulkancore {
    extern const char* shaders_spirv_load_dir;
    struct Vertex {
        glm::vec3 pos;
        glm::vec3 color;
        glm::vec2 texCoord;

        static VkVertexInputBindingDescription getBindingDescription();
        static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
    };
    class GraphicsPipeline: public DeletableVulkanResource {
    public:
        GraphicsPipeline(VulkanContext* ctx, RenderState renderState,
            const char* shader_name, RenderGraphNode* node,
            std::vector<VkDescriptorSetLayout> descriptorSetLayouts,
            VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT,
            VkBool32 useStencilTesting = VK_FALSE,
            VkStencilOpState stencilState = {},
            VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL,
            VkFrontFace frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
            VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT);
        void destroy_back(VulkanContext* ctx) override;
    private:
        VkShaderModule buildShaderModule(VulkanContext* ctx, const std::vector<char>& code);
        VkShaderModule vertexShaderModule;
        VkShaderModule fragmentShaderModule;
        VkPipeline pipeline;
        VkPipelineLayout pipelineLayout;
    };
}