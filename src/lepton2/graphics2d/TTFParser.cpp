#include "TTFParser.h"

using namespace lepton2::graphics2d::ttfparse;

static inline constexpr uint16_t swapEndian16(uint16_t v) {
    return (v << 8) | (v >> 8);
}

static inline constexpr uint32_t swapEndian32(uint32_t v) {
    return (swapEndian16(v & 0xffffu) << 16) | swapEndian16(v >> 16);
}

static uint8_t getUint8(const std::vector<char>& contents, size_t* ptr) {
    uint8_t result = *(uint8_t*)(contents.data() + *ptr);
    (*ptr)++;
    return result;
}

static uint16_t getUint16(const std::vector<char>& contents, size_t* ptr) {
    uint16_t result = swapEndian16(*(uint16_t*)(contents.data() + *ptr));
    (*ptr) += 2;
    return result;
}

static uint32_t getUint32(const std::vector<char>& contents, size_t* ptr) {
    uint32_t result = swapEndian32(*(uint32_t*)(contents.data() + *ptr));
    (*ptr) += 4;
    return result;
}

static int8_t getInt8(const std::vector<char>& contents, size_t* ptr) {
    uint8_t result = getUint8(contents, ptr);
    return *(int8_t*)(&result);
}

static int16_t getInt16(const std::vector<char>& contents, size_t* ptr) {
    uint16_t result = getUint16(contents, ptr);
    return *(int16_t*)(&result);
}

static int32_t getInt32(const std::vector<char>& contents, size_t* ptr) {
    uint32_t result = getUint32(contents, ptr);
    return *(int32_t*)(&result);
}

static float getF2Dot14(const std::vector<char>& contents, size_t* ptr) {
    return ((float)getInt16(contents, ptr)) / ((float)(1 << 14));
}

void TTFOffsetSubtable::create(const std::vector<char>& contents, size_t* ptr) {
    (*ptr) += 4;
    numTables = getUint16(contents, ptr);
    (*ptr) += 6;
}

void TTFTableDirectory::create(const std::vector<char>& contents, size_t* ptr) {
    *(uint32_t*)tag = swapEndian32(getUint32(contents, ptr));
    checksum = getUint32(contents, ptr);
    offset = getUint32(contents, ptr);
    length = getUint32(contents, ptr);
}

bool TTFTableDirectory::checkChecksum(const std::vector<char>& contents) {
    uint32_t* table = (uint32_t*)(contents.data() + offset);
    uint32_t nLongs = (length + 3) / 4;
    uint32_t sum = 0;
    while (nLongs-- > 0) sum += swapEndian32(*table++);
    return (sum == checksum);
}

static TTFTableDirectory* findTable(const char* tag, std::vector<TTFTableDirectory>& tables) {
    for (uint32_t i = 0; i < tables.size(); i++) {
        if (strncmp(tables[i].tag, tag, 4) == 0) return &tables[i];
    }
    throw std::runtime_error("Failed to locate TTF table.");
}

void TTFHeadTable::create(const std::vector<char>& contents, size_t* ptr) {
    (*ptr) += 18;
    unitsPerEm = getUint16(contents, ptr);
    (*ptr) += 30;
    int16_t indexToLocFormat = getInt16(contents, ptr);
    if (indexToLocFormat != 0 && indexToLocFormat != 1) {
        throw std::runtime_error("Unrecognized 'head'->indexToLocFormat value.");
    }
    longLocFormat = (bool)(indexToLocFormat);
}

void TTFMaxpTable::create(const std::vector<char>& contents, size_t* ptr) {
    (*ptr) += 4;
    numGlyphs = getUint16(contents, ptr);
}

void TTFHheaTable::create(const std::vector<char>& contents, size_t* ptr) {
    (*ptr) += 34;
    numOfLongHorMetrics = getUint16(contents, ptr);
}

