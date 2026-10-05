#version 450

layout(location = 0) in vec2 vUV;
layout(location = 1) in float vShade;
layout(location = 2) in float vDist;

layout(set = 0, binding = 1) uniform sampler2D atlas;
layout(set = 0, binding = 0) uniform Ubo {
    mat4 viewProj;
    vec4 camPos;
    vec4 fogParams;
    vec4 misc;
    vec4 day;
} ubo;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 c = texture(atlas, vUV);
    // 水的色调系数是"显示空间乘法"，线性空间里要取 2.2 次幂（同 vShade）
    vec3 tint = pow(vec3(0.40, 0.62, 1.10), vec3(2.2));
    vec3 col = c.rgb * vShade * ubo.day.x * tint;
    float fog = clamp((vDist - ubo.fogParams.x) / (ubo.fogParams.y - ubo.fogParams.x), 0.0, 1.0);
    vec3 sky = mix(vec3(0.10, 0.13, 0.24), vec3(ubo.fogParams.z, ubo.fogParams.w, ubo.misc.x), ubo.day.x);
    col = mix(col, sky, fog);
    if (ubo.camPos.w > 0.5) {
        vec3 waterCol = vec3(0.06, 0.22, 0.40);
        float depth = clamp(vDist / ubo.fogParams.y, 0.0, 1.0);
        col = mix(col, waterCol, clamp(0.30 + depth * 0.70, 0.0, 1.0));
    }
    outColor = vec4(col, 0.82);
}
