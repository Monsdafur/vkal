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

constexpr vk::Extent2D WINDOW_EXTENT = vk::Extent2D(1200, 800);

struct Vertex {
    glm::vec3 position;
    glm::vec2 uv;
};

struct Uniform {
    glm::mat4 translation;
    glm::mat4 rotation;
    glm::mat4 scale;
    glm::mat4 projection;
    glm::mat4 view;
};

class ScreenPass : public vkal::RenderPass {
  public:
    ScreenPass() {
        this->viewport.x = 0.0f;
        this->viewport.y = 0.0f;
        this->viewport.width = static_cast<float>(WINDOW_EXTENT.width);
        this->viewport.height = static_cast<float>(WINDOW_EXTENT.height);
        this->viewport.minDepth = 0.0f;
        this->viewport.maxDepth = 1.0f;
        this->scissor.offset.x = 0;
        this->scissor.offset.y = 0;
        this->scissor.extent = WINDOW_EXTENT;
    }

    virtual void setup_metadata(
        const std::unordered_map<std::string, std::reference_wrapper<vkal::Buffer>>& buffers,
        const std::unordered_map<std::string, std::reference_wrapper<vkal::Image>>& images,
        const std::unordered_map<std::string, std::reference_wrapper<vk::RenderingAttachmentInfo>>&
            render_attachments,
        std::optional<std::reference_wrapper<vkal::Pipeline>> pipeline_opt,
        std::optional<std::reference_wrapper<vkal::DescriptorSet>> descriptor_sets_opt) override {
        if (pipeline_opt.has_value()) {
            this->pipeline = &pipeline_opt->get();
            if (descriptor_sets_opt.has_value()) {
                this->descriptor_sets = &descriptor_sets_opt->get();
            }
        }
    }

    virtual void render(vk::CommandBuffer command) override {
        command.setViewport(0, this->viewport);
        command.setScissor(0, this->scissor);

        vkal::Pipeline& graphics_pipeline = *pipeline;
        command.bindPipeline(graphics_pipeline.get_bind_point(), graphics_pipeline.get());
        vk::DescriptorSet set = descriptor_sets->get(0);
        command.bindDescriptorSets2(vk::BindDescriptorSetsInfo(
            vk::ShaderStageFlagBits::eVertex, graphics_pipeline.get_layout().get(), 0, 1, &set, 0));
        command.draw(4, 1, 0, 0);
    }

    void set_viewport_size(float width, float height) {
        this->viewport.width = width;
        this->viewport.height = height;
    }

    void set_scissor_size(const vk::Extent2D& extent) {
        this->scissor.extent = extent;
    }

  private:
    vk::Viewport viewport;
    vk::Rect2D scissor;
    vkal::Pipeline* pipeline;
    vkal::DescriptorSet* descriptor_sets;
};

class TexturePass : public vkal::RenderPass {
  public:
    TexturePass() {
        this->viewport.x = 0.0f;
        this->viewport.y = 0.0f;
        this->viewport.width = static_cast<float>(WINDOW_EXTENT.width);
        this->viewport.height = static_cast<float>(WINDOW_EXTENT.height);
        this->viewport.minDepth = 0.0f;
        this->viewport.maxDepth = 1.0f;
        this->scissor.offset.x = 0;
        this->scissor.offset.y = 0;
        this->scissor.extent = WINDOW_EXTENT;
    }

    virtual void setup_metadata(
        const std::unordered_map<std::string, std::reference_wrapper<vkal::Buffer>>& buffers,
        const std::unordered_map<std::string, std::reference_wrapper<vkal::Image>>& images,
        const std::unordered_map<std::string, std::reference_wrapper<vk::RenderingAttachmentInfo>>&
            render_attachments,
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
        command.drawIndexed(6, 1, 0, 0, 0);
    }

    void set_viewport_size(float width, float height) {
        this->viewport.width = width;
        this->viewport.height = height;
    }

