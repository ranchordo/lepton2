#pragma once

#include "Entity2d.h"

namespace lepton2::graphics2d {
class Polygon2d : public Entity2d {
   public:
    Polygon2d(vkc::VulkanContext* ctx, const char* shaderName, size_t ubo_size, const std::vector<glm::vec2>& points) : Entity2d(ctx, nullptr, {}, ubo_size) {
        this->points = points;
        entities.resize(points.size());
        for (uint32_t i = 0; i < points.size(); i++) {
            entities[i] = new Rectangle2d(ctx, shaderName, ubo_size, nullptr);
            entities[i]->region = {points[i], {0.005f, 0.005f}, 0.01f};
            this->addChild(entities[i]);
        }
    }

    void postInit(vkc::VulkanContext* ctx, vkc::RenderPass* renderState, vkc::RenderSubpass* node, gph::GraphicalConfigurationStore* store) override {
        for (uint32_t i = 0; i < entities.size(); i++) {
            entities[i]->initialize(ctx, renderState, node, store);
        }
    }

    void destroy2d(vkc::VulkanContext* ctx) override {
        for (Rectangle2d* e : entities) {
            e->destroy(ctx);
            delete e;
        }
    }

   private:
    std::vector<glm::vec2> points;
    std::vector<Rectangle2d*> entities;
};
}  // namespace lepton2::graphics2d