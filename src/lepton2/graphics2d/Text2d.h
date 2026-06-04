#pragma once

#include "Entity2d.h"
#include "TextFont.h"

namespace lepton2::graphics2d {

class TextGlyph2d : public Entity2d {
   public:
    TextGlyph2d(vkc::VulkanContext* ctx, const char* shaderName, ProcessedGlyph* glyph) : Entity2d(ctx, shaderName, simpleVsd2d, 0) {
        this->setObjectData(glyph->objData);
        this->setTexture(nullptr);
    }

    // Useful for debugging glyph processing logic
    void modifyPipelineRequirements(vkc::GraphicsPipelineConstraints* req) override {
        if (!wireframe) return;
        req->polygonMode = VK_POLYGON_MODE_LINE;
        req->cullMode = VK_CULL_MODE_NONE;
    }

    bool wireframe = false;
};

class Text2d : public Entity2d {
    public:
    Text2d(vkc::VulkanContext* ctx, const char* shaderName, TextFont* font) : Entity2d(ctx, nullptr, {}, 0) {
        this->shaderName = shaderName;
        this->font = font;
    }

    void postInit(vkc::VulkanContext* ctx, vkc::RenderPass* renderState, vkc::RenderSubpass* node, gph::GraphicalConfigurationStore* store) override {
        this->initInfo.pass = renderState;
        this->initInfo.subpass = node;
        this->initInfo.store = store;
    }

    void setString(vkc::VulkanContext* ctx, const char* string);

    void destroy_back(vkc::VulkanContext* ctx) override;

    bool wireframe = false;

    private:
    TextGlyph2d* getInactiveGlyph(vkc::VulkanContext* ctx, uint32_t codepoint, ProcessedGlyph** pgptr);
    std::unordered_map<uint32_t, std::vector<TextGlyph2d*>> glyphPool;
    const char* shaderName;
    TextFont* font;
    struct {
        vkc::RenderPass* pass;
        vkc::RenderSubpass* subpass;
        gph::GraphicalConfigurationStore* store;
    } initInfo;
    std::vector<TextGlyph2d*> glyphs;
};

}  // namespace lepton2::graphics2d