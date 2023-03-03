//
// Created by JaaK on 28.2.2023.
//

#ifndef MAPENGINE_VULKANTILE_H
#define MAPENGINE_VULKANTILE_H

#include "VulkanRenderer.h"

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include <stdexcept>
#include <fstream>

class VulkanTile {
public:
    // TODO: optimoi niin, että vain yksi instanssi koko systeemissä
    VkPipeline graphicsPipeline{};
    VkPipelineLayout graphicsPipelineLayout{};
    VkBuffer vertexBuffer{};
    VkDeviceMemory vertexMemory{};

    explicit VulkanTile(VulkanRenderer& renderer);

    void render(VkCommandBuffer commandBuffer, float cx, float cy, float tileSide, const glm::mat4& viewMatrix);
};


#endif //MAPENGINE_VULKANTILE_H
