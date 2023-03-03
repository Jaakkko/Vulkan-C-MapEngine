//
// Created by JaaK on 28.2.2023.
//

#ifndef MAPENGINE_VIEW_H
#define MAPENGINE_VIEW_H

#include <vector>
#include <glm/glm.hpp>
#include <glm/ext/matrix_transform.hpp>

struct ViewportTile {
    // in map coords
    float cx;
    float cy;
    float tileSide;
};

struct ViewInitDefault {
    float cx, cy;
    float angle;
    float width, height;
};
struct ViewInitWithWindowSize {
    float cx, cy;
    float angle;
    float windowWidth, windowHeight;
};

class View {
private:
    float cx, cy;
    float angle;
    float width, height;
    float windowWidth, windowHeight;
    glm::mat4 viewMatrix;

    void updateViewMatrix();

public:
    explicit View(
            float cx,
            float cy,
            float angle,
            float width,
            float windowWidth,
            float windowHeight
    );

    void zoom(float scaleFactor, float winX, float winY);

    [[nodiscard]] const glm::mat4 &getViewMatrix() const { return this->viewMatrix; };

    std::vector<ViewportTile> getTiles();
};


#endif //MAPENGINE_VIEW_H
