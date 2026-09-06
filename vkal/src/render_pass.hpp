#pragma once

#include "buffer.hpp"
#include "descriptor_set.hpp"
#include "image.hpp"
#include "pipeline.hpp"

#include <unordered_map>

namespace vkal {

class RenderPass {
  public:
    virtual ~RenderPass() = default;
    virtual void setup_metadata(
        const std::unordered_map<std::string, std::reference_wrapper<Buffer>>&,
        const std::unordered_map<std::string, std::reference_wrapper<Image>>&,
        const std::unordered_map<std::string, std::reference_wrapper<vk::RenderingAttachmentInfo>>&,
        std::optional<std::reference_wrapper<Pipeline>>,
        std::optional<std::reference_wrapper<DescriptorSet>>) {
    }
    virtual void render(vk::CommandBuffer) {
    }
    virtual void
    on_rebound_resources(const std::unordered_map<std::string, std::reference_wrapper<Buffer>>&,
                         const std::unordered_map<std::string, std::reference_wrapper<Image>>&) {
    }
};

}; // namespace vkal