void TTFHmtxTable::create(TTFHheaTable& hhea, const std::vector<char>& contents, size_t* ptr) {
    advanceWidths.resize(hhea.numOfLongHorMetrics);
    for (uint32_t i = 0; i < hhea.numOfLongHorMetrics; i++) {
        advanceWidths[i] = getUint16(contents, ptr);
        (*ptr) += 2;  // leftSideBearing
    }
}

void TTFLocaTable::create(TTFMaxpTable& maxp, TTFHeadTable& head, const std::vector<char>& contents, size_t* ptr) {
    glyphOffsets.resize(maxp.numGlyphs + 1);
    if (head.longLocFormat) {
        for (uint32_t i = 0; i < maxp.numGlyphs + 1; i++) glyphOffsets[i] = getUint32(contents, ptr);
    } else {
        for (uint32_t i = 0; i < maxp.numGlyphs + 1; i++) glyphOffsets[i] = 2 * getUint16(contents, ptr);
    }
    for (uint32_t i = 0; i < maxp.numGlyphs; i++) {
        if (glyphOffsets[i + 1] == glyphOffsets[i]) {  // No glyph data
            glyphOffsets[i] = UINT32_MAX;
        }
    }
}

// Specifies priorities for Unicode platformSpecificID
static const uint8_t unicodePSIDKeys[] = {1, 2, 0, 3, 4, 0, 0};

void TTFCmapTable::create(const std::vector<char>& contents, size_t* ptr) {
    size_t cmap_start = *ptr;
    uint16_t version = getUint16(contents, ptr);
    assert(version == 0);
    uint16_t numSubtables = getUint16(contents, ptr);
    uint32_t bestOffset = UINT32_MAX;
    uint8_t bestUnicodePSIDKey = 0;
    for (uint32_t i = 0; i < numSubtables; i++) {
        uint16_t platformID = getUint16(contents, ptr);
        uint16_t platformSpecificID = getUint16(contents, ptr);
        uint32_t offset = getUint32(contents, ptr);
        if (platformID != 0) continue;  // Only support unicode
        assert(platformSpecificID <= 6);
        uint8_t psIDKey = unicodePSIDKeys[platformSpecificID];
        if (psIDKey > bestUnicodePSIDKey) {
            bestUnicodePSIDKey = psIDKey;
            bestOffset = offset;
        }
    }
    if (bestUnicodePSIDKey == 0) {
        throw std::runtime_error("Failed to find a suitable TTF 'cmap' mapping table\n");
    }
    (*ptr) = cmap_start + bestOffset;
    this->format = getUint16(contents, ptr);
    switch (format) {
        case 4: {
            (*ptr) += 4;
            uint16_t segCount = getUint16(contents, ptr) >> 1;
            (*ptr) += 6;
            segments.resize(segCount);
            for (uint32_t i = 0; i < segCount; i++) segments[i].endCode = getUint16(contents, ptr);
            (*ptr) += 2;
            for (uint32_t i = 0; i < segCount; i++) segments[i].startCode = getUint16(contents, ptr);
            for (uint32_t i = 0; i < segCount; i++) segments[i].s.seg4.idDelta = getUint16(contents, ptr);
            std::vector<std::pair<size_t, size_t>> regions;
            size_t rangeOffsetsLoc = *ptr;  // For later
            for (uint32_t i = 0; i < segCount; i++) {
                uint16_t idRangeOffset = getUint16(contents, ptr);
                if (idRangeOffset == 0) continue;
                uint32_t numCodes = 1 + segments[i].endCode - segments[i].startCode;
                size_t regionStart = *ptr + idRangeOffset - 2;
                size_t regionEnd = regionStart + 2 * numCodes;
                for (uint32_t reg = 0; reg < regions.size(); reg++) {
                    if (regions[reg].second < regionStart - 1) continue;
                    if (regions[reg].first > regionEnd + 1) continue;
                    if (regions[reg].first < regionStart) regionStart = regions[reg].first;
                    if (regions[reg].second > regionEnd) regionEnd = regions[reg].second;
                    regions[reg] = regions[regions.size() - 1];
                    regions.pop_back();
                }
                regions.push_back({regionStart, regionEnd});
            }

            size_t totalGIO4Size = 0;
            for (uint32_t i = 0; i < regions.size(); i++) {
                totalGIO4Size += (1 + regions[i].second - regions[i].first) / 2;
            }
            this->glyphIndexOffsets4.resize(totalGIO4Size);
            totalGIO4Size = 0;
            std::vector<uint32_t> startIdxs;
            startIdxs.resize(regions.size());
            for (uint32_t i = 0; i < regions.size(); i++) {
                size_t segSize = (1 + regions[i].second - regions[i].first) / 2;
                (*ptr) = regions[i].first;
                for (size_t j = 0; j < segSize; j++) glyphIndexOffsets4[totalGIO4Size + j] = getUint16(contents, ptr);
                startIdxs[i] = totalGIO4Size;
                totalGIO4Size += segSize;
            }

            (*ptr) = rangeOffsetsLoc;
            for (uint32_t i = 0; i < segCount; i++) {
                uint16_t idRangeOffset = getUint16(contents, ptr);
                segments[i].s.seg4.glyphIndexOffsetStart = UINT16_MAX;
                if (idRangeOffset == 0) continue;
                uint32_t numCodes = 1 + segments[i].endCode - segments[i].startCode;
                size_t regionStart = *ptr + idRangeOffset - 2;
                for (uint32_t reg = 0; reg < regions.size(); reg++) {
                    if (regions[reg].first <= regionStart && regions[reg].second >= (regionStart + 2 * numCodes)) {
                        uint32_t offs = startIdxs[reg] + (regionStart - regions[reg].first) / 2;
                        assert(offs < UINT16_MAX);
                        segments[i].s.seg4.glyphIndexOffsetStart = offs;
                        break;
                    }
                }
                if (segments[i].s.seg4.glyphIndexOffsetStart == UINT16_MAX) {
                    throw std::runtime_error("Failed to generate complete region list for glyphIndexOffsets4 compactification.");
                }
            }
            break;
        }
        case 12: {
            (*ptr) += 10;
            uint32_t nGroups = getUint32(contents, ptr);
            segments.resize(nGroups);
            for (uint32_t i = 0; i < nGroups; i++) {
                segments[i].startCode = getUint32(contents, ptr);
                segments[i].endCode = getUint32(contents, ptr);
                segments[i].s.seg12.startGlyphCode = getUint32(contents, ptr);
            }
            break;
        }
        default:
            throw std::runtime_error("Unrecognized 'cmap' mapping table format.");
    }
}

