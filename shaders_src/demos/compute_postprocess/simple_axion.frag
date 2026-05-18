#version 450

layout(location = 0) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D texSampler;

void main() {
    outColor = vec4(vec3(fragTexCoord, 0).rbg * (vec3(1) - texture(texSampler, fragTexCoord).rgb), 1.0);
}