#version 450

layout(location = 0) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

void main() {
    float p = fragTexCoord.x + fragTexCoord.y + (fragTexCoord.x - fragTexCoord.y) * (fragTexCoord.x - fragTexCoord.y);
    if (p < 0.75 || p > 1.75) discard;
    outColor = vec4(fragTexCoord, 1, 1);
}