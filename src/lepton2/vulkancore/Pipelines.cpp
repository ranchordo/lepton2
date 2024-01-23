#include "Pipelines.h"

#include "ObjectData.h"
#include "RenderState.h"
#include "VulkanContext.h"

using namespace lepton2::vulkancore;

PipelineInfo::PipelineInfo(const char* _shaderName,
                           DescriptorSetLayoutInfo _dsaLayout,
                           DescriptorSetArray* _dsaLayoutReference,
                           VkSampleCountFlagBits _samples, VkBool32 _useStencilTesting,
                           VkStencilOpState _stencilState, VkPolygonMode _polygonMode,
                           VkFrontFace _frontFace, VkCullModeFlags _cullMode) {
    this->shaderName = _shaderName;
    this->dsaLayout = _dsaLayout;
    this->samples = _samples;
    this->useStencilTesting = _useStencilTesting;
    this->stencilState = _stencilState;
    this->polygonMode = _polygonMode;
    this->frontFace = _frontFace;
    this->cullMode = _cullMode;
    if (_dsaLayoutReference != nullptr) {
        this->descriptorSetLayout = _dsaLayoutReference->descriptorSetLayout;
    }
}

PipelineInfo::PipelineInfo(const PipelineInfo& other) {
    this->shaderName = other.shaderName;
    this->dsaLayout = other.dsaLayout;
    this->samples = other.samples;
    this->useStencilTesting = other.useStencilTesting;
    this->stencilState = other.stencilState;
    this->polygonMode = other.polygonMode;
    this->frontFace = other.frontFace;
    this->cullMode = other.cullMode;
    this->descriptorSetLayout = other.descriptorSetLayout;
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

GraphicsPipeline::GraphicsPipeline(VulkanContext* ctx, uint32_t subpassIndex, VkRenderPass renderPass, PipelineInfo cInfo) : creationInfo(cInfo) {
    this->creationInfo.descriptorSetLayout = VK_NULL_HANDLE;  // Don't keep old pointers
    size_t combined_length = snprintf(nullptr, 0, "%s/%s.vert.spv", ctx->shaders_spirv_load_path, cInfo.shaderName);
    char filename_buffer[combined_length + 1];
    snprintf(filename_buffer, combined_length + 1, "%s/%s.vert.spv", ctx->shaders_spirv_load_path, cInfo.shaderName);
    std::vector<char> vertex_code = readFile(std::string(filename_buffer));
    this->vertexShaderModule = this->buildShaderModule(ctx, vertex_code);
    snprintf(filename_buffer, combined_length + 1, "%s/%s.frag.spv", ctx->shaders_spirv_load_path, cInfo.shaderName);
    std::vector<char> fragment_code = readFile(std::string(filename_buffer));
    this->fragmentShaderModule = this->buildShaderModule(ctx, fragment_code);

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
    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = (uint32_t)(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();
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
    rasterizer.polygonMode = cInfo.polygonMode;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = cInfo.cullMode;
    rasterizer.frontFace = cInfo.frontFace;
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.depthBiasConstantFactor = 0.0f;
    rasterizer.depthBiasClamp = 0.0f;
    rasterizer.depthBiasSlopeFactor = 0.0f;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = cInfo.samples;
    multisampling.minSampleShading = 1.0f;
    multisampling.pSampleMask = nullptr;
    multisampling.alphaToCoverageEnable = VK_FALSE;
    multisampling.alphaToOneEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges = nullptr;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &cInfo.descriptorSetLayout;

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
    depthStencil.stencilTestEnable = cInfo.useStencilTesting;
    depthStencil.back = cInfo.stencilState;
    depthStencil.front = cInfo.stencilState;

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
    pipelineInfo.subpass = subpassIndex;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = -1;

    if (vkCreateGraphicsPipelines(ctx->device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create graphics pipeline.");
    }
}

void GraphicsPipeline::bind(VkCommandBuffer commandBuffer) {
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, this->pipeline);
}

void GraphicsPipeline::bindDescriptorSet(VkCommandBuffer commandBuffer, DescriptorSetArray* dsa, uint32_t index) {
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &dsa->singleDescriptorSets[index].descriptorSet, 0, nullptr);
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