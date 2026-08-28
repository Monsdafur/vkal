#include "../vkal/src/surface.cpp"

#include <gtest/gtest.h>

namespace vkal {

TEST(SurfaceTest, SurfaceFormatEvaluation) {
    vk::SurfaceFormatKHR surface_format;
    vk::SurfaceFormatKHR desired_surface_format =
        vk::SurfaceFormatKHR(vk::Format::eR8G8B8A8Sint, vk::ColorSpaceKHR::eAdobergbNonlinearEXT);
    std::vector<vk::SurfaceFormat2KHR> available_surface_formats = {
        vk::SurfaceFormat2KHR(
            vk::SurfaceFormatKHR(vk::Format::eR8G8B8A8Srgb, vk::ColorSpaceKHR::eSrgbNonlinear)),
        vk::SurfaceFormat2KHR(vk::SurfaceFormatKHR(vk::Format::eR8G8B8A8Sint,
                                                   vk::ColorSpaceKHR::eAdobergbNonlinearEXT)),
        vk::SurfaceFormat2KHR(vk::SurfaceFormatKHR(vk::Format::eR32G32B32Sfloat,
                                                   vk::ColorSpaceKHR::eExtendedSrgbLinearEXT)),
    };

    surface_format = evaluate_surface_format(available_surface_formats, desired_surface_format);

    ASSERT_EQ(surface_format.format, desired_surface_format.format);
    ASSERT_EQ(surface_format.colorSpace, desired_surface_format.colorSpace);
}

TEST(SurfaceTest, SurfaceFormatEvaluationFallback) {
    vk::SurfaceFormatKHR surface_format;
    vk::SurfaceFormatKHR desired_surface_format =
        vk::SurfaceFormatKHR(vk::Format::eR8G8B8A8Srgb, vk::ColorSpaceKHR::eAdobergbNonlinearEXT);
    std::vector<vk::SurfaceFormat2KHR> available_surface_formats = {
        vk::SurfaceFormat2KHR(
            vk::SurfaceFormatKHR(vk::Format::eR8G8B8A8Srgb, vk::ColorSpaceKHR::eSrgbNonlinear)),
        vk::SurfaceFormat2KHR(vk::SurfaceFormatKHR(vk::Format::eR8G8B8A8Sint,
                                                   vk::ColorSpaceKHR::eAdobergbNonlinearEXT)),
        vk::SurfaceFormat2KHR(vk::SurfaceFormatKHR(vk::Format::eR32G32B32Sfloat,
                                                   vk::ColorSpaceKHR::eExtendedSrgbLinearEXT)),
    };

    surface_format = evaluate_surface_format(available_surface_formats, desired_surface_format);

    ASSERT_EQ(surface_format.format, available_surface_formats.front().surfaceFormat.format);
    ASSERT_EQ(surface_format.colorSpace,
              available_surface_formats.front().surfaceFormat.colorSpace);
}

TEST(SurfaceTest, PresentModeEvaluation) {
    std::vector<vk::PresentModeKHR> present_modes = {
        vk::PresentModeKHR::eFifo,
        vk::PresentModeKHR::eMailbox,
    };
    std::vector<vk::PresentModeKHR> available_present_modes = {
        vk::PresentModeKHR::eMailbox,
        vk::PresentModeKHR::eFifo,
        vk::PresentModeKHR::eFifoLatestReady,
        vk::PresentModeKHR::eFifoRelaxed,
    };
    vk::PresentModeKHR present_mode =
        evaluate_present_modes(available_present_modes, present_modes);

    ASSERT_EQ(present_mode, vk::PresentModeKHR::eFifo);
}

TEST(SurfaceTest, PresentModeEvaluationFallbackSecond) {
    std::vector<vk::PresentModeKHR> present_modes = {
        vk::PresentModeKHR::eFifo,
        vk::PresentModeKHR::eMailbox,
    };
    std::vector<vk::PresentModeKHR> available_present_modes = {
        vk::PresentModeKHR::eMailbox,
        vk::PresentModeKHR::eFifoLatestReady,
        vk::PresentModeKHR::eFifoRelaxed,
    };
    vk::PresentModeKHR present_mode =
        evaluate_present_modes(available_present_modes, present_modes);

    ASSERT_EQ(present_mode, vk::PresentModeKHR::eMailbox);
}

} // namespace vkal
