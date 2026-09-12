#pragma once

#include "device.hpp"
#include "image.hpp"

#include <functional>
#include <memory>

namespace vkal {

struct SurfaceParams {
    SDL_Window* window;
    Instance& instance;
    Device& device;
    uint32_t image_count;
    vk::SurfaceFormatKHR surface_format;
    std::vector<vk::PresentModeKHR> present_modes;
};

class Surface {
  public:
    // No copy and no move
    Surface(const Surface&) = delete;
    Surface& operator=(const Surface&) = delete;
    Surface(Surface&&) = delete;
    Surface& operator=(Surface&&) = delete;

    explicit Surface(const SurfaceParams& params);

    ~Surface();

    vk::Semaphore& get_current_semaphore();

    void
    set_resize_callback(std::function<void(const vk::Extent2D&, const vk::Extent2D&)> callback);

    std::optional<uint32_t> acquire_next_frame(vk::Semaphore semaphore);

    void present();

    size_t get_image_count() const;

    Image& get_image(size_t index);

    const vk::SurfaceCapabilitiesKHR& get_capabilities() const;

  private:
    void create_surface();

    void update_surface_capabilities();

    void setup_surface_settings(const SurfaceParams& params);

    void create_swapchain();

    bool initialized = false;
    uint32_t image_index = 0;
    SDL_Window* window;

    std::function<void(const vk::Extent2D&, const vk::Extent2D&)> resize_callback =
        [](const vk::Extent2D&, const vk::Extent2D&) {};

    Instance& instance;
    Device& device;
    vk::SurfaceKHR vk_surface;
    vk::Queue vk_present_queue;
    vk::PhysicalDeviceSurfaceInfo2KHR surface_info;
    vk::SurfaceCapabilitiesKHR capabilities;
    vk::Extent2D current_extent;
    uint32_t image_count;
    vk::SurfaceFormatKHR surface_format;
    vk::PresentModeKHR present_mode;
    vk::SwapchainKHR vk_swapchain = nullptr;
    std::vector<ImagePtr> images;
    std::vector<vk::Semaphore> vk_semaphores;
};

using SurfacePtr = std::unique_ptr<Surface>;

inline SurfacePtr surface_ptr(const SurfaceParams& params) {
    return std::make_unique<Surface>(params);
}

} // namespace vkal
