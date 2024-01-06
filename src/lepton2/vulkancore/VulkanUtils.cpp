#include "VulkanUtils.h"
#include "VulkanContext.h"
#include "RenderState.h"

namespace lepton2::vulkancore {

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

    void createBuffer(VulkanContext* ctx, VkDeviceSize size,
        VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
        VulkanBuffer* buffer) {

        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateBuffer(ctx->device, &bufferInfo, nullptr, &buffer->buffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create buffer.");
        }
        buffer->findMemory(ctx, properties);
    }

    void createImage(VulkanContext* ctx,
        uint32_t width, uint32_t height,
        VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage,
        VkSampleCountFlagBits samples, VkMemoryPropertyFlags properties,
        VkImageAspectFlags aspectFlags, VulkanImage* image) {

        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
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
        image->do_not_destroy_image = false;
        image->findMemory(ctx, properties);
        image->buildImageView(ctx, aspectFlags);
    }

    void createRenderTarget(VulkanContext* ctx, VkExtent2D extent,
        RenderTargetImageCreationInfo* rticInfo, VulkanImage* image) {
        createImage(ctx, extent.width, extent.height, rticInfo->format,
            rticInfo->imageTiling, rticInfo->usage, rticInfo->samples,
            rticInfo->memoryProperties, rticInfo->aspectFlags, image);
    }

    VkCommandBuffer beginSingleTimeCommands(VulkanContext* ctx) {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = ctx->vk_command_pools.transientGraphics;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(ctx->device, &allocInfo, &commandBuffer);
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        return commandBuffer;
    }

    void endSingleTimeCommands(VulkanContext* ctx, VkCommandBuffer commandBuffer) {
        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;
        vkQueueSubmit(ctx->vk_queues.graphics, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(ctx->vk_queues.graphics);
        // TODO: Fences
        vkFreeCommandBuffers(ctx->device, ctx->vk_command_pools.transientGraphics, 1, &commandBuffer);
    }

    void transitionImageLayout(VulkanContext* ctx, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, ImageLayoutTransitionMode iltm) {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands(ctx);
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = 0;

        VkPipelineStageFlags sourceStage;
        VkPipelineStageFlags destinationStage;
        switch (iltm) {
        case ILTM_UNDEF_2_XFER_DST:
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            break;
        case ILTM_XFER_DST_2_FRAG_RO:
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            break;
        default:
            throw std::runtime_error("Invalid image layout transition mode.");
        }
        vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        endSingleTimeCommands(ctx, commandBuffer);
    }

    void copyBufferToImage(VulkanContext* ctx, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands(ctx);
        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = { 0, 0, 0 };
        region.imageExtent = { width, height, 1 };
        vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        endSingleTimeCommands(ctx, commandBuffer);
    }

    VkImageView createImageView(VulkanContext* ctx, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
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

    void copyBuffer(VulkanContext* ctx, VulkanBuffer* src, VulkanBuffer* dst, VkDeviceSize size,
        VkDeviceSize srcOffset, VkDeviceSize dstOffset) {

        VkCommandBuffer commandBuffer = beginSingleTimeCommands(ctx);
        VkBufferCopy copyRegion{};
        copyRegion.srcOffset = srcOffset;
        copyRegion.dstOffset = dstOffset;
        copyRegion.size = size;
        vkCmdCopyBuffer(commandBuffer, src->buffer, dst->buffer, 1, &copyRegion);

        endSingleTimeCommands(ctx, commandBuffer);
    }

    std::vector<char> readFile(const std::string& filename) {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);
        if (!file.is_open()) {
            printf("Attempted to open file %s\n", filename.c_str());
            throw std::runtime_error("Could not open miscellaneous file for reading.");
        }
        size_t fsize = (size_t)file.tellg();
        std::vector<char> buffer(fsize);
        file.seekg(0);
        file.read(buffer.data(), fsize);
        file.close();
        return buffer;
    }

    VkSemaphore createGenericSemaphore(VulkanContext* ctx) {
        VkSemaphore ret;
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        if (vkCreateSemaphore(ctx->device, &semaphoreInfo, nullptr, &ret) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create generic semaphore.");
        }
        return ret;
    }

    VkFence createGenericFence(VulkanContext* ctx, bool signaled) {
        VkFence ret;
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0;
        if (vkCreateFence(ctx->device, &fenceInfo, nullptr, &ret) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create generic fence.");
        }
        return ret;
    }

    std::chrono::steady_clock::time_point startTiming() {
        return std::chrono::steady_clock::now();
    }

    double getElapsedSeconds(std::chrono::steady_clock::time_point time_point) {
        auto now = startTiming();
        std::chrono::duration<double> interval = now - time_point;
        return interval.count();
    }

    std::filesystem::path getExecutableLocation(char* argv0, bool force_absolute) {
        std::filesystem::path relative_path = std::filesystem::path(argv0).parent_path();
        if (!force_absolute) {
            return relative_path;
        }
        return std::filesystem::absolute(relative_path);
    }
}