uint32_t TTFCmapTable::getGlyphIndex(uint32_t charCode) {
    uint32_t lowSegment = 0;
    uint32_t highSegment = segments.size() - 1;
    while (highSegment >= lowSegment) {
        uint32_t seg = (lowSegment + highSegment) / 2;
        if (charCode > segments[seg].endCode) {
            lowSegment = seg + 1;
        } else if (charCode < segments[seg].startCode) {
            highSegment = seg - 1;
        } else {
            switch (this->format) {
                case 4:
                    if (segments[seg].s.seg4.glyphIndexOffsetStart == UINT16_MAX)
                        return (charCode + segments[seg].s.seg4.idDelta) & 0xffff;
                    else {
                        uint32_t arrIndex = segments[seg].s.seg4.glyphIndexOffsetStart + charCode - segments[seg].startCode;
                        uint16_t indexOffset = glyphIndexOffsets4[arrIndex];
                        if (indexOffset != 0) return (indexOffset + segments[seg].s.seg4.idDelta) & 0xffff;
                    }
                    return 0;
                case 12:
                    return segments[seg].s.seg12.startGlyphCode + (charCode - segments[seg].startCode);
            }
        }
    }
    return 0;  // Missing character
}

TTFGlyph::TTFGlyph(const std::vector<char>& contents, size_t* ptr) {
    int16_t numContours = getInt16(contents, ptr);
    xmin = getInt16(contents, ptr);
    ymin = getInt16(contents, ptr);
    xmax = getInt16(contents, ptr);
    ymax = getInt16(contents, ptr);
    if (numContours < 0) {
        this->parseCompound(contents, ptr);
        return;
    }

    contourEndIndices.resize(numContours);
    for (int16_t i = 0; i < numContours; i++) {
        contourEndIndices[i] = getUint16(contents, ptr);
    }
    uint16_t instructionLength = getUint16(contents, ptr);
    (*ptr) += instructionLength;

    std::vector<uint8_t> flags;
    uint32_t numPoints = contourEndIndices[contourEndIndices.size() - 1] + 1;
    while (flags.size() < numPoints) {
        uint8_t flag = getUint8(contents, ptr);
        flags.push_back(flag);
        if ((flag >> 3) & 1) {
            uint8_t rpt = getUint8(contents, ptr);
            for (uint8_t i = 0; i < rpt; i++) flags.push_back(flag);
        }
    }
    onCurve.resize(numPoints);
    for (uint32_t i = 0; i < numPoints; i++) onCurve[i] = ((flags[i] & 1) > 0);
    xCoords.reserve(numPoints);
    yCoords.reserve(numPoints);
    while (xCoords.size() < numPoints) {
        int16_t lastX = xCoords.size() > 0 ? xCoords[xCoords.size() - 1] : 0;
        if (((flags[xCoords.size()] >> 1) & 1) > 0) {
            int16_t offset = getUint8(contents, ptr);
            xCoords.push_back(((flags[xCoords.size()] >> 4) & 1) > 0 ? (lastX + offset) : (lastX - offset));
        } else {
            if (((flags[xCoords.size()] >> 4) & 1) > 0) {
                xCoords.push_back(lastX);
            } else {
                xCoords.push_back(lastX + getInt16(contents, ptr));
            }
        }
    }
    while (yCoords.size() < numPoints) {
        int16_t lastY = yCoords.size() > 0 ? yCoords[yCoords.size() - 1] : 0;
        if (((flags[yCoords.size()] >> 2) & 1) > 0) {
            int16_t offset = getUint8(contents, ptr);
            yCoords.push_back(((flags[yCoords.size()] >> 5) & 1) > 0 ? (lastY + offset) : (lastY - offset));
        } else {
            if (((flags[yCoords.size()] >> 5) & 1) > 0) {
                yCoords.push_back(lastY);
            } else {
                yCoords.push_back(lastY + getInt16(contents, ptr));
            }
        }
    }

    // Bounds check
    int16_t rxmin = INT16_MAX, rymin = INT16_MAX, rxmax = INT16_MIN, rymax = INT16_MIN;
    for (uint32_t i = 0; i < numPoints; i++) {
        if (xCoords[i] < rxmin) rxmin = xCoords[i];
        if (xCoords[i] > rxmax) rxmax = xCoords[i];
        if (yCoords[i] < rymin) rymin = yCoords[i];
        if (yCoords[i] > rymax) rymax = yCoords[i];
    }

    if (rxmin != xmin || rymin != ymin || rxmax != xmax || rymax != ymax) {
        printf("Warning: Failed bounds check for TTF glyph.\n");
    }
}

