#include "pipeline_layout.hpp"

namespace vkal {

///////////////////////////////////////////////////////////
PipelineLayout::PipelineLayout(const PipelineLayoutParams& params)
    : vkal_device(params.vkal_device), vkal_descriptor_layouts(params.vkal_descriptor_layouts) {
    std::vector<vk::DescriptorSetLayout> descriptor_layouts;
    for (DescriptorLayout& layout : this->vkal_descriptor_layouts) {
        descriptor_layouts.push_back(layout.get());
    }
    vk::PipelineLayoutCreateInfo pipeline_layout_create_info;
    pipeline_layout_create_info.setSetLayouts(descriptor_layouts)
        .setPushConstantRanges(params.push_constants);

    this->layout = this->vkal_device.get().createPipelineLayout(pipeline_layout_create_info);
}

///////////////////////////////////////////////////////////
PipelineLayout::~PipelineLayout() {
    this->vkal_device.get().destroyPipelineLayout(this->layout);
}

///////////////////////////////////////////////////////////
vk::PipelineLayout PipelineLayout::get() {
    return this->layout;
}

///////////////////////////////////////////////////////////
std::vector<std::reference_wrapper<DescriptorLayout>>& PipelineLayout::get_descriptor_layouts() {
    return this->vkal_descriptor_layouts;
}

} // namespace vkal
