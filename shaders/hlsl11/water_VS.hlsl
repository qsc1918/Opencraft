cbuffer VSUbo : register(b0) { float4 viewProjRows[4]; float4 camPos; float4 fogParams; float4 misc; };
cbuffer Tiles : register(b1) { float4 tileRect[128]; };
cbuffer Pc : register(b2) { float4 pcOrigin; };
struct VSIn { float4 inPos : POSITION; float4 inMeta : TEXCOORD0; };
struct VSOut {
    float4 pos : SV_Position;
    float4 params : TEXCOORD1;
    float4 rect : TEXCOORD2;
    float wrap : TEXCOORD3;
};
VSOut VSMain(VSIn input) {
    VSOut o;
    float3 p = float3(input.inPos.xyz) / 16.0;
    float3 wp = p + pcOrigin.xyz;
    float4x4 viewProj = transpose(float4x4(viewProjRows[0], viewProjRows[1], viewProjRows[2], viewProjRows[3]));
    o.pos = mul(viewProj, float4(wp, 1.0));
    uint tex = (uint)input.inMeta.z;
    float2 tileUV = input.inMeta.xy;
    float4 r = tileRect[tex];
    float2 px = r.xy + tileUV * r.zw / 16.0;
    o.rect = r;
    o.wrap = input.inPos.w;
    o.params = float4(px / float2(misc.y, misc.z),
                      pow(input.inMeta.w / 255.0, 2.2),
                      length(wp - camPos.xyz));
    return o;
}
