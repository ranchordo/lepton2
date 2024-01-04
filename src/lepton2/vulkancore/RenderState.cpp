#include "RenderState.h"

using namespace lepton2::vulkancore;

RenderGraphNode::RenderGraphNode() {}

RenderGraphNode::RenderGraphNode(VulkanContext* ctx, bool isTerminatingNode) {
    if (!isTerminatingNode) {
        throw std::runtime_error("Need to construct a RenderGraphNode with isTerminatingNode==1 for this constructor.");
    }
    this->isTerminatingNode = true;

    VkAttachmentDescription desc{};
    desc.format = ctx->swapChain.swapChainImageFormat;
    desc.samples = VK_SAMPLE_COUNT_1_BIT;
    bool clear = ctx->swapChain.clearSwapChain;
    desc.loadOp = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
    desc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    desc.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    RenderTargetImageCreationInfo rticInfo;
    rticInfo.use_swapchain = true;
    ColorAttachmentInfo info;
    info.desc = desc;
    info.rticInfo = rticInfo;

    this->colorAttachments.push_back(info);
}

void RenderGraphNode::addColorAttachment(RenderTargetImageCreationInfo rticInfo, bool clear) {
    if (this->isTerminatingNode) {
        throw std::runtime_error("Color attachments of presenting/terminating nodes in the RenderGraph are predefined.");
    }
    VkAttachmentDescription desc{};
    desc.format = rticInfo.format;
    desc.samples = rticInfo.samples;
    desc.loadOp = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
    desc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    desc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    ColorAttachmentInfo info;
    info.desc = desc;
    info.rticInfo = rticInfo;

    this->colorAttachments.push_back(info);
}

void RenderGraphNode::connectToNode(uint32_t color_output, RenderGraphNode* node) {
    if (this->isTerminatingNode) {
        throw std::runtime_error("Cannot connect the terminating node anywhere.");
    }
    if (color_output >= this->colorAttachments.size()) {
        throw std::runtime_error("Color output index doesn't exist for this node.");
    }
    std::pair<uint32_t, RenderGraphNode*> opair(color_output, node);
    std::pair<uint32_t, RenderGraphNode*> ipair(color_output, this);
    this->outputs.push_back(opair);
    node->inputs.push_back(ipair);
}

void RenderGraphNode::destroy_back(VulkanContext* ctx) {
    // So far nothing
}

RenderGraphNode* RenderGraph::buildNewNode() {
    RenderGraphNode* node = new RenderGraphNode();
    node->nodeIndex = this->nodes.size();
    this->nodes.push_back(node);
    return node;
}

void RenderState::addPipeline(std::string key, PipelineInfo cInfo) {
    GraphicsPipeline* pipeline = new GraphicsPipeline(this->ctx, this->renderPass, cInfo);
    this->pipelines[key] = pipeline;
}

GraphicsPipeline* RenderState::getPipeline(std::string key) {
    return this->pipelines[key];
}

