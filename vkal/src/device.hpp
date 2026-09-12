#pragma once

#include <SDL3/SDL_vulkan.h>
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include "instance.hpp"

#include <cstdint>

namespace vkal {

struct DeviceParams {
    Instance& instance;
    std::vector<std::string> device_extensions;
};

class Device {
  public:
    // No copy and no move
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;
    Device(Device&&) = delete;
    Device& operator=(Device&&) = delete;

    explicit Device(const DeviceParams& params);

    ~Device();

    vk::PhysicalDevice get_physical();

    vk::Device get();

    const vk::PhysicalDeviceProperties& get_properties() const;

    uint32_t get_queue_index(vk::QueueFlags queue_flags);

    vk::Queue get_queue(uint32_t index);

    vk::Queue get_present_queue(const vk::SurfaceKHR& surface);

    vk::CommandPool get_command_pool(uint32_t index);

    const std::vector<vk::Format>& get_supported_depth_formats(vk::ImageTiling tiling) const;

  private:
    void select_physical_device(const DeviceParams& params);

    void create_device(const DeviceParams& params);

    void get_queues();

    void query_depth_formats();

    Instance& instance;
    vk::PhysicalDeviceProperties physical_device_properties;
    vk::PhysicalDevice vk_physical_device = nullptr;
    vk::Device vk_device = nullptr;
    std::vector<vk::QueueFamilyProperties> queue_properties;
    std::vector<vk::Queue> vk_queues;
    std::vector<vk::CommandPool> vk_command_pools;
    std::vector<vk::Format> linear_depth_formats;
    std::vector<vk::Format> optimal_depth_formats;
};

using DevicePtr = std::unique_ptr<Device>;

inline DevicePtr device_ptr(const DeviceParams& params) {
    return std::make_unique<Device>(params);
}

} // namespace vkal
