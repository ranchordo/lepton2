#version 450

layout(location = 0) out vec2 fragTexCoord;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;

void main() {
    gl_Position = vec4(inPosition.xyz, 1.0);
    fragTexCoord = inTexCoord;
}