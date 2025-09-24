cmake_minimum_required(VERSION 3.0...3.25)
project(
    LearnWebGPU # name of the project, which will also be the name of the visual studio solution if you use it
    VERSION 0.1.0 # any version number
    LANGUAGES CXX C # programming languages used by the project
)

set(CMAKE_BUILD_TYPE Release)

# PhysX Configuration
set(PHYSX_DIR "${CMAKE_SOURCE_DIR}/vendor/physx")
set(PHYSX_BUILD_TYPE "checked" CACHE STRING "The build type of PhysX")
set_property(CACHE PHYSX_BUILD_TYPE PROPERTY STRINGS debug checked profile release)

# PhysX build options - disable unnecessary components

# Build early for now
include_directories(vendor/physx/physx/include)
link_directories("vendor/physx/physx/bin/win.x86_64.vc143.md/${PHYSX_BUILD_TYPE}")

# Application
set(APP_INC ${CMAKE_SOURCE_DIR}/src)
file(GLOB_RECURSE APP_SRC ${CMAKE_SOURCE_DIR}/src/*.cpp)

add_executable(App ${APP_SRC})

option(DEV_MODE "Set up development helper settings" ON)

if(DEV_MODE)
    target_compile_definitions(App PRIVATE
        RESOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/res/"
    )
else()
    # Release version
    target_compile_definitions(App PRIVATE
        RESOURCE_DIR="./res/"
    )
endif()

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
add_subdirectory(vendor/dawn)
add_subdirectory(vendor/glfw3webgpu)
add_subdirectory(vendor/spdlog)
add_subdirectory(vendor/glm)
add_subdirectory(vendor/imgui)

target_include_directories(App
    PRIVATE
        ${APP_INC}
        ${PHYSX_DIR}/physx/include
)

# Link libraries - PhysX libraries need specific ordering
if(UNIX AND NOT APPLE)
    # Linux linking with proper grouping
    target_link_libraries(App
        PRIVATE
            glfw3webgpu
            webgpu
            glm
            glfw
            imgui
            spdlog
            -Wl,--start-group
            PhysXExtensions_static_64
            PhysX_static_64
            PhysXPvdSDK_static_64
            PhysXCommon_static_64
            PhysXFoundation_static_64
            -Wl,--end-group

            pthread
            dl
    )
else()
    # Windows/Mac linking
    target_link_libraries(App
        PRIVATE
            glfw3webgpu
            webgpu
            glm
            glfw
            imgui
            spdlog
            PhysXExtensions_static_64
            PhysX_64
            PhysXPvdSDK_static_64
            PhysXCommon_64
            PhysXFoundation_64
    )
endif()

target_copy_webgpu_binaries(App)