cbuffer Ubo : register(b0) {
    float4x4 viewProj;
    float4 camPos;
    float4 fogParams;
    float4 misc;
    float4 day;
    float4 texSharp;
    float4 tileRect[128];
};
Texture2D<float4> atlas : register(t0);
SamplerState atlasSamp : register(s0);
Texture2D<float4> atlasSharp : register(t1);
SamplerState sharpSamp : register(s1);
struct PSIn {
    float4 params : TEXCOORD1;
    float4 rect : TEXCOORD2;
    float wrap : TEXCOORD3;
};
float4 PSMain(PSIn v) : SV_Target {
    float2 uv = v.params.xy;
    float shade = v.params.z;
    float dist = v.params.w;
    float t = saturate((dist - texSharp.x) / max(texSharp.y - texSharp.x, 0.001));
    float4 c;
    if (v.wrap < 0.5) {
        if (t <= 0.0) c = atlasSharp.SampleLevel(sharpSamp, uv, 0);
        else if (t >= 1.0) c = atlas.Sample(atlasSamp, uv);
        else c = lerp(atlasSharp.SampleLevel(sharpSamp, uv, 0), atlas.Sample(atlasSamp, uv), t);
    } else {
        float2 atlasSize = float2(misc.y, misc.z);
        float2 rel = uv * atlasSize - v.rect.xy;
        if (fmod(v.wrap, 2.0) >= 1.0) rel.x = fmod(rel.x, v.rect.z);
        if (v.wrap >= 2.0) rel.y = fmod(rel.y, v.rect.w);
        float2 wuv = (v.rect.xy + rel) / atlasSize;
        float2 dx = ddx(uv), dy = ddy(uv);
        c = lerp(atlasSharp.SampleGrad(sharpSamp, wuv, dx, dy), atlas.SampleGrad(atlasSamp, wuv, dx, dy), t);
    }
    if (c.a < 0.3) discard;
    float3 col = c.rgb * shade * day.x;
    float fog = saturate((dist - fogParams.x) / (fogParams.y - fogParams.x));
    float3 sky = lerp(float3(0.10, 0.13, 0.24), float3(fogParams.z, fogParams.w, misc.x), day.x);
    col = lerp(col, sky, fog);
    if (camPos.w > 0.5) {
        float3 waterCol = float3(0.06, 0.22, 0.40);
        float depth = saturate(dist / fogParams.y);
        col = lerp(col, waterCol, saturate(0.30 + depth * 0.70));
    }
    return float4(v.params.z, v.params.w * 0.01, v.params.x, 1.0);
}
