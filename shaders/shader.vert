#version 450

layout(location = 0) in vec2 vkCoordinate;
layout(location = 1) in vec2 inTexCoord;

layout(location = 0) out vec2 fragTexCoord;

layout(push_constant, std430) uniform pc {
    vec4 tileSide;
    vec4 center;
    mat4 viewMatrix;
};

void main() {
    gl_Position = viewMatrix * vec4(center.xy + vkCoordinate * tileSide.x, 0.0, 1.0);
    fragTexCoord = inTexCoord;
}