#include "sampler.hpp"

namespace vkal {

///////////////////////////////////////////////////////////
Sampler::Sampler(const SamplerParams& params) : device(params.device) {
    vk::SamplerCreateInfo sampler_create_info;
    sampler_create_info.setMagFilter(params.filter)
        .setMinFilter(params.filter)
        .setMipmapMode(params.mip_map_mode)
        .setAddressModeU(params.address_mode)
        .setAddressModeV(params.address_mode)
        .setAddressModeW(params.address_mode)
        .setMipLodBias(params.mip_lod_bias)
        .setAnisotropyEnable(params.enable_anisotropy)
        .setMaxAnisotropy(params.max_anisotropy)
        .setCompareEnable(false)
        .setMinLod(params.min_lod)
        .setMaxLod(params.max_lod);

    this->vk_sampler = this->device.get().createSampler(sampler_create_info);
}

///////////////////////////////////////////////////////////
Sampler::~Sampler() {
    this->device.get().destroySampler(this->vk_sampler);
}

///////////////////////////////////////////////////////////
vk::Sampler Sampler::get() {
    return this->vk_sampler;
}

} // namespace vkal
