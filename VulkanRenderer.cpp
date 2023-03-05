//
// Created by JaaK on 26.2.2023.
//

#include "VulkanRenderer.h"


VulkanRenderer::VulkanRenderer(const std::function<void(VkInstance instance, VkSurfaceKHR *surface)> &createSurface,
                               const std::function<void(uint32_t* width, uint32_t* height)>& getWindowSize) {
    // instance
    {
        const char *layers[]{
                "VK_LAYER_KHRONOS_validation"
        };

        std::vector<const char *> extensions{
                VK_KHR_SURFACE_EXTENSION_NAME,
                VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
                VK_EXT_DEBUG_UTILS_EXTENSION_NAME
        };
        VkApplicationInfo appInfo{
                .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                .pApplicationName = "Hello Triangle",
                .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
                .pEngineName = "No Engine",
                .engineVersion = VK_MAKE_VERSION(1, 0, 0),
                .apiVersion = VK_API_VERSION_1_3,
        };

        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{
                .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
                .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                   VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                   VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
                .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
                .pfnUserCallback = debugCallback,
        };

        VkInstanceCreateInfo createInfo{
                .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                .pNext = (VkDebugUtilsMessengerCreateInfoEXT *) &debugCreateInfo,
                .flags = 0,
                .pApplicationInfo = &appInfo,
                .enabledLayerCount = 1,
                .ppEnabledLayerNames = layers,
                .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
                .ppEnabledExtensionNames = extensions.data(),
        };

        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
            throw std::runtime_error("failed to create instance!");
        }

        CreateDebugUtilsMessengerEXT(instance, &debugCreateInfo, nullptr, &debugMessenger);

        resourceStack.emplace([=]() {
            DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
            vkDestroyInstance(instance, nullptr);
        });
    }

    // surface
    createSurface(instance, &surface);
    resourceStack.emplace([=]() {
        vkDestroySurfaceKHR(instance, surface, nullptr);
    });

    std::vector<const char *> requiredDeviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    // pick physical device
    uint32_t physicalDeviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr);
    std::vector<VkPhysicalDevice> devices(physicalDeviceCount);
    vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, devices.data());
    // Check for required extensions
    for (auto it = devices.begin(); it != devices.end(); ++it) {
        uint32_t count;
        vkEnumerateDeviceExtensionProperties(*it, nullptr, &count, nullptr);
        std::vector<VkExtensionProperties> extensions(count);
        vkEnumerateDeviceExtensionProperties(*it, nullptr, &count, extensions.data());
        for (const char *extensionName: requiredDeviceExtensions) {
            bool found = false;
            for (auto &extension: extensions) {
                if (std::strcmp(extensionName, extension.extensionName) == 0) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                devices.erase(it);
            }
        }
    }

    // Sort. device with the lowest score wins
    std::array<int, 5> score{
            VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU,
            VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU,
            VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU,
            VK_PHYSICAL_DEVICE_TYPE_CPU,
            VK_PHYSICAL_DEVICE_TYPE_OTHER,
    };
    std::sort(devices.begin(), devices.end(), [&](VkPhysicalDevice &aD, VkPhysicalDevice &bD) {
        VkPhysicalDeviceProperties a;
        VkPhysicalDeviceProperties b;
        vkGetPhysicalDeviceProperties(aD, &a);
        vkGetPhysicalDeviceProperties(bD, &b);
        auto aScore = std::distance(score.begin(), std::find(score.begin(), score.end(), a.deviceType));
        auto bScore = std::distance(score.begin(), std::find(score.begin(), score.end(), b.deviceType));
        return aScore < bScore;
    });
    auto deviceIt = devices.begin();
    for (; deviceIt != devices.end(); ++deviceIt) {
        physicalDevice = *deviceIt;

        uint32_t count;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &count, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilyProperties(count);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &count, queueFamilyProperties.data());

        float queuePriority = 1.0f;
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

        for (uint32_t i = 0; i < count; ++i) {
            bool pushCreateInfo = false;
            VkDeviceQueueCreateInfo queueCreateInfo = {
                    .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                    .queueFamilyIndex = i,
                    .queueCount = 1,
                    .pQueuePriorities = &queuePriority,
            };

            if (queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                graphicsQueue.familyIndex = i;
                pushCreateInfo = true;
            }

            VkBool32 surfaceSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &surfaceSupport);
            if (surfaceSupport) {
                surfaceQueue.familyIndex = i;
                pushCreateInfo = true;
            }

            if (pushCreateInfo) {
                queueCreateInfos.push_back(queueCreateInfo);
            }
        }
        if (!surfaceQueue.familyIndex.has_value() || !graphicsQueue.familyIndex.has_value()) {
            continue;
        }

        std::vector<const char *> extensions{
                VK_KHR_SWAPCHAIN_EXTENSION_NAME
        };

        VkPhysicalDeviceVulkan13Features features = {
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
                .dynamicRendering = true
        };

        VkDeviceCreateInfo deviceCreateInfo{
                .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                .pNext = &features,
                .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
                .pQueueCreateInfos = queueCreateInfos.data(),
                .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
                .ppEnabledExtensionNames = extensions.data(),
        };
        vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device);
        resourceStack.emplace([=]() {
            vkDestroyDevice(device, nullptr);
        });

        vkGetDeviceQueue(device, *graphicsQueue.familyIndex, 0, &graphicsQueue.queue);
        vkGetDeviceQueue(device, *surfaceQueue.familyIndex, 0, &surfaceQueue.queue);
        break;
    }
    if (deviceIt == devices.end()) {
        throw std::runtime_error("Device not found!");
    }

    // Create swapchain
    {
        VkSurfaceCapabilitiesKHR capabilities;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities);

        VkSurfaceFormatKHR surfaceFormat;
        {
            uint32_t formatCount;
            vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
            if (formatCount != 0) {
                std::vector<VkSurfaceFormatKHR> formats;
                formats.resize(formatCount);
                vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.data());
                for (const auto &availableFormat: formats) {
                    if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
                        availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                        surfaceFormat = availableFormat;
                        swapchainImageFormat = availableFormat.format;
                        break;
                    }
                }
            } else throw std::runtime_error("formatCount = 0");
        }

        VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
        {
            uint32_t presentModeCount;
            vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);

            if (presentModeCount != 0) {
                std::vector<VkPresentModeKHR> presentModes;
                presentModes.resize(presentModeCount);
                vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount,
                                                          presentModes.data());

                for (const auto &availablePresentMode: presentModes) {
                    if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                        presentMode = availablePresentMode;
                        break;
                    }
                }
            }
        }

        VkExtent2D extent;
        if (capabilities.currentExtent.width != 0xFFFFFFFF) {
            extent = capabilities.currentExtent;
        } else {
            uint32_t width, height;
            getWindowSize(&width, &height);

            VkExtent2D actualExtent = {
                    static_cast<uint32_t>(width),
                    static_cast<uint32_t>(height)
            };

            actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width,
                                            capabilities.maxImageExtent.width);
            actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height,
                                             capabilities.maxImageExtent.height);

            extent = actualExtent;
        }
        renderArea.extent = extent;

        VkSwapchainCreateInfoKHR createInfo = {
                .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
                .surface = surface,
                .minImageCount = std::clamp(capabilities.minImageCount + 1, capabilities.minImageCount, capabilities.maxImageCount),
                .imageFormat = surfaceFormat.format,
                .imageColorSpace = surfaceFormat.colorSpace,
                .imageExtent = extent,
                .imageArrayLayers = 1,
                .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
                .preTransform = capabilities.currentTransform,
                .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
                .presentMode = presentMode,
                .clipped = VK_TRUE,
        };

        if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create swapchain");
        }

        resourceStack.emplace([=](){
            vkDestroySwapchainKHR(device, swapchain, nullptr);
        });
    }

    // determine records size
    // setup swapchain images
    {
        uint32_t imageCount;
        vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
        // max frames in flight = 2
        records.resize(min(2, imageCount));
        swapchainImages.resize(imageCount);

        std::vector<VkImage> images(imageCount);
        vkGetSwapchainImagesKHR(device, swapchain, &imageCount, images.data());

        for (int i = 0; i < imageCount; i++) {
            SwapchainImage& swapchainImage = swapchainImages[i];
            swapchainImage.image = images[i];

            VkImageViewCreateInfo createInfo{
                    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                    .image = images[i],
                    .viewType = VK_IMAGE_VIEW_TYPE_2D,
                    .format = swapchainImageFormat,
                    .components = {
                            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                            .a = VK_COMPONENT_SWIZZLE_IDENTITY,
                    },
                    .subresourceRange = {
                            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                            .baseMipLevel = 0,
                            .levelCount = 1,
                            .baseArrayLayer = 0,
                            .layerCount = 1,
                    }
            };

            if (vkCreateImageView(device, &createInfo, nullptr, &swapchainImage.imageView) !=
                VK_SUCCESS) {
                throw std::runtime_error("failed to create image views!");
            }

            resourceStack.emplace([=](){
                vkDestroyImageView(device, swapchainImage.imageView, nullptr);
            });
        }
    }

    // Command Pool
    {
        VkCommandPoolCreateInfo createInfo = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                .queueFamilyIndex = *graphicsQueue.familyIndex
        };
        vkCreateCommandPool(device, &createInfo, nullptr, &commandPool);
        resourceStack.emplace([=](){
            vkDestroyCommandPool(device, commandPool, nullptr);
        });
    }

    // Record fields
    {
        auto numOfRecords = static_cast<uint32_t>(records.size());

        std::vector<VkCommandBuffer> commandBuffers(numOfRecords);
        VkCommandBufferAllocateInfo allocateInfo = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .commandPool = commandPool,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = numOfRecords
        };
        vkAllocateCommandBuffers(device, &allocateInfo, commandBuffers.data());
        resourceStack.emplace([=](){
            vkFreeCommandBuffers(device, commandPool, numOfRecords, commandBuffers.data());
        });

        for (int i = 0; i < numOfRecords; i++) {
            Record& record = records[i];
            record.commandBuffer = commandBuffers[i];

            VkFenceCreateInfo fenceCreateInfo = {
                    .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                    .flags = VK_FENCE_CREATE_SIGNALED_BIT,
            };
            vkCreateFence(device, &fenceCreateInfo, nullptr, &record.inFlightFence);

            VkSemaphoreCreateInfo semaphoreCreateInfo = {
                    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            };
            vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &record.imageAvailableSemaphore);
            vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &record.renderFinishedSemaphore);

            resourceStack.emplace([=](){
                vkDestroySemaphore(device, record.imageAvailableSemaphore, nullptr);
                vkDestroySemaphore(device, record.renderFinishedSemaphore, nullptr);
                vkDestroyFence(device, record.inFlightFence, nullptr);
            });
        }
    }
}

