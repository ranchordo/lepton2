#pragma once

#include "../utils/LeptonUtils.h"
#include "../vulkancore/ObjectData.h"
#include "../vulkancore/VulkanContext.h"

namespace lepton2::graphics2d::ttfparse {

struct TTFOffsetSubtable {
    void create(const std::vector<char>& contents, size_t* ptr);
    uint16_t numTables;
};

struct TTFTableDirectory {
    void create(const std::vector<char>& contents, size_t* ptr);
    char tag[4];
    uint32_t checksum;
    uint32_t offset;
    uint32_t length;
    bool checkChecksum(const std::vector<char>& contents);
};

struct TTFHeadTable {
    void create(const std::vector<char>& contents, size_t* ptr);
    inline float applyScale(int16_t dist) {
        return ((float)dist) / ((float)unitsPerEm);
    }
    inline float applyScalef(float dist) {
        return dist / ((float)unitsPerEm);
    }
    uint16_t unitsPerEm;
    bool longLocFormat;
};

struct TTFMaxpTable {
    void create(const std::vector<char>& contents, size_t* ptr);
    uint16_t numGlyphs;
};

struct TTFHheaTable {
    void create(const std::vector<char>& contents, size_t* ptr);
    uint16_t numOfLongHorMetrics;
};

struct TTFHmtxTable {
    void create(TTFHheaTable& hhea, const std::vector<char>& contents, size_t* ptr);
    uint16_t getAdvanceWidth(uint32_t glyph) {
        if (glyph >= advanceWidths.size()) return advanceWidths[advanceWidths.size() - 1];
        return advanceWidths[glyph];
    }
    std::vector<uint16_t> advanceWidths;
};

struct TTFLocaTable {
    void create(TTFMaxpTable& maxp, TTFHeadTable& head, const std::vector<char>& contents, size_t* ptr);
    std::vector<uint32_t> glyphOffsets;
};

struct _TTFCmapSegment4 {
    uint16_t idDelta;
    uint16_t glyphIndexOffsetStart;
};

struct _TTFCmapSegment12 {
    uint32_t startGlyphCode;
};

struct _TTFCmapSegment {
    uint32_t startCode;
    uint32_t endCode;
    union {
        _TTFCmapSegment4 seg4;
        _TTFCmapSegment12 seg12;
    } s;
};

struct TTFCmapTable {
    void create(const std::vector<char>& contents, size_t* ptr);
    uint32_t getGlyphIndex(uint32_t charCode);

    std::vector<_TTFCmapSegment> segments;
    std::vector<uint16_t> glyphIndexOffsets4;
    uint16_t format;
};

struct TTFCompoundGlyphComponent {
    uint32_t glyphIdx;
    glm::mat2 transform;
    glm::vec2 offset;
};

struct TTFGlyph {
    TTFGlyph(const std::vector<char>& contents, size_t* ptr);
    void parseCompound(const std::vector<char>& contents, size_t* ptr);

    glm::vec2 getPoint(uint32_t index, TTFHeadTable& head);

    bool isCompound = false;
    std::vector<TTFCompoundGlyphComponent> compoundComponents;

    std::vector<uint16_t> contourEndIndices;
    int16_t xmin, xmax, ymin, ymax;
    std::vector<int16_t> xCoords;
    std::vector<int16_t> yCoords;
    std::vector<bool> onCurve;
};

class TTFParser {
   public:
    TTFParser(const std::vector<char>& ttfContents);

    TTFHeadTable head;
    TTFMaxpTable maxp;
    TTFLocaTable loca;
    TTFCmapTable cmap;
    TTFHheaTable hhea;
    TTFHmtxTable hmtx;
    std::vector<char> glyfData;
};
}