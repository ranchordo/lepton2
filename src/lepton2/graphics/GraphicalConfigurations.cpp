#include "GraphicalConfigurations.h"

#include "../vulkancore/RenderPass.h"
#include "GraphicalEntity.h"

using namespace lepton2::graphics;
using namespace lepton2::vulkancore;

void GraphicalConfiguration::renderAllUsers(VkCommandBuffer commandBuffer, uint32_t frameIndex, uint32_t setidx) {
    this->pipeline->bind(commandBuffer);
    for (GraphicalEntity* entity : this->users) {
        entity->render(commandBuffer, frameIndex, setidx);
    }
}

void GraphicalConfiguration::preRenderAllUsers(VulkanContext* ctx, uint32_t frameIndex) {
    for (GraphicalEntity* entity : this->users) {
        entity->doPreRender(ctx, frameIndex);
    }
}

void GraphicalConfiguration::destroy_back(VulkanContext* ctx) {
    this->pipeline->destroy(ctx);
    delete this->pipeline;
    this->layoutReference->destroy(ctx);
    delete this->layoutReference;
}

GraphicalConfiguration* SubpassGraphicalConfigurationStore::createNewConfiguration(VulkanContext* ctx, RenderPass* renderState, GraphicsPipelineConstraints constraints) {
    GraphicalConfiguration* newConfiguration = new GraphicalConfiguration();
    newConfiguration->layoutReference = new DescriptorSetArray(constraints.layoutInfo);
    newConfiguration->layoutReference->buildDescriptorSetLayout(ctx);

    std::vector<VkDescriptorSetLayout> dsl(renderState->getSuperpassLayouts());
    // Add pass DSL
    if (renderState->getPassDsa() != nullptr) {
        dsl.push_back(renderState->getPassDsa()->descriptorSetLayout);
    }
    // Add subpass DSLs
    if (parent->getSubpassDsa() != nullptr) {
        dsl.push_back(parent->getSubpassDsa()->descriptorSetLayout);
    }
    if (parent->getSubpassAttachmentDsa() != nullptr) {
        dsl.push_back(parent->getSubpassAttachmentDsa()->descriptorSetLayout);
    }
    // Add pipeline DSL
    if (newConfiguration->layoutReference->layoutInfo.bindings.size() > 0) {
        dsl.push_back(newConfiguration->layoutReference->descriptorSetLayout);
    }
    GraphicsPipelineInfo newPipelineInfo(dsl, constraints);
    newConfiguration->pipeline = new GraphicsPipeline(ctx, this->parent, renderState, newPipelineInfo);
    return newConfiguration;
}

GraphicalConfigurationHandle SubpassGraphicalConfigurationStore::getConfiguration(VulkanContext* ctx, GraphicsPipelineConstraints constraints, GraphicalEntity* user) {
    std::unordered_set<GraphicalConfiguration*>* selectedVector = nullptr;
    if (this->cache.count(constraints.shaderName) == 0) {
        std::unordered_set<GraphicalConfiguration*> newTempVector;
        this->cache[constraints.shaderName] = newTempVector;
        selectedVector = &this->cache[constraints.shaderName];
    } else {
        std::unordered_set<GraphicalConfiguration*>& candidates = this->cache[constraints.shaderName];
        for (GraphicalConfiguration* candidate : candidates) {
            if (!constraints.compatible(candidate->pipeline->creationConstraints)) continue;
            GraphicalConfigurationHandle ret;
            ret.user = user;
            ret.config = candidate;
            ret.store = this;
            ret.config->users.push_back(user);
            return ret;
        }
        selectedVector = &candidates;
    }
    GraphicalConfiguration* newConfiguration = this->createNewConfiguration(ctx, renderPass, constraints);
    selectedVector->insert(newConfiguration);
    this->allConfigs.push_back(newConfiguration);
    GraphicalConfigurationHandle ret;
    ret.user = user;
    ret.config = newConfiguration;
    ret.store = this;
    ret.config->users.push_back(user);
    return ret;
}

