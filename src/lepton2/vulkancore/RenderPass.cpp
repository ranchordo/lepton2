#include "RenderPass.h"

#include "VulkanContext.h"

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
    ret.isTerminal = false;
    ret.imageTiling = VK_IMAGE_TILING_OPTIMAL;
    ret.memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    ret.samples = samples;
    ret.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
    ret.format = format;
    ret.aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
    ret.blendState = blendState;
    return ret;
}
}  // namespace lepton2::vulkancore

RenderSubpass::RenderSubpass() {
    this->isTerminatingNode = false;
}

RenderSubpass::RenderSubpass(VulkanContext* ctx, TerminatingSubpassConfig config) {
    this->isTerminatingNode = true;

    RenderTargetImageCreationInfo rticInfo{};
    rticInfo.isTerminal = true;
    rticInfo.blendState = config.blendState;

    ColorAttachmentInfo info;
    info.desc = config.attachmentDescription;
    info.rticInfo = rticInfo;

    this->colorAttachments.push_back(info);
}

ColorAttachmentInfo* RenderSubpass::addColorAttachment(RenderTargetImageCreationInfo rticInfo, bool clear) {
    if (this->isTerminatingNode) {
        throw std::runtime_error("Color attachments of terminating nodes are predefined.");
    }
    rticInfo.isTerminal = false;
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

void RenderSubpass::connectFromNode(RenderSubpass* src, uint32_t color_output, uint32_t inputAttachmentIndex) {
    if (src->isTerminatingNode) {
        throw std::runtime_error("Cannot connect from a terminating node.");
    }
    if (color_output >= src->colorAttachments.size()) {
        throw std::runtime_error("Color output index doesn't exist for this node.");
    }
    std::pair<uint32_t, RenderSubpass*> ipair(color_output, src);
    this->inputs[inputAttachmentIndex] = ipair;
}

void RenderSubpass::requestDepthAsInput(uint32_t index) {
    this->depthInputRequest = index;
}

// FIXME: Avoid passing dslis - they contain vectors and stuff
void RenderSubpass::setupSubpassDescriptorSet(VulkanContext* ctx, RenderPass* parent, DescriptorSetLayoutInfo dsli) {
    if (this->subpassDsa != nullptr || this->subpassAttachmentDsa != nullptr) {
        throw std::runtime_error("Subpass DSA may only be set up once.");
    }

    if (dsli.bindings.size() > 0) {
        this->subpassDsa = new DescriptorSetArray(dsli);
        this->addLinkedResource(subpassDsa, true);
        this->subpassDsa->buildDescriptorSetLayout(ctx);
        ctx->descriptorPoolManager.allocateDescriptorSets(ctx, this->subpassDsa, parent->getResourceMultiplicity());
    }

    if (this->inputs.size() > 0 || this->depthInputRequest != UINT32_MAX) {
        DescriptorSetLayoutInfo attachDsli;
        uint32_t numInputAttachments = this->inputs.size() + ((this->depthInputRequest != UINT32_MAX) ? 1 : 0);
        uint32_t earliestSubpassIndex = this->getSubpassIndex();
        RenderSubpass* earliestSubpass = this;
        for (uint32_t i = 0; i < numInputAttachments; i++) {
            DescriptorInfo descInfo;
            if (i == this->depthInputRequest) {
                earliestSubpassIndex = 0;
                earliestSubpass = parent->getNode(0);
                descInfo.inputAttachmentData.depthAttachmentInfo = parent->getDepthAttachmentInfo();
            } else {
                std::pair<uint32_t, RenderSubpass*> pair = this->inputs[i];
                if (pair.second->getSubpassIndex() < earliestSubpassIndex) {
                    earliestSubpassIndex = pair.second->getSubpassIndex();
                    earliestSubpass = pair.second;
                }
                descInfo.inputAttachmentData.colorAttachmentInfo = &pair.second->colorAttachments[pair.first];
            }
            descInfo.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
            descInfo.shaderStages = VK_SHADER_STAGE_FRAGMENT_BIT;
            attachDsli.addNewBinding(descInfo);
        }
        this->subpassAttachmentDsa = new DescriptorSetArray(attachDsli);
        earliestSubpass->addLinkedResource(subpassAttachmentDsa, true);
        this->subpassAttachmentDsa->buildDescriptorSetLayout(ctx);
        if (parent->getNumFramebuffers() == 0) {
            throw std::runtime_error("Parent RenderPass has no framebuffers. Please initialize them.");
        }
        ctx->descriptorPoolManager.allocateDescriptorSets(ctx, this->subpassAttachmentDsa, parent->getNumFramebuffers());
    }

    std::vector<VkDescriptorSetLayout> dsls(parent->getSuperpassLayouts());
    if (parent->getPassDsa() != nullptr) {
        dsls.push_back(parent->getPassDsa()->descriptorSetLayout);
    }
    if (this->subpassDsa != nullptr) {
        dsls.push_back(this->subpassDsa->descriptorSetLayout);
    }
    if (this->subpassAttachmentDsa != nullptr) {
        dsls.push_back(this->subpassAttachmentDsa->descriptorSetLayout);
    }
    if (this->subpassDsa != nullptr || this->subpassAttachmentDsa != nullptr) {
        this->dummySubpassLayout = GraphicsPipeline::createPipelineLayout(ctx, dsls);
    }
}

void RenderSubpass::destroy_back(VulkanContext* ctx) {
    if (this->dummySubpassLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(ctx->device, this->dummySubpassLayout, nullptr);
    }
}

void RenderPass::begin(VkCommandBuffer commandBuffer, uint32_t framebufferIndex) {
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.pNext = nullptr;
    renderPassInfo.renderPass = renderPass;
    renderPassInfo.framebuffer = this->targets[framebufferIndex]->getFramebuffer();
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = this->targets[framebufferIndex]->getExtent();

    std::vector<VkClearValue> clearValues;
    clearValues.resize(this->clearValuePtrs.size());
    for (uint32_t i = 0; i < this->clearValuePtrs.size(); i++) {
        clearValues[i] = *this->clearValuePtrs[i];
    }
    renderPassInfo.clearValueCount = (uint32_t)clearValues.size();
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void RenderPass::preRenderAll(VulkanContext* ctx, uint32_t frameIndex) {
    for (uint32_t i = 0; i < this->nodes.size(); i++) {
        this->nodes[i]->getRenderCallback()->preRenderSubpass(ctx, frameIndex);
    }
}

void RenderPass::renderAll(VkCommandBuffer commandBuffer, uint32_t frameIndex, uint32_t framebufferIndex) {
    uint32_t baseSetidx = this->superpassLayouts.size();
    if (this->dummyPassLayout != VK_NULL_HANDLE) {
        GraphicsPipeline::bindDescriptorSet(commandBuffer, dummyPassLayout, passDsa, frameIndex, baseSetidx);
        baseSetidx++;
    }
    for (uint32_t i = 0; i < this->nodes.size(); i++) {
        RenderSubpass* node = this->nodes[i];
        if (i != 0) {
            vkCmdNextSubpass(commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
        }
        VkPipelineLayout dummySubpassLayout = node->getDummySubpassLayout();
        uint32_t setidx = baseSetidx;
        if (dummySubpassLayout != VK_NULL_HANDLE) {
            if (node->getSubpassDsa() != nullptr) {
                GraphicsPipeline::bindDescriptorSet(commandBuffer, dummySubpassLayout, node->getSubpassDsa(), frameIndex, setidx);
                setidx++;
            }
            if (node->getSubpassAttachmentDsa() != nullptr) {
                GraphicsPipeline::bindDescriptorSet(commandBuffer, dummySubpassLayout, node->getSubpassAttachmentDsa(), framebufferIndex, setidx);
                setidx++;
            }
        }
        if (node->getRenderCallback() != nullptr) {
            node->getRenderCallback()->renderSubpassCmd(commandBuffer, this, frameIndex, setidx);
        }
    }
}

void RenderPass::end(VkCommandBuffer buffer) {
    vkCmdEndRenderPass(buffer);
}

void RenderPass::destroy_back(VulkanContext* ctx) {
    this->destroyFramebuffers(ctx);
    if (this->renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(ctx->device, this->renderPass, nullptr);
    }
    vkDestroyPipelineLayout(ctx->device, dummyPassLayout, nullptr);
}

void RenderPass::generateFramebuffers(VulkanContext* ctx, std::vector<VulkanImage*>* finalImages, VkExtent2D extent) {
    this->targets.resize(finalImages->size());
    for (uint32_t i = 0; i < targets.size(); i++) {
        Framebuffer* nfb = new Framebuffer();
        for (RenderTargetImageCreationInfo rticInfo : this->rticInfos) {
            if (rticInfo.isTerminal) {
                nfb->addImage(finalImages->at(i));
            } else {
                VulkanImage* rtImage = new VulkanImage();
                createRenderTarget(ctx, extent, &rticInfo, rtImage);
                nfb->addImage(rtImage);
                this->renderTargetImages.push_back(rtImage);
                if (rticInfo.aspectFlags & VK_IMAGE_ASPECT_DEPTH_BIT) {
                    this->depthAttachmentInfo.depthTargetImages.push_back(rtImage);
                } else if (rticInfo.colorAttachmentIdx != UINT32_MAX && !rticInfo.isTerminal) {
                    nodes[rticInfo.nodeidx]->getColorAttachments()[rticInfo.colorAttachmentIdx].renderTargets.push_back(rtImage);
                }
            }
        }
        nfb->buildFramebuffer(ctx, renderPass, extent);
        this->targets[i] = nfb;
    }

    // Handle descriptor updates
    for (RenderSubpass* subpass : this->nodes) {
        DescriptorSetArray* dsa = subpass->getSubpassAttachmentDsa();
        if (dsa != nullptr) {
            dsa->updateAllDescriptorSets(ctx);
        }
    }
}

void RenderPass::destroyFramebuffers(VulkanContext* ctx) {
    this->depthAttachmentInfo.depthTargetImages.clear();
    for (RenderTargetImageCreationInfo rticInfo : this->rticInfos) {
        if (rticInfo.colorAttachmentIdx != UINT32_MAX && !rticInfo.isTerminal) {
            nodes[rticInfo.nodeidx]->getColorAttachments()[rticInfo.colorAttachmentIdx].renderTargets.clear();
        }
    }
    for (Framebuffer* fb : this->targets) {
        fb->destroy(ctx);
        delete fb;
    }
    this->targets.clear();
    for (VulkanImage* img : this->renderTargetImages) {
        img->destroy(ctx);
        delete img;
    }
    this->renderTargetImages.clear();
}

void RenderPass::setSuperpassLayouts(std::vector<VkDescriptorSetLayout> _superpassLayouts) {
    if (this->superpassLayouts.size() > 0) {
        throw std::runtime_error("Superpass layouts may only be set once.");
    }
    if (this->passDsa != nullptr) {
        throw std::runtime_error("RenderPass DSA must be set up after superpass DSA setup.");
    }
    for (uint32_t i = 0; i < this->nodes.size(); i++) {
        if (nodes[i]->getSubpassDsa() != nullptr) {
            throw std::runtime_error("Subpass DSA must be set up after setting superpass layouts.");
        }
    }
    this->superpassLayouts = _superpassLayouts;
}

void RenderPass::setupPassDescriptorSet(VulkanContext* ctx, DescriptorSetLayoutInfo dsli) {
    if (this->passDsa != nullptr) {
        throw std::runtime_error("RenderPass DSA may only be set up once.");
    }
    for (uint32_t i = 0; i < this->nodes.size(); i++) {
        if (nodes[i]->getSubpassDsa() != nullptr) {
            throw std::runtime_error("Subpass DSA must be set up after parent RenderPass DSA setup.");
        }
    }
    this->passDsa = new DescriptorSetArray(dsli);
    this->addLinkedResource(passDsa, true);
    this->passDsa->buildDescriptorSetLayout(ctx);
    ctx->descriptorPoolManager.allocateDescriptorSets(ctx, this->passDsa, this->getResourceMultiplicity());

    std::vector<VkDescriptorSetLayout> dsls(this->superpassLayouts);
    dsls.push_back(this->passDsa->descriptorSetLayout);
    this->dummyPassLayout = GraphicsPipeline::createPipelineLayout(ctx, dsls);
}

#define NODE_MARK_TEMPORARY 1
#define NODE_MARK_EXHAUSTED 2

void RenderSubpass::topologicalSort(std::vector<RenderSubpass*>* orderedNodes) {
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

static void addSubpassDependency(std::vector<VkSubpassDependency>& subpassDependencies, VkSubpassDependency dependency) {
    for (uint32_t j = 0; j < subpassDependencies.size(); j++) {
        VkSubpassDependency* depPtr = &subpassDependencies[j];
        if (depPtr->srcSubpass == dependency.srcSubpass && depPtr->dstSubpass == dependency.dstSubpass) {
            depPtr->srcStageMask |= dependency.srcStageMask;
            depPtr->dstStageMask |= dependency.dstStageMask;
            depPtr->srcAccessMask |= dependency.srcAccessMask;
            depPtr->dstAccessMask |= dependency.dstAccessMask;
            depPtr->dependencyFlags |= dependency.dependencyFlags;
            return;
        }
    }
    subpassDependencies.push_back(dependency);
}

RenderPass::RenderPass(VulkanContext* ctx, const std::vector<RenderSubpass*>& _nodes, uint32_t _resourceMultiplicity) {
    this->resourceMultiplicity = _resourceMultiplicity;

    // Subpass wrangling
    std::vector<RenderSubpass*> orderedNodes;
    for (RenderSubpass* node : _nodes) node->markings = 0;
    while (orderedNodes.size() < _nodes.size()) {
        for (RenderSubpass* node : _nodes) {
            if (!(node->markings & NODE_MARK_EXHAUSTED)) node->topologicalSort(&orderedNodes);
        }
    }
    this->nodes = orderedNodes;
    uint32_t numTerminatingNodes = 0;
    for (uint32_t i = 0; i < this->nodes.size(); i++) {
        if (this->nodes[i]->isTerminatingNode) numTerminatingNodes++;
        this->nodes[i]->nodeIndex = i;
    }

    if (numTerminatingNodes != 1) {
        throw std::runtime_error("RenderPasses must contain exactly one terminating node.");
    }

    // Initialize attachments
    std::vector<VkAttachmentDescription> attachments;

    // Depth/stencil creation
    RenderTargetImageCreationInfo depthStencilCreationInfo{};
    depthStencilCreationInfo.format = VK_FORMAT_D32_SFLOAT_S8_UINT;
    depthStencilCreationInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    depthStencilCreationInfo.imageTiling = VK_IMAGE_TILING_OPTIMAL;
    depthStencilCreationInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
    depthStencilCreationInfo.memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    depthStencilCreationInfo.aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
    depthStencilCreationInfo.isTerminal = false;
    depthStencilCreationInfo.nodeidx = UINT32_MAX;
    depthStencilCreationInfo.colorAttachmentIdx = UINT32_MAX;

    VkAttachmentDescription depthStencilAttachment{};
    depthStencilAttachment.format = depthStencilCreationInfo.format;
    depthStencilAttachment.samples = depthStencilCreationInfo.samples;
    depthStencilAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthStencilAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthStencilAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthStencilAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthStencilAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthStencilAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    uint32_t depthStencilAttachmentIndex = attachments.size();
    attachments.push_back(depthStencilAttachment);
    this->rticInfos.push_back(depthStencilCreationInfo);
    this->clearValuePtrs.push_back(&this->depthStencilClearValue);

    // Rest of attachments for each node
    for (uint32_t nodeidx = 0; nodeidx < this->nodes.size(); nodeidx++) {
        RenderSubpass* node = this->nodes[nodeidx];
        node->subpassInfo.colorAttachmentReferences.resize(node->colorAttachments.size());
        for (uint32_t i = 0; i < node->colorAttachments.size(); i++) {
            node->subpassInfo.colorAttachmentReferences[i].attachment = attachments.size();
            node->subpassInfo.colorAttachmentReferences[i].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            node->colorAttachments[i].rticInfo.nodeidx = nodeidx;
            node->colorAttachments[i].rticInfo.colorAttachmentIdx = i;
            this->rticInfos.push_back(node->colorAttachments[i].rticInfo);
            this->clearValuePtrs.push_back(&node->colorAttachments[i].clearValue);
            attachments.push_back(node->colorAttachments[i].desc);
        }
        node->subpassInfo.depthStencilAttachmentReference.attachment = depthStencilAttachmentIndex;
        if (node->depthInputRequest == UINT32_MAX) {
            node->subpassInfo.depthStencilAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        } else {
            node->subpassInfo.depthStencilAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        }
    }
    // Now all nodes have their VkAttachmentReferences built

    // Build subpass descriptions:
    std::vector<VkSubpassDescription> subpasses;
    for (uint32_t i = 0; i < this->nodes.size(); i++) {
        RenderSubpass* node = this->nodes[i];
        VkSubpassDescription subpassDescription{};
        subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpassDescription.colorAttachmentCount = node->colorAttachments.size();
        subpassDescription.pColorAttachments = node->subpassInfo.colorAttachmentReferences.data();
        uint32_t inputAttachmentSize = 0;
        for (std::pair<uint32_t, std::pair<uint32_t, RenderSubpass*>> pair : node->inputs) {
            if ((pair.first + 1) > inputAttachmentSize) {
                inputAttachmentSize = pair.first + 1;
            }
            if (node->depthInputRequest != UINT32_MAX && (node->depthInputRequest + 1) > inputAttachmentSize) {
                inputAttachmentSize = node->depthInputRequest + 1;
            }
        }
        node->subpassInfo.inputAttachmentReferences.resize(inputAttachmentSize);
        for (std::pair<uint32_t, std::pair<uint32_t, RenderSubpass*>> pair : node->inputs) {
            uint32_t inputAttachment = pair.second.second->subpassInfo.colorAttachmentReferences[pair.second.first].attachment;
            node->subpassInfo.inputAttachmentReferences[pair.first] = {inputAttachment, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        }
        if (node->depthInputRequest != UINT32_MAX) {
            VkAttachmentReference ref = {depthStencilAttachmentIndex, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL};
            node->subpassInfo.inputAttachmentReferences[node->depthInputRequest] = ref;
        }
        subpassDescription.inputAttachmentCount = node->subpassInfo.inputAttachmentReferences.size();
        subpassDescription.pInputAttachments = node->subpassInfo.inputAttachmentReferences.data();
        subpassDescription.pDepthStencilAttachment = &node->subpassInfo.depthStencilAttachmentReference;
        subpasses.push_back(subpassDescription);
    }

    // Build the render pass creation info
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.pNext = nullptr;
    renderPassInfo.attachmentCount = attachments.size();
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = subpasses.size();
    renderPassInfo.pSubpasses = subpasses.data();

    // Construct subpass dependencies
    std::vector<VkSubpassDependency> subpassDependencies;
    for (uint32_t i = 0; i < this->nodes.size(); i++) {
        RenderSubpass* node = this->nodes[i];
        for (std::pair<uint32_t, std::pair<uint32_t, RenderSubpass*>> p : node->inputs) {
            VkSubpassDependency dependency{};
            dependency.srcSubpass = p.second.second->nodeIndex;
            dependency.dstSubpass = node->nodeIndex;
            dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependency.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            dependency.dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
            dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
            addSubpassDependency(subpassDependencies, dependency);
        }
        if (node->depthInputRequest != UINT32_MAX) {
            uint32_t srcSubpass = UINT32_MAX;
            for (uint32_t i = node->nodeIndex - 1; i < node->nodeIndex; i--) {
                if (nodes[i]->depthInputRequest == UINT32_MAX) srcSubpass = i;
            }
            if (srcSubpass != UINT32_MAX) {
                VkSubpassDependency dependency{};
                dependency.srcSubpass = srcSubpass;
                dependency.dstSubpass = node->nodeIndex;
                dependency.srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
                dependency.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
                dependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                dependency.dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
                dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
                addSubpassDependency(subpassDependencies, dependency);
            }
        }
    }
    renderPassInfo.dependencyCount = subpassDependencies.size();
    renderPassInfo.pDependencies = subpassDependencies.data();

    if (vkCreateRenderPass(ctx->device, &renderPassInfo, nullptr, &this->renderPass) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create render pass.");
    }
}

TerminatingSubpassConfig RenderSubpass::getDefaultTerminatingConfig(VkFormat imageFormat, VkImageLayout finalLayout) {
    VkAttachmentDescription desc{};
    desc.format = imageFormat;
    desc.samples = VK_SAMPLE_COUNT_1_BIT;
    desc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    desc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    desc.finalLayout = finalLayout;

    VkPipelineColorBlendAttachmentState blendState{};
    blendState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blendState.blendEnable = VK_TRUE;
    blendState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blendState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendState.colorBlendOp = VK_BLEND_OP_ADD;
    blendState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blendState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    blendState.alphaBlendOp = VK_BLEND_OP_ADD;

    TerminatingSubpassConfig config{};
    config.attachmentDescription = desc;
    config.blendState = blendState;
    return config;
}