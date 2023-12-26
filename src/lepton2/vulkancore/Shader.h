#pragma once

#include "VulkanUtils.h"

namespace lepton2::vulkancore {
    extern const char* shaders_spirv_load_dir;
    class Shader: public DeletableVulkanResource {
    public:
        Shader(VulkanContext* ctx, const char* shader_name);
        void destroy(VulkanContext* ctx) override;
        VkShaderModule vertexShaderModule;
        VkShaderModule fragmentShaderModule;
    private:
        VkShaderModule buildShaderModule(VulkanContext* ctx, const std::vector<char>& code);
    };
}