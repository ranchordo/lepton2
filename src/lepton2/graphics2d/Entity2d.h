#pragma once

#include "../graphics/GraphicalEntity.h"
#include "../vulkancore/VulkanLoop.h"

namespace lepton2::graphics2d {

namespace gph = lepton2::graphics;
namespace vkc = lepton2::vulkancore;

struct SimpleVertex2d {
    glm::vec2 pos2d;
    glm::vec2 texcoord;
};

//! Vertex structure with a 2d position and 2d texture coordinate.
extern vkc::VertexStructDescriptor simpleVsd2d;

struct Region2d {
    glm::vec2 xyPos = glm::vec2(0.0f, 0.0f);
    glm::vec2 halfExtent = glm::vec2(0.5f, 0.5f);
    float zPos = 0.01f;
};

enum AspectControlMode2d {
    ASPECT_CONTROL_NONE_2D = 0,
    ASPECT_CONTROL_FIT_WIDTH_2D,
    ASPECT_CONTROL_FIT_HEIGHT_2D,
    ASPECT_CONTROL_FIT_MAX_DIM_2D,
    ASPECT_CONTROL_FIT_MIN_DIM_2D
};

enum HorizontalAlignmentMode2d {
    ALIGN_HORIZ_CENTER_2D = 0,
    ALIGN_HORIZ_LEFT_2D,
    ALIGN_HORIZ_RIGHT_2D
};

enum VerticalAlignmentMode2d {
    ALIGN_VERT_CENTER_2D = 0,
    ALIGN_VERT_TOP_2D,
    ALIGN_VERT_BOTTOM_2D
};

struct AlignmentMode2d {
    AspectControlMode2d aspectControl = ASPECT_CONTROL_NONE_2D;
    HorizontalAlignmentMode2d horiz = ALIGN_HORIZ_CENTER_2D;
    VerticalAlignmentMode2d vert = ALIGN_VERT_CENTER_2D;
};

class Entity2d : public gph::GraphicalEntity {
   public:
    Entity2d(vkc::VulkanContext* ctx, const char* shaderName, std::optional<vkc::VertexStructDescriptor> vsd, size_t ubo_size);
    vkc::GraphicsPipelineConstraints getPipelineRequirements() override;
    void preRender(vkc::VulkanContext* ctx, vkc::SingleDescriptorSet* sds, uint32_t frameIndex) override;
    void logic2d(vkc::VulkanContext* ctx) {
        this->logic(ctx);
        for (Entity2d* child : children) {
            child->logic2d(ctx);
        }
    }

    void destroy_back(vkc::VulkanContext* ctx) override {
        this->destroy2d(ctx);
        this->destroyEntityResources(ctx);
        if (this->free_ubo) {
            free(this->ubo);
        }
    }

    void requestBufferSync() { this->sync_buffers = 0xffff; }

    void updateRegionRecursive(float passAspect) {
        this->passAspect = passAspect;
        this->onScreenRegion = Entity2d::getOnScreenRegion(region, parentRegion, alignmentMode, passAspect);
        this->requestBufferSync();
        for (Entity2d* entity : children) {
            entity->parentRegion = this->onScreenRegion;
            entity->updateRegionRecursive(passAspect);
        }
    }

    void addChild(Entity2d* child) {
        if (child == this) return;
        for (Entity2d* entity : this->children) {
            if (entity == child) return;
        }
        this->children.push_back(child);
    }

    void removeChild(Entity2d* child) {
        auto elem = std::find(children.begin(), children.end(), child);
        if (elem != children.end()) {
            std::iter_swap(elem, children.end() - 1);
            children.pop_back();
        }
    }

    void removeAllChildren() { children.clear(); }

    void setAspectControlMode(AspectControlMode2d acm2d) { this->alignmentMode.aspectControl = acm2d; }
    void setHorizontalAlignment(HorizontalAlignmentMode2d ham2d) { this->alignmentMode.horiz = ham2d; }
    void setVerticalAlignment(VerticalAlignmentMode2d vam2d) { this->alignmentMode.vert = vam2d; }

    Region2d region;

   protected:
    Region2d parentRegion = {{0.0f, 0.0f}, {1.0f, 1.0f}, 0.0f};
    void setUbo(void* _ubo, size_t _ubo_size, bool _free_ubo) {
        free(this->ubo);
        this->ubo = _ubo;
        this->ubo_size = _ubo_size;
        this->free_ubo = _free_ubo;
    }

    void copyUbo(void* src) {
        memcpy(this->ubo, src, this->ubo_size);
    }

    void useRectangleGeometry(vkc::VulkanContext* ctx);
    void setTexture(vkc::Texture* texture) { this->texture = texture; }
    void setVsd(vkc::VertexStructDescriptor& vsd) { this->vsd = vsd; }
    virtual void modifyPipelineRequirements(vkc::GraphicsPipelineConstraints* req) {}
    virtual void logic(vkc::VulkanContext* ctx) {};
    virtual void destroy2d(vkc::VulkanContext* ctx) {};

   private:
    const char* shaderName;
    float passAspect;
    vkc::Texture* texture = nullptr;
    vkc::VertexStructDescriptor vsd;
    void* ubo = nullptr;
    bool free_ubo;
    uint16_t sync_buffers = 0;
    size_t ubo_size = 0;
    Region2d onScreenRegion;
    std::vector<Entity2d*> children;
    AlignmentMode2d alignmentMode;

    static Region2d getOnScreenRegion(Region2d region, Region2d parentOnScreenRegion, AlignmentMode2d align, float passAspect);

    static uint32_t rectUsers;
    static vkc::DeviceObjectData* rectData;
};

class Rectangle2d : public Entity2d {
   public:
    Rectangle2d(vkc::VulkanContext* ctx, const char* shaderName, size_t ubo_size, vkc::Texture* texture) : Entity2d(ctx, shaderName, {}, ubo_size) {
        this->useRectangleGeometry(ctx);
        this->setTexture(texture);
    }
};

class ConformalFrame2d : public Entity2d {
   public:
    ConformalFrame2d(vkc::VulkanContext* ctx, VkExtent2D* extentPtr) : Entity2d(ctx, nullptr, {}, 0) {
        this->extentPtr = extentPtr;
        this->lastKnownExtent = {0, 0};
        this->region = {glm::vec2(0.0f, 0.0f), glm::vec2(1.0f, 1.0f), 0.0f};
        this->setAspectControlMode(ASPECT_CONTROL_NONE_2D);
    }

    void logic(vkc::VulkanContext* ctx) override {
        VkExtent2D currentExtent = *extentPtr;
        if (currentExtent.width != lastKnownExtent.width || currentExtent.height != lastKnownExtent.height) {
            this->updateRegionRecursive(((float)currentExtent.width) / ((float)currentExtent.height));
            lastKnownExtent = currentExtent;
        }
    }

   private:
    VkExtent2D* extentPtr;
    VkExtent2D lastKnownExtent;
};
}  // namespace lepton2::graphics2d

namespace lepton2::vulkancore::loopmodifiers {

namespace gph2d = lepton2::graphics2d;

//! Call `logic2d` on a 2d graphical entity every frame, usually only needed for a ConformalFrame2d.
class Logic2DLoopModifier : public VulkanLoopModifier {
   public:
    Logic2DLoopModifier(gph2d::Entity2d* entity) {
        this->entity = entity;
    }
    void preRender(VulkanContext* ctx, uint32_t frameIndex) override {
        entity->logic2d(ctx);
    }

   private:
    gph2d::Entity2d* entity;
};
}  // namespace lepton2::vulkancore::loopmodifiers