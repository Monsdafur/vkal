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

TEST(BufferTest, MapData) {
    TestObjects o = create_test_objects();

    std::array<float, 4> input = {1.0f, 2.0f, 4.0f, 8.0f};
    std::array<float, 4> output;

    BufferPtr b = buffer_ptr(BufferParams{
        .vkal_device = *o.vkal_device,
        .memory_allocator = *o.memory_allocator,
        .size = sizeof(float) * input.size(),
        .usage = vk::BufferUsageFlagBits::eStorageBuffer,
        .memory_properties = vk::MemoryPropertyFlagBits::eHostVisible,
    });

    b->upload(input.data(), sizeof(float) * input.size());
    b->get_raw_data(output.data());

    for (size_t i = 0; i < input.size(); ++i) {
        ASSERT_EQ(output.at(i), input.at(i));
    }

    SDL_Quit();
}

} // namespace vkal
