#pragma once

#include "memory_allocator.hpp"

#include <memory>

namespace vkal {

struct BufferParams {
    Device& vkal_device;
    MemoryAllocator& memory_allocator;
    vk::DeviceSize size;
    vk::BufferUsageFlags usage;
    vk::MemoryPropertyFlags memory_properties;
};

class Buffer {
  public:
    // No copy and mo move
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;
    Buffer(Buffer&&) = delete;
    Buffer& operator=(Buffer&&) = delete;

    explicit Buffer(const BufferParams& params);

    ~Buffer();

    vk::Buffer get();

    const vk::MemoryRequirements get_memory_requirements() const;

    void upload(void* data, vk::DeviceSize size);

    void get_raw_data(void* data);

    vk::DeviceSize get_size();

  private:
    vk::Buffer create_buffer(const BufferParams& params);

    vk::MemoryRequirements get_requirements();

    std::pair<MemoryBlock&, MemoryChunk&> get_memory_block(const BufferParams& params);

    Device& vkal_device;
    MemoryAllocator& memory_allocator;
    vk::DeviceSize size;
    vk::Buffer buffer;
    vk::MemoryRequirements memory_requirements;
    std::pair<MemoryBlock&, MemoryChunk&> memory_block;
    void* data;

    // Test
    FRIEND_TEST(MemoryAllocatorTest, AddBufferHostVisible);
};

using BufferPtr = std::unique_ptr<Buffer>;

inline BufferPtr buffer_ptr(const BufferParams& params) {
    return std::make_unique<Buffer>(params);
}

} // namespace vkal
