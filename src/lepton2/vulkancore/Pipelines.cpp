#include "Pipelines.h"

#include "../utils/LeptonUtils.h"
#include "ObjectData.h"
#include "RenderState.h"
#include "VulkanContext.h"

using namespace lepton2::vulkancore;

PipelineConstraints::PipelineConstraints(const PipelineConstraints& other) {
    this->shaderName = other.shaderName;
    this->layoutInfo = other.layoutInfo;
    this->vsd = other.vsd;
    this->samples = other.samples;
    this->useStencilTesting = other.useStencilTesting;
    this->stencilState = other.stencilState;
    this->polygonMode = other.polygonMode;
    this->frontFace = other.frontFace;
    this->cullMode = other.cullMode;
}

bool PipelineConstraints::compatible(const PipelineConstraints& other) {
    if (strcmp(this->shaderName, other.shaderName) != 0) return false;
    if (!DescriptorSetArray::isLayoutCompatible(this->layoutInfo.bindings, other.layoutInfo.bindings)) return false;
    if (!this->vsd.equal(other.vsd)) return false;
    if (this->samples != other.samples) return false;
    if (this->useStencilTesting != other.useStencilTesting) return false;
    if (this->useStencilTesting) throw std::runtime_error("Cannot yet compare stencil states.");
    if (this->polygonMode != other.polygonMode) return false;
    if (this->frontFace != other.frontFace) return false;
    if (this->cullMode != other.cullMode) return false;
    return true;
}

VkShaderModule GraphicsPipeline::buildShaderModule(VulkanContext* ctx, const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = (uint32_t*)code.data();
    VkShaderModule shaderModule;
    if (vkCreateShaderModule(ctx->device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module.");
    }
    return shaderModule;
}

GraphicsPipeline::GraphicsPipeline(VulkanContext* ctx, RenderGraphNode* node, VkRenderPass renderPass,
                                   const PipelineInfo& cInfo) : creationConstraints(cInfo.constraints) {
    char* buf = ctx->buildShaderLoadPaths(cInfo.constraints.shaderName);
    size_t vcodelen = 0;
    {
        std::string vcode = std::string(buf);
        vcodelen = vcode.length();
        std::vector<char> vertex_code = lepton2::utils::readFile(vcode);
        this->vertexShaderModule = this->buildShaderModule(ctx, vertex_code);
    }
    {
        std::string fcode = std::string(buf + vcodelen + 1);
        std::vector<char> fragment_code = lepton2::utils::readFile(fcode);
        this->fragmentShaderModule = this->buildShaderModule(ctx, fragment_code);
    }
    free(buf);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = this->vertexShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = this->fragmentShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {
        vertShaderStageInfo,
        fragShaderStageInfo};

    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = (uint32_t)dynamicStates.size();
    dynamicState.pDynamicStates = dynamicStates.data();

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &cInfo.vsdBindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = (uint32_t)(cInfo.vsdAttributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = cInfo.vsdAttributeDescriptions.data();
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = cInfo.constraints.polygonMode;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = cInfo.constraints.cullMode;
    rasterizer.frontFace = cInfo.constraints.frontFace;
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.depthBiasConstantFactor = 0.0f;
    rasterizer.depthBiasClamp = 0.0f;
    rasterizer.depthBiasSlopeFactor = 0.0f;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = cInfo.constraints.samples;
    multisampling.minSampleShading = 1.0f;
    multisampling.pSampleMask = nullptr;
    multisampling.alphaToCoverageEnable = VK_FALSE;
    multisampling.alphaToOneEnable = VK_FALSE;

    std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments;
    for (uint32_t i = 0; i < node->getColorAttachments()->size(); i++) {
        colorBlendAttachments.push_back(node->getColorAttachments()->at(i).rticInfo.blendState);
    }

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = colorBlendAttachments.size();
    colorBlending.pAttachments = colorBlendAttachments.data();

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges = nullptr;
    pipelineLayoutInfo.setLayoutCount = (uint32_t)(cInfo.setLayouts.size());
    pipelineLayoutInfo.pSetLayouts = cInfo.setLayouts.data();

    if (vkCreatePipelineLayout(ctx->device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline layout.");
    }

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.minDepthBounds = 0.0f;
    depthStencil.maxDepthBounds = 1.0f;
    depthStencil.stencilTestEnable = cInfo.constraints.useStencilTesting;
    depthStencil.back = cInfo.constraints.stencilState;
    depthStencil.front = cInfo.constraints.stencilState;

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = node->getSubpassIndex();
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = -1;

    if (vkCreateGraphicsPipelines(ctx->device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create graphics pipeline.");
    }
}

void GraphicsPipeline::bind(VkCommandBuffer commandBuffer) {
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, this->pipeline);
}

void GraphicsPipeline::bindDescriptorSet(VkCommandBuffer commandBuffer, DescriptorSetArray* dsa, uint32_t index, uint32_t setidx) {
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, setidx, 1, &dsa->singleDescriptorSets[index].descriptorSet, 0, nullptr);
}

void GraphicsPipeline::destroy_back(VulkanContext* ctx) {
    if (this->vertexShaderModule != VK_NULL_HANDLE) {
        vkDestroyShaderModule(ctx->device, vertexShaderModule, nullptr);
    }
    if (this->fragmentShaderModule != VK_NULL_HANDLE) {
        vkDestroyShaderModule(ctx->device, fragmentShaderModule, nullptr);
    }
    if (this->pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(ctx->device, pipelineLayout, nullptr);
    }
    if (this->pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(ctx->device, this->pipeline, nullptr);
    }
}