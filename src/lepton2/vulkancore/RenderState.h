#pragma once

#include "VulkanUtils.h"
#include "Framebuffer.h"
#include "VulkanContext.h"

// Render graph:
// Nodes are subpasses, or individual processing / rendering steps
// Edges are shared output -> input or presentation framebuffers
// ! The graph must culminate on the single presentation framebuffer node.
// Each node has its own graphics pipeline, shader, and single framebuffer.

// Building a render state needs two passes:
// One, we traverse with minimal information and build the RenderPass incl. dependencies.
// Two, we build framebuffers and graphics pipelines for each node.
namespace lepton2::vulkancore {
    struct ColorAttachmentInfo {
        VkAttachmentDescription desc;
    };
    class RenderGraphNode: public DeletableVulkanResource {
    public:
        void addColorAttachment(VkFormat format, VkSampleCountFlagBits samples, bool clear);
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

    struct RenderState {
        RenderGraph* graph = nullptr;
        VkRenderPass renderPass;
        bool complete = false;
    };

    class RenderGraph: public DeletableVulkanResource {
    public:
        RenderGraph(VulkanContext* ctx);
        RenderGraphNode* getTerminatingNode() {
            return this->terminator;
        }
        RenderGraphNode* buildNewNode();
        RenderState buildRenderState(VulkanContext* ctx);
        void destroy_back(VulkanContext* ctx) override;
    private:
        std::vector<RenderGraphNode*> nodes;
        VulkanContext* ctx;
        RenderGraphNode* terminator;
        RenderState finalState;
    };
}