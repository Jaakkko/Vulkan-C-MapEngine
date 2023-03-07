//
// Created by JaaK on 28.2.2023.
//

#ifndef MAPENGINE_VIEW_H
#define MAPENGINE_VIEW_H

#include <vector>
#include <array>
#include <functional>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/matrix_access.hpp>

typedef glm::vec2 MapVec;
typedef glm::vec2 WindowVec;

struct TileVec {
    MapVec center;
    float tileSide;
    uint32_t row;
    uint32_t column;
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

    void scale(float scaleFactor);
    void boundingBox(MapVec& boundingBoxLeftTop, MapVec& boundingBoxRightBottom) const;

    void calculateViewMatrix();
    void calculateWindowToMapMatrix();

    void limitZoom(MapVec previousCenter, MapVec previousSize, float scaleFactor, MapVec zoomCenter);
    void limitTranslation();

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

    void rotate(float deltaAngle, float winX, float winY);
    void zoom(float scaleFactor, float winX, float winY);
    void translate(float winDX, float winDY);

    [[nodiscard]] const glm::mat4 &getViewMatrix() const {
        return this->viewMatrix;
    };

    std::vector<TileVec> getTiles();
};


#endif //MAPENGINE_VIEW_H
