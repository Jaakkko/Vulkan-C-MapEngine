//
// Created by JaaK on 28.2.2023.
//

#include "View.h"

void View::scale(float scaleFactor) {
    auto size = transformation.size.get();
    auto windowSize = transformation.windowSize.get();
    size.x *= scaleFactor;
    size.y = size.x * windowSize.y / windowSize.x;
    transformation.size = size;
}

void View::limitZoom(MapVec previousCenter, MapVec previousSize, float scaleFactor, MapVec zoomCenter) {
    MapVec boundingBoxLeftTop;
    MapVec boundingBoxRightBottom;
    boundingBox(boundingBoxLeftTop, boundingBoxRightBottom);
    if (boundingBoxRightBottom.x - boundingBoxLeftTop.x > 2 || boundingBoxLeftTop.y - boundingBoxRightBottom.y > 2) {
        auto negInf = -std::numeric_limits<float>::infinity();
        scaleFactor *= std::min(
                std::nextafterf(2 / (boundingBoxRightBottom.x - boundingBoxLeftTop.x), negInf),
                std::nextafterf(2 / (boundingBoxLeftTop.y - boundingBoxRightBottom.y), negInf)
        );

        transformation.center = scaleFactor * (previousCenter - zoomCenter) + zoomCenter;
        transformation.size = previousSize;
        scale(scaleFactor);
    }
}

void View::limitTranslation() {
    auto center = transformation.center.get();
    MapVec boundingBoxLeftTop;
    MapVec boundingBoxRightBottom;
    boundingBox(boundingBoxLeftTop, boundingBoxRightBottom);
    if (boundingBoxLeftTop.x < -1) {
        center.x += -1 - boundingBoxLeftTop.x;
    }
    if (boundingBoxRightBottom.x > 1) {
        center.x += 1 - boundingBoxRightBottom.x;
    }
    if (boundingBoxRightBottom.y < -1) {
        center.y += -1 - boundingBoxRightBottom.y;
    }
    if (boundingBoxLeftTop.y > 1) {
        center.y += 1 - boundingBoxLeftTop.y;
    }
    transformation.center = center;
}

void View::boundingBox(MapVec &boundingBoxLeftTop, MapVec &boundingBoxRightBottom) {
    auto windowSize = transformation.windowSize.get();
    const auto mapCoords = transformation.getWindowToMapMatrix() * glm::mat4(
            glm::vec4(0, 0, 0, 1),
            glm::vec4(windowSize.x, 0, 0, 1),
            glm::vec4(0, windowSize.y, 0, 1),
            glm::vec4(windowSize.x, windowSize.y, 0, 1)
    );
    const MapVec mapTopLeft = glm::column(mapCoords, 0).xy;
    const MapVec mapTopRight = glm::column(mapCoords, 1).xy;
    const MapVec mapBottomLeft = glm::column(mapCoords, 2).xy;
    const MapVec mapBottomRight = glm::column(mapCoords, 3).xy;
    const std::array<MapVec, 4> viewCorners = {mapTopLeft, mapTopRight, mapBottomLeft, mapBottomRight};
    boundingBoxLeftTop = MapVec(1, -1);
    boundingBoxRightBottom = MapVec(-1, 1);
    for (auto viewCorner: viewCorners) {
        boundingBoxLeftTop.x = std::min(boundingBoxLeftTop.x, viewCorner.x);
        boundingBoxLeftTop.y = std::max(boundingBoxLeftTop.y, viewCorner.y);
        boundingBoxRightBottom.x = std::max(boundingBoxRightBottom.x, viewCorner.x);
        boundingBoxRightBottom.y = std::min(boundingBoxRightBottom.y, viewCorner.y);
    }
}

void View::rotate(float deltaAngle, float winX, float winY) {
    auto center = transformation.center.get();
    auto unchangedCenter = center;
    auto z = glm::vec3(0, 0, 1);
    MapVec map = (transformation.getWindowToMapMatrix() * glm::vec4(winX, winY, 0, 1)).xy;
    center -= map;
    center = (glm::rotate(glm::mat4(1), -deltaAngle, z) * glm::vec4(center, 0, 1)).xy;
    center += map;
    transformation.center = center;
    transformation.angle += deltaAngle;

    map = (transformation.getWindowToMapMatrix() * glm::vec4(winX, winY, 0, 1)).xy;
    limitZoom(unchangedCenter, transformation.size.get(), 1, map);
    limitTranslation();
}

void View::zoom(float scaleFactor, float winX, float winY) {
    MapVec map = (transformation.getWindowToMapMatrix() * glm::vec4(winX, winY, 0, 1)).xy;
    auto unchangedCenter = transformation.center.get();
    auto unchangedSize = transformation.size.get();
    transformation.center = scaleFactor * (unchangedCenter - map) + map;
    scale(scaleFactor);

    map = (transformation.getWindowToMapMatrix() * glm::vec4(winX, winY, 0, 1)).xy;
    limitZoom(unchangedCenter, unchangedSize, scaleFactor, map);
    limitTranslation();
}

void View::translate(float winDX, float winDY) {
    auto r = transformation.getWindowToMapMatrix() * glm::mat2x4(
            glm::vec4(0, 0, 0, 1),
            glm::vec4(winDX, winDY, 0, 1)
    );
    transformation.center += glm::column(r, 0).xy - glm::column(r, 1).xy;
    limitTranslation();
}

