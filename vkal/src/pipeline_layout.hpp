#pragma once

#include "descriptor_layout.hpp"
#include "device.hpp"

#include <memory>

namespace vkal {

struct PipelineLayoutParams {
    Device& vkal_device;
    std::vector<std::reference_wrapper<DescriptorLayout>> vkal_descriptor_layouts;
    std::vector<vk::PushConstantRange> push_constants;
};

class PipelineLayout {
  public:
    // No copy and mo move
    PipelineLayout(const PipelineLayout&) = delete;
    PipelineLayout& operator=(const PipelineLayout&) = delete;
    PipelineLayout(PipelineLayout&&) = delete;
    PipelineLayout& operator=(PipelineLayout&&) = delete;

    explicit PipelineLayout(const PipelineLayoutParams& params);

    ~PipelineLayout();

    vk::PipelineLayout get();

    std::vector<std::reference_wrapper<DescriptorLayout>>& get_descriptor_layouts();

  private:
    vkal::Device& vkal_device;
    std::vector<std::reference_wrapper<DescriptorLayout>> vkal_descriptor_layouts;
    vk::PipelineLayout layout;
};

using PipelineLayoutPtr = std::unique_ptr<PipelineLayout>;

inline PipelineLayoutPtr pipeline_layout_ptr(const PipelineLayoutParams& params) {
    return std::make_unique<PipelineLayout>(params);
}

} // namespace vkal
