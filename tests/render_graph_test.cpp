#include "../vkal/src/render_graph.cpp"

#include <gtest/gtest.h>

namespace vkal {

TEST(RenderGraphTest, GraphGenerationTest) {
    RenderGraph render_graph(RenderGraphParams{});

    /*
     * a --> d --> c
     * a --> b --> c
     */

    render_graph.add_pass(RenderPassParams{
        .identifier = "pass a",
        .read_resources = {},
        .write_resources =
            {
                ResourceDescription{.identifier = "A-B&D"},
            },
    });

    render_graph.add_pass(RenderPassParams{
        .identifier = "pass b",
        .read_resources =
            {
                ResourceDescription{.identifier = "A-B&D"},
            },
        .write_resources =
            {
                ResourceDescription{.identifier = "B-C"},
            },
    });

    render_graph.add_pass(RenderPassParams{
        .identifier = "pass c",
        .read_resources = {ResourceDescription{.identifier = "B-C"},
                           ResourceDescription{.identifier = "D-C"}},
        .write_resources = {},
    });

    render_graph.add_pass(RenderPassParams{
        .identifier = "pass d",
        .read_resources =
            {
                ResourceDescription{.identifier = "A-B&D"},
            },
        .write_resources =
            {
                ResourceDescription{.identifier = "D-C"},
            },
    });

    std::vector<RenderGraph::Node> nodes = render_graph.generate_graph();

    ASSERT_EQ(nodes.size(), 4);

    RenderGraph::Node& pass_a = nodes[0];
    RenderGraph::Node& pass_b = nodes[1];
    RenderGraph::Node& pass_c = nodes[2];
    RenderGraph::Node& pass_d = nodes[3];

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
    RenderGraph render_graph(RenderGraphParams{});

    render_graph.add_pass(RenderPassParams{
        .identifier = "pass a",
        .read_resources = {},
        .write_resources =
            {
                ResourceDescription{.identifier = "A-B&C"},
            },
    });

    render_graph.add_pass(RenderPassParams{
        .identifier = "pass b",
        .read_resources =
            {
                ResourceDescription{.identifier = "A-B&C"},
            },
        .write_resources =
            {
                ResourceDescription{.identifier = "B-r"},
            },
    });

    render_graph.add_pass(RenderPassParams{
        .identifier = "pass c",
        .read_resources =
            {
                ResourceDescription{.identifier = "A-B&C"},
            },
        .write_resources =
            {
                ResourceDescription{.identifier = "C-D&r"},
            },
    });

    render_graph.add_pass(RenderPassParams{
        .identifier = "pass d",
        .read_resources =
            {
                ResourceDescription{.identifier = "C-D&r"},
            },
        .write_resources = {},
    });

    render_graph.add_pass(RenderPassParams{
        .identifier = "root pass",
        .is_root = true,
        .read_resources =
            {
                ResourceDescription{.identifier = "B-r"},
                ResourceDescription{.identifier = "C-D&r"},
            },
        .write_resources = {},
    });

    std::vector<RenderGraph::Node> nodes = render_graph.generate_graph();
    std::vector<std::pair<RenderGraph::Node, size_t>> post_pruned = render_graph.prune(nodes);

    auto extract_entry =
        [&post_pruned, &render_graph](const std::string& key) -> std::optional<RenderGraph::Node> {
        for (const auto& entry : post_pruned) {
            if (render_graph.pass_params[entry.first.pass_index].identifier == key) {
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

    RenderGraph::Node pass_a = pass_a_opt.value();
    RenderGraph::Node pass_b = pass_b_opt.value();
    RenderGraph::Node pass_c = pass_c_opt.value();
    RenderGraph::Node root_pass = root_pass_opt.value();

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

TEST(RenderGraphTest, ToplogySortTest) {
    RenderGraph render_graph(RenderGraphParams{});

    render_graph.add_pass(RenderPassParams{
        .identifier = "root pass",
        .is_root = true,
        .read_resources =
            {
                ResourceDescription{.identifier = "B-r"},
                ResourceDescription{.identifier = "C-D&r"},
            },
        .write_resources = {},
    });

    render_graph.add_pass(RenderPassParams{
        .identifier = "pass a",
        .read_resources = {},
        .write_resources =
            {
                ResourceDescription{.identifier = "A-B&C"},
            },
    });

    render_graph.add_pass(RenderPassParams{
        .identifier = "pass d",
        .read_resources =
            {
                ResourceDescription{.identifier = "C-D&r"},
            },
        .write_resources = {},
    });

    render_graph.add_pass(RenderPassParams{
        .identifier = "pass b",
        .read_resources =
            {
                ResourceDescription{.identifier = "A-B&C"},
            },
        .write_resources =
            {
                ResourceDescription{.identifier = "B-r"},
            },
    });

    render_graph.add_pass(RenderPassParams{
        .identifier = "pass c",
        .read_resources =
            {
                ResourceDescription{.identifier = "A-B&C"},
            },
        .write_resources =
            {
                ResourceDescription{.identifier = "C-D&r"},
            },
    });

    std::vector<RenderGraph::Node> nodes = render_graph.generate_graph();
    std::vector<std::pair<RenderGraph::Node, size_t>> post_pruned = render_graph.prune(nodes);
    render_graph.topology_sort(post_pruned);

    ASSERT_TRUE(post_pruned.empty());

    ASSERT_EQ(render_graph.pass_order[0].identifier, "pass a");
    ASSERT_EQ(render_graph.pass_order[1].identifier, "pass c");
    ASSERT_EQ(render_graph.pass_order[2].identifier, "pass b");
    ASSERT_EQ(render_graph.pass_order[3].identifier, "root pass");
}

} // namespace vkal
