#include "Framebuffer.h"
#include "VulkanContext.h"

using namespace lepton2::vulkancore;

void Framebuffer::addColorImage(VulkanImage* image) {
    this->colorImages.push_back(image);
}

void Framebuffer::setDepthImage(VulkanImage* image) {
    this->depthImage = image;
}

void Framebuffer::destroy_back(VulkanContext* ctx) {
    this->colorImages.clear();
    if (this->framebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(ctx->device, this->framebuffer, nullptr);
    }
}

void Framebuffer::buildFramebuffer(VkRenderPass renderPass, uint32_t width, uint32_t height) {
    std::vector<VkImageView> attachments;
    attachments.resize(this->colorImages.size() + 1);
    for (uint32_t i = 0; i < this->colorImages.size(); i++) {
        attachments.push_back(this->colorImages.at(i)->imageView);
    }
    if (this->depthImage->imageView != VK_NULL_HANDLE) {
        attachments.push_back(this->depthImage->imageView);
    }

    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = renderPass;
    framebufferInfo.attachmentCount = (uint32_t)attachments.size();
    framebufferInfo.pAttachments = attachments.data();
    framebufferInfo.width = width;
    framebufferInfo.height = height;
    framebufferInfo.layers = 1;
}