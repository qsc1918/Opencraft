#version 450

// 实体顶点着色器：与 terrain 同一套 UBO/描述符，区别只有两点
//  1. uv 是 0..255 映射整张贴图（生物模型要按原版 texOffs 逐面取子矩形，
//     1 像素必须能精确取到，所以不能再按 0..16 量化）
//  2. 世界偏移走 push constant（pc.origin = 实体位置），这样远处实体也不会
//     因为顶点 int16 存不下世界坐标而错位
#define kMaxTiles 128

layout(location = 0) in ivec4 inPos;   // xyz = 相对实体的 1/16 格整数
layout(location = 1) in uvec4 inMeta;  // 依次为 u、v、贴图索引、明暗

layout(location = 0) out vec2 vUV;
layout(location = 1) out float vShade;
layout(location = 2) out float vDist;
layout(location = 3) flat out vec4 vTileRect;
layout(location = 4) flat out float vWrap;

layout(push_constant) uniform Pc {
    vec4 origin;
} pc;

layout(set = 0, binding = 0) uniform Ubo {
    mat4 viewProj;
    vec4 camPos;
    vec4 fogParams;
    vec4 misc;
    vec4 day;
    vec4 texSharp;
    vec4 tileRect[kMaxTiles];
} ubo;

void main() {
    vec3 wp = vec3(inPos.xyz) / 16.0 + pc.origin.xyz;
    gl_Position = ubo.viewProj * vec4(wp, 1.0);

    int tex = int(inMeta.z);
    vec4 r = ubo.tileRect[tex];
    // u/v 是 0..255 的"贴图比例"，除以 256 得到 0..1
    vec2 tileUV = vec2(float(inMeta.x), float(inMeta.y)) / 256.0;
    vec2 px = r.xy + tileUV * r.zw;
    vUV = px / vec2(ubo.misc.y, ubo.misc.z);
    vTileRect = r;
    vWrap = 0.0f;   // 实体不做贪心平铺
    vShade = pow(float(inMeta.w) / 255.0, 2.2);
    vDist = length(wp - ubo.camPos.xyz);
}
