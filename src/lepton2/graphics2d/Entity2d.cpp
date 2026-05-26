#include "Entity2d.h"

#include "../graphics/GraphicalPresets.h"
#include "../vulkancore/Textures.h"

using namespace lepton2::graphics;
using namespace lepton2::graphics2d;
using namespace lepton2::vulkancore;
using namespace lepton2::graphics::graphicalpresets;

static std::vector<SimplePresetVertex> rectangleVertices = {
    {{-1.0f, -1.0f, 0.0f}, {0.0f, 0.0f}},
    {{+1.0f, -1.0f, 0.0f}, {1.0f, 0.0f}},
    {{+1.0f, +1.0f, 0.0f}, {1.0f, 1.0f}},
    {{-1.0f, +1.0f, 0.0f}, {0.0f, 1.0f}}};

static std::vector<uint32_t> rectangleIndices = {1, 0, 3, 1, 3, 2};

uint32_t Entity2d::rectUsers = 0;
DeviceObjectData* Entity2d::rectData = nullptr;

Entity2d::Entity2d(vkc::VulkanContext* ctx, const char* shaderName, std::optional<VertexStructDescriptor> vsd, size_t ubo_size) {
    this->shaderName = shaderName;
    if (vsd.has_value()) this->vsd = vsd.value();
    this->ubo_size = ubo_size;
    this->ubo = malloc(ubo_size);
    this->free_ubo = true;
}

GraphicsPipelineConstraints Entity2d::getPipelineRequirements() {
    DescriptorSetLayoutInfo dsli;
    {
        DescriptorInfo descInfo;
        descInfo.uniformBufferData.bufferSize = sizeof(Region2d);
        descInfo.uniformBufferData.createNewBuffer = true;
        descInfo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descInfo.shaderStages = VK_SHADER_STAGE_ALL_GRAPHICS;
        dsli.addNewBinding(descInfo);
    }
    if (ubo_size > 0) {
        DescriptorInfo descInfo;
        descInfo.uniformBufferData.bufferSize = ubo_size;
        descInfo.uniformBufferData.createNewBuffer = true;
        descInfo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descInfo.shaderStages = VK_SHADER_STAGE_ALL_GRAPHICS;
        dsli.addNewBinding(descInfo);
    }
    if (this->texture != nullptr) {
        for (uint32_t i = 0; i < this->texture->components.size(); i++) {
            DescriptorInfo descInfo;
            descInfo.imageSamplerData.container = this->texture;
            descInfo.imageSamplerData.componentIndex = i;
            descInfo.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descInfo.shaderStages = VK_SHADER_STAGE_FRAGMENT_BIT;
            dsli.addNewBinding(descInfo);
        }
    }
    GraphicsPipelineConstraints ret(this->shaderName, dsli, this->vsd);
    return ret;
}

void Entity2d::preRender(VulkanContext* ctx, SingleDescriptorSet* sds, uint32_t frameIndex) {
    if (sds != nullptr && (sync_buffers >> frameIndex) & 1) {
        VulkanBuffer* uniformBuffer = ((descriptortypes::UniformBufferDescriptor*)(sds->instances[0]))->uniformBuffer;
        void* data = uniformBuffer->chonklet.mapMemory(ctx, 0);
        memcpy(data, &onScreenRegion, sizeof(Region2d));
        uniformBuffer->chonklet.unmapMemory(ctx);

        if (this->ubo_size > 0) {
            uniformBuffer = ((descriptortypes::UniformBufferDescriptor*)(sds->instances[1]))->uniformBuffer;
            data = uniformBuffer->chonklet.mapMemory(ctx, 0);
            memcpy(data, ubo, ubo_size);
            uniformBuffer->chonklet.unmapMemory(ctx);
        }
        this->sync_buffers &= ~(1 << frameIndex);
    }
}

// Weird hack to achieve dynamic context-dependent singletons
class Entity2dRectDataMonitor : public DeletableVulkanResource {
   public:
    Entity2dRectDataMonitor(VulkanContext* ctx, uint32_t* users, DeviceObjectData** data) {
        if (*data == nullptr) {
            HostObjectData hostData(rectangleVertices.data(), rectangleVertices.size() * sizeof(SimplePresetVertex), rectangleIndices);
            (*data) = new DeviceObjectData(ctx, &hostData);
        }
        this->users = users;
        this->data = data;
        (*users)++;
    }

