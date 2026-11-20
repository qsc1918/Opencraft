#version 330 core
#define kMaxTiles 128

layout(location = 0) in vec2 vUV;
layout(location = 1) in float vShade;
layout(location = 2) in float vDist;
layout(location = 3) flat in vec4 vTileRect;
layout(location = 4) flat in float vWrap;

uniform sampler2D atlas;
uniform vec4 camPos;
uniform vec4 fogParams;
uniform vec4 misc;
uniform vec4 day;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 c;
    if (vWrap < 0.5) {
        c = texture(atlas, vUV);
    } else {
        vec2 atlasSize = vec2(misc.y, misc.z);
        vec2 rel = vUV * atlasSize - vTileRect.xy;
        if (mod(vWrap, 2.0) >= 1.0) rel.x = mod(rel.x, vTileRect.z);
        if (vWrap >= 2.0)           rel.y = mod(rel.y, vTileRect.w);
        vec2 uv = (vTileRect.xy + rel) / atlasSize;
        c = textureGrad(atlas, uv, dFdx(vUV), dFdy(vUV));
    }
    vec3 tint = pow(vec3(0.40, 0.62, 1.10), vec3(2.2));
    vec3 col = c.rgb * vShade * day.x * tint;
    float fog = clamp((vDist - fogParams.x) / (fogParams.y - fogParams.x), 0.0, 1.0);
    vec3 sky = mix(vec3(0.10, 0.13, 0.24), vec3(fogParams.z, fogParams.w, misc.x), day.x);
    col = mix(col, sky, fog);
    if (camPos.w > 0.5) {
        vec3 waterCol = vec3(0.06, 0.22, 0.40);
        float depth = clamp(vDist / fogParams.y, 0.0, 1.0);
        col = mix(col, waterCol, clamp(0.30 + depth * 0.70, 0.0, 1.0));
    }
    outColor = vec4(col, 0.82);
}
