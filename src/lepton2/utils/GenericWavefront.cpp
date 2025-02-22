#include "GenericWavefront.h"

#include <assert.h>

#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

using namespace lepton2::utils;
using namespace lepton2::vulkancore;

static uint8_t prefix_compare(const char* a, const char* pfx) {
    for (uint32_t ptr = 0;; ptr++) {
        if (pfx[ptr] == '\0') return 0xff;
        if (a[ptr] == '\0') {
            if (pfx[ptr + 1] != '\0') return 0xff;
            if (pfx[ptr] < '0' || pfx[ptr] > '9') {
                throw std::runtime_error("GenericWavefront: prefix_compare: Improper prefix specification");
            }
            return (pfx[ptr] - '0');
        }
        if (a[ptr] != pfx[ptr]) return 0xff;
    }
}

static uint8_t get_pfx_size(const char* pfx) {
    char c = pfx[strlen(pfx) - 1];
    if (c < '0' || c > '9') {
        throw std::runtime_error("GenericWavefront: get_pfx_size: Improper prefix specification");
    }
    return (c - '0');
}

static uint8_t get_fmt_size(VkFormat format) {
    switch (format) {
        case VK_FORMAT_R32_SFLOAT:
            return 1;
        case VK_FORMAT_R32G32_SFLOAT:
            return 2;
        case VK_FORMAT_R32G32B32_SFLOAT:
            return 3;
        case VK_FORMAT_R32G32B32A32_SFLOAT:
            return 4;
        default:
            throw std::runtime_error("GenericWavefront: get_fmt_size: Invalid VkFormat");
    };
}

static bool check_vsd_compat(uint32_t* pfx, uint32_t vsize, VertexStructDescriptor& vsd) {
    if (vsize != vsd.members.size()) return false;
    for (uint32_t i = 0; i < vsize; i++) {
        if (pfx[i] != get_fmt_size(vsd.members[i].second)) return false;
    }
    return true;
}

struct VertexReference {
    uint32_t* ptr;
    uint32_t size;
    bool operator==(const VertexReference& other) const {
        if (this->size != other.size) return false;
        for (uint32_t i = 0; i < this->size; i++) {
            if (this->ptr[i] != other.ptr[i]) return false;
        }
        return true;
    }
};

namespace std {
template <>
struct hash<VertexReference> {
    size_t operator()(VertexReference const& vertex) const {
        size_t prime = 13931;
        size_t result = 0;
        for (uint32_t i = 0; i < vertex.size; i++) {
            result = prime * result + vertex.ptr[i];
        }
        return result;
    }
};
}  // namespace std

namespace lepton2::utils {

void parseGenericWavefront(VulkanContext* ctx, const char* loc, std::vector<const char*> pfx,
                           uint32_t* pfx_sizes, std::vector<double>* l1, std::vector<uint32_t>* fd) {
    char* buf = ctx->buildAssetLoadPath(loc);
    std::ifstream file;
    file.open(buf);
    std::string line;

    free(buf);

    uint32_t pfx_size;
    uint32_t pfx_idx = UINT32_MAX;

    if (!file.good()) {
        throw std::runtime_error("Failed to open generic wavefront file for reading");
    }

    for (uint32_t i = 0; i < pfx.size(); i++) {
        pfx_sizes[i] = get_pfx_size(pfx[i]);
    }

    while (std::getline(file, line)) {
        std::stringstream stream(line);
        std::string chunk;
        if (stream >> chunk) {
            // chunk holds prefix, need to fix pfx info if necessary
            const char* a = chunk.c_str();
            if (pfx_idx == UINT32_MAX || prefix_compare(a, pfx[pfx_idx]) == 0xff) {
                pfx_size = 0;
                // Find a new prefix
                for (uint32_t i = 0; i < pfx.size(); i++) {
                    if (i == pfx_idx) continue;
                    uint8_t pfx_compare = prefix_compare(a, pfx[i]);
                    if (pfx_compare != 0xff) {
                        pfx_idx = i;
                        pfx_size = pfx_compare;
                        break;
                    }
                }
                if (pfx_size == 0) continue;
            }
        } else {
            continue;
        }
        for (uint32_t i = 0; i < pfx_size; i++) {
            if (!(stream >> chunk)) {
                throw std::runtime_error("Parsing generic wavefront failed: prefix too short");
            }
            if (pfx_idx == pfx.size() - 1) {  // Face prefix
                std::stringstream vstream(chunk);
                std::string vidx;
                for (uint32_t i = 0; i < pfx.size() - 1; i++) {
                    if (!std::getline(vstream, vidx, '/')) {
                        throw std::runtime_error("Parsing generic wavefront failed: vidx array too short");
                    }
                    fd->push_back(std::stoi(vidx) - 1);
                }
                if (std::getline(vstream, vidx, '/')) {
                    throw std::runtime_error("Parsing generic wavefront failed: vidx array too long");
                }
            } else {
                l1[pfx_idx].push_back(std::stod(chunk));
            }
        }
        if (stream >> chunk) {
            throw std::runtime_error("Parsing generic wavefront failed: prefix too long");
        }
    }
    file.close();
}

HostObjectData* buildGenericWavefrontObj(VulkanContext* ctx, uint32_t* pfx, uint32_t vsize, std::vector<double>* l1,
                                         std::vector<uint32_t>& fd, VertexStructDescriptor& vsd) {
    if (pfx[vsize] != 3) {
        throw std::runtime_error("GenericWavefront: vertices per face *must* be 3 to obtain HostObjectData");
    }
    if (!check_vsd_compat(pfx, vsize, vsd)) {
        throw std::runtime_error("GenericWavefront: buildGenericWavefrontObj: pfx/vsd layouts incompatible");
    }
    std::unordered_map<VertexReference, uint32_t> vertmap;
    std::vector<uint32_t> indices;
    for (uint32_t i = 0; i < fd.size(); i += vsize) {
        VertexReference vref{};
        vref.ptr = fd.data() + i;
        vref.size = vsize;
        if (vertmap.count(vref) == 0) {
            vertmap[vref] = vertmap.size();
        }
        indices.push_back(vertmap[vref]);
    }

    char* vbuf = (char*)malloc(vertmap.size() * vsd.size);
    for (std::pair<VertexReference, uint32_t> entry : vertmap) {
        char* vertptr = vbuf + (vsd.size * entry.second);
        for (uint32_t p = 0; p < vsize; p++) {
            uint32_t l1idx = entry.first.ptr[p];
            float* target = (float*)(vertptr + vsd.members[p].first);
            double* source = l1[p].data() + (l1idx * pfx[p]);
            // Can't use memcpy bc of precision conversion
            for (uint32_t j = 0; j < pfx[p]; j++) {
                target[j] = (float)(source[j]);
            }
        }
    }

    // No need to free vbuf - lives on inside objdata
    HostObjectData* objdata = new HostObjectData(vbuf, vertmap.size() * vsd.size, indices, false);
    return objdata;
}

}  // namespace lepton2::utils