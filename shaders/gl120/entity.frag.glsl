#version 120

varying vec2 vUV;
varying float vShade;
varying float vDist;
varying vec4 vTileRect;
varying float vWrap;

uniform sampler2D atlas;
uniform vec4 camPos;
uniform vec4 fogParams;
uniform vec4 misc;
uniform vec4 day;

void main() {
    vec4 c = texture2D(atlas, vUV);
    if (c.a < 0.3) discard;
    vec3 col = pow(c.rgb, vec3(2.2)) * vShade * day.x;
    float fog = clamp((vDist - fogParams.x) / (fogParams.y - fogParams.x), 0.0, 1.0);
    vec3 sky = mix(vec3(0.10, 0.13, 0.24), vec3(fogParams.z, fogParams.w, misc.x), day.x);
    col = mix(col, sky, fog);
    if (camPos.w > 0.5) {
        vec3 waterCol = vec3(0.06, 0.22, 0.40);
        float depth = clamp(vDist / fogParams.y, 0.0, 1.0);
        col = mix(col, waterCol, clamp(0.30 + depth * 0.70, 0.0, 1.0));
    }
    gl_FragColor = vec4(pow(col, vec3(1.0 / 2.2)), 1.0);
}
