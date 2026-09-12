#pragma once

#include "device.hpp"
#include "memory.hpp"

#include <gtest/gtest_prod.h>

#include <memory>

namespace vkal {

struct MemoryAllocatorParams {
    Device& device;
    vk::DeviceSize block_size;
};

struct MemoryAllocatorInfo;

class MemoryAllocator {
  public:
    // No copy and no move
    MemoryAllocator(const MemoryAllocator&) = delete;
    MemoryAllocator& operator=(const MemoryAllocator&) = delete;
    MemoryAllocator(MemoryAllocator&&) = delete;
    MemoryAllocator& operator=(MemoryAllocator&&) = delete;

    explicit MemoryAllocator(const MemoryAllocatorParams& params);

    ~MemoryAllocator() = default;

    MemoryAllocatorInfo bind_buffer(vk::Buffer buffer, vk::MemoryPropertyFlags memory_properties,
                                    const vk::MemoryRequirements& memory_requirements);

    MemoryAllocatorInfo bind_image(vk::Image image, vk::MemoryPropertyFlags memory_properties,
                                   const vk::MemoryRequirements& memory_requirements);

    void clean(MemoryBlock& block);

    void flush();

    void dump(vk::DeviceSize unit);

    void dump();

  private:
    MemoryAllocatorInfo query_block(vk::MemoryPropertyFlags memory_properties,
                                    const vk::MemoryRequirements& memory_requirements);

    Device& device;
    vk::DeviceSize block_size;
    std::vector<MemoryBlockPtr> blocks;

    // Tests
    FRIEND_TEST(MemoryAllocatorTest, AddBuffer);
    FRIEND_TEST(MemoryAllocatorTest, BufferFail);
    FRIEND_TEST(MemoryAllocatorTest, AddBufferNewBlockOverflow);
    FRIEND_TEST(MemoryAllocatorTest, AddBufferHostVisible);
    FRIEND_TEST(MemoryAllocatorTest, AddBufferNewBlockIncompatibleType);
    FRIEND_TEST(MemoryAllocatorTest, RemoveBufferFreeBlock);

    FRIEND_TEST(MemoryAllocatorTest, AddImage);
    FRIEND_TEST(MemoryAllocatorTest, ImageFail);
};

struct MemoryAllocatorInfo {
    MemoryAllocator& allocator;
    MemoryBlock& block;
    MemoryChunk& chunk;
};

using MemoryAllocatorPtr = std::unique_ptr<MemoryAllocator>;

inline MemoryAllocatorPtr memory_allocator_ptr(const MemoryAllocatorParams& params) {
    return std::make_unique<MemoryAllocator>(params);
}

} // namespace vkal
