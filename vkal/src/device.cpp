#include "device.hpp"
#include "common.hpp"
#include "vulkan/vulkan.hpp"

#include <cstdint>
#include <stdexcept>
#include <vector>

namespace vkal {

///////////////////////////////////////////////////////////
static bool is_device_suitable(const std::vector<vk::ExtensionProperties>& device_extensions,
                               const vk::PhysicalDeviceProperties& properties,
                               const std::vector<std::string>& required_device_extensions) {
#if defined(ENABLE_DEBUG)
    debug(std::format("Device: {}", std::string(properties.deviceName)));
#endif

    // Device must be dedicated GPU
    bool is_dedicated_gpu = properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu;

#if defined(ENABLE_DEBUG)
    debug(std::format("\tIs dedicated GPU: {}", is_dedicated_gpu ? "yes" : "no"));
#endif

    // Device must support Vulkan 1.3 or 1.4 of the macro is defined
    bool supported_version = false;
#if defined(VULKAN_VERSION_1_4)
    supported_version = properties.apiVersion >= vk::ApiVersion14;
#else
    supported_version = properties.apiVersion >= vk::ApiVersion13;
#endif
    uint32_t version_major = vk::versionMajor(properties.apiVersion);
    uint32_t version_minor = vk::versionMinor(properties.apiVersion);
    uint32_t version_patch = vk::versionPatch(properties.apiVersion);

#if defined(ENABLE_DEBUG)
    debug(std::format("\tVulkan version {}.{}.{}", version_major, version_minor, version_patch));
#endif

    bool support_all_extensions = true;

    for (const std::string& extension : required_device_extensions) {
        bool support_extension = std::ranges::any_of(
            device_extensions,
            [extension](const vk::ExtensionProperties& extension_properties) -> bool {
                return std::string(extension_properties.extensionName) == extension;
            });

#if defined(ENABLE_DEBUG)
        debug(std::format("\t\tExtension: {} -- {}", extension,
                          support_extension ? "supported" : "not supported"));
#endif

        support_all_extensions = support_all_extensions && support_extension;
    }

    bool suitable = is_dedicated_gpu && supported_version && support_all_extensions;

#if defined(ENABLE_DEBUG)
    debug(std::format("\tDevice {}", suitable ? "suitable" : "not suitable"));
#endif

    return suitable;
}

///////////////////////////////////////////////////////////
Device::Device(const DeviceParams& params) : vkal_instance(params.vkal_instance) {
    this->select_physical_device(params);
    this->create_device(params);
    this->get_queues();
}

///////////////////////////////////////////////////////////
Device::~Device() {
    this->device.waitIdle();
    for (auto command_pool : this->command_pools) {
        this->device.destroyCommandPool(command_pool);
    }
    this->device.destroy();
}

///////////////////////////////////////////////////////////
vk::PhysicalDevice Device::get_physical() {
    return this->physical_device;
}

///////////////////////////////////////////////////////////
vk::Device Device::get() {
    return this->device;
}

///////////////////////////////////////////////////////////
const vk::PhysicalDeviceProperties& Device::get_properties() const {
    return this->physical_device_properties;
}

///////////////////////////////////////////////////////////
uint32_t Device::get_queue_index(vk::QueueFlags queue_flags) {
    uint32_t index = 0;
    for (auto& properties : this->queue_properties) {
        if ((properties.queueFlags & queue_flags) == queue_flags) {
            return index;
        }
        index++;
    }

    throw std::runtime_error("Failed to find any queue index with matching flags");
}

///////////////////////////////////////////////////////////
vk::Queue Device::get_queue(uint32_t index) {
    return this->queues.at(index);
}

///////////////////////////////////////////////////////////
vk::Queue Device::get_present_queue(const vk::SurfaceKHR& surface) {
    for (size_t i = 0; i < this->queue_properties.size(); ++i) {
        if (this->physical_device.getSurfaceSupportKHR(i, surface)) {
            return this->queues.at(i);
        }
    }

    throw std::runtime_error("Failed to find any queue with surface support");
}

vk::CommandPool Device::get_command_pool(uint32_t index) {
    return this->command_pools.at(index);
}

///////////////////////////////////////////////////////////
void Device::select_physical_device(const DeviceParams& params) {
    std::vector<vk::PhysicalDevice> physical_devices =
        this->vkal_instance.get().enumeratePhysicalDevices();

    for (vk::PhysicalDevice physical_device : physical_devices) {
        vk::PhysicalDeviceProperties2 device_properties = physical_device.getProperties2();
        std::vector<vk::ExtensionProperties> available_extensions =
            physical_device.enumerateDeviceExtensionProperties();
        if (is_device_suitable(available_extensions, device_properties.properties,
                               params.device_extensions)) {
            this->physical_device_properties = device_properties.properties;
            this->physical_device = physical_device;
            break;
        }
    }

    if (!this->physical_device) {
        throw std::runtime_error("Failed to query any suitable device");
    }
}

///////////////////////////////////////////////////////////
void Device::create_device(const DeviceParams& params) {
    std::vector<vk::QueueFamilyProperties2> queue_family_properties =
        this->physical_device.getQueueFamilyProperties2();
    std::vector<vk::DeviceQueueCreateInfo> queue_create_infos(queue_family_properties.size());
    std::array<float, 1> queue_priorities = {0.5f};
    uint32_t queue_index = 0;
    for (auto& queue_create_info : queue_create_infos) {
        this->queue_properties.push_back(
            queue_family_properties.at(queue_index).queueFamilyProperties);
        queue_create_info.setQueueCount(1)
            .setQueuePriorities(queue_priorities)
            .setQueueFamilyIndex(queue_index++);
    }

    // Add more features to dynamic state if needed
    vk::PhysicalDeviceExtendedDynamicState2FeaturesEXT dynamic_state_features;
    dynamic_state_features.setExtendedDynamicState2(true);

    // Add more features to vulkan physical device if needed
    vk::PhysicalDeviceVulkan12Features physical_device_vulkan_features_12;
    physical_device_vulkan_features_12.setRuntimeDescriptorArray(true)
        .setDescriptorBindingPartiallyBound(true)
        .setDescriptorBindingVariableDescriptorCount(true);

    vk::PhysicalDeviceVulkan13Features physical_device_vulkan_features_13;
    physical_device_vulkan_features_13.setSynchronization2(true).setDynamicRendering(true);

#if defined(VK2D_VULKAN_VERSION_1_4)
    vk::PhysicalDeviceVulkan14Features physical_device_vulkan_features_14;
#endif

    // Add more features to physical device if needed
    vk::PhysicalDeviceFeatures2 physical_device_features;
    physical_device_features.setFeatures(
        vk::PhysicalDeviceFeatures().setFillModeNonSolid(true).setSamplerAnisotropy(true));

    // Chaining device features
#if defined(VK2D_VULKAN_VERSION_1_4)
    physical_device_features.setPNext(&physical_device_vulkan_features_14);
    physical_device_vulkan_features_14.setPNext(physical_device_vulkan_features_13);
#else
    physical_device_features.setPNext(&physical_device_vulkan_features_13);
#endif
    physical_device_vulkan_features_13.setPNext(physical_device_vulkan_features_12);
    physical_device_vulkan_features_12.setPNext(dynamic_state_features);

    std::vector<const char*> enabled_extension_names;
    for (const auto& extension : params.device_extensions) {
        enabled_extension_names.push_back(extension.c_str());
    }

    // Creating device
    vk::DeviceCreateInfo device_create_info;
    device_create_info.setPNext(physical_device_features)
        .setQueueCreateInfos(queue_create_infos)
        .setPEnabledExtensionNames(enabled_extension_names);

    this->device = this->physical_device.createDevice(device_create_info);
}

///////////////////////////////////////////////////////////
void Device::get_queues() {
    vk::DeviceQueueInfo2 device_queue_info;
    device_queue_info.setQueueIndex(0);
    vk::CommandPoolCreateInfo command_pool_create_info;
    command_pool_create_info.setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
    for (uint32_t i = 0; i < this->queue_properties.size(); ++i) {
        device_queue_info.setQueueFamilyIndex(i);
        this->queues.push_back(this->device.getQueue2(device_queue_info));

        command_pool_create_info.setQueueFamilyIndex(i);
        this->command_pools.push_back(this->device.createCommandPool(command_pool_create_info));
    }
}

} // namespace vkal
