#pragma once

#include "buffer.hpp"
#include "image.hpp"

#include <unordered_map>

namespace vkal {

class RenderPass {
  public:
    virtual ~RenderPass() = default;
    virtual void
    setup_metadata(const std::unordered_map<std::string, std::reference_wrapper<Buffer>>&,
                   const std::unordered_map<std::string, std::reference_wrapper<Image>>&) {
    }
    virtual void render(vk::CommandBuffer) {
    }
};

}; // namespace vkal
