#version 330 core

layout(location = 0) in vec2 vUV;
layout(location = 1) in float vShade;
layout(location = 2) in float vDist;
layout(location = 3) flat in vec4 vTileRect;
layout(location = 4) flat in float vWrap;

uniform sampler2D atlas;
uniform sampler2D atlasSharp;
uniform vec4 camPos;
uniform vec4 fogParams;
uniform vec4 misc;
uniform vec4 day;
uniform vec4 texSharp;

layout(location = 0) out vec4 outColor;

void main() {
    float t = clamp((vDist - texSharp.x) / max(texSharp.y - texSharp.x, 0.001), 0.0, 1.0);
    vec4 c;
    if (vWrap < 0.5) {
        if (t <= 0.0)      c = texture(atlasSharp, vUV);
        else if (t >= 1.0) c = texture(atlas, vUV);
        else               c = mix(texture(atlasSharp, vUV), texture(atlas, vUV), t);
    } else {
        vec2 atlasSize = vec2(misc.y, misc.z);
        vec2 rel = vUV * atlasSize - vTileRect.xy;
        if (mod(vWrap, 2.0) >= 1.0) rel.x = mod(rel.x, vTileRect.z);
        if (vWrap >= 2.0)           rel.y = mod(rel.y, vTileRect.w);
        vec2 uv = (vTileRect.xy + rel) / atlasSize;
        vec2 dx = dFdx(vUV), dy = dFdy(vUV);
        c = mix(textureGrad(atlasSharp, uv, dx, dy), textureGrad(atlas, uv, dx, dy), t);
    }
    if (c.a < 0.3) discard;
    vec3 col = c.rgb * vShade * day.x;
    float fog = clamp((vDist - fogParams.x) / (fogParams.y - fogParams.x), 0.0, 1.0);
    vec3 sky = mix(vec3(0.10, 0.13, 0.24), vec3(fogParams.z, fogParams.w, misc.x), day.x);
    col = mix(col, sky, fog);
    if (camPos.w > 0.5) {
        vec3 waterCol = vec3(0.06, 0.22, 0.40);
        float depth = clamp(vDist / fogParams.y, 0.0, 1.0);
        col = mix(col, waterCol, clamp(0.30 + depth * 0.70, 0.0, 1.0));
    }
    outColor = vec4(col, 1.0);
}
