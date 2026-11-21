struct VSIn { float2 inPos : POSITION; float2 inUV : TEXCOORD0; float4 inTint : COLOR; };
struct VSOut { float4 pos : SV_Position; float2 uv : TEXCOORD0; float4 tint : TEXCOORD1; };
VSOut VSMain(VSIn input) { VSOut o; o.pos = float4(input.inPos, 0.0, 1.0); o.uv = input.inUV; o.tint = input.inTint; return o; }
