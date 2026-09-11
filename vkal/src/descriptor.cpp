#include "descriptor.hpp"
#include "common.hpp"

#include <ranges>

namespace vkal {
///////////////////////////////////////////////////////////
static std::vector<vk::DescriptorPoolSize> collect_descriptor_pool_sizes(
    const std::vector<std::reference_wrapper<DescriptorLayout>>& layouts) {
    std::vector<vk::DescriptorPoolSize> layout_pool_sizes;
    for (DescriptorLayout& layout : layouts) {
        const std::vector<vk::DescriptorSetLayoutBinding>& bindings = layout.get_bindings();
        for (const vk::DescriptorSetLayoutBinding& binding : bindings) {
            bool found = false;
            for (vk::DescriptorPoolSize& pool_size : layout_pool_sizes) {
                if (pool_size.type == binding.descriptorType) {
                    pool_size.descriptorCount += binding.descriptorCount;
                    found = true;
                    break;
                }
            }
            if (!found) {
                layout_pool_sizes.push_back(
                    vk::DescriptorPoolSize(binding.descriptorType, binding.descriptorCount));
            }
        }
    }

    return layout_pool_sizes;
}

///////////////////////////////////////////////////////////
std::vector<size_t>
match_descriptor_pool_sizes(const std::vector<vk::DescriptorPoolSize>& layout_pool_sizes,
                            const std::vector<vk::DescriptorPoolSize>& pool_sizes) {
    // Match for any descriptor pool
    std::vector<size_t> pool_indices;
    for (const vk::DescriptorPoolSize& layout_pool_size : layout_pool_sizes) {
        auto it = std::ranges::find_if(
            pool_sizes, [&layout_pool_size](const vk::DescriptorPoolSize& pool_size) {
                return layout_pool_size.type == pool_size.type &&
                       layout_pool_size.descriptorCount <= pool_size.descriptorCount;
            });

        if (it == pool_sizes.end()) {
            return pool_indices;
        }
        pool_indices.push_back(std::distance(pool_sizes.begin(), it));
    }

    return pool_indices;
}

///////////////////////////////////////////////////////////
Descriptor::Descriptor(const DescriptorParams& params)
    : device(params.device), max_sets(params.max_sets), pool_size(params.pool_size) {
}

///////////////////////////////////////////////////////////
Descriptor::~Descriptor() {
    for (const auto& pool : this->descriptor_pool_datas) {
        this->device.get().destroyDescriptorPool(pool->vk_descriptor_pool);
    }
}

///////////////////////////////////////////////////////////
DescriptorPoolInfo
Descriptor::get_pool(const std::vector<std::reference_wrapper<DescriptorLayout>>& layouts) {
    // Collect all descriptor pool sizes
    std::vector<vk::DescriptorPoolSize> layout_pool_sizes = collect_descriptor_pool_sizes(layouts);

    for (const std::unique_ptr<DescriptorPoolData>& descriptor_pool_data :
         this->descriptor_pool_datas) {
        std::vector<size_t> pool_indices =
            match_descriptor_pool_sizes(layout_pool_sizes, descriptor_pool_data->pool_sizes);
        if (pool_indices.size() != layout_pool_sizes.size() ||
            descriptor_pool_data->remaining_sets == 0) {
            continue;
        }

        // If a matching descriptor pool is found decrease each pool size
        for (size_t i = 0; i < layout_pool_sizes.size(); ++i) {
            descriptor_pool_data->pool_sizes[pool_indices[i]].descriptorCount -=
                layout_pool_sizes[i].descriptorCount;
        }
        descriptor_pool_data->remaining_sets--;

        return DescriptorPoolInfo{
            .descriptor_pool_data = *descriptor_pool_data,
            .pool_size_indices = pool_indices,
            .pool_sizes = layout_pool_sizes,
        };
    }

    // If no suitable descriptor pool is found then create a new one
    // Create a current pool size list with full capacity
    std::vector<vk::DescriptorPoolSize> current_pool_sizes;
    for (vk::DescriptorPoolSize& pool_size : layout_pool_sizes) {
        if (pool_size.descriptorCount > this->pool_size) {
            throw std::runtime_error(std::format("Pool size {} for descriptor type {} exceeds "
                                                 "designated descroptor pool size limit {}",
                                                 pool_size.descriptorCount,
                                                 vk::to_string(pool_size.type), this->pool_size));
        }

        // A set is being allocated so each pool size have to be shrunken to match the current set
        current_pool_sizes.push_back(
            vk::DescriptorPoolSize(pool_size.type, this->pool_size - pool_size.descriptorCount));
    }

    // Setup descriptor pool create info
    vk::DescriptorPoolCreateInfo descriptor_pool_create_info;
    descriptor_pool_create_info.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
        .setMaxSets(this->max_sets)
        .setPoolSizes(current_pool_sizes);

    vk::DescriptorPool descriptor_pool =
        this->device.get().createDescriptorPool(descriptor_pool_create_info);
    this->descriptor_pool_datas.push_back(std::make_unique<DescriptorPoolData>(DescriptorPoolData{
        .vk_descriptor_pool = descriptor_pool,
        .remaining_sets =
            this->max_sets - 1, // A set is being allocated so the max sets must be decreased by 1
        .pool_sizes = current_pool_sizes,
    }));

    // A new pool is created to accomodate the allocated set so all pool indices matches the set
    // pool indices
    std::vector<size_t> pool_indices(layout_pool_sizes.size());
    for (const auto& [index, pool_index] : std::ranges::views::enumerate(pool_indices)) {
        pool_index = index;
    }

    return DescriptorPoolInfo{
        .descriptor_pool_data = *this->descriptor_pool_datas.back(),
        .pool_size_indices = pool_indices,
        .pool_sizes = layout_pool_sizes,
    };
}

///////////////////////////////////////////////////////////
void Descriptor::clean(DescriptorPoolData& descriptor_pool_data) {
    // Remaining sets matches max sets means this pool no longer contain any sets and will be
    // removed
    if (descriptor_pool_data.remaining_sets == this->max_sets) {
        for (size_t i = 0; i < this->descriptor_pool_datas.size(); ++i) {
            if (this->descriptor_pool_datas[i].get() == &descriptor_pool_data) {
                this->device.get().destroyDescriptorPool(
                    this->descriptor_pool_datas[i]->vk_descriptor_pool);
                this->descriptor_pool_datas.erase(this->descriptor_pool_datas.begin() + i);
                return;
            }
        }
        throw std::runtime_error("Pool does not belong to descriptor");
    }
}

///////////////////////////////////////////////////////////
void Descriptor::dump() {
    size_t index = 0;
    for (const auto& pool : this->descriptor_pool_datas) {
        std::string remaining_sets_string(this->max_sets, '+');
        for (size_t i = 0; i < pool->remaining_sets; ++i) {
            remaining_sets_string[this->max_sets - i - 1] = '-';
        }
        debug(std::format("Pool {}:", index++));
        debug(std::format("Sets: {}", remaining_sets_string));
        for (const vk::DescriptorPoolSize& pool_size : pool->pool_sizes) {
            std::string pool_size_string(this->pool_size, '+');
            for (size_t i = 0; i < pool_size.descriptorCount; ++i) {
                pool_size_string[this->pool_size - i - 1] = '-';
            }
            debug(std::format("Type {}: {}", vk::to_string(pool_size.type), pool_size_string));
        }
    }
}

} // namespace vkal
