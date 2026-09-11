#include "buffer.hpp"

namespace vkal {

///////////////////////////////////////////////////////////
Buffer::Buffer(const BufferParams& params)
    : vkal_device(params.vkal_device), size(params.size), buffer(this->create_buffer(params)),
      memory_requirements(this->get_requirements()),
      allocator_info(params.memory_allocator.bind_buffer(this->buffer, params.memory_properties,
                                                         this->memory_requirements)) {
    this->allocator_info.block.map_data(this->allocator_info.chunk, &this->data);
}

///////////////////////////////////////////////////////////
Buffer::~Buffer() {
    this->vkal_device.get().destroyBuffer(this->buffer);

    // Sync with memory allocator data
    this->allocator_info.block.remove_chunk(this->allocator_info.chunk);
    this->allocator_info.allocator.clean(this->allocator_info.block);
#if defined(ENABLE_DEBUG)
    this->allocator_info.allocator.dump();
#endif
}

///////////////////////////////////////////////////////////
vk::Buffer Buffer::get() {
    return this->buffer;
}

///////////////////////////////////////////////////////////
void Buffer::set_barrier(vk::AccessFlags2 access, vk::PipelineStageFlags2 stage) {
    this->access = access;
    this->stage = stage;
}

///////////////////////////////////////////////////////////
const vk::MemoryRequirements Buffer::get_memory_requirements() const {
    return this->memory_requirements;
}

///////////////////////////////////////////////////////////
void Buffer::upload(void* data, vk::DeviceSize size) {
    if (this->data == nullptr) {
        throw std::runtime_error("Uploading to a non host visible buffer");
    }
    if (size > memory_requirements.size) {
        throw std::runtime_error("Upload size exceeds buffer size");
    }
    std::memcpy(this->data, data, size);
}

///////////////////////////////////////////////////////////
void Buffer::get_raw_data(void* data) {
    if (this->data == nullptr) {
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
vk::AccessFlags2 Buffer::get_access() {
    return this->access;
}

///////////////////////////////////////////////////////////
vk::PipelineStageFlags2 Buffer::get_stage() {
    return this->stage;
}

} // namespace vkal