static inline constexpr float maxf(float a, float b) {
    return (a > b) ? a : b;
}

void TTFGlyph::parseCompound(const std::vector<char>& contents, size_t* ptr) {
    this->isCompound = true;
    for (;;) {
        TTFCompoundGlyphComponent component;
        uint16_t flags = getUint16(contents, ptr);
        component.glyphIdx = (uint32_t)getUint16(contents, ptr);
        if ((flags & (1 << 1)) == 0) {
            throw std::runtime_error("Point matching in TTF compound glyphs is unimplemented.");
        }
        int16_t offsX = ((flags & (1 << 0)) > 0) ? getInt16(contents, ptr) : getInt8(contents, ptr);
        int16_t offsY = ((flags & (1 << 0)) > 0) ? getInt16(contents, ptr) : getInt8(contents, ptr);
        if ((flags & (1 << 3)) > 0) {
            component.transform = glm::mat2(getF2Dot14(contents, ptr));
        } else if ((flags & (1 << 6)) > 0) {
            float scaleX = getF2Dot14(contents, ptr);
            component.transform = glm::mat2(scaleX, 0.f, 0.f, getF2Dot14(contents, ptr));
        } else if ((flags & (1 << 7)) > 0) {
            float a = getF2Dot14(contents, ptr);
            float b = getF2Dot14(contents, ptr);
            float c = getF2Dot14(contents, ptr);
            float d = getF2Dot14(contents, ptr);
            component.transform = glm::mat2(a, c, b, d);
        } else {
            component.transform = glm::mat2(1.f);
        }
        float m0 = maxf(fabs(component.transform[0][0]), fabs(component.transform[0][1]));
        float n0 = maxf(fabs(component.transform[1][0]), fabs(component.transform[1][1]));
        float dac = fabs(fabs(component.transform[0][0]) - fabs(component.transform[1][0]));
        if (dac < (33.f / 65536.f)) m0 *= 2;
        float dbd = fabs(fabs(component.transform[0][1]) - fabs(component.transform[1][1]));
        if (dbd < (33.f / 65536.f)) n0 *= 2;
        component.transform = glm::mat2(component.transform[0][0] / m0, component.transform[0][1] / m0,
                                        component.transform[1][0] / n0, component.transform[1][1] / n0);
        component.offset = glm::vec2(m0, n0) * glm::vec2(offsX, offsY);
        this->compoundComponents.push_back(component);
        if ((flags & (1 << 5)) == 0) break;
    }
}

