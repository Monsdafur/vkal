#include "render_graph.hpp"
#include "common.hpp"

#include <ranges>
#include <set>

namespace vkal {

///////////////////////////////////////////////////////////
void sort_resource_access(const std::vector<ResourceDescription>& resources,
                          std::vector<size_t>& read_resources,
                          std::vector<size_t>& write_resources) {
    for (const auto& [index, resource] : std::ranges::views::enumerate(resources)) {
        if (!resource.barrier.has_value() ||
            static_cast<bool>(resource.barrier->access & VK_ACCESS_READ_FLAGS)) {
            read_resources.push_back(index);
        }
        if (resource.barrier.has_value() &&
            static_cast<bool>(resource.barrier->access & VK_ACCESS_WRITE_FLAGS)) {
            write_resources.push_back(index);
        }
    }
}

///////////////////////////////////////////////////////////
static std::vector<GraphNode> generate_graph(const std::vector<RenderPassParams>& pass_params) {
    std::vector<GraphNode> nodes;
    for (const auto& [index, pass] : std::ranges::views::enumerate(pass_params)) {
        GraphNode node;
        node.pass_index = index;

        std::vector<size_t> read_resources;
        std::vector<size_t> write_resources;
        sort_resource_access(pass.resources, read_resources, write_resources);

        for (const auto& [other_index, other_pass] : std::ranges::views::enumerate(pass_params)) {
            if (index == other_index) {
                continue;
            }
            // Matching to find any matching resource on both passes
            std::vector<size_t> other_read_resources;
            std::vector<size_t> other_write_resources;
            sort_resource_access(other_pass.resources, other_read_resources, other_write_resources);

            for (size_t write_index : write_resources) {
                bool has_dependency = false;
                for (size_t read_index : other_read_resources) {
                    if (pass.resources[write_index].identifier ==
                        other_pass.resources[read_index].identifier) {
                        has_dependency = true;
                        break;
                    }
                }
                if (has_dependency) {
                    node.outs.push_back(other_index);
                }
            }

            for (size_t read_index : read_resources) {
                bool has_dependency = false;
                for (size_t write_index : other_write_resources) {
                    if (pass.resources[read_index].identifier ==
                        other_pass.resources[write_index].identifier) {
                        has_dependency = true;
                        break;
                    }
                }
                if (has_dependency) {
                    node.ins.push_back(other_index);
                }
            }
        }

        nodes.push_back(node);
    }

    return nodes;
}

///////////////////////////////////////////////////////////
std::vector<std::pair<GraphNode, size_t>> prune(const std::vector<RenderPassParams>& pass_params,
                                                const std::vector<GraphNode>& nodes) {
    std::vector<std::pair<GraphNode, size_t>> dfs;
    std::vector<GraphNode> stack;
    std::set<size_t> visited;

    // Find root node
    for (const GraphNode& node : nodes) {
        if (pass_params[node.pass_index].is_root) {
            stack.push_back(node);
        }
    }

    // DFS walk backward from the root node will eliminate all dead branches
    while (!stack.empty()) {
        GraphNode node = stack.back();
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
std::vector<size_t> topology_sort(std::vector<std::pair<GraphNode, size_t>>& nodes) {
    std::vector<size_t> pass_order;
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

                pass_order.push_back(entry.first.pass_index);
                nodes.erase(nodes.begin() + i);
                break;
            }
            i++;
        }
    }
    return pass_order;
}

///////////////////////////////////////////////////////////
RenderGraph::RenderGraph(const RenderGraphParams& params)
    : vkal_device(params.vkal_device), vkal_surface(params.vkal_surface),
      render_resources(params.render_resources), vkal_descriptor(params.vkal_descriptor) {

    this->semaphore = this->vkal_device.get().createSemaphore(vk::SemaphoreCreateInfo());
    this->fence = this->vkal_device.get().createFence(
        vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled));
    this->command_submit_info.setDeviceMask(1);
    this->wait_semaphore_info.setSemaphore(this->semaphore)
        .setStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput);
    this->signal_semaphore_info.setStageMask(vk::PipelineStageFlagBits2::eAllCommands);

    this->submit_info.setCommandBufferInfos(command_submit_info)
        .setWaitSemaphoreInfos(wait_semaphore_info)
        .setSignalSemaphoreInfos(signal_semaphore_info);

    this->reset_swapchain_images();
}

