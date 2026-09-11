#include "command.hpp"

namespace vkal {

///////////////////////////////////////////////////////////
Command::Command(const CommandParams& params)
    : device(params.device), vk_command_pool(this->device.get_command_pool(params.queue_index)) {
    vk::CommandBufferAllocateInfo command_buffer_allocate_info;
    command_buffer_allocate_info.setCommandPool(this->vk_command_pool)
        .setLevel(params.level)
        .setCommandBufferCount(params.command_count);
    this->vk_commands =
        this->device.get().allocateCommandBuffers(command_buffer_allocate_info);
}

///////////////////////////////////////////////////////////
Command::~Command() {
    this->device.get().freeCommandBuffers(this->vk_command_pool, this->vk_commands);
}

///////////////////////////////////////////////////////////
vk::CommandBuffer Command::get(uint32_t index) {
    return this->vk_commands.at(index);
}

} // namespace vkal
