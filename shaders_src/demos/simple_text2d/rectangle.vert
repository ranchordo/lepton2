#version 450

layout(location = 0) out vec2 fragTexCoord;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;

layout(binding = 0) uniform UniformBufferObject {
    vec2 xyPos;
    vec2 extent;
    float zPos;
} dubo;

void main() {
    gl_Position = vec4(inPosition.xy * dubo.extent + dubo.xyPos, inPosition.z, 1.0);
    fragTexCoord = inTexCoord;
}