Texture2D<float4> menu : register(t0);
SamplerState texSamp : register(s0);
struct PSIn { float2 uv : TEXCOORD0; float4 tint : TEXCOORD1; };
float4 PSMain(PSIn v) : SV_Target { return menu.SampleLevel(texSamp, v.uv, 0) * v.tint; }
