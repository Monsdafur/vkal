#pragma once

#include <vulkan/vulkan.hpp>

#include <string>

namespace vkal {

inline constexpr vk::AccessFlags2 VK_ACCESS_READ_FLAGS =
    vk::AccessFlagBits2::eIndirectCommandRead | vk::AccessFlagBits2::eIndexRead |
    vk::AccessFlagBits2::eVertexAttributeRead | vk::AccessFlagBits2::eUniformRead |
    vk::AccessFlagBits2::eInputAttachmentRead | vk::AccessFlagBits2::eShaderRead |
    vk::AccessFlagBits2::eColorAttachmentRead | vk::AccessFlagBits2::eDepthStencilAttachmentRead |
    vk::AccessFlagBits2::eTransferRead | vk::AccessFlagBits2::eHostRead |
    vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eTransformFeedbackCounterReadEXT |
    vk::AccessFlagBits2::eConditionalRenderingReadEXT |
    vk::AccessFlagBits2::eColorAttachmentReadNoncoherentEXT |
    vk::AccessFlagBits2::eAccelerationStructureReadKHR |
    vk::AccessFlagBits2::eFragmentShadingRateAttachmentReadKHR |
    vk::AccessFlagBits2::eFragmentDensityMapReadEXT |
    vk::AccessFlagBits2::eCommandPreprocessReadNV | vk::AccessFlagBits2::eDescriptorBufferReadEXT |
    vk::AccessFlagBits2::eOpticalFlowReadNV | vk::AccessFlagBits2::eMicromapReadEXT;

inline constexpr vk::AccessFlags2 VK_ACCESS_WRITE_FLAGS =
    vk::AccessFlagBits2::eShaderWrite | vk::AccessFlagBits2::eColorAttachmentWrite |
    vk::AccessFlagBits2::eDepthStencilAttachmentWrite | vk::AccessFlagBits2::eTransferWrite |
    vk::AccessFlagBits2::eHostWrite | vk::AccessFlagBits2::eMemoryWrite |
    vk::AccessFlagBits2::eTransformFeedbackWriteEXT |
    vk::AccessFlagBits2::eTransformFeedbackCounterWriteEXT |
    vk::AccessFlagBits2::eAccelerationStructureWriteNV |
    vk::AccessFlagBits2::eCommandPreprocessWriteNV | vk::AccessFlagBits2::eMicromapWriteEXT;

std::string size_as_string(vk::DeviceSize size);

void debug(const std::string& message);

vk::DeviceSize kilobytes(vk::DeviceSize value);

vk::DeviceSize megabytes(vk::DeviceSize value);

vk::DeviceSize gigabytes(vk::DeviceSize value);

} // namespace vkal
