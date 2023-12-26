#include "VulkanMemory.h"

#include "VulkanContext.h"

// #define DEBUG_MEMORY_MANAGER

using namespace lepton2::vulkancore;

void MemoryChonkus::destroy(VulkanContext* ctx) {
    if (this->memory != VK_NULL_HANDLE) {
        vkFreeMemory(ctx->device, this->memory, nullptr);
        this->memory = VK_NULL_HANDLE;
    }
}

void MemoryChonklet::destroy(VulkanContext* ctx) {
    if (this->chonkus != nullptr) {
        ctx->allocManager.freeChonklet(*this);
        this->chonkus = nullptr;
    }
}

#ifdef DEBUG_MEMORY_MANAGER
void printFreeList(MemoryChonkus* chonkus) {
    MemoryChonkletEntry* entry = chonkus->entry;
    printf("[ ");
    MemoryChonkletEntry* stash = nullptr;
    while (entry != nullptr) {
        printf("{offs:%d,size:%d} ", (int)entry->offset, (int)entry->size);
        if (entry->next == nullptr) {
            stash = entry;
        }
        entry = entry->next;
    }
    printf("], --> [ ");
    while (stash != nullptr) {
        printf("{offs:%d,size:%d} ", (int)stash->offset, (int)stash->size);
        stash = stash->prev;
    }
    printf("]");
}
#endif

MemoryChonklet VulkanAllocationManager::findMemory(VkDeviceSize size, uint32_t memoryTypeIndex) {
#ifdef DEBUG_MEMORY_MANAGER
    printf("Requested allocation of size %d, type %d.\n", (int)size, (int)memoryTypeIndex);
#endif
    MemoryChonkletEntry* cEntry;
    MemoryChonkus* selectedChonkus;
    if (chonki.count(memoryTypeIndex) == 0 || chonki[memoryTypeIndex].size() == 0) {
        uint64_t GET_A_NEW_SIZE_IDIOT = 1024;
        MemoryChonkus newChonkus = this->buildChonkus(GET_A_NEW_SIZE_IDIOT, memoryTypeIndex);
#ifdef DEBUG_MEMORY_MANAGER
        printf("Building new chonkus vector with size %d\n", (int)GET_A_NEW_SIZE_IDIOT);
#endif
        std::vector<MemoryChonkus> newChonki = { newChonkus };
        std::vector<MemoryChonkus>& lookup = chonki[memoryTypeIndex];
        lookup = newChonki;
        selectedChonkus = lookup.data();
        cEntry = this->findAvailableEntry(selectedChonkus, size);
    } else {
        std::vector<MemoryChonkus>& allChonki = chonki[memoryTypeIndex];
        for (int32_t i = allChonki.size() - 1; i >= 0; i--) {
            selectedChonkus = allChonki.data() + i;
            cEntry = this->findAvailableEntry(selectedChonkus, size);
            if (cEntry != nullptr) {
                break;
            }
        }
        if (cEntry == nullptr) {
            uint64_t GET_A_NEW_SIZE_IDIOT = 1024;
#ifdef DEBUG_MEMORY_MANAGER
            printf("Building new chonkus on existing vector with size %d\n", (int)GET_A_NEW_SIZE_IDIOT);
#endif
            MemoryChonkus newChonkus = this->buildChonkus(GET_A_NEW_SIZE_IDIOT, memoryTypeIndex);
            std::vector<MemoryChonkus>& lookup = chonki[memoryTypeIndex];
            lookup.push_back(newChonkus);
            selectedChonkus = &lookup.back();
            cEntry = this->findAvailableEntry(selectedChonkus, size);
        }
    }
    if (cEntry == nullptr || cEntry->size < size) {
        throw std::runtime_error("Failed to allocate any chonklet entries whatsoever.");
    }
    VkDeviceSize remainder = cEntry->size - size;
    if (remainder > 0) {
        VkDeviceSize offset = cEntry->offset + size;
        MemoryChonkletEntry* nEntry = new MemoryChonkletEntry(offset, remainder, cEntry->next, cEntry);
#ifdef DEBUG_MEMORY_MANAGER
        printf("Inserting offs:%d, size:%d\n", (int)nEntry->offset, (int)nEntry->size);
#endif
        cEntry->next = nEntry;
        cEntry->size = size;
    }
#ifdef DEBUG_MEMORY_MANAGER
    printf("After insertion: ");
    printFreeList(selectedChonkus);
    printf("\n");
#endif
    MemoryChonklet ret;
    ret.chonkus = selectedChonkus;
    ret.offset = cEntry->offset;
    ret.size = cEntry->size;
    this->deleteEntry(selectedChonkus, cEntry);
#ifdef DEBUG_MEMORY_MANAGER
    printf("After deletion: ");
    printFreeList(selectedChonkus);
    printf("\n");
    printf("Allocated for memorytype %d a block with offset %d, size %d\n", (int)memoryTypeIndex, (int)ret.offset, (int)ret.size);
#endif
    return ret;
}

