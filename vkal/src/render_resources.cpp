#include "render_resources.hpp"

namespace vkal {

///////////////////////////////////////////////////////////
RenderResources::RenderResources(const RenderResourcesParams& params)
    : vkal_device(params.vkal_device), memory_allocator(params.memory_allocator) {
}

///////////////////////////////////////////////////////////
RenderResources::~RenderResources() {
}

///////////////////////////////////////////////////////////
DescriptorLayout& RenderResources::create_descriptor_layout(
    const DescriptorLayoutResourceParams& descriptor_layout_params) {
    DescriptorLayoutPtr descriptor_layout = descriptor_layout_ptr(
        DescriptorLayoutParams{.vkal_device = this->vkal_device,
                               .bindings = descriptor_layout_params.bindings,
                               .binding_flags = descriptor_layout_params.binding_flags});

    this->descriptor_layouts.insert_or_assign(descriptor_layout_params.identifier,
                                              std::move(descriptor_layout));
    return *this->descriptor_layouts.at(descriptor_layout_params.identifier);
}

///////////////////////////////////////////////////////////
PipelineLayout& RenderResources::create_pipeline_layout(
    const PipelineLayoutResourceParams& pipeline_layout_params) {
    std::vector<std::reference_wrapper<DescriptorLayout>> descriptor_layouts;
    for (const std::string& identifier : pipeline_layout_params.descriptor_layout_identifiers) {
        descriptor_layouts.push_back(*this->descriptor_layouts.at(identifier));
    }
    PipelineLayoutPtr pipeline_layout = pipeline_layout_ptr(PipelineLayoutParams{
        .vkal_device = this->vkal_device,
        .vkal_descriptor_layouts = descriptor_layouts,
        .push_constants = pipeline_layout_params.push_constants,
    });

    this->pipeline_layouts.insert_or_assign(pipeline_layout_params.identifier,
                                            std::move(pipeline_layout));
    return *this->pipeline_layouts.at(pipeline_layout_params.identifier);
}

///////////////////////////////////////////////////////////
Pipeline&
RenderResources::create_graphics_pipeline(const GraphicsPipelineResourceParams& pipeline_params) {
    PipelinePtr pipeline = graphics_pipeline_ptr(GraphicsPipelineParams{
        .vkal_device = this->vkal_device,
        .vkal_layout = *this->pipeline_layouts.at(pipeline_params.layout_identifier),
        .shader_stages = pipeline_params.shader_stages,
        .color_attachment_formats = pipeline_params.color_attachment_formats,
        .rasterization_sample_count = pipeline_params.rasterization_sample_count,
        .vertex_input_rate = pipeline_params.vertex_input_rate,
        .vertex_stride = pipeline_params.vertex_stride,
        .vertex_descriptions = pipeline_params.vertex_descriptions,
        .topology = pipeline_params.topology,
    });
    this->pipelines.insert_or_assign(pipeline_params.identifier, std::move(pipeline));
    return *this->pipelines.at(pipeline_params.identifier);
}

///////////////////////////////////////////////////////////
Pipeline&
RenderResources::create_compute_pipeline(const ComputePipelineResourceParams& pipeline_params) {
    PipelinePtr pipeline = compute_pipeline_ptr(ComputePipelineParams{
        .vkal_device = this->vkal_device,
        .vkal_layout = *this->pipeline_layouts.at(pipeline_params.identifier),
        .shader_stage = pipeline_params.shader_stage,
    });
    this->pipelines.insert_or_assign(pipeline_params.identifier, std::move(pipeline));
    return *this->pipelines.at(pipeline_params.identifier);
}

///////////////////////////////////////////////////////////
Buffer& RenderResources::create_buffer(const BufferResourceParams& buffer_params) {
    BufferPtr buffer = buffer_ptr(BufferParams{
        .vkal_device = this->vkal_device,
        .memory_allocator = this->memory_allocator,
        .size = buffer_params.size,
        .usage = buffer_params.usage,
        .memory_properties = buffer_params.memory_properties,
    });

    this->buffers.insert_or_assign(buffer_params.identifier, std::move(buffer));
    return *this->buffers.at(buffer_params.identifier);
}

///////////////////////////////////////////////////////////
Image& RenderResources::create_image(const ImageResourceParams& image_params) {
    ImagePtr image = image_ptr(ImageParams{
        .vkal_device = this->vkal_device,
        .memory_allocator = this->memory_allocator,
        .type = image_params.type,
        .view_type = image_params.view_type,
        .extent = image_params.extent,
        .format = image_params.format,
        .sample_count = image_params.sample_count,
        .aspects = image_params.aspects,
        .mip_levels = image_params.mip_levels,
        .usage = image_params.usage,
        .memory_properties = image_params.memory_properties,
    });

    this->images.insert_or_assign(image_params.identifier, std::move(image));
    return *this->images.at(image_params.identifier);
}

///////////////////////////////////////////////////////////
Sampler& RenderResources::create_sampler(const SamplerResourceParams& sampler_params) {
    SamplerPtr sampler = sampler_ptr(SamplerParams{
        .vkal_device = this->vkal_device,
        .address_mode = sampler_params.address_mode,
        .filter = sampler_params.filter,
        .mip_lod_bias = sampler_params.mip_lod_bias,
        .min_lod = sampler_params.min_lod,
        .max_lod = sampler_params.max_lod,
        .mip_map_mode = sampler_params.mip_map_mode,
        .enable_anisotropy = sampler_params.enable_anisotropy,
        .max_anisotropy = sampler_params.max_anisotropy,
    });

    this->samplers.insert_or_assign(sampler_params.identifier, std::move(sampler));
    return *this->samplers.at(sampler_params.identifier);
}

///////////////////////////////////////////////////////////
Buffer& RenderResources::get_buffer(const std::string& identifier) {
    return *this->buffers.at(identifier);
}

///////////////////////////////////////////////////////////
Image& RenderResources::get_image(const std::string& identifier) {
    return *this->images.at(identifier);
}

///////////////////////////////////////////////////////////
Sampler& RenderResources::get_sampler(const std::string& identifier) {
    return *this->samplers.at(identifier);
}

///////////////////////////////////////////////////////////
Pipeline& RenderResources::get_pipeline(const std::string& identifier) {
    return *this->pipelines.at(identifier);
}

} // namespace vkal
