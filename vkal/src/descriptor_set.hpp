#pragma once

#include "buffer.hpp"
#include "descriptor.hpp"
#include "descriptor_layout.hpp"
#include "device.hpp"
#include "image.hpp"
#include "sampler.hpp"

#include <cstdint>
#include <memory>

namespace vkal {

struct BufferWriteParams {
    std::vector<std::reference_wrapper<Buffer>> buffers;
    size_t set_index;
    vk::DescriptorType type;
    uint32_t binding;
    uint32_t first_element = 0;
};

struct SamplerWriteParams {
    std::vector<std::reference_wrapper<Sampler>> samplers;
    std::vector<std::reference_wrapper<Image>> images;
    vk::ImageLayout layout;
    size_t set_index;
    vk::DescriptorType type;
    uint32_t binding;
    uint32_t first_element;
};

struct DescriptorSetParams {
    Device& vkal_device;
    std::vector<std::reference_wrapper<DescriptorLayout>> vkal_layouts;
    Descriptor& vkal_descriptor;
    bool enable_dynamic_sized_array = false;
    uint32_t dynamic_array_size = 1;
};

class DescriptorSet {
  public:
    // No copy and mo move
    DescriptorSet(const DescriptorSet&) = delete;
    DescriptorSet& operator=(const DescriptorSet&) = delete;
    DescriptorSet(DescriptorSet&&) = delete;
    DescriptorSet& operator=(DescriptorSet&&) = delete;

    explicit DescriptorSet(const DescriptorSetParams& params);

    ~DescriptorSet();

    vk::DescriptorSet get(uint32_t index);

    void write_buffer(const BufferWriteParams& write_params);

    void write_sampler(const SamplerWriteParams& write_params);

  private:
    // Pravate methods
    vk::DescriptorSet allocate_sets(bool enable_dynamic_sized_array, uint32_t array_size);

    // Members
    Device& vkal_device;
    std::vector<std::reference_wrapper<DescriptorLayout>> vkal_descriptor_layouts;
    Descriptor& vkal_descriptor;
    DescriptorPoolInfo pool_info;
    std::vector<vk::DescriptorSet> sets;
};

using DescriptorSetPtr = std::unique_ptr<DescriptorSet>;

inline DescriptorSetPtr descriptor_set_ptr(const DescriptorSetParams& params) {
    return std::make_unique<DescriptorSet>(params);
}

} // namespace vkal
