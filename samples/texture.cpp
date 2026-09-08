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
#include <random>
#include <ranges>
#include <stdexcept>

constexpr uint32_t SPRITE_COUNT = 1000;

float random_range(std::mt19937& generator, float min, float max) {
    std::uniform_real_distribution<float> distribution(min, max);
    return distribution(generator);
}

float random_range(std::mt19937& generator, int32_t min, int32_t max) {
    std::uniform_int_distribution<int32_t> distribution(min, max);
    return distribution(generator);
}

struct Vertex {
    glm::vec3 position;
    glm::vec2 uv;
};

struct Uniform {
    glm::mat4 projection;
    glm::mat4 view;
};

struct Sprite {
    glm::vec3 position;
    uint32_t pad0;
    glm::vec3 scale;
    uint32_t pad1;
    float rotation;
    uint32_t texture_index;
    uint32_t pad2[2];
};

class CopyPass : public vkal::RenderPass {
  public:
    virtual void setup_metadata(
        const std::unordered_map<std::string, std::reference_wrapper<vkal::Buffer>>& buffers,
        const std::unordered_map<std::string, std::reference_wrapper<vkal::Image>>& images,
        std::optional<std::reference_wrapper<vkal::Pipeline>> pipeline_opt,
        std::optional<std::reference_wrapper<vkal::DescriptorSet>> descriptor_sets_opt) override {
        this->sprite_staging_buffer = &buffers.at("staging sprite buffer").get();
        this->sprite_buffer = &buffers.at("sprite buffer").get();
    }

    virtual void render(vk::CommandBuffer command) override {
        vk::BufferCopy2 region;
        region.setSrcOffset(0).setSrcOffset(0).setSize(sprite_buffer->get_size());
        vk::CopyBufferInfo2 copy_info;
        copy_info.setSrcBuffer(this->sprite_staging_buffer->get())
            .setDstBuffer(this->sprite_buffer->get())
            .setRegions(region);

        command.copyBuffer2(copy_info);
    }

  private:
    vkal::Buffer* sprite_staging_buffer;
    vkal::Buffer* sprite_buffer;
};

class TexturePass : public vkal::RenderPass {
  public:
    TexturePass(const vk::Extent2D& extent) {
        this->viewport.x = 0.0f;
        this->viewport.y = 0.0f;
        this->viewport.width = static_cast<float>(extent.width);
        this->viewport.height = static_cast<float>(extent.height);
        this->viewport.minDepth = 0.0f;
        this->viewport.maxDepth = 1.0f;
        this->scissor.offset.x = 0;
        this->scissor.offset.y = 0;
        this->scissor.extent = extent;
    }

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
        command.drawIndexed(6, SPRITE_COUNT, 0, 0, 0);
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
        std::random_device random_device;
        std::mt19937 generator(random_device());

