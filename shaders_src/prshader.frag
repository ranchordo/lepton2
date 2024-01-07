#version 450

layout(input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput color;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 subpass = subpassLoad(color).rgb;
    float mixer = (fragTexCoord.x - 0.5) * (fragTexCoord.y - 0.5);
    if (mixer < 0) {
        outColor = vec4(1 - subpass.x, 1 - subpass.y, 1 - subpass.z, 1.0);
    } else {
        outColor = vec4(subpass, 1.0);
    }
}