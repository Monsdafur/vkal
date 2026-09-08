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

#include "vkal-helper/utilities.hpp"

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

class VertexColorPass : public vkal::RenderPass {
  public:
    virtual void setup_metadata(
        const std::unordered_map<std::string, std::reference_wrapper<vkal::Buffer>>& buffers,
        const std::unordered_map<std::string, std::reference_wrapper<vkal::Image>>& images,
        std::optional<std::reference_wrapper<vkal::Pipeline>> pipeline_opt,
        std::optional<std::reference_wrapper<vkal::DescriptorSet>> descriptor_sets_opt) override {
        this->vertex_buffer = &buffers.at("vertex buffer").get();
        this->index_buffer = &buffers.at("index buffer").get();
        if (pipeline_opt.has_value()) {
            this->pipeline = &pipeline_opt->get();
            if (descriptor_sets_opt.has_value()) {
                this->descriptor_sets = &descriptor_sets_opt->get();
            }
        }
    }

    virtual void render(vk::CommandBuffer command) override {
        this->viewport.x = 0.0f;
        this->viewport.y = 0.0f;
        this->viewport.width = static_cast<float>(WINDOW_EXTENT.width);
        this->viewport.height = static_cast<float>(WINDOW_EXTENT.height);
        this->viewport.minDepth = 0.0f;
        this->viewport.maxDepth = 1.0f;
        this->scissor.offset.x = 0;
        this->scissor.offset.y = 0;
        this->scissor.extent = WINDOW_EXTENT;
        command.setViewport(0, this->viewport);
        command.setScissor(0, this->scissor);

        vkal::Pipeline& graphics_pipeline = *pipeline;
        command.bindPipeline(graphics_pipeline.get_bind_point(), graphics_pipeline.get());
        vk::DescriptorSet set = descriptor_sets->get(0);
        command.bindDescriptorSets2(vk::BindDescriptorSetsInfo(
            vk::ShaderStageFlagBits::eVertex, graphics_pipeline.get_layout().get(), 0, 1, &set, 0));
        command.bindVertexBuffers2(0, vertex_buffer->get(), {0});
        command.bindIndexBuffer2(index_buffer->get(), 0, index_buffer->get_size(),
                                 vk::IndexType::eUint32);
        command.drawIndexed(3, 1, 0, 0, 0);
    }

  private:
    vk::Viewport viewport;
    vk::Rect2D scissor;
    vkal::Buffer* vertex_buffer;
    vkal::Buffer* index_buffer;
    vkal::Pipeline* pipeline;
    vkal::DescriptorSet* descriptor_sets;
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
        vkal::RenderResourcesPtr render_resources =
            vkal::render_resources_ptr(vkal::RenderResourcesParams{
                .vkal_device = *vkal_device,
                .memory_allocator = *memory_allocator,
            });

