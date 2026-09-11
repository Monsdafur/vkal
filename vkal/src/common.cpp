#include "common.hpp"

#include <print>

namespace vkal {

///////////////////////////////////////////////////////////
std::string size_as_string(vk::DeviceSize size) {
    std::string level[4] = {"B", "KiB", "MB", "GiB"};
    size_t current_level = 0;
    while (size >= 1024 && current_level < 3) {
        size /= 1024;
        current_level++;
    }

    return std::format("{} {}", size, level[current_level]);
}

///////////////////////////////////////////////////////////
void debug(const std::string& message) {
    std::println("INFO   | {}", message);
}

///////////////////////////////////////////////////////////
vk::DeviceSize kilobytes(vk::DeviceSize value) {
    return value * 1024;
}

///////////////////////////////////////////////////////////
vk::DeviceSize megabytes(vk::DeviceSize value) {
    return kilobytes(value) * 1024;
}

///////////////////////////////////////////////////////////
vk::DeviceSize gigabytes(vk::DeviceSize value) {
    return megabytes(value) * 1024;
}

} // namespace vkal