static void debugPrintDescriptorSetArray(DescriptorSetArray* dsa, uint32_t* setidx, const char* phase) {
    uint32_t numdesc = dsa->layoutInfo.descInfo.size();
    uint32_t rmult = dsa->singleDescriptorSets.size();
    printf("    %s @(set = %u) with %u descriptor(s), multiplicity %u:\n", phase, *setidx, numdesc, rmult);
    for (uint32_t j = 0; j < numdesc; j++) {
        printf("        Descriptor @(binding = %u), type ", j);
        DescriptorInfo& descInfo = dsa->layoutInfo.descInfo[j];
#define ADD_TYPE_CASE(type) \
    case type:              \
        printf(#type "\n"); \
        break;
        switch (descInfo.descriptorType) {
            ADD_TYPE_CASE(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
            ADD_TYPE_CASE(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
            ADD_TYPE_CASE(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
            ADD_TYPE_CASE(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT);
            default:
                throw std::runtime_error("Unsupported descriptor type.");
        }
#undef ADD_TYPE_CASE
    }
    (*setidx)++;
}

void GraphicalConfigurationHandle::debugPrintAllBoundDescriptors() {
    printf("GraphicalConfigurationHandle::debugPrintAllBoundDescriptors:\n");
    printf("    Shader name: %s\n", config->pipeline->creationConstraints.shaderName);
    uint32_t start_set = store->renderPass->getSuperpassLayouts().size();
    printf("    Not shown: %u superpass descriptor sets\n", start_set);

    uint32_t setidx = 0;
    // Add pass DSA
    if (store->renderPass->getPassDsa() != nullptr) {
        debugPrintDescriptorSetArray(store->renderPass->getPassDsa(), &setidx, "renderPass->passDsa");
    }
    // Add subpass DSAs
    if (store->parent->getSubpassDsa() != nullptr) {
        debugPrintDescriptorSetArray(store->parent->getSubpassDsa(), &setidx, "renderSubpass->subpassDsa");
    }
    if (store->parent->getSubpassAttachmentDsa() != nullptr) {
        debugPrintDescriptorSetArray(store->parent->getSubpassAttachmentDsa(), &setidx, "renderSubpass->subpassAttachmentDsa");
    }
    // Add entity DSA
    if (config->layoutReference->layoutInfo.bindings.size() > 0) {
        debugPrintDescriptorSetArray(user->dsa, &setidx, "graphicalEntity->dsa");
    }
    printf("End of descriptor sets\n");
}

void SubpassGCSRenderCallback::renderSubpassCmd(VkCommandBuffer commandBuffer, RenderPass* pass, uint32_t frameIndex, uint32_t setidx) {
    this->store->renderAllConfigurations(commandBuffer, frameIndex, setidx);
}

void SubpassGCSRenderCallback::preRenderSubpass(VulkanContext* ctx, uint32_t frameIndex) {
    this->store->preRenderAllConfigurations(ctx, frameIndex);
}

void SubpassGraphicalConfigurationStore::dropConfigurationHandle(VulkanContext* ctx, GraphicalConfigurationHandle handle) {
    CHECK_DESTRUCTION();
    if (this->cache.count(handle.config->pipeline->creationConstraints.shaderName) == 0) {
        return;
    }

    auto elem = std::find(handle.config->users.begin(), handle.config->users.end(), handle.user);
    if (elem != handle.config->users.end()) {
        std::iter_swap(elem, handle.config->users.end() - 1);
        handle.config->users.pop_back();
    }
    if (handle.config->users.size() == 0) {
        std::unordered_set<GraphicalConfiguration*>& configurations = cache[handle.config->pipeline->creationConstraints.shaderName];
        configurations.erase(handle.config);
        auto elem2 = std::find(allConfigs.begin(), allConfigs.end(), handle.config);
        if (elem2 != allConfigs.end()) {
            std::iter_swap(elem2, allConfigs.end() - 1);
            allConfigs.pop_back();
        }
        handle.config->destroy(ctx);
        delete handle.config;
    }
}

void SubpassGraphicalConfigurationStore::renderAllConfigurations(VkCommandBuffer commandBuffer, uint32_t frameIndex, uint32_t setidx) {
    for (GraphicalConfiguration* config : allConfigs) {
        config->renderAllUsers(commandBuffer, frameIndex, setidx);
    }
}

void SubpassGraphicalConfigurationStore::preRenderAllConfigurations(VulkanContext* ctx, uint32_t frameIndex) {
    for (GraphicalConfiguration* config : allConfigs) {
        config->preRenderAllUsers(ctx, frameIndex);
    }
}

void SubpassGraphicalConfigurationStore::destroy_back(VulkanContext* ctx) {
    for (std::pair<std::string, std::unordered_set<GraphicalConfiguration*>> const& pair : this->cache) {
        for (GraphicalConfiguration* config : pair.second) {
            config->destroy(ctx);
            delete config;
        }
    }
    this->cache.clear();
}

void GraphicalConfigurationStore::addAllSubpasses(RenderPass* pass) {
    if (this->subpassStores.count(pass) > 0) return;
    std::vector<SubpassGraphicalConfigurationStore*> stores(pass->numSubpasses());
    for (uint32_t i = 0; i < pass->numSubpasses(); i++) {
        SubpassGraphicalConfigurationStore* store = new SubpassGraphicalConfigurationStore(pass->getNode(i), pass);
        this->addLinkedResource(store, true);
        stores[i] = store;
        pass->getNode(i)->setRenderCallback(store->getRenderCallback());
    }
    subpassStores[pass] = stores;
}