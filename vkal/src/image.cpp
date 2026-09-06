#include "image.hpp"

namespace vkal {

///////////////////////////////////////////////////////////
Image::Image(const ImageParams& params)
    : vkal_device(params.vkal_device), type(params.type), view_type(params.view_type),
      extent(params.extent), format(params.format), aspects(params.aspects),
      mip_levels(params.mip_levels), image(this->create_image(params)),
      memory_requirements(this->get_requirements()),
      allocator_info(params.memory_allocator.bind_image(this->image, params.memory_properties,
                                                        this->memory_requirements)),
      view(this->create_view()) {
    this->allocator_info->block.map_data(this->allocator_info->chunk, &this->data);
    this->is_swapchain_owned = false;

    this->accesses = std::vector<vk::AccessFlags2>(this->mip_levels, vk::AccessFlagBits2::eNone);
    this->stages =
        std::vector<vk::PipelineStageFlags2>(this->mip_levels, vk::PipelineStageFlagBits2::eNone);
    this->layouts = std::vector<vk::ImageLayout>(this->mip_levels, vk::ImageLayout::eUndefined);
}

///////////////////////////////////////////////////////////
Image::Image(const SwapchainImageParams& params)
    : vkal_device(params.vkal_device), type(vk::ImageType::e2D), view_type(vk::ImageViewType::e2D),
      extent(params.extent, 1), format(params.format), aspects(params.aspects), mip_levels(1),
      image(params.image), view(this->create_view()) {
    this->is_swapchain_owned = true;
}

///////////////////////////////////////////////////////////
Image::~Image() {
    this->vkal_device.get().destroyImageView(this->view);
    if (!this->is_swapchain_owned) {
        this->vkal_device.get().destroyImage(this->image);

        // Sync with memory allocator data
        this->allocator_info->block.remove_chunk(this->allocator_info->chunk);
        this->allocator_info->allocator.clean(this->allocator_info->block);
#if defined(ENABLE_DEBUG)
        this->allocator_info->allocator.dump();
#endif
    }
}

///////////////////////////////////////////////////////////
void Image::set_barrier(vk::ImageLayout layout, vk::AccessFlags2 access,
                        vk::PipelineStageFlags2 stage, uint32_t target_mip, uint32_t mip_count) {
    for (size_t i = target_mip; i < target_mip + mip_count; ++i) {
        this->accesses[i] = access;
        this->stages[i] = stage;
        this->layouts[i] = layout;
    }
}

///////////////////////////////////////////////////////////
const vk::MemoryRequirements Image::get_memory_requirements() const {
    return this->memory_requirements;
}

///////////////////////////////////////////////////////////
vk::Image Image::get() {
    return this->image;
}

///////////////////////////////////////////////////////////
uint32_t Image::get_mip_count() const {
    return this->mip_levels;
}

vk::Extent3D Image::get_extent() const {
    return this->extent;
}

///////////////////////////////////////////////////////////
vk::ImageView Image::get_view() const {
    return this->view;
}

///////////////////////////////////////////////////////////
vk::Image Image::create_image(const ImageParams& params) {
    vk::ImageCreateInfo image_create_info;
    image_create_info.setImageType(this->type)
        .setExtent(this->extent)
        .setFormat(this->format)
        .setMipLevels(this->mip_levels)
        .setUsage(params.usage)
        .setSamples(params.sample_count)
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
vk::ImageAspectFlags Image::get_aspect() {
    return this->aspects;
}

///////////////////////////////////////////////////////////
vk::ImageView Image::create_view() {
    vk::ImageViewCreateInfo image_view_create_info;

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
        .setViewType(this->view_type)
        .setFormat(this->format)
        .setComponents(component_mapping)
        .setSubresourceRange(sub_resource_range);

    return this->vkal_device.get().createImageView(image_view_create_info);
}

///////////////////////////////////////////////////////////
vk::AccessFlags2 Image::get_access(size_t index) {
    return this->accesses[index];
}

///////////////////////////////////////////////////////////
vk::PipelineStageFlags2 Image::get_stage(size_t index) {
    return this->stages[index];
}

///////////////////////////////////////////////////////////
vk::ImageLayout Image::get_layout(size_t index) {
    return this->layouts[index];
}

} // namespace vkal
