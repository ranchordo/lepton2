#pragma once

#include "VulkanUtils.h"
#include "Framebuffer.h"
#include "VulkanContext.h"
#include "SwapChain.h"
#include "Pipelines.h"

// Render graph:
// Nodes are subpasses, or individual processing / rendering steps
// Edges are shared output -> input or presentation framebuffers
// ! The graph must culminate on the single presentation framebuffer node.
// Each node has its own graphics pipeline, shader, and single framebuffer.

// Building a render state needs two passes:
// One, we traverse with minimal information and build the RenderPass incl. dependencies.
// Two, we build framebuffers and graphics pipelines for each node.
namespace lepton2::vulkancore {
    struct RenderTargetImageCreationInfo {
        bool use_swapchain = false;
        VkFormat format;
        VkSampleCountFlagBits samples;
        VkImageTiling imageTiling;
        VkImageUsageFlags usage;
        VkMemoryPropertyFlags memoryProperties;
        VkImageAspectFlags aspectFlags;
    };

    struct ColorAttachmentInfo {
        VkAttachmentDescription desc;
        RenderTargetImageCreationInfo rticInfo;
        VkClearValue clearValue = { 0.0f, 0.0f, 0.0f, 0.0f };
    };

    class RenderGraphNode: public DeletableVulkanResource {
    public:
        void addColorAttachment(RenderTargetImageCreationInfo rticInfo, bool clear);
        void connectToNode(uint32_t color_output, RenderGraphNode* node);
        void destroy_back(VulkanContext* ctx) override;
        uint32_t getSubpassIndex() { return this->nodeIndex; }
    private:
        RenderGraphNode();
        RenderGraphNode(VulkanContext* ctx, bool is_terminating_node);
        struct {
            std::vector<VkAttachmentReference> colorAttachmentReferences;
            std::vector<VkAttachmentReference> inputAttachmentReferences;
            VkAttachmentReference depthStencilAttachmentReference;
        } subpassInfo;
        std::vector<ColorAttachmentInfo> colorAttachments;
        bool isTerminatingNode;
        uint32_t nodeIndex = 0;
        std::vector<std::pair<uint32_t, RenderGraphNode*>> outputs; // FIXME: Determine if needed
        std::vector<std::pair<uint32_t, RenderGraphNode*>> inputs;
        friend class RenderGraph;
    };

    class RenderGraph;

    class RenderState: public DeletableVulkanResource {
    public:
        VkRenderPass renderPass = VK_NULL_HANDLE;
        std::vector<VkClearValue*> clearValuePtrs;
        std::vector<RenderTargetImageCreationInfo> rticInfos;
        void addPipeline(std::string key, PipelineInfo cInfo);
        GraphicsPipeline* getPipeline(std::string key);
        void bind(VkCommandBuffer commandBuffer, SwapChainFrame swapChainFrame);
        void destroy_back(VulkanContext* ctx) override;
        VkClearValue depthStencilClearValue = { 1.0f, 0 };
    private:
        std::map<std::string, GraphicsPipeline*> pipelines;
        VulkanContext* ctx = nullptr;
        friend class RenderGraph;
    };

    class RenderGraph: public DeletableVulkanResource {
    public:
        RenderGraph(VulkanContext* ctx);
        RenderGraphNode* getTerminatingNode() {
            return this->terminator;
        }
        RenderGraphNode* buildNewNode();
        RenderState* buildRenderState();
        void destroy_back(VulkanContext* ctx) override;
    private:
        std::vector<RenderGraphNode*> nodes;
        VulkanContext* ctx;
        RenderGraphNode* terminator;
        friend class RenderState;
    };
}