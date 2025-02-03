#version 450

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 model;
} ubo;

layout(set = 1, binding = 0) uniform SubpassUniformBufferObject {
    mat4 view;
    mat4 proj;
} subo;

void main() {
    gl_Position = subo.proj * subo.view * ubo.model * vec4(inPosition, 1.0);
    fragColor = vec3(1, 1, 1);
    fragTexCoord = inTexCoord;
}