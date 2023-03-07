#define GLFW_INCLUDE_VULKAN
#define STB_IMAGE_IMPLEMENTATION

#include <GLFW/glfw3.h>

#include <glm/ext/matrix_transform.hpp>

#include <iostream>
#include <stdexcept>
#include <chrono>
#include <forward_list>

#include "VulkanRenderer.h"
#include "VulkanTile.h"
#include "View.h"
#include "Input.h"


void main_throws() {
    glfwInit();
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    GLFWwindow *window;
    window = glfwCreateWindow(1920, 1080, "Vulkan", nullptr, nullptr);

    int windowWidth, windowHeight;
    glfwGetWindowSize(window, &windowWidth, &windowHeight);
    View view(0, 0, 0, 2, static_cast<float>(windowWidth), static_cast<float>(windowHeight));
    handleInput(window, &view);

    VulkanRenderer renderer([=](VkInstance instance, VkSurfaceKHR *surface) {
        VkResult result;
        if ((result = glfwCreateWindowSurface(instance, window, nullptr, surface)) != VK_SUCCESS) {
            std::stringstream ss;
            ss << "failed to create window surface! code = ";
            ss << result;
            throw std::runtime_error(ss.str());
        } else {
            std::cout << "Surface created" << std::endl;
        }
    }, [=](uint32_t *w, uint32_t *h) {
        int intWidth, intHeight;
        glfwGetWindowSize(window, &intWidth, &intHeight);
        *w = intWidth;
        *h = intHeight;
    });

    VulkanTile tile(renderer);


    auto fpsStartTime = std::chrono::system_clock::now();
    auto frames = 0;
    while (!glfwWindowShouldClose(window)) {
        frames++;
        auto now = std::chrono::system_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - fpsStartTime).count() > 1000) {
            fpsStartTime = now;
            std::cout << "Frames: " << frames << std::endl;
            frames = 0;
        }
        std::vector<TileVec> tileList = view.getTiles();
        std::forward_list<std::function<void(VkCommandBuffer)>> list = {
                [&](VkCommandBuffer commandBuffer) {
                    for (const auto &t: tileList) {
                        tile.render(commandBuffer, t.center.x, t.center.y, t.tileSide, view.getViewMatrix());
                    }
                },
        };
        //angle += 1;
        renderer.nextFrame(list);

        // glfwPollEvents();
        glfwWaitEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
}

int main() {
    try {
        main_throws();
    } catch (std::exception &exception) {
        std::cout << exception.what() << std::endl;
    }

    return EXIT_SUCCESS;
}