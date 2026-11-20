#version 330 core
#define kMaxTiles 128

layout(location = 0) in ivec4 inPos;
layout(location = 1) in uvec4 inMeta;

layout(location = 0) out vec2 vUV;
layout(location = 1) out float vShade;
layout(location = 2) out float vDist;
layout(location = 3) flat out vec4 vTileRect;
layout(location = 4) flat out float vWrap;

uniform vec4 pcOrigin;
uniform mat4 viewProj;
uniform vec4 camPos;
uniform vec4 fogParams;
uniform vec4 misc;
uniform vec4 day;
uniform vec4 texSharp;
uniform vec4 tileRect[kMaxTiles];

void main() {
    vec3 wp = vec3(inPos.xyz) / 16.0 + pcOrigin.xyz;
    gl_Position = viewProj * vec4(wp, 1.0);
    gl_Position.y = -gl_Position.y; // Vulkan 的 NDC +y 朝下，GL 朝上

    int tex = int(inMeta.z);
    vec4 r = tileRect[tex];
    vec2 tileUV = vec2(float(inMeta.x), float(inMeta.y)) / 256.0;
    vec2 px = r.xy + tileUV * r.zw;
    vUV = px / vec2(misc.y, misc.z);
    vTileRect = r;
    vWrap = 0.0;
    vShade = pow(float(inMeta.w) / 255.0, 2.2);
    vDist = length(wp - camPos.xyz);
}
