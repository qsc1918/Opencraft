#version 100
precision highp float;
precision highp int;

varying vec2 vUV;
varying vec4 vTint;
uniform sampler2D menu;

void main() {
    gl_FragColor = texture2D(menu, vUV) * vTint;
}
