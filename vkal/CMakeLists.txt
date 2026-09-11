cmake_minimum_required(VERSION 4.4.1)

# Vulkan
find_package(Vulkan REQUIRED)

# Fetch GLFW
include(FetchContent)
# Fetch SDL3 contents
FetchContent_Declare(
    sdl3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.4.10
)
FetchContent_MakeAvailable(sdl3)

# Fetch SDL3 image contents
FetchContent_Declare(
    sdl3_image
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_image.git
    GIT_TAG release-3.4.4
)
FetchContent_MakeAvailable(sdl3_image)

# Fetch gtest
FetchContent_Declare(
    gtest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.18.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(gtest)

# Create custom library
file (GLOB VKAL_SRC 
    "src/*.cpp" 
)
add_library(vkal STATIC ${VKAL_SRC})
include_directories(vkal PUBLIC ${CMAKE_SOURCE_DIR}/vkal)
target_link_libraries(vkal PUBLIC SDL3::SDL3 SDL3_image::SDL3_image Vulkan::Vulkan GTest::gtest_main)
