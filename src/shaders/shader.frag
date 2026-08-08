#version 450

layout (location = 0) out vec4 outColor;

layout (binding = 0) uniform UniformBufferObject {
    vec3 color;
} ubo;

layout (location = 0) in vec3 normal;
layout (location = 1) in vec3 L;

void main() {
    // outColor = vec4(ubo.color, 1.0f);
    outColor = vec4(normal.x, -normal.y, -normal.z, 1.0f);
    // outColor = vec4(L, 1.0f);
}
