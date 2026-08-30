#pragma once

#include <gtest/gtest_prod.h>
#include <vulkan/vulkan.hpp>

#include <memory>
#include <vector>

namespace vkal {

struct ResourceBarrier {
    vk::AccessFlags2 access;
    vk::PipelineStageFlags2 stage;
};

struct ResourceDescription {
    std::string identifier;
    ResourceBarrier barrier;
};

struct RenderPassParams {
    std::string identifier;
    bool is_root = false;
    std::vector<ResourceDescription> read_resources;
    std::vector<ResourceDescription> write_resources;
};

struct RenderGraphParams {};

class RenderGraph {
  public:
    // No copy and mo move
    RenderGraph(const RenderGraph&) = delete;
    RenderGraph& operator=(const RenderGraph&) = delete;
    RenderGraph(RenderGraph&&) = delete;
    RenderGraph& operator=(RenderGraph&&) = delete;

    explicit RenderGraph(const RenderGraphParams& params);

    ~RenderGraph() = default;

    void add_pass(const RenderPassParams& pass_params);

    void compile();

  private:
    struct Node {
        size_t pass_imdex;
        std::vector<size_t> ins;
        std::vector<size_t> outs;
    };

    std::vector<Node> generate_graph();

    std::vector<std::pair<Node, size_t>> prune(const std::vector<Node>& nodes);

    void topology_sort(std::vector<std::pair<Node, size_t>>& nodes);

    std::vector<RenderPassParams> pass_params;

    std::vector<RenderPassParams> pass_order;

    FRIEND_TEST(RenderGraphTest, GraphGenerationTest);
    FRIEND_TEST(RenderGraphTest, PruningTest);
    FRIEND_TEST(RenderGraphTest, ToplogySortTest);
};

using RenderGraphPtr = std::unique_ptr<RenderGraph>;

inline RenderGraphPtr command_ptr(const RenderGraphParams& params) {
    return std::make_unique<RenderGraph>(params);
}

} // namespace vkal
