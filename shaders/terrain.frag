#version 450

layout(location = 0) in vec2 vUV;
layout(location = 1) in float vShade;
layout(location = 2) in float vDist;

layout(set = 0, binding = 1) uniform sampler2D atlas;
layout(set = 0, binding = 2) uniform sampler2D atlasSharp;
layout(set = 0, binding = 0) uniform Ubo {
    mat4 viewProj;
    vec4 camPos;
    vec4 fogParams;
    vec4 misc;
    vec4 day;
    vec4 texSharp; // x 近景清晰距离(格), y 过渡结束距离
} ubo;

layout(location = 0) out vec4 outColor;

void main() {
    // 图集单元内已有 8px 边缘扩展，配合线性 mip + 各向异性过滤，
    // 大片平面斜看时不会拉出条纹。
    // 近景清晰：texSharp.x 之内锁 mip0 最近邻（部分核显驱动开各向异性后
    // 近处也选高 mip，方块发糊），之后 6 格平滑过渡回常规 mip 采样。
    float t = clamp((vDist - ubo.texSharp.x) / max(ubo.texSharp.y - ubo.texSharp.x, 0.001), 0.0, 1.0);
    vec4 c;
    if (t <= 0.0)      c = texture(atlasSharp, vUV);
    else if (t >= 1.0) c = texture(atlas, vUV);
    else               c = mix(texture(atlasSharp, vUV), texture(atlas, vUV), t);
    if (c.a < 0.3) discard;
    vec3 col = c.rgb * vShade * ubo.day.x;
    float fog = clamp((vDist - ubo.fogParams.x) / (ubo.fogParams.y - ubo.fogParams.x), 0.0, 1.0);
    vec3 sky = mix(vec3(0.10, 0.13, 0.24), vec3(ubo.fogParams.z, ubo.fogParams.w, ubo.misc.x), ubo.day.x);
    col = mix(col, sky, fog);
    if (ubo.camPos.w > 0.5) {
        vec3 waterCol = vec3(0.06, 0.22, 0.40);
        float depth = clamp(vDist / ubo.fogParams.y, 0.0, 1.0);
        col = mix(col, waterCol, clamp(0.30 + depth * 0.70, 0.0, 1.0));
    }
    outColor = vec4(col, 1.0);
}
