#include "TextFont.h"

#include "../vulkancore/VulkanUtils.h"
#include "Entity2d.h"

using namespace lepton2::graphics2d;
using namespace lepton2::vulkancore;

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

static int16_t getInt16(const std::vector<char>& contents, size_t* ptr) {
    uint16_t result = getUint16(contents, ptr);
    return *(int16_t*)(&result);
}

static int32_t getInt32(const std::vector<char>& contents, size_t* ptr) {
    uint32_t result = getUint32(contents, ptr);
    return *(int32_t*)(&result);
}

struct TTFOffsetSubtable {
    void create(const std::vector<char>& contents, size_t* ptr) {
        (*ptr) += 4;
        numTables = getUint16(contents, ptr);
        (*ptr) += 6;
    }
    uint16_t numTables;
};

struct TTFTableDirectory {
    void create(const std::vector<char>& contents, size_t* ptr) {
        *(uint32_t*)tag = swapEndian32(getUint32(contents, ptr));
        checksum = getUint32(contents, ptr);
        offset = getUint32(contents, ptr);
        length = getUint32(contents, ptr);
    }
    char tag[4];
    uint32_t checksum;
    uint32_t offset;
    uint32_t length;
    bool checkChecksum(const std::vector<char>& contents) {
        uint32_t* table = (uint32_t*)(contents.data() + offset);
        uint32_t nLongs = (length + 3) / 4;
        uint32_t sum = 0;
        while (nLongs-- > 0) sum += swapEndian32(*table++);
        return (sum == checksum);
    }
};

static TTFTableDirectory* findTable(const char* tag, std::vector<TTFTableDirectory>& tables) {
    for (uint32_t i = 0; i < tables.size(); i++) {
        if (strncmp(tables[i].tag, tag, 4) == 0) return &tables[i];
    }
    throw std::runtime_error("Failed to locate TTF table.");
}

struct TTFHeadTable {
    void create(const std::vector<char>& contents, size_t* ptr) {
        (*ptr) += 18;
        unitsPerEm = getUint16(contents, ptr);
        (*ptr) += 30;
        int16_t indexToLocFormat = getInt16(contents, ptr);
        if (indexToLocFormat != 0 && indexToLocFormat != 1) {
            throw std::runtime_error("Unrecognized 'head'->indexToLocFormat value.");
        }
        longLocFormat = (bool)(indexToLocFormat);
    }
    inline float applyScale(int16_t dist) {
        return ((float)dist) / ((float)unitsPerEm);
    }
    uint16_t unitsPerEm;
    bool longLocFormat;
};

struct TTFMaxpTable {
    void create(const std::vector<char>& contents, size_t* ptr) {
        (*ptr) += 4;
        numGlyphs = getUint16(contents, ptr);
    }
    uint16_t numGlyphs;
};

struct TTFHheaTable {
    void create(const std::vector<char>& contents, size_t* ptr) {
        (*ptr) += 34;
        numOfLongHorMetrics = getUint16(contents, ptr);
    }
    uint16_t numOfLongHorMetrics;
};

struct TTFHmtxTable {
    void create(TTFHheaTable& hhea, const std::vector<char>& contents, size_t* ptr) {
        advanceWidths.resize(hhea.numOfLongHorMetrics);
        for (uint32_t i = 0; i < hhea.numOfLongHorMetrics; i++) {
            advanceWidths[i] = getUint16(contents, ptr);
            (*ptr) += 2;  // leftSideBearing
        }
    }
    uint16_t getAdvanceWidth(uint32_t glyph) {
        if (glyph >= advanceWidths.size()) return advanceWidths[advanceWidths.size() - 1];
        return advanceWidths[glyph];
    }
    std::vector<uint16_t> advanceWidths;
};

