#include "memory.hpp"
#include "common.hpp"

namespace vkal {

///////////////////////////////////////////////////////////
static uint32_t find_memory_type(vk::PhysicalDevice physical_device, uint32_t filter,
                                 vk::MemoryPropertyFlags properties) {
    vk::PhysicalDeviceMemoryProperties physical_device_memory_properties =
        physical_device.getMemoryProperties();

    for (uint32_t i = 0; i < physical_device_memory_properties.memoryTypeCount; ++i) {
        if ((filter & (1 << i)) && (physical_device_memory_properties.memoryTypes[i].propertyFlags &
                                    properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("Failed to match memory type filter");
}

///////////////////////////////////////////////////////////
MemoryBlock::MemoryBlock(const MemoryBlockParams& params)
    : device(params.device), size(params.block_size), properties(params.properties),
      memory_type(find_memory_type(this->device.get_physical(),
                                   params.memory_requirements.memoryTypeBits, this->properties)) {

    vk::MemoryAllocateInfo memory_allocate_info;
    memory_allocate_info.setAllocationSize(this->size).setMemoryTypeIndex(this->memory_type);
    this->vk_memory = this->device.get().allocateMemory(memory_allocate_info);

    // Only map data when the host visible bits are active
    if ((properties & vk::MemoryPropertyFlagBits::eHostVisible) ==
        vk::MemoryPropertyFlagBits::eHostVisible) {
#if defined(VULKAN_VERSION_1_4)
        this->data = this->device.get().mapMemory2(
            vk::MemoryMapInfo(vk::MemoryMapFlags(), this->vk_memory, 0, this->size));
#else
        this->data = this->device.get().mapMemory(this->vk_memory, 0, this->size);
#endif
    }

    // Add initial memory chunk that covers everything
    this->first_chunk = std::make_unique<MemoryChunk>(MemoryChunk{
        .offset = 0,
        .size = this->size,
    });
}

///////////////////////////////////////////////////////////
MemoryBlock::~MemoryBlock() {
    if ((properties & vk::MemoryPropertyFlagBits::eHostVisible) ==
        vk::MemoryPropertyFlagBits::eHostVisible) {

#if defined(VULKAN_VERSION_1_4)
        this->device.get().unmapMemory2(vk::MemoryUnmapInfo(vk::MemoryUnmapFlags(), this->vk_memory));
#else
        this->device.get().unmapMemory(this->vk_memory);
#endif
    }

    this->device.get().freeMemory(this->vk_memory);
}

///////////////////////////////////////////////////////////
std::vector<std::reference_wrapper<MemoryChunk>> MemoryBlock::get_all_chunks() const {
    std::vector<std::reference_wrapper<MemoryChunk>> chunks;
    for (auto* chunk = this->first_chunk.get(); chunk; chunk = chunk->next.get()) {
        chunks.push_back(*chunk);
    }

    return chunks;
}

///////////////////////////////////////////////////////////
void MemoryBlock::map_data(MemoryChunk& chunk, void** data) {
    if ((this->properties & vk::MemoryPropertyFlagBits::eHostVisible) !=
        vk::MemoryPropertyFlagBits::eHostVisible) {
        return;
    }
    *data = (unsigned char*)this->data + chunk.offset;
}

///////////////////////////////////////////////////////////
void MemoryBlock::flush() {
    bool is_visible = (this->properties & vk::MemoryPropertyFlagBits::eHostVisible) ==
                      vk::MemoryPropertyFlagBits::eHostVisible;
    bool is_coherent = (this->properties & vk::MemoryPropertyFlagBits::eHostCoherent) ==
                       vk::MemoryPropertyFlagBits::eHostCoherent;
    if (is_visible && !is_coherent) {
        this->device.get().flushMappedMemoryRanges(
            vk::MappedMemoryRange(this->vk_memory, 0, this->size));
    }
}

///////////////////////////////////////////////////////////
std::string MemoryBlock::get_dump_info(vk::DeviceSize unit) {
    std::string info = "|";
    for (auto* chunk = this->first_chunk.get(); chunk; chunk = chunk->next.get()) {
        size_t count = chunk->size / unit;
        std::string indicator;
        indicator = std::string(count == 0 ? 0 : count - 1, chunk->occupied ? '+' : '-');
        indicator.push_back('|');
        info += indicator;
    }

    return info;
}

///////////////////////////////////////////////////////////
std::string MemoryBlock::get_dump_info() {
    std::string info = "|";
    for (auto* chunk = this->first_chunk.get(); chunk; chunk = chunk->next.get()) {
        if (chunk->occupied) {
            info += std::format("\033[32m+++ {} +++\033[0m|", size_as_string(chunk->size));
        } else {
            info += std::format("\033[31m--- {} ---\033[0m|", size_as_string(chunk->size));
        }
    }
    return info;
}

///////////////////////////////////////////////////////////
void MemoryBlock::remove_chunk(MemoryChunk& chunk) {
    MemoryChunk* left_chunk = chunk.previous;
    MemoryChunk* right_chunk = chunk.next.get();

    bool left = left_chunk != nullptr && !left_chunk->occupied;
    bool right = right_chunk != nullptr && !right_chunk->occupied;

    if (left && !right) { // If right chunk isn't available while left is free expand left and
                          // consume middle
        left_chunk->size += chunk.size;
        this->remove(chunk);
    } else if (!left && right) { // If left chunk isn't available while right is free flip middle
                                 // occupied flag and expand middle to consume right
        chunk.occupied = false;
        chunk.size += right_chunk->size;
        this->remove(*right_chunk);
    } else if (!left && !right) { // Only flip the flag if 2 chunks are not available
        chunk.occupied = false;
    } else { // If 2 chunks are free expand left to consume both middle and right
        left_chunk->size += chunk.size + right_chunk->size;
        this->remove(chunk);
        this->remove(*right_chunk);
    }
}

///////////////////////////////////////////////////////////
std::optional<std::reference_wrapper<MemoryChunk>>
MemoryBlock::get_chunk(vk::DeviceSize size, vk::DeviceSize alignment) {
    for (auto* chunk = this->first_chunk.get(); chunk; chunk = chunk->next.get()) {
        if (chunk->occupied) {
            continue;
        }
        auto chunk_opt = this->carve(*chunk, size, alignment);
        if (chunk_opt.has_value()) {
            return chunk_opt;
        }
    }
    return std::nullopt;
}

///////////////////////////////////////////////////////////
MemoryChunk& MemoryBlock::insert(MemoryChunk& chunk, bool occupied, vk::DeviceSize offset,
                                 vk::DeviceSize size) {
    std::unique_ptr<MemoryChunk> new_chunk = std::make_unique<MemoryChunk>(MemoryChunk{
        .occupied = occupied,
        .offset = offset,
        .size = size,
        .next = std::move(chunk.next),
        .previous = &chunk,
    });
    if (new_chunk->next) {
        new_chunk->next->previous = new_chunk.get();
    }
    chunk.next = std::move(new_chunk);
    this->chunk_count++;
    return *chunk.next;
}

///////////////////////////////////////////////////////////
void MemoryBlock::remove(MemoryChunk& chunk) {
    if (chunk.previous) {
        if (chunk.next) {
            chunk.next->previous = chunk.previous;
            chunk.previous->next = std::move(chunk.next);
        } else {
            chunk.previous->next.reset();
        }
        this->chunk_count--;
    } else {
        if (chunk.next) { // No previous means the current chunk is the first chunk
            this->first_chunk = std::move(this->first_chunk->next);
            this->first_chunk->previous = nullptr;
            this->chunk_count--;
        }
    }
}

///////////////////////////////////////////////////////////
std::optional<std::reference_wrapper<MemoryChunk>>
MemoryBlock::carve(MemoryChunk& chunk, vk::DeviceSize size, vk::DeviceSize alignment) {
    vk::DeviceSize l0 = chunk.offset;
    vk::DeviceSize l1 = (chunk.offset + alignment - 1) & ~(alignment - 1);
    vk::DeviceSize r0 = l1 + size;
    vk::DeviceSize r1 = chunk.offset + chunk.size;

    // Carved size does not fit into chunk
    if (r0 > r1) {
        return std::nullopt;
    }
    vk::DeviceSize s0 = l1 - l0;
    vk::DeviceSize s1 = r1 - r0;

    /*
     * l0       l1             r0       r1
     * |---s0---|   occupied   |---s1---|
     */

    if (s0 == 0) {
        chunk.occupied = true;
        // Carved chunk not perfectly fit only left part is occupied
        if (s1 > 0) {
            chunk.size = r0 - l0;
            this->insert(chunk, false, r0, s1);
        }
        return chunk;
    } else {
        chunk.size = s0;
        // Only right part is occupied
        if (s1 == 0) {
            return this->insert(chunk, true, l1, r1 - l1);
        } else { // Carved chunk lies in the middle leaving 2 empty parts on both side
            auto& carve_chunk = this->insert(chunk, true, l1, r0 - l1);
            this->insert(carve_chunk, false, r0, s1);
            return carve_chunk;
        }
    }
}

} // namespace vkal
