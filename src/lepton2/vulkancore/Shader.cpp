#include "Shader.h"

#include "VulkanContext.h"

using namespace lepton2::vulkancore;

namespace lepton2::vulkancore {
    const char* shaders_spirv_load_dir = "shaders_build";
}

VkShaderModule Shader::buildShaderModule(VulkanContext* ctx, const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = (uint32_t*)code.data();
    VkShaderModule shaderModule;
    if (vkCreateShaderModule(ctx->device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module.");
    }
    return shaderModule;
}

Shader::Shader(VulkanContext* ctx, const char* shader_name) {
    size_t combined_length = snprintf(nullptr, 0, "%s/%s.vert.spv", shaders_spirv_load_dir, shader_name);
    char filename_buffer[combined_length + 1];
    snprintf(filename_buffer, combined_length + 1, "%s/%s.vert.spv", shaders_spirv_load_dir, shader_name);
    std::vector<char> vertex_code = readFile(std::string(filename_buffer));
    this->vertexShaderModule = this->buildShaderModule(ctx, vertex_code);
    snprintf(filename_buffer, combined_length + 1, "%s/%s.frag.spv", shaders_spirv_load_dir, shader_name);
    std::vector<char> fragment_code = readFile(std::string(filename_buffer));
    this->fragmentShaderModule = this->buildShaderModule(ctx, fragment_code);
}

void Shader::destroy(VulkanContext* ctx) {
    if (this->vertexShaderModule != VK_NULL_HANDLE) {
        vkDestroyShaderModule(ctx->device, vertexShaderModule, nullptr);
        vertexShaderModule = VK_NULL_HANDLE;
    }
    if (this->fragmentShaderModule != VK_NULL_HANDLE) {
        vkDestroyShaderModule(ctx->device, fragmentShaderModule, nullptr);
        fragmentShaderModule = VK_NULL_HANDLE;
    }
}