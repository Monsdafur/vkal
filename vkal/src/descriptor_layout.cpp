#include "descriptor_layout.hpp"

namespace vkal {

///////////////////////////////////////////////////////////
DescriptorLayout::DescriptorLayout(const DescriptorLayoutParams& params)
    : vkal_device(params.vkal_device), bindings(params.bindings) {
    vk::DescriptorSetLayoutCreateInfo descriptor_set_layout_create_info;
    vk::DescriptorSetLayoutBindingFlagsCreateInfo binding_flags;
    if (!params.binding_flags.empty()) {
        binding_flags.setBindingFlags(params.binding_flags);
        descriptor_set_layout_create_info.setPNext(binding_flags);
    }
    descriptor_set_layout_create_info.setBindings(this->bindings);

    this->layout =
        this->vkal_device.get().createDescriptorSetLayout(descriptor_set_layout_create_info);
}

///////////////////////////////////////////////////////////
DescriptorLayout::~DescriptorLayout() {
    this->vkal_device.get().destroyDescriptorSetLayout(this->layout);
}

///////////////////////////////////////////////////////////
vk::DescriptorSetLayout DescriptorLayout::get() {
    return this->layout;
}

} // namespace vkal
