#version 450

// 实体片元：与地形同一套雾/昼夜/水下色调，只是没有"平铺折回"那条分支
// （实体顶点自己就带精确 uv，见 entity.vert 的注释）。
layout(location = 0) in vec2 vUV;
layout(location = 1) in float vShade;
layout(location = 2) in float vDist;
layout(location = 3) flat in vec4 vTileRect;
layout(location = 4) flat in float vWrap;

layout(set = 0, binding = 1) uniform sampler2D atlas;
layout(set = 0, binding = 2) uniform sampler2D atlasSharp;
layout(set = 0, binding = 0) uniform Ubo {
    mat4 viewProj;
    vec4 camPos;
    vec4 fogParams;
    vec4 misc;
    vec4 day;
    vec4 texSharp;
} ubo;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 c;
    if (vDist <= ubo.texSharp.x)      c = texture(atlasSharp, vUV);
    else if (vDist >= ubo.texSharp.y) c = texture(atlas, vUV);
    else c = mix(texture(atlasSharp, vUV), texture(atlas, vUV),
                 (vDist - ubo.texSharp.x) / max(ubo.texSharp.y - ubo.texSharp.x, 0.001));
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
