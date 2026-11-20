#version 100

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
uniform vec4 tileRect[128];

void main() {
    vec3 wp = inPos.xyz / 16.0 + pcOrigin.xyz;
    gl_Position = viewProj * vec4(wp, 1.0);
    gl_Position.y = -gl_Position.y;
    vec4 r = tileRect[int(inMeta.z + 0.5)];
    vec2 px = r.xy + (inMeta.xy / 256.0) * r.zw;
    vUV = px / vec2(misc.y, misc.z);
    vTileRect = r;
    vWrap = 0.0;
    vShade = pow(inMeta.w / 255.0, 2.2);
    vDist = length(wp - camPos.xyz);
}
