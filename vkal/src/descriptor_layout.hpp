#pragma once

#include "device.hpp"

#include <memory>

namespace vkal {

struct DescriptorLayoutParams {
    Device& vkal_device;
    std::vector<vk::DescriptorSetLayoutBinding> bindings;
    std::vector<vk::DescriptorBindingFlags> binding_flags;
};

class DescriptorLayout {
  public:
    // No copy and mo move
    DescriptorLayout(const DescriptorLayout&) = delete;
    DescriptorLayout& operator=(const DescriptorLayout&) = delete;
    DescriptorLayout(DescriptorLayout&&) = delete;
    DescriptorLayout& operator=(DescriptorLayout&&) = delete;

    explicit DescriptorLayout(const DescriptorLayoutParams& params);

    ~DescriptorLayout();

    vk::DescriptorSetLayout get();

    const std::vector<vk::DescriptorSetLayoutBinding>& get_bindings() const;

  private:
    Device& vkal_device;
    std::vector<vk::DescriptorSetLayoutBinding> bindings;
    vk::DescriptorSetLayout layout;
};

using DescriptorLayoutPtr = std::unique_ptr<DescriptorLayout>;

inline DescriptorLayoutPtr descriptor_layout_ptr(const DescriptorLayoutParams& params) {
    return std::make_unique<DescriptorLayout>(params);
}

} // namespace vkal
