#include "RenderState.h"

using namespace lepton2::vulkancore;

namespace lepton2::vulkancore {
RenderTargetImageCreationInfo defaultColorAttachmentRTIC(VkFormat format, VkSampleCountFlagBits samples) {
    VkPipelineColorBlendAttachmentState blendState{};
    blendState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blendState.blendEnable = VK_TRUE;
    blendState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blendState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendState.colorBlendOp = VK_BLEND_OP_ADD;
    blendState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blendState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    blendState.alphaBlendOp = VK_BLEND_OP_ADD;
    RenderTargetImageCreationInfo ret{};
    ret.use_presenter = false;
    ret.imageTiling = VK_IMAGE_TILING_OPTIMAL;
    ret.memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    ret.samples = samples;
    ret.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
    ret.format = format;
    ret.aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
    ret.blendState = blendState;
    return ret;
}
}  // namespace lepton2::vulkancore

RenderGraphNode::RenderGraphNode() {
    this->isTerminatingNode = false;
}

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
    RenderTargetImageCreationInfo rticInfo{};
    rticInfo.use_presenter = true;
    VkPipelineColorBlendAttachmentState blendState{};
    // FIXME: Give some option to override these defaults
    // Maybe a more manual terminating node construction method?
    blendState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blendState.blendEnable = VK_TRUE;
    blendState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blendState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendState.colorBlendOp = VK_BLEND_OP_ADD;
    blendState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blendState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    blendState.alphaBlendOp = VK_BLEND_OP_ADD;
    rticInfo.blendState = blendState;
    ColorAttachmentInfo info;
    info.desc = desc;
    info.rticInfo = rticInfo;

    this->colorAttachments.push_back(info);
}

ColorAttachmentInfo* RenderGraphNode::addColorAttachment(RenderTargetImageCreationInfo rticInfo, bool clear) {
    if (this->isTerminatingNode) {
        throw std::runtime_error("Color attachments of terminating nodes in the RenderGraph are predefined.");
    }
    rticInfo.use_presenter = false;
    VkAttachmentDescription desc{};
    desc.format = rticInfo.format;
    desc.samples = rticInfo.samples;
    desc.loadOp = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
    desc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    desc.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    ColorAttachmentInfo info;
    info.desc = desc;
    info.rticInfo = rticInfo;

    this->colorAttachments.push_back(info);
    return &this->colorAttachments.at(this->colorAttachments.size() - 1);
}

void RenderGraphNode::connectFromNode(RenderGraphNode* src, uint32_t color_output, uint32_t inputAttachmentIndex) {
    if (src->isTerminatingNode) {
        throw std::runtime_error("Cannot connect from a terminating node.");
    }
    if (color_output >= src->colorAttachments.size()) {
        throw std::runtime_error("Color output index doesn't exist for this node.");
    }
    std::pair<uint32_t, RenderGraphNode*> ipair(color_output, src);
    this->inputs[inputAttachmentIndex] = ipair;
}

void RenderGraphNode::requestDepthAsInput(uint32_t index) {
    this->depthInputRequest = index;
}

void RenderGraphNode::setupSubpassDescriptorSet(VulkanContext* ctx, DescriptorSetLayoutInfo dsli) {
    this->removeSubpassDescriptorSet(ctx);
    this->subpassDsa = new DescriptorSetArray(dsli);
    this->addLinkedResource(subpassDsa, true);
    this->subpassDsa->buildDescriptorSetLayout(ctx);
    ctx->descriptorPoolManager.allocateDescriptorSets(ctx, this->subpassDsa, ctx->swapChain.swapChainImages.size());
}

void RenderGraphNode::removeSubpassDescriptorSet(VulkanContext* ctx) {
    if (this->subpassDsa != nullptr) {
        this->removeLinkedResource(this->subpassDsa);
        this->subpassDsa->destroy(ctx);
        delete this->subpassDsa;
        this->subpassDsa = nullptr;
    }
}

void RenderGraphNode::destroy_back(VulkanContext* ctx) {
    // Nothing get
}

RenderGraphNode* RenderGraph::buildNewNode() {
    RenderGraphNode* node = new RenderGraphNode();
    this->nodes.push_back(node);
    return node;
}

