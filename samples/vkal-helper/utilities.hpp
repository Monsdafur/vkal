#pragma once

#include "../../vkal/src/render_resources.hpp"

vkal::Buffer& add_device_local_buffer(vk::Queue queue, vk::CommandBuffer command,
                                      vkal::Device& device, vkal::RenderResources& render_resources,
                                      const std::string& identifier, vk::DeviceSize size,
                                      vk::BufferUsageFlags usage,
                                      vk::MemoryPropertyFlags memory_properties, void* data);

std::vector<std::reference_wrapper<vkal::Image>>
load_images(vk::Queue queue, vk::CommandBuffer command, vkal::Device& device,
            vkal::RenderResources& render_resources,
            const std::vector<std::filesystem::path>& paths);
