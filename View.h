//
// Created by JaaK on 28.2.2023.
//

#ifndef MAPENGINE_VIEW_H
#define MAPENGINE_VIEW_H

#include <iostream>
#include <vector>
#include <array>
#include <glm/glm.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/matrix_access.hpp>


typedef glm::highp_vec2 MapVec;
typedef glm::highp_vec2 WindowVec;

struct ViewportTile {
    MapVec center;
    float tileSide;
};

class View {
private:
    MapVec center;
    float angle;
    MapVec size; // zoom
    WindowVec windowSize;

    // map coord to vulkan coord
    glm::mat4 viewMatrix{};

    // window coord to map coord
    glm::mat4 windowToMapMatrix{};

    // map coord to window coord

    void updateViewMatrix();

public:
    /**
     * Camera
     *
     * x=-1 -> map's left border
     * x=1 -> map's right border
     * y=1 -> map's top border
     * y=-1 -> map's bottom border
     *
     * @param cx center x in map units
     * @param cy center y in map units
     * @param angle view angle in radians
     * @param width view width in map units
     * @param windowWidth in pixels
     * @param windowHeight in pixels
     */
    explicit View(
            float cx,
            float cy,
            float angle,
            float width,
            float windowWidth,
            float windowHeight
    );

    void zoom(float scaleFactor, float winX, float winY);

    [[nodiscard]] const glm::mat4 &getViewMatrix() const {
        return this->viewMatrix;
    };

    std::vector<ViewportTile> getTiles();
};


#endif //MAPENGINE_VIEW_H
