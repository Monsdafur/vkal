#pragma once

#include "device.hpp"
#include "image.hpp"

#include <functional>
#include <memory>

namespace vkal {

struct SurfaceParams {
    SDL_Window* window;
    Instance& vkal_instance;
    Device& vkal_device;
    uint32_t image_count;
    vk::SurfaceFormatKHR surface_format;
    std::vector<vk::PresentModeKHR> present_modes;
};

class Surface {
  public:
    // No copy and mo move
    Surface(const Surface&) = delete;
    Surface& operator=(const Surface&) = delete;
    Surface(Surface&&) = delete;
    Surface& operator=(Surface&&) = delete;

    explicit Surface(const SurfaceParams& params);

    ~Surface();

    vk::Semaphore& get_current_semaphore();

    void set_resize_callback(std::function<void(vk::Extent2D)> callback);

    std::optional<std::reference_wrapper<Image>> acquire_next_frame(vk::Semaphore& semaphore);

    void present();

  private:
    void create_surface();

    void update_surface_capabilities();

    void setup_surface_settings(const SurfaceParams& params);

    void create_swapchain();

    bool initialized = false;
    uint32_t image_index = 0;
    SDL_Window* window;

    std::function<void(vk::Extent2D)> resize_callback = [](vk::Extent2D) {};

    Instance& vkal_instance;
    Device& vkal_device;
    vk::SurfaceKHR surface;
    vk::Queue present_queue;
    vk::PhysicalDeviceSurfaceInfo2KHR surface_info;
    vk::SurfaceCapabilitiesKHR capabilities;
    uint32_t image_count;
    vk::SurfaceFormatKHR surface_format;
    vk::PresentModeKHR present_mode;
    vk::SwapchainKHR swapchain = nullptr;
    std::vector<ImagePtr> images;
    std::vector<vk::Semaphore> semaphores;

    vk::AcquireNextImageInfoKHR acquire_info;
    vk::PresentInfoKHR present_info;
};

using SurfacePtr = std::unique_ptr<Surface>;

inline SurfacePtr surface_ptr(const SurfaceParams& params) {
    return std::make_unique<Surface>(params);
}

} // namespace vkal
