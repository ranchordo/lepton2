#pragma once

#include <stdint.h>

#include <vector>

#include "../vulkancore/ObjectData.h"
#include "../vulkancore/VulkanContext.h"

namespace lepton2::utils {

namespace vkc = lepton2::vulkancore;

extern void parseGenericWavefront(vkc::VulkanContext* ctx, const char* loc, std::vector<const char*> pfx,
                                  uint32_t* pfx_sizes, std::vector<double>* l1, std::vector<uint32_t>* fd);
extern vkc::HostObjectData* buildGenericWavefrontObj(vkc::VulkanContext* ctx, uint32_t* pfx, uint32_t vsize, std::vector<double>* l1,
                                                     std::vector<uint32_t>& fd, vkc::VertexStructDescriptor& vsd);

template <uint32_t num_l1_prefix>
class GenericWavefrontData {
   public:
    std::vector<double> l1_data[num_l1_prefix];
    std::vector<uint32_t> face_data;
    uint32_t pfx_sizes[num_l1_prefix + 1];
    GenericWavefrontData(vkc::VulkanContext* ctx, const char* loc, std::vector<const char*> pfx) {
        if (pfx.size() - 1 != num_l1_prefix) {
            throw std::runtime_error("Incorrect number of l1 prefixes used for parsing generic wavefront");
        }
        parseGenericWavefront(ctx, loc, pfx, this->pfx_sizes, this->l1_data, &this->face_data);
    }

    vkc::HostObjectData* getObjData(vkc::VulkanContext* ctx, vkc::VertexStructDescriptor& vsd) {
        return buildGenericWavefrontObj(ctx, pfx_sizes, num_l1_prefix, l1_data, face_data, vsd);
    }
};

}  // namespace lepton2::utils