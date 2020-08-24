cmake_minimum_required(VERSION 3.17)
project(Pyrite)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# TODO: Anyway to do this better?
set(GLSL_CMD "glslc -O --target-env=vulkan1.1 -mfmt=num")
add_custom_target(
    PyriteShaders
    COMMAND glslc -O --target-env=vulkan1.1 -mfmt=num "${CMAKE_CURRENT_SOURCE_DIR}/Source/Shaders/Triangle.vert" -o "${CMAKE_CURRENT_SOURCE_DIR}/Source/Shaders/Triangle.vert.spv"
    COMMAND glslc -O --target-env=vulkan1.1 -mfmt=num "${CMAKE_CURRENT_SOURCE_DIR}/Source/Shaders/Triangle.frag" -o "${CMAKE_CURRENT_SOURCE_DIR}/Source/Shaders/Triangle.frag.spv"
)

file(GLOB PyriteSources
    "Source/*.cpp"
    "Source/*.hpp"
)
add_executable(Pyrite "${PyriteSources}")
target_compile_definitions(Pyrite PUBLIC VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1)
target_compile_options(Pyrite PUBLIC -Wall)
target_include_directories(Pyrite PUBLIC
    "$ENV{GLFW_PATH}/include"
    "$ENV{GLM_PATH}"
    "$ENV{VK_SDK_PATH}/Include"
)
target_link_libraries(Pyrite PUBLIC
    glfw3
    vulkan-1
)
add_dependencies(Pyrite PyriteShaders)

target_link_directories(Pyrite PUBLIC
    "$ENV{GLFW_PATH}/lib-vc2019" # TODO: Parameterize this!
    "$ENV{VK_SDK_PATH}/Lib"
)
