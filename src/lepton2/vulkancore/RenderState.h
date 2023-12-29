#pragma once

#include "VulkanUtils.h"
#include "Shader.h"
#include "Framebuffer.h"

namespace lepton2::vulkancore {
    class SubpassAccumulator {
    public:
        void bindShader(Shader shader) { this->shader = shader; }
        void useFramebufferArchitecture(Framebuffer framebuffer) { this->attachmentInfo = framebuffer; }
    private:
        Shader shader;
        Framebuffer attachmentInfo;
    };
    class RenderStateAccumulator {

    };
}