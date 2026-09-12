#include "../vkal/src/render_graph.cpp"

#include <gtest/gtest.h>

namespace vkal {

TEST(RenderGraphTest, GraphGenerationTest) {
    std::vector<RenderPassDescription> pass_params;
    /*
     * a --> d --> c
     * a --> b --> c
     */

    pass_params.push_back(RenderPassDescription{
        .identifier = "pass a",
        .buffer_resources =
            {
                BufferResourceDescription{
                    .identifier = "A-B&D",
                    .barrier = ResourceBarrier{.access = vk::AccessFlagBits2::eShaderWrite},
                },
            },
    });

    pass_params.push_back(RenderPassDescription{
        .identifier = "pass b",
        .buffer_resources =
            {
                BufferResourceDescription{
                    .identifier = "A-B&D",
                    .barrier = ResourceBarrier{.access = vk::AccessFlagBits2::eShaderRead},
                },
                BufferResourceDescription{
                    .identifier = "B-C",
                    .barrier = ResourceBarrier{.access = vk::AccessFlagBits2::eShaderWrite},
                },
            },
    });

    pass_params.push_back(RenderPassDescription{
        .identifier = "pass c",
        .buffer_resources = {BufferResourceDescription{
                                 .identifier = "B-C",
                                 .barrier =
                                     ResourceBarrier{.access = vk::AccessFlagBits2::eShaderRead},
                             },
                             BufferResourceDescription{
                                 .identifier = "D-C",
                                 .barrier =
                                     ResourceBarrier{.access = vk::AccessFlagBits2::eShaderRead},
                             }},
    });

    pass_params.push_back(RenderPassDescription{
        .identifier = "pass d",
        .buffer_resources =
            {
                BufferResourceDescription{
                    .identifier = "A-B&D",
                    .barrier = ResourceBarrier{.access = vk::AccessFlagBits2::eShaderRead},
                },
                BufferResourceDescription{
                    .identifier = "D-C",
                    .barrier = ResourceBarrier{.access = vk::AccessFlagBits2::eShaderWrite},
                },
            },
    });

    std::vector<GraphNode> nodes = generate_graph(pass_params);

    ASSERT_EQ(nodes.size(), 4);

    GraphNode& pass_a = nodes[0];
    GraphNode& pass_b = nodes[1];
    GraphNode& pass_c = nodes[2];
    GraphNode& pass_d = nodes[3];

    ASSERT_EQ(pass_a.ins.size(), 0);
    ASSERT_EQ(pass_a.outs.size(), 2);
    ASSERT_EQ(pass_a.outs[0], 1);
    ASSERT_EQ(pass_a.outs[1], 3);

    ASSERT_EQ(pass_b.ins.size(), 1);
    ASSERT_EQ(pass_b.outs.size(), 1);
    ASSERT_EQ(pass_b.ins[0], 0);
    ASSERT_EQ(pass_b.outs[0], 2);

    ASSERT_EQ(pass_c.ins.size(), 2);
    ASSERT_EQ(pass_c.outs.size(), 0);
    ASSERT_EQ(pass_c.ins[0], 1);

    ASSERT_EQ(pass_d.ins.size(), 1);
    ASSERT_EQ(pass_d.outs.size(), 1);
    ASSERT_EQ(pass_d.ins[0], 0);
    ASSERT_EQ(pass_d.outs[0], 2);
}

TEST(RenderGraphTest, PruningTest) {
    std::vector<RenderPassDescription> pass_params;

    pass_params.push_back(RenderPassDescription{
        .identifier = "pass a",
        .image_resources =
            {
                ImageResourceDescription{
                    .identifier = "A-B&C",
                    .barrier =
                        ResourceBarrier{.access = vk::AccessFlagBits2::eColorAttachmentWrite},
                },
            },
    });

    pass_params.push_back(RenderPassDescription{
        .identifier = "pass b",
        .buffer_resources =
            {
                BufferResourceDescription{
                    .identifier = "B-r",
                    .barrier = ResourceBarrier{.access = vk::AccessFlagBits2::eShaderWrite},
                },
            },
        .image_resources =
            {
                ImageResourceDescription{
                    .identifier = "A-B&C",
                    .barrier = ResourceBarrier{.access = vk::AccessFlagBits2::eShaderSampledRead},
                },
            },
    });

    pass_params.push_back(RenderPassDescription{
        .identifier = "pass c",
        .buffer_resources =
            {
                BufferResourceDescription{
                    .identifier = "C-D&r",
                    .barrier = ResourceBarrier{.access = vk::AccessFlagBits2::eShaderWrite},
                },
            },
        .image_resources =
            {
                ImageResourceDescription{
                    .identifier = "A-B&C",
                    .barrier = ResourceBarrier{.access = vk::AccessFlagBits2::eShaderSampledRead},
                },
            },
    });

    pass_params.push_back(RenderPassDescription{
        .identifier = "pass d",
        .buffer_resources =
            {
                BufferResourceDescription{
                    .identifier = "C-D&r",
                    .barrier = ResourceBarrier{.access = vk::AccessFlagBits2::eShaderRead},
                },
            },
    });

    pass_params.push_back(RenderPassDescription{
        .identifier = "root pass",
        .buffer_resources =
            {
                BufferResourceDescription{
                    .identifier = "B-r",
                    .barrier = ResourceBarrier{.access = vk::AccessFlagBits2::eShaderRead},
                },
                BufferResourceDescription{
                    .identifier = "C-D&r",
                    .barrier = ResourceBarrier{.access = vk::AccessFlagBits2::eShaderRead},
                },
            },
        .render_attachments =
            {
                vkal::RenderAttachmentDescription{
                    .type = vkal::RenderAttachmentType::SWAPCHAIN,
                    .identifier = "swapchain attachment",
                    .clear_value = vk::ClearValue(
                        vk::ClearColorValue(std::array<float, 4>{1.0f, 1.0f, 1.0f, 1.0f})),
                    .load_op = vk::AttachmentLoadOp::eClear,
                    .store_op = vk::AttachmentStoreOp::eStore,
                },
            },
    });

    std::vector<GraphNode> nodes = generate_graph(pass_params);
    std::vector<std::pair<GraphNode, size_t>> post_pruned = prune(pass_params, nodes);

    auto extract_entry = [&post_pruned,
                          &pass_params](const std::string& key) -> std::optional<GraphNode> {
        for (const auto& entry : post_pruned) {
            if (pass_params[entry.first.pass_index].identifier == key) {
                return entry.first;
            }
        }

        return std::nullopt;
    };
    ASSERT_EQ(post_pruned.size(), 4);

    auto pass_a_opt = extract_entry("pass a");
    auto pass_b_opt = extract_entry("pass b");
    auto pass_c_opt = extract_entry("pass c");
    auto pass_d_opt = extract_entry("pass d");
    auto root_pass_opt = extract_entry("root pass");

    ASSERT_TRUE(pass_a_opt.has_value());
    ASSERT_TRUE(pass_b_opt.has_value());
    ASSERT_TRUE(pass_c_opt.has_value());
    ASSERT_FALSE(pass_d_opt.has_value());
    ASSERT_TRUE(root_pass_opt.has_value());

    GraphNode pass_a = pass_a_opt.value();
    GraphNode pass_b = pass_b_opt.value();
    GraphNode pass_c = pass_c_opt.value();
    GraphNode root_pass = root_pass_opt.value();

    auto containing = [](size_t index, const std::vector<size_t>& indices) -> bool {
        for (size_t current_index : indices) {
            if (current_index == index) {
                return true;
            }
        }
        return false;
    };

    ASSERT_EQ(pass_a.ins.size(), 0);
    ASSERT_TRUE(containing(pass_b.pass_index, pass_a.outs));
    ASSERT_TRUE(containing(pass_c.pass_index, pass_a.outs));

    ASSERT_EQ(pass_b.ins.size(), 1);
    ASSERT_TRUE(containing(pass_a.pass_index, pass_b.ins));
    ASSERT_TRUE(containing(root_pass.pass_index, pass_b.outs));

    ASSERT_EQ(pass_c.ins.size(), 1);
    ASSERT_TRUE(containing(pass_a.pass_index, pass_c.ins));
    ASSERT_TRUE(containing(root_pass.pass_index, pass_c.outs));

    ASSERT_EQ(root_pass.ins.size(), 2);
    ASSERT_TRUE(containing(pass_b.pass_index, root_pass.ins));
    ASSERT_TRUE(containing(pass_c.pass_index, root_pass.ins));
}

TEST(RenderGraphTest, TopologySortTest) {
    std::vector<RenderPassDescription> pass_params;

    pass_params.push_back(RenderPassDescription{
        .identifier = "root pass",
        .buffer_resources =
            {
                BufferResourceDescription{
                    .identifier = "B-r",
                    .barrier = ResourceBarrier{.access = vk::AccessFlagBits2::eShaderRead},
                },
                BufferResourceDescription{
                    .identifier = "C-D&r",
                    .barrier = ResourceBarrier{.access = vk::AccessFlagBits2::eShaderRead},
                },
            },
        .render_attachments =
            {
                vkal::RenderAttachmentDescription{
                    .type = vkal::RenderAttachmentType::SWAPCHAIN,
                    .identifier = "swapchain attachment",
                    .clear_value = vk::ClearValue(
                        vk::ClearColorValue(std::array<float, 4>{1.0f, 1.0f, 1.0f, 1.0f})),
                    .load_op = vk::AttachmentLoadOp::eClear,
                    .store_op = vk::AttachmentStoreOp::eStore,
                },
            },
    });

    pass_params.push_back(RenderPassDescription{
        .identifier = "pass a",
        .buffer_resources =
            {
                BufferResourceDescription{
                    .identifier = "A-B&C",
                    .barrier = ResourceBarrier{.access = vk::AccessFlagBits2::eShaderWrite},
                },
            },
    });

    pass_params.push_back(RenderPassDescription{
        .identifier = "pass d",
        .buffer_resources =
            {
                BufferResourceDescription{
                    .identifier = "C-D&r",
                    .barrier = ResourceBarrier{.access = vk::AccessFlagBits2::eShaderRead},
                },
            },
    });

    pass_params.push_back(RenderPassDescription{
        .identifier = "pass b",
        .buffer_resources =
            {
                BufferResourceDescription{
                    .identifier = "A-B&C",
                    .barrier = ResourceBarrier{.access = vk::AccessFlagBits2::eShaderRead},
                },
                BufferResourceDescription{
                    .identifier = "B-r",
                    .barrier = ResourceBarrier{.access = vk::AccessFlagBits2::eShaderWrite},
                },
            },
    });

    pass_params.push_back(RenderPassDescription{
        .identifier = "pass c",
        .buffer_resources =
            {
                BufferResourceDescription{
                    .identifier = "A-B&C",
                    .barrier = ResourceBarrier{.access = vk::AccessFlagBits2::eShaderRead},
                },
                BufferResourceDescription{
                    .identifier = "C-D&r",
                    .barrier = ResourceBarrier{.access = vk::AccessFlagBits2::eShaderWrite},
                },
            },
    });

    std::vector<GraphNode> nodes = generate_graph(pass_params);
    std::vector<std::pair<GraphNode, size_t>> post_pruned = prune(pass_params, nodes);
    std::vector<size_t> pass_order = topology_sort(post_pruned);

    ASSERT_TRUE(post_pruned.empty());

    ASSERT_EQ(pass_params[pass_order[0]].identifier, "pass a");
    ASSERT_EQ(pass_params[pass_order[1]].identifier, "pass c");
    ASSERT_EQ(pass_params[pass_order[2]].identifier, "pass b");
    ASSERT_EQ(pass_params[pass_order[3]].identifier, "root pass");
}

} // namespace vkal
