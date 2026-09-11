#include "pipeline_layout.hpp"

namespace vkal {

///////////////////////////////////////////////////////////
PipelineLayout::PipelineLayout(const PipelineLayoutParams& params)
    : device(params.device), descriptor_layouts(params.descriptor_layouts) {
    std::vector<vk::DescriptorSetLayout> descriptor_layouts;
    for (DescriptorLayout& layout : this->descriptor_layouts) {
        descriptor_layouts.push_back(layout.get());
    }
    vk::PipelineLayoutCreateInfo pipeline_layout_create_info;
    pipeline_layout_create_info.setSetLayouts(descriptor_layouts)
        .setPushConstantRanges(params.push_constants);

    this->vk_layout = this->device.get().createPipelineLayout(pipeline_layout_create_info);
}

///////////////////////////////////////////////////////////
PipelineLayout::~PipelineLayout() {
    this->device.get().destroyPipelineLayout(this->vk_layout);
}

///////////////////////////////////////////////////////////
vk::PipelineLayout PipelineLayout::get() {
    return this->vk_layout;
}

///////////////////////////////////////////////////////////
std::vector<std::reference_wrapper<DescriptorLayout>>& PipelineLayout::get_descriptor_layouts() {
    return this->descriptor_layouts;
}

} // namespace vkal
