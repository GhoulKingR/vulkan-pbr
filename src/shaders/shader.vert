#version 450

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;

layout (binding = 1) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;

    vec3 lightPos;
    mat3 model_normal;
} ubo;

layout (location = 0) out vec3 normal;
layout (location = 1) out vec3 L;


void main() {
    vec4 worldSpace = ubo.model * vec4(inPosition, 1.0);
    gl_Position = ubo.proj * ubo.view * worldSpace;
    // normal = normalize(ubo.model_normal * inNormal);
    normal = normalize(inNormal);
    L = ubo.lightPos - worldSpace.xyz;
}
