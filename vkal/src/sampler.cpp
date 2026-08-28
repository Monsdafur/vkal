#include "sampler.hpp"

namespace vkal {

///////////////////////////////////////////////////////////
Sampler::Sampler(const SamplerParams& params) : vkal_device(params.vkal_device) {
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

    this->sampler = this->vkal_device.get().createSampler(sampler_create_info);
}

///////////////////////////////////////////////////////////
Sampler::~Sampler() {
    this->vkal_device.get().destroySampler(this->sampler);
}

///////////////////////////////////////////////////////////
vk::Sampler Sampler::get() {
    return this->sampler;
}

} // namespace vkal
