#pragma once

#include "memory_allocator.hpp"

#include <memory>

namespace vkal {

struct ImageParams {
    Device& vkal_device;
    MemoryAllocator& memory_allocator;
    vk::ImageType type;
    vk::Extent3D extent;
    vk::Format format;
    vk::ImageAspectFlags aspects;
    uint32_t mip_levels;
    vk::ImageUsageFlags usage;
    vk::MemoryPropertyFlags memory_properties;
};

class Image {
  public:
    // No copy and mo move
    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;
    Image(Image&&) = delete;
    Image& operator=(Image&&) = delete;

    explicit Image(const ImageParams& params);

    ~Image();

    const vk::MemoryRequirements get_memory_requirements() const;

    void upload(void* data, vk::DeviceSize size);

    void get_raw_data(void* data);

    vk::ImageAspectFlags get_aspect();

  private:
    vk::Image create_image(const ImageParams& params);

    vk::MemoryRequirements get_requirements();

    std::pair<MemoryBlock&, MemoryChunk&> get_memory_block(const ImageParams& params);

    vk::ImageView create_view(const ImageParams& params);

    Device& vkal_device;
    MemoryAllocator& memory_allocator;

    vk::ImageType type;
    vk::Extent3D extent;
    vk::Format format;
    vk::ImageAspectFlags aspects;
    uint32_t mip_levels;

    vk::Image image;
    vk::MemoryRequirements memory_requirements;
    std::pair<MemoryBlock&, MemoryChunk&> memory_block;
    vk::ImageView view;
    void* data;

    // Test
    FRIEND_TEST(MemoryAllocatorTest, AddImage);
};

using ImagePtr = std::unique_ptr<Image>;

inline ImagePtr image_ptr(const ImageParams& params) {
    return std::make_unique<Image>(params);
}

} // namespace vkal
