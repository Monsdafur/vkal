cmake_minimum_required(VERSION 4.4.1)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)

project(VulkanAbstractLayer)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Copy images to binary directory
file(COPY "${CMAKE_SOURCE_DIR}/resources/textures" DESTINATION "${CMAKE_BINARY_DIR}/bin/resources")
file(COPY "${CMAKE_SOURCE_DIR}/resources/data" DESTINATION "${CMAKE_BINARY_DIR}/bin/resources")

# define macros
# add_compile_definitions(VULKAN_VERSION_1_4)
add_compile_definitions(SLANG_PRIMARY)
add_compile_definitions(ENABLE_VALIDATION)
add_compile_definitions(ENABLE_DEBUG)

# SPIR-V compilation
find_program(SLANGC NAMES slangc)
message(STATUS "Slangc: ${SLANGC}")

set(SHADER_SOURCE_DIR ${CMAKE_SOURCE_DIR}/resources/shaders)
set(SHADER_OUTPUT_DIR ${CMAKE_BINARY_DIR}/bin/resources/shaders)
file(MAKE_DIRECTORY ${SHADER_OUTPUT_DIR})

# Find all Slang shader files
file(GLOB SHADER_FILES 
    ${SHADER_SOURCE_DIR}/*.slang
)

# Compile each Slang file to SPIR-V
foreach(SHADER ${SHADER_FILES})
    get_filename_component(SHADER_NAME ${SHADER} NAME)
    get_filename_component(SHADER_STEM ${SHADER} NAME_WE)
    set(SPV_OUTPUT "${SHADER_OUTPUT_DIR}/${SHADER_STEM}.spv")

    file(READ ${SHADER} SHADER_SOURCE)
    
    set(ENTRIES)
    # Check for each entries in the shader file and append to the entry set
    if(SHADER_SOURCE MATCHES "\\[shader\\(\"vertex\"\\)\\]")
        list(APPEND ENTRIES -entry vertMain)
    endif()
    if(SHADER_SOURCE MATCHES "\\[shader\\(\"fragment\"\\)\\]")
        list(APPEND ENTRIES -entry fragMain)
    endif()
    if(SHADER_SOURCE MATCHES "\\[shader\\(\"compute\"\\)\\]")
        list(APPEND ENTRIES -entry compMain)
    endif()

    add_custom_command(
        OUTPUT ${SPV_OUTPUT}
        COMMAND ${SLANGC} ${SHADER} -target spirv -profile spirv_1_4 -emit-spirv-directly -fvk-use-entrypoint-name ${ENTRIES} -o ${SPV_OUTPUT}

        DEPENDS ${SHADER}
        COMMENT "Compiled ${SHADER_NAME}"
        VERBATIM
    )
    list(APPEND SPV_FILES ${SPV_OUTPUT})
endforeach()
add_custom_target(shader ALL DEPENDS ${SPV_FILES})

add_subdirectory(vkal)

# Fetch glm
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.3
)
FetchContent_MakeAvailable(glm)

# Add tests
enable_testing()

add_executable(device_test tests/device_test.cpp)
target_link_libraries(device_test PUBLIC vkal GTest::gtest_main)

add_executable(memory_allocator_test tests/memory_allocator_test.cpp)
target_link_libraries(memory_allocator_test PUBLIC vkal GTest::gtest_main)

add_executable(buffer_test tests/buffer_test.cpp)
target_link_libraries(buffer_test PUBLIC vkal)

add_executable(surface_test tests/surface_test.cpp)
target_link_libraries(surface_test PUBLIC vkal)

add_executable(render_graph_test tests/render_graph_test.cpp)
target_link_libraries(render_graph_test PUBLIC vkal)

add_executable(descriptor_test tests/descriptor_test.cpp)
target_link_libraries(descriptor_test PUBLIC vkal)

include(GoogleTest)
gtest_discover_tests(device_test)
gtest_discover_tests(memory_allocator_test)
gtest_discover_tests(buffer_test)
gtest_discover_tests(surface_test)
gtest_discover_tests(render_graph_test)
gtest_discover_tests(descriptor_test)

# Create helper library
file (GLOB VKAL_HELPER_SRC 
    "samples/vkal-helper/*.cpp" 
)
add_library(vkal-helper STATIC ${VKAL_HELPER_SRC})
include_directories(vkal-helper PUBLIC ${CMAKE_SOURCE_DIR}/samplers/vkal-helper)
target_link_libraries(vkal-helper PUBLIC vkal glm)

# Add executables
add_executable(triangle samples/triangle.cpp)
target_link_libraries(triangle PUBLIC vkal-helper)
target_compile_definitions(triangle PRIVATE _USE_MATH_DEFINES)

add_executable(texture samples/texture.cpp)
target_link_libraries(texture PUBLIC vkal-helper)
target_compile_definitions(texture PRIVATE _USE_MATH_DEFINES)

add_executable(tilemap samples/tilemap.cpp)
target_link_libraries(tilemap PUBLIC vkal-helper)
target_compile_definitions(tilemap PRIVATE _USE_MATH_DEFINES)

# Apply clang-format
file (GLOB_RECURSE SOURCES
    "${CMAKE_SOURCE_DIR}/samples/*.cpp"
    "${CMAKE_SOURCE_DIR}/vkal/src/*.cpp"
)
find_program(CLANG_FORMAT_EXE NAMES clang-format)
if(CLANG_FORMAT_EXE)
  message(STATUS "clang-format found:" ${CLANG_FORMAT_EXE})
  add_custom_target(format COMMAND ${CLANG_FORMAT_EXE} -i -style=file ${SOURCES})
endif()

# Apply clang-tidy
find_program(CLANG_TIDY_EXE NAMES clang-tidy)
if(CLANG_TIDY_EXE)
    message(STATUS "clang-tidy found: ${CLANG_TIDY_EXE}")

    set(TIDY_CMD
        "${CLANG_TIDY_EXE}"
        "--config-file=${CMAKE_SOURCE_DIR}/.clang-tidy"
        "--header-filter=^${CMAKE_SOURCE_DIR}/(triangle|texture|vkal)"
    )
    set_target_properties(vkal PROPERTIES CXX_CLANG_TIDY "${TIDY_CMD}")
    set_target_properties(triangle PROPERTIES CXX_CLANG_TIDY "${TIDY_CMD}")
    set_target_properties(texture PROPERTIES CXX_CLANG_TIDY "${TIDY_CMD}")
endif()
