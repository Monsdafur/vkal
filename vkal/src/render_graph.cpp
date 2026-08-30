#include "render_graph.hpp"

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
    for (size_t i = 0; i < this->pass_params.size(); ++i) {
        const RenderPassParams& pass = this->pass_params[i];
        Node node;
        node.pass_imdex = i;
        for (size_t j = 0; j < this->pass_params.size(); ++j) {
            if (i == j) {
                continue;
            }
            const RenderPassParams& other_pass = this->pass_params[j];

            // Matching to find any matching resource on both passes
            for (const ResourceDescription& resource : pass.write_resources) {
                if (std::ranges::any_of(other_pass.read_resources,
                                        [&resource](const ResourceDescription& other_resource) {
                                            return resource.identifier == other_resource.identifier;
                                        })) {
                    node.outs.push_back(j);
                }
            }

            for (const ResourceDescription& resource : pass.read_resources) {
                if (std::ranges::any_of(other_pass.write_resources,
                                        [&resource](const ResourceDescription& other_resource) {
                                            return resource.identifier == other_resource.identifier;
                                        })) {
                    node.ins.push_back(j);
                }
            }
        }

        nodes.push_back(node);
    }

    return nodes;
}

std::vector<std::pair<RenderGraph::Node, size_t>>
RenderGraph::prune(const std::vector<Node>& nodes) {
    std::vector<std::pair<RenderGraph::Node, size_t>> dfs;
    std::vector<Node> stack;
    std::set<size_t> visited;

    // Find root node
    for (const Node& node : nodes) {
        if (this->pass_params[node.pass_imdex].is_root) {
            stack.push_back(node);
        }
    }

    // DFS walk backward from the root node will eliminate all dead branches
    while (!stack.empty()) {
        Node node = stack.back();
        stack.pop_back();
        if (visited.contains(node.pass_imdex)) {
            continue;
        }
        visited.emplace(node.pass_imdex);
        dfs.push_back({node, node.ins.size()});
        for (size_t in_index : node.ins) {
            if (!visited.contains(in_index)) {
                stack.push_back(nodes[in_index]);
            }
        }
    }

    std::vector<size_t> keeps;
    for (auto& pair : dfs) {
        Node& node = pair.first;
        // Iterate through all out nodes and remove the ones that were pruned
        keeps.clear();
        for (size_t out_index : node.outs) {
            if (visited.contains(out_index)) {
                keeps.push_back(out_index);
            }
        }
        node.outs = keeps;
    }

    return dfs;
}

void RenderGraph::topology_sort(std::vector<std::pair<Node, size_t>>& nodes) {
    while (!nodes.empty()) {
        size_t i = 0;
        for (auto& entry : nodes) {
            if (entry.second == 0) {
                for (size_t index : entry.first.outs) {
                    for (auto& entry_other : nodes) {
                        if (entry_other.first.pass_imdex == index) {
                            entry_other.second--;
                            break;
                        }
                    }
                }

                this->pass_order.push_back(this->pass_params[entry.first.pass_imdex]);
                nodes.erase(nodes.begin() + i);
                break;
            }
            i++;
        }
    }
}

} // namespace vkal
