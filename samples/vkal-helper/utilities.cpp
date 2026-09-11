#include "utilities.hpp"

#include <cmath>
#include <fstream>
#include <print>
#include <ranges>

///////////////////////////////////////////////////////////
vkal::Buffer& add_device_local_buffer(vk::Queue queue, vk::CommandBuffer command,
                                      vkal::Device& device, vkal::RenderResources& render_resources,
                                      const std::string& identifier, vk::DeviceSize size,
                                      vk::BufferUsageFlags usage,
                                      vk::MemoryPropertyFlags memory_properties, void* data) {
    vkal::Buffer& buffer = render_resources.create_buffer(vkal::BufferResourceParams{
        .identifier = identifier,
        .size = size,
        .usage = vk::BufferUsageFlagBits::eTransferDst | usage,
        .memory_properties = vk::MemoryPropertyFlagBits::eDeviceLocal | memory_properties,
    });

    vkal::BufferPtr staging = vkal::buffer_ptr(vkal::BufferParams{
        .vkal_device = device,
        .memory_allocator = render_resources.get_memory_allocator(),
        .size = size,
        .usage = vk::BufferUsageFlagBits::eTransferSrc,
        .memory_properties =
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
    });
    staging->upload(data, size);

    command.begin(vk::CommandBufferBeginInfo());
    vk::BufferCopy2 region;
    region.setSrcOffset(0).setDstOffset(0).setSize(size);
    vk::CopyBufferInfo2 copy_buffer_info;
    copy_buffer_info.setSrcBuffer(staging->get()).setDstBuffer(buffer.get()).setRegions(region);
    command.copyBuffer2(copy_buffer_info);
    command.end();

    vk::CommandBufferSubmitInfo command_submit_info;
    command_submit_info.setCommandBuffer(command).setDeviceMask(1);
    vk::SubmitInfo2 submit_info;
    submit_info.setCommandBufferInfos(command_submit_info);
    queue.submit2(submit_info);
    queue.waitIdle();

    return buffer;
}

///////////////////////////////////////////////////////////
static void barrier_image(vkal::Image& image, vk::CommandBuffer command, vk::AccessFlags2 access,
                          vk::PipelineStageFlags2 stage, vk::ImageLayout layout, uint32_t base_mip,
                          uint32_t mip_count) {
    std::vector<vk::ImageMemoryBarrier2> barriers(mip_count);
    for (const auto& [index, barrier] : std::ranges::views::enumerate(barriers)) {
        uint32_t mip_level = base_mip + static_cast<uint32_t>(index);
        barrier.setImage(image.get())
            .setSrcAccessMask(image.get_access(mip_level))
            .setSrcStageMask(image.get_stage(mip_level))
            .setOldLayout(image.get_layout(mip_level))
            .setDstAccessMask(access)
            .setDstStageMask(stage)
            .setNewLayout(layout)
            .setSubresourceRange(vk::ImageSubresourceRange(image.get_aspect(), mip_level, 1, 0, 1));

        image.set_barrier(layout, access, stage, mip_level);
    }

    vk::DependencyInfo dependency_info;
    dependency_info.setImageMemoryBarriers(barriers);
    command.pipelineBarrier2(dependency_info);
}

///////////////////////////////////////////////////////////
static void copy_buffer_to_image(vkal::Buffer& buffer, vkal::Image& image,
                                 vk::CommandBuffer command) {
    // Pre copy barrier
    barrier_image(image, command, vk::AccessFlagBits2::eTransferWrite,
                  vk::PipelineStageFlagBits2::eTransfer, vk::ImageLayout::eTransferDstOptimal, 0,
                  1);

    vk::BufferImageCopy2 region;
    region.setBufferOffset(0)
        .setBufferRowLength(0)
        .setBufferImageHeight(0)
        .setImageOffset(vk::Offset3D(0, 0, 0))
        .setImageExtent(image.get_extent())
        .setImageSubresource(vk::ImageSubresourceLayers(image.get_aspect(), 0, 0, 1));
    vk::CopyBufferToImageInfo2 copy_info;
    copy_info.setSrcBuffer(buffer.get())
        .setDstImage(image.get())
        .setRegions(region)
        .setDstImageLayout(vk::ImageLayout::eTransferDstOptimal);
    command.copyBufferToImage2(copy_info);
}

