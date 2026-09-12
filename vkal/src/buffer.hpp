#pragma once

#include "memory_allocator.hpp"

#include <memory>

namespace vkal {

struct BufferParams {
    Device& device;
    MemoryAllocator& memory_allocator;
    vk::DeviceSize size;
    vk::BufferUsageFlags usage;
    vk::MemoryPropertyFlags memory_properties;
};

class Buffer {
  public:
    // No copy and no move
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;
    Buffer(Buffer&&) = delete;
    Buffer& operator=(Buffer&&) = delete;

    explicit Buffer(const BufferParams& params);

    ~Buffer();

    void set_barrier(vk::AccessFlags2 access, vk::PipelineStageFlags2 stage);

    vk::Buffer get();

    const vk::MemoryRequirements get_memory_requirements() const;

    void upload(void* data, vk::DeviceSize size);

    void get_raw_data(void* data);

    vk::DeviceSize get_size();

    vk::AccessFlags2 get_access();

    vk::PipelineStageFlags2 get_stage();

  private:
    vk::Buffer create_buffer(const BufferParams& params);

    vk::MemoryRequirements get_requirements();

    Device& device;
    vk::DeviceSize size;
    vk::Buffer vk_buffer;
    vk::MemoryRequirements memory_requirements;
    MemoryAllocatorInfo allocator_info;
    void* data = nullptr;
    vk::AccessFlags2 access;
    vk::PipelineStageFlags2 stage;

    // Test
    FRIEND_TEST(MemoryAllocatorTest, AddBufferHostVisible);
};

using BufferPtr = std::unique_ptr<Buffer>;

inline BufferPtr buffer_ptr(const BufferParams& params) {
    return std::make_unique<Buffer>(params);
}

} // namespace vkal
