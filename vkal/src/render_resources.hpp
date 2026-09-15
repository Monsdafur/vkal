#pragma once

#include "buffer.hpp"
#include "image.hpp"
#include "pipeline.hpp"
#include "sampler.hpp"

#include <map>
#include <string>

namespace vkal {

struct DescriptorLayoutResourceParams {
    std::string identifier;
    std::vector<vk::DescriptorSetLayoutBinding> bindings;
    std::vector<vk::DescriptorBindingFlags> binding_flags;
};

struct PipelineLayoutResourceParams {
    std::string identifier;
    std::vector<std::string> descriptor_layout_identifiers;
    std::vector<vk::PushConstantRange> push_constants;
};

struct GraphicsPipelineResourceParams {
    std::string identifier;
    std::string layout_identifier;
    std::vector<ShaderStage> shader_stages;
    std::vector<vk::Format> color_attachment_formats;
    vk::Format depth_format;
    vk::SampleCountFlagBits rasterization_sample_count = vk::SampleCountFlagBits::e1;
    vk::VertexInputRate vertex_input_rate = vk::VertexInputRate::eVertex;
    vk::DeviceSize vertex_stride;
    std::vector<vk::VertexInputAttributeDescription> vertex_descriptions;
    vk::PrimitiveTopology topology = vk::PrimitiveTopology::eTriangleList;
    vk::FrontFace front_face = vk::FrontFace::eCounterClockwise;
    vk::CullModeFlags cull_mode = vk::CullModeFlagBits::eBack;
    vk::PolygonMode polygon_mode = vk::PolygonMode::eFill;
    bool enable_depth_test = false;
    bool enable_depth_write = false;
};

struct ComputePipelineResourceParams {
    std::string identifier;
    std::string layout_identifier;
    ShaderStage shader_stage;
};

struct BufferResourceParams {
    std::string identifier;
    vk::DeviceSize size;
    vk::BufferUsageFlags usage;
    vk::MemoryPropertyFlags memory_properties;
};

struct ImageResourceParams {
    std::string identifier;
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

struct SamplerResourceParams {
    std::string identifier;
    vk::SamplerAddressMode address_mode;
    vk::Filter filter = vk::Filter::eNearest;
    float mip_lod_bias = 0.0f;
    float min_lod = 0.0f;
    float max_lod = 0.0f;
    vk::SamplerMipmapMode mip_map_mode = vk::SamplerMipmapMode::eNearest;
    bool enable_anisotropy = false;
    float max_anisotropy = 1.0f;
};

struct RenderResourcesParams {
    Device& device;
    MemoryAllocator& memory_allocator;
};

class RenderResources {
  public:
    // No copy and no move
    RenderResources(const RenderResources&) = delete;
    RenderResources& operator=(const RenderResources&) = delete;
    RenderResources(RenderResources&&) = delete;
    RenderResources& operator=(RenderResources&&) = delete;

    RenderResources(const RenderResourcesParams& params);

    ~RenderResources();

    DescriptorLayout&
    create_descriptor_layout(const DescriptorLayoutResourceParams& descriptor_layout_params);

    PipelineLayout&
    create_pipeline_layout(const PipelineLayoutResourceParams& pipeline_layout_params);

    Pipeline& create_graphics_pipeline(const GraphicsPipelineResourceParams& pipeline_params);

    Pipeline& create_compute_pipeline(const ComputePipelineResourceParams& pipeline_params);

    Buffer& create_buffer(const BufferResourceParams& buffer_params);

    Image& create_image(const ImageResourceParams& image_params);

    Sampler& create_sampler(const SamplerResourceParams& sampler_params);

    MemoryAllocator& get_memory_allocator();

    Buffer& get_buffer(const std::string& identifier);

    Image& get_image(const std::string& identifier);

    Sampler& get_sampler(const std::string& identifier);

    Pipeline& get_pipeline(const std::string& identifier);

    void remove_descriptor_layout(const std::string& identifier);

    void remove_pipeline_layout(const std::string& identifier);

    void remove_pipeline(const std::string& identifier);

    void remove_buffer(const std::string& identifier);

    void remove_image(const std::string& identifier);

    void remove_sampler(const std::string& identifier);

    void reset();

  private:
    Device& device;
    MemoryAllocator& memory_allocator;

    std::map<std::string, DescriptorLayoutPtr> descriptor_layouts;
    std::map<std::string, PipelineLayoutPtr> pipeline_layouts;
    std::map<std::string, PipelinePtr> pipelines;

    std::map<std::string, BufferPtr> buffers;
    std::map<std::string, ImagePtr> images;
    std::map<std::string, SamplerPtr> samplers;
};

using RenderResourcesPtr = std::unique_ptr<RenderResources>;

inline RenderResourcesPtr render_resources_ptr(const RenderResourcesParams& params) {
    return std::make_unique<RenderResources>(params);
}

} // namespace vkal
