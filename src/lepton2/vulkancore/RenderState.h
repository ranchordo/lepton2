#pragma once

#include "VulkanUtils.h"
#include "Shader.h"

namespace lepton2::vulkancore {
    class SubpassAccumulator {
    public:
        
        // Shaders
        // Textures
        // Attachments
    private:
        Shader shader;
        std::vector<VkAttachmentReference> attachments;
    };
    class RenderStateAccumulator {

    };
}