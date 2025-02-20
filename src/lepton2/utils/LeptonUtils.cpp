#include "LeptonUtils.h"

#include "../external/tiny_obj_loader.h"
#include "../graphics/GraphicalPresets.h"
#include "../vulkancore/VulkanContext.h"

using namespace lepton2::vulkancore;
using namespace lepton2::graphics::graphicalpresets;

namespace lepton2::utils {

lepton2_time_point startTiming() {
    return std::chrono::steady_clock::now();
}

double getElapsedSeconds(lepton2_time_point time_point) {
    lepton2_time_point now = startTiming();
    std::chrono::duration<double> interval = now - time_point;
    return interval.count();
}

std::filesystem::path getExecutableLocation(const char* argv0, bool force_absolute) {
    std::filesystem::path relative_path = std::filesystem::path(argv0).parent_path();
    if (!force_absolute) {
        return relative_path;
    }
    return std::filesystem::absolute(relative_path);
}

std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        printf("Attempted to open file %s\n", filename.c_str());
        throw std::runtime_error("Could not open miscellaneous file for reading.");
    }
    size_t fsize = (size_t)file.tellg();
    std::vector<char> buffer(fsize);
    file.seekg(0);
    file.read(buffer.data(), fsize);
    file.close();
    return buffer;
}

HostObjectData* loadObjFile(VulkanContext* ctx, const char* filename) {
    char* buf = ctx->buildAssetLoadPath(filename);
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string err;
    std::vector<uint32_t> indices;
    std::vector<ObjLoadVertex> vertices;
    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &err, buf)) {
        throw std::runtime_error(err);
    }
    free(buf);

    std::unordered_map<ObjLoadVertex, uint32_t> unique{};

    for (const tinyobj::shape_t& shape : shapes) {
        for (const tinyobj::index_t& index : shape.mesh.indices) {
            ObjLoadVertex vertex{};
            vertex.pos = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };
            vertex.texcoord = {
                attrib.texcoords[2 * index.texcoord_index + 0],
                1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
            };
            vertex.normal = {
                attrib.normals[3 * index.normal_index + 0],
                attrib.normals[3 * index.normal_index + 1],
                attrib.normals[3 * index.normal_index + 2]
            };
            if (unique.count(vertex) == 0) {
                unique[vertex] = (uint32_t)(vertices.size());
                vertices.push_back(vertex);
            }
            indices.push_back(unique[vertex]);
        }
    }

    HostObjectData* ret = new HostObjectData(vertices.data(), vertices.size() * sizeof(ObjLoadVertex), indices);
    return ret;
}

}  // namespace lepton2::utils