RenderGraph::~RenderGraph() {
    this->vkal_device.get().destroySemaphore(this->semaphore);
    this->vkal_device.get().destroyFence(this->fence);
}

///////////////////////////////////////////////////////////
void RenderGraph::add_pass(const RenderPassParams& pass_params) {
    this->pass_params.push_back(pass_params);
}

///////////////////////////////////////////////////////////
void RenderGraph::compile() {
    std::vector<GraphNode> nodes = generate_graph(this->pass_params);
    std::vector<std::pair<GraphNode, size_t>> post_pruned = prune(this->pass_params, nodes);
    this->pass_order = topology_sort(post_pruned);

    this->generate_passes();
}

///////////////////////////////////////////////////////////
void RenderGraph::generate_passes() {
    for (size_t pass_index : this->pass_order) {
        const RenderPassParams& pass_params = this->pass_params[pass_index];

        std::unique_ptr<RenderPass> pass = std::make_unique<RenderPass>();
        pass->pre_render_callback = pass_params.pre_render_callback;
        pass->render_callback = pass_params.render_callback;

        if (pass_params.pipeline.has_value()) {
            pass->pipeline = this->render_resources.get_pipeline(*pass_params.pipeline);
            pass->descriptor_set = descriptor_set_ptr(DescriptorSetParams{
                .vkal_device = this->vkal_device,
                .vkal_layouts = pass->pipeline->get().get_layout().get_descriptor_layouts(),
                .vkal_descriptor = this->vkal_descriptor,
            });
        }
        // Store image layouts by identifier to later setup render attachments
        std::unordered_map<std::string, vk::ImageLayout> layouts;

        // Collect all buffer and image resources
        for (const ResourceDescription& resource_description : pass_params.resources) {
            const std::string& identifier = resource_description.identifier;
            std::optional<ResourceBarrier> barrier = resource_description.barrier;

            if (resource_description.type == ResourceDescription::Type::BUFFER) {
                Buffer& buffer = this->render_resources.get_buffer(identifier);
                pass->buffer_resources.try_emplace(identifier, buffer);

                // Setup barrier if there are any
                if (barrier.has_value()) {
                    vk::BufferMemoryBarrier2 buffer_barrier;
                    buffer_barrier.setBuffer(buffer.get())
                        .setOffset(0)
                        .setSize(buffer.get_size())
                        .setDstAccessMask(barrier->access)
                        .setDstStageMask(barrier->stage);
                    pass->buffer_barriers.push_back(buffer_barrier);
                    pass->buffer_barriers_refs.push_back(buffer);
                }

                // Write to descriptor set if enabled
                if (pass->descriptor_set != nullptr && resource_description.write_set) {
                    pass->descriptor_set->write_buffer(BufferWriteParams{
                        .buffers = {buffer},
                        .set_index = resource_description.set,
                        .type = resource_description.descriptor_type,
                        .binding = resource_description.binding,
                        .array_size = 1,
                        .first_element = 0,
                    });
                }
            } else if (resource_description.type == ResourceDescription::Type::IMAGE) {
                Image& image = this->render_resources.get_image(identifier);
                pass->image_resources.try_emplace(identifier, image);

                // Setup barrier if there are any
                if (barrier.has_value()) {
                    vk::ImageMemoryBarrier2 image_barrier;
                    image_barrier.setImage(image.get())
                        .setDstAccessMask(barrier->access)
                        .setDstStageMask(barrier->stage)
                        .setNewLayout(barrier->layout)
                        .setSubresourceRange(
                            vk::ImageSubresourceRange(image.get_aspect(), 0, 1, 0, 1));
                    pass->image_barriers.push_back(std::move(image_barrier));
                    pass->image_barriers_refs.push_back(image);
                    layouts.emplace(identifier, barrier->layout);
                }

                // Write to descriptor set if enabled
                if (pass->descriptor_set != nullptr && resource_description.write_set) {
                    pass->descriptor_set->write_sampler(SamplerWriteParams{
                        .images = {image},
                        .set_index = resource_description.set,
                        .type = resource_description.descriptor_type,
                        .binding = resource_description.binding,
                        .array_size = 1,
                        .first_element = 0,
                    });
                }
            }
        }

        // Collect all sampler resources
        for (const std::string& identifier : pass_params.samplers) {
            pass->sampler_resources.try_emplace(identifier,
                                                this->render_resources.get_sampler(identifier));
        }

        // Create a dependency info if there are resource barriers
        if (!pass->buffer_barriers.empty() || !pass->image_barriers.empty()) {
            pass->dependency_info.emplace();
            if (!pass->buffer_barriers.empty()) {
                pass->dependency_info->setBufferMemoryBarriers(pass->buffer_barriers);
            }
            if (!pass->image_barriers.empty()) {
                pass->dependency_info->setImageMemoryBarriers(pass->image_barriers);
            }
        }

        // Setup render attachments if pass is not root
        if (pass_params.is_root) {
            pass->swapchain_attachment
                .setClearValue(vk::ClearValue(
                    vk::ClearColorValue(std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f})))
                .setLoadOp(vk::AttachmentLoadOp::eClear)
                .setStoreOp(vk::AttachmentStoreOp::eStore)
                .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal);
            pass->rendering_info.setLayerCount(1).setColorAttachments(pass->swapchain_attachment);

            pass->swapchain_depedency_info.emplace();
            pass->swapchain_depedency_info->setImageMemoryBarriers(pass->swapchain_barrier);
        } else {
            for (const RenderAttachmentParams& attachment_params : pass_params.render_attachments) {
                Image& image = pass->image_resources.at(attachment_params.resolve_image);
                vk::RenderingAttachmentInfo render_attachment;
                render_attachment.setClearValue(attachment_params.clear_value)
                    .setLoadOp(attachment_params.load_op)
                    .setStoreOp(attachment_params.store_op)
                    .setImageView(
                        pass->image_resources.at(attachment_params.image).get().get_view())
                    .setImageLayout(layouts.at(attachment_params.image))
                    .setResolveImageView(image.get_view())
                    .setResolveImageLayout(layouts.at(attachment_params.resolve_image));
                pass->render_attachments.push_back(render_attachment);
                pass->attachment_images.push_back(image);
            }
            pass->rendering_info.setLayerCount(1).setColorAttachments(pass->render_attachments);
        }

        this->passes.push_back(std::move(pass));
    }
}

