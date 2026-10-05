#version 450

#define kMaxTiles 128

layout(location = 0) in vec2 vUV;
layout(location = 1) in float vShade;
layout(location = 2) in float vDist;
layout(location = 3) flat in vec4 vTileRect;
layout(location = 4) flat in float vWrap;

layout(set = 0, binding = 1) uniform sampler2D atlas;
layout(set = 0, binding = 0) uniform Ubo {
    mat4 viewProj;
    vec4 camPos;
    vec4 fogParams;
    vec4 misc;
    vec4 day;
    vec4 texSharp;  // 与 terrain.vert 的 UBO 对齐，缺了它 tileRect 会整体错位
    vec4 tileRect[kMaxTiles];
} ubo;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 c;
    if (vWrap < 0.5) {
        c = texture(atlas, vUV);
    } else {
        // 合并出的大面：按图块折回重复铺；导数取折回前的 uv，避免接缝处跳 mip
        vec2 atlasSize = vec2(ubo.misc.y, ubo.misc.z);
        vec2 rel = vUV * atlasSize - vTileRect.xy;
        if (mod(vWrap, 2.0) >= 1.0) rel.x = mod(rel.x, vTileRect.z);
        if (vWrap >= 2.0)           rel.y = mod(rel.y, vTileRect.w);
        vec2 uv = (vTileRect.xy + rel) / atlasSize;
        c = textureGrad(atlas, uv, dFdx(vUV), dFdy(vUV));
    }
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
