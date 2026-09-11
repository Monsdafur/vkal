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

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "vkal-helper/utilities.hpp"

#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <exception>
#include <print>
#include <random>
#include <ranges>
#include <stdexcept>

constexpr uint32_t OBJECT_COUNT = 4000;
constexpr float PI = std::numbers::pi;

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
    glm::vec3 normal;
    glm::vec2 uv;
};

struct Uniform {
    glm::mat4 projection;
    glm::mat4 view;
    glm::vec3 light_direction;
    float diffuse;
    float ambient;
    uint32_t pad1[2];
};

struct Object {
    glm::mat4 model;
    glm::mat4 inv_model;
    uint32_t texture_index;
    uint32_t pad[3];
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
        command.bindDescriptorSets(this->pipeline->get_bind_point(),
                                   this->pipeline->get_layout().get(), 0, set, {});
        command.bindVertexBuffers(0, this->vertex_buffer->get(), {0});
        command.bindIndexBuffer(this->index_buffer->get(), 0, vk::IndexType::eUint32);
        command.drawIndexed(36, OBJECT_COUNT, 0, 0, 0);
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

        SDL_Window* window = SDL_CreateWindow("Texture", 800, 600, SDL_WINDOW_VULKAN);
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
        const std::vector<vk::Format>& depth_formats =
            vkal_device->get_supported_depth_formats(vk::ImageTiling::eOptimal);
        auto depth_it = std::ranges::find_if(
            depth_formats, [](vk::Format format) { return format == vk::Format::eD32Sfloat; });
        vk::Format depth_format =
            depth_it != depth_formats.end() ? *depth_it : depth_formats.front();

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
                    vk::PresentModeKHR::eImmediate,
                    vk::PresentModeKHR::eFifo,
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
                                                   vk::ShaderStageFlagBits::eVertex |
                                                       vk::ShaderStageFlagBits::eFragment),
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
            .depth_format = depth_format,
            .rasterization_sample_count = vk::SampleCountFlagBits::e4,
            .vertex_input_rate = vk::VertexInputRate::eVertex,
            .vertex_stride = sizeof(Vertex),
            .vertex_descriptions =
                {
                    vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat,
                                                        offsetof(Vertex, position)),
                    vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32B32Sfloat,
                                                        offsetof(Vertex, normal)),
                    vk::VertexInputAttributeDescription(2, 0, vk::Format::eR32G32Sfloat,
                                                        offsetof(Vertex, uv)),
                },
            .topology = vk::PrimitiveTopology::eTriangleList,
            .front_face = vk::FrontFace::eCounterClockwise,
            .cull_mode = vk::CullModeFlagBits::eBack,
            .polygon_mode = vk::PolygonMode::eFill,
            .enable_depth_test = true,
            .enable_depth_write = true,
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

        // Initializing resources
        std::array<Vertex, 24> vertices = {
            // Z-
            Vertex{
                .position = glm::vec3(0.5f, 0.5f, -0.5f),
                .normal = glm::vec3(0.0f, 0.0f, -1.0f),
                .uv = glm::vec2(1.0f, 1.0f),
            },
            Vertex{
                .position = glm::vec3(-0.5f, 0.5f, -0.5f),
                .normal = glm::vec3(0.0f, 0.0f, -1.0f),
                .uv = glm::vec2(0.0f, 1.0f),
            },
            Vertex{
                .position = glm::vec3(-0.5f, -0.5f, -0.5f),
                .normal = glm::vec3(0.0f, 0.0f, -1.0f),
                .uv = glm::vec2(0.0f, 0.0f),
            },
            Vertex{
                .position = glm::vec3(0.5f, -0.5f, -0.5f),
                .normal = glm::vec3(0.0f, 0.0f, -1.0f),
                .uv = glm::vec2(1.0f, 0.0f),
            },

            // Z+
            Vertex{
                .position = glm::vec3(-0.5f, 0.5f, 0.5f),
                .normal = glm::vec3(0.0f, 0.0f, 1.0f),
                .uv = glm::vec2(0.0f, 1.0f),
            },
            Vertex{
                .position = glm::vec3(0.5f, 0.5f, 0.5f),
                .normal = glm::vec3(0.0f, 0.0f, 1.0f),
                .uv = glm::vec2(1.0f, 1.0f),
            },
            Vertex{
                .position = glm::vec3(0.5f, -0.5f, 0.5f),
                .normal = glm::vec3(0.0f, 0.0f, 1.0f),
                .uv = glm::vec2(1.0f, 0.0f),
            },
            Vertex{
                .position = glm::vec3(-0.5f, -0.5f, 0.5f),
                .normal = glm::vec3(0.0f, 0.0f, 1.0f),
                .uv = glm::vec2(0.0f, 0.0f),
            },

            // X-
            Vertex{
                .position = glm::vec3(-0.5f, 0.5f, -0.5f),
                .normal = glm::vec3(-1.0f, 0.0f, 0.0f),
                .uv = glm::vec2(1.0f, 1.0f),
            },
            Vertex{
                .position = glm::vec3(-0.5f, 0.5f, 0.5f),
                .normal = glm::vec3(-1.0f, 0.0f, 0.0f),
                .uv = glm::vec2(0.0f, 1.0f),
            },
            Vertex{
                .position = glm::vec3(-0.5f, -0.5f, 0.5f),
                .normal = glm::vec3(-1.0f, 0.0f, 0.0f),
                .uv = glm::vec2(0.0f, 0.0f),
            },
            Vertex{
                .position = glm::vec3(-0.5f, -0.5f, -0.5f),
                .normal = glm::vec3(-1.0f, 0.0f, 0.0f),
                .uv = glm::vec2(1.0f, 0.0f),
            },

            // X+
            Vertex{
                .position = glm::vec3(0.5f, 0.5f, 0.5f),
                .normal = glm::vec3(1.0f, 0.0f, 0.0f),
                .uv = glm::vec2(1.0f, 1.0f),
            },
            Vertex{
                .position = glm::vec3(0.5f, 0.5f, -0.5f),
                .normal = glm::vec3(1.0f, 0.0f, 0.0f),
                .uv = glm::vec2(0.0f, 1.0f),
            },
            Vertex{
                .position = glm::vec3(0.5f, -0.5f, -0.5f),
                .normal = glm::vec3(1.0f, 0.0f, 0.0f),
                .uv = glm::vec2(0.0f, 0.0f),
            },
            Vertex{
                .position = glm::vec3(0.5f, -0.5f, 0.5f),
                .normal = glm::vec3(1.0f, 0.0f, 0.0f),
                .uv = glm::vec2(1.0f, 0.0f),
            },

            // Y-
            Vertex{
                .position = glm::vec3(-0.5f, -0.5f, -0.5f),
                .normal = glm::vec3(0.0f, -1.0f, 0.0f),
                .uv = glm::vec2(0.0f, 1.0f),
            },
            Vertex{
                .position = glm::vec3(-0.5f, -0.5f, 0.5f),
                .normal = glm::vec3(0.0f, -1.0f, 0.0f),
                .uv = glm::vec2(0.0f, 0.0f),
            },
            Vertex{
                .position = glm::vec3(0.5f, -0.5f, 0.5f),
                .normal = glm::vec3(0.0f, -1.0f, 0.0f),
                .uv = glm::vec2(1.0f, 0.0f),
            },
            Vertex{
                .position = glm::vec3(0.5f, -0.5f, -0.5f),
                .normal = glm::vec3(0.0f, -1.0f, 0.0f),
                .uv = glm::vec2(1.0f, 1.0f),
            },

            // Y+
            Vertex{
                .position = glm::vec3(-0.5f, 0.5f, 0.5f),
                .normal = glm::vec3(0.0f, 1.0f, 0.0f),
                .uv = glm::vec2(0.0f, 1.0f),
            },
            Vertex{
                .position = glm::vec3(-0.5f, 0.5f, -0.5f),
                .normal = glm::vec3(0.0f, 1.0f, 0.0f),
                .uv = glm::vec2(0.0f, 0.0f),
            },
            Vertex{
                .position = glm::vec3(0.5f, 0.5f, -0.5f),
                .normal = glm::vec3(0.0f, 1.0f, 0.0f),
                .uv = glm::vec2(1.0f, 0.0f),
            },
            Vertex{
                .position = glm::vec3(0.5f, 0.5f, 0.5f),
                .normal = glm::vec3(0.0f, 1.0f, 0.0f),
                .uv = glm::vec2(1.0f, 1.0f),
            },
        };

        std::array<uint32_t, 36> indices = {
            // Z-
            0,
            2,
            1,
            0,
            3,
            2,

            // Z+
            4,
            6,
            5,
            4,
            7,
            6,

            // X-
            8,
            10,
            9,
            8,
            11,
            10,

            // X+
            12,
            14,
            13,
            12,
            15,
            14,

            // Y-
            16,
            18,
            17,
            16,
            19,
            18,

            // Y+
            20,
            22,
            21,
            20,
            23,
            22,
        };

        Uniform uniform;
        {
            float fwidth = static_cast<float>(window_extent.width);
            float fheight = static_cast<float>(window_extent.height);
            uniform.projection =
                glm::perspective(glm::radians(80.0f), fwidth / fheight, 0.1f, 100.0f);
            uniform.view = glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f),
                                       glm::vec3(0.0f, 1.0f, 0.0f));
            uniform.light_direction = glm::vec3(0.5f, -1.0f, 0.25f);
            uniform.light_direction = glm::normalize(uniform.light_direction);
            uniform.diffuse = 1.0f;
            uniform.ambient = 0.07f;
        }

        std::array<Object, OBJECT_COUNT> objects;
        for (const auto& [index, object] : std::ranges::views::enumerate(objects)) {
            glm::vec3 position;
            position.x = random_range(generator, -50.0f, 50.0f);
            position.y = random_range(generator, -50.0f, 50.0f);
            position.z = random_range(generator, -50.0f, 50.0f);

            float scale_factor = random_range(generator, 1.0f, 2.0f);
            glm::vec3 scale = glm::vec3(scale_factor);

            glm::vec3 axis;
            axis.x = random_range(generator, -1.0f, 1.0f);
            axis.y = random_range(generator, -1.0f, 1.0f);
            axis.z = random_range(generator, -1.0f, 1.0f);
            axis = glm::normalize(axis);

            float angle = random_range(generator, 0.0f, PI * 2.0f);

            object.model = glm::translate(glm::mat4(1.0f), position);
            object.model = glm::rotate(object.model, angle, axis);
            object.model = glm::scale(object.model, scale);
            object.inv_model = glm::inverse(object.model);
            object.texture_index = random_range(generator, 0, 9);
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

        // Create object buffer
        add_device_local_buffer(queue, command, *vkal_device, *render_resources, "object buffer",
                                sizeof(Object) * objects.size(),
                                vk::BufferUsageFlagBits::eStorageBuffer, vk::MemoryPropertyFlags(),
                                objects.data());

        // Create uniform buffer
        vkal::Buffer& uniform_buffer = render_resources->create_buffer(vkal::BufferResourceParams{
            .identifier = "uniform buffer",
            .size = sizeof(Uniform),
            .usage = vk::BufferUsageFlagBits::eUniformBuffer,
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

        // Create depth attachments
        vk::ImageAspectFlags depth_aspect = vk::ImageAspectFlagBits::eDepth;
        if (depth_format == vk::Format::eD16UnormS8Uint ||
            depth_format == vk::Format::eD24UnormS8Uint ||
            depth_format == vk::Format::eD32SfloatS8Uint) {
            depth_aspect |= vk::ImageAspectFlagBits::eStencil;
        }
        vkal::ImageResourceParams depth_params = {
            .identifier = "depth",
            .type = vk::ImageType::e2D,
            .view_type = vk::ImageViewType::e2D,
            .extent = vk::Extent3D(window_extent, 1),
            .format = depth_format,
            .sample_count = vk::SampleCountFlagBits::e4,
            .aspects = depth_aspect,
            .mip_levels = 1,
            .usage = vk::ImageUsageFlagBits::eDepthStencilAttachment |
                     vk::ImageUsageFlagBits::eTransientAttachment,
            .memory_properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
        };
        render_resources->create_image(depth_params);

        // Create render graph
        vkal::RenderGraphPtr render_graph = vkal::render_graph_ptr(vkal::RenderGraphParams{
            .vkal_device = *vkal_device,
            .vkal_surface = *vkal_surface,
            .render_resources = *render_resources,
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
            buffer_resrouces_descriptions.push_back(vkal::BufferResourceDescription{
                .identifier = "object buffer",
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
            image_resrouces_descriptions.push_back(vkal::ImageResourceDescription{
                .identifier = "depth",
                .barrier =
                    vkal::ResourceBarrier{
                        .preserve = false,
                        .layout = vk::ImageLayout::eDepthAttachmentOptimal,
                        .access = vk::AccessFlagBits2::eDepthStencilAttachmentRead |
                                  vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
                        .stage = vk::PipelineStageFlagBits2::eEarlyFragmentTests |
                                 vk::PipelineStageFlagBits2::eLateFragmentTests,
                    },
            });

            std::vector<vkal::SamplerResourceDescription> sampler_resouces_description;
            sampler_resouces_description.push_back(vkal::SamplerResourceDescription{
                .identifier = "basic sampler",
                .set = 0,
                .binding = 2,
            });

            std::vector<vkal::RenderAttachmentDescription> render_attachments;
            render_attachments.push_back(vkal::RenderAttachmentDescription{
                .type = vkal::RenderAttachmentType::SWAPCHAIN,
                .identifier = "main attachment",
                .image = "msaa",
                .resolve_mode = vk::ResolveModeFlagBits::eAverage,
                .clear_value = vk::ClearValue(
                    vk::ClearColorValue(std::array<float, 4>{0.01f, 0.01f, 0.01f, 1.0f})),
                .load_op = vk::AttachmentLoadOp::eClear,
                .store_op = vk::AttachmentStoreOp::eStore,
            });
            render_attachments.push_back(vkal::RenderAttachmentDescription{
                .type = vkal::RenderAttachmentType::DEPTH,
                .identifier = "depth attachment",
                .image = "depth",
                .clear_value = vk::ClearValue(vk::ClearDepthStencilValue(1.0f, 0)),
                .load_op = vk::AttachmentLoadOp::eClear,
                .store_op = vk::AttachmentStoreOp::eDontCare,
            });

            std::unique_ptr<TexturePass> texture_pass_ptr =
                std::make_unique<TexturePass>(window_extent);
            texture_pass = texture_pass_ptr.get();
            vkal::RenderPassDescription pass_description{
                .identifier = "texture pass",
                .buffer_resources = buffer_resrouces_descriptions,
                .image_resources = image_resrouces_descriptions,
                .render_attachments = render_attachments,
                .sampler_resources = sampler_resouces_description,
                .pipeline = "texture map pipeline",
            };
            render_graph->add_pass(std::move(texture_pass_ptr), std::move(pass_description));
        }

        render_graph->compile();

        vkal_surface->set_resize_callback(
            [&render_resources, &render_graph, &uniform, &texture_pass, &msaa_params,
             &depth_params](const vk::Extent2D& old_extent, const vk::Extent2D& new_extent) {
                vk::Extent3D extent = vk::Extent3D(new_extent, 1);
                msaa_params.extent = extent;
                depth_params.extent = extent;

                render_resources->create_image(msaa_params);
                render_resources->create_image(depth_params);

                render_graph->rebind_resources();

                float fwidth = static_cast<float>(extent.width);
                float fheight = static_cast<float>(extent.height);
                uniform.projection = glm::perspective(PI / 3.0f, fwidth / fheight, 0.1f, 100.0f);

                texture_pass->set_viewport_size(fwidth, fheight);
                texture_pass->set_scissor_size(vk::Extent2D(extent.width, extent.height));
            });

        // Main loop
        float previous_time;
        float delta_time;
        SDL_Event event;
        bool running = true;
        glm::vec3 camera_position(0.0f);
        float speed_scale = 0.1f;
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
            glm::vec3 look_at_vector(cosf(current_time * speed_scale), 0.0f,
                                     sinf(current_time * speed_scale));
            uniform.view = glm::lookAt(camera_position, camera_position + look_at_vector,
                                       glm::vec3(0.0f, 1.0f, 0.0f));

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
