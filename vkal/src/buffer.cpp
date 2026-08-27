#include "buffer.hpp"

namespace vkal {

///////////////////////////////////////////////////////////
Buffer::Buffer(const BufferParams& params)
    : vkal_device(params.vkal_device), memory_allocator(params.memory_allocator), size(params.size),
      buffer(this->create_buffer(params)), memory_requirements(this->get_requirements()),
      memory_block(this->get_memory_block(params)) {

    this->memory_block.first.map_data(this->memory_block.second, &this->data);
}

///////////////////////////////////////////////////////////
Buffer::~Buffer() {
    this->vkal_device.get().destroyBuffer(this->buffer);

    // Sync with memory allocator data
    this->memory_block.first.remove_chunk(this->memory_block.second);
    this->memory_allocator.clean(this->memory_block.first);
}

///////////////////////////////////////////////////////////
vk::Buffer Buffer::get() {
    return this->buffer;
}

///////////////////////////////////////////////////////////
const vk::MemoryRequirements Buffer::get_memory_requirements() const {
    return this->memory_requirements;
}

///////////////////////////////////////////////////////////
void Buffer::upload(void* data, vk::DeviceSize size) {
    if (!this->data) {
        throw std::runtime_error("Uploading to a non host visible buffer");
    }
    if (size > memory_requirements.size) {
        throw std::runtime_error("Upload size exceeds buffer size");
    }
    std::memcpy(this->data, data, size);
}

///////////////////////////////////////////////////////////
void Buffer::get_raw_data(void* data) {
    if (!this->data) {
        throw std::runtime_error("Reading from a non host visible buffer");
    }
    std::memcpy(data, this->data, this->size);
}

///////////////////////////////////////////////////////////
vk::DeviceSize Buffer::get_size() {
    return this->size;
}

///////////////////////////////////////////////////////////
vk::Buffer Buffer::create_buffer(const BufferParams& params) {
    vk::BufferCreateInfo buffer_create_info;
    buffer_create_info.setSize(params.size)
        .setUsage(params.usage)
        .setSharingMode(vk::SharingMode::eExclusive);

    return this->vkal_device.get().createBuffer(buffer_create_info);
}

///////////////////////////////////////////////////////////
vk::MemoryRequirements Buffer::get_requirements() {
    vk::BufferMemoryRequirementsInfo2 memory_requirements_info;
    memory_requirements_info.setBuffer(this->buffer);
    return this->vkal_device.get()
        .getBufferMemoryRequirements2(memory_requirements_info)
        .memoryRequirements;
}

///////////////////////////////////////////////////////////
std::pair<MemoryBlock&, MemoryChunk&> Buffer::get_memory_block(const BufferParams& params) {
    return this->memory_allocator.bind_buffer(this->buffer, params.memory_properties,
                                              this->memory_requirements);
}

} // namespace vkal
