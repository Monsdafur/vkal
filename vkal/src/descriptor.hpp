#pragma once

#include "device.hpp"

#include <cstdint>
#include <map>
#include <memory>

namespace vkal {

struct DescriptorParams {
    Device& vkal_device;
    uint32_t max_sets;
    std::vector<vk::DescriptorPoolSize> pool_sizes;
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

    vk::DescriptorPool get();

  private:
    vkal::Device& vkal_device;
    uint32_t set_count;
    std::map<vk::DescriptorType, uint32_t> pool_sizes;
    vk::DescriptorPool pool;
};

using DescriptorPtr = std::unique_ptr<Descriptor>;

inline DescriptorPtr descriptor_ptr(const DescriptorParams& params) {
    return std::make_unique<Descriptor>(params);
}

} // namespace vkal
