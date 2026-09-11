#pragma once

#include "descriptor_layout.hpp"
#include "device.hpp"

#include <gtest/gtest_prod.h>

#include <cstdint>
#include <memory>

namespace vkal {

struct DescriptorPoolData {
    vk::DescriptorPool vk_descriptor_pool;
    uint32_t remaining_sets;
    std::vector<vk::DescriptorPoolSize> pool_sizes;
};

struct DescriptorPoolInfo {
    DescriptorPoolData& descriptor_pool_data;
    std::vector<size_t> pool_size_indices;
    std::vector<vk::DescriptorPoolSize> pool_sizes;
};

struct DescriptorParams {
    Device& device;
    uint32_t max_sets;
    uint32_t pool_size;
};

class Descriptor {
  public:
    // No copy and mo move
    Descriptor(const Descriptor&) = delete;
    Descriptor& operator=(const Descriptor&) = delete;
    Descriptor(Descriptor&&) = delete;
    Descriptor& operator=(Descriptor&&) = delete;

    explicit Descriptor(const DescriptorParams& params);

    ~Descriptor();

    DescriptorPoolInfo
    get_pool(const std::vector<std::reference_wrapper<DescriptorLayout>>& layouts);

    void clean(DescriptorPoolData& descriptor_pool_data);

    void dump();

  private:
    Device& device;
    uint32_t max_sets;
    uint32_t pool_size;
    std::vector<std::unique_ptr<DescriptorPoolData>> descriptor_pool_datas;

    FRIEND_TEST(DescriptorTest, GetPool);
    FRIEND_TEST(DescriptorTest, GetPoolFail);
    FRIEND_TEST(DescriptorTest, GetMultiplePools);
    FRIEND_TEST(DescriptorTest, GetMultipleUnfit);
    FRIEND_TEST(DescriptorTest, FreeSet);
    FRIEND_TEST(DescriptorTest, FreeSetSharedPool);
    FRIEND_TEST(DescriptorTest, FreeSetSeparatePool);
};

using DescriptorPtr = std::unique_ptr<Descriptor>;

inline DescriptorPtr descriptor_ptr(const DescriptorParams& params) {
    return std::make_unique<Descriptor>(params);
}

} // namespace vkal
