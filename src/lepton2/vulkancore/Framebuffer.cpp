#include "Framebuffer.h"

#include "VulkanContext.h"

using namespace lepton2::vulkancore;

void Framebuffer::addImage(VulkanImage* image) {
    this->images.push_back(image);
}

void Framebuffer::destroy_back(VulkanContext* ctx) {
    this->images.clear();
    if (this->framebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(ctx->device, this->framebuffer, nullptr);
    }
}

void Framebuffer::buildFramebuffer(VulkanContext* ctx, VkRenderPass renderPass, VkExtent2D extent) {
    this->extent = extent;
    std::vector<VkImageView> attachments;
    for (uint32_t i = 0; i < this->images.size(); i++) {
        attachments.push_back(this->images[i]->imageView);
    }

    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.pNext = nullptr;
    framebufferInfo.renderPass = renderPass;
    framebufferInfo.attachmentCount = (uint32_t)attachments.size();
    framebufferInfo.pAttachments = attachments.data();
    framebufferInfo.width = extent.width;
    framebufferInfo.height = extent.height;
    framebufferInfo.layers = 1;
    if (vkCreateFramebuffer(ctx->device, &framebufferInfo, nullptr, &this->framebuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create a framebuffer.");
    }
}