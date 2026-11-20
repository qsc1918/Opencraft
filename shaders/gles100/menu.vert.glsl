#version 100

attribute vec2 inPos;
attribute vec2 inUV;
attribute vec4 inTint;
varying vec2 vUV;
varying vec4 vTint;

void main() {
    gl_Position = vec4(inPos, 0.0, 1.0);
    gl_Position.y = -gl_Position.y;
    vUV = inUV;
    vTint = inTint;
}
