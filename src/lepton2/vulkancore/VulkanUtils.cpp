#include "VulkanUtils.h"

#include "RenderPass.h"
#include "VulkanContext.h"

#define STB_IMAGE_IMPLEMENTATION
#include "../external/stb_image.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include "../external/tiny_obj_loader.h"

namespace lepton2::vulkancore {

void ImageArray::destroy_back(VulkanContext* ctx) {
    for (VulkanImage* img : images) {
        img->destroy(ctx);
        delete img;
    }
}

uint32_t findMemoryType(VulkanContext* ctx, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(ctx->physicalDevice, &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if (typeFilter & (1 << i)) {
            if ((memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }
    }
    throw std::runtime_error("Failed to find suitable memory type.");
}

void createBuffer(VulkanContext* ctx, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VulkanBuffer* buffer) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.pNext = nullptr;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(ctx->device, &bufferInfo, nullptr, &buffer->buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer.");
    }
    buffer->findMemory(ctx, properties);
}

void createImage(VulkanContext* ctx, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage,
                 VkSampleCountFlagBits samples, VkMemoryPropertyFlags properties, VkImageAspectFlags aspectFlags, VulkanImage* image) {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.pNext = nullptr;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = samples;
    imageInfo.flags = 0;
    if (vkCreateImage(ctx->device, &imageInfo, nullptr, &image->image) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create image.");
    }
    image->imageFormat = format;
    image->doNotDestroyImage = false;
    image->findMemory(ctx, properties);
    if (aspectFlags != 0) {
        image->buildImageView(ctx, aspectFlags);
    }
}

void createRenderTarget(VulkanContext* ctx, VkExtent2D extent, RenderTargetImageCreationInfo* rticInfo, VulkanImage* image) {
    createImage(ctx, extent.width, extent.height, rticInfo->format, rticInfo->imageTiling,
                rticInfo->usage, rticInfo->samples, rticInfo->memoryProperties, rticInfo->aspectFlags, image);
}

VkCommandBuffer beginSingleTimeCommands(VulkanContext* ctx, VkCommandPool pool) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.pNext = nullptr;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = pool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    if (vkAllocateCommandBuffers(ctx->device, &allocInfo, &commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate one-time command buffer.");
    }
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.pNext = nullptr;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to begin one-time command buffer.");
    }

    return commandBuffer;
}

void endSingleTimeCommands(VulkanContext* ctx, VkCommandBuffer commandBuffer, VkQueue queue, VkCommandPool pool) {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pNext = nullptr;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    VkFence fence = createGenericFence(ctx, false);
    vkQueueSubmit(queue, 1, &submitInfo, fence);
    vkWaitForFences(ctx->device, 1, &fence, VK_TRUE, UINT64_MAX);
    vkFreeCommandBuffers(ctx->device, pool, 1, &commandBuffer);
    vkDestroyFence(ctx->device, fence, nullptr);
}

void transitionImageLayout(VulkanContext* ctx, VulkanImage* image, ImageLayoutTransitionMode iltm) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands(ctx, ctx->commandPools.transientGraphicsCompute);
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.pNext = nullptr;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image->image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;
    switch (iltm) {
        case ILTM_UNDEFINED_TO_TRANSFER_DST:
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            break;
        case ILTM_TRANSFER_DST_TO_SHADER_READ_ONLY:
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            break;
        case ILTM_UNDEFINED_TO_SHADER_READ_ONLY:
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            break;
        case ILTM_UNDEFINED_TO_PRESENT_SRC_KHR:
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = 0;
            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
            barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            break;
        default:
            throw std::runtime_error("Invalid image layout transition mode.");
    }
    vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    endSingleTimeCommands(ctx, commandBuffer, ctx->queues.graphics, ctx->commandPools.transientGraphicsCompute);
}

void copyBufferToImage(VulkanContext* ctx, VulkanBuffer* buffer, VulkanImage* image, uint32_t width, uint32_t height) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands(ctx, ctx->commandPools.transientGraphicsCompute);
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};
    vkCmdCopyBufferToImage(commandBuffer, buffer->buffer, image->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    endSingleTimeCommands(ctx, commandBuffer, ctx->queues.graphics, ctx->commandPools.transientGraphicsCompute);
}

VkImageView createImageView(VulkanContext* ctx, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.pNext = nullptr;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView imageView;
    if (vkCreateImageView(ctx->device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create image view.");
    }
    return imageView;
}

void copyBuffer(VulkanContext* ctx, VulkanBuffer* src, VulkanBuffer* dst, VkDeviceSize size, VkDeviceSize srcOffset, VkDeviceSize dstOffset) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands(ctx, ctx->commandPools.transientGraphicsCompute);
    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = srcOffset;
    copyRegion.dstOffset = dstOffset;
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, src->buffer, dst->buffer, 1, &copyRegion);

    endSingleTimeCommands(ctx, commandBuffer, ctx->queues.graphics, ctx->commandPools.transientGraphicsCompute);
}

VkSemaphore createGenericSemaphore(VulkanContext* ctx) {
    VkSemaphore ret;
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreInfo.pNext = nullptr;
    if (vkCreateSemaphore(ctx->device, &semaphoreInfo, nullptr, &ret) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create generic semaphore.");
    }
    return ret;
}

VkFence createGenericFence(VulkanContext* ctx, bool signaled) {
    VkFence ret;
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.pNext = nullptr;
    fenceInfo.flags = signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0;
    if (vkCreateFence(ctx->device, &fenceInfo, nullptr, &ret) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create generic fence.");
    }
    return ret;
}

void fillRenderTargetArray(VulkanContext* ctx, RenderTargetImageCreationInfo* rticInfo, ImageArray* array, VkExtent2D extent, uint32_t multiplicity) {
    for (uint32_t i = 0; i < array->images.size(); i++) {
        array->images[i]->destroy(ctx);
        delete array->images[i];
    }

    array->images.resize(multiplicity);
    for (uint32_t i = 0; i < multiplicity; i++) {
        array->images[i] = new VulkanImage();
        createRenderTarget(ctx, extent, rticInfo, array->images[i]);
    }
    array->extent = extent;
}

void submitDebugMessage(VulkanContext* ctx, const char* msg, VkDebugUtilsMessageSeverityFlagBitsEXT sev, VkDebugUtilsMessageTypeFlagBitsEXT type) {
    VkDebugUtilsMessengerCallbackDataEXT callbackData{};
    callbackData.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT;
    callbackData.pNext = nullptr;
    callbackData.pMessage = msg;
    callbackData.messageIdNumber = 0;

    PFN_vkSubmitDebugUtilsMessageEXT fptr = (PFN_vkSubmitDebugUtilsMessageEXT)vkGetInstanceProcAddr(ctx->instance, "vkSubmitDebugUtilsMessageEXT");

    if (fptr != nullptr) {
        fptr(ctx->instance, sev, type, &callbackData);
    } else {
        printf("[lepton2_internal] Failed to get function pointer for vkSubmitDebugUtilsMessageEXT\n");
    }
}

}  // namespace lepton2::vulkancore