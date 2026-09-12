#pragma once

#include "memory_allocator.hpp"

#include <memory>

namespace vkal {

struct ImageParams {
    Device& device;
    MemoryAllocator& memory_allocator;
    vk::ImageType type = vk::ImageType::e2D;
    vk::ImageViewType view_type = vk::ImageViewType::e2D;
    vk::Extent3D extent;
    vk::Format format = vk::Format::eR8G8B8A8Srgb;
    vk::SampleCountFlagBits sample_count = vk::SampleCountFlagBits::e1;
    vk::ImageAspectFlags aspects = vk::ImageAspectFlagBits::eColor;
    uint32_t mip_levels = 1;
    vk::ImageUsageFlags usage;
    vk::MemoryPropertyFlags memory_properties;
};

struct SwapchainImageParams {
    Device& device;
    vk::Image image;
    vk::Extent2D extent;
    vk::Format format;
    vk::ImageAspectFlags aspects;
};

class Image {
  public:
    // No copy and no move
    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;
    Image(Image&&) = delete;
    Image& operator=(Image&&) = delete;

    explicit Image(const ImageParams& params);

    explicit Image(const SwapchainImageParams& params);

    ~Image();

    void set_barrier(vk::ImageLayout layout, vk::AccessFlags2 access, vk::PipelineStageFlags2 stage,
                     uint32_t target_mip = 0, uint32_t mip_count = 1);

    const vk::MemoryRequirements get_memory_requirements() const;

    vk::Image get();

    uint32_t get_mip_count() const;

    vk::Extent3D get_extent() const;

    vk::ImageView get_view() const;

    vk::ImageAspectFlags get_aspect();

    vk::AccessFlags2 get_access(size_t index);

    vk::PipelineStageFlags2 get_stage(size_t index);

    vk::ImageLayout get_layout(size_t index);

  private:
    vk::Image create_image(const ImageParams& params);

    vk::MemoryRequirements get_requirements();

    vk::ImageView create_view();

    Device& device;
    bool is_swapchain_owned;

    vk::ImageType type;
    vk::ImageViewType view_type;
    vk::Extent3D extent;
    vk::Format format;
    vk::ImageAspectFlags aspects;
    uint32_t mip_levels;

    vk::Image vk_image;
    vk::MemoryRequirements memory_requirements;
    std::optional<MemoryAllocatorInfo> allocator_info;
    vk::ImageView vk_view;
    std::vector<vk::AccessFlags2> accesses;
    std::vector<vk::PipelineStageFlags2> stages;
    std::vector<vk::ImageLayout> layouts;

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
