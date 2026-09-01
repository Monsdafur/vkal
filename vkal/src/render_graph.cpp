#include "render_graph.hpp"

#include <ranges>
#include <set>

namespace vkal {

///////////////////////////////////////////////////////////
RenderGraph::RenderGraph(const RenderGraphParams& params) {
}

///////////////////////////////////////////////////////////
void RenderGraph::add_pass(const RenderPassParams& pass_params) {
    this->pass_params.push_back(pass_params);
}

///////////////////////////////////////////////////////////
std::vector<RenderGraph::Node> RenderGraph::generate_graph() {
    std::vector<Node> nodes;
    for (const auto& [index, pass] : std::ranges::views::enumerate(this->pass_params)) {
        Node node;
        node.pass_index = index;
        for (const auto& [other_index, other_pass] :
             std::ranges::views::enumerate(this->pass_params)) {
            if (index == other_index) {
                continue;
            }
            // Matching to find any matching resource on both passes
            for (const ResourceDescription& resource : pass.write_resources) {
                if (std::ranges::any_of(other_pass.read_resources,
                                        [&resource](const ResourceDescription& other_resource) {
                                            return resource.identifier == other_resource.identifier;
                                        })) {
                    node.outs.push_back(other_index);
                }
            }

            for (const ResourceDescription& resource : pass.read_resources) {
                if (std::ranges::any_of(other_pass.write_resources,
                                        [&resource](const ResourceDescription& other_resource) {
                                            return resource.identifier == other_resource.identifier;
                                        })) {
                    node.ins.push_back(other_index);
                }
            }
        }

        nodes.push_back(node);
    }

    return nodes;
}

///////////////////////////////////////////////////////////
std::vector<std::pair<RenderGraph::Node, size_t>>
RenderGraph::prune(const std::vector<Node>& nodes) {
    std::vector<std::pair<RenderGraph::Node, size_t>> dfs;
    std::vector<Node> stack;
    std::set<size_t> visited;

    // Find root node
    for (const Node& node : nodes) {
        if (this->pass_params[node.pass_index].is_root) {
            stack.push_back(node);
        }
    }

    // DFS walk backward from the root node will eliminate all dead branches
    while (!stack.empty()) {
        Node node = stack.back();
        stack.pop_back();
        if (visited.contains(node.pass_index)) {
            continue;
        }
        visited.emplace(node.pass_index);
        dfs.push_back({node, node.ins.size()});
        for (size_t in_index : node.ins) {
            if (!visited.contains(in_index)) {
                stack.push_back(nodes[in_index]);
            }
        }
    }
    return dfs;
}

///////////////////////////////////////////////////////////
void RenderGraph::topology_sort(std::vector<std::pair<Node, size_t>>& nodes) {
    while (!nodes.empty()) {
        size_t i = 0;
        for (auto& entry : nodes) {
            if (entry.second == 0) {
                for (size_t index : entry.first.outs) {
                    for (auto& entry_other : nodes) {
                        if (entry_other.first.pass_index == index) {
                            entry_other.second--;
                            break;
                        }
                    }
                }

                this->pass_order.push_back(this->pass_params[entry.first.pass_index]);
                nodes.erase(nodes.begin() + i);
                break;
            }
            i++;
        }
    }
}

} // namespace vkal
