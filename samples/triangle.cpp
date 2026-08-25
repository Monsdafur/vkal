#include "../vkal/src/buffer.hpp"
#include "../vkal/src/command.hpp"
#include "../vkal/src/device.hpp"
#include "../vkal/src/instance.hpp"

#include <SDL3/SDL.h>

#include <exception>
#include <print>
#include <stdexcept>

int main() {
    try {
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            throw std::runtime_error(SDL_GetError());
        }

        vkal::InstancePtr vkal_instance = vkal::instance_ptr();
        vkal::DevicePtr vkal_device = vkal::device_ptr(vkal::DeviceParams{
            .vkal_instance = *vkal_instance,
            .device_extensions = {vk::KHRSwapchainExtensionName, vk::KHRSpirv14ExtensionName,
                                  vk::KHRSynchronization2ExtensionName}});
        uint32_t transfer_index = vkal_device->get_queue_index(vk::QueueFlagBits::eTransfer);
        vk::Queue transfer_queue = vkal_device->get_queue(transfer_index);
        vkal::CommandPtr transfer_commands = vkal::command_ptr(
            (vkal::CommandParams{.vkal_device = *vkal_device, .queue_index = transfer_index}));

        vkal::MemoryAllocatorPtr memory_allocator =
            vkal::memory_allocator_ptr(vkal::MemoryAllocatorParams{
                .vkal_device = *vkal_device,
                .block_size = 2048,
            });

        std::vector<vkal::BufferPtr> buffers;
        {
            std::vector<vkal::BufferPtr> buffers_s;
            for (uint32_t i = 0; i < 40; ++i) {
                if (i % 5 == 0) {
                    buffers.emplace_back(vkal::buffer_ptr(vkal::BufferParams{
                        .vkal_device = *vkal_device,
                        .memory_allocator = *memory_allocator,
                        .size = sizeof(float) * 32,
                        .usage = vk::BufferUsageFlagBits::eIndexBuffer |
                                 vk::BufferUsageFlagBits::eTransferDst,
                        .memory_properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
                    }));
                }
                if (i % 2 == 0) {
                    buffers_s.emplace_back(vkal::buffer_ptr(vkal::BufferParams{
                        .vkal_device = *vkal_device,
                        .memory_allocator = *memory_allocator,
                        .size = sizeof(float) * 96,
                        .usage = vk::BufferUsageFlagBits::eVertexBuffer,
                        .memory_properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
                    }));
                }
            }
            std::println("ORIGINAL");
            memory_allocator->dump(16);
        }

        std::println("AFTER FREEING UP SOME MEMORY");
        memory_allocator->dump(16);
        for (uint32_t i = 0; i < 40; ++i) {
            buffers.emplace_back(vkal::buffer_ptr(vkal::BufferParams{
                .vkal_device = *vkal_device,
                .memory_allocator = *memory_allocator,
                .size = sizeof(float) * 48,
                .usage = vk::BufferUsageFlagBits::eUniformBuffer,
                .memory_properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
            }));
        }

        std::println("AFTER ALLOCATING NEW SECTIONS");
        memory_allocator->dump(16);

        SDL_Quit();
    } catch (const std::exception& e) {
        std::println("ERROR  | {}", e.what());
    }

    return 0;
}