struct TTFLocaTable {
    void create(TTFMaxpTable& maxp, TTFHeadTable& head, const std::vector<char>& contents, size_t* ptr) {
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
    std::vector<uint32_t> glyphOffsets;
};

// Specifies priorities for Unicode platformSpecificID
static const uint8_t unicodePSIDKeys[] = {1, 2, 0, 3, 4, 0, 0};

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
    void create(const std::vector<char>& contents, size_t* ptr) {
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

    uint32_t getGlyphIndex(uint32_t charCode) {
        uint32_t lowSegment = 0;
        uint32_t highSegment = segments.size() - 1;
        while (highSegment >= lowSegment) {
            uint32_t seg = (lowSegment + highSegment) / 2;
            if (charCode > segments[seg].endCode) {
                lowSegment = seg + 1;
            } else if (charCode < segments[seg].startCode) {
                highSegment = seg;
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

    std::vector<_TTFCmapSegment> segments;
    std::vector<uint16_t> glyphIndexOffsets4;
    uint16_t format;
};

struct TTFGlyph {
    TTFGlyph(const std::vector<char>& contents, size_t* ptr) {
        int16_t numContours = getInt16(contents, ptr);
        if (numContours < 0) throw std::runtime_error("Compound glyph.\n");
        xmin = getInt16(contents, ptr);
        ymin = getInt16(contents, ptr);
        xmax = getInt16(contents, ptr);
        ymax = getInt16(contents, ptr);

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

    glm::vec2 getPoint(uint32_t index, TTFHeadTable& head) {
        glm::vec2 point;
        point.x = head.applyScale(xCoords[index]);
        point.y = 0.5f - head.applyScale(yCoords[index]);
        return point;
    }

    std::vector<uint16_t> contourEndIndices;
    int16_t xmin, xmax, ymin, ymax;
    std::vector<int16_t> xCoords;
    std::vector<int16_t> yCoords;
    std::vector<bool> onCurve;
};

namespace lepton2::graphics2d {
class TTFParser {
   public:
    TTFParser(const std::vector<char>& ttfContents) {
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

    TTFHeadTable head;
    TTFMaxpTable maxp;
    TTFLocaTable loca;
    TTFCmapTable cmap;
    TTFHheaTable hhea;
    TTFHmtxTable hmtx;
    std::vector<char> glyfData;
};
}  // namespace lepton2::graphics2d

struct Triangle {
    uint32_t t[3];
    glm::vec2 cc;
    float cr;
};

static inline constexpr float cross2d(glm::vec2 v1, glm::vec2 v2) {
    return v1.x * v2.y - v1.y * v2.x;
}

static bool intersectionTest(glm::vec2 p1, glm::vec2 p2, glm::vec2 q1, glm::vec2 q2, float clip) {
    glm::vec2 p1p2 = p1 - p2, q1q2 = q1 - q2;
    float d = cross2d(p1p2, q1q2);
    if (fabs(d) < 0.001) return false;
    float s = cross2d(q2 - p2, p1p2) / d;
    if ((0.5 - fabs(s - 0.5)) < clip) return false;
    float t = cross2d(q2 - p2, q1q2) / d;
    return ((0.5 - fabs(t - 0.5)) >= clip);
}

#define VEC2_YX(v) glm::vec2(v.y, v.x)
#define VEC2_D11(v) glm::dot(glm::vec2(1.f, 1.f), v)

static void pushTriangle(std::vector<SimpleVertex2d>& points, std::vector<Triangle>& tris, uint32_t t0, uint32_t t1, uint32_t t2) {
    glm::vec2 p0 = points[t0].pos2d;
    glm::vec2 p1 = points[t1].pos2d;
    glm::vec2 p2 = points[t2].pos2d;

    Triangle tri;
    tri.t[0] = t0;
    tri.t[1] = t1;
    tri.t[2] = t2;
    // Compute circumcircle - good luck working this one out
    glm::vec2 d01 = p0 - p1, d12 = p1 - p2;
    tri.cc = 0.5f * (glm::dot(p1 + p2, d12) * VEC2_YX(d01) - glm::dot(p0 + p1, d01) * VEC2_YX(d12)) / (d12 * VEC2_YX(d01) - d01 * VEC2_YX(d12));
    tri.cr = glm::distance(tri.cc, p0);

    tris.push_back(tri);
}

static inline constexpr bool triangleContainsEdge(Triangle& tri, uint32_t e0, uint32_t e1) {
    if (tri.t[0] != e0 && tri.t[1] != e0 && tri.t[2] != e0) return false;
    return (tri.t[0] == e1 || tri.t[1] == e1 || tri.t[2] == e1);
}

static uint8_t findTriangleTexcoord(std::vector<SimpleVertex2d>& points, Triangle* tri, glm::vec2 target) {
    if (glm::distance(points[tri->t[0]].texcoord, target) < 1e-6) return 0;
    if (glm::distance(points[tri->t[1]].texcoord, target) < 1e-6) return 1;
    if (glm::distance(points[tri->t[2]].texcoord, target) < 1e-6) return 2;
    return 255;
}

static inline void replaceVertexTexcoord(std::vector<SimpleVertex2d>& points, Triangle* tri, uint8_t idx, glm::vec2 target) {
    SimpleVertex2d vert = {points[tri->t[idx]].pos2d, target};
    tri->t[idx] = points.size();
    points.push_back(vert);
}

static void repairTriangleTexcoord(std::vector<SimpleVertex2d>& points, Triangle* tri, glm::vec2* targets) {
    uint8_t idx0 = findTriangleTexcoord(points, tri, targets[0]);
    uint8_t idx1 = findTriangleTexcoord(points, tri, targets[1]);
    uint8_t idx2 = findTriangleTexcoord(points, tri, targets[2]);
    if ((idx0 == 255 ? 1 : 0) + (idx1 == 255 ? 1 : 0) + (idx2 == 255 ? 1 : 0) > 1) {
        printf("Warning: Failed to repair triangle texture coordinate data - possible texcoord heuristic failure.\n");
        return;
    }
    if (idx0 == 255) replaceVertexTexcoord(points, tri, 3 - idx1 - idx2, targets[0]);
    if (idx1 == 255) replaceVertexTexcoord(points, tri, 3 - idx0 - idx2, targets[1]);
    if (idx2 == 255) replaceVertexTexcoord(points, tri, 3 - idx0 - idx1, targets[2]);
}

// Use Bowyer-Watson algorithm to build Delaunay triangulation
static void triangulate(std::vector<SimpleVertex2d>& points, std::vector<Triangle>& tris) {
    // Bounding triangle
    glm::vec2 bbox_min = glm::vec2(1.f / 0.f);
    glm::vec2 bbox_max = glm::vec2(-1.f / 0.f);
    for (SimpleVertex2d pt : points) {
        if (pt.pos2d.x < bbox_min.x) bbox_min.x = pt.pos2d.x;
        if (pt.pos2d.y < bbox_min.y) bbox_min.y = pt.pos2d.y;
        if (pt.pos2d.x > bbox_max.x) bbox_max.x = pt.pos2d.x;
        if (pt.pos2d.y > bbox_max.y) bbox_max.y = pt.pos2d.y;
    }
    glm::vec2 bbox_pad = 0.2f * (bbox_max - bbox_min);
    bbox_min -= bbox_pad;
    bbox_max += bbox_pad;
    points.push_back({glm::vec2(1.5f * bbox_min.x - 0.5f * bbox_max.x, bbox_min.y), glm::vec2(0)});
    points.push_back({glm::vec2(1.5f * bbox_max.x - 0.5f * bbox_min.x, bbox_min.y), glm::vec2(0)});
    points.push_back({glm::vec2(0.5f * bbox_min.x + 0.5f * bbox_max.x, 2.f * bbox_max.y - bbox_min.y), glm::vec2(0)});
    pushTriangle(points, tris, points.size() - 3, points.size() - 2, points.size() - 1);

    std::vector<Triangle> badtris;
    for (uint32_t idx = 0; idx < points.size() - 3; idx++) {
        glm::vec2 pt = points[idx].pos2d;
        for (uint32_t tridx = tris.size() - 1; tridx < tris.size(); tridx--) {
            Triangle* tri = &tris[tridx];
            if (glm::distance(pt, tri->cc) <= tri->cr) {
                badtris.push_back(*tri);
                tris[tridx] = tris[tris.size() - 1];
                tris.pop_back();
            }
        }

        for (uint32_t tridx = 0; tridx < badtris.size(); tridx++) {
            Triangle* tri = &badtris[tridx];
            const uint32_t* e0 = tri->t;
            const uint32_t e1[3] = {e0[1], e0[2], e0[0]};
            for (uint8_t i = 0; i < 3; i++) {
                bool edgeNotShared = true;
                for (uint32_t tridx2 = 0; tridx2 < badtris.size(); tridx2++) {
                    if (tridx2 == tridx) continue;
                    if (triangleContainsEdge(badtris[tridx2], e0[i], e1[i])) {
                        edgeNotShared = false;
                        break;
                    }
                }
                if (edgeNotShared) {
                    pushTriangle(points, tris, idx, e0[i], e1[i]);
                }
            }
        }
        badtris.clear();
    }
}

static void addUniqueIndex(std::vector<uint32_t>& vec, uint32_t idx) {
    for (uint32_t i = 0; i < vec.size(); i++) {
        if (vec[i] == idx) return;
    }
    vec.push_back(idx);
}

struct MonotoneIndexComparator {
    MonotoneIndexComparator(std::vector<SimpleVertex2d>* points, glm::vec2 q1, glm::vec2 q2) {
        this->q1 = q1;
        this->q2 = q2;
        this->points = points;
    }
    bool operator()(uint32_t i1, uint32_t i2) {
        float d1 = glm::dot(points->at(i1).pos2d - q1, q2 - q1);
        float d2 = glm::dot(points->at(i2).pos2d - q1, q2 - q1);
        return (d1 <= d2);
    }
    std::vector<SimpleVertex2d>* points;
    glm::vec2 q1, q2;
};

void fillMonotone(std::vector<SimpleVertex2d>& points, std::vector<Triangle>& tris, std::vector<uint32_t>& chain, float fillsign) {
    std::vector<uint32_t> stack;
    for (uint32_t idx : chain) {
        while (stack.size() >= 2) {
            glm::vec2 d1 = points[stack[stack.size() - 1]].pos2d - points[stack[stack.size() - 2]].pos2d;
            glm::vec2 d2 = points[idx].pos2d - points[stack[stack.size() - 1]].pos2d;
            if (cross2d(d1, d2) * fillsign < 0) break;
            pushTriangle(points, tris, stack[stack.size() - 2], stack[stack.size() - 1], idx);
            stack.pop_back();
        }
        stack.push_back(idx);
    }
    if (stack.size() != 2) {
        printf("Warning: Invalid stack state after fillMonotone during edge cut TTF processing phase.\n");
    }
}

// Cut across a triangulation to enforce polygon edges
static void cutEdge(std::vector<SimpleVertex2d>& points, std::vector<Triangle>& tris, uint32_t f0, uint32_t f1) {
    std::vector<uint32_t> chain1;  // While asymptotically faster, std::unordered_set would
    std::vector<uint32_t> chain2;  // introduce unnecessary overhead for sets this small
    chain1.push_back(f0);
    chain2.push_back(f0);
    glm::vec2 q1 = points[f0].pos2d, q2 = points[f1].pos2d;
    MonotoneIndexComparator comp(&points, q1, q2);
    for (uint32_t tridx = tris.size() - 1; tridx < tris.size(); tridx--) {
        Triangle* tri = &tris[tridx];
        const uint32_t* e0 = tri->t;
        const uint32_t e1[3] = {e0[1], e0[2], e0[0]};
        for (uint8_t i = 0; i < 3; i++) {
            glm::vec2 p1 = points[e0[i]].pos2d, p2 = points[e1[i]].pos2d;
            if (intersectionTest(p1, p2, q1, q2, 1e-4)) {
                for (uint8_t j = 0; j < 3; j++) {
                    float c2d = cross2d(points[e0[j]].pos2d - q1, q2 - q1);
                    if (c2d < -1e-6) addUniqueIndex(chain1, e0[j]);
                    if (c2d > 1e-6) addUniqueIndex(chain2, e0[j]);
                }
                tris[tridx] = tris[tris.size() - 1];
                tris.pop_back();
                break;
            }
        }
    }
    std::sort(chain1.begin() + 1, chain1.end(), comp);
    std::sort(chain2.begin() + 1, chain2.end(), comp);
    chain1.push_back(f1);
    chain2.push_back(f1);
    fillMonotone(points, tris, chain1, -1.f);
    fillMonotone(points, tris, chain2, 1.f);
}

static inline uint32_t getNextPoint(std::vector<uint32_t>& contours, uint32_t cnt, uint32_t pt) {
    return (pt < contours[cnt]) ? (pt + 1) : ((cnt == 0) ? 0 : (contours[cnt - 1] + 1));
}

static inline uint32_t getPrevPoint(std::vector<uint32_t>& contours, uint32_t cnt, uint32_t pt) {
    return (pt > ((cnt == 0) ? 0 : (contours[cnt - 1] + 1))) ? (pt - 1) : contours[cnt];
}

static inline uint8_t getQuadrant(glm::vec2 p) {
    return (p.x > 0) ? ((p.y > 0) ? 1 : 4) : ((p.y > 0) ? 2 : 3);
}

static inline bool pointInPolygon(std::vector<std::pair<glm::vec2, glm::vec2>> edges, glm::vec2 point) {
    int8_t winding = 0;
    for (std::pair<glm::vec2, glm::vec2> edge : edges) {
        glm::vec2 e1 = edge.first - point;
        glm::vec2 e2 = edge.second - point;
        uint8_t q1 = getQuadrant(e1);
        uint8_t q2 = getQuadrant(e2);
        int8_t qd = (int8_t)(((q2 - q1) + 5) & 3) - 1;
        if (qd != 2) {
            winding += qd;
            continue;
        }
        float xint = cross2d(e1, e2) / (e2.y - e1.y);
        winding += ((q1 < q2) != (xint > 0)) ? 2 : -2;
    }
    return (winding != 0);
}

TextFont::TextFont(VulkanContext* ctx, const char* ttf) {
    char* buf = ctx->buildAssetLoadPath(ttf);
    std::vector<char> ttfContents = lepton2::utils::readFile(buf);
    free(buf);
    this->parser = new TTFParser(ttfContents);
}

bool TextFont::containsGlyph(uint32_t codepoint) {
    uint32_t glyphIndex = parser->cmap.getGlyphIndex(codepoint);
    return (glyphIndex != 0);
}

// #define GLYPH_LOAD_PROFILING

ProcessedGlyph* TextFont::getGlyph(VulkanContext* ctx, uint32_t codepoint) {
    if (glyphMap.count(codepoint) > 0) {
        return glyphMap[codepoint];  // I'm so dynamic
    }

#ifdef GLYPH_LOAD_PROFILING
    lepton2::utils::lepton2_time_point start_time = lepton2::utils::startTiming();
    printf("Start building glyph for codepoint %u\n", codepoint);
#endif

    ProcessedGlyph* glyph = new ProcessedGlyph();
    uint32_t glyphIndex = parser->cmap.getGlyphIndex(codepoint);
    glyph->advanceWidth = parser->head.applyScale(parser->hmtx.getAdvanceWidth(glyphIndex));

    size_t glyfPtr = parser->loca.glyphOffsets[glyphIndex];

    if (glyfPtr == UINT32_MAX) {
        glyph->objData = nullptr;
        glyphMap[codepoint] = glyph;
        return glyph;
    }

    TTFGlyph glyph_raw(parser->glyfData, &glyfPtr);

    std::vector<SimpleVertex2d> coords;
    std::vector<bool> onCurve;
    onCurve.reserve(glyph_raw.onCurve.size());
    coords.reserve(glyph_raw.onCurve.size());

    std::vector<uint32_t> endIndices;
    endIndices.resize(glyph_raw.contourEndIndices.size());

    // Add implied on-curve points and apply coordinate transforms
    uint32_t contourStart = 0;
    for (uint32_t cnt = 0; cnt < glyph_raw.contourEndIndices.size(); cnt++) {
        uint32_t contourEnd = glyph_raw.contourEndIndices[cnt];
        for (uint32_t i = contourStart; i <= contourEnd; i++) {
            glm::vec2 point = glyph_raw.getPoint(i, parser->head);
            coords.push_back({point, glm::vec2(0.5f)});
            onCurve.push_back(glyph_raw.onCurve[i]);
            if (glyph_raw.onCurve[i] || i == contourEnd || glyph_raw.onCurve[i + 1]) continue;
            glm::vec2 nextpoint = glyph_raw.getPoint(i + 1, parser->head);
            coords.push_back({0.5f * (point + nextpoint), glm::vec2(0.5f)});
            onCurve.push_back(true);
        }
        if (!glyph_raw.onCurve[contourStart] && !glyph_raw.onCurve[contourEnd]) {
            glm::vec2 point1 = glyph_raw.getPoint(contourStart, parser->head);
            glm::vec2 point2 = glyph_raw.getPoint(contourEnd, parser->head);
            coords.push_back({0.5f * (point1 + point2), glm::vec2(0.5f)});
            onCurve.push_back(true);
        }
        contourStart = contourEnd + 1;
        endIndices[cnt] = coords.size() - 1;
    }

#ifdef GLYPH_LOAD_PROFILING
    printf("Generated implied polygon at %lf seconds\n", lepton2::utils::getElapsedSeconds(start_time));
#endif

    std::vector<Triangle> tris;
    triangulate(coords, tris);

#ifdef GLYPH_LOAD_PROFILING
    printf("Built %u Delaunay triangles at %lf seconds\n", tris.size(), lepton2::utils::getElapsedSeconds(start_time));
#endif

    uint32_t cstart = 0;
    // Definitely the step which should be optimized using quadtree location techniques
    for (uint32_t cnt = 0; cnt < endIndices.size(); cnt++) {
        for (uint32_t t = cstart; t < endIndices[cnt] + 1; t++) {
            uint32_t next = getNextPoint(endIndices, cnt, t);
            cutEdge(coords, tris, t, next);
            // printf("Cut %u->%u (1)\n", t, next);
            if (!onCurve[next]) {
                // printf("Cut %u->%u (2)\n", t, getNextPoint(endIndices, cnt, next));
                cutEdge(coords, tris, t, getNextPoint(endIndices, cnt, next));
            }
        }
        cstart = endIndices[cnt] + 1;
    }

    coords.resize(coords.size() - 3);  // Remove bounding triangle points

#ifdef GLYPH_LOAD_PROFILING
    printf("Completed edge cuts at %lf seconds\n", lepton2::utils::getElapsedSeconds(start_time));
#endif

    std::vector<std::pair<glm::vec2, glm::vec2>> edgesSet1;
    std::vector<std::pair<glm::vec2, glm::vec2>> edgesSet2;

    cstart = 0;
    for (uint32_t cnt = 0; cnt < endIndices.size(); cnt++) {
        uint32_t prevIdx = UINT32_MAX, firstIdx = UINT32_MAX;
        bool texState = false;
        for (uint32_t t = cstart; t != firstIdx; t = getNextPoint(endIndices, cnt, t)) {
            if (!onCurve[t]) {
                texState = !texState;
                continue;
            }
            if (prevIdx != UINT32_MAX) {
                if (firstIdx == UINT32_MAX) firstIdx = t;
                coords[t].texcoord = glm::vec2((texState ? 0.75f : 0.25f), 0.5f);
                edgesSet1.push_back({coords[t].pos2d, coords[prevIdx].pos2d});
            }
            prevIdx = t;
        }
        prevIdx = UINT32_MAX, firstIdx = UINT32_MAX;
        for (uint32_t t = cstart; t != firstIdx; t = getNextPoint(endIndices, cnt, t)) {  // FIXME: just all edges
            if (prevIdx != UINT32_MAX) {
                if (firstIdx == UINT32_MAX) firstIdx = t;
                edgesSet2.push_back({coords[t].pos2d, coords[prevIdx].pos2d});
            }
            prevIdx = t;
        }
        cstart = endIndices[cnt] + 1;
    }

    std::vector<uint8_t> triangleFlags;
    triangleFlags.resize(tris.size());
    for (uint32_t i = 0; i < tris.size(); i++) {
        Triangle* tri = &tris[i];
        triangleFlags[i] = 0;
        if (tri->t[0] >= coords.size() || tri->t[1] >= coords.size() || tri->t[2] >= coords.size()) {
            continue;
        }
        glm::vec2 p1 = (coords[tri->t[0]].pos2d + coords[tri->t[1]].pos2d + coords[tri->t[2]].pos2d) / 3.f;
        if (pointInPolygon(edgesSet1, p1)) triangleFlags[i] |= 1;
        if (pointInPolygon(edgesSet2, p1)) triangleFlags[i] |= 2;
    }

#ifdef GLYPH_LOAD_PROFILING
    printf("Completed tri flagging at %lf seconds\n", lepton2::utils::getElapsedSeconds(start_time));
#endif

    for (uint32_t i = tris.size() - 1; i < tris.size(); i--) {
        if ((3 & triangleFlags[i]) == 0) {
            tris[i] = tris[tris.size() - 1];
            triangleFlags[i] = triangleFlags[tris.size() - 1];
            tris.pop_back();
            triangleFlags.pop_back();
        } else if (triangleFlags[i] != 3) {
            const uint32_t verts[3] = {tris[i].t[0], tris[i].t[1], tris[i].t[2]};
            for (uint32_t vert : verts) {
                if (coords[vert].texcoord.y != 0.5f) continue;
                if (fabs(coords[vert].texcoord.x - 0.5f) < 0.125f) {
                    coords[vert].texcoord = glm::vec2((triangleFlags[i] == 2) ? 1.f : 0.5f);
                    continue;
                } else if (fabs(coords[vert].texcoord.x - 0.5f) > 0.375f) {
                    continue;
                }
                if (triangleFlags[i] == 2) {
                    coords[vert].texcoord = (coords[vert].texcoord.x > 0.5f) ? glm::vec2(1.f, 0.5f) : glm::vec2(0.5f, 1.f);
                } else {
                    coords[vert].texcoord = (coords[vert].texcoord.x > 0.5f) ? glm::vec2(0.5f, 0.f) : glm::vec2(0.f, 0.5f);
                }
            }
        }
    }

    for (uint32_t vert = 0; vert < coords.size(); vert++) {
        if (coords[vert].texcoord.y != 0.5f) continue;
        if (fabs(coords[vert].texcoord.x - 0.5f) < 0.375f) {
            coords[vert].texcoord = glm::vec2(0.5f);
        }
    }

#ifdef GLYPH_LOAD_PROFILING
    printf("Completed texcoord heuristic at %lf seconds\n", lepton2::utils::getElapsedSeconds(start_time));
    uint32_t initialCoordsSize = coords.size();
#endif

    // This simple approach may introduce some duplicate vertices. Further deduplication or some sort of
    // candidate repair match detection system could be implemented if willing to go even more insane.
    for (uint32_t tridx = 0; tridx < tris.size(); tridx++) {
        Triangle* tri = &tris[tridx];
        switch (triangleFlags[tridx]) {
            case 1: {
                glm::vec2 targets[3] = {{0.f, 0.5f}, {0.5f, 0.f}, {0.5f, 0.5f}};
                repairTriangleTexcoord(coords, tri, targets);
                continue;
            }
            case 2: {
                glm::vec2 targets[3] = {{1.f, 0.5f}, {0.5f, 1.f}, {1.f, 1.f}};
                repairTriangleTexcoord(coords, tri, targets);
                continue;
            }
            case 3: {
                for (uint8_t j = 0; j < 3; j++) {
                    if (glm::distance(coords[tri->t[j]].texcoord, glm::vec2(1.f, 1.f)) < 1e-6) {
                        replaceVertexTexcoord(coords, tri, j, glm::vec2(0.5f));
                    }
                }
                uint8_t idx05 = findTriangleTexcoord(coords, tri, glm::vec2(0.f, 0.5f));
                uint8_t idx50 = findTriangleTexcoord(coords, tri, glm::vec2(0.5f, 0.f));
                if (idx05 != 255 && idx50 != 255) {
                    replaceVertexTexcoord(coords, tri, idx05, glm::vec2(0.5f));
                }
                continue;
            }
        }
    }

#ifdef GLYPH_LOAD_PROFILING
    printf("Completed %u conflicted vertex duplications at %lf seconds\n", (uint32_t)coords.size() - initialCoordsSize, lepton2::utils::getElapsedSeconds(start_time));
#endif

    std::vector<uint32_t> indices;
    indices.resize(3 * tris.size());
    for (uint32_t i = 0; i < tris.size(); i++) {
        indices[3 * i + 0] = tris[i].t[0];
        float a = cross2d(coords[tris[i].t[1]].pos2d - coords[tris[i].t[0]].pos2d,
                          coords[tris[i].t[2]].pos2d - coords[tris[i].t[1]].pos2d);
        if (a < 0) {  // Allow culling (not strictly necessary with pipeline modifications)
            indices[3 * i + 1] = tris[i].t[1];
            indices[3 * i + 2] = tris[i].t[2];
        } else {
            indices[3 * i + 1] = tris[i].t[2];
            indices[3 * i + 2] = tris[i].t[1];
        }
    }

    HostObjectData* hostObjData = new HostObjectData(coords.data(), coords.size() * sizeof(SimpleVertex2d), indices);

#ifdef GLYPH_LOAD_PROFILING
    printf("Built CPU-side glyph data in %lf seconds\n", lepton2::utils::getElapsedSeconds(start_time));
#endif

    DeviceObjectData* data = new DeviceObjectData(ctx, hostObjData);
    hostObjData->destroy(ctx);
    delete hostObjData;

    glyph->objData = data;

    glyphMap[codepoint] = glyph;
    return glyph;
}

void TextFont::destroy_back(VulkanContext* ctx) {
    delete this->parser;
    for (std::pair<uint32_t, ProcessedGlyph*> glyph : glyphMap) {
        glyph.second->destroy(ctx);
        delete glyph.second;
    }
}