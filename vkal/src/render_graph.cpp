#include "render_graph.hpp"
#include "common.hpp"

#include <ranges>
#include <set>

namespace vkal {

///////////////////////////////////////////////////////////
static void
categorize_buffer_resources(const std::vector<BufferResourceDescription>& buffer_resources,
                            std::vector<std::pair<size_t, size_t>>& read_resources,
                            std::vector<std::pair<size_t, size_t>>& write_resources) {
    for (const auto& [index, resource] : std::ranges::views::enumerate(buffer_resources)) {
        if (!resource.barrier.has_value() ||
            static_cast<bool>(resource.barrier->access & VK_ACCESS_READ_FLAGS)) {
            read_resources.push_back({0, index});
        }
        if (resource.barrier.has_value() &&
            static_cast<bool>(resource.barrier->access & VK_ACCESS_WRITE_FLAGS)) {
            write_resources.push_back({0, index});
        }
    }
}

///////////////////////////////////////////////////////////
static void categorize_image_resources(const std::vector<ImageResourceDescription>& image_resources,
                                       std::vector<std::pair<size_t, size_t>>& read_resources,
                                       std::vector<std::pair<size_t, size_t>>& write_resources) {
    for (const auto& [index, resource] : std::ranges::views::enumerate(image_resources)) {
        if (static_cast<bool>(resource.barrier.access & VK_ACCESS_READ_FLAGS)) {
            read_resources.push_back({1, index});
        }
        if (static_cast<bool>(resource.barrier.access & VK_ACCESS_WRITE_FLAGS)) {
            write_resources.push_back({1, index});
        }
    }
}

///////////////////////////////////////////////////////////
static std::vector<GraphNode> generate_graph(const std::vector<RenderPassParams>& pass_params) {
    std::vector<GraphNode> nodes;
    for (const auto& [index, pass] : std::ranges::views::enumerate(pass_params)) {
        GraphNode node;
        node.pass_index = index;

        // First member of the pair determines the type of the resource
        // 0 = buffer
        // 1 = image
        std::vector<std::pair<size_t, size_t>> read_resources;
        std::vector<std::pair<size_t, size_t>> write_resources;
        categorize_buffer_resources(pass.buffer_resources, read_resources, write_resources);
        categorize_image_resources(pass.image_resources, read_resources, write_resources);

        for (const auto& [other_index, other_pass] : std::ranges::views::enumerate(pass_params)) {
            if (index == other_index) {
                continue;
            }
            // Matching to find any matching resource on both passes
            std::vector<std::pair<size_t, size_t>> other_read_resources;
            std::vector<std::pair<size_t, size_t>> other_write_resources;
            categorize_buffer_resources(other_pass.buffer_resources, other_read_resources,
                                        other_write_resources);
            categorize_image_resources(other_pass.image_resources, other_read_resources,
                                       other_write_resources);

            for (const std::pair<size_t, size_t>& write : write_resources) {
                bool has_dependency = false;
                for (const std::pair<size_t, size_t>& read : other_read_resources) {
                    if (write.first == read.first) {
                        if (write.first == 0) {
                            if (pass.buffer_resources[write.second].identifier ==
                                other_pass.buffer_resources[read.second].identifier) {
                                has_dependency = true;
                                break;
                            }
                        } else {
                            if (pass.image_resources[write.second].identifier ==
                                other_pass.image_resources[read.second].identifier) {
                                has_dependency = true;
                                break;
                            }
                        }
                    }
                }
                if (has_dependency) {
                    node.outs.push_back(other_index);
                }
            }

            for (const std::pair<size_t, size_t>& read : read_resources) {
                bool has_dependency = false;
                for (const std::pair<size_t, size_t>& write : other_write_resources) {
                    if (read.first == write.first) {
                        if (read.first == 0) {
                            if (pass.buffer_resources[read.second].identifier ==
                                other_pass.buffer_resources[write.second].identifier) {
                                has_dependency = true;
                                break;
                            }
                        } else {
                            if (pass.image_resources[read.second].identifier ==
                                other_pass.image_resources[write.second].identifier) {
                                has_dependency = true;
                                break;
                            }
                        }
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

    // The root node is the ndoe that contains a render attachment that writes to the swapchain
    for (const GraphNode& node : nodes) {
        bool is_root = false;
        for (const RenderAttachmentParams& attachment_params :
             pass_params[node.pass_index].render_attachments) {
            if (attachment_params.type == RenderAttachmentType::SWAPCHAIN) {
                is_root = true;
                break;
            }
        }
        if (is_root) {
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

    this->reset_swapchain_images();
}

RenderGraph::~RenderGraph() {
    this->vkal_device.get().destroySemaphore(this->semaphore);
    this->vkal_device.get().destroyFence(this->fence);
}

///////////////////////////////////////////////////////////
void RenderGraph::add_pass(RenderPassParams pass_params) {
    this->pass_params.push_back(std::move(pass_params));
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
    debug(std::format("Compiling render graph ({} passes)", this->pass_params.size()));
    for (size_t pass_index : this->pass_order) {
        RenderPassParams& pass_params = this->pass_params[pass_index];

        std::unique_ptr<RenderPassData> pass = std::make_unique<RenderPassData>();

        if (pass_params.pipeline.has_value()) {
            pass->pipeline = this->render_resources.get_pipeline(*pass_params.pipeline);
            std::vector<std::reference_wrapper<DescriptorLayout>> set_layouts =
                pass->pipeline->get().get_layout().get_descriptor_layouts();
            uint32_t max_array_size = 1;
            for (const DescriptorLayout& set_layout : set_layouts) {
                std::vector<vk::DescriptorSetLayoutBinding> bindings = set_layout.get_bindings();
                for (vk::DescriptorSetLayoutBinding& binding : bindings) {
                    max_array_size = std::max(binding.descriptorCount, max_array_size);
                }
            }

            DescriptorSetParams set_params = {
                .vkal_device = this->vkal_device,
                .vkal_layouts = set_layouts,
                .vkal_descriptor = this->vkal_descriptor,
            };
            if (max_array_size > 1) {
                set_params.enable_dynamic_sized_array = true;
                set_params.dynamic_array_size = max_array_size;
            }
            pass->descriptor_set = descriptor_set_ptr(set_params);
        }
        // Store image layouts by identifier to later setup render attachments
        std::unordered_map<std::string, vk::ImageLayout> layouts;

        // Collect all buffer and image resources
        for (const BufferResourceDescription& buffer_description : pass_params.buffer_resources) {
            const std::string& identifier = buffer_description.identifier;
            std::optional<ResourceBarrier> barrier = buffer_description.barrier;

            Buffer& buffer = this->render_resources.get_buffer(identifier);
            pass->buffer_resources.try_emplace(identifier, buffer);

            // Setup barrier if there are any
            if (barrier.has_value()) {
                pass->buffer_barrier_builders.push_back(BufferBarrierBuilder{
                    .identifier = identifier,
                    .preserve = barrier->preserve,
                    .buffer = buffer,
                    .access = barrier->access,
                    .stage = barrier->stage,
                });
            }

            // Write to descriptor set if enabled
            if (pass->descriptor_set != nullptr &&
                buffer_description.resource_descriptor.has_value()) {
                pass->descriptor_set->write_buffer(BufferWriteParams{
                    .buffers = {buffer},
                    .set_index = buffer_description.resource_descriptor->set,
                    .type = buffer_description.resource_descriptor->type,
                    .binding = buffer_description.resource_descriptor->binding,
                    .first_element = buffer_description.resource_descriptor->array_index,
                });
                pass->buffer_descriptor_data.push_back(BufferDescriptorData{
                    .identifier = identifier,
                    .descriptor = *buffer_description.resource_descriptor,
                });
            }
        }
        pass->buffer_barriers.reserve(pass->buffer_barrier_builders.size());

        for (const ImageResourceDescription& image_description : pass_params.image_resources) {
            const std::string& identifier = image_description.identifier;
            ResourceBarrier barrier = image_description.barrier;

            Image& image = this->render_resources.get_image(identifier);
            pass->image_resources.try_emplace(identifier, image);

            // Setup image barrier
            pass->image_barrier_builders.push_back(ImageBarrierBuilder{
                .identifier = identifier,
                .preserve = barrier.preserve,
                .image = image,
                .access = barrier.access,
                .stage = barrier.stage,
                .layout = barrier.layout,
            });
            layouts.emplace(identifier, barrier.layout);

            // Write to descriptor set if enabled
            if (pass->descriptor_set != nullptr &&
                image_description.resource_descriptor.has_value()) {
                pass->descriptor_set->write_sampler(SamplerWriteParams{
                    .images = {image},
                    .layout = barrier.layout,
                    .set_index = image_description.resource_descriptor->set,
                    .type = image_description.resource_descriptor->type,
                    .binding = image_description.resource_descriptor->binding,
                    .first_element = image_description.resource_descriptor->array_index,
                });
                pass->image_descriptor_data.push_back(ImageDescriptorData{
                    .identifier = identifier,
                    .layout = barrier.layout,
                    .descriptor = *image_description.resource_descriptor,
                });
            }
        }
        pass->image_barriers.reserve(pass->image_barrier_builders.size() + 1);

        // Collect all sampler resources
        for (const SamplerResourceDescription& sampler_resource : pass_params.sampler_resources) {
            const std::string& identifier = sampler_resource.identifier;
            Sampler& sampler = this->render_resources.get_sampler(identifier);
            if (pass->descriptor_set != nullptr) {
                pass->descriptor_set->write_sampler(SamplerWriteParams{
                    .samplers = {sampler},
                    .set_index = sampler_resource.set,
                    .type = vk::DescriptorType::eSampler,
                    .binding = sampler_resource.binding,
                    .first_element = sampler_resource.array_index,
                });
            }
            pass->sampler_resources.try_emplace(identifier, sampler);
        }

        // Render attachments are only relevant if the pipeline is a graphics pipeline
        vk::Extent3D required_extent(0, 0, 0);
        if (pass->pipeline.has_value() &&
            pass->pipeline->get().get_bind_point() == vk::PipelineBindPoint::eGraphics) {
            for (const RenderAttachmentParams& attachment_params : pass_params.render_attachments) {
                RenderAttachmentBuilder attachment_builder = {
                    .type = attachment_params.type,
                    .image_identifier = attachment_params.image,
                    .resolve_image_identifier = attachment_params.resolve_image,
                    .clear_value = attachment_params.clear_value,
                    .load_op = attachment_params.load_op,
                    .store_op = attachment_params.store_op,
                };
                vk::Extent3D attachment_extent(0, 0, 0);
                if (attachment_params.image.has_value()) {
                    Image& image = pass->image_resources.at(*attachment_params.image);
                    attachment_builder.image = image;
                    attachment_builder.layout = layouts.at(*attachment_params.image);
                    attachment_extent = image.get_extent();
                }

                if (attachment_params.type == RenderAttachmentType::SWAPCHAIN) {
                    pass->write_swapchain = true;
                    vk::Extent3D swapchain_extent =
                        this->swapchain_images.front().get().get_extent();
                    if (attachment_extent.width == 0) {
                        attachment_extent = swapchain_extent;
                    } else if (attachment_extent != swapchain_extent) {
                        throw std::runtime_error("Render graph compite error (Swapchain "
                                                 "image and main image extent mismatch)");
                    }

                    // Swapchain layout is guaranteed to be color attachment optimal
                    if (attachment_builder.image.has_value()) {
                        attachment_builder.resolve_layout =
                            vk::ImageLayout::eColorAttachmentOptimal;
                    } else {
                        attachment_builder.layout = vk::ImageLayout::eColorAttachmentOptimal;
                    }
                } else if (attachment_builder.resolve_image_identifier.has_value()) {
                    Image& resolve_image =
                        pass->image_resources.at(*attachment_params.resolve_image);
                    if (attachment_extent != resolve_image.get_extent()) {
                        throw std::runtime_error("Render graph compite error (Main "
                                                 "image and resolve image extent mismatch)");
                    }
                    attachment_builder.resolve_image = resolve_image;
                    attachment_builder.resolve_layout =
                        layouts.at(*attachment_builder.resolve_image_identifier);
                    attachment_builder.resolve_mode = attachment_params.resolve_mode;
                }

                // Detect extent variation between render attachments
                if (required_extent.width == 0) {
                    required_extent = attachment_extent;
                } else if (required_extent != attachment_extent) {
                    throw std::runtime_error(
                        "Render graph compile error (Attachments have different extents)");
                }

                pass->render_attachment_builders.push_back(std::move(attachment_builder));
            }
            pass->render_attachment_infos.reserve(pass->render_attachment_builders.size());
        }

        pass->pass = std::move(pass_params.pass);
        std::optional<std::reference_wrapper<DescriptorSet>> descriptor_set = std::nullopt;
        if (pass->descriptor_set != nullptr) {
            descriptor_set = *pass->descriptor_set;
        }
        pass->pass->setup_metadata(pass->buffer_resources, pass->image_resources, pass->pipeline,
                                   descriptor_set);
        this->pass_data.push_back(std::move(pass));
    }

    debug("Compiled render graph");
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
    command.reset();
    command.begin(vk::CommandBufferBeginInfo());

    Image& swapchain_image = this->swapchain_images[swapchain_index];

    for (std::unique_ptr<RenderPassData>& pass : this->pass_data) {
        pass->render_attachment_infos.clear();
        pass->buffer_barriers.clear();
        pass->image_barriers.clear();

        // Pre render barriers
        vk::DependencyInfo dependency_info;
        for (const BufferBarrierBuilder& buffer_barrier_builder : pass->buffer_barrier_builders) {
            Buffer& buffer = buffer_barrier_builder.buffer;
            vk::BufferMemoryBarrier2 buffer_barrier;
            bool preserve = buffer_barrier_builder.preserve;
            buffer_barrier.setBuffer(buffer.get())
                .setOffset(0)
                .setSize(buffer.get_size())
                .setSrcAccessMask(preserve ? buffer.get_access() : vk::AccessFlagBits2::eNone)
                .setSrcStageMask(preserve ? buffer.get_stage() : vk::PipelineStageFlagBits2::eNone)
                .setDstAccessMask(buffer_barrier_builder.access)
                .setDstStageMask(buffer_barrier_builder.stage);
            pass->buffer_barriers.push_back(buffer_barrier);
            buffer.set_barrier(buffer_barrier_builder.access, buffer_barrier_builder.stage);
        }

        for (const ImageBarrierBuilder& image_barrier_builder : pass->image_barrier_builders) {
            Image& image = image_barrier_builder.image;
            vk::ImageMemoryBarrier2 image_barrier;
            bool preserve = image_barrier_builder.preserve;
            image_barrier.setImage(image.get())
                .setSrcAccessMask(preserve ? image.get_access(0) : vk::AccessFlagBits2::eNone)
                .setSrcStageMask(preserve ? image.get_stage(0) : vk::PipelineStageFlagBits2::eNone)
                .setOldLayout(preserve ? image.get_layout(0) : vk::ImageLayout::eUndefined)
                .setDstAccessMask(image_barrier_builder.access)
                .setDstStageMask(image_barrier_builder.stage)
                .setNewLayout(image_barrier_builder.layout)
                .setSubresourceRange(
                    vk::ImageSubresourceRange(image.get_aspect(), 0, image.get_mip_count(), 0, 1));
            pass->image_barriers.push_back(image_barrier);
            image.set_barrier(image_barrier_builder.layout, image_barrier_builder.access,
                              image_barrier_builder.stage);
        }

        if (pass->write_swapchain) {
            vk::ImageMemoryBarrier2 swapchain_barrier;
            swapchain_barrier.setImage(swapchain_image.get())
                .setSrcAccessMask(vk::AccessFlagBits2::eNone)
                .setSrcStageMask(vk::PipelineStageFlagBits2::eNone)
                .setOldLayout(vk::ImageLayout::eUndefined)
                .setDstAccessMask(vk::AccessFlagBits2::eColorAttachmentWrite)
                .setDstStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput)
                .setNewLayout(vk::ImageLayout::eColorAttachmentOptimal)
                .setSubresourceRange(
                    vk::ImageSubresourceRange(swapchain_image.get_aspect(), 0, 1, 0, 1));
            swapchain_image.set_barrier(vk::ImageLayout::eColorAttachmentOptimal,
                                        vk::AccessFlagBits2::eColorAttachmentWrite,
                                        vk::PipelineStageFlagBits2::eColorAttachmentOutput);
            pass->image_barriers.push_back(swapchain_barrier);
        }

        dependency_info.setBufferMemoryBarriers(pass->buffer_barriers)
            .setImageMemoryBarriers(pass->image_barriers);
        command.pipelineBarrier2(dependency_info);

        vk::Extent3D render_extent;
        if (!pass->render_attachment_builders.empty()) {
            for (const auto& [index, attachment_builder] :
                 std::ranges::views::enumerate(pass->render_attachment_builders)) {
                vk::RenderingAttachmentInfo render_attachment_info;
                render_attachment_info.setClearValue(attachment_builder.clear_value)
                    .setLoadOp(attachment_builder.load_op)
                    .setStoreOp(attachment_builder.store_op);

                if (attachment_builder.type == RenderAttachmentType::SWAPCHAIN) {
                    render_extent = swapchain_image.get_extent();

                    if (attachment_builder.image.has_value()) {
                        render_attachment_info
                            .setImageView(attachment_builder.image->get().get_view())
                            .setImageLayout(attachment_builder.layout);
                        render_attachment_info.setResolveImageView(swapchain_image.get_view())
                            .setResolveImageLayout(attachment_builder.resolve_layout)
                            .setResolveMode(attachment_builder.resolve_mode);
                    } else { // If render attachment both writes to swapchain and contains a valid
                             // image the swapchain will be used as resolve image
                        render_attachment_info.setImageView(swapchain_image.get_view())
                            .setImageLayout(attachment_builder.layout);
                    }
                } else {
                    render_extent = attachment_builder.image->get().get_extent();
                    render_attachment_info.setImageView(attachment_builder.image->get().get_view())
                        .setImageLayout(attachment_builder.layout);
                    if (attachment_builder.resolve_image.has_value()) {
                        render_attachment_info
                            .setResolveImageView(attachment_builder.resolve_image->get().get_view())
                            .setResolveImageLayout(attachment_builder.resolve_layout)
                            .setResolveMode(attachment_builder.resolve_mode);
                    }
                }

                pass->render_attachment_infos.push_back(render_attachment_info);
            }

            // Assume all render attachment images are of the same extent
            vk::RenderingInfo rendering_info;
            rendering_info
                .setRenderArea(vk::Rect2D(vk::Offset2D(0, 0),
                                          vk::Extent2D(render_extent.width, render_extent.height)))
                .setLayerCount(1)
                .setColorAttachments(pass->render_attachment_infos);
            command.beginRendering(rendering_info);
            pass->pass->render(command);
            command.endRendering();
        } else { // If pass contains no render attachments proceed without render commands
            pass->pass->render(command);
        }

        // Post render barriers
        if (pass->write_swapchain) {
            vk::ImageMemoryBarrier2 swapchain_barrier;
            swapchain_barrier.setImage(swapchain_image.get())
                .setSrcAccessMask(swapchain_image.get_access(0))
                .setSrcStageMask(swapchain_image.get_stage(0))
                .setOldLayout(swapchain_image.get_layout(0))
                .setDstAccessMask(vk::AccessFlagBits2::eColorAttachmentRead)
                .setDstStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput)
                .setNewLayout(vk::ImageLayout::ePresentSrcKHR)
                .setSubresourceRange(
                    vk::ImageSubresourceRange(swapchain_image.get_aspect(), 0, 1, 0, 1));
            dependency_info.setBufferMemoryBarriers({}).setImageMemoryBarriers(swapchain_barrier);
            command.pipelineBarrier2(dependency_info);
        }
    }

    command.end();

    vk::CommandBufferSubmitInfo command_submit_info;
    command_submit_info.setCommandBuffer(command).setDeviceMask(1);

    vk::SemaphoreSubmitInfo signal_semaphore_info;
    signal_semaphore_info.setSemaphore(semaphore).setStageMask(
        vk::PipelineStageFlagBits2::eColorAttachmentOutput);

    vk::SemaphoreSubmitInfo wait_semaphore_info;
    wait_semaphore_info.setSemaphore(this->semaphore)
        .setStageMask(vk::PipelineStageFlagBits2::eAllCommands);

    vk::SubmitInfo2 submit_info;
    submit_info.setCommandBufferInfos(command_submit_info)
        .setSignalSemaphoreInfos(signal_semaphore_info)
        .setWaitSemaphoreInfos(wait_semaphore_info);
    queue.submit2(submit_info, this->fence);
}

///////////////////////////////////////////////////////////
void RenderGraph::rebind_resources() {
    for (std::unique_ptr<RenderPassData>& pass_data : this->pass_data) {
        for (auto& entry : pass_data->buffer_resources) {
            entry.second = this->render_resources.get_buffer(entry.first);
        }
        for (auto& entry : pass_data->image_resources) {
            entry.second = this->render_resources.get_image(entry.first);
        }
        for (const BufferDescriptorData& descriptor_data : pass_data->buffer_descriptor_data) {
            pass_data->descriptor_set->write_buffer(BufferWriteParams{
                .buffers = {pass_data->buffer_resources.at(descriptor_data.identifier)},
                .set_index = descriptor_data.descriptor.set,
                .type = descriptor_data.descriptor.type,
                .binding = descriptor_data.descriptor.binding,
                .first_element = 0,
            });
        }
        for (const ImageDescriptorData& descriptor_data : pass_data->image_descriptor_data) {
            pass_data->descriptor_set->write_sampler(SamplerWriteParams{
                .images = {pass_data->image_resources.at(descriptor_data.identifier)},
                .layout = descriptor_data.layout,
                .set_index = descriptor_data.descriptor.set,
                .type = descriptor_data.descriptor.type,
                .binding = descriptor_data.descriptor.binding,
                .first_element = 0,
            });
        }
        for (RenderAttachmentBuilder& attachment_builder : pass_data->render_attachment_builders) {
            if (attachment_builder.image_identifier.has_value()) {
                attachment_builder.image =
                    pass_data->image_resources.at(*attachment_builder.image_identifier);
                if (attachment_builder.resolve_image_identifier.has_value()) {
                    attachment_builder.resolve_image =
                        pass_data->image_resources.at(*attachment_builder.resolve_image_identifier);
                }
            }
        }
        for (BufferBarrierBuilder& barrier_builder : pass_data->buffer_barrier_builders) {
            barrier_builder.buffer = pass_data->buffer_resources.at(barrier_builder.identifier);
        }
        for (ImageBarrierBuilder& barrier_builder : pass_data->image_barrier_builders) {
            barrier_builder.image = pass_data->image_resources.at(barrier_builder.identifier);
        }
        pass_data->pass->on_rebound_resources(pass_data->buffer_resources,
                                              pass_data->image_resources);
    }
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