glm::vec2 TTFGlyph::getPoint(uint32_t index, TTFHeadTable& head) {
    glm::vec2 point;
    point.x = head.applyScale(xCoords[index]);
    point.y = 0.5f - head.applyScale(yCoords[index]);
    return point;
}

TTFParser::TTFParser(const std::vector<char>& ttfContents) {
    size_t ttfPtr = 0;
    TTFOffsetSubtable offsetSubtable;
    offsetSubtable.create(ttfContents, &ttfPtr);
    if (offsetSubtable.numTables >= 64) {
        throw std::runtime_error("TTF file appears to have too many tables.");
    }
    std::vector<TTFTableDirectory> tables;
    tables.resize(offsetSubtable.numTables);
    for (uint32_t i = 0; i < offsetSubtable.numTables; i++) {
        tables[i].create(ttfContents, &ttfPtr);
        if (!tables[i].checkChecksum(ttfContents)) {
            if (strncmp(tables[i].tag, "head", 4) != 0) {  // Head table checksum is special, we do not check it here.
                throw std::runtime_error("Failed checksum check for TTF table.");
            }
        }
        if (strncmp(tables[i].tag, "GSUB", 4) == 0) {
            printf(
                "Warning: Loading TTF which contains a 'GSUB' table, which is currently unimplemented. "
                "Ligatures and other 'GSUB' behavior will not work properly.\n");
        }
    }

    ttfPtr = findTable("head", tables)->offset;
    head.create(ttfContents, &ttfPtr);
    ttfPtr = findTable("maxp", tables)->offset;
    maxp.create(ttfContents, &ttfPtr);
    ttfPtr = findTable("loca", tables)->offset;
    loca.create(maxp, head, ttfContents, &ttfPtr);
    ttfPtr = findTable("cmap", tables)->offset;
    cmap.create(ttfContents, &ttfPtr);
    ttfPtr = findTable("hhea", tables)->offset;
    hhea.create(ttfContents, &ttfPtr);
    ttfPtr = findTable("hmtx", tables)->offset;
    hmtx.create(hhea, ttfContents, &ttfPtr);

    size_t totalGlyfSize = loca.glyphOffsets[loca.glyphOffsets.size() - 1];
    glyfData.resize(totalGlyfSize);
    size_t glyfOffset = findTable("glyf", tables)->offset;
    memcpy(glyfData.data(), ttfContents.data() + glyfOffset, totalGlyfSize);
}