        // Create descriptor layout
        render_resources->create_descriptor_layout(vkal::DescriptorLayoutResourceParams{
            .identifier = "default descriptor layout",
            .bindings =
                {
                    vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1,
                                                   vk::ShaderStageFlagBits::eVertex),
                },
            .binding_flags = {vk::DescriptorBindingFlags()},
        });

        // Create graphcis pipeline
        render_resources->create_pipeline_layout(vkal::PipelineLayoutResourceParams{
            .identifier = "default pipeline layout",
            .descriptor_layout_identifiers = {"default descriptor layout"},
        });

        vkal::Pipeline& vkal_graphics_pipeline =
            render_resources->create_graphics_pipeline(vkal::GraphicsPipelineResourceParams{
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
        vkal::Buffer& vertex_buffer = add_device_local_buffer(
            queue, command, *vkal_device, *render_resources, "vertex buffer",
            sizeof(Vertex) * vertices.size(), vk::BufferUsageFlagBits::eVertexBuffer,
            vk::MemoryPropertyFlags(), vertices.data());

        // Create index buffer
        vkal::Buffer& index_buffer = add_device_local_buffer(
            queue, command, *vkal_device, *render_resources, "index buffer",
            sizeof(uint32_t) * indices.size(), vk::BufferUsageFlagBits::eIndexBuffer,
            vk::MemoryPropertyFlags(), indices.data());

        // Create uniform buffer
        vkal::Buffer& uniform_buffer = render_resources->create_buffer(vkal::BufferResourceParams{
            .identifier = "uniform buffer",
            .size = sizeof(Uniform),
            .usage = vk::BufferUsageFlagBits::eUniformBuffer,
            .memory_properties = vk::MemoryPropertyFlagBits::eHostVisible |
                                 vk::MemoryPropertyFlagBits::eHostCoherent,
        });

        // Create MSAA texture
        vkal::ImageResourceParams msaa_params = {
            .identifier = "msaa",
            .type = vk::ImageType::e2D,
            .view_type = vk::ImageViewType::e2D,
            .extent = vk::Extent3D(WINDOW_EXTENT, 1),
            .format = vk::Format::eB8G8R8A8Srgb,
            .sample_count = vk::SampleCountFlagBits::e4,
            .aspects = vk::ImageAspectFlagBits::eColor,
            .mip_levels = 1,
            .usage = vk::ImageUsageFlagBits::eColorAttachment |
                     vk::ImageUsageFlagBits::eTransientAttachment,
            .memory_properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
        };
        render_resources->create_image(msaa_params);

        // Render graph
        vkal::RenderGraphPtr render_graph = vkal::render_graph_ptr(vkal::RenderGraphParams{
            .vkal_device = *vkal_device,
            .vkal_surface = *vkal_surface,
            .render_resources = *render_resources,
            .vkal_descriptor = *vkal_descriptor,
        });

        {
            std::vector<vkal::BufferResourceDescription> resrouces_descriptions;
            resrouces_descriptions.push_back(vkal::BufferResourceDescription{
                .identifier = "vertex buffer",
            });
            resrouces_descriptions.push_back(vkal::BufferResourceDescription{
                .identifier = "index buffer",
            });
            resrouces_descriptions.push_back(vkal::BufferResourceDescription{
                .identifier = "uniform buffer",
                .barrier =
                    vkal::ResourceBarrier{
                        .access = vk::AccessFlagBits2::eShaderRead,
                        .stage = vk::PipelineStageFlagBits2::eVertexShader,
                    },
                .resource_descriptor =
                    vkal::ResourceDescriptor{
                        .type = vk::DescriptorType::eUniformBuffer,
                        .set = 0,
                        .binding = 0,
                    },
            });

            std::vector<vkal::ImageResourceDescription> image_resrouces_descriptions;
            image_resrouces_descriptions.push_back(vkal::ImageResourceDescription{
                .identifier = "msaa",
                .barrier =
                    vkal::ResourceBarrier{
                        .preserve = false,
                        .layout = vk::ImageLayout::eColorAttachmentOptimal,
                        .access = vk::AccessFlagBits2::eColorAttachmentWrite,
                        .stage = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                    },
            });

            std::vector<vkal::RenderAttachmentParams> render_attachments = {
                vkal::RenderAttachmentParams{
                    .type = vkal::RenderAttachmentType::SWAPCHAIN,
                    .identifier = "main attachment",
                    .image = "msaa",
                    .resolve_mode = vk::ResolveModeFlagBits::eAverage,
                    .clear_value = vk::ClearValue(
                        vk::ClearColorValue(std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f})),
                    .load_op = vk::AttachmentLoadOp::eClear,
                    .store_op = vk::AttachmentStoreOp::eStore,
                },
            };

            vkal::RenderPassParams pass_params{
                .identifier = "final pass",
                .buffer_resources = resrouces_descriptions,
                .image_resources = image_resrouces_descriptions,
                .render_attachments = render_attachments,
                .pipeline = "graphics pipeline",
                .pass = std::make_unique<VertexColorPass>(),
            };
            render_graph->add_pass(std::move(pass_params));
        }

        render_graph->compile();

        vkal_surface->set_resize_callback(
            [&render_graph, &render_resources,
             &msaa_params](const vk::SurfaceCapabilitiesKHR& surface_capabilities) {
                vk::Extent3D extent = vk::Extent3D(surface_capabilities.currentExtent, 1);
                msaa_params.extent = extent;
                render_resources->create_image(msaa_params);
                render_graph->reset_swapchain_images();
                render_graph->rebind_resources();
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