    void destroy_back(VulkanContext* ctx) override {
        (*users)--;
        if (*users == 0 && *data != nullptr) {
            (*data)->destroy(ctx);
            delete (*data);
            (*data) = nullptr;
        }
    }

   private:
    uint32_t* users;
    DeviceObjectData** data;
};

void Entity2d::useRectangleGeometry(VulkanContext* ctx) {
    this->addLinkedResource(new Entity2dRectDataMonitor(ctx, &Entity2d::rectUsers, &Entity2d::rectData), true);
    this->setObjectData(Entity2d::rectData);
    this->vsd = simplePresetVsd;
}

static void adjustHorizontalAlign(Region2d* bbox, Region2d* parent, HorizontalAlignmentMode2d horiz) {
    switch (horiz) {
        case ALIGN_HORIZ_CENTER_2D:
            break;
        case ALIGN_HORIZ_LEFT_2D:
            bbox->xyPos.x -= parent->halfExtent.x - bbox->halfExtent.x;
            break;
        case ALIGN_HORIZ_RIGHT_2D:
            bbox->xyPos.x += parent->halfExtent.x - bbox->halfExtent.x;
            break;
        default:
            throw std::runtime_error("Unhandled HorizontalAlignmentMode2d.");
    }
}

static void adjustVerticalAlign(Region2d* bbox, Region2d* parent, VerticalAlignmentMode2d vert) {
    switch (vert) {
        case ALIGN_VERT_CENTER_2D:
            break;
        case ALIGN_VERT_TOP_2D:
            bbox->xyPos.y -= parent->halfExtent.y - bbox->halfExtent.y;
            break;
        case ALIGN_VERT_BOTTOM_2D:
            bbox->xyPos.y += parent->halfExtent.y - bbox->halfExtent.y;
            break;
        default:
            throw std::runtime_error("Unhandled VerticalAlignmentMode2d.");
    }
}

Region2d Entity2d::getOnScreenRegion(Region2d region, Region2d parentOnScreenRegion, AlignmentMode2d align, float passAspect) {
    Region2d bbox = parentOnScreenRegion;
    switch (align.aspectControl) {
        case ASPECT_CONTROL_NONE_2D:
            break;
        case ASPECT_CONTROL_FIT_WIDTH_2D:
            bbox.halfExtent.y = bbox.halfExtent.x * passAspect;
            adjustVerticalAlign(&bbox, &parentOnScreenRegion, align.vert);
            break;
        case ASPECT_CONTROL_FIT_HEIGHT_2D:
            bbox.halfExtent.x = bbox.halfExtent.y / passAspect;
            adjustHorizontalAlign(&bbox, &parentOnScreenRegion, align.horiz);
            break;
        case ASPECT_CONTROL_FIT_MAX_DIM_2D:
            if (bbox.halfExtent.x * passAspect > bbox.halfExtent.y) {
                bbox.halfExtent.y = bbox.halfExtent.x * passAspect;
                adjustVerticalAlign(&bbox, &parentOnScreenRegion, align.vert);
            } else {
                bbox.halfExtent.x = bbox.halfExtent.y / passAspect;
                adjustHorizontalAlign(&bbox, &parentOnScreenRegion, align.horiz);
            }
            break;
        case ASPECT_CONTROL_FIT_MIN_DIM_2D:
            if (bbox.halfExtent.x * passAspect > bbox.halfExtent.y) {
                bbox.halfExtent.x = bbox.halfExtent.y / passAspect;
                adjustHorizontalAlign(&bbox, &parentOnScreenRegion, align.horiz);
            } else {
                bbox.halfExtent.y = bbox.halfExtent.x * passAspect;
                adjustVerticalAlign(&bbox, &parentOnScreenRegion, align.vert);
            }
            break;
        default:
            throw std::runtime_error("Unhandled AspectControlMode2d.");
    }
    return {region.xyPos * bbox.halfExtent + bbox.xyPos, region.halfExtent * bbox.halfExtent, region.zPos + bbox.zPos};
}