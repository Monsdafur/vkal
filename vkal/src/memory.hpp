#pragma once

#include "device.hpp"

#include <gtest/gtest_prod.h>

#include <memory>

namespace vkal {

struct MemoryChunk {
    bool occupied;
    vk::DeviceSize offset;
    vk::DeviceSize size;
    std::unique_ptr<MemoryChunk> next;
    MemoryChunk* previous;
};

struct MemoryBlockParams {
    Device& vkal_device;
    vk::DeviceSize block_size;
    vk::MemoryPropertyFlags properties;
    vk::MemoryRequirements memory_requirements;
};

class MemoryBlock {
  public:
    // No copy and mo move
    MemoryBlock(const MemoryBlock&) = delete;
    MemoryBlock& operator=(const MemoryBlock&) = delete;
    MemoryBlock(MemoryBlock&&) = delete;
    MemoryBlock& operator=(MemoryBlock&&) = delete;

    MemoryBlock(const MemoryBlockParams& params);

    ~MemoryBlock();

    std::vector<std::reference_wrapper<MemoryChunk>> get_all_chunks() const;

    void map_data(MemoryChunk& chunk, void** data);

    void flush();

    void remove_chunk(MemoryChunk& chunk);

    void dump(vk::DeviceSize unit);

  private:
    friend class MemoryAllocator;

    std::optional<std::reference_wrapper<MemoryChunk>> get_chunk(vk::DeviceSize size,
                                                                 vk::DeviceSize alignment);

    MemoryChunk& insert(MemoryChunk& chunk, bool occupied, vk::DeviceSize offset,
                        vk::DeviceSize size);

    void remove(MemoryChunk& chunk);

    std::optional<std::reference_wrapper<MemoryChunk>>
    carve(MemoryChunk& chunk, vk::DeviceSize size, vk::DeviceSize alignment);

    Device& vkal_device;
    vk::MemoryPropertyFlags properties;
    uint32_t memory_type;
    vk::DeviceMemory memory;
    void* data;
    vk::MappedMemoryRange memory_range;

    size_t chunk_count = 1;
    std::unique_ptr<MemoryChunk> first_chunk;

    // Tests
    FRIEND_TEST(MemoryBlockTest, CarvePerfectFit);
    FRIEND_TEST(MemoryBlockTest, CarveLeftFit);
    FRIEND_TEST(MemoryBlockTest, CarveRightFit);
    FRIEND_TEST(MemoryBlockTest, CarveMiddleFit);
    FRIEND_TEST(MemoryBlockTest, RemoveNoMerge);
    FRIEND_TEST(MemoryBlockTest, RemoveMergeRight);
    FRIEND_TEST(MemoryBlockTest, RemoveMergeLeft);
    FRIEND_TEST(MemoryBlockTest, RemoveMergeBoth);

    FRIEND_TEST(MemoryAllocatorTest, AddBuffer);
};

using MemoryBlockPtr = std::unique_ptr<MemoryBlock>;

inline MemoryBlockPtr memory_block_ptr(const MemoryBlockParams& params) {
    return std::make_unique<MemoryBlock>(params);
}

}; // namespace vkal
