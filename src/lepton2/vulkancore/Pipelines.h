#pragma once

#include "Descriptors.h"
#include "ObjectData.h"
#include "VulkanUtils.h"

namespace lepton2::vulkancore {
class RenderSubpass;

//! Constraints for creating or matching graphics pipelines.
struct GraphicsPipelineConstraints {
    GraphicsPipelineConstraints(const char* _shaderName,
                                DescriptorSetLayoutInfo _layoutInfo,
                                VertexStructDescriptor _vsd) {
        this->shaderName = _shaderName;
        this->layoutInfo = _layoutInfo;
        this->vsd = _vsd;
    }
    GraphicsPipelineConstraints(const GraphicsPipelineConstraints& other);
    const char* shaderName;
    DescriptorSetLayoutInfo layoutInfo;
    VertexStructDescriptor vsd;
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
    VkBool32 useStencilTesting = VK_FALSE;
    VkBool32 depthTestEnable = VK_TRUE;
    VkBool32 depthWriteEnable = VK_TRUE;
    VkStencilOpState stencilState = {};
    VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL;
    VkFrontFace frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT;

    bool compatible(const GraphicsPipelineConstraints& other);
};

//! Full creation information for a graphical pipeline.
struct GraphicsPipelineInfo {
    GraphicsPipelineInfo(const std::vector<VkDescriptorSetLayout>& dsl,
                         GraphicsPipelineConstraints& constraints) : constraints(constraints) {
        this->setLayouts = dsl;
        this->vsdBindingDescription = constraints.vsd.getBindingDescription();
        this->vsdAttributeDescriptions = constraints.vsd.getAttributeDescriptions();
    }

    std::vector<VkDescriptorSetLayout> setLayouts;
    GraphicsPipelineConstraints constraints;
    VkVertexInputBindingDescription vsdBindingDescription;
    std::vector<VkVertexInputAttributeDescription> vsdAttributeDescriptions;
};

class GraphicsPipeline : public DeletableVulkanResource {
   public:
    GraphicsPipeline(VulkanContext* ctx, RenderSubpass* node,
                     VkRenderPass renderPass, const GraphicsPipelineInfo& cInfo);
    void bind(VkCommandBuffer commandBuffer);
    VkPipeline getPipeline() { return this->pipeline; }
    static void bindDescriptorSet(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout,
                                  DescriptorSetArray* dsa, uint32_t index, uint32_t setidx);
    void destroy_back(VulkanContext* ctx) override;
    GraphicsPipelineConstraints creationConstraints;
    VkPipelineLayout getPipelineLayout() {
        return this->pipelineLayout;
    }

    static VkShaderModule buildShaderModule(VulkanContext* ctx, const std::vector<char>& code);
    static VkPipelineLayout createPipelineLayout(VulkanContext* ctx, std::vector<VkDescriptorSetLayout> dsl);

   private:
    VkShaderModule vertexShaderModule;
    VkShaderModule fragmentShaderModule;
    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;
};

//! Full creation information for a compute pipeline.
struct ComputePipelineInfo {
    ComputePipelineInfo(const std::vector<VkDescriptorSetLayout>& dsl, const char* _shaderName) {
        this->shaderName = _shaderName;
        this->setLayouts = dsl;
    }
    const char* shaderName;
    std::vector<VkDescriptorSetLayout> setLayouts;
};

class ComputePipeline : public DeletableVulkanResource {
   public:
    ComputePipeline(VulkanContext* ctx, const ComputePipelineInfo& cInfo);
    void bind(VkCommandBuffer commandBuffer);
    static void bindDescriptorSet(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout,
                                  DescriptorSetArray* dsa, uint32_t index, uint32_t setidx);
    VkPipelineLayout getPipelineLayout() {
        return this->computePipelineLayout;
    }
    void destroy_back(VulkanContext* ctx) override;

   private:
    VkShaderModule shaderModule;
    VkPipelineLayout computePipelineLayout;
    VkPipeline computePipeline;
};

}  // namespace lepton2::vulkancore