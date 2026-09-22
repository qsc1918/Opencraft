#version 450

layout(location = 0) in ivec4 inPos;
layout(location = 1) in uvec4 inMeta; // 依次为 u、v、贴图索引、明暗

layout(location = 0) out vec2 vUV;
layout(location = 1) out float vShade;
layout(location = 2) out float vDist;

layout(push_constant) uniform Pc {
    vec4 origin;
} pc;

layout(set = 0, binding = 0) uniform Ubo {
    mat4 viewProj;
    vec4 camPos;
    vec4 fogParams; // x 雾起, y 雾止, z 天红, w 天绿
    vec4 misc;      // x 天蓝, y 图集边长, z 横向格数, w 单元像素
    vec4 day;       // x 日照 0..1, yzw 太阳方向
} ubo;

void main() {
    // inPos.w 存 y 的 1/16 小数部分（末地传送门框架是 13/16 高）
    vec3 p = vec3(inPos.xyz) + vec3(0.0, float(inPos.w) / 16.0, 0.0);
    vec3 wp = p + pc.origin.xyz;
    gl_Position = ubo.viewProj * vec4(wp, 1.0);

    int tex = int(inMeta.z);
    vec2 tileUV = vec2(float(inMeta.x), float(inMeta.y));
    // 图集按 32 像素单元排布：在单元内偏移 8 像素，留边框防采样渗色
    vec2 tileIdx = vec2(float(tex % int(ubo.misc.z)), float(tex / int(ubo.misc.z)));
    vec2 px = tileIdx * ubo.misc.w + vec2(ubo.misc.w * 0.25) + tileUV + 0.5;
    vUV = px / ubo.misc.y;
    vShade = float(inMeta.w) / 255.0;
    vDist = length(wp - ubo.camPos.xyz);
}
