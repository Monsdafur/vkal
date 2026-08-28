#include "pipeline.hpp"

#include <fstream>

namespace vkal {

///////////////////////////////////////////////////////////
static std::vector<uint32_t> load_shader_file(const std::filesystem::path& file_path) {
    std::ifstream file(file_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        throw std::runtime_error(
            std::format("Failed to open shader at path {}", file_path.generic_string()));
    }

    // Get the size of the file by reading the pointer position
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);

    // Read data to vector
    std::vector<char> buffer(size);
    if (file.read(buffer.data(), size)) {
        std::vector<uint32_t> data(size / sizeof(uint32_t));
        std::memcpy(data.data(), buffer.data(), size);
        file.close();
        return data;
    } else {
        file.close();
        throw std::runtime_error(
            std::format("Failed to read shader data at path {}", file_path.generic_string()));
    }
}

///////////////////////////////////////////////////////////
static vk::ShaderModule create_shader_module(vk::Device device,
                                             const std::filesystem::path& file_path) {
    std::vector<uint32_t> shader_code = load_shader_file(file_path);

    vk::ShaderModuleCreateInfo shader_module_create_create_info;
    shader_module_create_create_info.setCodeSize(shader_code.size() * sizeof(uint32_t))
        .setCode(shader_code);

    vk::ShaderModule shader_module = device.createShaderModule(shader_module_create_create_info);

    return shader_module;
}

///////////////////////////////////////////////////////////
Pipeline::Pipeline(const GraphicsPipelineParams& params)
    : vkal_device(params.vkal_device), vkal_layout(params.vkal_layout) {
    // Set bind point
    this->bind_point = vk::PipelineBindPoint::eGraphics;

    // Dynamic rendering state
    std::array<vk::DynamicState, 2> dynamic_states = {
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor,
    };
    vk::PipelineDynamicStateCreateInfo pipeline_dynamic_state_create_info;
    pipeline_dynamic_state_create_info.setDynamicStates(dynamic_states);

    // Vertex input
    vk::VertexInputBindingDescription vertex_input_binding_descriptrion;
    vertex_input_binding_descriptrion.setBinding(0)
        .setInputRate(params.vertex_input_rate)
        .setStride(params.vertex_stride);
    vk::PipelineVertexInputStateCreateInfo pipeline_vertex_input_state_create_info;
    pipeline_vertex_input_state_create_info
        .setVertexBindingDescriptions(vertex_input_binding_descriptrion)
        .setVertexAttributeDescriptions(params.vertex_descriptions);

    // Input assembly
    vk::PipelineInputAssemblyStateCreateInfo pipeline_input_assembly_state_create_info;
    pipeline_input_assembly_state_create_info.setTopology(params.topology);

    // Viewport
    vk::PipelineViewportStateCreateInfo pipeline_viewport_state_create_info;
    pipeline_viewport_state_create_info.setViewportCount(1).setScissorCount(1);

    // Rasterizer
    vk::PipelineRasterizationStateCreateInfo pipeline_rasterization_state;
    pipeline_rasterization_state.setDepthClampEnable(false)
        .setRasterizerDiscardEnable(false)
        .setPolygonMode(vk::PolygonMode::eFill)
        .setCullMode(vk::CullModeFlagBits::eNone)
        .setDepthBiasEnable(false)
        .setLineWidth(1.0f);

    // Multisample
    vk::PipelineMultisampleStateCreateInfo pipeline_multisample_state_create_info;
    pipeline_multisample_state_create_info.setRasterizationSamples(vk::SampleCountFlagBits::e1)
        .setSampleShadingEnable(false);

    // Depth stencil
    vk::PipelineDepthStencilStateCreateInfo pipeline_depth_stencil_state;
    pipeline_depth_stencil_state.setDepthTestEnable(false).setDepthWriteEnable(false);

    // Color blend
    vk::PipelineColorBlendAttachmentState color_blend_attachment;
    color_blend_attachment.setBlendEnable(true)
        .setSrcColorBlendFactor(vk::BlendFactor::eSrcAlpha)
        .setDstColorBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
        .setColorBlendOp(vk::BlendOp::eAdd)
        .setSrcAlphaBlendFactor(vk::BlendFactor::eOne)
        .setDstAlphaBlendFactor(vk::BlendFactor::eZero)
        .setAlphaBlendOp(vk::BlendOp::eAdd)
        .setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                           vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);
    vk::PipelineColorBlendStateCreateInfo pipeline_color_blend_state_create_info;
    pipeline_color_blend_state_create_info.setLogicOpEnable(false).setAttachments(
        color_blend_attachment);

    // Pipeline rendering
    vk::PipelineRenderingCreateInfo pipeline_rendering_create_info;
    pipeline_rendering_create_info.setColorAttachmentFormats(params.color_attachment_formats);

    // Shader stage
    std::vector<vk::PipelineShaderStageCreateInfo> pipeline_shader_stage_create_infos;
    std::vector<vk::ShaderModule> shader_modules;
    for (ShaderStage shader_stage : params.shader_stages) {
        vk::ShaderModule module =
            create_shader_module(params.vkal_device.get(), shader_stage.file_path);
        vk::PipelineShaderStageCreateInfo pipeline_shader_stage_create_info;
        pipeline_shader_stage_create_info.setStage(shader_stage.stage).setModule(module);

#if defined(SLANG_PRIMARY)
        switch (shader_stage.stage) {
        case vk::ShaderStageFlagBits::eCompute:
            pipeline_shader_stage_create_info.setPName("compMain");
            break;
        case vk::ShaderStageFlagBits::eFragment:
            pipeline_shader_stage_create_info.setPName("fragMain");
            break;
        case vk::ShaderStageFlagBits::eGeometry:
            pipeline_shader_stage_create_info.setPName("geoMain");
            break;
        case vk::ShaderStageFlagBits::eVertex:
            pipeline_shader_stage_create_info.setPName("vertMain");
            break;
        default:
            throw std::runtime_error(std::format("Shader stage {} is currently not supported",
                                                 vk::to_string(shader_stage.stage)));
        }
#elif defined(GLSL_PRIMARY)
        pipeline_shader_stage_create_info.setPName("main");
#else
        throw std::runtime_error("No shading language defined");
#endif

        pipeline_shader_stage_create_infos.push_back(std::move(pipeline_shader_stage_create_info));
        shader_modules.emplace_back(std::move(module));
    }

    // Create pipeline
    vk::GraphicsPipelineCreateInfo pipeline_create_info;
    pipeline_create_info.setPNext(pipeline_rendering_create_info)
        .setStages(pipeline_shader_stage_create_infos)
        .setPVertexInputState(&pipeline_vertex_input_state_create_info)
        .setPInputAssemblyState(&pipeline_input_assembly_state_create_info)
        .setPViewportState(&pipeline_viewport_state_create_info)
        .setPRasterizationState(&pipeline_rasterization_state)
        .setPMultisampleState(&pipeline_multisample_state_create_info)
        .setPDepthStencilState(&pipeline_depth_stencil_state)
        .setPColorBlendState(&pipeline_color_blend_state_create_info)
        .setPDynamicState(&pipeline_dynamic_state_create_info)
        .setLayout(this->vkal_layout.get());

    vk::ResultValue<vk::Pipeline> pipeline_result =
        this->vkal_device.get().createGraphicsPipeline(nullptr, pipeline_create_info);

    if (pipeline_result.result == vk::Result::eSuccess) {
        this->pipeline = pipeline_result.value;
        for (vk::ShaderModule module : shader_modules) {
            this->vkal_device.get().destroyShaderModule(module);
        }
    } else {
        this->pipeline = pipeline_result.value;
        for (vk::ShaderModule module : shader_modules) {
            this->vkal_device.get().destroyShaderModule(module);
        }
        throw std::runtime_error(std::format("Failed to create graphics pipeline due to {}",
                                             vk::to_string(pipeline_result.result)));
    }
}

