#pragma once

#include "../utils/LeptonUtils.h"
#include "../vulkancore/ObjectData.h"
#include "../vulkancore/VulkanContext.h"
#include "TTFParser.h"

namespace lepton2::graphics2d {

namespace vkc = lepton2::vulkancore;

//! Holds a glyph retrieved from a TextFont - basically a lepton2::vulkancore::DeviceObjectData with misc glyph info.
struct ProcessedGlyph {
    vkc::DeviceObjectData* objData;
    vkc::HostObjectData* hostData;  // Only for loading compound glyphs
    float advanceWidth;

    void destroy(vkc::VulkanContext* ctx) {
        if (objData != nullptr) {
            objData->destroy(ctx);
            delete objData;
        }
        if (hostData != nullptr) {
            hostData->destroy(ctx);
            delete hostData;
        }
    }
};

//! TTF loader which generates triangulated true vector glyphs using a shader trick.
class TextFont : public vkc::DeletableVulkanResource {
   public:
    TextFont(vkc::VulkanContext* ctx, const char* ttf);

    bool containsGlyph(uint32_t codepoint);
    ProcessedGlyph* getGlyph(vkc::VulkanContext* ctx, uint32_t codepoint);

    void destroy_back(vkc::VulkanContext* ctx) override;

   private:
    ProcessedGlyph* getGlyphByIndex(vkc::VulkanContext* ctx, uint32_t glyphIndex);
    std::unordered_map<uint32_t, ProcessedGlyph*> glyphMap;
    ttfparse::TTFParser* parser;
};

}  // namespace lepton2::graphics2d