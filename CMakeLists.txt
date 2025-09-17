cmake_minimum_required(VERSION 3.0...3.25)
project(
    LearnWebGPU # name of the project, which will also be the name of the visual studio solution if you use it
    VERSION 0.1.0 # any version number
    LANGUAGES CXX C # programming languages used by the project
)

# Application
set(APP_INC ${CMAKE_SOURCE_DIR}/src)
file(GLOB_RECURSE APP_SRC ${CMAKE_SOURCE_DIR}/src/*.cpp)

add_executable(App ${APP_SRC})

set_target_properties(App PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    COMPILE_WARNING_AS_ERROR ON
)

# if (MSVC)
#     target_compile_options(App PRIVATE /W4)
# else()
#     target_compile_options(App PRIVATE -Wall -Wextra -pedantic)
# endif()

if (XCODE)
    set_target_properties(App PROPERTIES
        XCODE_GENERATE_SCHEME ON
        XCODE_SCHEME_ENABLE_GPU_FRAME_CAPTURE_MODE "Metal"
    )
endif()

# Compile Dawn then add it as a dependency
set(DAWN_ENABLE_D3D12 OFF CACHE BOOL "Disable DX12 backend" FORCE)
set(DAWN_ENABLE_METAL OFF CACHE BOOL "Disable Metal backend" FORCE)
set(DAWN_ENABLE_OPENGL OFF CACHE BOOL "Disable OpenGL backend" FORCE)
set(DAWN_ENABLE_WGPU OFF CACHE BOOL "Disable WebGPU backend" FORCE)
set(DAWN_ENABLE_VULKAN ON CACHE BOOL "Enable Vulkan backend" FORCE)
set(Dawn_USE_INTERNAL_GLFW OFF CACHE BOOL "Disable internal GLFW" FORCE)
add_subdirectory(vendor/dawn)
add_subdirectory(vendor/glfw3webgpu)

target_include_directories(App
    PRIVATE
        ${APP_INC}
)

target_link_libraries(App
    PRIVATE
        webgpu
        glfw
        glfw3webgpu
)

target_copy_webgpu_binaries(App)