#include "descriptor.hpp"

namespace vkal {

///////////////////////////////////////////////////////////
Descriptor::Descriptor(const DescriptorParams& params)
    : vkal_device(params.vkal_device), set_count(params.max_sets) {

    for (const vk::DescriptorPoolSize& pool_size : params.pool_sizes) {
        this->pool_sizes.emplace(pool_size.type, pool_size.descriptorCount);
    }

    vk::DescriptorPoolCreateInfo descriptor_pool_create_info;
    descriptor_pool_create_info.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
        .setMaxSets(this->set_count)
        .setPoolSizes(params.pool_sizes);

    this->pool = this->vkal_device.get().createDescriptorPool(descriptor_pool_create_info);
}

///////////////////////////////////////////////////////////
Descriptor::~Descriptor() {
    this->vkal_device.get().destroyDescriptorPool(this->pool);
}

///////////////////////////////////////////////////////////
vk::DescriptorPool Descriptor::get() {
    return this->pool;
}

} // namespace vkal
