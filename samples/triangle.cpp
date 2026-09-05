#include "../vkal/src/buffer.hpp"
#include "../vkal/src/command.hpp"
#include "../vkal/src/common.hpp"
#include "../vkal/src/descriptor.hpp"
#include "../vkal/src/device.hpp"
#include "../vkal/src/instance.hpp"
#include "../vkal/src/pipeline.hpp"
#include "../vkal/src/render_graph.hpp"
#include "../vkal/src/render_resources.hpp"
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

void pre_render(vk::Viewport& viewport, vk::Rect2D& scissor) {
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(WINDOW_EXTENT.width);
    viewport.height = static_cast<float>(WINDOW_EXTENT.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent = WINDOW_EXTENT;
}

void render(vk::CommandBuffer command,
            const std::unordered_map<std::string, std::reference_wrapper<vkal::Buffer>>& buffers,
            const std::unordered_map<std::string, std::reference_wrapper<vkal::Image>>& images,
            std::optional<std::reference_wrapper<vkal::Pipeline>> pipeline,
            vkal::DescriptorSet& descriptor_sets) {
    if (!pipeline.has_value()) {
        return;
    }
    vkal::Buffer& vertex_buffer = buffers.at("vertex buffer");
    vkal::Buffer& index_buffer = buffers.at("index buffer");

    vkal::Pipeline& graphics_pipeline = *pipeline;
    command.bindPipeline(graphics_pipeline.get_bind_point(), graphics_pipeline.get());
    vk::DescriptorSet set = descriptor_sets.get(0);
    command.bindDescriptorSets2(vk::BindDescriptorSetsInfo(
        vk::ShaderStageFlagBits::eVertex, graphics_pipeline.get_layout().get(), 0, 1, &set, 0));
    command.bindVertexBuffers2(0, vertex_buffer.get(), {0});
    command.bindIndexBuffer2(index_buffer.get(), 0, index_buffer.get_size(),
                             vk::IndexType::eUint32);
    command.drawIndexed(3, 1, 0, 0, 0);
}

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
                    vk::PresentModeKHR::eImmediate,
                    vk::PresentModeKHR::eMailbox,
                },
        });

        // Create memory allocator
        vkal::MemoryAllocatorPtr memory_allocator =
            vkal::memory_allocator_ptr(vkal::MemoryAllocatorParams{
                .vkal_device = *vkal_device,
                .block_size = vkal::megabytes(128),
            });

        // Craete descriptor
        vkal::DescriptorPtr vkal_descriptor = vkal::descriptor_ptr(vkal::DescriptorParams{
            .vkal_device = *vkal_device,
            .max_sets = 10,
            .pool_size = 10,
        });

        // Create graphics resource manager
        vkal::RenderResources render_resources(vkal::RenderResourcesParams{
            .vkal_device = *vkal_device,
            .memory_allocator = *memory_allocator,
        });

        // Create descriptor layout
        render_resources.create_descriptor_layout(vkal::DescriptorLayoutResourceParams{
            .identifier = "default descriptor layout",
            .bindings =
                {
                    vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1,
                                                   vk::ShaderStageFlagBits::eVertex),
                },
            .binding_flags = {vk::DescriptorBindingFlags()},
        });

        // Create graphcis pipeline
        render_resources.create_pipeline_layout(vkal::PipelineLayoutResourceParams{
            .identifier = "default pipeline layout",
            .descriptor_layout_identifiers = {"default descriptor layout"},
        });

        vkal::Pipeline& vkal_graphics_pipeline =
            render_resources.create_graphics_pipeline(vkal::GraphicsPipelineResourceParams{
                .identifier = "graphics pipeline",
                .layout_identifier = "default pipeline layout",
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
                .rasterization_sample_count = vk::SampleCountFlagBits::e1,
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
        render_resources.create_buffer(vkal::BufferResourceParams{
            .identifier = "vertex buffer",
            .size = sizeof(Vertex) * vertices.size(),
            .usage = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
            .memory_properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
        });
        vkal::Buffer& vertex_buffer = render_resources.get_buffer("vertex buffer");

        // Create index buffer
        render_resources.create_buffer(vkal::BufferResourceParams{
            .identifier = "index buffer",
            .size = sizeof(uint32_t) * indices.size(),
            .usage = vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
            .memory_properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
        });
        vkal::Buffer& index_buffer = render_resources.get_buffer("index buffer");

        // Craete uniform buffer
        render_resources.create_buffer(vkal::BufferResourceParams{
            .identifier = "uniform buffer",
            .size = sizeof(Uniform),
            .usage = vk::BufferUsageFlagBits::eUniformBuffer,
            .memory_properties = vk::MemoryPropertyFlagBits::eHostVisible |
                                 vk::MemoryPropertyFlagBits::eHostCoherent,
        });
        vkal::Buffer& uniform_buffer = render_resources.get_buffer("uniform buffer");

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
            region.setSrcOffset(0).setDstOffset(0).setSize(vertex_buffer.get_size());
            vk::CopyBufferInfo2 copy_buffer_info;
            copy_buffer_info.setSrcBuffer(staging0->get())
                .setDstBuffer(vertex_buffer.get())
                .setRegions(region);

            command.copyBuffer2(copy_buffer_info);
            region.setSize(index_buffer.get_size());
            copy_buffer_info.setSrcBuffer(staging1->get()).setDstBuffer(index_buffer.get());
            command.copyBuffer2(copy_buffer_info);

            command.end();

            vk::CommandBufferSubmitInfo command_submit_info;
            command_submit_info.setCommandBuffer(command).setDeviceMask(1);
            vk::SubmitInfo2 submit_info;
            submit_info.setCommandBufferInfos(command_submit_info);
            queue.submit2(submit_info);
            queue.waitIdle();
        }

        // Render graph
        vkal::RenderGraphPtr render_graph = vkal::render_graph_ptr(vkal::RenderGraphParams{
            .vkal_device = *vkal_device,
            .vkal_surface = *vkal_surface,
            .render_resources = render_resources,
            .vkal_descriptor = *vkal_descriptor,
        });

        {
            std::vector<vkal::ResourceDescription> resrouces_descriptions;
            resrouces_descriptions.push_back(vkal::ResourceDescription{
                .type = vkal::ResourceDescription::Type::BUFFER,
                .identifier = "vertex buffer",
            });
            resrouces_descriptions.push_back(vkal::ResourceDescription{
                .type = vkal::ResourceDescription::Type::BUFFER,
                .identifier = "index buffer",
            });
            resrouces_descriptions.push_back(vkal::ResourceDescription{
                .type = vkal::ResourceDescription::Type::BUFFER,
                .identifier = "uniform buffer",
                .barrier =
                    vkal::ResourceBarrier{
                        .access = vk::AccessFlagBits2::eShaderRead,
                        .stage = vk::PipelineStageFlagBits2::eVertexShader,
                    },
                .write_set = true,
                .set = 0,
                .binding = 0,
                .descriptor_type = vk::DescriptorType::eUniformBuffer,
            });
            render_graph->add_pass(vkal::RenderPassParams{
                .identifier = "final pass",
                .is_root = true,
                .resources = resrouces_descriptions,
                .pipeline = "graphics pipeline",
                .pre_render_callback = pre_render,
                .render_callback = render,
            });
        }

        render_graph->compile();

        vkal_surface->set_resize_callback(
            [&render_graph](vk::Extent2D extent) { render_graph->reset_swapchain_images(); });

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
            uniform_buffer.upload(&uniform, sizeof(Uniform));

            render_graph->sync();
            std::optional<uint32_t> swapchain_index_opt =
                vkal_surface->acquire_next_frame(render_graph->get_semaphore());
            if (!swapchain_index_opt.has_value()) {
                continue;
            }

            render_graph->execute(*swapchain_index_opt, queue, command,
                                  vkal_surface->get_current_semaphore());
            vkal_surface->present();
        }

        vkal_device->get().waitIdle();
        SDL_Quit();
    } catch (const std::exception& e) {
        std::println("ERROR  | {}", e.what());
    }

    return 0;
}
