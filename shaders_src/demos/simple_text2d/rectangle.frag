#version 450

layout(location = 0) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

void main() {
    if (length(fragTexCoord - vec2(0.5, 0.5)) > 0.5) discard;
    outColor = vec4(1, 1, 1, 1);
}