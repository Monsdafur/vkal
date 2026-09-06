#pragma once

#include "descriptor_set.hpp"
#include "render_pass.hpp"
#include "render_resources.hpp"
#include "surface.hpp"

#include <gtest/gtest_prod.h>

#include <memory>
#include <unordered_map>
#include <vector>

namespace vkal {

struct ResourceBarrier {
    bool preserve = true;
    vk::ImageLayout layout;
    vk::AccessFlags2 access;
    vk::PipelineStageFlags2 stage;
};

struct RenderAttachmentParams {
    enum Type {
        COLOR,
        DEPTH,
    };

    std::string identifier;
    std::string image;
    std::optional<std::string> resolve_image;
    vk::ResolveModeFlagBits resolve_mode;
    vk::ClearValue clear_value;
    vk::AttachmentLoadOp load_op;
    vk::AttachmentStoreOp store_op;
};

struct ResourceDescriptor {
    vk::DescriptorType type;
    uint32_t set;
    uint32_t binding;
};

struct BufferResourceDescription {
    std::string identifier;
    std::optional<ResourceBarrier> barrier;
    std::optional<ResourceDescriptor> resource_descriptor = std::nullopt;
};

struct ImageResourceDescription {
    std::string identifier;
    ResourceBarrier barrier; // A barrier is a must for an image resource
    std::optional<ResourceDescriptor> resource_descriptor = std::nullopt;
};

struct SamplerResourceDescription {
    std::string identifier;
    uint32_t set;
    uint32_t binding;
};

struct RenderPassParams {
    std::string identifier;
    bool is_root = false;

    std::vector<BufferResourceDescription> buffer_resources;
    std::vector<ImageResourceDescription> image_resources;
    std::vector<RenderAttachmentParams> render_attachments;
    std::vector<SamplerResourceDescription> sampler_resources;

    std::optional<std::string> pipeline;

    std::unique_ptr<RenderPass> pass;
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

struct BufferBarrierBuilder {
    // Identifier for later rebind operation
    std::string identifier;
    bool preserve;
    std::reference_wrapper<Buffer> buffer;
    vk::AccessFlags2 access;
    vk::PipelineStageFlags2 stage;
};

struct ImageBarrierBuilder {
    // Identifier for later rebind operation
    std::string identifier;
    bool preserve;
    std::reference_wrapper<Image> image;
    vk::AccessFlags2 access;
    vk::PipelineStageFlags2 stage;
    vk::ImageLayout layout;
};

struct RenderAttachmentBuilder {
    // Identifiers for later rebind operation
    std::string image_identifier;
    std::optional<std::string> resolve_image_identifier;
    std::reference_wrapper<Image> image;
    std::optional<std::reference_wrapper<Image>> resolve_image;
    vk::ImageLayout layout;
    vk::ImageLayout resolve_layout;
    vk::ResolveModeFlagBits resolve_mode;
    vk::ClearValue clear_value;
    vk::AttachmentLoadOp load_op;
    vk::AttachmentStoreOp store_op;
};

struct RenderPassData {
    bool is_root;

    std::optional<std::reference_wrapper<Pipeline>> pipeline;
    DescriptorSetPtr descriptor_set;

    std::unordered_map<std::string, std::reference_wrapper<Buffer>> buffer_resources;
    std::unordered_map<std::string, std::reference_wrapper<Image>> image_resources;
    std::unordered_map<std::string, std::reference_wrapper<Sampler>> sampler_resources;

    std::vector<RenderAttachmentBuilder> render_attachment_builders;
    std::vector<vk::RenderingAttachmentInfo> render_attachment_infos;

    std::vector<BufferBarrierBuilder> buffer_barrier_builders;
    std::vector<vk::BufferMemoryBarrier2> buffer_barriers;

    std::vector<ImageBarrierBuilder> image_barrier_builders;
    std::vector<vk::ImageMemoryBarrier2> image_barriers;

    std::unique_ptr<RenderPass> pass;
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

    void add_pass(RenderPassParams pass_params);

    void compile();

    void sync();

    void execute(uint32_t swapchain_index, vk::Queue queue, vk::CommandBuffer command,
                 vk::Semaphore semaphore);

    void rebind_resources();

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
    std::vector<std::unique_ptr<RenderPassData>> pass_data;

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