void RenderState::bind(VkCommandBuffer commandBuffer, SwapChainFrame swapChainFrame) {
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass;
    renderPassInfo.framebuffer = swapChainFrame.framebuffer->framebuffer;
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = ctx->swapChain.swapChainExtent;

    std::vector<VkClearValue> clearValues;
    clearValues.resize(this->clearValuePtrs.size());
    for (uint32_t i = 0; i < this->clearValuePtrs.size(); i++) {
        // De-reference clearvalue
        clearValues[i] = *this->clearValuePtrs[i];
    }
    renderPassInfo.clearValueCount = (uint32_t)clearValues.size();
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void RenderState::destroy_back(VulkanContext* ctx) {
    if (this->renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(ctx->device, this->renderPass, nullptr);
    }
    for (auto const& p : this->pipelines) {
        p.second->destroy(ctx);
        delete p.second;
    }
    this->pipelines.clear();
}

RenderGraph::RenderGraph(VulkanContext* ctx) {
    this->ctx = ctx;
    this->terminator = new RenderGraphNode(ctx, true);
    this->terminator->nodeIndex = this->nodes.size();
    this->nodes.push_back(this->terminator);
}

RenderState* RenderGraph::buildRenderState() {
    RenderState* finalState = new RenderState();
    std::vector<VkAttachmentDescription> attachments;

    RenderTargetImageCreationInfo depthStencilCreationInfo{};
    depthStencilCreationInfo.format = VK_FORMAT_D32_SFLOAT_S8_UINT;
    depthStencilCreationInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    depthStencilCreationInfo.imageTiling = VK_IMAGE_TILING_OPTIMAL;
    depthStencilCreationInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    depthStencilCreationInfo.memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    depthStencilCreationInfo.aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
    depthStencilCreationInfo.use_swapchain = false;
    VkAttachmentDescription depthStencilAttachment{};
    depthStencilAttachment.format = depthStencilCreationInfo.format;
    depthStencilAttachment.samples = depthStencilCreationInfo.samples;
    depthStencilAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthStencilAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthStencilAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthStencilAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthStencilAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthStencilAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    uint32_t depthStencilAttachmentIndex = attachments.size();
    attachments.push_back(depthStencilAttachment);
    finalState->rticInfos.push_back(depthStencilCreationInfo);
    finalState->clearValuePtrs.push_back(&finalState->depthStencilClearValue);
    for (uint32_t i = 0; i < this->nodes.size(); i++) {
        RenderGraphNode* node = this->nodes[i];
        node->subpassInfo.colorAttachmentReferences.resize(node->colorAttachments.size());
        for (uint32_t i = 0; i < node->colorAttachments.size(); i++) {
            node->subpassInfo.colorAttachmentReferences[i].attachment = attachments.size();
            node->subpassInfo.colorAttachmentReferences[i].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            finalState->rticInfos.push_back(node->colorAttachments[i].rticInfo);
            finalState->clearValuePtrs.push_back(&node->colorAttachments[i].clearValue);
            attachments.push_back(node->colorAttachments[i].desc);
        }
        node->subpassInfo.depthStencilAttachmentReference.attachment = depthStencilAttachmentIndex;
        node->subpassInfo.depthStencilAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }
    // Now all nodes have their VkAttachmentReferences built
    std::vector<VkSubpassDescription> subpasses;
    for (uint32_t i = 0; i < this->nodes.size(); i++) {
        RenderGraphNode* node = this->nodes[i];
        VkSubpassDescription subpassDescription{};
        subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpassDescription.colorAttachmentCount = node->colorAttachments.size();
        subpassDescription.pColorAttachments = node->subpassInfo.colorAttachmentReferences.data();
        for (std::pair<uint32_t, RenderGraphNode*> p : node->inputs) {
            VkAttachmentReference inputAttachmentReference = p.second->subpassInfo.colorAttachmentReferences[p.first];
            node->subpassInfo.inputAttachmentReferences.push_back(inputAttachmentReference);
        }
        subpassDescription.inputAttachmentCount = node->subpassInfo.inputAttachmentReferences.size();
        subpassDescription.pInputAttachments = node->subpassInfo.inputAttachmentReferences.data();
        subpassDescription.pDepthStencilAttachment = &node->subpassInfo.depthStencilAttachmentReference;
        subpasses.push_back(subpassDescription);
    }
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = attachments.size();
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = subpasses.size();
    renderPassInfo.pSubpasses = subpasses.data();

    std::vector<VkSubpassDependency> subpassDependencies;
    for (uint32_t i = 0; i < this->nodes.size(); i++) {
        RenderGraphNode* node = this->nodes[i];
        for (std::pair<uint32_t, RenderGraphNode*> p : node->inputs) {
            VkSubpassDependency dependency{};
            dependency.srcSubpass = p.second->nodeIndex;
            dependency.dstSubpass = node->nodeIndex;
            dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependency.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            dependency.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
            subpassDependencies.push_back(dependency);
        }
    }

    renderPassInfo.dependencyCount = subpassDependencies.size();
    renderPassInfo.pDependencies = subpassDependencies.data();

    VkRenderPass* dstRenderPass = &finalState->renderPass;
    if (vkCreateRenderPass(ctx->device, &renderPassInfo, nullptr, dstRenderPass) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create render pass.");
    }
    finalState->ctx = this->ctx;
    return finalState;
}

void RenderGraph::destroy_back(VulkanContext* ctx) {
    this->nodes.clear();
    // User must destroy all gotten nodes
}