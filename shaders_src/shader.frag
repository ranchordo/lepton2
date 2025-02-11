#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec2 phasor;
layout(location = 4) in float phase_thresh;

layout(location = 0) out vec4 outColor;

layout(binding = 1) uniform sampler2D texSampler;

vec3 hsv2rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

void main() {
    vec3 light = normalize(vec3(1, 1, 0));
    vec3 light2 = normalize(vec3(-1.5, -1, 0));
    float intensity = 0.2 + 2.0 * max(0, dot(normal, light)) + 1.0 * max(0, dot(normal, light2));
    float phase = atan(phasor.y, phasor.x);
    vec3 phasecol = hsv2rgb(vec3((phase / 6.283) + 0.5, 1, 1));
    vec3 col = intensity * phasecol * texture(texSampler, vec2(fragTexCoord.x, fragTexCoord.y)).rgb;
    float dphase = mod(phase - phase_thresh + 3.142, 6.283) - 3.142;
    outColor = vec4(col, abs(dphase) < 1.571 ? 1.0 : 0.0);
}