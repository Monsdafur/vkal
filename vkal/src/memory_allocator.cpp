#include "memory_allocator.hpp"
#include "common.hpp"

#include <print>

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

    throw std::runtime_error("Failed to match memory type filter or unsupported memory properties");
}

///////////////////////////////////////////////////////////
MemoryAllocator::MemoryAllocator(const MemoryAllocatorParams& params)
    : vkal_device(params.vkal_device), block_size(params.block_size) {
}

///////////////////////////////////////////////////////////
std::pair<MemoryBlock&, MemoryChunk&>
MemoryAllocator::bind_buffer(vk::Buffer buffer, vk::MemoryPropertyFlags memory_properties,
                             const vk::MemoryRequirements& memory_requirements) {
    auto memory_block = this->query_block(memory_properties, memory_requirements);

    // Bind buffer
    vk::BindBufferMemoryInfo bind_buffer_info;
    bind_buffer_info.setBuffer(buffer)
        .setMemory(memory_block.first.memory)
        .setMemoryOffset(memory_block.second.offset);
    this->vkal_device.get().bindBufferMemory2(bind_buffer_info);

    return memory_block;
}

std::pair<MemoryBlock&, MemoryChunk&>
MemoryAllocator::bind_image(vk::Image image, vk::MemoryPropertyFlags memory_properties,
                            const vk::MemoryRequirements& memory_requirements) {
    auto memory_block = this->query_block(memory_properties, memory_requirements);
    // Bind buffer
    vk::BindImageMemoryInfo bind_image_info;
    bind_image_info.setImage(image)
        .setMemory(memory_block.first.memory)
        .setMemoryOffset(memory_block.second.offset);
    this->vkal_device.get().bindImageMemory2(bind_image_info);

    return memory_block;
}

///////////////////////////////////////////////////////////
void MemoryAllocator::flush() {
    for (auto& block : this->blocks) {
        block->flush();
    }
}

///////////////////////////////////////////////////////////
void MemoryAllocator::dump(vk::DeviceSize unit) {
    size_t index = 0;
    for (auto& block : this->blocks) {
        std::print("Block: {}", index++);
        block->dump(unit);
    }
}

///////////////////////////////////////////////////////////
void MemoryAllocator::dump() {
    size_t index = 0;
    for (auto& block : this->blocks) {
        std::print("Block: {}", index++);
        block->dump();
    }
}

///////////////////////////////////////////////////////////
void MemoryAllocator::clean(MemoryBlock& block) {
    if (block.chunk_count == 1 && !block.first_chunk->occupied) {
        for (size_t i = 0; i < this->blocks.size(); ++i) {
            if (this->blocks.at(i).get() == &block) {
                this->blocks.erase(this->blocks.begin() + i);
                return;
            }
        }
        throw std::runtime_error("This memory block is not or no longer belong to this allocator");
    }
}

///////////////////////////////////////////////////////////
std::pair<MemoryBlock&, MemoryChunk&>
MemoryAllocator::query_block(vk::MemoryPropertyFlags memory_properties,
                             const vk::MemoryRequirements& memory_requirements) {
    if (memory_requirements.size > this->block_size) {
        throw std::runtime_error(std::format(
            "Request memory size ({}) exceed memory block size ({}) limit",
            size_as_string(memory_requirements.size), size_as_string(this->block_size)));
    }

    uint32_t memory_type = find_memory_type(this->vkal_device.get_physical(),
                                            memory_requirements.memoryTypeBits, memory_properties);

    std::optional<std::reference_wrapper<MemoryBlock>> query_block;
    std::optional<std::reference_wrapper<MemoryChunk>> query_chunk;

    // Find for any appropriate memory block and memory chunk
    for (auto& block : this->blocks) {
        // A block must have compatible memory type with requested buffer
        bool compatible_memory_type =
            ((1 << block->memory_type) & memory_requirements.memoryTypeBits) != 0;
        // A memory block must have contain requested memory properties
        bool compatible_memory_properties =
            (memory_properties & block->properties) == block->properties;

        if (compatible_memory_type && compatible_memory_properties) {
            auto chunk_opt =
                block->get_chunk(memory_requirements.size, memory_requirements.alignment);
            if (chunk_opt.has_value()) {
                query_block = *block;
                query_chunk = chunk_opt.value();
            }
        }
    }

    // If no suitable memory block is found create a new block
    if (!query_block.has_value()) {
        this->blocks.push_back(memory_block_ptr(MemoryBlockParams{
            .vkal_device = this->vkal_device,
            .block_size = this->block_size,
            .properties = memory_properties,
            .memory_requirements = memory_requirements,
        }));

        auto chunk_opt =
            this->blocks.back()->get_chunk(memory_requirements.size, memory_requirements.alignment);
        if (!chunk_opt.has_value()) {
            throw std::runtime_error(
                "Failed to bind buffer probably due to ambiguous memory properties");
        }
        query_chunk = chunk_opt.value();
        query_block = *this->blocks.back();
    }

    return {query_block->get(), query_chunk->get()};
}

} // namespace vkal
