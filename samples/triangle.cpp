#include "../vkal/src/buffer.hpp"
#include "../vkal/src/command.hpp"
#include "../vkal/src/common.hpp"
#include "../vkal/src/descriptor.hpp"
#include "../vkal/src/descriptor_set.hpp"
#include "../vkal/src/device.hpp"
#include "../vkal/src/image.hpp"
#include "../vkal/src/instance.hpp"
#include "../vkal/src/pipeline.hpp"
#include "../vkal/src/surface.hpp"

#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <exception>
#include <print>
#include <stdexcept>

constexpr vk::Extent2D WINDOW_EXTENT = vk::Extent2D(600, 600);

struct Vertex {
    glm::vec3 position;
    glm::vec3 color;
};

struct Uniform {
    glm::mat4 rotation;
};

int main() {
    try {
        // Create Window
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            throw std::runtime_error(SDL_GetError());
        }

        SDL_Window* window = SDL_CreateWindow("Texture", WINDOW_EXTENT.width, WINDOW_EXTENT.height,
                                              SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
        if (!window) {
            throw std::runtime_error(SDL_GetError());
        }

        // Create device
        vkal::InstancePtr vkal_instance = vkal::instance_ptr();
        vkal::DevicePtr vkal_device = vkal::device_ptr(vkal::DeviceParams{
            .vkal_instance = *vkal_instance,
            .device_extensions = {vk::KHRSwapchainExtensionName, vk::KHRSpirv14ExtensionName,
                                  vk::KHRSynchronization2ExtensionName}});

        uint32_t queue_index = vkal_device->get_queue_index(vk::QueueFlagBits::eGraphics);
        vkal::CommandPtr vkal_command = vkal::command_ptr(vkal::CommandParams{
            .vkal_device = *vkal_device,
            .level = vk::CommandBufferLevel::ePrimary,
            .queue_index = queue_index,
            .command_count = 1,
        });
        vk::Queue queue = vkal_device->get_queue(queue_index);
        vk::CommandBuffer command = vkal_command->get(0);

        // Create surface
        vkal::SurfacePtr vkal_surface = vkal::surface_ptr(vkal::SurfaceParams{
            .window = window,
            .vkal_instance = *vkal_instance,
            .vkal_device = *vkal_device,
            .image_count = 3,
            .surface_format =
                vk::SurfaceFormatKHR(vk::Format::eB8G8R8A8Srgb, vk::ColorSpaceKHR::eSrgbNonlinear),
            .present_modes =
                {
                    vk::PresentModeKHR::eFifo,
                    vk::PresentModeKHR::eFifoLatestReady,
                    vk::PresentModeKHR::eMailbox,
                },
        });

        // Create memory allocator
        vkal::MemoryAllocatorPtr memory_allocator =
            vkal::memory_allocator_ptr(vkal::MemoryAllocatorParams{
                .vkal_device = *vkal_device,
                .block_size = vkal::megabytes(128),
            });

        // Create descriptor layout
        vkal::DescriptorLayoutPtr vkal_descriptor_layout =
            vkal::descriptor_layout_ptr(vkal::DescriptorLayoutParams{
                .vkal_device = *vkal_device,
                .bindings =
                    {
                        vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1,
                                                       vk::ShaderStageFlagBits::eVertex),
                    },
                .binding_flags = {vk::DescriptorBindingFlags()},
            });

        // Create graphcis pipeline
        vkal::PipelineLayoutPtr vkal_pipeline_layout =
            vkal::pipeline_layout_ptr(vkal::PipelineLayoutParams{
                .vkal_device = *vkal_device,
                .vkal_descriptor_layouts = {*vkal_descriptor_layout},
            });

        vkal::PipelinePtr vkal_graphics_pipeline =
            vkal::graphics_pipeline_ptr(vkal::GraphicsPipelineParams{
                .vkal_device = *vkal_device,
                .vkal_layout = *vkal_pipeline_layout,
                .shader_stages =
                    {
                        vkal::ShaderStage{
                            .file_path = "resources/shaders/vertex_color.spv",
                            .stage = vk::ShaderStageFlagBits::eVertex,
                        },
                        vkal::ShaderStage{
                            .file_path = "resources/shaders/vertex_color.spv",
                            .stage = vk::ShaderStageFlagBits::eFragment,
                        },
                    },
                .color_attachment_formats = {vk::Format::eB8G8R8A8Srgb},
                .rasterization_sample_count = vk::SampleCountFlagBits::e4,
                .vertex_input_rate = vk::VertexInputRate::eVertex,
                .vertex_stride = sizeof(Vertex),
                .vertex_descriptions =
                    {
                        vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat,
                                                            offsetof(Vertex, position)),
                        vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32B32Sfloat,
                                                            offsetof(Vertex, color)),
                    },
                .topology = vk::PrimitiveTopology::eTriangleList,
            });

        // Initializing resources
        std::array<Vertex, 3> vertices = {
            Vertex{.position =
                       glm::vec3(cosf(glm::radians(90.0f)), -sinf(glm::radians(90.0f)), 0.0f) *
                       0.75f,
                   .color = glm::vec3(1.0f, 0.0f, 0.0f)},
            Vertex{.position =
                       glm::vec3(cosf(glm::radians(-30.0f)), -sinf(glm::radians(-30.0f)), 0.0f) *
                       0.75f,
                   .color = glm::vec3(0.0f, 1.0f, 0.0f)},
            Vertex{.position =
                       glm::vec3(cosf(glm::radians(-150.0f)), -sinf(glm::radians(-150.0f)), 0.0f) *
                       0.75f,
                   .color = glm::vec3(0.0f, 0.0f, 1.0f)},
        };

        std::array<uint32_t, 3> indices = {0, 1, 2};

        Uniform uniform = {.rotation = glm::mat4(1.0f)};

        // Create vertex buffer
        vkal::BufferPtr vertex_buffer = vkal::buffer_ptr(vkal::BufferParams{
            .vkal_device = *vkal_device,
            .memory_allocator = *memory_allocator,
            .size = sizeof(Vertex) * vertices.size(),
            .usage = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
            .memory_properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
        });

        // Create index buffer
        vkal::BufferPtr index_buffer = vkal::buffer_ptr(vkal::BufferParams{
            .vkal_device = *vkal_device,
            .memory_allocator = *memory_allocator,
            .size = sizeof(uint32_t) * indices.size(),
            .usage = vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
            .memory_properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
        });

        // Craete uniform buffer
        vkal::BufferPtr uniform_buffer = vkal::buffer_ptr(vkal::BufferParams{
            .vkal_device = *vkal_device,
            .memory_allocator = *memory_allocator,
            .size = sizeof(Uniform),
            .usage = vk::BufferUsageFlagBits::eUniformBuffer,
            .memory_properties = vk::MemoryPropertyFlagBits::eHostVisible |
                                 vk::MemoryPropertyFlagBits::eHostCoherent,
        });

        {
            vkal::BufferPtr staging0 = vkal::buffer_ptr(vkal::BufferParams{
                .vkal_device = *vkal_device,
                .memory_allocator = *memory_allocator,
                .size = sizeof(Vertex) * vertices.size(),
                .usage = vk::BufferUsageFlagBits::eTransferSrc,
                .memory_properties = vk::MemoryPropertyFlagBits::eHostVisible |
                                     vk::MemoryPropertyFlagBits::eHostCoherent,
            });
            vkal::BufferPtr staging1 = vkal::buffer_ptr(vkal::BufferParams{
                .vkal_device = *vkal_device,
                .memory_allocator = *memory_allocator,
                .size = sizeof(uint32_t) * indices.size(),
                .usage = vk::BufferUsageFlagBits::eTransferSrc,
                .memory_properties = vk::MemoryPropertyFlagBits::eHostVisible |
                                     vk::MemoryPropertyFlagBits::eHostCoherent,
            });
            staging0->upload(vertices.data(), sizeof(Vertex) * vertices.size());
            staging1->upload(indices.data(), sizeof(uint32_t) * indices.size());

            command.begin(vk::CommandBufferBeginInfo());
            vk::BufferCopy2 region;
            region.setSrcOffset(0).setDstOffset(0).setSize(vertex_buffer->get_size());
            vk::CopyBufferInfo2 copy_buffer_info;
            copy_buffer_info.setSrcBuffer(staging0->get())
                .setDstBuffer(vertex_buffer->get())
                .setRegions(region);

            command.copyBuffer2(copy_buffer_info);
            region.setSize(index_buffer->get_size());
            copy_buffer_info.setSrcBuffer(staging1->get()).setDstBuffer(index_buffer->get());
            command.copyBuffer2(copy_buffer_info);

            command.end();

            vk::CommandBufferSubmitInfo command_submit_info;
            command_submit_info.setCommandBuffer(command).setDeviceMask(1);
            vk::SubmitInfo2 submit_info;
            submit_info.setCommandBufferInfos(command_submit_info);
            queue.submit2(submit_info);
            queue.waitIdle();
        }

        vkal::ImagePtr msaa = vkal::image_ptr(vkal::ImageParams{
            .vkal_device = *vkal_device,
            .memory_allocator = *memory_allocator,
            .type = vk::ImageType::e2D,
            .view_type = vk::ImageViewType::e2D,
            .extent = vk::Extent3D(WINDOW_EXTENT, 1),
            .format = vk::Format::eB8G8R8A8Srgb,
            .sample_count = vk::SampleCountFlagBits::e4,
            .aspects = vk::ImageAspectFlagBits::eColor,
            .mip_levels = 1,
            .usage = vk::ImageUsageFlagBits::eColorAttachment,
            .memory_properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
        });

        // Create descriptor sets
        vkal::DescriptorPtr vkal_descriptor = vkal::descriptor_ptr(vkal::DescriptorParams{
            .vkal_device = *vkal_device,
            .max_sets = 10,
            .pool_size = 10,
        });

        vkal::DescriptorSetPtr vkal_descriptor_set =
            vkal::descriptor_set_ptr(vkal::DescriptorSetParams{
                .vkal_device = *vkal_device,
                .vkal_layouts = {*vkal_descriptor_layout},
                .vkal_descriptor = *vkal_descriptor,
            });

        vkal_descriptor_set->write_buffer(vkal::BufferWriteParams{
            .buffers = {*uniform_buffer},
            .set_index = 0,
            .type = vk::DescriptorType::eUniformBuffer,
            .binding = 0,
        });

        // Create fence and semaphore
        vk::Fence fence =
            vkal_device->get().createFence(vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled));
        vk::Semaphore semaphore = vkal_device->get().createSemaphore(vk::SemaphoreCreateInfo());

        vk::RenderingAttachmentInfo rendering_attachment;
        rendering_attachment.setImageView(msaa->get_view())
            .setResolveMode(vk::ResolveModeFlagBits::eAverage)
            .setClearValue(vk::ClearColorValue(std::array<float, 4>{0.01f, 0.01f, 0.01f, 1.0f}))
            .setLoadOp(vk::AttachmentLoadOp::eClear)
            .setStoreOp(vk::AttachmentStoreOp::eStore)
            .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
            .setResolveImageLayout(vk::ImageLayout::eColorAttachmentOptimal);

        vk::RenderingInfo rendering_info;
        rendering_info.setColorAttachments(rendering_attachment).setLayerCount(1);

        vk::Viewport viewport(0, 0, WINDOW_EXTENT.width, WINDOW_EXTENT.height, 0.0, 1.0);
        vk::Rect2D scissor(vk::Offset2D(0, 0), WINDOW_EXTENT);

        vk::ImageSubresourceRange sub_resource_range;
        sub_resource_range.setAspectMask(vk::ImageAspectFlagBits::eColor)
            .setBaseMipLevel(0)
            .setLevelCount(1)
            .setBaseArrayLayer(0)
            .setLayerCount(1);

        vk::ImageMemoryBarrier2 present_pre_barrier;
        present_pre_barrier.setOldLayout(vk::ImageLayout::eUndefined)
            .setNewLayout(vk::ImageLayout::eColorAttachmentOptimal)
            .setSrcAccessMask(vk::AccessFlagBits2::eNone)
            .setDstAccessMask(vk::AccessFlagBits2::eColorAttachmentWrite)
            .setSrcStageMask(vk::PipelineStageFlagBits2::eNone)
            .setDstStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput)
            .setSubresourceRange(sub_resource_range);

        vk::ImageMemoryBarrier2 present_post_barrier;
        present_post_barrier.setOldLayout(present_pre_barrier.newLayout)
            .setNewLayout(vk::ImageLayout::ePresentSrcKHR)
            .setSrcAccessMask(present_pre_barrier.dstAccessMask)
            .setDstAccessMask(vk::AccessFlagBits2::eNone)
            .setSrcStageMask(present_pre_barrier.dstStageMask)
            .setDstStageMask(vk::PipelineStageFlagBits2::eBottomOfPipe)
            .setSubresourceRange(sub_resource_range);

        vk::ImageMemoryBarrier2 msaa_pre_barrier;
        msaa_pre_barrier.setImage(msaa->get())
            .setOldLayout(vk::ImageLayout::eUndefined)
            .setNewLayout(vk::ImageLayout::eColorAttachmentOptimal)
            .setSrcAccessMask(vk::AccessFlagBits2::eNone)
            .setDstAccessMask(vk::AccessFlagBits2::eColorAttachmentWrite)
            .setSrcStageMask(vk::PipelineStageFlagBits2::eNone)
            .setDstStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput)
            .setSubresourceRange(sub_resource_range);

        vk::DependencyInfo dependency_info;

        vk::SemaphoreSubmitInfo signal_semaphore_info;
        vk::SemaphoreSubmitInfo wait_semaphore_info;
        vk::CommandBufferSubmitInfo command_submit_info;
        command_submit_info.setCommandBuffer(vkal_command->get(0)).setDeviceMask(1);
        wait_semaphore_info.setSemaphore(semaphore).setStageMask(
            vk::PipelineStageFlagBits2::eColorAttachmentOutput);
        vk::SubmitInfo2 submit_info;
        submit_info.setCommandBufferInfos(command_submit_info)
            .setWaitSemaphoreInfos(wait_semaphore_info)
            .setSignalSemaphoreInfos(signal_semaphore_info);

        vkal_surface->set_resize_callback([&msaa, &vkal_device, &memory_allocator,
                                           &msaa_pre_barrier,
                                           &rendering_attachment](vk::Extent2D extent) {
            if (extent != vk::Extent2D(msaa->get_extent().width, msaa->get_extent().height)) {
                msaa = vkal::image_ptr(vkal::ImageParams{
                    .vkal_device = *vkal_device,
                    .memory_allocator = *memory_allocator,
                    .type = vk::ImageType::e2D,
                    .view_type = vk::ImageViewType::e2D,
                    .extent = vk::Extent3D(extent, 1),
                    .format = vk::Format::eB8G8R8A8Srgb,
                    .sample_count = vk::SampleCountFlagBits::e4,
                    .aspects = vk::ImageAspectFlagBits::eColor,
                    .mip_levels = 1,
                    .usage = vk::ImageUsageFlagBits::eColorAttachment,
                    .memory_properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
                });
                msaa_pre_barrier.setImage(msaa->get());
                rendering_attachment.setImageView(msaa->get_view());
            }
        });

        // Main loop
        SDL_Event event;
        bool running = true;
        while (running) {
            while (SDL_PollEvent(&event)) {
                switch (event.type) {
                case SDL_EVENT_QUIT:
                    running = false;
                case SDL_EVENT_KEY_DOWN:
                    switch (event.key.key) {
                    case SDLK_ESCAPE:
                        running = false;
                    }
                }
            }

            float t = SDL_GetTicks() / 1000.0f;
            uniform.rotation = glm::rotate(glm::mat4(1.0f), t, glm::vec3(0.0f, 0.0f, 1.0f));
            uniform_buffer->upload(&uniform, sizeof(Uniform));

            vk::Result wait_result = vkal_device->get().waitForFences(fence, true, UINT64_MAX);
            if (wait_result != vk::Result::eSuccess) {
                throw std::runtime_error(
                    std::format("Failed to wait for fence due to {}", vk::to_string(wait_result)));
            }

            auto image_opt = vkal_surface->acquire_next_frame(semaphore);
            if (!image_opt.has_value()) {
                continue;
            }
            vkal_device->get().resetFences(fence);
            vkal::Image& image = image_opt.value();
            rendering_info.setRenderArea(
                vk::Rect2D(vk::Offset2D(0, 0),
                           vk::Extent2D(image.get_extent().width, image.get_extent().height)));
            rendering_attachment.setResolveImageView(image.get_view());
            present_pre_barrier.setImage(image.get());
            present_post_barrier.setImage(image.get());
            std::array<vk::ImageMemoryBarrier2, 2> pre_barriers = {present_pre_barrier,
                                                                   msaa_pre_barrier};
            dependency_info.setImageMemoryBarriers(pre_barriers);
            signal_semaphore_info.setSemaphore(vkal_surface->get_current_semaphore());

            command.begin(vk::CommandBufferBeginInfo());
            command.pipelineBarrier2(dependency_info);
            command.beginRendering(rendering_info);

            // Begin render
            command.setViewport(0, viewport);
            command.setScissor(0, scissor);
            command.bindPipeline(vkal_graphics_pipeline->get_bind_point(),
                                 vkal_graphics_pipeline->get());
            command.bindDescriptorSets(vkal_graphics_pipeline->get_bind_point(),
                                       vkal_graphics_pipeline->get_layout().get(), 0,
                                       vkal_descriptor_set->get(0), {});
            command.bindVertexBuffers(0, vertex_buffer->get(), {0});
            command.bindIndexBuffer(index_buffer->get(), 0, vk::IndexType::eUint32);
            command.drawIndexed(3, 1, 0, 0, 0);
            // End render

            command.endRendering();
            dependency_info.setImageMemoryBarriers(present_post_barrier);
            command.pipelineBarrier2(dependency_info);
            command.end();

            queue.submit2(submit_info, fence);

            vkal_surface->present();
        }

        vkal_device->get().waitIdle();
        vkal_device->get().destroySemaphore(semaphore);
        vkal_device->get().destroyFence(fence);

        SDL_Quit();
    } catch (const std::exception& e) {
        std::println("ERROR  | {}", e.what());
    }

    return 0;
}
