cmake_minimum_required(VERSION 3.24)
project(MapEngine)

set(CMAKE_CXX_STANDARD 20)

add_compile_definitions(
        VK_USE_PLATFORM_WIN32_KHR
        GLM_FORCE_SWIZZLE
)

include_directories(
        C:/VulkanSDK/1.3.239.0/Include
        C:/Libraries/glfw-3.3.8.bin.WIN64/include
)

add_executable(MapEngine main.cpp VulkanRenderer.cpp debug_messenger.cpp VulkanTile.cpp View.cpp)

target_link_libraries(MapEngine
        C:/Libraries/glfw-3.3.8.bin.WIN64/lib-vc2022/glfw3.lib
        C:/VulkanSDK/1.3.239.0/Lib/vulkan-1.lib
        )

add_custom_target(build_shaders
        COMMAND ../shaders/compile.bat
        WORKING_DIRECTORY ../shaders)