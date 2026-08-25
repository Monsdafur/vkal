#include "../vkal/src/buffer.hpp"
#include "../vkal/src/memory_allocator.hpp"

#include <SDL3/SDL.h>
#include <gtest/gtest.h>

namespace vkal {

struct TestObjects {
    InstancePtr vkal_instance;
    DevicePtr vkal_device;
    MemoryAllocatorPtr memory_allocator;
};

TestObjects create_test_objects() {
    SDL_Init(SDL_INIT_VIDEO);

    InstancePtr vkal_instance = vkal::instance_ptr();
    DevicePtr vkal_device = vkal::device_ptr(vkal::DeviceParams{
        .vkal_instance = *vkal_instance,
        .device_extensions = {vk::KHRSwapchainExtensionName, vk::KHRSpirv14ExtensionName,
                              vk::KHRSynchronization2ExtensionName}});
    MemoryAllocatorPtr memory_allocator = vkal::memory_allocator_ptr(vkal::MemoryAllocatorParams{
        .vkal_device = *vkal_device,
        .block_size = 1024,
    });

    return TestObjects{
        .vkal_instance = std::move(vkal_instance),
        .vkal_device = std::move(vkal_device),
        .memory_allocator = std::move(memory_allocator),
    };
}

BufferPtr create_buffer(vk::DeviceSize size, TestObjects* objects) {
    return buffer_ptr(BufferParams{
        .vkal_device = *objects->vkal_device,
        .memory_allocator = *objects->memory_allocator,
        .size = size,
        .usage = vk::BufferUsageFlagBits::eTransferSrc,
        .memory_properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
    });
}

TEST(MemoryBlockTest, CarvePerfectFit) {
    TestObjects o = create_test_objects();
    BufferPtr b = create_buffer(256, &o);
    MemoryBlock block = MemoryBlock(MemoryBlockParams{
        .vkal_device = *o.vkal_device,
        .block_size = 1024,
        .properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
        .memory_requirements = b->get_memory_requirements(),
    });

    ASSERT_TRUE(block.get_chunk(1024, 16));

    auto chunks = block.get_all_chunks();

    ASSERT_EQ(block.chunk_count, 1);

    ASSERT_EQ(chunks.at(0).get().offset, 0);
    ASSERT_EQ(chunks.at(0).get().size, 1024);
    ASSERT_TRUE(chunks.at(0).get().occupied);

    SDL_Quit();
}

TEST(MemoryBlockTest, CarveLeftFit) {
    TestObjects o = create_test_objects();
    BufferPtr b = create_buffer(256, &o);
    MemoryBlock block = MemoryBlock(MemoryBlockParams{
        .vkal_device = *o.vkal_device,
        .block_size = 1024,
        .properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
        .memory_requirements = b->get_memory_requirements(),
    });

    ASSERT_TRUE(block.get_chunk(128, 16));

    auto chunks = block.get_all_chunks();

    ASSERT_EQ(block.chunk_count, 2);

    ASSERT_EQ(chunks.at(0).get().offset, 0);
    ASSERT_EQ(chunks.at(0).get().size, 128);
    ASSERT_TRUE(chunks.at(0).get().occupied);

    ASSERT_EQ(chunks.at(1).get().offset, 128);
    ASSERT_EQ(chunks.at(1).get().size, 896);
    ASSERT_FALSE(chunks.at(1).get().occupied);

    SDL_Quit();
}

TEST(MemoryBlockTest, CarveRightFit) {
    TestObjects o = create_test_objects();
    BufferPtr b = create_buffer(256, &o);
    MemoryBlock block = MemoryBlock(MemoryBlockParams{
        .vkal_device = *o.vkal_device,
        .block_size = 1024,
        .properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
        .memory_requirements = b->get_memory_requirements(),
    });

    ASSERT_TRUE(block.get_chunk(128, 16));
    ASSERT_TRUE(block.get_chunk(512, 512));

    auto chunks = block.get_all_chunks();

    ASSERT_EQ(block.chunk_count, 3);

    ASSERT_EQ(chunks.at(0).get().offset, 0);
    ASSERT_EQ(chunks.at(0).get().size, 128);
    ASSERT_TRUE(chunks.at(0).get().occupied);

    ASSERT_EQ(chunks.at(1).get().offset, 128);
    ASSERT_EQ(chunks.at(1).get().size, 384);
    ASSERT_FALSE(chunks.at(1).get().occupied);

    ASSERT_EQ(chunks.at(2).get().offset, 512);
    ASSERT_EQ(chunks.at(2).get().size, 512);
    ASSERT_TRUE(chunks.at(2).get().occupied);

    SDL_Quit();
}

TEST(MemoryBlockTest, CarveMiddleFit) {
    TestObjects o = create_test_objects();
    BufferPtr b = create_buffer(256, &o);
    MemoryBlock block = MemoryBlock(MemoryBlockParams{
        .vkal_device = *o.vkal_device,
        .block_size = 1024,
        .properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
        .memory_requirements = b->get_memory_requirements(),
    });

    ASSERT_TRUE(block.get_chunk(64, 16));
    ASSERT_TRUE(block.get_chunk(512, 128));

    auto chunks = block.get_all_chunks();

    ASSERT_EQ(block.chunk_count, 4);

    ASSERT_EQ(chunks.at(0).get().offset, 0);
    ASSERT_EQ(chunks.at(0).get().size, 64);
    ASSERT_TRUE(chunks.at(0).get().occupied);

    ASSERT_EQ(chunks.at(1).get().offset, 64);
    ASSERT_EQ(chunks.at(1).get().size, 64);
    ASSERT_FALSE(chunks.at(1).get().occupied);

    ASSERT_EQ(chunks.at(2).get().offset, 128);
    ASSERT_EQ(chunks.at(2).get().size, 512);
    ASSERT_TRUE(chunks.at(2).get().occupied);

    ASSERT_EQ(chunks.at(3).get().offset, 640);
    ASSERT_EQ(chunks.at(3).get().size, 384);
    ASSERT_FALSE(chunks.at(3).get().occupied);

    SDL_Quit();
}

TEST(MemoryBlockTest, RemoveNoMerge) {
    TestObjects o = create_test_objects();
    BufferPtr b = create_buffer(256, &o);
    MemoryBlock block = MemoryBlock(MemoryBlockParams{
        .vkal_device = *o.vkal_device,
        .block_size = 1024,
        .properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
        .memory_requirements = b->get_memory_requirements(),
    });

    auto chunk = block.get_chunk(1024, 16);
    ASSERT_TRUE(chunk.has_value());
    block.remove_chunk(*chunk);

    auto chunks = block.get_all_chunks();

    ASSERT_EQ(block.chunk_count, 1);

    ASSERT_EQ(chunks.at(0).get().offset, 0);
    ASSERT_EQ(chunks.at(0).get().size, 1024);
    ASSERT_FALSE(chunks.at(0).get().occupied);

    SDL_Quit();
}

TEST(MemoryBlockTest, RemoveMergeRight) {
    TestObjects o = create_test_objects();
    BufferPtr b = create_buffer(256, &o);
    MemoryBlock block = MemoryBlock(MemoryBlockParams{
        .vkal_device = *o.vkal_device,
        .block_size = 1024,
        .properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
        .memory_requirements = b->get_memory_requirements(),
    });

    auto chunk = block.get_chunk(256, 16);
    ASSERT_TRUE(chunk.has_value());
    block.remove_chunk(*chunk);

    auto chunks = block.get_all_chunks();

    ASSERT_EQ(block.chunk_count, 1);

    ASSERT_EQ(chunks.at(0).get().offset, 0);
    ASSERT_EQ(chunks.at(0).get().size, 1024);
    ASSERT_FALSE(chunks.at(0).get().occupied);

    SDL_Quit();
}

TEST(MemoryBlockTest, RemoveMergeLeft) {
    TestObjects o = create_test_objects();
    BufferPtr b = create_buffer(256, &o);
    MemoryBlock block = MemoryBlock(MemoryBlockParams{
        .vkal_device = *o.vkal_device,
        .block_size = 1024,
        .properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
        .memory_requirements = b->get_memory_requirements(),
    });

    ASSERT_TRUE(block.get_chunk(128, 16));
    auto chunk = block.get_chunk(512, 512);
    ASSERT_TRUE(chunk.has_value());
    block.remove_chunk(*chunk);

    auto chunks = block.get_all_chunks();

    ASSERT_EQ(block.chunk_count, 2);

    ASSERT_EQ(chunks.at(0).get().offset, 0);
    ASSERT_EQ(chunks.at(0).get().size, 128);
    ASSERT_TRUE(chunks.at(0).get().occupied);

    ASSERT_EQ(chunks.at(1).get().offset, 128);
    ASSERT_EQ(chunks.at(1).get().size, 896);
    ASSERT_FALSE(chunks.at(1).get().occupied);

    SDL_Quit();
}

TEST(MemoryBlockTest, RemoveMergeBoth) {
    TestObjects o = create_test_objects();
    BufferPtr b = create_buffer(256, &o);
    MemoryBlock block = MemoryBlock(MemoryBlockParams{
        .vkal_device = *o.vkal_device,
        .block_size = 1024,
        .properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
        .memory_requirements = b->get_memory_requirements(),
    });

    ASSERT_TRUE(block.get_chunk(64, 16));
    auto chunk = block.get_chunk(512, 128);
    ASSERT_TRUE(chunk.has_value());
    block.remove_chunk(*chunk);

    auto chunks = block.get_all_chunks();

    ASSERT_EQ(block.chunk_count, 2);

    ASSERT_EQ(chunks.at(0).get().offset, 0);
    ASSERT_EQ(chunks.at(0).get().size, 64);
    ASSERT_TRUE(chunks.at(0).get().occupied);

    ASSERT_EQ(chunks.at(1).get().offset, 64);
    ASSERT_EQ(chunks.at(1).get().size, 960);
    ASSERT_FALSE(chunks.at(1).get().occupied);

    SDL_Quit();
}

TEST(MemoryAllocatorTest, AddBuffer) {
    TestObjects o = create_test_objects();
    BufferPtr b = create_buffer(256, &o);

    ASSERT_EQ(o.memory_allocator->blocks.size(), 1);

    std::vector<std::vector<std::reference_wrapper<MemoryChunk>>> block_chunks;

    for (auto& block : o.memory_allocator->blocks) {
        block_chunks.push_back(block->get_all_chunks());
    }

    ASSERT_EQ(block_chunks.at(0).at(0).get().offset, 0);
    ASSERT_EQ(block_chunks.at(0).at(0).get().size, 256);
    ASSERT_TRUE(block_chunks.at(0).at(0).get().occupied);

    SDL_Quit();
}

TEST(MemoryAllocatorTest, BufferFail) {
    TestObjects o = create_test_objects();
    EXPECT_THROW(create_buffer(2048, &o), std::runtime_error);

    SDL_Quit();
}

TEST(MemoryAllocatorTest, AddBufferNewBlockOverflow) {
    TestObjects o = create_test_objects();
    BufferPtr b0 = create_buffer(740, &o);
    BufferPtr b1 = create_buffer(800, &o);

    ASSERT_EQ(o.memory_allocator->blocks.size(), 2);

    SDL_Quit();
}

TEST(MemoryAllocatorTest, AddBufferHostVisible) {
    TestObjects o = create_test_objects();
    BufferPtr b = buffer_ptr(BufferParams{
        .vkal_device = *o.vkal_device,
        .memory_allocator = *o.memory_allocator,
        .size = 16,
        .usage = vk::BufferUsageFlagBits::eStorageBuffer,
        .memory_properties =
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
    });

    ASSERT_EQ(o.memory_allocator->blocks.size(), 1);
    ASSERT_NE(b->data, nullptr);

    SDL_Quit();
}

TEST(MemoryAllocatorTest, AddBufferNewBlockIncompatibleType) {
    TestObjects o = create_test_objects();
    BufferPtr b0 = buffer_ptr(BufferParams{
        .vkal_device = *o.vkal_device,
        .memory_allocator = *o.memory_allocator,
        .size = 40,
        .usage = vk::BufferUsageFlagBits::eStorageBuffer,
        .memory_properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
    });
    BufferPtr b1 = buffer_ptr(BufferParams{
        .vkal_device = *o.vkal_device,
        .memory_allocator = *o.memory_allocator,
        .size = 16,
        .usage = vk::BufferUsageFlagBits::eStorageBuffer,
        .memory_properties =
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
    });

    ASSERT_EQ(o.memory_allocator->blocks.size(), 2);

    SDL_Quit();
}

} // namespace vkal