///////////////////////////////////////////////////////////
void RenderGraph::sync() {
    vk::Result result = this->vkal_device.get().waitForFences(this->fence, true, UINT64_MAX);
    if (result != vk::Result::eSuccess) {
        throw std::runtime_error(
            std::format("Failed to wait for fences due to {}", vk::to_string(result)));
    }
}

///////////////////////////////////////////////////////////
void RenderGraph::execute(uint32_t swapchain_index, vk::Queue queue, vk::CommandBuffer command,
                          vk::Semaphore semaphore) {
    this->vkal_device.get().resetFences(this->fence);
    this->signal_semaphore_info.setSemaphore(semaphore);
    command.reset();
    command.begin(vk::CommandBufferBeginInfo());

    for (std::unique_ptr<RenderPass>& pass : this->passes) {
        // Pre render barriers
        if (pass->dependency_info.has_value()) {
            for (const auto& [index, buffer_barrier] :
                 std::ranges::views::enumerate(pass->buffer_barriers)) {
                buffer_barrier
                    .setSrcAccessMask(pass->buffer_barriers_refs[index].get().get_access())
                    .setSrcStageMask(pass->buffer_barriers_refs[index].get().get_stage());
                pass->buffer_barriers_refs[index].get().set_barrier(buffer_barrier.dstAccessMask,
                                                                    buffer_barrier.dstStageMask);
            }

            for (const auto& [index, image_barrier] :
                 std::ranges::views::enumerate(pass->image_barriers)) {
                image_barrier.setDstAccessMask(pass->image_barriers_refs[index].get().get_access(0))
                    .setDstStageMask(pass->image_barriers_refs[index].get().get_stage(0));
                pass->image_barriers_refs[index].get().set_barrier(image_barrier.newLayout,
                                                                   image_barrier.dstAccessMask,
                                                                   image_barrier.dstStageMask);
            }
            command.pipelineBarrier2(*pass->dependency_info);
        }

        if (pass->swapchain_depedency_info.has_value()) {
            Image& image = this->swapchain_images[swapchain_index];
            pass->swapchain_barrier.setImage(image.get())
                .setSrcAccessMask(vk::AccessFlagBits2::eNone)
                .setSrcStageMask(vk::PipelineStageFlagBits2::eNone)
                .setOldLayout(vk::ImageLayout::eUndefined)
                .setDstAccessMask(vk::AccessFlagBits2::eColorAttachmentWrite)
                .setDstStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput)
                .setNewLayout(vk::ImageLayout::eColorAttachmentOptimal)
                .setSubresourceRange(vk::ImageSubresourceRange(image.get_aspect(), 0, 1, 0, 1));

            command.pipelineBarrier2(*pass->swapchain_depedency_info);
        }

        if (pass->render_attachments.empty()) {
            Image& image = this->swapchain_images[swapchain_index];
            pass->swapchain_attachment.setImageView(image.get_view());
            pass->rendering_info.setRenderArea(
                vk::Rect2D(vk::Offset2D(0, 0),
                           vk::Extent2D(image.get_extent().width, image.get_extent().height)));
        } else {
            Image& image = pass->attachment_images.front();
            pass->rendering_info.setRenderArea(
                vk::Rect2D(vk::Offset2D(0, 0),
                           vk::Extent2D(image.get_extent().width, image.get_extent().height)));
        }

        command.beginRendering(pass->rendering_info);
        pass->pre_render_callback(pass->viewport, pass->scissor);
        command.setViewport(0, pass->viewport);
        command.setScissor(0, pass->scissor);
        pass->render_callback(command, pass->buffer_resources, pass->image_resources,
                              pass->pipeline, *pass->descriptor_set);

        command.endRendering();

        // Post render barriers
        if (pass->swapchain_depedency_info.has_value()) {
            pass->swapchain_barrier.setSrcAccessMask(pass->swapchain_barrier.dstAccessMask)
                .setSrcStageMask(pass->swapchain_barrier.dstStageMask)
                .setOldLayout(pass->swapchain_barrier.newLayout)
                .setDstAccessMask(vk::AccessFlagBits2::eColorAttachmentRead)
                .setDstStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput)
                .setNewLayout(vk::ImageLayout::ePresentSrcKHR);

            command.pipelineBarrier2(*pass->swapchain_depedency_info);
        }
    }

    command.end();
    this->command_submit_info.setCommandBuffer(command);
    queue.submit2(this->submit_info, this->fence);
}

///////////////////////////////////////////////////////////
vk::Semaphore RenderGraph::get_semaphore() {
    return this->semaphore;
}

///////////////////////////////////////////////////////////
void RenderGraph::reset_swapchain_images() {
    this->swapchain_images.clear();
    for (size_t i = 0; i < this->vkal_surface.get_image_count(); ++i) {
        this->swapchain_images.push_back(this->vkal_surface.get_image(i));
    }
}

} // namespace vkal
