#pragma once

#include "descriptor_set.hpp"
#include "render_resources.hpp"
#include "surface.hpp"

#include <gtest/gtest_prod.h>

#include <memory>
#include <unordered_map>
#include <vector>

namespace vkal {

struct ResourceBarrier {
    vk::ImageLayout layout;
    vk::AccessFlags2 access;
    vk::PipelineStageFlags2 stage;
};

struct RenderAttachmentParams {
    enum Type {
        COLOR,
        DEPTH,
    };

    std::string resolve_image;
    std::string image;
    vk::ResolveModeFlags resolve_mode;
    vk::ClearValue clear_value;
    vk::AttachmentLoadOp load_op;
    vk::AttachmentStoreOp store_op;
};

struct ResourceDescription {
    enum Type {
        BUFFER,
        IMAGE,
    };

    Type type;
    std::string identifier;
    std::optional<ResourceBarrier> barrier;
    bool write_set = false;
    uint32_t set;
    uint32_t binding;
    vk::DescriptorType descriptor_type;
};

struct RenderPassParams {
    std::string identifier;
    bool is_root = false;

    std::vector<ResourceDescription> resources;
    std::vector<RenderAttachmentParams> render_attachments;
    std::vector<std::string> samplers;

    std::optional<std::string> pipeline;

    std::function<void(vk::Viewport&, vk::Rect2D&)> pre_render_callback;
    std::function<void(vk::CommandBuffer,
                       const std::unordered_map<std::string, std::reference_wrapper<Buffer>>&,
                       const std::unordered_map<std::string, std::reference_wrapper<Image>>&,
                       std::optional<std::reference_wrapper<Pipeline>>, DescriptorSet&)>
        render_callback;
};

struct RenderGraphParams {
    Device& vkal_device;
    Surface& vkal_surface;
    RenderResources& render_resources;
    Descriptor& vkal_descriptor;
};

struct GraphNode {
    size_t pass_index;
    std::vector<size_t> ins;
    std::vector<size_t> outs;
};

struct RenderPass {
    std::optional<std::reference_wrapper<Pipeline>> pipeline;
    DescriptorSetPtr descriptor_set;

    std::unordered_map<std::string, std::reference_wrapper<Buffer>> buffer_resources;
    std::unordered_map<std::string, std::reference_wrapper<Image>> image_resources;
    std::unordered_map<std::string, std::reference_wrapper<Sampler>> sampler_resources;

    std::vector<vk::RenderingAttachmentInfo> render_attachments;
    std::vector<std::reference_wrapper<Image>> attachment_images;
    vk::RenderingAttachmentInfo swapchain_attachment;
    vk::RenderingInfo rendering_info;

    std::vector<vk::BufferMemoryBarrier2> buffer_barriers;
    std::vector<std::reference_wrapper<Buffer>> buffer_barriers_refs;
    std::vector<vk::ImageMemoryBarrier2> image_barriers;
    std::vector<std::reference_wrapper<Image>> image_barriers_refs;
    vk::ImageMemoryBarrier2 swapchain_barrier;

    std::optional<vk::DependencyInfo> dependency_info;
    std::optional<vk::DependencyInfo> swapchain_depedency_info;

    vk::Viewport viewport;
    vk::Rect2D scissor;

    std::function<void(vk::Viewport&, vk::Rect2D&)> pre_render_callback;
    std::function<void(vk::CommandBuffer,
                       const std::unordered_map<std::string, std::reference_wrapper<Buffer>>&,
                       const std::unordered_map<std::string, std::reference_wrapper<Image>>&,
                       std::optional<std::reference_wrapper<Pipeline>>, DescriptorSet&)>
        render_callback;
};

class RenderGraph {
  public:
    // No copy and mo move
    RenderGraph(const RenderGraph&) = delete;
    RenderGraph& operator=(const RenderGraph&) = delete;
    RenderGraph(RenderGraph&&) = delete;
    RenderGraph& operator=(RenderGraph&&) = delete;

    explicit RenderGraph(const RenderGraphParams& params);

    ~RenderGraph();

    void add_pass(const RenderPassParams& pass_params);

    void compile();

    void sync();

    void execute(uint32_t swapchain_index, vk::Queue queue, vk::CommandBuffer command,
                 vk::Semaphore semaphore);

    vk::Semaphore get_semaphore();

    void reset_swapchain_images();

  private:
    void generate_passes();

    Device& vkal_device;
    Surface& vkal_surface;
    RenderResources& render_resources;
    Descriptor& vkal_descriptor;

    std::vector<std::reference_wrapper<Image>> swapchain_images;

    std::vector<RenderPassParams> pass_params;
    std::vector<size_t> pass_order;
    std::vector<std::unique_ptr<RenderPass>> passes;

    vk::CommandBufferSubmitInfo command_submit_info;
    vk::SemaphoreSubmitInfo wait_semaphore_info;
    vk::SemaphoreSubmitInfo signal_semaphore_info;

    vk::SubmitInfo2 submit_info;
    vk::Semaphore semaphore;
    vk::Fence fence;

    FRIEND_TEST(RenderGraphTest, GraphGenerationTest);
    FRIEND_TEST(RenderGraphTest, PruningTest);
    FRIEND_TEST(RenderGraphTest, ToplogySortTest);
};

using RenderGraphPtr = std::unique_ptr<RenderGraph>;

inline RenderGraphPtr render_graph_ptr(const RenderGraphParams& params) {
    return std::make_unique<RenderGraph>(params);
}

} // namespace vkal