///////////////////////////////////////////////////////////
static void generate_mipmaps(vkal::Image& image, vk::CommandBuffer command) {
    vk::ImageBlit2 image_blit;
    image_blit.setSrcSubresource(vk::ImageSubresourceLayers(image.get_aspect(), 0, 0, 1))
        .setSrcOffsets({vk::Offset3D(0, 0, 0)})
        .setDstSubresource(vk::ImageSubresourceLayers(image.get_aspect(), 0, 0, 1))
        .setDstOffsets({vk::Offset3D(0, 0, 0)});

    // Source image layout will always be transfer source
    // And dst image layout will always be transfer dst
    vk::ImageLayout src_layout = vk::ImageLayout::eTransferSrcOptimal;
    vk::AccessFlags2 src_access = vk::AccessFlagBits2::eTransferRead;
    vk::PipelineStageFlags2 src_stage = vk::PipelineStageFlagBits2::eTransfer;

    vk::ImageLayout dst_layout = vk::ImageLayout::eTransferDstOptimal;
    vk::AccessFlags2 dst_access = vk::AccessFlagBits2::eTransferWrite;
    vk::PipelineStageFlags2 dst_stage = vk::PipelineStageFlagBits2::eTransfer;

    vk::BlitImageInfo2 blit_image_info;
    blit_image_info.setSrcImage(image.get())
        .setSrcImageLayout(src_layout)
        .setDstImage(image.get())
        .setDstImageLayout(dst_layout)
        .setRegions(image_blit)
        .setFilter(vk::Filter::eLinear);

    vk::Extent3D base_extent = image.get_extent();
    vk::Offset3D src_offset(base_extent.width, base_extent.height, base_extent.depth);

    uint32_t mip_count = image.get_mip_count();

    // We want to transit all mip levels to transfer dst
    // Each iteration we transit the current mip level i to the transfer
    // src layout and perform blitting to the next mip level
    barrier_image(image, command, dst_access, dst_stage, dst_layout, 1, mip_count - 1);

    // Now we want to only transit each mip level for each iteration from
    // transfer dst to transfer src
    for (uint32_t i = 0; i < mip_count; ++i) {
        // Transit currernt mip level to src layout
        barrier_image(image, command, src_access, src_stage, src_layout, i, 1);

        // In the last mip level we only transit and not blitting
        // so that all mip levels are at transfer src layout for
        // the last transition
        if (i == mip_count - 1) {
            break;
        }

        // The extent halves for each mip level
        vk::Offset3D dst_offset(src_offset.x > 1 ? src_offset.x / 2 : 1,
                                src_offset.y > 1 ? src_offset.y / 2 : 1, 1);

        image_blit.srcSubresource.mipLevel = i;
        image_blit.dstSubresource.mipLevel = i + 1;
        image_blit.srcOffsets[1] = src_offset;
        image_blit.dstOffsets[1] = dst_offset;

        command.blitImage2(blit_image_info);

        src_offset = dst_offset;
    }
}

