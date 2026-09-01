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
    : vkal_device(params.vkal_device), max_sets(params.max_sets), pool_size(params.pool_size) {
}

///////////////////////////////////////////////////////////
Descriptor::~Descriptor() {
    for (const auto& pool : this->pools) {
        this->vkal_device.get().destroyDescriptorPool(pool->pool);
    }
}

///////////////////////////////////////////////////////////
DescriptorPoolInfo
Descriptor::get_pool(const std::vector<std::reference_wrapper<DescriptorLayout>>& layouts) {
    // Collect all descriptor pool sizes
    std::vector<vk::DescriptorPoolSize> layout_pool_sizes = collect_descriptor_pool_sizes(layouts);

    for (const auto& pool : this->pools) {
        std::vector<size_t> pool_indices =
            match_descriptor_pool_sizes(layout_pool_sizes, pool->pool_sizes);
        if (pool_indices.size() != layout_pool_sizes.size() || pool->remaining_sets == 0) {
            continue;
        }

        // If a matching descriptor pool is found descrease each pool size
        for (size_t i = 0; i < layout_pool_sizes.size(); ++i) {
            pool->pool_sizes[pool_indices[i]].descriptorCount -=
                layout_pool_sizes[i].descriptorCount;
        }
        pool->remaining_sets--;

        return DescriptorPoolInfo{
            .pool = *pool,
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
        current_pool_sizes.push_back(vk::DescriptorPoolSize(pool_size.type, this->pool_size));
    }

    // Setup descriptor pool create info
    vk::DescriptorPoolCreateInfo descriptor_pool_create_info;
    descriptor_pool_create_info.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
        .setMaxSets(this->max_sets)
        .setPoolSizes(current_pool_sizes);

    // Update descriptor pool sizes
    for (size_t i = 0; i < layout_pool_sizes.size(); ++i) {
        current_pool_sizes[i].descriptorCount -= layout_pool_sizes[i].descriptorCount;
    }

    vk::DescriptorPool descriptor_pool =
        this->vkal_device.get().createDescriptorPool(descriptor_pool_create_info);
    this->pools.push_back(std::make_unique<Pool>(Pool{
        .pool = descriptor_pool,
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
        .pool = *this->pools.back(),
        .pool_size_indices = pool_indices,
        .pool_sizes = layout_pool_sizes,
    };
}

///////////////////////////////////////////////////////////
void Descriptor::clean(Pool& pool) {
    if (pool.remaining_sets == this->max_sets) {
        for (size_t i = 0; i < this->pools.size(); ++i) {
            if (this->pools[i].get() == &pool) {
                this->vkal_device.get().destroyDescriptorPool(this->pools[i]->pool);
                this->pools.erase(this->pools.begin() + i);
                return;
            }
        }
        throw std::runtime_error("Pool does not belong to descriptor");
    }
}

///////////////////////////////////////////////////////////
void Descriptor::dump() {
    size_t index = 0;
    for (const auto& pool : this->pools) {
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
