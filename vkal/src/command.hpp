#pragma once

#include "device.hpp"

#include <cstdint>
#include <memory>

namespace vkal {

struct CommandParams {
    Device& vkal_device;
    vk::CommandBufferLevel level = vk::CommandBufferLevel::ePrimary;
    uint32_t queue_index;
    uint32_t command_count = 1;
};

class Command {
  public:
    // No copy and mo move
    Command(const Command&) = delete;
    Command& operator=(const Command&) = delete;
    Command(Command&&) = delete;
    Command& operator=(Command&&) = delete;

    explicit Command(const CommandParams& params);

    ~Command();

    vk::CommandBuffer get(uint32_t index);

  private:
    vkal::Device& vkal_device;
    vk::CommandPool command_pool;
    std::vector<vk::CommandBuffer> commands;
};

using CommandPtr = std::unique_ptr<Command>;

inline CommandPtr command_ptr(const CommandParams& params) {
    return std::make_unique<Command>(params);
}

} // namespace vkal
