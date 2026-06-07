#include "Text2d.h"

#include <wchar.h>

using namespace lepton2::graphics;
using namespace lepton2::graphics2d;
using namespace lepton2::vulkancore;

static inline uint32_t parseNextByte(const char** str) {
    uint8_t c = (uint8_t)(*((*str)++));
    assert((0xc0 & c) == 0x80);
    return (uint32_t)(c & 0x3f);
}

static uint32_t parseCodepoint(const char** str) {
    uint8_t c = (uint8_t)(*((*str)++));
    if (c <= 0x7f) return (uint32_t)c;
    if ((0xe0 & c) == 0xc0) {
        return ((0x1f & c) << 6) | parseNextByte(str);
    } else if ((0xf0 & c) == 0xe0) {
        return ((0x0f & c) << 12) | (parseNextByte(str) << 6) | parseNextByte(str);
    } else if ((0xf8 & c) == 0xf0) {
        return ((0x07 & c) << 18) | (parseNextByte(str) << 12) | (parseNextByte(str) << 6) | parseNextByte(str);
    }
    throw std::runtime_error("Invalid UTF-8");
}

TextGlyph2d* Text2d::getInactiveGlyph(VulkanContext* ctx, uint32_t codepoint, ProcessedGlyph** pgptr) {
    auto insertion = glyphPool.insert(std::make_pair(codepoint, std::vector<TextGlyph2d*>()));
    std::vector<TextGlyph2d*>& vec = insertion.first->second;
    TextGlyph2d* result = nullptr;
    for (uint32_t i = 0; i < vec.size(); i++) {
        if (vec[i] == nullptr) {
            (*pgptr) = font->getGlyph(ctx, codepoint);
            return nullptr;
        }
        if (!vec[i]->isActive()) {
            result = vec[i];
            break;
        }
    }
    if (result == nullptr) {
        ProcessedGlyph* pg = font->getGlyph(ctx, codepoint);
        (*pgptr) = pg;
        if (pg->objData == nullptr) {  // No glyph data for this one
            vec.push_back(nullptr);
            return nullptr;
        }
        result = new TextGlyph2d(ctx, shaderName, pg);
        result->wireframe = this->wireframe;
        result->initialize(ctx, initInfo.pass, initInfo.subpass, initInfo.store);
        vec.push_back(result);
    }
    return result;
}

void Text2d::setString(VulkanContext* ctx, const char* string) {
    for (TextGlyph2d* glyph : glyphs) {
        glyph->setActive(false);
        this->removeChild(glyph);
    }
    glyphs.clear();

    float xpos = -0.5f;
    while (*string) {
        uint32_t codepoint = parseCodepoint(&string);
        ProcessedGlyph* pg;
        TextGlyph2d* newGlyph = this->getInactiveGlyph(ctx, codepoint, &pg);
        if (newGlyph != nullptr) {
            this->addChild(newGlyph);
            glyphs.push_back(newGlyph);
            newGlyph->setActive(true);
            newGlyph->region = {{xpos, -0.5f}, {2.0f, 2.0f}, 0.f};
        }
        xpos += 2.f * pg->advanceWidth;
    }
}

void Text2d::destroy_back(VulkanContext* ctx) {
    for (std::pair<uint32_t, std::vector<TextGlyph2d*>> entry : glyphPool) {
        for (TextGlyph2d* glyph : entry.second) {
            if (glyph == nullptr) continue;
            glyph->destroy(ctx);
            delete glyph;  // Yes, I could have used addLinkedResource, and no, I didn't.
        }
    }
    glyphs.clear();
}