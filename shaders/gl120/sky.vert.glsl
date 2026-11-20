#version 120

attribute vec2 inPos;
varying vec2 vUV;

void main() {
    vUV = inPos;
    gl_Position = vec4(inPos * 2.0 - 1.0, 0.0, 1.0);
    gl_Position.y = -gl_Position.y;
}
