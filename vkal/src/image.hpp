#pragma once

#include "memory_allocator.hpp"

#include <memory>

namespace vkal {

struct ImageParams {
    Device& vkal_device;
    MemoryAllocator& memory_allocator;
    vk::ImageType type;
    vk::ImageViewType view_type;
    vk::Extent3D extent;
    vk::Format format;
    vk::SampleCountFlagBits sample_count;
    vk::ImageAspectFlags aspects;
    uint32_t mip_levels;
    vk::ImageUsageFlags usage;
    vk::MemoryPropertyFlags memory_properties;
};

struct SwapchainImageParams {
    Device& vkal_device;
    vk::Image image;
    vk::Extent2D extent;
    vk::Format format;
    vk::ImageAspectFlags aspects;
    vk::ImageUsageFlags usage;
};

class Image {
  public:
    // No copy and mo move
    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;
    Image(Image&&) = delete;
    Image& operator=(Image&&) = delete;

    explicit Image(const ImageParams& params);

    explicit Image(const SwapchainImageParams& params);

    ~Image();

    const vk::MemoryRequirements get_memory_requirements() const;

    vk::Image get();

    vk::Extent3D get_extent() const;

    vk::ImageView get_view() const;

    void upload(void* data, vk::DeviceSize size);

    void get_raw_data(void* data);

    vk::ImageAspectFlags get_aspect();

  private:
    vk::Image create_image(const ImageParams& params);

    vk::MemoryRequirements get_requirements();

    vk::ImageView create_view();

    Device& vkal_device;
    bool is_swapchain_owned;

    vk::ImageType type;
    vk::ImageViewType view_type;
    vk::Extent3D extent;
    vk::Format format;
    vk::ImageAspectFlags aspects;
    uint32_t mip_levels;

    vk::Image image;
    vk::MemoryRequirements memory_requirements;
    std::optional<MemoryAllocatorInfo> allocator_info;
    vk::ImageView view;
    void* data;

    // Test
    FRIEND_TEST(MemoryAllocatorTest, AddImage);
};

using ImagePtr = std::unique_ptr<Image>;

inline ImagePtr image_ptr(const ImageParams& params) {
    return std::make_unique<Image>(params);
}

inline ImagePtr swapchain_image_ptr(const SwapchainImageParams& params) {
    return std::make_unique<Image>(params);
}

} // namespace vkal
