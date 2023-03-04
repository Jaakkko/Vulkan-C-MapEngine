#define GLFW_INCLUDE_VULKAN

#include <GLFW/glfw3.h>

#include <glm/ext/matrix_transform.hpp>

#include <iostream>
#include <stdexcept>
#include <chrono>
#include <forward_list>

#include "VulkanRenderer.h"
#include "VulkanTile.h"
#include "View.h"


void main_throws() {
    struct MousePos {
        double winX;
        double winY;
    };
    struct WindowContext {
        View *view;
        std::optional<MousePos> lastMousePos;
    } windowContext{};

    glfwInit();
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    GLFWwindow *window;
    window = glfwCreateWindow(1920, 1080, "Vulkan", nullptr, nullptr);
    glfwSetWindowUserPointer(window, &windowContext);
    glfwSetKeyCallback(window, [](GLFWwindow *window, int key, int scancode, int action, int mods) {
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
            glfwSetWindowShouldClose(window, GLFW_TRUE);
    });

    glfwSetCursorPosCallback(window, [](GLFWwindow *window, double xpos, double ypos) {
        auto context = static_cast<WindowContext *>(glfwGetWindowUserPointer(window));
        double winX, winY;
        glfwGetCursorPos(window, &winX, &winY);
        if (context->lastMousePos.has_value() && GLFW_PRESS == glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT)) {
            auto& mousePos = context->lastMousePos.value();
            context->view->translate(static_cast<float>(winX - mousePos.winX), static_cast<float>(winY - mousePos.winY));
        }
        context->lastMousePos = {winX,winY};
    });

    glfwSetScrollCallback(window, [](GLFWwindow *window, double xoffset, double yoffset) {
        auto context = static_cast<WindowContext *>(glfwGetWindowUserPointer(window));
        float scaleFactor = 1 - static_cast<float>(yoffset) * .01f;

        double winX, winY;
        glfwGetCursorPos(window, &winX, &winY);
        context->view->zoom(scaleFactor, static_cast<float>(winX), static_cast<float>(winY));
    });

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

    int windowWidth, windowHeight;
    glfwGetWindowSize(window, &windowWidth, &windowHeight);
    View view(0, 0, 0, 2, static_cast<float>(windowWidth), static_cast<float>(windowHeight));
    windowContext.view = &view;

    /*
     * Tile listan testailua
     */
    std::vector<ViewportTile> tileList = view.getTiles();

    VulkanTile tile(renderer);
    std::vector<ViewportTile> tileList = view.getTiles();
    std::forward_list<std::function<void(VkCommandBuffer)>> list = {
            [&](VkCommandBuffer commandBuffer) {
                for (const auto &t: tileList) {
                    tile.render(commandBuffer, t.center.x, t.center.y, t.tileSide, view.getViewMatrix());
                }
            },
    };

    auto fpsStartTime = std::chrono::system_clock::now();
    auto frames = 0;
    while (!glfwWindowShouldClose(window)) {
        frames++;
        auto now = std::chrono::system_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - fpsStartTime).count() > 1000) {
            fpsStartTime = now;
            std::cout << frames << std::endl;
            frames = 0;
        }
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