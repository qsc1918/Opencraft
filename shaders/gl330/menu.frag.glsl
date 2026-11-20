#version 330 core

layout(location = 0) in vec2 vUV;
layout(location = 1) in vec4 vTint;

uniform sampler2D menu;
layout(location = 0) out vec4 outColor;

void main() {
    outColor = texture(menu, vUV) * vTint;
}
