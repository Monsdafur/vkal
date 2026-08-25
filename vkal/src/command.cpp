#include "command.hpp"

namespace vkal {

///////////////////////////////////////////////////////////
Command::Command(const CommandParams& params)
    : vkal_device(params.vkal_device),
      command_pool(this->vkal_device.get_command_pool(params.queue_index)) {
    vk::CommandBufferAllocateInfo command_buffer_allocate_info;
    command_buffer_allocate_info.setCommandPool(this->command_pool)
        .setLevel(params.level)
        .setCommandBufferCount(params.command_count);
    this->commands = this->vkal_device.get().allocateCommandBuffers(command_buffer_allocate_info);
}

///////////////////////////////////////////////////////////
Command::~Command() {
    this->vkal_device.get().freeCommandBuffers(this->command_pool, this->commands);
}

///////////////////////////////////////////////////////////
vk::CommandBuffer Command::get(uint32_t index) {
    return this->commands.at(index);
}

} // namespace vkal
