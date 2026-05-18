#version 450

layout(input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput color;
layout(input_attachment_index = 1, set = 0, binding = 1) uniform subpassInput depth;

layout(location = 0) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

void main() {
    float depth = clamp(0.08 / (1.0 - subpassLoad(depth).x) - 0.8, 0, 1);
    vec3 subpass = subpassLoad(color).rgb;
    float mixer = (fragTexCoord.x - 0.5) * (fragTexCoord.y - 0.5);
    outColor = vec4(mixer < 0 ? subpass : ((1 - depth) * (subpass) + depth * vec3(0.5176, 0.7451, 0.9725)), 1.0);
}