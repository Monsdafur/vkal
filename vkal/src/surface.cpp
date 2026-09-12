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
    : window(params.window), instance(params.instance), device(params.device) {
    this->create_surface();
    this->update_surface_capabilities();
    this->setup_surface_settings(params);
    this->create_swapchain();
    this->current_extent = this->capabilities.currentExtent;
}

///////////////////////////////////////////////////////////
Surface::~Surface() {
    for (vk::Semaphore semaphore : this->vk_semaphores) {
        this->device.get().destroySemaphore(semaphore);
    }
    this->device.get().destroySwapchainKHR(this->vk_swapchain);
    this->instance.get().destroySurfaceKHR(this->vk_surface);
}

///////////////////////////////////////////////////////////
vk::Semaphore& Surface::get_current_semaphore() {
    return this->vk_semaphores.at(this->image_index);
}

///////////////////////////////////////////////////////////
void Surface::set_resize_callback(
    std::function<void(const vk::Extent2D&, const vk::Extent2D&)> callback) {
    this->resize_callback = callback;
}

///////////////////////////////////////////////////////////
std::optional<uint32_t> Surface::acquire_next_frame(vk::Semaphore semaphore) {
    uint32_t window_flags = SDL_GetWindowFlags(window);
    if (window_flags & SDL_WINDOW_MINIMIZED) {
        return std::nullopt;
    }

    vk::AcquireNextImageInfoKHR acquire_info;
    acquire_info.setSwapchain(this->vk_swapchain)
        .setSemaphore(semaphore)
        .setTimeout(UINT64_MAX)
        .setDeviceMask(1);
    vk::Result acquire_result;
    try {
        vk::ResultValue<uint32_t> acquire_result_value =
            this->device.get().acquireNextImage2KHR(acquire_info);
        this->image_index = acquire_result_value.value;
        acquire_result = acquire_result_value.result;
    } catch (const vk::OutOfDateKHRError& e) {
        acquire_result = vk::Result::eErrorOutOfDateKHR;
    }

#if defined(ENABLE_DEBUG)
    if (acquire_result != vk::Result::eSuccess) {
        debug(std::format("Image acquire result: {}", vk::to_string(acquire_result)));
    }
#endif

    switch (acquire_result) {
    case vk::Result::eNotReady:
        return std::nullopt;
    case vk::Result::eSuccess:
        return this->image_index;
    case vk::Result::eSuboptimalKHR:
    case vk::Result::eErrorOutOfDateKHR:
        this->device.get().waitIdle();
        this->update_surface_capabilities();
        if (this->current_extent != this->capabilities.currentExtent) {
            this->create_swapchain();
            this->resize_callback(this->current_extent, this->capabilities.currentExtent);
            this->current_extent = this->capabilities.currentExtent;
        }
        return std::nullopt;
    default:
        throw std::runtime_error(std::format("Failed to acquire next image with code {}",
                                             vk::to_string(acquire_result)));
    }
}

///////////////////////////////////////////////////////////
void Surface::present() {
    vk::PresentInfoKHR present_info;
    present_info.setSwapchains(this->vk_swapchain)
        .setWaitSemaphores(this->vk_semaphores[this->image_index])
        .setImageIndices(this->image_index);

    vk::Result present_result;
    try {
        present_result = this->vk_present_queue.presentKHR(present_info);
    } catch (const vk::OutOfDateKHRError& e) {
        present_result = vk::Result::eErrorOutOfDateKHR;
    }

#if defined(ENABLE_DEBUG)
    if (present_result != vk::Result::eSuccess) {
        debug(std::format("Present result: {}", vk::to_string(present_result)));
    }
#endif

    switch (present_result) {
    case vk::Result::eSuccess:
        break;
    case vk::Result::eSuboptimalKHR:
    case vk::Result::eErrorOutOfDateKHR:

        this->device.get().waitIdle();
        this->update_surface_capabilities();
        if (this->current_extent != this->capabilities.currentExtent) {
            this->create_swapchain();
            this->resize_callback(this->current_extent, this->capabilities.currentExtent);
            this->current_extent = this->capabilities.currentExtent;
        }
        break;
    default:
        throw std::runtime_error(std::format("Failed to present image with code {}",
                                             vk::to_string(present_result)));
    }
}

///////////////////////////////////////////////////////////
void Surface::create_surface() {
    VkSurfaceKHR surface_handler = nullptr;
    SDL_Vulkan_CreateSurface(this->window, this->instance.get(), nullptr, &surface_handler);
    this->vk_surface = vk::SurfaceKHR(surface_handler);
    this->vk_present_queue = this->device.get_present_queue(this->vk_surface);
    this->surface_info.setSurface(this->vk_surface);
}

///////////////////////////////////////////////////////////
void Surface::update_surface_capabilities() {
    this->capabilities = this->device.get_physical()
                             .getSurfaceCapabilities2KHR(this->surface_info)
                             .surfaceCapabilities;
}

///////////////////////////////////////////////////////////
void Surface::setup_surface_settings(const SurfaceParams& params) {
    // Evaluate surface formats
    std::vector<vk::SurfaceFormat2KHR> surface_formats =
        this->device.get_physical().getSurfaceFormats2KHR(surface_info);
    this->surface_format = evaluate_surface_format(surface_formats, params.surface_format);

    // Evaluate present modes
    std::vector<vk::PresentModeKHR> available_present_modes =
        this->device.get_physical().getSurfacePresentModesKHR(this->vk_surface);
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
    swapchain_create_info.setSurface(this->vk_surface)
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
        swapchain_create_info.setOldSwapchain(this->vk_swapchain);
    }

    vk::SwapchainKHR new_swapchain =
        this->device.get().createSwapchainKHR(swapchain_create_info);
    if (this->initialized) {
        this->device.get().destroySwapchainKHR(this->vk_swapchain);
    }
    this->vk_swapchain = new_swapchain;

    this->initialized = true;

    // Create frame resources
    this->images.clear();
    std::vector<vk::Image> swapchain_images =
        this->device.get().getSwapchainImagesKHR(this->vk_swapchain);
    for (vk::Image& image : swapchain_images) {
        ImagePtr image_ptr_obj = swapchain_image_ptr(SwapchainImageParams{
            .device = this->device,
            .image = image,
            .extent = this->capabilities.currentExtent,
            .format = this->surface_format.format,
            .aspects = vk::ImageAspectFlagBits::eColor,
        });
        this->images.push_back(std::move(image_ptr_obj));
    }

    // Create semaphores
    for (vk::Semaphore semaphore : this->vk_semaphores) {
        this->device.get().destroySemaphore(semaphore);
    }
    this->vk_semaphores.clear();
    vk::SemaphoreCreateInfo semaphore_create_info;
    for (size_t i = 0; i < this->images.size(); ++i) {
        this->vk_semaphores.push_back(
            this->device.get().createSemaphore(semaphore_create_info));
    }
}

///////////////////////////////////////////////////////////
size_t Surface::get_image_count() const {
    return this->image_count;
}

///////////////////////////////////////////////////////////
Image& Surface::get_image(size_t index) {
    return *this->images.at(index);
}

///////////////////////////////////////////////////////////
const vk::SurfaceCapabilitiesKHR& Surface::get_capabilities() const {
    return this->capabilities;
}

} // namespace vkal