std::vector<TileVec> View::getTiles() {
    // TODO: optimize 45 deg angle

    MapVec boundingBoxLeftTop;
    MapVec boundingBoxRightBottom;
    boundingBox(boundingBoxLeftTop, boundingBoxRightBottom);
    MapVec maxDiffVec(boundingBoxRightBottom.x - boundingBoxLeftTop.x, boundingBoxLeftTop.y - boundingBoxRightBottom.y);

    double pixels;
    double maxDiff;
    {
        if (maxDiffVec.x > maxDiffVec.y) {
            maxDiffVec.y = 0;
            maxDiff = maxDiffVec.x;
        } else {
            maxDiffVec.x = 0;
            maxDiff = maxDiffVec.y;
        }

        glm::mat4 vulkanToWindow(1);
        auto windowSize = transformation.windowSize.get();
        vulkanToWindow = glm::scale(vulkanToWindow, glm::vec3(windowSize.x / 2, windowSize.y / 2, 1));
        vulkanToWindow = glm::translate(vulkanToWindow, glm::vec3(1, 1, 0));
        auto r = vulkanToWindow * transformation.getViewMatrix() * glm::mat2x4(
                glm::vec4(maxDiffVec, 0, 1),
                glm::vec4(0, 0, 0, 1)
        );
        pixels = glm::length(glm::column(r, 0) - glm::column(r, 1));
    }

    // TODO: Do not draw tiles that are not in the window
    const int layer = floor(log2(2 / maxDiff * pixels / 256));
    const auto tilesPerDimensionInMap = static_cast<double>(1U << layer);
    const auto tileSide = static_cast<float>(2 / tilesPerDimensionInMap);
    const int hCount = static_cast<int>(1 + ceil((boundingBoxRightBottom.x - boundingBoxLeftTop.x) / tileSide));
    const int vCount = static_cast<int>(1 + ceil((boundingBoxLeftTop.y - boundingBoxRightBottom.y) / tileSide));
    MapVec tileHorPos;
    MapVec tileVerPos = glm::vec2(0, 0);

    std::vector<TileVec> output;
    for (int j = 0; j < vCount; j++) {
        tileHorPos = glm::vec2(0, 0);
        for (int i = 0; i < hCount; i++) {
            auto tilePos = boundingBoxLeftTop + tileHorPos + tileVerPos;
            double column = floor((tilePos.x + 1) / 2 * tilesPerDimensionInMap);
            double row = floor((tilePos.y - 1) / -2 * tilesPerDimensionInMap);
            if (column >= 0 && row >= 0 && column < (1 << layer) && row < (1 << layer)) {
                tilePos = MapVec(
                        static_cast<float>(column / tilesPerDimensionInMap * 2.f - 1.f + tileSide / 2),
                        static_cast<float>(row / tilesPerDimensionInMap * -2.f + 1.f - tileSide / 2)
                );

                output.push_back({
                                         tilePos,
                                         tileSide,
                                         static_cast<uint32_t>(row),
                                         static_cast<uint32_t>(column)
                                 });
            }

            tileHorPos.x += tileSide;
        }
        tileVerPos.y -= tileSide;
    }

    return output;
}

View::View(
        float cx,
        float cy,
        float angle,
        float width,
        float windowWidth,
        float windowHeight
)
        : transformation(cx, cy, angle, width, windowWidth, windowHeight) {
    limitZoom(transformation.center.get(), transformation.size.get(), 1, transformation.center.get());
    limitTranslation();
}

void View::Transformation::calculateWindowToMapMatrix() {
    glm::mat4 windowToVulkan(1);
    windowToVulkan = glm::translate(windowToVulkan, glm::vec3(-1, -1, 0));
    windowToVulkan = glm::scale(windowToVulkan, glm::vec3(2 / windowSize.get().x, 2 / windowSize.get().y, 1));

    glm::mat4 vulkanToMap(1);
    vulkanToMap = glm::translate(vulkanToMap, glm::vec3(center.get(), 0));
    vulkanToMap = glm::rotate(vulkanToMap, -angle.get(), glm::vec3(0, 0, 1));
    vulkanToMap = glm::scale(vulkanToMap, glm::vec3(size.get().x / 2, -size.get().y / 2, 1));

    windowToMapMatrix = vulkanToMap * windowToVulkan;
}

void View::Transformation::calculateViewMatrix() {
    viewMatrix = glm::mat4(1);
    viewMatrix = glm::scale(viewMatrix, glm::vec3(2 / size.get().x, -2 / size.get().y, 1));
    viewMatrix = glm::rotate(viewMatrix, angle.get(), glm::vec3(0, 0, 1));
    viewMatrix = glm::translate(viewMatrix, glm::vec3(-center.get(), 0));
}

const glm::mat4 &View::Transformation::getWindowToMapMatrix() {
    if (winToMapMatrixDirty) {
        calculateWindowToMapMatrix();
        winToMapMatrixDirty = false;
    }
    return windowToMapMatrix;
}

const glm::mat4 &View::Transformation::getViewMatrix() {
    if (viewMatrixDirty) {
        calculateViewMatrix();
        viewMatrixDirty = false;
    }
    return viewMatrix;
}

View::Transformation::Transformation(float cx, float cy, float angle, float width, float windowWidth, float windowHeight) :
        center{MapVec(cx, cy), winToMapMatrixDirty, viewMatrixDirty},
        angle{angle, winToMapMatrixDirty, viewMatrixDirty},
        size{MapVec(width, width * windowHeight / windowWidth), winToMapMatrixDirty, viewMatrixDirty},
        windowSize{WindowVec(windowWidth, windowHeight), winToMapMatrixDirty, viewMatrixDirty} {

}