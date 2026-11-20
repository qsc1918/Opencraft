#version 120
#define kMaxTiles 128

attribute vec4 inPos;
attribute vec4 inMeta;

varying vec2 vUV;
varying float vShade;
varying float vDist;
varying vec4 vTileRect;
varying float vWrap;

uniform vec4 pcOrigin;
uniform mat4 viewProj;
uniform vec4 camPos;
uniform vec4 misc;
uniform vec4 tileRect[kMaxTiles];

void main() {
    vec3 p = inPos.xyz / 16.0;
    vec3 wp = p + pcOrigin.xyz;
    gl_Position = viewProj * vec4(wp, 1.0);
    gl_Position.y = -gl_Position.y;

    int tex = int(inMeta.z + 0.5);
    vec2 tileUV = inMeta.xy;
    vec4 r = tileRect[tex];
    vec2 px = r.xy + tileUV * r.zw / 16.0;
    vUV = px / vec2(misc.y, misc.z);
    vTileRect = r;
    vWrap = inPos.w;
    vShade = pow(inMeta.w / 255.0, 2.2);
    vDist = length(wp - camPos.xyz);
}
