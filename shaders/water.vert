#version 450

// 图块数量上限，必须与 atlas.hpp 的 kAtlasMaxTiles、UBO 里的 tileRect 一致
#define kMaxTiles 128

layout(location = 0) in ivec4 inPos;
layout(location = 1) in uvec4 inMeta; // 依次为 u、v、贴图索引、明暗

layout(location = 0) out vec2 vUV;
layout(location = 1) out float vShade;
layout(location = 2) out float vDist;

layout(push_constant) uniform Pc {
    vec4 origin;
} pc;

// 与 terrain.vert 共用同一份 UBO（std140），字段顺序必须完全一致
layout(set = 0, binding = 0) uniform Ubo {
    mat4 viewProj;
    vec4 camPos;
    vec4 fogParams;
    vec4 misc;      // x 天蓝, y 图集宽, z 图集高, w 备用
    vec4 day;
    vec4 texSharp;
    vec4 tileRect[kMaxTiles];
} ubo;

void main() {
    // inPos.xyz 是 1/16 格单位的整数（与 terrain.vert 同一顶点格式）
    vec3 p = vec3(inPos.xyz) / 16.0;
    vec3 wp = p + pc.origin.xyz;
    gl_Position = ubo.viewProj * vec4(wp, 1.0);

    int tex = int(inMeta.z);
    vec2 tileUV = vec2(float(inMeta.x), float(inMeta.y));
    // 与 terrain.vert 同款：按图块自身像素矩形取 UV（支持任意分辨率贴图）
    vec4 r = ubo.tileRect[tex];
    vec2 px = r.xy + tileUV * r.zw / 16.0;
    vUV = px / vec2(ubo.misc.y, ubo.misc.z);
    // 明暗系数按下标语义是"显示空间乘法"，线性空间里等价于 系数^2.2
    vShade = pow(float(inMeta.w) / 255.0, 2.2);
    vDist = length(wp - ubo.camPos.xyz);
}