    void set_scissor_size(const vk::Extent2D& extent) {
        this->scissor.extent = extent;
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
        vkal::RenderResources render_resources(vkal::RenderResourcesParams{
            .vkal_device = *vkal_device,
            .memory_allocator = *memory_allocator,
        });

        // Create descriptor layout
        render_resources.create_descriptor_layout(vkal::DescriptorLayoutResourceParams{
            .identifier = "texture map descriptor layout",
            .bindings =
                {
                    vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1,
                                                   vk::ShaderStageFlagBits::eVertex),
                    vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eSampledImage, 1,
                                                   vk::ShaderStageFlagBits::eFragment),
                    vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eSampler, 1,
                                                   vk::ShaderStageFlagBits::eFragment),
                },
            .binding_flags =
                {
                    vk::DescriptorBindingFlags(),
                    vk::DescriptorBindingFlags(),
                    vk::DescriptorBindingFlags(),
                },
        });

        render_resources.create_descriptor_layout(vkal::DescriptorLayoutResourceParams{
            .identifier = "screen descriptor layout",
            .bindings =
                {
                    vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eSampledImage, 1,
                                                   vk::ShaderStageFlagBits::eFragment),
                    vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eSampler, 1,
                                                   vk::ShaderStageFlagBits::eFragment),
                },
            .binding_flags =
                {
                    vk::DescriptorBindingFlags(),
                    vk::DescriptorBindingFlags(),
                },
        });

        // Create pipeline layouts
        render_resources.create_pipeline_layout(vkal::PipelineLayoutResourceParams{
            .identifier = "texture map pipeline layout",
            .descriptor_layout_identifiers = {"texture map descriptor layout"},
        });
        render_resources.create_pipeline_layout(vkal::PipelineLayoutResourceParams{
            .identifier = "screen pipeline layout",
            .descriptor_layout_identifiers = {"screen descriptor layout"},
        });

        // Craete pipelines
        render_resources.create_graphics_pipeline(vkal::GraphicsPipelineResourceParams{
            .identifier = "texture map pipeline",
            .layout_identifier = "texture map pipeline layout",
            .shader_stages =
                {
                    vkal::ShaderStage{
                        .file_path = "resources/shaders/texture_map.spv",
                        .stage = vk::ShaderStageFlagBits::eVertex,
                    },
                    vkal::ShaderStage{
                        .file_path = "resources/shaders/texture_map.spv",
                        .stage = vk::ShaderStageFlagBits::eFragment,
                    },
                },
            .color_attachment_formats = {vk::Format::eR8G8B8A8Srgb},
            .rasterization_sample_count = vk::SampleCountFlagBits::e1,
            .vertex_input_rate = vk::VertexInputRate::eVertex,
            .vertex_stride = sizeof(Vertex),
            .vertex_descriptions =
                {
                    vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat,
                                                        offsetof(Vertex, position)),
                    vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32Sfloat,
                                                        offsetof(Vertex, uv)),
                },
            .topology = vk::PrimitiveTopology::eTriangleList,
        });

        render_resources.create_graphics_pipeline(vkal::GraphicsPipelineResourceParams{
            .identifier = "screen pipeline",
            .layout_identifier = "screen pipeline layout",
            .shader_stages =
                {
                    vkal::ShaderStage{
                        .file_path = "resources/shaders/screen.spv",
                        .stage = vk::ShaderStageFlagBits::eVertex,
                    },
                    vkal::ShaderStage{
                        .file_path = "resources/shaders/screen.spv",
                        .stage = vk::ShaderStageFlagBits::eFragment,
                    },
                },
            .color_attachment_formats = {vk::Format::eB8G8R8A8Srgb},
            .rasterization_sample_count = vk::SampleCountFlagBits::e1,
            .vertex_input_rate = vk::VertexInputRate::eVertex,
            .vertex_stride = sizeof(Vertex),
            .vertex_descriptions = {},
            .topology = vk::PrimitiveTopology::eTriangleFan,
        });

        // Load images
        load_images(queue, command, *vkal_device, render_resources,
                    {
                        "resources/textures/256x256/Bricks/Bricks_01-256x256.png",
                    });

        render_resources.create_sampler(vkal::SamplerResourceParams{
            .identifier = "basic sampler",
            .address_mode = vk::SamplerAddressMode::eRepeat,
            .filter = vk::Filter::eLinear,
            .min_lod = 0.0f,
            .max_lod = static_cast<float>(9),
            .mip_map_mode = vk::SamplerMipmapMode::eLinear,
            .enable_anisotropy = true,
            .max_anisotropy = 4.0f,
        });

        render_resources.create_sampler(vkal::SamplerResourceParams{
            .identifier = "screen sampler",
            .address_mode = vk::SamplerAddressMode::eClampToBorder,
            .filter = vk::Filter::eNearest,
            .min_lod = 0.0f,
            .max_lod = 1.0f,
            .mip_map_mode = vk::SamplerMipmapMode::eNearest,
            .enable_anisotropy = false,
        });

        // Initializing resources
        std::array<Vertex, 4> vertices = {
            Vertex{
                .position = glm::vec3(0.5f, 0.5f, 0.0f),
                .uv = glm::vec2(1.0f, 1.0f),
            },
            Vertex{
                .position = glm::vec3(-0.5f, 0.5f, 0.0f),
                .uv = glm::vec2(0.0f, 1.0f),
            },
            Vertex{
                .position = glm::vec3(-0.5f, -0.5f, 0.0f),
                .uv = glm::vec2(0.0f, 0.0f),
            },
            Vertex{
                .position = glm::vec3(0.5f, -0.5f, 0.0f),
                .uv = glm::vec2(1.0f, 0.0f),
            },
        };

        std::array<uint32_t, 6> indices = {0, 1, 2, 0, 2, 3};

        Uniform uniform;
        {
            float width = static_cast<float>(WINDOW_EXTENT.width);
            float height = static_cast<float>(WINDOW_EXTENT.height);
            uniform.projection = glm::ortho(0.0f, width, 0.0f, height, -1.0f, 1.0f);
            uniform.view = glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f),
                                       glm::vec3(0.0f, 1.0f, 0.0f));
            uniform.translation =
                glm::translate(glm::mat4(1.0f), glm::vec3(width, height, 0.0f) * 0.5f);
        }

        // Create vertex buffer
        add_device_local_buffer(queue, command, *vkal_device, render_resources, "vertex buffer",
                                sizeof(Vertex) * vertices.size(),
                                vk::BufferUsageFlagBits::eVertexBuffer, vk::MemoryPropertyFlags(),
                                vertices.data());

        // Create index buffer
        add_device_local_buffer(queue, command, *vkal_device, render_resources, "index buffer",
                                sizeof(uint32_t) * indices.size(),
                                vk::BufferUsageFlagBits::eIndexBuffer, vk::MemoryPropertyFlags(),
                                indices.data());

        // Create uniform buffer
        vkal::Buffer& uniform_buffer = render_resources.create_buffer(vkal::BufferResourceParams{
            .identifier = "uniform buffer",
            .size = sizeof(Uniform),
            .usage = vk::BufferUsageFlagBits::eUniformBuffer,
            .memory_properties = vk::MemoryPropertyFlagBits::eHostVisible |
                                 vk::MemoryPropertyFlagBits::eHostCoherent,
        });

        // Create color attachment
        render_resources.create_image(vkal::ImageResourceParams{
            .identifier = "color attachment",
            .type = vk::ImageType::e2D,
            .view_type = vk::ImageViewType::e2D,
            .extent = vk::Extent3D(WINDOW_EXTENT, 1),
            .format = vk::Format::eR8G8B8A8Srgb,
            .sample_count = vk::SampleCountFlagBits::e1,
            .aspects = vk::ImageAspectFlagBits::eColor,
            .mip_levels = 1,
            .usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
            .memory_properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
        });

        // Create render graph
        vkal::RenderGraphPtr render_graph = vkal::render_graph_ptr(vkal::RenderGraphParams{
            .vkal_device = *vkal_device,
            .vkal_surface = *vkal_surface,
            .render_resources = render_resources,
            .vkal_descriptor = *vkal_descriptor,
        });

        // Texture pass
        TexturePass* texture_pass;
        {
            std::vector<vkal::BufferResourceDescription> buffer_resrouces_descriptions;
            buffer_resrouces_descriptions.push_back(vkal::BufferResourceDescription{
                .identifier = "vertex buffer",
            });
            buffer_resrouces_descriptions.push_back(vkal::BufferResourceDescription{
                .identifier = "index buffer",
            });
            buffer_resrouces_descriptions.push_back(vkal::BufferResourceDescription{
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
                .identifier = "Bricks_01-256x256",
                .barrier =
                    vkal::ResourceBarrier{
                        .layout = vk::ImageLayout::eShaderReadOnlyOptimal,
                        .access = vk::AccessFlagBits2::eShaderSampledRead,
                        .stage = vk::PipelineStageFlagBits2::eFragmentShader,
                    },
                .resource_descriptor =
                    vkal::ResourceDescriptor{
                        .type = vk::DescriptorType::eSampledImage,
                        .set = 0,
                        .binding = 1,
                    },
            });
            image_resrouces_descriptions.push_back(vkal::ImageResourceDescription{
                .identifier = "color attachment",
                .barrier =
                    vkal::ResourceBarrier{
                        .layout = vk::ImageLayout::eColorAttachmentOptimal,
                        .access = vk::AccessFlagBits2::eColorAttachmentWrite,
                        .stage = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                    },
            });

            std::vector<vkal::SamplerResourceDescription> sampler_resouces_description;
            sampler_resouces_description.push_back(vkal::SamplerResourceDescription{
                .identifier = "basic sampler",
                .set = 0,
                .binding = 2,
            });

            std::vector<vkal::RenderAttachmentParams> render_attachments = {
                vkal::RenderAttachmentParams{
                    .identifier = "main attachment",
                    .image = "color attachment",
                    .clear_value = vk::ClearValue(
                        vk::ClearColorValue(std::array<float, 4>{1.0f, 1.0f, 1.0f, 1.0f})),
                    .load_op = vk::AttachmentLoadOp::eClear,
                    .store_op = vk::AttachmentStoreOp::eStore,
                },
            };

            std::unique_ptr<TexturePass> texture_pass_ptr = std::make_unique<TexturePass>();
            texture_pass = texture_pass_ptr.get();
            vkal::RenderPassParams pass_params{
                .identifier = "texture pass",
                .buffer_resources = buffer_resrouces_descriptions,
                .image_resources = image_resrouces_descriptions,
                .render_attachments = render_attachments,
                .sampler_resources = sampler_resouces_description,
                .pipeline = "texture map pipeline",
                .pass = std::move(texture_pass_ptr),
            };
            render_graph->add_pass(std::move(pass_params));
        }

        // Screen pass
        {
            std::vector<vkal::ImageResourceDescription> image_resrouces_descriptions;
            image_resrouces_descriptions.push_back(vkal::ImageResourceDescription{
                .identifier = "color attachment",
                .barrier =
                    vkal::ResourceBarrier{
                        .layout = vk::ImageLayout::eShaderReadOnlyOptimal,
                        .access = vk::AccessFlagBits2::eShaderSampledRead,
                        .stage = vk::PipelineStageFlagBits2::eFragmentShader,
                    },
                .resource_descriptor =
                    vkal::ResourceDescriptor{
                        .type = vk::DescriptorType::eSampledImage,
                        .set = 0,
                        .binding = 0,
                    },
            });

            std::vector<vkal::SamplerResourceDescription> sampler_resouces_description;
            sampler_resouces_description.push_back(vkal::SamplerResourceDescription{
                .identifier = "screen sampler",
                .set = 0,
                .binding = 1,
            });

            vkal::RenderPassParams pass_params{
                .identifier = "screen pass",
                .is_root = true,
                .image_resources = image_resrouces_descriptions,
                .sampler_resources = sampler_resouces_description,
                .pipeline = "screen pipeline",
                .pass = std::make_unique<ScreenPass>(),
            };
            render_graph->add_pass(std::move(pass_params));
        }

        render_graph->compile();

        vkal_surface->set_resize_callback(
            [&render_graph, &uniform, &texture_pass](vk::Extent2D extent) {
                render_graph->reset_swapchain_images();
                float width = static_cast<float>(extent.width);
                float height = static_cast<float>(extent.height);
                uniform.projection = glm::ortho(0.0f, width, 0.0f, height, -1.0f, 1.0f);
                uniform.translation =
                    glm::translate(glm::mat4(1.0f), glm::vec3(width, height, 0.0f) * 0.5f);
                texture_pass->set_viewport_size(width, height);
                texture_pass->set_scissor_size(extent);
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
            uniform.scale = glm::scale(glm::mat4(1.0f), glm::vec3(256.0f, 256.0f, 1.0f));
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
