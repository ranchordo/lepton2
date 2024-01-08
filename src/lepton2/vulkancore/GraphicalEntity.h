#pragma once

#include "VulkanUtils.h"
#include "Descriptors.h"
#include "ObjectData.h"

namespace lepton2::vulkancore {
class GraphicalConfigurationStore {
    #error
};

class GraphicalEntity: public DeletableVulkanResource {
    const char* pipelineIdentifier;
    DescriptorSetArray* dsa;
    DeviceObjectData* objectData;
    void destroy_back(VulkanContext* ctx) override;
};
}  // namespace lepton2::vulkancore