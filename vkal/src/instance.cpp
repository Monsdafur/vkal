#include "instance.hpp"

#include <print>

namespace vkal {

///////////////////////////////////////////////////////////
static VKAPI_ATTR vk::Bool32 VKAPI_CALL debug_callback(
    vk::DebugUtilsMessageSeverityFlagBitsEXT severity, vk::DebugUtilsMessageTypeFlagsEXT type,
    const vk::DebugUtilsMessengerCallbackDataEXT* callback_data, void* user_data) {

    std::string severity_string;
    switch (severity) {
    case vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo:
        severity_string = "INFO   |";
        break;
    case vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning:
        severity_string = "WARNING|";
        break;
    case vk::DebugUtilsMessageSeverityFlagBitsEXT::eError:
        severity_string = "ERROR  |";
        break;
    default:
        severity_string = "INFO   |";
        break;
    }
    std::string message = callback_data ? callback_data->pMessage : "<null>";
    std::println("{} {}", severity_string, message);

    return VK_FALSE;
}

///////////////////////////////////////////////////////////
Instance::Instance() {
    this->create_instance();
#if defined(ENABLE_VALIDATION)
    this->create_message_debugger();
#endif
}

///////////////////////////////////////////////////////////
Instance::~Instance() {
#if defined(ENABLE_VALIDATION)
    this->vk_instance.destroyDebugUtilsMessengerEXT(this->vk_debugger);
#endif
    this->vk_instance.destroy();
}

///////////////////////////////////////////////////////////
vk::Instance Instance::get() {
    return this->vk_instance;
}

///////////////////////////////////////////////////////////
void Instance::create_instance() {
    PFN_vkGetInstanceProcAddr vk_get_instance_proc_addr =
        this->dynamic_loader.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
    VULKAN_HPP_DEFAULT_DISPATCHER.init(vk_get_instance_proc_addr);

    vk::ApplicationInfo app_info;
    app_info.setPApplicationName("")
        .setApplicationVersion(vk::makeVersion(1, 0, 0))
        .setPEngineName("")
        .setEngineVersion(vk::makeVersion(1, 0, 0));

#if defined(VULKAN_VERSION_1_4)
    app_info.setApiVersion(vk::ApiVersion14);
#else
    app_info.setApiVersion(vk::ApiVersion13);
#endif

    // Get SDL extensions
    uint32_t sdl_extension_count;
    const char* const* sdl_extensions = SDL_Vulkan_GetInstanceExtensions(&sdl_extension_count);

    // Merge SDL extensions with additional extensions
    std::vector<const char*> required_extensions =
        std::vector<const char*>(sdl_extensions, sdl_extensions + sdl_extension_count);

    // Add more extensions if needed
#if defined(ENABLE_VALIDATION)
    required_extensions.push_back(vk::EXTDebugUtilsExtensionName);
#endif
    required_extensions.push_back(vk::KHRGetSurfaceCapabilities2ExtensionName);

    // Get supported extensions
    std::vector<vk::ExtensionProperties> extension_properties =
        vk::enumerateInstanceExtensionProperties();

    // Check support for each required extension
    for (const char* extension : required_extensions) {
        if (std::ranges::none_of(extension_properties,
                                 [extension](const vk::ExtensionProperties& extension_property) {
                                     return strcmp(extension_property.extensionName, extension) ==
                                            0;
                                 })) {
            throw std::runtime_error(std::format("Missing required extension {}", extension));
        }
    }

    // Setup layers
    std::vector<const char*> required_layers;
#if defined(ENABLE_VALIDATION)
    required_layers.push_back("VK_LAYER_KHRONOS_validation");
#endif

    // Get supported layers
    std::vector<vk::LayerProperties> layer_properties = vk::enumerateInstanceLayerProperties();

    for (const char* layer : required_layers) {
        if (std::ranges::none_of(layer_properties,
                                 [layer](const vk::LayerProperties& layer_property) {
                                     return strcmp(layer_property.layerName, layer) == 0;
                                 })) {
            throw std::runtime_error(std::format("Missing required layer {}", layer));
        }
    }

    vk::ValidationFeaturesEXT validation_features;
    vk::InstanceCreateInfo instance_create_info =
        vk::InstanceCreateInfo()
            .setPNext(&validation_features)
            .setPApplicationInfo(&app_info)
            .setPEnabledExtensionNames(required_extensions)
            .setPEnabledLayerNames(required_layers);
    this->vk_instance = vk::createInstance(instance_create_info);
    VULKAN_HPP_DEFAULT_DISPATCHER.init(this->vk_instance);
}

///////////////////////////////////////////////////////////
void Instance::create_message_debugger() {
    vk::DebugUtilsMessengerCreateInfoEXT debugger_create_info;
    debugger_create_info
        .setMessageSeverity(vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                            vk::DebugUtilsMessageSeverityFlagBitsEXT::eError)
        .setMessageType(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                        vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
                        vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation)
        .setPfnUserCallback(debug_callback);
    this->vk_debugger = this->vk_instance.createDebugUtilsMessengerEXT(debugger_create_info);
}

} // namespace vkal