void RenderState::begin(VkCommandBuffer commandBuffer, SwapChainFrame swapChainFrame) {
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass;
    renderPassInfo.framebuffer = swapChainFrame.framebuffer->framebuffer;
    renderPassInfo.renderArea.offset = {0, 0};
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
    if (this->passDsa != nullptr) {
        this->dsaQueue.push_back({2, this->passDsa});
    }
}

void RenderState::renderAll(VkCommandBuffer commandBuffer, SwapChainFrame swapChainFrame) {
    for (uint32_t i = 0; i < this->nodes.size(); i++) {
        RenderGraphNode* node = this->nodes[i];
        if (i != 0) {
            vkCmdNextSubpass(commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
        }
        if (node->getSubpassDsa() != nullptr) {
            this->dsaQueue.push_back({1, node->getSubpassDsa()});
        }
        if (node->getRenderCallback() != nullptr) {
            node->getRenderCallback()->renderSubpassCmd(commandBuffer, this, swapChainFrame.index);
        }
        // node->configurationStore.renderAllConfigurations(this, commandBuffer, swapChainFrame.index);
    }
}

void RenderState::end(VkCommandBuffer buffer) {
    vkCmdEndRenderPass(buffer);
}

void RenderState::destroy_back(VulkanContext* ctx) {
    if (this->renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(ctx->device, this->renderPass, nullptr);
    }
}

void RenderState::setupPassDescriptorSet(DescriptorSetLayoutInfo dsli) {
    this->removePassDescriptorSet();
    this->passDsa = new DescriptorSetArray(dsli);
    this->addLinkedResource(passDsa, true);
    this->passDsa->buildDescriptorSetLayout(ctx);
    ctx->descriptorPoolManager.allocateDescriptorSets(ctx, this->passDsa, ctx->swapChain.swapChainImages.size());
}

void RenderState::removePassDescriptorSet() {
    if (this->passDsa != nullptr) {
        this->removeLinkedResource(this->passDsa);
        this->passDsa->destroy(ctx);
        delete this->passDsa;
        this->passDsa = nullptr;
    }
}

RenderGraph::RenderGraph(VulkanContext* ctx) {
    this->ctx = ctx;
}

RenderGraphNode* RenderGraph::buildPresentingNode() {
    if (this->terminator != nullptr) {
        throw std::runtime_error("Cannot replace terminating node");
    }
    this->terminator = new RenderGraphNode(ctx, true);
    this->nodes.push_back(this->terminator);
    return this->terminator;
}

#define NODE_MARK_TEMPORARY 1
#define NODE_MARK_EXHAUSTED 2

void RenderGraphNode::topologicalSort(std::vector<RenderGraphNode*>* orderedNodes) {
    if (this->markings & NODE_MARK_EXHAUSTED) {
        return;
    }
    if (this->markings & NODE_MARK_TEMPORARY) {
        throw std::runtime_error("Topological sort: cycle detected in render graph.");
    }
    this->markings |= NODE_MARK_TEMPORARY;
    for (auto pair : this->inputs) {
        pair.second.second->topologicalSort(orderedNodes);
    }
    this->markings &= (~NODE_MARK_TEMPORARY);
    this->markings |= NODE_MARK_EXHAUSTED;
    orderedNodes->push_back(this);
}

RenderState* RenderGraph::buildRenderState() {
    // Commence topological sort
    for (RenderGraphNode* node : this->nodes) {
        node->markings = 0;
    }
    std::vector<RenderGraphNode*> orderedNodes;
    while (orderedNodes.size() < this->nodes.size()) {
        for (RenderGraphNode* node : this->nodes) {
            if (!(node->markings & NODE_MARK_EXHAUSTED)) {
                node->topologicalSort(&orderedNodes);
            }
        }
    }
    this->nodes = orderedNodes;
    // Alright, carry on
    for (uint32_t i = 0; i < this->nodes.size(); i++) {
        this->nodes[i]->nodeIndex = i;
    }
    RenderState* finalState = new RenderState();
    std::vector<VkAttachmentDescription> attachments;

    // Depth/stencil creation
    RenderTargetImageCreationInfo depthStencilCreationInfo{};
    depthStencilCreationInfo.format = VK_FORMAT_D32_SFLOAT_S8_UINT;
    depthStencilCreationInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    depthStencilCreationInfo.imageTiling = VK_IMAGE_TILING_OPTIMAL;
    depthStencilCreationInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    depthStencilCreationInfo.memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    depthStencilCreationInfo.aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
    depthStencilCreationInfo.use_presenter = false;
    depthStencilCreationInfo.creationTracker = nullptr;
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
    // Rest of attachments for each node
    for (uint32_t i = 0; i < this->nodes.size(); i++) {
        RenderGraphNode* node = this->nodes[i];
        node->subpassInfo.colorAttachmentReferences.resize(node->colorAttachments.size());
        for (uint32_t i = 0; i < node->colorAttachments.size(); i++) {
            node->subpassInfo.colorAttachmentReferences[i].attachment = attachments.size();
            node->subpassInfo.colorAttachmentReferences[i].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            if (!node->colorAttachments[i].rticInfo.use_presenter) {
                node->colorAttachments[i].rticInfo.creationTracker = &node->colorAttachments[i].swapChainCreations;
            }
            finalState->rticInfos.push_back(node->colorAttachments[i].rticInfo);
            finalState->clearValuePtrs.push_back(&node->colorAttachments[i].clearValue);
            attachments.push_back(node->colorAttachments[i].desc);
        }
        node->subpassInfo.depthStencilAttachmentReference.attachment = depthStencilAttachmentIndex;
        node->subpassInfo.depthStencilAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }
    // Now all nodes have their VkAttachmentReferences built
    // Build subpass descriptions:
    std::vector<VkSubpassDescription> subpasses;
    for (uint32_t i = 0; i < this->nodes.size(); i++) {
        RenderGraphNode* node = this->nodes[i];
        VkSubpassDescription subpassDescription{};
        subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpassDescription.colorAttachmentCount = node->colorAttachments.size();
        subpassDescription.pColorAttachments = node->subpassInfo.colorAttachmentReferences.data();
        uint32_t inputAttachmentSize = 0;
        for (std::pair<uint32_t, std::pair<uint32_t, RenderGraphNode*>> pair : node->inputs) {
            if ((pair.first + 1) > inputAttachmentSize) {
                inputAttachmentSize = pair.first + 1;
            }
        }
        node->subpassInfo.inputAttachmentReferences.resize(inputAttachmentSize);
        for (std::pair<uint32_t, std::pair<uint32_t, RenderGraphNode*>> pair : node->inputs) {
            uint32_t inputAttachment = pair.second.second->subpassInfo.colorAttachmentReferences[pair.second.first].attachment;
            node->subpassInfo.inputAttachmentReferences[pair.first] = {inputAttachment, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        }
        if (node->depthInputRequest != UINT32_MAX) {
            node->subpassInfo.inputAttachmentReferences[node->depthInputRequest] = {depthStencilAttachmentIndex, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL};
        }
        subpassDescription.inputAttachmentCount = node->subpassInfo.inputAttachmentReferences.size();
        subpassDescription.pInputAttachments = node->subpassInfo.inputAttachmentReferences.data();
        subpassDescription.pDepthStencilAttachment = &node->subpassInfo.depthStencilAttachmentReference;
        subpasses.push_back(subpassDescription);
    }
    // Build the render pass creation info
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = attachments.size();
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = subpasses.size();
    renderPassInfo.pSubpasses = subpasses.data();
    // Construct subpass dependencies
    std::vector<VkSubpassDependency> subpassDependencies;
    for (uint32_t i = 0; i < this->nodes.size(); i++) {
        RenderGraphNode* node = this->nodes[i];
        for (std::pair<uint32_t, std::pair<uint32_t, RenderGraphNode*>> p : node->inputs) {
            VkSubpassDependency dependency{};
            dependency.srcSubpass = p.second.second->nodeIndex;
            dependency.dstSubpass = node->nodeIndex;
            for (VkSubpassDependency dep : subpassDependencies) {
                if (dep.srcSubpass == dependency.srcSubpass && dep.dstSubpass == dependency.dstSubpass) {
                    dependency.srcSubpass = UINT32_MAX;
                    break;
                }
            }
            if (dependency.srcSubpass == UINT32_MAX) {
                continue; // Filter duplicate dependencies
            }
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
    // Build render pass, pass to final state
    VkRenderPass* dstRenderPass = &finalState->renderPass;
    if (vkCreateRenderPass(ctx->device, &renderPassInfo, nullptr, dstRenderPass) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create render pass.");
    }
    finalState->ctx = this->ctx;
    finalState->nodes = this->nodes;
    return finalState;
}

void RenderGraph::destroy_back(VulkanContext* ctx) {
    this->nodes.clear();
    // User must destroy all gotten nodes
}