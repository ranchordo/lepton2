#pragma once

#include "../utils/LeptonUtils.h"
#include "../vulkancore/VulkanContext.h"
#include "Entity2d.h"

namespace lepton2::graphics2d {

struct ProcessedGlyph {
    std::vector<glm::vec2> coords;
    std::vector<bool> onCurve;
    float advanceWidth;

    void destroy(vkc::VulkanContext* ctx) {
        // Do nothing
    }
};

class TTFParser;
class TextFont : public vkc::DeletableVulkanResource {
    public:
    TextFont(vkc::VulkanContext* ctx, const char* ttf);

    bool containsGlyph(uint32_t codepoint);
    ProcessedGlyph* getGlyph(vkc::VulkanContext* ctx, uint32_t codepoint);

    void destroy_back(vkc::VulkanContext* ctx) override;

    private:
    std::unordered_map<uint32_t, ProcessedGlyph*> glyphMap;
    TTFParser* parser;
};

}  // namespace lepton2::graphics2d