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
    glm::vec3 color;
};

const static std::vector<Vertex> vertices = {
        {{-0.5f, -0.5f}, {0,               163.0f / 255.0f, 104.0f / 255.0f}},
        {{0.5f,  -0.5f}, {255.0f / 255.0f, 211.0f / 255.0f, 0}},
        {{0.5f,  0.5f},  {0,               136.0f / 255.0f, 191.0f / 255.0f}},
        {{-0.5f, 0.5f},  {196.0f / 255.0f, 2.0f / 255.0f,   51.0f / 255.0f}},
};

VulkanTile::VulkanTile(VulkanRenderer &renderer) {
    VkDevice device = renderer.device;
    
    // Pipeline layout
    {
        VkPushConstantRange range{
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                .offset = 0,
                .size = sizeof(PushConstants)
        };

        VkPipelineLayoutCreateInfo createInfo{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
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

        std::vector<VkPipelineShaderStageCreateInfo> stages = {
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

        std::vector<VkVertexInputAttributeDescription> attributeDescriptions = {
                VkVertexInputAttributeDescription{
                        .location=0,
                        .binding=0,
                        .format=VK_FORMAT_R32G32_SFLOAT,
                        .offset=static_cast<uint32_t>(offsetof(Vertex, pos)),
                },
                VkVertexInputAttributeDescription{
                        .location=1,
                        .binding=0,
                        .format=VK_FORMAT_R32G32B32_SFLOAT,
                        .offset=static_cast<uint32_t>(offsetof(Vertex, color)),
                },
        };

        std::vector<VkVertexInputBindingDescription> bindingDescription = {
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
    {
        VkBufferCreateInfo createInfo = {
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .flags = 0,
                .size = sizeof(Vertex) * vertices.size(),
                .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };
        if (vkCreateBuffer(device, &createInfo, nullptr, &vertexBuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to create buffer");
        }
        renderer.resourceStack.emplace([=]() {
            vkDestroyBuffer(device, vertexBuffer, nullptr);
        });
    }

    // Allocate device memory
    {
        VkMemoryRequirements memoryRequirements;
        vkGetBufferMemoryRequirements(device, vertexBuffer, &memoryRequirements);

        VkPhysicalDeviceMemoryProperties deviceMemoryProperties;
        vkGetPhysicalDeviceMemoryProperties(renderer.physicalDevice, &deviceMemoryProperties);

        uint32_t memoryTypeIndex = -1;
        for (int i = 0; i < deviceMemoryProperties.memoryTypeCount; ++i) {
            const uint32_t flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
                                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
            if (memoryRequirements.memoryTypeBits & (1 << i)) {
                if (deviceMemoryProperties.memoryTypes[i].propertyFlags & flags) {
                    memoryTypeIndex = i;
                }
            }
        }
        if (memoryTypeIndex == -1) throw std::runtime_error("Memory not found");

        VkMemoryAllocateInfo allocateInfo = {
                .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                .allocationSize = memoryRequirements.size,
                .memoryTypeIndex = memoryTypeIndex
        };
        if (vkAllocateMemory(device, &allocateInfo, nullptr, &vertexMemory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate memory");
        }
        renderer.resourceStack.emplace([=]() {
            vkFreeMemory(device, vertexMemory, nullptr);
        });
    }

    // Copy data to vertex buffer
    {
        void *valuesDst;
        vkMapMemory(device, vertexMemory, 0, VK_WHOLE_SIZE, 0, &valuesDst);
        memcpy(valuesDst, vertices.data(), sizeof(Vertex) * vertices.size());
        vkUnmapMemory(device, vertexMemory);
        vkBindBufferMemory(device, vertexBuffer, vertexMemory, 0);
    }
}

void VulkanTile::render(VkCommandBuffer commandBuffer, float cx, float cy, float tileSide, const glm::mat4& viewMatrix) {
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