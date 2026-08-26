#include "common.hpp"

#include <print>

namespace vkal {

void debug(const std::string& message) {
    std::println("INFO   | {}", message);
}

vk::DeviceSize kilobytes(vk::DeviceSize value) {
    return value * 1024;
}

vk::DeviceSize megabytes(vk::DeviceSize value) {
    return kilobytes(value) * 1024;
}

vk::DeviceSize gigabytes(vk::DeviceSize value) {
    return megabytes(value) * 1024;
}

} // namespace vkal
