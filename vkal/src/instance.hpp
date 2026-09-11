#pragma once

#include <SDL3/SDL_vulkan.h>
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>

#include <memory>

namespace vkal {

class Instance {
  public:
    // No copy and mo move
    Instance(const Instance&) = delete;
    Instance& operator=(const Instance&) = delete;
    Instance(Instance&&) = delete;
    Instance& operator=(Instance&&) = delete;

    Instance();

    ~Instance();

    vk::Instance get();

  private:
    void create_instance();

    void create_message_debugger();

    vk::detail::DynamicLoader dynamic_loader;
    vk::Instance vk_instance = nullptr;
    vk::DebugUtilsMessengerEXT vk_debugger = nullptr;
};

using InstancePtr = std::unique_ptr<Instance>;

inline InstancePtr instance_ptr() {
    return std::make_unique<Instance>();
}

} // namespace vkal