VulkanRenderer::~VulkanRenderer() {
    for (const auto& record : records) {
        vkWaitForFences(device, 1, &record.inFlightFence, VK_TRUE, UINT64_MAX);
    }

    while (!resourceStack.empty()) {
        auto freeFunc = resourceStack.top();
        freeFunc();
        resourceStack.pop();
    }
}

void VulkanRenderer::nextFrame(const std::forward_list<std::function<void(VkCommandBuffer)>>& renderingList) {
    auto &[
            commandBuffer,
            inFlightFence,
            imageAvailableSemaphore,
            renderFinishedSemaphore
    ] = records[currentFrame];

    vkWaitForFences(device, 1, &inFlightFence, VK_TRUE, UINT64_MAX);
    vkResetFences(device, 1, &inFlightFence);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX,
                                            imageAvailableSemaphore, VK_NULL_HANDLE,
                                            &imageIndex);
    auto &[swapchainImage, swapchainImageView] = swapchainImages[imageIndex];

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        throw std::runtime_error("OUT OF DATE");
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("failed to acquire swap chain swapchainImage!");
    }

    vkResetCommandBuffer(commandBuffer, 0);
    VkCommandBufferBeginInfo beginInfo {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    VkImageMemoryBarrier imageMemoryBarrier {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .image = swapchainImage,
            .subresourceRange = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
            }
    };

    vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,  // srcStageMask
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // dstStageMask
            0,
            0,
            nullptr,
            0,
            nullptr,
            1, // imageMemoryBarrierCount
            &imageMemoryBarrier // pImageMemoryBarriers
    );

    VkRenderingAttachmentInfo colorAttachment {
            .sType=VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView=swapchainImageView,
            .imageLayout=VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR,
            .resolveMode=VK_RESOLVE_MODE_NONE,
            .loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp=VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue={0.0f, 0.0f, 0.0f, 1.0f}
    };
    VkRenderingInfo renderingInfo {
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .renderArea = renderArea,
            .layerCount=1,
            .colorAttachmentCount=1,
            .pColorAttachments=&colorAttachment,
    };

    VkViewport viewport{
            .width=static_cast<float>(renderArea.extent.width),
            .height=static_cast<float>(renderArea.extent.height),
            .minDepth=0.0f,
            .maxDepth=1.0f,
    };
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{
            .extent=renderArea.extent
    };
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    vkCmdBeginRendering(commandBuffer, &renderingInfo);
    for (const auto& queue : renderingList) {
        queue(commandBuffer);
    }
    vkCmdEndRendering(commandBuffer);

    imageMemoryBarrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            .image = swapchainImage,
            .subresourceRange = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
            }
    };

    vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,  // srcStageMask
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, // dstStageMask
            0,
            0,
            nullptr,
            0,
            nullptr,
            1, // imageMemoryBarrierCount
            &imageMemoryBarrier // pImageMemoryBarriers
    );

    vkEndCommandBuffer(commandBuffer);

    // submit
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSubmitInfo submitInfo = {
            .sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &imageAvailableSemaphore,
            .pWaitDstStageMask = waitStages,
            .commandBufferCount = 1,
            .pCommandBuffers = &commandBuffer,
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &renderFinishedSemaphore
    };
    if (vkQueueSubmit(graphicsQueue.queue, 1, &submitInfo, inFlightFence) != VK_SUCCESS) {
        throw std::runtime_error("failed to submit draw command buffer!");
    }

    VkPresentInfoKHR presentInfo = {
            .sType=VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &renderFinishedSemaphore,
            .swapchainCount = 1,
            .pSwapchains = &swapchain,
            .pImageIndices = &imageIndex
    };
    result = vkQueuePresentKHR(surfaceQueue.queue, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("out of date 2");
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("failed to present swap chain swapchainImage!");
    }

    currentFrame = (currentFrame + 1) % static_cast<int>(records.size());
}