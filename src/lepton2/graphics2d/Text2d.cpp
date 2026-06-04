#include "Text2d.h"

using namespace lepton2::graphics;
using namespace lepton2::graphics2d;
using namespace lepton2::vulkancore;

// FIXME: Stop using ASCII and deal with UTF-8 properly
static uint32_t parseCodepoint(const char** str) {
    uint32_t c = (uint32_t)(**str);
    (*str)++;
    if (c > 127) {
        throw std::runtime_error("Implement UTF-8 you dumbhead");
    }
    return c;
}

TextGlyph2d* Text2d::getInactiveGlyph(VulkanContext* ctx, uint32_t codepoint, ProcessedGlyph** pgptr) {
    auto insertion = glyphPool.insert(std::make_pair(codepoint, std::vector<TextGlyph2d*>()));
    std::vector<TextGlyph2d*>& vec = insertion.first->second;
    TextGlyph2d* result = nullptr;
    for (uint32_t i = 0; i < vec.size(); i++) {
        if (vec[i] == nullptr) return nullptr;
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
        xpos += 2.f * pg->advanceWidth;
        if (newGlyph != nullptr) {
            this->addChild(newGlyph);
            glyphs.push_back(newGlyph);
            newGlyph->setActive(true);
            newGlyph->region = {{xpos, -0.5f}, {2.0f, 2.0f}, 0.f};
        }
    }
}

void Text2d::destroy_back(VulkanContext* ctx) {
    for (std::pair<uint32_t, std::vector<TextGlyph2d*>> entry : glyphPool) {
        for (TextGlyph2d* glyph : entry.second) {
            glyph->destroy(ctx);
            delete glyph;  // Yes, I could have used addLinkedResource, and no, I didn't.
        }
    }
    glyphs.clear();
}