        // Create Window
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            throw std::runtime_error(SDL_GetError());
        }

        SDL_Window* window =
            SDL_CreateWindow("Texture", 1920, 1080, SDL_WINDOW_VULKAN | SDL_WINDOW_FULLSCREEN);
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
        vk::Extent2D window_extent = vkal_surface->get_capabilities().currentExtent;

        // Create memory allocator
        vkal::MemoryAllocatorPtr memory_allocator =
            vkal::memory_allocator_ptr(vkal::MemoryAllocatorParams{
                .vkal_device = *vkal_device,
                .block_size = vkal::megabytes(32),
            });

        // Craete descriptor
        vkal::DescriptorPtr vkal_descriptor = vkal::descriptor_ptr(vkal::DescriptorParams{
            .vkal_device = *vkal_device,
            .max_sets = 100,
            .pool_size = 100,
        });

        // Create graphics resource manager
        vkal::RenderResourcesPtr render_resources =
            vkal::render_resources_ptr(vkal::RenderResourcesParams{
                .vkal_device = *vkal_device,
                .memory_allocator = *memory_allocator,
            });

        // Create descriptor layout
        render_resources->create_descriptor_layout(vkal::DescriptorLayoutResourceParams{
            .identifier = "texture map descriptor layout",
            .bindings =
                {
                    vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1,
                                                   vk::ShaderStageFlagBits::eVertex),
                    vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eStorageBuffer, 1,
                                                   vk::ShaderStageFlagBits::eVertex),
                    vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eSampler, 1,
                                                   vk::ShaderStageFlagBits::eFragment),
                    vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eSampledImage, 10,
                                                   vk::ShaderStageFlagBits::eFragment),
                },
            .binding_flags =
                {
                    vk::DescriptorBindingFlags(),
                    vk::DescriptorBindingFlags(),
                    vk::DescriptorBindingFlags(),
                    vk::DescriptorBindingFlagBits::eVariableDescriptorCount |
                        vk::DescriptorBindingFlagBits::ePartiallyBound,
                },
        });

        // Create pipeline layouts
        render_resources->create_pipeline_layout(vkal::PipelineLayoutResourceParams{
            .identifier = "texture map pipeline layout",
            .descriptor_layout_identifiers = {"texture map descriptor layout"},
        });

        // Craete pipelines
        render_resources->create_graphics_pipeline(vkal::GraphicsPipelineResourceParams{
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
            .color_attachment_formats = {vk::Format::eB8G8R8A8Srgb},
            .rasterization_sample_count = vk::SampleCountFlagBits::e4,
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

        // Load images
        std::map<std::string, std::reference_wrapper<vkal::Image>> image_map =
            load_images(queue, command, *vkal_device, *render_resources,
                        {
                            "resources/textures/256x256/Bricks/Bricks_01-256x256.png",
                            "resources/textures/256x256/Bricks/Bricks_02-256x256.png",
                            "resources/textures/256x256/Bricks/Bricks_03-256x256.png",
                            "resources/textures/256x256/Bricks/Bricks_04-256x256.png",
                            "resources/textures/256x256/Bricks/Bricks_05-256x256.png",
                            "resources/textures/256x256/Bricks/Bricks_06-256x256.png",
                            "resources/textures/256x256/Bricks/Bricks_07-256x256.png",
                            "resources/textures/256x256/Bricks/Bricks_08-256x256.png",
                            "resources/textures/256x256/Bricks/Bricks_09-256x256.png",
                            "resources/textures/256x256/Bricks/Bricks_10-256x256.png",
                        });

        render_resources->create_sampler(vkal::SamplerResourceParams{
            .identifier = "basic sampler",
            .address_mode = vk::SamplerAddressMode::eRepeat,
            .filter = vk::Filter::eLinear,
            .min_lod = 0.0f,
            .max_lod = static_cast<float>(9),
            .mip_map_mode = vk::SamplerMipmapMode::eLinear,
            .enable_anisotropy = true,
            .max_anisotropy = 4.0f,
        });

        render_resources->create_sampler(vkal::SamplerResourceParams{
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
            float width = static_cast<float>(window_extent.width);
            float height = static_cast<float>(window_extent.height);
            uniform.projection = glm::ortho(0.0f, width, 0.0f, height, -1.0f, 1.0f);
            uniform.view = glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f),
                                       glm::vec3(0.0f, 1.0f, 0.0f));
        }

        std::array<Sprite, SPRITE_COUNT> sprites;
        std::array<float, SPRITE_COUNT> angular_velocities;
        for (const auto& [index, sprite] : std::ranges::views::enumerate(sprites)) {
            sprite.position.x = random_range(generator, 0.0f, 1920.0f);
            sprite.position.y = random_range(generator, 0.0f, 1080.0f);
            sprite.position.z = 0.0f;
            float scale = random_range(generator, 25.0f, 100.0f);
            sprite.scale = glm::vec3(scale, scale, 1.0f);
            sprite.rotation = random_range(generator, 0.0f, 6.28f);
            sprite.texture_index = random_range(generator, 0, 9);
            angular_velocities[index] = random_range(generator, -2.0f, 2.0f);
        }

        // Create vertex buffer
        add_device_local_buffer(queue, command, *vkal_device, *render_resources, "vertex buffer",
                                sizeof(Vertex) * vertices.size(),
                                vk::BufferUsageFlagBits::eVertexBuffer, vk::MemoryPropertyFlags(),
                                vertices.data());

        // Create index buffer
        add_device_local_buffer(queue, command, *vkal_device, *render_resources, "index buffer",
                                sizeof(uint32_t) * indices.size(),
                                vk::BufferUsageFlagBits::eIndexBuffer, vk::MemoryPropertyFlags(),
                                indices.data());

        // Create uniform buffer
        vkal::Buffer& uniform_buffer = render_resources->create_buffer(vkal::BufferResourceParams{
            .identifier = "uniform buffer",
            .size = sizeof(Uniform),
            .usage = vk::BufferUsageFlagBits::eUniformBuffer,
            .memory_properties = vk::MemoryPropertyFlagBits::eHostVisible |
                                 vk::MemoryPropertyFlagBits::eHostCoherent,
        });

        // Create sprite device local buffer
        render_resources->create_buffer(vkal::BufferResourceParams{
            .identifier = "sprite buffer",
            .size = sizeof(Sprite) * sprites.size(),
            .usage =
                vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer,
            .memory_properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
        });

        // Create sprite host buffer
        vkal::Buffer& staging_sprite_buffer =
            render_resources->create_buffer(vkal::BufferResourceParams{
                .identifier = "staging sprite buffer",
                .size = sizeof(Sprite) * sprites.size(),
                .usage = vk::BufferUsageFlagBits::eTransferSrc,
                .memory_properties = vk::MemoryPropertyFlagBits::eHostVisible |
                                     vk::MemoryPropertyFlagBits::eHostCoherent,
            });

        // Create color attachments
        vkal::ImageResourceParams msaa_params = {
            .identifier = "msaa",
            .type = vk::ImageType::e2D,
            .view_type = vk::ImageViewType::e2D,
            .extent = vk::Extent3D(window_extent, 1),
            .format = vk::Format::eB8G8R8A8Srgb,
            .sample_count = vk::SampleCountFlagBits::e4,
            .aspects = vk::ImageAspectFlagBits::eColor,
            .mip_levels = 1,
            .usage = vk::ImageUsageFlagBits::eColorAttachment |
                     vk::ImageUsageFlagBits::eTransientAttachment,
            .memory_properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
        };
        render_resources->create_image(msaa_params);

        // Create render graph
        vkal::RenderGraphPtr render_graph = vkal::render_graph_ptr(vkal::RenderGraphParams{
            .vkal_device = *vkal_device,
            .vkal_surface = *vkal_surface,
            .render_resources = *render_resources,
            .vkal_descriptor = *vkal_descriptor,
        });

        // Copy pass
        {
            std::vector<vkal::BufferResourceDescription> buffer_resrouces_descriptions;
            buffer_resrouces_descriptions.push_back(vkal::BufferResourceDescription{
                .identifier = "staging sprite buffer",
                .barrier =
                    vkal::ResourceBarrier{
                        .access = vk::AccessFlagBits2::eTransferRead,
                        .stage = vk::PipelineStageFlagBits2::eTransfer,
                    },
            });
            buffer_resrouces_descriptions.push_back(vkal::BufferResourceDescription{
                .identifier = "sprite buffer",
                .barrier =
                    vkal::ResourceBarrier{
                        .access = vk::AccessFlagBits2::eTransferWrite,
                        .stage = vk::PipelineStageFlagBits2::eTransfer,
                    },
            });

            vkal::RenderPassParams pass_params{
                .identifier = "copy pass",
                .buffer_resources = buffer_resrouces_descriptions,
                .pass = std::make_unique<CopyPass>(),
            };
            render_graph->add_pass(std::move(pass_params));
        }

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
            buffer_resrouces_descriptions.push_back(vkal::BufferResourceDescription{
                .identifier = "sprite buffer",
                .barrier =
                    vkal::ResourceBarrier{
                        .access = vk::AccessFlagBits2::eShaderRead,
                        .stage = vk::PipelineStageFlagBits2::eVertexShader,
                    },
                .resource_descriptor =
                    vkal::ResourceDescriptor{
                        .type = vk::DescriptorType::eStorageBuffer,
                        .set = 0,
                        .binding = 1,
                    },
            });

            std::vector<vkal::ImageResourceDescription> image_resrouces_descriptions;
            size_t index = 0;
            for (const auto& entry : image_map) {
                image_resrouces_descriptions.push_back(vkal::ImageResourceDescription{
                    .identifier = entry.first,
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
                            .binding = 3,
                            .array_index = static_cast<uint32_t>(index),
                        },
                });
                index++;
            }

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

            std::vector<vkal::SamplerResourceDescription> sampler_resouces_description;
            sampler_resouces_description.push_back(vkal::SamplerResourceDescription{
                .identifier = "basic sampler",
                .set = 0,
                .binding = 2,
            });

            std::vector<vkal::RenderAttachmentParams> render_attachments;
            render_attachments.push_back(vkal::RenderAttachmentParams{
                .type = vkal::RenderAttachmentType::SWAPCHAIN,
                .identifier = "main attachment",
                .image = "msaa",
                .resolve_mode = vk::ResolveModeFlagBits::eAverage,
                .clear_value = vk::ClearValue(
                    vk::ClearColorValue(std::array<float, 4>{0.01f, 0.01f, 0.01f, 1.0f})),
                .load_op = vk::AttachmentLoadOp::eClear,
                .store_op = vk::AttachmentStoreOp::eStore,
            });

            std::unique_ptr<TexturePass> texture_pass_ptr =
                std::make_unique<TexturePass>(window_extent);
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

        render_graph->compile();

        vkal_surface->set_resize_callback(
            [&render_resources, &render_graph, &uniform, &texture_pass,
             &msaa_params](const vk::SurfaceCapabilitiesKHR& surface_capabilities) {
                vk::Extent3D extent = vk::Extent3D(surface_capabilities.currentExtent, 1);
                msaa_params.extent = extent;

                render_resources->create_image(msaa_params);

                render_graph->reset_swapchain_images();
                render_graph->rebind_resources();
                float width = static_cast<float>(extent.width);
                float height = static_cast<float>(extent.height);
                uniform.projection = glm::ortho(0.0f, width, 0.0f, height, -1.0f, 1.0f);
                texture_pass->set_viewport_size(width, height);
                texture_pass->set_scissor_size(vk::Extent2D(extent.width, extent.height));
            });

        // Main loop
        float previous_time;
        float delta_time;
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

            float current_time = SDL_GetTicks() / 1000.0f;
            delta_time = current_time - previous_time;
            previous_time = current_time;

            uniform_buffer.upload(&uniform, sizeof(Uniform));
            staging_sprite_buffer.upload(sprites.data(), sizeof(Sprite) * sprites.size());
            for (const auto& [index, sprite] : std::ranges::views::enumerate(sprites)) {
                sprite.rotation += angular_velocities[index] * delta_time;
            }

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