///////////////////////////////////////////////////////////
Pipeline::Pipeline(const ComputePipelineParams& params)
    : vkal_device(params.vkal_device), vkal_layout(params.vkal_layout) {
    // Set bind point
    this->bind_point = vk::PipelineBindPoint::eCompute;

    // Shader stage
    vk::ShaderModule shader_module =
        create_shader_module(params.vk2d_device.get(), params.shader_stage.file_path);
    vk::PipelineShaderStageCreateInfo pipeline_shader_stage_create_info;
    pipeline_shader_stage_create_info.setStage(params.shader_stage.stage).setModule(shader_module);

#if defined(SLANG_PRIMARY)
    pipeline_shader_stage_create_info.setPName("compMain");
#elif defined(GLSL_PRIMARY)
    pipeline_shader_stage_create_info.setPName("main");
#else
    throw std::runtime_error("No shading language defined");
#endif

    // Create pipeline
    vk::ComputePipelineCreateInfo pipeline_create_info;
    pipeline_create_info.setStage(pipeline_shader_stage_create_info)
        .setLayout(this->vkal_layout.get());

    vk::ResultValue<vk::Pipeline> pipeline_result =
        this->vkal_device.get().createComputePipeline(nullptr, pipeline_create_info);

    if (pipeline_result.result == vk::Result::eSuccess) {
        this->pipeline = pipeline_result.value;
        this->vkal_device.get().destroyShaderModule(shader_module);
    } else {
        this->vkal_device.get().destroyShaderModule(shader_module);
        throw std::runtime_error(std::format("Failed to create graphics pipeline due to {}",
                                             vk::to_string(pipeline_result.result)));
    }
}

///////////////////////////////////////////////////////////
Pipeline::~Pipeline() {
    this->vkal_device.get().destroyPipeline(this->pipeline);
}

///////////////////////////////////////////////////////////
vk::Pipeline Pipeline::get() {
    return this->pipeline;
}

///////////////////////////////////////////////////////////
PipelineLayout& Pipeline::get_layout() {
    return this->vkal_layout;
}

///////////////////////////////////////////////////////////
vk::PipelineBindPoint Pipeline::get_bind_point() {
    return this->bind_point;
}

} // namespace vkal
