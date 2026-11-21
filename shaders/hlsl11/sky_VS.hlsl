struct VSOut { float4 pos : SV_Position; float2 uv : TEXCOORD0; };
VSOut VSMain(uint id : SV_VertexID) { VSOut o; float2 p = float2(float((id << 1) & 2), float(id & 2)); o.uv = p; o.pos = float4(p * 2.0 - 1.0, 0.0, 1.0); return o; }
