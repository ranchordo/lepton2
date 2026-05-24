#version 450

layout(input_attachment_index = 0, set = 0, binding = 0) uniform subpassInputMS color;

layout(location = 0) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

void main() {
    float mixer = (fragTexCoord.x - 0.5) * (fragTexCoord.y - 0.5);
    vec3 subpass = subpassLoad(color, mixer < 0 ? 0 : gl_SampleID).rgb;
    outColor = vec4(mixer < 0 ? subpass : 0.9 * subpass, 1.0);
}