///////////////////////////////////////////////////////////
std::map<std::string, std::reference_wrapper<vkal::Image>>
load_images(vk::Queue queue, vk::CommandBuffer command, vkal::Device& device,
            vkal::RenderResources& render_resources,
            const std::vector<std::filesystem::path>& paths) {
    std::vector<SDL_Surface*> surfaces;
    std::vector<std::reference_wrapper<vkal::Image>> images;
    std::vector<std::string> stems;
    std::vector<vkal::BufferPtr> stagings;
    for (const std::filesystem::path& path : paths) {
        // Load surface from source
        SDL_Surface* unconverted_surface = SDL_LoadSurface(path.generic_string().c_str());
        if (!unconverted_surface) {
            throw std::runtime_error(std::format("Failed to load image {}", path.generic_string()));
        }
        if (unconverted_surface->format !=
            SDL_PIXELFORMAT_ABGR8888) { // Convert of Srgb format if the surface isn't already in
                                        // the correct format
            surfaces.push_back(SDL_ConvertSurface(unconverted_surface, SDL_PIXELFORMAT_ABGR8888));
            SDL_DestroySurface(unconverted_surface);
        } else {
            surfaces.push_back(unconverted_surface);
        }
        SDL_Surface* surface = surfaces.back();

        // Calculate mip count
        uint32_t mip_count = std::min(surfaces.back()->w, surfaces.back()->h);
        mip_count = static_cast<uint32_t>(log2(mip_count)) + 1;

        // Create image
        std::string stem = path.stem().generic_string();
        images.push_back(render_resources.create_image(vkal::ImageResourceParams{
            .identifier = stem,
            .type = vk::ImageType::e2D,
            .view_type = vk::ImageViewType::e2D,
            .extent = vk::Extent3D(surface->w, surface->h, 1),
            .format = vk::Format::eR8G8B8A8Srgb,
            .sample_count = vk::SampleCountFlagBits::e1,
            .aspects = vk::ImageAspectFlagBits::eColor,
            .mip_levels = mip_count,
            .usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc |
                     vk::ImageUsageFlagBits::eSampled,
            .memory_properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
        }));
        stems.push_back(stem);

        // Create staging buffers
        vk::DeviceSize base_size(surface->w * surface->h * 4);
        stagings.push_back(std::move(vkal::buffer_ptr(vkal::BufferParams{
            .vkal_device = device,
            .memory_allocator = render_resources.get_memory_allocator(),
            .size = base_size,
            .usage = vk::BufferUsageFlagBits::eTransferSrc,
            .memory_properties = vk::MemoryPropertyFlagBits::eHostVisible |
                                 vk::MemoryPropertyFlagBits::eHostCoherent,
        })));
        stagings.back()->upload(surface->pixels, base_size);
    }

    // Begin recording commands
    command.begin(vk::CommandBufferBeginInfo());

    // Perform copying
    for (size_t i = 0; i < images.size(); ++i) {
        vkal::Image& image = images[i];
        vkal::Buffer& staging = *stagings[i];
        copy_buffer_to_image(staging, image, command);
        generate_mipmaps(image, command);
    }

    // Submit all commands
    command.end();
    vk::CommandBufferSubmitInfo command_submit_info;
    command_submit_info.setCommandBuffer(command).setDeviceMask(1);
    vk::SubmitInfo2 submit_info;
    submit_info.setCommandBufferInfos(command_submit_info);
    queue.submit2(submit_info);
    queue.waitIdle();

    for (SDL_Surface* surface : surfaces) {
        SDL_DestroySurface(surface);
    }

    std::map<std::string, std::reference_wrapper<vkal::Image>> image_map;
    for (const auto& [index, image] : std::ranges::views::enumerate(images)) {
        image_map.try_emplace(stems[index], image);
    }

    return image_map;
}

///////////////////////////////////////////////////////////
nlohmann::json load_level_from_path(const std::filesystem::path& path, const std::string& level) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error(
            std::format("Failed to load json file at path {}", path.generic_string()));
    }

    nlohmann::json json_data;
    file >> json_data;
    nlohmann::json levels = json_data.at("levels");
    std::optional<std::reference_wrapper<const nlohmann::json>> level_data_opt;
    for (const nlohmann::json& level_data : levels) {
        if (level_data.at("identifier").get<std::string>() == level) {
            level_data_opt = level_data;
            break;
        }
    }
    if (!level_data_opt.has_value()) {
        throw std::runtime_error(std::format("Cannot find any level with identifer {}", level));
    }
    const nlohmann::json& level_data = *level_data_opt;
    for (const auto& [index, layer] : level_data.at("layerInstances").items()) {
        std::println("Layer {}:", index);
        for (const auto& [key, value] : layer.items()) {
            std::println("{} {}", key, value.type_name());
        }
    }

    return json_data;
}
