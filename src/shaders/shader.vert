#version 450

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;

layout (binding = 1) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;

    vec3 lightPos;
    vec3 viewPos;
    mat3 model_normal;
} ubo;

layout (location = 0) out vec3 N;
layout (location = 1) out vec3 L;
layout (location = 2) out vec3 V;
layout (location = 3) out float dist;


void main() {
    vec4 worldSpace = ubo.model * vec4(inPosition, 1.0);
    gl_Position = ubo.proj * ubo.view * worldSpace;
    // normal = ubo.model_normal * inNormal;

    N = inNormal;
    L = normalize(ubo.lightPos - worldSpace.xyz);
    V = normalize(ubo.viewPos  - worldSpace.xyz);
    dist = length(ubo.lightPos - worldSpace.xyz);
}
