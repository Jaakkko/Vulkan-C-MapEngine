//
// Created by JaaK on 26.2.2023.
//

#ifndef MAPENGINE_VULKANRENDERER_H
#define MAPENGINE_VULKANRENDERER_H

#include <vulkan/vulkan.h>
#include <vector>
#include <algorithm>
#include <array>
#include <functional>
#include <iostream>
#include <stack>
#include <optional>
#include <forward_list>

#include "debug_messenger.h"

class VulkanRenderer {
public:

    VkDebugUtilsMessengerEXT debugMessenger{};
    VkInstance instance{};
    VkPhysicalDevice physicalDevice{};

    VkRect2D renderArea{};
    VkFormat swapchainImageFormat;
    VkSwapchainKHR swapchain{};

    VkCommandPool commandPool;

    struct SwapchainImage {
        VkImage image;
        VkImageView imageView;
    };
    struct Record {
        VkCommandBuffer commandBuffer;
        VkFence inFlightFence;
        VkSemaphore imageAvailableSemaphore;
        VkSemaphore renderFinishedSemaphore;
    };

    int currentFrame = 0;
    std::vector<Record> records;
    std::vector<SwapchainImage> swapchainImages;

    struct {
        VkQueue queue = nullptr;
        std::optional<uint32_t> familyIndex;
    } graphicsQueue, surfaceQueue;

    std::stack<std::function<void()>> resourceStack;
    VkDevice device{};
    VkSurfaceKHR surface{};

    explicit VulkanRenderer(
            const std::function<void(VkInstance instance, VkSurfaceKHR* surface)>& createSurface,
            const std::function<void(uint32_t* width, uint32_t* height)>& getWindowSize
            );
    ~VulkanRenderer();

    void nextFrame(const std::forward_list<std::function<void(VkCommandBuffer)>>& renderingList);
};


#endif //MAPENGINE_VULKANRENDERER_H
