#include "../vkal/src/device.cpp"

#include <gtest/gtest.h>

TEST(DeviceTest, DeviceSuitable) {
    std::string ext_name_a = "ExtensionA";
    std::string ext_name_b = "ExtensionB";
    std::string ext_name_c = "ExtensionC";
    std::string ext_name_d = "ExtensionD";
    vk::ExtensionProperties ext_a;
    ext_a.extensionName = ext_name_a;
    vk::ExtensionProperties ext_b;
    ext_b.extensionName = ext_name_b;
    vk::ExtensionProperties ext_c;
    ext_c.extensionName = ext_name_c;
    vk::ExtensionProperties ext_d;
    ext_d.extensionName = ext_name_d;

    vk::PhysicalDeviceProperties device_properties;
    device_properties.apiVersion = vk::ApiVersion14;
    device_properties.deviceType = vk::PhysicalDeviceType::eDiscreteGpu;

    std::vector<std::string> required_device_extensions = {
        "ExtensionA",
        "ExtensionC",
    };

    EXPECT_TRUE(vkal::is_device_suitable({ext_a, ext_b, ext_c, ext_d}, device_properties,
                                         required_device_extensions));
}

TEST(DeviceTest, DeviceExtensionNotSupported) {
    std::string ext_name_a = "ExtensionA";
    std::string ext_name_b = "ExtensionB";
    std::string ext_name_c = "ExtensionC";
    std::string ext_name_d = "ExtensionD";
    vk::ExtensionProperties ext_a;
    ext_a.extensionName = ext_name_a;
    vk::ExtensionProperties ext_b;
    ext_b.extensionName = ext_name_b;
    vk::ExtensionProperties ext_c;
    ext_c.extensionName = ext_name_c;
    vk::ExtensionProperties ext_d;
    ext_d.extensionName = ext_name_d;

    vk::PhysicalDeviceProperties device_properties;
    device_properties.apiVersion = vk::ApiVersion14;
    device_properties.deviceType = vk::PhysicalDeviceType::eDiscreteGpu;

    std::vector<std::string> required_device_extensions = {
        "ExtensionA",
        "ExtensionC",
        "ExtensionE",
    };

    EXPECT_FALSE(vkal::is_device_suitable({ext_a, ext_b, ext_c, ext_d}, device_properties,
                                          required_device_extensions));
}

TEST(DeviceTest, DeviceApiNotSupported) {
    std::string ext_name_a = "ExtensionA";
    vk::ExtensionProperties ext_a;
    ext_a.extensionName = ext_name_a;

    vk::PhysicalDeviceProperties device_properties;
    device_properties.apiVersion = vk::ApiVersion12;
    device_properties.deviceType = vk::PhysicalDeviceType::eDiscreteGpu;

    std::vector<std::string> required_device_extensions = {
        "ExtensionA",
    };

    EXPECT_FALSE(vkal::is_device_suitable({ext_a}, device_properties, required_device_extensions));
}

TEST(DeviceTest, DeviceTypeNotSuitable) {
    std::string ext_name_a = "ExtensionA";
    vk::ExtensionProperties ext_a;
    ext_a.extensionName = ext_name_a;

    vk::PhysicalDeviceProperties device_properties;
    device_properties.apiVersion = vk::ApiVersion14;
    device_properties.deviceType = vk::PhysicalDeviceType::eIntegratedGpu;

    std::vector<std::string> required_device_extensions = {
        "ExtensionA",
    };

    EXPECT_FALSE(vkal::is_device_suitable({ext_a}, device_properties, required_device_extensions));
}
