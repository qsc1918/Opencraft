#version 100
precision highp float;
precision highp int;

varying vec2 vUV;
uniform vec4 horizon;
uniform vec4 zenith;

void main() {
    vec3 c = mix(zenith.rgb, horizon.rgb, clamp(vUV.y, 0.0, 1.0));
    if (horizon.a > 0.5) {
        vec3 waterCol = vec3(0.05, 0.18, 0.34);
        c = mix(c, waterCol, 0.85);
    }
    gl_FragColor = vec4(pow(c, vec3(1.0 / 2.2)), 1.0);
}
