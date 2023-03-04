//
// Created by JaaK on 28.2.2023.
//

#include "View.h"

void View::updateViewMatrix() {
    viewMatrix = glm::mat4(1);
    viewMatrix = glm::scale(viewMatrix, glm::vec3(2 / size.x, -2 / size.y, 1));
    viewMatrix = glm::rotate(viewMatrix, angle, glm::vec3(0, 0, 1));
    viewMatrix = glm::translate(viewMatrix, glm::vec3(-center, 0));

    glm::mat4 windowToVulkan(1);
    windowToVulkan = glm::translate(windowToVulkan, glm::vec3(-1, -1, 0));
    windowToVulkan = glm::scale(windowToVulkan, glm::vec3(2 / windowSize.x, 2 / windowSize.y, 1));

    glm::mat4 vulkanToMap(1);
    vulkanToMap = glm::translate(vulkanToMap, glm::vec3(center, 0));
    vulkanToMap = glm::rotate(vulkanToMap, -angle, glm::vec3(0, 0, 1));
    vulkanToMap = glm::scale(vulkanToMap, glm::vec3(size.x / 2, -size.y / 2, 1));

    windowToMapMatrix = vulkanToMap * windowToVulkan;
}

void View::setAngle(float angle) {
    this->angle = angle;
    updateViewMatrix();
}

void View::zoom(float scaleFactor, float winX, float winY) {
    MapVec map = (windowToMapMatrix * glm::vec4(winX, winY, 0, 1)).xy;
    center = scaleFactor * (center - map) + map;
    size.x *= scaleFactor;
    size.y = size.x * windowSize.y / windowSize.x;
    updateViewMatrix();
}

void View::translate(float winDX, float winDY) {
    auto r = windowToMapMatrix * glm::mat2x4(
            glm::vec4(0, 0, 0, 1),
            glm::vec4(winDX, winDY, 0, 1)
    );
    center = center + glm::column(r, 0).xy - glm::column(r, 1).xy;
    updateViewMatrix();
}

std::vector<ViewportTile> View::getTiles() {
    // TODO: optimize

    const auto mapCoords = windowToMapMatrix * glm::mat4x4(
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

    MapVec boundingBoxLeftTop(1, -1);
    MapVec boundingBoxRightBottom(-1, 1);
    for (auto viewCorner: viewCorners) {
        boundingBoxLeftTop.x = std::min(boundingBoxLeftTop.x, viewCorner.x);
        boundingBoxLeftTop.y = std::max(boundingBoxLeftTop.y, viewCorner.y);
        boundingBoxRightBottom.x = std::max(boundingBoxRightBottom.x, viewCorner.x);
        boundingBoxRightBottom.y = std::min(boundingBoxRightBottom.y, viewCorner.y);
    }
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
        vulkanToWindow = glm::scale(vulkanToWindow, glm::vec3(windowSize.x / 2, windowSize.y / 2, 1));
        vulkanToWindow = glm::translate(vulkanToWindow, glm::vec3(1, 1, 0));
        auto r = vulkanToWindow * viewMatrix * glm::mat2x4(
                glm::vec4(maxDiffVec, 0, 1),
                glm::vec4(0, 0, 0, 1)
        );
        pixels = glm::length(glm::column(r, 0) - glm::column(r, 1));
    }

    // TODO: Do not draw tiles that are not in the window
    const int layer = floor(log2(2 / maxDiff * pixels / 256));
    const auto tilesPerDimensionInMap = static_cast<double>(1U << layer);
    const auto tileSize = static_cast<float>(2 / tilesPerDimensionInMap);
    const int hCount = static_cast<int>(1 + ceil((boundingBoxRightBottom.x - boundingBoxLeftTop.x) / tileSize));
    const int vCount = static_cast<int>(1 + ceil((boundingBoxLeftTop.y - boundingBoxRightBottom.y) / tileSize));
    auto roundToNearestTileCenter = std::function < MapVec(MapVec) > ([=](MapVec a) {
        return MapVec(
                round((a.x + 1) / 2 * tilesPerDimensionInMap) / tilesPerDimensionInMap * 2 - 1,
                round((a.y - 1) / 2 * tilesPerDimensionInMap) / tilesPerDimensionInMap * 2 + 1
        );
    });
    MapVec tileHorPos;
    MapVec tileVerPos = glm::vec2(0, 0);

    std::vector<ViewportTile> output;
    for (int j = 0; j < vCount; j++) {
        tileHorPos = glm::vec2(0, 0);
        for (int i = 0; i < hCount; i++) {
            // should be rounded
            output.push_back({
                                     roundToNearestTileCenter(boundingBoxLeftTop + tileHorPos + tileVerPos),
                                     tileSize
                             });
            tileHorPos.x += tileSize;
        }
        tileVerPos.y -= tileSize;
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
        : center(cx, cy), angle(angle), size(width, width * windowHeight / windowWidth),
          windowSize(windowWidth, windowHeight) {
    updateViewMatrix();
}
