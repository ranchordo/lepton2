#pragma once

#include "Descriptors.h"
#include "ObjectData.h"
#include "VulkanUtils.h"

namespace lepton2::vulkancore {
class RenderGraphNode;

struct PipelineConstraints {
    PipelineConstraints(const char* _shaderName,
                        DescriptorSetLayoutInfo _layoutInfo,
                        VertexStructDescriptor _vsd) {
        this->shaderName = _shaderName;
        this->layoutInfo = _layoutInfo;
        this->vsd = _vsd;
    }
    PipelineConstraints(const PipelineConstraints& other);
    const char* shaderName;
    DescriptorSetLayoutInfo layoutInfo;
    VertexStructDescriptor vsd;
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
    VkBool32 useStencilTesting = VK_FALSE;
    VkStencilOpState stencilState = {};
    VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL;
    VkFrontFace frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT;

    bool compatible(const PipelineConstraints& other);
};

struct PipelineInfo {
    PipelineInfo(std::vector<VkDescriptorSetLayout> dsl,
                 PipelineConstraints& constraints) : constraints(constraints) {
        this->setLayouts = dsl;
        this->vsdBindingDescription = constraints.vsd.getBindingDescription();
        this->vsdAttributeDescriptions = constraints.vsd.getAttributeDescriptions();
    }

    std::vector<VkDescriptorSetLayout> setLayouts;
    PipelineConstraints constraints;
    VkVertexInputBindingDescription vsdBindingDescription;
    std::vector<VkVertexInputAttributeDescription> vsdAttributeDescriptions;
};

class GraphicsPipeline : public DeletableVulkanResource {
   public:
    GraphicsPipeline(VulkanContext* ctx, RenderGraphNode* node,
                     VkRenderPass renderPass, const PipelineInfo& cInfo);
    void bind(VkCommandBuffer commandBuffer);
    VkPipeline getPipeline() { return this->pipeline; }
    static void bindDescriptorSet(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout,
                                  DescriptorSetArray* dsa, uint32_t index, uint32_t setidx);
    void destroy_back(VulkanContext* ctx) override;
    PipelineConstraints creationConstraints;
    VkPipelineLayout getPipelineLayout() {
        return this->pipelineLayout;
    }

    static VkPipelineLayout createPipelineLayout(VulkanContext* ctx, std::vector<VkDescriptorSetLayout> dsl);

   private:
    VkShaderModule buildShaderModule(VulkanContext* ctx,
                                     const std::vector<char>& code);
    VkShaderModule vertexShaderModule;
    VkShaderModule fragmentShaderModule;
    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;
};
}  // namespace lepton2::vulkancore