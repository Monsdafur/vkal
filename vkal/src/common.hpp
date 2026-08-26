#include <vulkan/vulkan.hpp>

#include <string>

namespace vkal {

void debug(const std::string& message);

vk::DeviceSize kilobytes(vk::DeviceSize value);

vk::DeviceSize megabytes(vk::DeviceSize value);

vk::DeviceSize gigabytes(vk::DeviceSize value);

} // namespace vkal
