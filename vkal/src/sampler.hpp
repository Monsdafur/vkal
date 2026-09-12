#pragma once

#include "device.hpp"

#include <memory>

namespace vkal {

struct SamplerParams {
    Device& device;
    vk::SamplerAddressMode address_mode;
    vk::Filter filter = vk::Filter::eNearest;
    float mip_lod_bias = 0.0f;
    float min_lod = 0.0f;
    float max_lod = 0.0f;
    vk::SamplerMipmapMode mip_map_mode = vk::SamplerMipmapMode::eNearest;
    bool enable_anisotropy = false;
    float max_anisotropy = 1.0f;
};

class Sampler {
  public:
    // No copy and no move
    Sampler(const Sampler&) = delete;
    Sampler& operator=(const Sampler&) = delete;
    Sampler(Sampler&&) = delete;
    Sampler& operator=(Sampler&&) = delete;

    explicit Sampler(const SamplerParams& params);

    ~Sampler();

    vk::Sampler get();

  private:
    Device& device;
    vk::Sampler vk_sampler;
};

using SamplerPtr = std::unique_ptr<Sampler>;

inline SamplerPtr sampler_ptr(const SamplerParams& params) {
    return std::make_unique<Sampler>(params);
}

} // namespace vkal
