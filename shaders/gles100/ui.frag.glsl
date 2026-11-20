#version 100
precision highp float;
precision highp int;

varying vec2 vUV;
varying vec4 vTint;
uniform sampler2D atlas;

void main() {
    // 顶点色与图集都是显示空间字节，直出即可，不需要再做 sRGB 转换。
    gl_FragColor = texture2D(atlas, vUV) * vTint;
}
