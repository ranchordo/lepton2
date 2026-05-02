#pragma once

#include "Framebuffer.h"
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
    VkPipelineColorBlendAttachmentState blendState;
    std::vector<VulkanImage*>* creationTracker;
};

// Utility function to decrease verbosity
RenderTargetImageCreationInfo defaultColorAttachmentRTIC(VkFormat format, VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT);

struct ColorAttachmentInfo {
    VkAttachmentDescription desc;
    RenderTargetImageCreationInfo rticInfo;
    VkClearValue clearValue = {0.0f, 0.0f, 0.0f, 0.0f};
    std::vector<VulkanImage*> swapChainCreations;
};

class SubpassRenderCallback {
   public:
    virtual void renderSubpassCmd(VkCommandBuffer commandBuffer, RenderPass* pass, uint32_t scfi, uint32_t setidx) = 0;
    virtual void preRenderSubpass(VulkanContext* ctx, uint32_t scfi) = 0;
};

class RenderPass;
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
    void setupSubpassDescriptorSet(VulkanContext* ctx, RenderPass* parent, DescriptorSetLayoutInfo dsli);
    DescriptorSetArray* getSubpassDsa() { return this->subpassDsa; }
    void setRenderCallback(SubpassRenderCallback* callback) { this->renderCallback = callback; }
    SubpassRenderCallback* getRenderCallback() { return this->renderCallback; }
    VkPipelineLayout getDummySubpassLayout() { return this->dummySubpassLayout; }

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
    std::unordered_map<uint32_t, std::pair<uint32_t, RenderGraphNode*>> inputs;
    DescriptorSetArray* subpassDsa = nullptr;
    SubpassRenderCallback* renderCallback = nullptr;
    VkPipelineLayout dummySubpassLayout = VK_NULL_HANDLE;

    friend class RenderGraph;
};

class RenderGraph;

class RenderPass : public DeletableVulkanResource {
   public:
    VkRenderPass renderPass = VK_NULL_HANDLE;
    std::vector<VkClearValue*> clearValuePtrs;
    std::vector<RenderTargetImageCreationInfo> rticInfos;
    void begin(VulkanContext* ctx, VkCommandBuffer commandBuffer, uint32_t scfi);
    void preRenderAll(VulkanContext* ctx, uint32_t frameIndex);
    void renderAll(VkCommandBuffer commandBuffer, uint32_t frameIndex);
    void end(VkCommandBuffer commandBuffer);
    void destroy_back(VulkanContext* ctx) override;
    VkClearValue depthStencilClearValue = {1.0f, 0};

    uint32_t numSubpasses() { return this->nodes.size(); }
    RenderGraphNode* getNode(uint32_t idx) { return this->nodes[idx]; }

    void setupPassDescriptorSet(VulkanContext* ctx, DescriptorSetLayoutInfo dsli);
    void setSuperpassLayouts(std::vector<VkDescriptorSetLayout> superpassLayouts);
    // void removePassDescriptorSet();
    DescriptorSetArray* getPassDsa() { return this->passDsa; }
    std::vector<VkDescriptorSetLayout> getSuperpassLayouts() { return this->superpassLayouts; }

   private:
    std::vector<RenderGraphNode*> nodes;
    DescriptorSetArray* passDsa = nullptr;
    VkPipelineLayout dummyPassLayout = VK_NULL_HANDLE;
    std::vector<VkDescriptorSetLayout> superpassLayouts;
    friend class RenderGraph;
};

class RenderGraph : public DeletableVulkanResource {
   public:
    RenderGraphNode* buildPresentingNode(VulkanContext* ctx);
    RenderGraphNode* buildNewNode();
    RenderPass* buildRenderState(VulkanContext* ctx);
    void destroy_back(VulkanContext* ctx) override;

   private:
    std::vector<RenderGraphNode*> nodes;
    RenderGraphNode* terminator = nullptr;
    friend class RenderPass;
};
}  // namespace lepton2::vulkancore