MemoryChonkus VulkanAllocationManager::buildChonkus(VkDeviceSize size, uint32_t memoryTypeIndex) {
    MemoryChonkus chonkus;
    chonkus.allocationSize = size;
    chonkus.entry = new MemoryChonkletEntry(0, size, nullptr, nullptr);
    chonkus.memory_type = memoryTypeIndex;
    chonkus.memory = VK_NULL_HANDLE;

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = size;
    allocInfo.memoryTypeIndex = memoryTypeIndex;
    if (vkAllocateMemory(this->ctx->device, &allocInfo, nullptr, &chonkus.memory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate chonkus memory.");
    }
    return chonkus;
}

void VulkanAllocationManager::deleteEntry(MemoryChonkus* chonkus, MemoryChonkletEntry* entry) {
    if (entry->prev == nullptr) {
#ifdef DEBUG_MEMORY_MANAGER
        printf("First-element case deletion!\n");
#endif
        chonkus->entry = entry->next;
        entry->next->prev = nullptr;
    } else {
#ifdef DEBUG_MEMORY_MANAGER
        printf("Not first-element case deletion!\n");
#endif
        entry->prev->next = entry->next;
        entry->next->prev = entry->prev;
    }
    delete entry;
}

MemoryChonkletEntry* VulkanAllocationManager::findAvailableEntry(MemoryChonkus* chonkus, VkDeviceSize size) {
#ifdef DEBUG_MEMORY_MANAGER
    printf("Finding an entry.\nFreeList: ");
    printFreeList(chonkus);
    printf("\n");
#endif
    if (chonkus->entry == nullptr) return nullptr;
    MemoryChonkletEntry* current = chonkus->entry;
    while (true) {
        if (current->size >= size) {
#ifdef DEBUG_MEMORY_MANAGER
            printf("Got entry offs:%d, size:%d\n", (int)current->offset, (int)current->size);
#endif
            return current;
        }
        if (current->next == nullptr) {
            return nullptr;
        }
        current = current->next;
    }
    return nullptr;
}

void VulkanAllocationManager::freeChonklet(MemoryChonklet chonklet) {
#ifdef DEBUG_MEMORY_MANAGER
    printf("Trying to free offs:%d, size:%d\n", (int)chonklet.offset, (int)chonklet.size);
    printf("Initial state: ");
    printFreeList(chonklet.chonkus);
    printf("\n");
#endif
    MemoryChonkletEntry* nEntry = new MemoryChonkletEntry(chonklet.offset, chonklet.size, chonklet.chonkus->entry, nullptr);
#ifdef DEBUG_MEMORY_MANAGER
    printf("Right first: searching for a block with offset %d\n", (int)(nEntry->offset + nEntry->size));
#endif
    MemoryChonkletEntry* current = chonklet.chonkus->entry;
    // Check right first coalescence with left secondary chain
    while (current != nullptr) {
        if (current->offset == (nEntry->offset + nEntry->size)) {
            // Trigger coalescence
            current->offset = nEntry->offset;
            current->size += nEntry->size;
            delete nEntry;
#ifdef DEBUG_MEMORY_MANAGER
            printf("Triggered. After right first coalescence: ");
            printFreeList(chonklet.chonkus);
            printf("\n");
#endif
            // Check left secondary coalescence
#ifdef DEBUG_MEMORY_MANAGER
            printf("Left secondary: searching for a block with offset+size = %d\n", (int)current->offset);
#endif
            MemoryChonkletEntry* current2 = chonklet.chonkus->entry;
            while (current2 != nullptr) {
                if ((current2->size + current2->offset) == current->offset) {
                    current->offset = current2->offset;
                    current->size += current2->size;
                    this->deleteEntry(chonklet.chonkus, current2);
#ifdef DEBUG_MEMORY_MANAGER
                    printf("Triggered. After left secondary coalescence: ");
                    printFreeList(chonklet.chonkus);
                    printf("\n");
#endif
                    this->checkChonkusDeletion(chonklet.chonkus);
                    return;
                }
                current2 = current2->next;
            }
#ifdef DEBUG_MEMORY_MANAGER
            printf("No secondary coalescence.\n");
#endif
            this->checkChonkusDeletion(chonklet.chonkus);
            return;
        }
        current = current->next;
    }
    // Check left first coalescence
    current = chonklet.chonkus->entry;
#ifdef DEBUG_MEMORY_MANAGER
    printf("Left first: searching for a block with offset+size = %d\n", (int)nEntry->offset);
#endif
    while (current != nullptr) {
        if ((current->offset + current->size) == nEntry->offset) {
            // Left first
            current->size += nEntry->size;
            delete nEntry;
#ifdef DEBUG_MEMORY_MANAGER
            printf("Triggered. After left first coalescence: ");
            printFreeList(chonklet.chonkus);
            printf("\n");
#endif
            this->checkChonkusDeletion(chonklet.chonkus);
            return;
        }
        current = current->next;
    }
    chonklet.chonkus->entry = nEntry;
    nEntry->next->prev = nEntry;
#ifdef DEBUG_MEMORY_MANAGER
    printf("After insertion: ");
    printFreeList(chonklet.chonkus);
    printf("\n");
#endif
    this->checkChonkusDeletion(chonklet.chonkus);
}

void VulkanAllocationManager::checkChonkusDeletion(MemoryChonkus* chonkus) {
#ifdef DEBUG_MEMORY_MANAGER
    printf("Checking for deletion condition for chonkus with type %d\n", (int)chonkus->memory_type);
#endif
    if (chonkus->entry == nullptr) {
#ifdef DEBUG_MEMORY_MANAGER
        printf("No free blocks. Nope.\n");
#endif
        return;
    }
    if (chonkus->entry->next != nullptr) {
#ifdef DEBUG_MEMORY_MANAGER
        printf("Too many free blocks. Nope.\n");
#endif
        return;
    }
    if (chonkus->entry->size != chonkus->allocationSize) {
#ifdef DEBUG_MEMORY_MANAGER
        printf("Free block too small. Nope.\n");
#endif
        return;
    }
#ifdef DEBUG_MEMORY_MANAGER
    printf("Conditions met. Deleting this chonkus...\n");
#endif
    std::vector<MemoryChonkus>& allChonki = this->chonki[chonkus->memory_type];
    for (uint32_t i = 0; i < allChonki.size(); i++) {
        if (allChonki[i].memory == chonkus->memory) {
#ifdef DEBUG_MEMORY_MANAGER
            printf("Deleting chonkus with index %d, old length is %d, ", (int)i, (int)allChonki.size());
#endif
            allChonki.erase(allChonki.begin() + i);
#ifdef DEBUG_MEMORY_MANAGER
            printf("new length is %d\n", (int)allChonki.size());
#endif
            chonkus->destroy(this->ctx);
            return;
        }
    }
    throw std::runtime_error("Failed to find chonkus to delete!!!");
}

void VulkanAllocationManager::destroy(VulkanContext* ctx) {
    for (auto& x : this->chonki) {
        std::vector<MemoryChonkus>& vec = x.second;
        for (MemoryChonkus& c : vec) {
#ifdef DEBUG_MEMORY_MANAGER
            printf("Destroying chonkus %p\n", c.memory);
#endif
            c.destroy(ctx);
        }
        vec.clear();
    }
    this->chonki.clear();
}