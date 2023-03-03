//
// Created by JaaK on 28.2.2023.
//

#include "View.h"

void View::updateViewMatrix() {
    this->viewMatrix = glm::mat4(1);
    this->viewMatrix = glm::translate(this->viewMatrix, glm::vec3(-cx, -cy, 0));
    this->viewMatrix = glm::scale(this->viewMatrix, glm::vec3(2 / width, 2 / height, 0));
    this->viewMatrix = glm::rotate(this->viewMatrix, angle, glm::vec3(0, 0, 1));
    this->viewMatrix = glm::translate(this->viewMatrix, glm::vec3(cx, cy, 0));
}

void View::zoom(float scaleFactor) {
    this->width *= scaleFactor;
    this->height *= scaleFactor;
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
) : cx(cx),cy(cy),angle(angle),width(width),height(width*windowHeight/windowWidth),viewMatrix({}){
    updateViewMatrix();
}