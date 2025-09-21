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

target_include_directories(App
    PRIVATE
        ${APP_INC}
)

target_link_libraries(App
    PRIVATE
        glfw
        glfw3webgpu
        webgpu
        spdlog
        glm
)

target_copy_webgpu_binaries(App)