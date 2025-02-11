#version 450

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 normal;
layout(location = 3) out vec2 phasor;
layout(location = 4) out float phase_thresh;

layout(location = 0) in vec4 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec4 inNormal;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    vec4 model_rot;
    vec3 model_trans;
} ubo;

layout(set = 1, binding = 0) uniform SubpassUniformBufferObject {
    mat4 proj;
    mat4 view_post;
    vec4 view_rot;
    vec3 view_trans;
    float phase_thresh;
} subo;

vec4 apply_quat(vec4 q, vec4 spinor) {
    vec4 result = q.w * spinor;
    result += q.x * vec4(spinor.w, -spinor.z, spinor.y, -spinor.x);
    result += q.y * vec4(-spinor.z, -spinor.w, spinor.x, spinor.y);
    result += q.z * vec4(spinor.y, -spinor.x, -spinor.w, spinor.z);
    return result;
}

vec2 zdiv(vec2 a, vec2 b) {
    vec2 s1 = vec2(a.x * b.x + a.y * b.y, a.y * b.x - a.x * b.y);
    return s1 / (b.x * b.x + b.y * b.y);
}

vec2 zmul(vec2 a, vec2 b) {
    return vec2(a.x * b.x - a.y * b.y, a.x * b.y + a.y * b.x);
}

vec2 zsqrt(vec2 z) {
    float phase = atan(z.y, z.x);
    return sqrt(length(z)) * vec2(cos(phase / 2), sin(phase / 2));
}

vec3 to_vector(vec4 spinor) {
    vec2 r = zdiv(spinor.zw, spinor.xy);
    float phi = atan(r.y, r.x);
    float theta = 2 * atan(length(spinor.zw), length(spinor.xy));
    float sintheta = sin(theta);
    return length(spinor) * vec3(sintheta * cos(phi), sintheta * sin(phi), cos(theta));
}

float spinor_phase(vec4 spinor) {
    return atan(spinor.y, spinor.x);
}

vec4 flip_spinor(vec4 spinor) {
    return vec4(spinor.xy * length(spinor.zw) / length(spinor.xy),
                -spinor.zw * length(spinor.xy) / length(spinor.zw));
}

vec4 apply_translation_raw(vec4 spinor, vec3 c) {
    vec3 x = to_vector(spinor);
    vec3 A = normalize(cross(x, c));
    float magproduct = length(x) * length(x + c);
    float dotthing = dot(x, x + c);
    float scal = length(x + c) / length(x);

    float coshalftheta = sqrt((magproduct + dotthing) / (2 * magproduct));
    float sinhalftheta = sqrt((magproduct - dotthing) / (2 * magproduct));
    vec4 q = vec4(A * sinhalftheta, coshalftheta);
    return scal * apply_quat(q, spinor);
}

vec4 apply_translation(vec4 spinor, vec3 c) {
    vec4 tr = apply_translation_raw(spinor, c);
    vec4 tr_conj = flip_spinor(apply_translation_raw(flip_spinor(spinor), -c));
    vec2 phasor = zsqrt(zdiv(tr_conj.xy, tr.xy));
    return vec4(zmul(tr.xy, phasor), zmul(tr.zw, phasor));
}

void main() {
    vec4 world_spinor = apply_quat(ubo.model_rot, inPosition);
    world_spinor = apply_translation(world_spinor, subo.view_trans);
    vec4 view_spinor = apply_quat(subo.view_rot, world_spinor);
    gl_Position = subo.proj * subo.view_post * vec4(to_vector(view_spinor), 1.0);
    float phase = spinor_phase(view_spinor);
    phasor = vec2(cos(phase), sin(phase));
    fragColor = vec3(1, 1, 1);
    fragTexCoord = inTexCoord;
    normal = to_vector(inNormal);
    phase_thresh = subo.phase_thresh;
}