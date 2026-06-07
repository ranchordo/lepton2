#include "TextFont.h"

#include "../vulkancore/VulkanUtils.h"
#include "Entity2d.h"

using namespace lepton2::graphics2d;
using namespace lepton2::vulkancore;
using namespace lepton2::graphics2d::ttfparse;

struct Triangle {
    uint32_t t[3];
    glm::vec2 cc;
    float cr;
};

static inline constexpr float cross2d(glm::vec2 v1, glm::vec2 v2) {
    return v1.x * v2.y - v1.y * v2.x;
}

static inline constexpr glm::vec2 lerp(glm::vec2 v1, glm::vec2 v2, float t) {
    return v1 * (1.f - t) + v2 * t;
}

static bool intersectionTest(glm::vec2 p1, glm::vec2 p2, glm::vec2 q1, glm::vec2 q2, float clip_p, float clip_q) {
    glm::vec2 p1p2 = p1 - p2, q1q2 = q1 - q2;
    float d = cross2d(p1p2, q1q2);
    if (fabs(d) < 0.001) {
        // Avoid failing just because edges are small - want true degeneracy test
        if (fabs(cross2d(glm::normalize(p1p2), glm::normalize(q1q2))) < 0.001) return false;
    }
    float s = cross2d(q2 - p2, p1p2) / d;
    if ((0.5 - fabs(s - 0.5)) < clip_q) return false;
    float t = cross2d(q2 - p2, q1q2) / d;
    return ((0.5 - fabs(t - 0.5)) >= clip_p);
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

static inline constexpr uint8_t triangleContainsEdge(Triangle& tri, uint32_t e0, uint32_t e1) {
    // Assumes the triangle does not contain a single point more than once
    if (tri.t[0] == e0 || tri.t[0] == e1) {
        if (tri.t[1] == e0 || tri.t[1] == e1) return 0;
        if (tri.t[2] == e0 || tri.t[2] == e1) return 2;
    } else {
        if (tri.t[1] == e0 || tri.t[1] == e1) {
            if (tri.t[2] == e0 || tri.t[2] == e1) return 1;
        }
    }
    return UINT8_MAX;
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
    uint8_t borked = (idx0 == 255 ? 1 : 0) + (idx1 == 255 ? 1 : 0) + (idx2 == 255 ? 1 : 0);
    if (borked == 0) return;
    if (borked > 1) {
        printf("Warning: Failed to repair (%u, %u, %u) texture coordinate data - possible texcoord heuristic failure.\n", tri->t[0], tri->t[1], tri->t[2]);
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
                    if (triangleContainsEdge(badtris[tridx2], e0[i], e1[i]) != UINT8_MAX) {
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

struct LinkedBoundaryVertex {
    uint32_t vertex;
    uint32_t idx1 = UINT32_MAX;  // Indices into vertex chains
    uint32_t idx2 = UINT32_MAX;  // Indices into vertex chains
};

static uint32_t addBlankBoundaryVertex(std::vector<LinkedBoundaryVertex>& vec, uint32_t idx) {
    for (uint32_t i = 0; i < vec.size(); i++) {
        if (vec[i].vertex == idx) return i;
    }
    vec.push_back({idx, UINT32_MAX, UINT32_MAX});
    return vec.size() - 1;
}

static void addVertexLink(LinkedBoundaryVertex* vert, uint32_t idx) {
    if (vert->idx1 == UINT32_MAX) {
        vert->idx1 = idx;
        return;
    }
    if (vert->idx2 != UINT32_MAX) {
        throw std::runtime_error("TTF edge cut phase: Attempting to add too many links to a boundary vertex.");
    }
    vert->idx2 = idx;
}

static uint32_t sortBoundaryChain(std::vector<LinkedBoundaryVertex>& chain, uint32_t start) {
    uint32_t prevIdx = UINT32_MAX;
    uint32_t idx = start;
    uint32_t count = 0;
    for (;;) {
        count++;
        if (count > chain.size() + 1) {
            throw std::runtime_error("TTF edge cut phase: cycle detected during boundary chain sorting.");
        }
        if (prevIdx == UINT32_MAX) {
            if (chain[idx].idx2 != UINT32_MAX) return UINT32_MAX;
            if (chain[idx].idx1 == UINT32_MAX) return UINT32_MAX;
            prevIdx = idx;
            idx = chain[idx].idx1;
            continue;
        }
        if (chain[idx].idx1 == UINT32_MAX) return UINT32_MAX;
        if (chain[idx].idx1 != prevIdx) {
            prevIdx = idx;
            idx = chain[idx].idx1;
            continue;
        }
        if (chain[idx].idx2 == UINT32_MAX) break;
        chain[idx].idx1 = chain[idx].idx2;
        prevIdx = idx;
        idx = chain[idx].idx1;
    }
    chain[idx].idx1 = UINT32_MAX;  // Terminate
    return chain[idx].vertex;
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

void fillMonotone(std::vector<SimpleVertex2d>& points, std::vector<Triangle>& tris, std::vector<LinkedBoundaryVertex>& chain,
                  uint32_t chainStart, float fillsign) {
    std::vector<uint32_t> stack;
    uint32_t chainIdx = chainStart;
    for (;;) {
        uint32_t idx = chain[chainIdx].vertex;
        while (stack.size() >= 2) {
            glm::vec2 d1 = points[stack[stack.size() - 1]].pos2d - points[stack[stack.size() - 2]].pos2d;
            glm::vec2 d2 = points[idx].pos2d - points[stack[stack.size() - 1]].pos2d;
            if (cross2d(d1, d2) * fillsign < 0) break;
            pushTriangle(points, tris, stack[stack.size() - 2], stack[stack.size() - 1], idx);
            stack.pop_back();
        }
        stack.push_back(idx);
        if (chain[chainIdx].idx1 == UINT32_MAX) break;
        chainIdx = chain[chainIdx].idx1;
    }
    if (stack.size() != 2) {
        printf("Warning: Invalid stack state after fillMonotone during edge cut TTF processing phase.\n");
    }
}

// Cut across a triangulation to enforce polygon edges
static void cutEdge(std::vector<SimpleVertex2d>& points, std::vector<Triangle>& tris, uint32_t f0, uint32_t f1) {
    std::vector<LinkedBoundaryVertex> chain1;
    std::vector<LinkedBoundaryVertex> chain2;
    uint32_t chain1Start = UINT32_MAX;
    uint32_t chain2Start = UINT32_MAX;
    glm::vec2 q1 = points[f0].pos2d, q2 = points[f1].pos2d;
    MonotoneIndexComparator comp(&points, q1, q2);
    std::vector<uint32_t> badIndices;
    for (uint32_t tridx = tris.size() - 1; tridx < tris.size(); tridx--) {
        Triangle* tri = &tris[tridx];
        const uint32_t* e0 = tri->t;
        const uint32_t e1[3] = {e0[1], e0[2], e0[0]};
        for (uint8_t i = 0; i < 3; i++) {
            glm::vec2 p1 = points[e0[i]].pos2d, p2 = points[e1[i]].pos2d;
            if (intersectionTest(p1, p2, q1, q2, 1e-4f, -1e-4f)) {
                uint8_t chainInfo[3] = {0, 0, 0};
                uint32_t index1Info[3] = {UINT32_MAX, UINT32_MAX, UINT32_MAX};
                uint32_t index2Info[3] = {UINT32_MAX, UINT32_MAX, UINT32_MAX};
#pragma unroll 3
                for (uint8_t j = 0; j < 3; j++) {
                    float c2d = cross2d(points[e0[j]].pos2d - q1, q2 - q1);
                    if (c2d < 1e-8f) {
                        chainInfo[j] |= 1;
                        index1Info[j] = addBlankBoundaryVertex(chain1, e0[j]);
                        if (e0[j] == f0) chain1Start = index1Info[j];
                    }
                    if (c2d > -1e-8f) {
                        chainInfo[j] |= 2;
                        index2Info[j] = addBlankBoundaryVertex(chain2, e0[j]);
                        if (e0[j] == f0) chain2Start = index2Info[j];
                    }
                }
#pragma unroll 3
                for (uint8_t j = 0; j < 3; j++) {
                    if (j == i) continue;
                    if (j > i) {
                        p1 = points[e0[j]].pos2d, p2 = points[e1[j]].pos2d;
                        if (intersectionTest(p1, p2, q1, q2, 1e-4f, -1e-4f)) continue;
                    }
                    uint8_t k = (j + 1) % 3;
                    if ((chainInfo[j] & 1) > 0 && (chainInfo[k] & 1) > 0) {
                        LinkedBoundaryVertex* vj = &chain1[index1Info[j]];
                        LinkedBoundaryVertex* vk = &chain1[index1Info[k]];
                        addVertexLink(vj, index1Info[k]);
                        addVertexLink(vk, index1Info[j]);
                    }
                    if ((chainInfo[j] & 2) > 0 && (chainInfo[k] & 2) > 0) {
                        LinkedBoundaryVertex* vj = &chain2[index2Info[j]];
                        LinkedBoundaryVertex* vk = &chain2[index2Info[k]];
                        addVertexLink(vj, index2Info[k]);
                        addVertexLink(vk, index2Info[j]);
                    }
                }
                badIndices.push_back(tridx);
                break;
            }
        }
    }
    if (badIndices.size() == 0) return;
    if (chain1Start == UINT32_MAX || chain2Start == UINT32_MAX) return;
    if (f1 != sortBoundaryChain(chain1, chain1Start)) return;
    if (f1 != sortBoundaryChain(chain2, chain2Start)) return;
    for (uint32_t badIdx : badIndices) {
        tris[badIdx] = tris[tris.size() - 1];
        tris.pop_back();
    }
    fillMonotone(points, tris, chain1, chain1Start, -1.f);
    fillMonotone(points, tris, chain2, chain2Start, 1.f);
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

struct HatOverstep {
    uint32_t contour;
    uint32_t lastOnCurve;
    glm::vec2 q1;
    glm::vec2 q2;
    uint32_t contour2;
    uint32_t lastOnCurve2;
    bool subdivideHalf = false;
};

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
// #define BYPASS_FILTERING

ProcessedGlyph* TextFont::getGlyphByIndex(VulkanContext* ctx, uint32_t glyphIndex) {
    auto insertion = glyphMap.insert(std::make_pair(glyphIndex, (ProcessedGlyph*)nullptr));
    if (!insertion.second) {
        return insertion.first->second;
    }

#ifdef GLYPH_LOAD_PROFILING
    lepton2::utils::lepton2_time_point start_time = lepton2::utils::startTiming();
    printf("Start building glyph at index %u\n", glyphIndex);
#endif

    ProcessedGlyph* glyph = new ProcessedGlyph();
    insertion.first->second = glyph;
    glyph->advanceWidth = parser->head.applyScale(parser->hmtx.getAdvanceWidth(glyphIndex));

    size_t glyfPtr = parser->loca.glyphOffsets[glyphIndex];

    if (glyfPtr == UINT32_MAX) {
        glyph->objData = nullptr;
        glyph->hostData = nullptr;
        return glyph;
    }

    TTFGlyph glyph_raw(parser->glyfData, &glyfPtr);

    if (glyph_raw.isCompound) {
        std::vector<SimpleVertex2d> vertices;
        std::vector<uint32_t> indices;
        for (TTFCompoundGlyphComponent comp : glyph_raw.compoundComponents) {
            HostObjectData* compData = this->getGlyphByIndex(ctx, comp.glyphIdx)->hostData;
            uint32_t nv = compData->vsize / sizeof(SimpleVertex2d);
            SimpleVertex2d* vertData = (SimpleVertex2d*)(compData->vertices);
            uint32_t vertOffset = vertices.size();
            vertices.resize(vertOffset + nv);
            glm::vec2 offs2d = glm::vec2(parser->head.applyScalef(comp.offset.x), parser->head.applyScalef(comp.offset.y));
            for (uint32_t i = 0; i < nv; i++) {
                glm::vec2 orig2d = glm::vec2(vertData[i].pos2d.x, 0.5f - vertData[i].pos2d.y);
                glm::vec2 v2d = comp.transform * orig2d + offs2d;
                vertices[i + vertOffset].pos2d = glm::vec2(v2d.x, 0.5f - v2d.y);
                vertices[i + vertOffset].texcoord = vertData[i].texcoord;
            }
            indices.reserve(indices.size() + compData->indices.size());
            for (uint32_t i = 0; i < compData->indices.size(); i++) {
                indices.push_back(compData->indices[i] + vertOffset);
            }
        }
        glyph->hostData = new HostObjectData(vertices.data(), vertices.size() * sizeof(SimpleVertex2d), indices);
        glyph->objData = new DeviceObjectData(ctx, glyph->hostData);
        return glyph;
    }

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

    std::vector<HatOverstep> hatOversteps;

    uint32_t cstart = 0;
    for (uint32_t cnt1 = 0; cnt1 < endIndices.size(); cnt1++) {
        for (uint32_t t1 = cstart; t1 < endIndices[cnt1] + 1; t1++) {
            uint32_t next = getNextPoint(endIndices, cnt1, t1);
            if (onCurve[next]) continue;
            uint32_t nextnext = getNextPoint(endIndices, cnt1, next);
            uint32_t cstart2 = 0;
            for (uint32_t cnt2 = 0; cnt2 < endIndices.size(); cnt2++) {
                uint32_t prevIdx = UINT32_MAX, firstIdx = UINT32_MAX;
                for (uint32_t t2 = cstart2; t2 != firstIdx; t2 = getNextPoint(endIndices, cnt2, t2)) {
                    if (!onCurve[t2]) continue;
                    if (prevIdx != UINT32_MAX) {
                        if (firstIdx == UINT32_MAX) firstIdx = t2;
                        glm::vec2 p1 = coords[t1].pos2d, p2 = coords[next].pos2d;
                        glm::vec2 q1 = coords[t2].pos2d, q2 = coords[prevIdx].pos2d;
                        if (intersectionTest(p1, p2, q1, q2, 1e-4f, 1e-4f)) {
                            hatOversteps.push_back({cnt1, t1, q1, q2, cnt2, prevIdx, false});
                        }
                        if (intersectionTest(p2, coords[nextnext].pos2d, q1, q2, 1e-4f, 1e-4f)) {
                            hatOversteps.push_back({cnt1, t1, q1, q2, cnt2, prevIdx, false});
                        }
                    }
                    prevIdx = t2;
                }
                cstart2 = endIndices[cnt2] + 1;
            }
        }
        cstart = endIndices[cnt1] + 1;
    }

    for (uint32_t i = hatOversteps.size() - 1; i < hatOversteps.size(); i--) {
        HatOverstep h = hatOversteps[i];
        uint32_t next = getNextPoint(endIndices, h.contour, h.lastOnCurve);
        uint32_t nextnext = getNextPoint(endIndices, h.contour, next);
        float t0 = 0.5f;
        bool trulyIntersected = false;
        if (!h.subdivideHalf) {
            float n1 = cross2d(h.q2 - h.q1 - 0.5f * (coords[nextnext].pos2d - coords[h.lastOnCurve].pos2d), h.q2 - h.q1);
            float d1 = cross2d(coords[next].pos2d - 0.5f * (coords[nextnext].pos2d + coords[h.lastOnCurve].pos2d), h.q2 - h.q1);
            float t0 = 0.5f * (1.f - n1 / d1);
            if (t0 < 1e-4f || t0 > (1.f - 1e-4f)) {
                trulyIntersected = true;
            }
        }
        glm::vec2 p2 = lerp(coords[h.lastOnCurve].pos2d, coords[next].pos2d, t0);
        glm::vec2 p4 = lerp(coords[next].pos2d, coords[nextnext].pos2d, t0);
        glm::vec2 p3 = lerp(p2, p4, t0);
        if (!h.subdivideHalf) {
            if (intersectionTest(coords[h.lastOnCurve].pos2d, p3, h.q1, h.q2, 0.f, 0.f)) trulyIntersected = true;
            if (intersectionTest(p3, coords[nextnext].pos2d, h.q1, h.q2, 0.f, 0.f)) trulyIntersected = true;
        }
        if (trulyIntersected) {
            HatOverstep newOverstep = {h.contour2, h.lastOnCurve2, glm::vec2(0), glm::vec2(0), UINT32_MAX, UINT32_MAX, true};
            for (uint32_t j = i; j < hatOversteps.size(); j--) {
                if (j == 0 || hatOversteps[j].contour <= h.contour2) {
                    hatOversteps.insert(hatOversteps.begin() + j, newOverstep);
                    break;
                }
            }
            i++;
            continue;
        }
        onCurve.insert(onCurve.begin() + h.lastOnCurve + 1, false);
        coords.insert(coords.begin() + h.lastOnCurve + 1, {p2, glm::vec2(0.5f)});
        if (h.lastOnCurve < next) next++;
        onCurve[next] = true;
        coords[next] = {p3, glm::vec2(0.5f)};
        onCurve.insert(onCurve.begin() + next + 1, false);
        coords.insert(coords.begin() + next + 1, {p4, glm::vec2(0.5f)});
        for (uint32_t j = h.contour; j < endIndices.size(); j++) {
            endIndices[j] += 2;
        }
        for (uint32_t j = i; j < hatOversteps.size(); j--) {
            if (hatOversteps[j].contour < h.contour) break;
            if (h.lastOnCurve < hatOversteps[j].lastOnCurve) hatOversteps[j].lastOnCurve++;
            if (next < hatOversteps[j].lastOnCurve) hatOversteps[j].lastOnCurve++;
        }
    }

#ifdef GLYPH_LOAD_PROFILING
    printf("Finished bezier subdivision at %lf seconds\n", lepton2::utils::getElapsedSeconds(start_time));
#endif

    std::vector<Triangle> tris;
    triangulate(coords, tris);

#ifdef GLYPH_LOAD_PROFILING
    printf("Built %u Delaunay triangles at %lf seconds\n", tris.size(), lepton2::utils::getElapsedSeconds(start_time));
#endif

cstart = 0;
    // Definitely the step which should be optimized using quadtree location techniques
    for (uint32_t cnt = 0; cnt < endIndices.size(); cnt++) {
        for (uint32_t t = cstart; t < endIndices[cnt] + 1; t++) {
            uint32_t next = getNextPoint(endIndices, cnt, t);
            if (!onCurve[next]) {
                uint32_t nextnext = getNextPoint(endIndices, cnt, next);
                glm::vec2 d1 = glm::normalize(coords[next].pos2d - coords[t].pos2d);
                glm::vec2 d2 = glm::normalize(coords[nextnext].pos2d - coords[t].pos2d);
                float alpha = cross2d(d1, d2);
                cutEdge(coords, tris, t, next);
                if (fabs(alpha) > 1e-3f) {  // Avoid cutting nearly-degenerate triangles
                    cutEdge(coords, tris, t, nextnext);
                }
            } else {
                cutEdge(coords, tris, t, next);
            }
        }
        cstart = endIndices[cnt] + 1;
    }

#ifndef BYPASS_FILTERING
    coords.resize(coords.size() - 3);  // Remove bounding triangle points
#endif

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
#ifndef BYPASS_FILTERING
            tris[i] = tris[tris.size() - 1];
            triangleFlags[i] = triangleFlags[tris.size() - 1];
            tris.pop_back();
            triangleFlags.pop_back();
#endif
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

#ifndef BYPASS_FILTERING
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
                    idx05 = findTriangleTexcoord(coords, tri, glm::vec2(0.f, 0.5f));
                    if (idx05 != 255) {
                        replaceVertexTexcoord(coords, tri, idx05, glm::vec2(0.5f));
                    }
                }
                continue;
            }
        }
    }
#endif

#ifdef GLYPH_LOAD_PROFILING
    printf("Completed %u conflicted vertex duplications at %lf seconds\n", (uint32_t)coords.size() - initialCoordsSize, lepton2::utils::getElapsedSeconds(start_time));
#endif

    std::vector<uint32_t> indices;
    indices.reserve(3 * tris.size());
    for (uint32_t i = 0; i < tris.size(); i++) {
        indices.resize(3 * (i + 1));
        float a = cross2d(coords[tris[i].t[1]].pos2d - coords[tris[i].t[0]].pos2d,
                          coords[tris[i].t[2]].pos2d - coords[tris[i].t[1]].pos2d);
        if (fabs(a) < 1e-6f) {
            tris[i] = tris[tris.size() - 1];
            tris.pop_back();
            i--;
        } else if (a < 0) {  // Allow culling (not strictly necessary with pipeline modifications)
            indices[3 * i + 0] = tris[i].t[0];
            indices[3 * i + 1] = tris[i].t[1];
            indices[3 * i + 2] = tris[i].t[2];
        } else {
            indices[3 * i + 0] = tris[i].t[0];
            indices[3 * i + 1] = tris[i].t[2];
            indices[3 * i + 2] = tris[i].t[1];
        }
    }

    glyph->hostData = new HostObjectData(coords.data(), coords.size() * sizeof(SimpleVertex2d), indices);

#ifdef GLYPH_LOAD_PROFILING
    printf("Built CPU-side glyph data in %lf seconds\n", lepton2::utils::getElapsedSeconds(start_time));
#endif

    glyph->objData = new DeviceObjectData(ctx, glyph->hostData);
    return glyph;
}

ProcessedGlyph* TextFont::getGlyph(VulkanContext* ctx, uint32_t codepoint) {
    uint32_t glyphIndex = parser->cmap.getGlyphIndex(codepoint);
    return this->getGlyphByIndex(ctx, glyphIndex);
}

void TextFont::destroy_back(VulkanContext* ctx) {
    delete this->parser;
    for (std::pair<uint32_t, ProcessedGlyph*> glyph : glyphMap) {
        glyph.second->destroy(ctx);
        delete glyph.second;
    }
}