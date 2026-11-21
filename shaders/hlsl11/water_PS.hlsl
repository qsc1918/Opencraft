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
struct PSIn {
    float4 params : TEXCOORD1;
    float4 rect : TEXCOORD2;
    float wrap : TEXCOORD3;
};
float4 PSMain(PSIn v) : SV_Target {
    float2 uv = v.params.xy;
    float shade = v.params.z;
    float dist = v.params.w;
    float4 c;
    if (v.wrap < 0.5) c = atlas.Sample(atlasSamp, uv);
    else {
        float2 atlasSize = float2(misc.y, misc.z);
        float2 rel = uv * atlasSize - v.rect.xy;
        if (fmod(v.wrap, 2.0) >= 1.0) rel.x = fmod(rel.x, v.rect.z);
        if (v.wrap >= 2.0) rel.y = fmod(rel.y, v.rect.w);
        c = atlas.SampleGrad(atlasSamp, (v.rect.xy + rel) / atlasSize, ddx(uv), ddy(uv));
    }
    float3 tint = pow(float3(0.40, 0.62, 1.10), float3(2.2, 2.2, 2.2));
    float3 col = c.rgb * shade * day.x * tint;
    float fog = saturate((dist - fogParams.x) / (fogParams.y - fogParams.x));
    float3 sky = lerp(float3(0.10, 0.13, 0.24), float3(fogParams.z, fogParams.w, misc.x), day.x);
    col = lerp(col, sky, fog);
    if (camPos.w > 0.5) {
        float3 waterCol = float3(0.06, 0.22, 0.40);
        float depth = saturate(dist / fogParams.y);
        col = lerp(col, waterCol, saturate(0.30 + depth * 0.70));
    }
    return float4(col, 0.82);
}
