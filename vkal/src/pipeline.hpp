#pragma once

#include "device.hpp"
#include "pipeline_layout.hpp"

#include <filesystem>

#include <memory>

namespace vkal {

struct ShaderStage {
    std::filesystem::path file_path;
    vk::ShaderStageFlagBits stage;
};

struct GraphicsPipelineParams {
    Device& device;
    PipelineLayout& layout;
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

struct ComputePipelineParams {
    Device& device;
    PipelineLayout& layout;
    ShaderStage shader_stage;
};

class Pipeline {
  public:
    // No copy and mo move
    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;
    Pipeline(Pipeline&&) = delete;
    Pipeline& operator=(Pipeline&&) = delete;

    explicit Pipeline(const GraphicsPipelineParams& params);

    explicit Pipeline(const ComputePipelineParams& params);

    ~Pipeline();

    vk::Pipeline get();

    PipelineLayout& get_layout();

    vk::PipelineBindPoint get_bind_point();

  private:
    Device& device;
    PipelineLayout& layout;
    vk::PipelineBindPoint bind_point;
    vk::Pipeline vk_pipeline;
};

using PipelinePtr = std::unique_ptr<Pipeline>;

inline PipelinePtr graphics_pipeline_ptr(const GraphicsPipelineParams& params) {
    return std::make_unique<Pipeline>(params);
}

inline PipelinePtr compute_pipeline_ptr(const ComputePipelineParams& params) {
    return std::make_unique<Pipeline>(params);
}

} // namespace vkal
