#include "../vkal/src/buffer.hpp"
#include "../vkal/src/command.hpp"
#include "../vkal/src/common.hpp"
#include "../vkal/src/descriptor.hpp"
#include "../vkal/src/device.hpp"
#include "../vkal/src/instance.hpp"
#include "../vkal/src/pipeline.hpp"
#include "../vkal/src/render_graph.hpp"
#include "../vkal/src/render_resources.hpp"
#include "../vkal/src/surface.hpp"

#include "vkal-helper/json.hpp"
#include "vkal-helper/utilities.hpp"

#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <exception>
#include <print>
#include <stdexcept>

int main() {
    try {
        // Create Window
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            throw std::runtime_error(SDL_GetError());
        }

        SDL_Window* window = SDL_CreateWindow("Tilemap", 600, 600, SDL_WINDOW_VULKAN);
        if (!window) {
            throw std::runtime_error(SDL_GetError());
        }

        // Create device
        vkal::InstancePtr instance = vkal::instance_ptr();
        vkal::DevicePtr device = vkal::device_ptr(vkal::DeviceParams{
            .instance = *instance,
            .device_extensions = {vk::KHRSwapchainExtensionName, vk::KHRSpirv14ExtensionName,
                                  vk::KHRSynchronization2ExtensionName}});

        uint32_t queue_index = device->get_queue_index(vk::QueueFlagBits::eGraphics);
        vkal::CommandPtr command = vkal::command_ptr(vkal::CommandParams{
            .device = *device,
            .level = vk::CommandBufferLevel::ePrimary,
            .queue_index = queue_index,
            .command_count = 1,
        });
        vk::Queue vk_queue = device->get_queue(queue_index);
        vk::CommandBuffer vk_command = command->get(0);

        // Create surface
        vkal::SurfacePtr surface = vkal::surface_ptr(vkal::SurfaceParams{
            .window = window,
            .instance = *instance,
            .device = *device,
            .image_count = 3,
            .surface_format =
                vk::SurfaceFormatKHR(vk::Format::eB8G8R8A8Srgb, vk::ColorSpaceKHR::eSrgbNonlinear),
            .present_modes =
                {
                    vk::PresentModeKHR::eFifo,
                    vk::PresentModeKHR::eImmediate,
                    vk::PresentModeKHR::eMailbox,
                },
        });

        // Create memory allocator
        vkal::MemoryAllocatorPtr memory_allocator =
            vkal::memory_allocator_ptr(vkal::MemoryAllocatorParams{
                .device = *device,
                .block_size = vkal::megabytes(128),
            });

        // Create descriptor
        vkal::DescriptorPtr descriptor = vkal::descriptor_ptr(vkal::DescriptorParams{
            .device = *device,
            .max_sets = 10,
            .pool_size = 10,
        });

        // Create graphics resource manager
        vkal::RenderResourcesPtr render_resources =
            vkal::render_resources_ptr(vkal::RenderResourcesParams{
                .device = *device,
                .memory_allocator = *memory_allocator,
            });

        // Load level data
        nlohmann::json json_data = load_level_from_path("resources/data/null.ldtk", "Level_0");

        device->get().waitIdle();
        SDL_Quit();
    } catch (const std::exception& e) {
        std::println("ERROR  | {}", e.what());
    }

    return 0;
}
