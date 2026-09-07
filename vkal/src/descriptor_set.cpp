#include "descriptor_set.hpp"
#include "common.hpp"

#include <ranges>

namespace vkal {

///////////////////////////////////////////////////////////
DescriptorSet::DescriptorSet(const DescriptorSetParams& params)
    : vkal_device(params.vkal_device), vkal_descriptor_layouts(params.vkal_layouts),
      vkal_descriptor(params.vkal_descriptor),
      pool_info(this->vkal_descriptor.get_pool(this->vkal_descriptor_layouts)) {
    std::vector<vk::DescriptorSetLayout> descriptor_layouts;
    for (DescriptorLayout& layout : this->vkal_descriptor_layouts) {
        descriptor_layouts.push_back(layout.get());
    }

    vk::DescriptorSetAllocateInfo set_allocate_info;
    set_allocate_info.setDescriptorPool(this->pool_info.pool.pool)
        .setSetLayouts(descriptor_layouts);
    vk::DescriptorSetVariableDescriptorCountAllocateInfo variable_descriptor_set_info;
    if (params.enable_dynamic_sized_array) {
        variable_descriptor_set_info.setDescriptorSetCount(this->vkal_descriptor_layouts.size())
            .setDescriptorCounts(params.dynamic_array_size);
        set_allocate_info.pNext = variable_descriptor_set_info;
    }

#if defined(ENABLE_DEBUG)
    debug("Descriptor changed...");
    this->vkal_descriptor.dump();
#endif

    this->sets = this->vkal_device.get().allocateDescriptorSets(set_allocate_info);
}

///////////////////////////////////////////////////////////
DescriptorSet::~DescriptorSet() {
    this->vkal_device.get().freeDescriptorSets(this->pool_info.pool.pool, this->sets);
    for (const auto& [index, pool_index] :
         std::ranges::views::enumerate(this->pool_info.pool_size_indices)) {
        this->pool_info.pool.pool_sizes[pool_index].descriptorCount +=
            this->pool_info.pool_sizes[index].descriptorCount;
    }
    this->pool_info.pool.remaining_sets++;
    this->vkal_descriptor.clean(this->pool_info.pool);

#if defined(ENABLE_DEBUG)
    debug("Descriptor changed...");
    this->vkal_descriptor.dump();
#endif
}

///////////////////////////////////////////////////////////
vk::DescriptorSet DescriptorSet::get(uint32_t index) {
    return this->sets.at(index);
}

///////////////////////////////////////////////////////////
void DescriptorSet::write_buffer(const BufferWriteParams& write_params) {
    std::vector<vk::DescriptorBufferInfo> descriptor_buffer_infos;
    for (Buffer& buffer : write_params.buffers) {
        vk::DescriptorBufferInfo descriptor_buffer_info;
        descriptor_buffer_info.setBuffer(buffer.get()).setOffset(0).setRange(buffer.get_size());
        descriptor_buffer_infos.push_back(descriptor_buffer_info);
    }
    vk::WriteDescriptorSet write_descriptor_set;
    write_descriptor_set.setBufferInfo(descriptor_buffer_infos)
        .setDescriptorCount(descriptor_buffer_infos.size())
        .setDstArrayElement(write_params.first_element)
        .setDescriptorType(write_params.type)
        .setDstSet(this->sets[write_params.set_index])
        .setDstBinding(write_params.binding);

    this->vkal_device.get().updateDescriptorSets(write_descriptor_set, {});
}

///////////////////////////////////////////////////////////
void DescriptorSet::write_sampler(const SamplerWriteParams& write_params) {
    if (write_params.images.empty() && write_params.samplers.empty()) {
        throw std::runtime_error(
            "Cannot write to descriptor as both image and sampler are invalid");
    }

    // Enumerate through both images and sampler
    std::vector<vk::DescriptorImageInfo> descriptor_image_infos;
    for (size_t i = 0; i < std::max(write_params.images.size(), write_params.samplers.size());
         ++i) {
        vk::DescriptorImageInfo descriptor_image_info;
        descriptor_image_info.setImageLayout(write_params.layout);
        if (i < write_params.images.size()) {
            descriptor_image_info.setImageView(write_params.images[i].get().get_view());
        }
        if (i < write_params.samplers.size()) {
            descriptor_image_info.setSampler(write_params.samplers[i].get().get());
        }
        descriptor_image_infos.push_back(descriptor_image_info);
    }

    vk::WriteDescriptorSet write_descriptor_set;
    write_descriptor_set.setImageInfo(descriptor_image_infos)
        .setDescriptorCount(descriptor_image_infos.size())
        .setDstArrayElement(write_params.first_element)
        .setDescriptorType(write_params.type)
        .setDstSet(this->sets[write_params.set_index])
        .setDstBinding(write_params.binding);
    this->vkal_device.get().updateDescriptorSets(write_descriptor_set, {});
}

} // namespace vkal
