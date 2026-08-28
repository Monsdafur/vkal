#include "surface.hpp"
#include "common.hpp"
#include "image.hpp"

namespace vkal {

///////////////////////////////////////////////////////////
static vk::SurfaceFormatKHR
evaluate_surface_format(const std::vector<vk::SurfaceFormat2KHR>& surface_formats,
                        vk::SurfaceFormatKHR desired_surface_format) {
    vk::SurfaceFormat2KHR surface_format = surface_formats.front();
    auto surface_format_it = std::ranges::find_if(
        surface_formats, [&desired_surface_format](const vk::SurfaceFormat2KHR& format) {
            return format.surfaceFormat.format == desired_surface_format.format &&
                   format.surfaceFormat.colorSpace == desired_surface_format.colorSpace;
        });
    if (surface_format_it != surface_formats.end()) {
        surface_format = *surface_format_it;
    } else {

#if defined(ENABLE_DEBUG)
        debug(std::format("Surface format {} {} not supported fallback to {} {}",
                          vk::to_string(desired_surface_format.format),
                          vk::to_string(desired_surface_format.colorSpace),
                          vk::to_string(surface_format.surfaceFormat.format),
                          vk::to_string(surface_format.surfaceFormat.colorSpace)));
#endif
    }

    return surface_format.surfaceFormat;
}

///////////////////////////////////////////////////////////
vk::PresentModeKHR
evaluate_present_modes(const std::vector<vk::PresentModeKHR>& available_present_modes,
                       const std::vector<vk::PresentModeKHR>& present_modes) {
#if defined(ENABLE_DEBUG)
    debug("Evaluating present modes...");
#endif

    vk::PresentModeKHR present_mode = available_present_modes.front();
    bool found = false;
    for (vk::PresentModeKHR desired_present_mode : present_modes) {
        auto present_mode_it = std::ranges::find_if(
            available_present_modes, [&desired_present_mode](const vk::PresentModeKHR& mode) {
                return mode == desired_present_mode;
            });
        bool supported = false;
        if (present_mode_it != available_present_modes.end()) {
            present_mode = *present_mode_it;
            supported = true;
            found = true;
        }

#if defined(ENABLE_DEBUG)
        debug(std::format("\tPresent mode {} -- {}", vk::to_string(present_mode),
                          supported ? "supported" : "not supported"));
#endif

        if (found) {
            break;
        }
    }

#if defined(ENABLE_DEBUG)
    if (!found) {
        debug(std::format("Failed to find any supported present mode fallback to {}",
                          vk::to_string(present_mode)));
    }
#endif
    return present_mode;
}

///////////////////////////////////////////////////////////
Surface::Surface(const SurfaceParams& params)
    : window(params.window), vkal_instance(params.vkal_instance), vkal_device(params.vkal_device) {
    this->create_surface();
    this->update_surface_capabilities();
    this->setup_surface_settings(params);
    this->create_swapchain();
}

///////////////////////////////////////////////////////////
Surface::~Surface() {
    for (vk::Semaphore semaphore : this->semaphores) {
        this->vkal_device.get().destroySemaphore(semaphore);
    }
    this->vkal_device.get().destroySwapchainKHR(this->swapchain);
    this->vkal_instance.get().destroySurfaceKHR(this->surface);
}

///////////////////////////////////////////////////////////
void Surface::create_surface() {
    VkSurfaceKHR surface_handler = nullptr;
    SDL_Vulkan_CreateSurface(this->window, this->vkal_instance.get(), nullptr, &surface_handler);
    this->surface = vk::SurfaceKHR(surface_handler);
    this->present_queue = this->vkal_device.get_present_queue(this->surface);
    this->surface_info.setSurface(this->surface);
}

///////////////////////////////////////////////////////////
void Surface::update_surface_capabilities() {
    this->capabilities = this->vkal_device.get_physical()
                             .getSurfaceCapabilities2KHR(this->surface_info)
                             .surfaceCapabilities;
}

///////////////////////////////////////////////////////////
void Surface::setup_surface_settings(const SurfaceParams& params) {
    // Evaluate surface formats
    std::vector<vk::SurfaceFormat2KHR> surface_formats =
        this->vkal_device.get_physical().getSurfaceFormats2KHR(surface_info);
    this->surface_format = evaluate_surface_format(surface_formats, params.surface_format);

    // Evaluate present modes
    std::vector<vk::PresentModeKHR> available_present_modes =
        this->vkal_device.get_physical().getSurfacePresentModesKHR(this->surface);
    this->present_mode = evaluate_present_modes(available_present_modes, params.present_modes);

    // Calculate swapchain image count
    this->image_count = params.image_count;
    this->image_count = std::max(this->image_count, this->capabilities.minImageCount);
    this->image_count = std::min(this->image_count,
                                 this->capabilities.maxImageCount < this->capabilities.minImageCount
                                     ? this->image_count
                                     : this->capabilities.maxImageCount);
}

///////////////////////////////////////////////////////////
void Surface::create_swapchain() {

    // Create swapchain
    vk::SwapchainCreateInfoKHR swapchain_create_info;
    swapchain_create_info.setSurface(this->surface)
        .setMinImageCount(this->image_count)
        .setImageFormat(this->surface_format.format)
        .setImageColorSpace(this->surface_format.colorSpace)
        .setImageExtent(this->capabilities.currentExtent)
        .setImageArrayLayers(1)
        .setImageUsage(vk::ImageUsageFlagBits::eColorAttachment)
        .setImageSharingMode(vk::SharingMode::eExclusive)
        .setPreTransform(this->capabilities.currentTransform)
        .setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque)
        .setPresentMode(this->present_mode)
        .setClipped(true);

    if (this->initialized) {
        swapchain_create_info.setOldSwapchain(this->swapchain);
    }

    this->initialized = true;
    this->swapchain = this->vkal_device.get().createSwapchainKHR(swapchain_create_info);

    // Create frame resources
    this->images.clear();
    std::vector<vk::Image> images = this->vkal_device.get().getSwapchainImagesKHR(this->swapchain);
    for (vk::Image& image : images) {
        ImagePtr vkal_image =
            swapchain_image_ptr(SwapchainImageParams{.vkal_device = this->vkal_device,
                                                     .image = image,
                                                     .extent = this->capabilities.currentExtent,
                                                     .format = this->surface_format.format,
                                                     .aspects = vk::ImageAspectFlagBits::eColor});
        this->images.push_back(std::move(vkal_image));
    }

    // Setup acquire and present info
    this->acquire_info.setSwapchain(this->swapchain).setTimeout(UINT64_MAX).setDeviceMask(1);
    this->present_info.setSwapchains(this->swapchain);

    this->resize_callback(this->capabilities.currentExtent);

    // Create semaphores
    for (vk::Semaphore semaphore : this->semaphores) {
        this->vkal_device.get().destroySemaphore(semaphore);
    }
    this->semaphores.clear();
    vk::SemaphoreCreateInfo semaphore_create_info;
    for (size_t i = 0; i < this->images.size(); ++i) {
        this->semaphores.push_back(this->vkal_device.get().createSemaphore(semaphore_create_info));
    }
}

} // namespace vkal
