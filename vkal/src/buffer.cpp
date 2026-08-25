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
const vk::MemoryRequirements Buffer::get_memory_requirements() const {
    return this->memory_requirements;
}

///////////////////////////////////////////////////////////
void Buffer::copy(vk::CommandBuffer command, Buffer& other) {
    vk::BufferCopy2 buffer_copy;
    buffer_copy.setSrcOffset(0).setDstOffset(0).setSize(this->size);
    vk::CopyBufferInfo2 copy_buffer_info;
    copy_buffer_info.setSrcBuffer(other.buffer).setDstBuffer(this->buffer).setRegions(buffer_copy);
    command.copyBuffer2(copy_buffer_info);
}

///////////////////////////////////////////////////////////
void Buffer::copy_and_submit(vk::Queue queue, vk::CommandBuffer command, Buffer& other) {
    command.begin(vk::CommandBufferBeginInfo());
    this->copy(command, other);
    command.end();

    vk::CommandBufferSubmitInfo command_submit_info;
    command_submit_info.setCommandBuffer(command).setDeviceMask(1);
    vk::SubmitInfo2 submit_info;
    submit_info.setCommandBufferInfos(command_submit_info);

    queue.submit2(submit_info);
    queue.waitIdle();
}

///////////////////////////////////////////////////////////
void Buffer::upload(void* data, vk::DeviceSize size) {
    if (size > memory_requirements.size) {
        throw std::runtime_error("Upload size exceeds buffer size");
    }
    std::memcpy(this->data, data, size);
}

///////////////////////////////////////////////////////////
void Buffer::get_raw_data(void* data) {
    std::memcpy(data, this->data, this->size);
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
