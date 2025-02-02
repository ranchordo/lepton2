#pragma once

#include "Framebuffer.h"
#include "GraphicalEntity.h"
#include "Pipelines.h"
#include "SwapChain.h"
#include "VulkanContext.h"
#include "VulkanUtils.h"

namespace lepton2::vulkancore {
struct RenderTargetImageCreationInfo {
    bool use_presenter = false;
    VkFormat format;
    VkSampleCountFlagBits samples;
    VkImageTiling imageTiling;
    VkImageUsageFlags usage;
    VkMemoryPropertyFlags memoryProperties;
    VkImageAspectFlags aspectFlags;
    std::vector<VulkanImage*>* creationTracker;
};

struct ColorAttachmentInfo {
    VkAttachmentDescription desc;
    RenderTargetImageCreationInfo rticInfo;
    VkClearValue clearValue = {0.0f, 0.0f, 0.0f, 0.0f};
    std::vector<VulkanImage*> swapChainCreations;
};

class RenderState;
class RenderGraphNode : public DeletableVulkanResource {
   public:
    ColorAttachmentInfo* addColorAttachment(RenderTargetImageCreationInfo rticInfo, bool clear);
    void connectFromNode(RenderGraphNode* src, uint32_t color_output, uint32_t inputAttachmentIndex);
    void destroy_back(VulkanContext* ctx) override;
    uint32_t getSubpassIndex() { return this->nodeIndex; }
    void requestDepthAsInput(uint32_t index);
    std::vector<ColorAttachmentInfo>* getColorAttachments() {
        return &this->colorAttachments;
    }
    GraphicalConfigurationStore configurationStore;
    void setupSubpassDescriptorSet(VulkanContext* ctx, DescriptorSetLayoutInfo dsli);
    void removeSubpassDescriptorSet(VulkanContext* ctx);
    DescriptorSetArray* getSubpassDsa() { return this->subpassDsa; }

   private:
    RenderGraphNode();
    RenderGraphNode(VulkanContext* ctx, bool is_terminating_node);
    void topologicalSort(std::vector<RenderGraphNode*>* orderedNodes);
    struct {
        std::vector<VkAttachmentReference> colorAttachmentReferences;
        std::vector<VkAttachmentReference> inputAttachmentReferences;
        VkAttachmentReference depthStencilAttachmentReference;
    } subpassInfo;
    std::vector<ColorAttachmentInfo> colorAttachments;
    bool isTerminatingNode;
    uint32_t nodeIndex = 0;
    uint8_t markings = 0;
    uint32_t depthInputRequest = UINT32_MAX;
    // std::vector<std::pair<uint32_t, RenderGraphNode*>> outputs;
    std::unordered_map<uint32_t, std::pair<uint32_t, RenderGraphNode*>> inputs;
    DescriptorSetArray* subpassDsa = nullptr;

    friend class RenderGraph;
};

class RenderGraph;

// FIXME: Misnamed, represents a single render pass actually.
class RenderState : public DeletableVulkanResource {
   public:
    VkRenderPass renderPass = VK_NULL_HANDLE;
    std::vector<VkClearValue*> clearValuePtrs;
    std::vector<RenderTargetImageCreationInfo> rticInfos;
    void begin(VkCommandBuffer commandBuffer, SwapChainFrame swapChainFrame);
    void renderAll(VkCommandBuffer commandBuffer, SwapChainFrame swapChainFrame);
    void end(VkCommandBuffer commandBuffer);
    void destroy_back(VulkanContext* ctx) override;
    VkClearValue depthStencilClearValue = {1.0f, 0};
    VulkanContext* ctx = nullptr;

    void setupPassDescriptorSet(DescriptorSetLayoutInfo dsli);
    void removePassDescriptorSet();
    DescriptorSetArray* getPassDsa() { return this->passDsa; }

    std::vector<std::pair<uint32_t, DescriptorSetArray*>> dsaQueue;

   private:
    std::vector<RenderGraphNode*> nodes;
    DescriptorSetArray* passDsa;
    friend class RenderGraph;
};

class RenderGraph : public DeletableVulkanResource {
   public:
    RenderGraph(VulkanContext* ctx);
    RenderGraphNode* buildPresentingNode();
    RenderGraphNode* buildNewNode();
    RenderState* buildRenderState();
    void destroy_back(VulkanContext* ctx) override;

   private:
    std::vector<RenderGraphNode*> nodes;
    VulkanContext* ctx;
    RenderGraphNode* terminator = NULL;
    friend class RenderState;
};
}  // namespace lepton2::vulkancore