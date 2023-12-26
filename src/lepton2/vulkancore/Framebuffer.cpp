#include "Framebuffer.h"
#include "VulkanContext.h"

using namespace lepton2::vulkancore;

void Framebuffer::addColorImage(VulkanImage image) {
    this->colorImages.push_back(image);
}

void Framebuffer::setDepthImage(VulkanImage image) {
    this->depthImage = image;
}

void Framebuffer::destroy(VulkanContext* ctx) {
    for (VulkanImage colorImage : this->colorImages) {
        colorImage.destroy(ctx);
    }
    this->colorImages.clear();
    if (this->depthImage.image != VK_NULL_HANDLE) {
        this->depthImage.destroy(ctx);
        this->depthImage.image = VK_NULL_HANDLE;
    }
}