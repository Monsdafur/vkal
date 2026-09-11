#include "descriptor_layout.hpp"

namespace vkal {

///////////////////////////////////////////////////////////
DescriptorLayout::DescriptorLayout(const DescriptorLayoutParams& params)
    : device(params.device), bindings(params.bindings) {
    vk::DescriptorSetLayoutCreateInfo descriptor_set_layout_create_info;
    vk::DescriptorSetLayoutBindingFlagsCreateInfo binding_flags;
    if (!params.binding_flags.empty()) {
        binding_flags.setBindingFlags(params.binding_flags);
        descriptor_set_layout_create_info.setPNext(binding_flags);
    }
    descriptor_set_layout_create_info.setBindings(this->bindings);

    this->vk_layout =
        this->device.get().createDescriptorSetLayout(descriptor_set_layout_create_info);
}

///////////////////////////////////////////////////////////
DescriptorLayout::~DescriptorLayout() {
    this->device.get().destroyDescriptorSetLayout(this->vk_layout);
}

///////////////////////////////////////////////////////////
vk::DescriptorSetLayout DescriptorLayout::get() {
    return this->vk_layout;
}

///////////////////////////////////////////////////////////
const std::vector<vk::DescriptorSetLayoutBinding>& DescriptorLayout::get_bindings() const {
    return this->bindings;
}

} // namespace vkal
