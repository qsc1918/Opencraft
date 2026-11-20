#version 330 core

layout(location = 0) in vec2 vUV;
uniform vec4 horizon;
uniform vec4 zenith;
layout(location = 0) out vec4 outColor;

void main() {
    vec3 c = mix(zenith.rgb, horizon.rgb, clamp(vUV.y, 0.0, 1.0));
    if (horizon.a > 0.5) {
        vec3 waterCol = vec3(0.05, 0.18, 0.34);
        c = mix(c, waterCol, 0.85);
    }
    outColor = vec4(c, 1.0);
}
