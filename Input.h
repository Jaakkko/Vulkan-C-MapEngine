//
// Created by JaaK on 5.3.2023.
//

#ifndef MAPENGINE_INPUT_H
#define MAPENGINE_INPUT_H

#include <GLFW/glfw3.h>
#include <optional>

#include "View.h"

void handleInput(GLFWwindow *window, View* view) {
    struct MousePos {
        double winX;
        double winY;
    };
    struct WindowContext {
        View* view{};
        std::optional<MousePos> lastMousePos;
    };

    auto* windowContext = new WindowContext;
    windowContext->view = view;

    glfwSetWindowUserPointer(window, windowContext);
    glfwSetKeyCallback(window, [](GLFWwindow *window, int key, int scancode, int action, int mods) {
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
            glfwSetWindowShouldClose(window, GLFW_TRUE);
    });

    glfwSetCursorPosCallback(window, [](GLFWwindow *window, double xpos, double ypos) {
        auto context = static_cast<WindowContext *>(glfwGetWindowUserPointer(window));
        double winX, winY;
        glfwGetCursorPos(window, &winX, &winY);
        if (context->lastMousePos.has_value() && GLFW_PRESS == glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT)) {
            auto &mousePos = context->lastMousePos.value();
            context->view->translate(static_cast<float>(winX - mousePos.winX),
                                     static_cast<float>(winY - mousePos.winY));
        }
        context->lastMousePos = {winX, winY};
    });

    glfwSetScrollCallback(window, [](GLFWwindow *window, double xoffset, double yoffset) {
        auto context = static_cast<WindowContext *>(glfwGetWindowUserPointer(window));
        float scaleFactor = 1 - static_cast<float>(yoffset) * .1f;

        double winX, winY;
        glfwGetCursorPos(window, &winX, &winY);
        context->view->zoom(scaleFactor, static_cast<float>(winX), static_cast<float>(winY));
    });

    glfwSetWindowCloseCallback(window, [](GLFWwindow* window){
        auto context = static_cast<WindowContext *>(glfwGetWindowUserPointer(window));
        delete context;
    });
}

#endif //MAPENGINE_INPUT_H
