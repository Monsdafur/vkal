#include "image.hpp"

namespace vkal {

///////////////////////////////////////////////////////////
Image::Image(const ImageParams& params)
    : vkal_device(params.vkal_device), memory_allocator(params.memory_allocator), type(params.type),
      extent(params.extent), format(params.format), aspects(params.aspects),
      mip_levels(params.mip_levels), image(this->create_image(params)),
      memory_requirements(this->get_requirements()), memory_block(this->get_memory_block(params)) {
    // Setting up resouce range correspond to each mip level
    vk::ImageSubresourceRange sub_resource_range;
    sub_resource_range.setAspectMask(this->aspects)
        .setBaseArrayLayer(0)
        .setLayerCount(1)
        .setLevelCount(1);
    vk::ImageMemoryBarrier2 barrier;
    barrier.setImage(this->image)
        .setOldLayout(vk::ImageLayout::eUndefined)
        .setSrcAccessMask(vk::AccessFlagBits2::eNone)
        .setSrcStageMask(vk::PipelineStageFlagBits2::eNone);

    this->memory_block.first.map_data(this->memory_block.second, &this->data);
}

///////////////////////////////////////////////////////////
Image::~Image() {
    this->vkal_device.get().destroyImageView(this->view);
    this->vkal_device.get().destroyImage(this->image);

    // Sync with memory allocator data
    this->memory_block.first.remove_chunk(this->memory_block.second);
    this->memory_allocator.clean(this->memory_block.first);
}

///////////////////////////////////////////////////////////
const vk::MemoryRequirements Image::get_memory_requirements() const {
    return this->memory_requirements;
}

///////////////////////////////////////////////////////////
vk::Image Image::create_image(const ImageParams& params) {
    vk::ImageCreateInfo image_create_info;
    image_create_info.setImageType(this->type)
        .setExtent(this->extent)
        .setFormat(this->format)
        .setMipLevels(this->mip_levels)
        .setUsage(params.usage)
        .setArrayLayers(1)
        .setSharingMode(vk::SharingMode::eExclusive)
        .setTiling(vk::ImageTiling::eOptimal);

    return this->vkal_device.get().createImage(image_create_info);
}

///////////////////////////////////////////////////////////
vk::MemoryRequirements Image::get_requirements() {
    vk::ImageMemoryRequirementsInfo2 memory_requirements_info;
    memory_requirements_info.setImage(this->image);
    return this->vkal_device.get()
        .getImageMemoryRequirements2(memory_requirements_info)
        .memoryRequirements;
}

///////////////////////////////////////////////////////////
std::pair<MemoryBlock&, MemoryChunk&> Image::get_memory_block(const ImageParams& params) {
    return this->memory_allocator.bind_image(this->image, params.memory_properties,
                                             this->memory_requirements);
}

///////////////////////////////////////////////////////////
void Image::upload(void* data, vk::DeviceSize size) {
    if (!this->data) {
        throw std::runtime_error("Uploading to a non host visible buffer");
    }
    if (size > memory_requirements.size) {
        throw std::runtime_error("Upload size exceeds image size");
    }
    std::memcpy(this->data, data, size);
}

///////////////////////////////////////////////////////////
void Image::get_raw_data(void* data) {
    if (!this->data) {
        throw std::runtime_error("Reading from a non host visible buffer");
    }
    std::memcpy(data, this->data, this->memory_requirements.size);
}

///////////////////////////////////////////////////////////
vk::ImageView Image::create_view(const ImageParams& params) {
    vk::ImageViewCreateInfo image_view_create_info;

    switch (this->type) {
    case vk::ImageType::e1D:
        image_view_create_info.setViewType(vk::ImageViewType::e1D);
    case vk::ImageType::e2D:
        image_view_create_info.setViewType(vk::ImageViewType::e2D);
    case vk::ImageType::e3D:
        image_view_create_info.setViewType(vk::ImageViewType::e3D);
    default:
        throw std::runtime_error("Abiguous image type not compatible to create image view");
    }

    vk::ComponentMapping component_mapping;
    component_mapping.setR(vk::ComponentSwizzle::eR)
        .setG(vk::ComponentSwizzle::eG)
        .setB(vk::ComponentSwizzle::eB)
        .setA(vk::ComponentSwizzle::eA);

    vk::ImageSubresourceRange sub_resource_range;
    sub_resource_range.setAspectMask(this->aspects)
        .setBaseMipLevel(0)
        .setLevelCount(this->mip_levels)
        .setBaseArrayLayer(0)
        .setLayerCount(1);

    image_view_create_info.setImage(this->image)
        .setFormat(this->format)
        .setComponents(component_mapping)
        .setSubresourceRange(sub_resource_range);

    return this->vkal_device.get().createImageView(image_view_create_info);
}

} // namespace vkal
