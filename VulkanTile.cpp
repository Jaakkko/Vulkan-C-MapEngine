//
// Created by JaaK on 28.2.2023.
//

#include "VulkanTile.h"

struct PushConstants {
    glm::vec4 tileSide; // float
    glm::vec4 center; // vec2
    glm::mat4 projection;
};

struct Vertex {
    glm::vec2 pos;
    glm::vec2 texCoord;
};

const static std::vector<Vertex> vertices = {
        {{-0.5f, +0.5f}, {0, 0}},
        {{+0.5f, +0.5f}, {1, 0}},
        {{+0.5f, -0.5f}, {1, 1}},
        {{-0.5f, -0.5f}, {0, 1}},
};

static uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("failed to find suitable memory type!");
}

VulkanTile::VulkanTile(VulkanRenderer &renderer) {
    this->renderer = &renderer;
    VkDevice device = renderer.device;
    VkPhysicalDevice physicalDevice = renderer.physicalDevice;
    auto *resourceStack = &renderer.resourceStack;

    auto createBuffer = [=](VkBuffer *buffer, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                            const void *data, VkDeviceSize size) {
        VkBufferCreateInfo createInfo = {
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .flags = 0,
                .size = size,
                .usage = usage,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };
        if (vkCreateBuffer(device, &createInfo, nullptr, buffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to create buffer");
        }
        resourceStack->emplace([=]() {
            vkDestroyBuffer(device, *buffer, nullptr);
        });

        VkMemoryRequirements memoryRequirements;
        vkGetBufferMemoryRequirements(device, *buffer, &memoryRequirements);

        auto memoryTypeIndex = findMemoryType(physicalDevice, memoryRequirements.memoryTypeBits, properties);
        VkMemoryAllocateInfo allocateInfo = {
                .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                .allocationSize = memoryRequirements.size,
                .memoryTypeIndex = memoryTypeIndex
        };
        VkDeviceMemory memory;
        if (vkAllocateMemory(device, &allocateInfo, nullptr, &memory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate memory");
        }
        resourceStack->emplace([=]() {
            vkFreeMemory(device, memory, nullptr);
        });

        // Copy data
        void *valuesDst;
        vkMapMemory(device, memory, 0, VK_WHOLE_SIZE, 0, &valuesDst);
        memcpy(valuesDst, data, size);
        vkUnmapMemory(device, memory);
        vkBindBufferMemory(device, *buffer, memory, 0);
    };

    // Create descriptor set layout
    VkDescriptorSetLayout descriptorSetLayout;
    {
        VkDescriptorSetLayoutBinding samplerLayoutBinding{};
        samplerLayoutBinding.binding = 1;
        samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        samplerLayoutBinding.descriptorCount = 1;
        samplerLayoutBinding.pImmutableSamplers = nullptr;
        samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &samplerLayoutBinding;

        if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor set layout!");
        }
        renderer.resourceStack.emplace([=]() {
            vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
        });
    }

    // Pipeline layout
    {
        VkPushConstantRange range{
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                .offset = 0,
                .size = sizeof(PushConstants)
        };

        VkPipelineLayoutCreateInfo createInfo{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                .setLayoutCount = 1,
                .pSetLayouts = &descriptorSetLayout,
                .pushConstantRangeCount = 1,
                .pPushConstantRanges = &range,
        };
        if (vkCreatePipelineLayout(device, &createInfo, nullptr, &graphicsPipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create pipeline layout!");
        }
        renderer.resourceStack.emplace([=]() {
            vkDestroyPipelineLayout(device, graphicsPipelineLayout, nullptr);
        });
    }

    // Create pipeline
    {
        VkShaderModule vertexShaderModule;
        {
            std::ifstream file("../shaders/vert.spv", std::ios::ate | std::ios::binary);
            if (!file.is_open()) {
                throw std::runtime_error("failed to open file!");
            }
            auto fileSize = file.tellg();
            std::vector<char> shaderCode(fileSize);
            file.seekg(0);
            file.read(shaderCode.data(), fileSize);
            file.close();

            VkShaderModuleCreateInfo shaderModuleCreateInfo = {
                    .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                    .codeSize = static_cast<size_t>(fileSize),
                    .pCode = reinterpret_cast<const uint32_t *>(shaderCode.data())
            };
            vkCreateShaderModule(device, &shaderModuleCreateInfo, nullptr, &vertexShaderModule);
        }

        VkShaderModule fragmentShaderModule;
        {
            std::ifstream file("../shaders/frag.spv", std::ios::ate | std::ios::binary);
            if (!file.is_open()) {
                throw std::runtime_error("failed to open file!");
            }
            auto fileSize = file.tellg();
            std::vector<char> shaderCode(fileSize);
            file.seekg(0);
            file.read(shaderCode.data(), fileSize);
            file.close();

            VkShaderModuleCreateInfo shaderModuleCreateInfo = {
                    .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                    .codeSize = static_cast<size_t>(fileSize),
                    .pCode = reinterpret_cast<const uint32_t *>(shaderCode.data())
            };
            vkCreateShaderModule(device, &shaderModuleCreateInfo, nullptr, &fragmentShaderModule);
        }

        std::array<VkPipelineShaderStageCreateInfo, 2> stages = {
                VkPipelineShaderStageCreateInfo{
                        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                        .stage = VK_SHADER_STAGE_VERTEX_BIT,
                        .module = vertexShaderModule,
                        .pName = "main"
                },
                VkPipelineShaderStageCreateInfo{
                        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                        .module = fragmentShaderModule,
                        .pName = "main"
                },
        };

        std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions = {
                VkVertexInputAttributeDescription{
                        .location=0,
                        .binding=0,
                        .format=VK_FORMAT_R32G32_SFLOAT,
                        .offset=static_cast<uint32_t>(offsetof(Vertex, pos)),
                },
                VkVertexInputAttributeDescription{
                        .location=1,
                        .binding=0,
                        .format=VK_FORMAT_R32G32_SFLOAT,
                        .offset=static_cast<uint32_t>(offsetof(Vertex, texCoord)),
                },
        };

        std::array<VkVertexInputBindingDescription, 1> bindingDescription = {
                VkVertexInputBindingDescription{
                        .binding=0,
                        .stride=sizeof(Vertex),
                        .inputRate=VK_VERTEX_INPUT_RATE_VERTEX
                },
        };

        VkPipelineVertexInputStateCreateInfo vertexInputState{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                .vertexBindingDescriptionCount = static_cast<uint32_t>(bindingDescription.size()),
                .pVertexBindingDescriptions = bindingDescription.data(),
                .vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size()),
                .pVertexAttributeDescriptions = attributeDescriptions.data(),
        };

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{
                .sType=VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
                .topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN,
                .primitiveRestartEnable=VK_FALSE
        };

        VkPipelineViewportStateCreateInfo viewportState{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
                .viewportCount = 1,
                .scissorCount = 1
        };

        VkPipelineRasterizationStateCreateInfo rasterizer{
                .sType=VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                .depthClampEnable = VK_FALSE,
                .rasterizerDiscardEnable = VK_FALSE,
                .polygonMode = VK_POLYGON_MODE_FILL,
                .cullMode = VK_CULL_MODE_BACK_BIT,
                .frontFace = VK_FRONT_FACE_CLOCKWISE,
                .depthBiasEnable = VK_FALSE,
                .lineWidth = 1.0f,
        };

        VkPipelineMultisampleStateCreateInfo multisampling{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
                .sampleShadingEnable = VK_FALSE,
        };

        VkPipelineColorBlendStateCreateInfo colorBlending{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                .logicOpEnable = VK_FALSE,
                .attachmentCount = 0,
        };

        std::vector<VkDynamicState> dynamicStates = {
                VK_DYNAMIC_STATE_VIEWPORT,
                VK_DYNAMIC_STATE_SCISSOR
        };
        VkPipelineDynamicStateCreateInfo dynamicState{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                .dynamicStateCount=static_cast<uint32_t>(dynamicStates.size()),
                .pDynamicStates=dynamicStates.data()
        };

        VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {
                .sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                .stageCount=static_cast<uint32_t>(stages.size()),
                .pStages=stages.data(),
                .pVertexInputState=&vertexInputState,
                .pInputAssemblyState=&inputAssembly,
                .pViewportState=&viewportState,
                .pRasterizationState=&rasterizer,
                .pMultisampleState=&multisampling,
                .pColorBlendState=&colorBlending,
                .pDynamicState=&dynamicState,
                .layout=graphicsPipelineLayout,
                .renderPass=VK_NULL_HANDLE,
                .basePipelineHandle = VK_NULL_HANDLE
        };
        vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, nullptr,
                                  &graphicsPipeline);
        renderer.resourceStack.emplace([=]() {
            vkDestroyPipeline(device, graphicsPipeline, nullptr);
        });

        vkDestroyShaderModule(device, vertexShaderModule, nullptr);
        vkDestroyShaderModule(device, fragmentShaderModule, nullptr);
    }

    // Create vertex buffer
    createBuffer(&vertexBuffer, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, vertices.data(),
                 sizeof(Vertex) * vertices.size());

    // Create descriptor pool
    VkDescriptorPool descriptorPool;
    {
        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSize.descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor pool!");
        }
        renderer.resourceStack.emplace([=]() {
            vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        });
    }

    // Create descriptor sets
    {
        std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
        allocInfo.pSetLayouts = layouts.data();
        if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSets) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate descriptor sets!");
        }
        renderer.resourceStack.emplace([=]() {
            vkFreeDescriptorSets(device, descriptorPool, MAX_FRAMES_IN_FLIGHT, descriptorSets);
        });
    }

    auto beginSingleTimeCommands = [=]() {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = this->renderer->commandPool;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        return commandBuffer;
    };

    auto endSingleTimeCommands = [=](VkCommandBuffer commandBuffer) {
        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        vkQueueSubmit(this->renderer->graphicsQueue.queue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(this->renderer->graphicsQueue.queue);

        vkFreeCommandBuffers(device, this->renderer->commandPool, 1, &commandBuffer);
    };

    auto copyBuffer = [=](VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();

        VkBufferCopy copyRegion{};
        copyRegion.size = size;
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

        endSingleTimeCommands(commandBuffer);
    };

    auto transitionImageLayout = [=](VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        VkPipelineStageFlags sourceStage;
        VkPipelineStageFlags destinationStage;

        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        } else {
            throw std::invalid_argument("unsupported layout transition!");
        }

        vkCmdPipelineBarrier(
                commandBuffer,
                sourceStage, destinationStage,
                0,
                0, nullptr,
                0, nullptr,
                1, &barrier
        );
        endSingleTimeCommands(commandBuffer);
    };

    auto copyBufferToImage = [=](VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();
        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;

        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;

        region.imageOffset = {0, 0, 0};
        region.imageExtent = {
                width,
                height,
                1
        };
        vkCmdCopyBufferToImage(
                commandBuffer,
                buffer,
                image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1,
                &region
        );
        endSingleTimeCommands(commandBuffer);
    };

    // Texture
    {
        int texWidth, texHeight, texChannels;
        stbi_uc *pixels = stbi_load("../texture.jpg", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
        VkDeviceSize imageSize = texWidth * texHeight * 4;
        if (!pixels) {
            throw std::runtime_error("failed to load texture image!");
        }

        VkBuffer stagingBuffer;
        createBuffer(&stagingBuffer, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, pixels, imageSize);

        stbi_image_free(pixels);

        VkImageCreateInfo imageInfo{
                .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                .flags = 0,
                .imageType = VK_IMAGE_TYPE_2D,
                .format = VK_FORMAT_R8G8B8A8_SRGB,
                .extent = {
                        .width = static_cast<uint32_t>(texWidth),
                        .height = static_cast<uint32_t>(texHeight),
                        .depth = 1,
                },
                .mipLevels = 1,
                .arrayLayers = 1,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .tiling = VK_IMAGE_TILING_OPTIMAL,
                .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        };
        if (vkCreateImage(device, &imageInfo, nullptr, &textureImage) != VK_SUCCESS) {
            throw std::runtime_error("failed to create image!");
        }
        renderer.resourceStack.emplace([=]() {
            transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            vkDestroyImage(device, textureImage, nullptr);
        });

        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(device, textureImage, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if (vkAllocateMemory(device, &allocInfo, nullptr, &textureImageMemory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate image memory!");
        }

        vkBindImageMemory(device, textureImage, textureImageMemory, 0);

        transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        copyBufferToImage(stagingBuffer, textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
        transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = textureImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        if (vkCreateImageView(device, &viewInfo, nullptr, &textureImageView) != VK_SUCCESS) {
            throw std::runtime_error("failed to create texture image view!");
        }
        renderer.resourceStack.emplace([=]() {
            vkDestroyImageView(device, textureImageView, nullptr);
        });

        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f;
        if (vkCreateSampler(device, &samplerInfo, nullptr, &textureSampler) != VK_SUCCESS) {
            throw std::runtime_error("failed to create texture sampler!");
        }
        renderer.resourceStack.emplace([=]() {
            vkDestroySampler(device, textureSampler, nullptr);
        });

        for (auto & descriptorSet : descriptorSets) {
            VkDescriptorImageInfo dimageInfo{};
            dimageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            dimageInfo.imageView = textureImageView;
            dimageInfo.sampler = textureSampler;

            VkWriteDescriptorSet descriptorWrite{};
            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = descriptorSet;
            descriptorWrite.dstBinding = 1;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pImageInfo = &dimageInfo;
            vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
        }
    }
}

void
VulkanTile::render(VkCommandBuffer commandBuffer, float cx, float cy, float tileSide, const glm::mat4 &viewMatrix) {
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipelineLayout, 0, 1, &descriptorSets[renderer->currentFrame], 0, nullptr);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
    VkBuffer vertexBuffers[] = {vertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

    PushConstants pushConstants{
            glm::vec4(tileSide, 0, 0, 0),
            glm::vec4(cx, cy, 0, 0),
            viewMatrix
    };
    vkCmdPushConstants(commandBuffer, graphicsPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants),
                       &pushConstants);
    vkCmdDraw(commandBuffer, static_cast<uint32_t>(vertices.size()), 1, 0, 0);
}