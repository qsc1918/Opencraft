cbuffer Sky : register(b0) { float4 horizon; float4 zenith; };
struct PSIn { float2 uv : TEXCOORD0; };
float4 PSMain(PSIn v) : SV_Target { float3 c = lerp(zenith.rgb, horizon.rgb, saturate(v.uv.y)); if (horizon.a > 0.5) c = lerp(c, float3(0.05, 0.18, 0.34), 0.85); return float4(c, 1.0); }
