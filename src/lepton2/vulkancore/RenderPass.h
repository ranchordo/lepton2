#pragma once

#include "Framebuffer.h"
#include "Pipelines.h"
#include "VulkanUtils.h"

namespace lepton2::vulkancore {
struct RenderTargetImageCreationInfo {
    bool isTerminal = false;
    VkFormat format;
    VkSampleCountFlagBits samples;
    VkImageTiling imageTiling;
    VkImageUsageFlags usage;
    VkMemoryPropertyFlags memoryProperties;
    VkImageAspectFlags aspectFlags;
    VkPipelineColorBlendAttachmentState blendState;
    uint32_t nodeidx;
    uint32_t colorAttachmentIdx;
};

// Utility function to decrease verbosity
RenderTargetImageCreationInfo defaultColorAttachmentRTIC(VkFormat format, VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT);

struct ColorAttachmentInfo {
    VkAttachmentDescription desc;
    RenderTargetImageCreationInfo rticInfo;
    VkClearValue clearValue = {0.0f, 0.0f, 0.0f, 0.0f};
    std::vector<VulkanImage*> renderTargets;
};

struct DepthAttachmentInfo {
    std::vector<VulkanImage*> depthTargetImages;
};

class RenderSubpass;
class RenderPass : public DeletableVulkanResource {
   public:
    RenderPass(VulkanContext* ctx, const std::vector<RenderSubpass*>& subpasses, uint32_t resourceMultiplicity);

    // Rendering
    void begin(VkCommandBuffer commandBuffer, uint32_t framebufferIndex);
    void preRenderAll(VulkanContext* ctx, uint32_t frameIndex);
    void renderAll(VkCommandBuffer commandBuffer, uint32_t frameIndex, uint32_t framebufferIndex);
    void end(VkCommandBuffer commandBuffer);

    // Initial configuration
    void generateFramebuffers(VulkanContext* ctx, ImageArray* finalImages);
    void destroyFramebuffers(VulkanContext* ctx);
    void setupPassDescriptorSet(VulkanContext* ctx, DescriptorSetLayoutInfo dsli);
    void setSuperpassLayouts(std::vector<VkDescriptorSetLayout> superpassLayouts);

    // Simple getters/setters
    uint32_t numSubpasses() { return this->nodes.size(); }
    RenderSubpass* getNode(uint32_t idx) { return this->nodes[idx]; }
    uint32_t getResourceMultiplicity() { return this->resourceMultiplicity; }
    uint32_t getNumFramebuffers() { return this->targets.size(); }
    DescriptorSetArray* getPassDsa() { return this->passDsa; }
    std::vector<VkDescriptorSetLayout>& getSuperpassLayouts() { return this->superpassLayouts; }
    VkRenderPass getRenderPass() { return this->renderPass; }
    DepthAttachmentInfo* getDepthAttachmentInfo() { return &this->depthAttachmentInfo; }
    void setDepthStencilClearValue(VkClearDepthStencilValue val) { this->depthStencilClearValue.depthStencil = val; }

    void destroy_back(VulkanContext* ctx) override;

   private:
    VkClearValue depthStencilClearValue = VkClearValue{.depthStencil = {1.0f, 0}};
    VkRenderPass renderPass = VK_NULL_HANDLE;
    std::vector<VkClearValue*> clearValuePtrs;
    std::vector<RenderTargetImageCreationInfo> rticInfos;
    std::vector<RenderSubpass*> nodes;
    std::vector<Framebuffer*> targets;
    std::vector<VulkanImage*> renderTargetImages;
    DepthAttachmentInfo depthAttachmentInfo;
    DescriptorSetArray* passDsa = nullptr;
    VkPipelineLayout dummyPassLayout = VK_NULL_HANDLE;
    std::vector<VkDescriptorSetLayout> superpassLayouts;
    uint32_t resourceMultiplicity;
};

class SubpassRenderCallback {
   public:
    virtual void renderSubpassCmd(VkCommandBuffer commandBuffer, RenderPass* pass, uint32_t frameIndex, uint32_t setidx) = 0;
    virtual void preRenderSubpass(VulkanContext* ctx, uint32_t frameIndex) = 0;
};

struct TerminatingSubpassConfig {
    VkAttachmentDescription attachmentDescription;
    VkPipelineColorBlendAttachmentState blendState;
};

class RenderSubpass : public DeletableVulkanResource {
   public:
    RenderSubpass();  // Non-terminating default
    RenderSubpass(VulkanContext* ctx, TerminatingSubpassConfig config);

    static TerminatingSubpassConfig getDefaultTerminatingConfig(VkFormat imageFormat, VkImageLayout finalLayout);

    ColorAttachmentInfo* addColorAttachment(RenderTargetImageCreationInfo rticInfo, bool clear);
    void connectFromNode(RenderSubpass* src, uint32_t color_output, uint32_t inputAttachmentIndex);
    void requestDepthAsInput(uint32_t index);

    uint32_t getSubpassIndex() { return this->nodeIndex; }
    std::vector<ColorAttachmentInfo>& getColorAttachments() { return this->colorAttachments; }
    SubpassRenderCallback* getRenderCallback() { return this->renderCallback; }
    VkPipelineLayout getDummySubpassLayout() { return this->dummySubpassLayout; }
    DescriptorSetArray* getSubpassDsa() { return this->subpassDsa; }
    DescriptorSetArray* getSubpassAttachmentDsa() { return this->subpassAttachmentDsa; }

    void setupSubpassDescriptorSet(VulkanContext* ctx, RenderPass* parent, DescriptorSetLayoutInfo dsli);
    void setRenderCallback(SubpassRenderCallback* callback) { this->renderCallback = callback; }

    void destroy_back(VulkanContext* ctx) override;

   private:
    void topologicalSort(std::vector<RenderSubpass*>* orderedNodes);
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
    std::unordered_map<uint32_t, std::pair<uint32_t, RenderSubpass*>> inputs;
    DescriptorSetArray* subpassDsa = nullptr;
    DescriptorSetArray* subpassAttachmentDsa = nullptr;
    SubpassRenderCallback* renderCallback = nullptr;
    VkPipelineLayout dummySubpassLayout = VK_NULL_HANDLE;

    friend RenderPass::RenderPass(VulkanContext*, const std::vector<RenderSubpass*>&, uint32_t);
};
}  // namespace lepton2::vulkancore