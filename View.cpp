//
// Created by JaaK on 28.2.2023.
//

#include "View.h"

void View::updateViewMatrix() {
    this->viewMatrix = glm::mat4(1);
    this->viewMatrix = glm::scale(this->viewMatrix, glm::vec3(2 / size.x, -2 / size.y, 1));
    this->viewMatrix = glm::rotate(this->viewMatrix, angle, glm::vec3(0, 0, 1));
    this->viewMatrix = glm::translate(this->viewMatrix, glm::vec3(-center, 0));
}

void View::zoom(float scaleFactor, float winX, float winY) {
    float vulkanX = winX / windowSize.x * 2 - 1;
    float vulkanY = winY / windowSize.y * 2 - 1;
    glm::mat4 vulkanToMap(1);
    vulkanToMap = glm::translate(vulkanToMap, glm::vec3(center, 0));
    vulkanToMap = glm::rotate(vulkanToMap, -angle, glm::vec3(0, 0, 1));
    vulkanToMap = glm::scale(vulkanToMap, glm::vec3(size.x / 2, -size.y / 2, 1));
    MapVec map = (vulkanToMap * glm::vec4(vulkanX, vulkanY, 0, 1)).xy();
    center = scaleFactor * (center - map) + map;
    size.x *= scaleFactor;
    size.y = size.x * windowSize.y / windowSize.x;
    updateViewMatrix();
}

std::vector<ViewportTile> View::getTiles() {
    /*
     * get top left coords
     * get top right coords
     * get bottom left coords
     * calculate h and v
     * calculate delta
     */
    glm::highp_mat4 M;
    float tileSizeInPixels = 256;

    return {};
}

View::View(
        float cx,
        float cy,
        float angle,
        float width,
        float windowWidth,
        float windowHeight
)
        : center(cx, cy), angle(angle), size(width, width * windowHeight / windowWidth),
          windowSize(windowWidth, windowHeight), viewMatrix({}) {
    updateViewMatrix();
}