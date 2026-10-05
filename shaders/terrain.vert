#version 450

// 图块数量上限，必须与 atlas.hpp 的 kAtlasMaxTiles、UBO 里的 tileRect 一致
#define kMaxTiles 128

layout(location = 0) in ivec4 inPos;
layout(location = 1) in uvec4 inMeta; // 依次为 u、v、贴图索引、明暗

layout(location = 0) out vec2 vUV;
layout(location = 1) out float vShade;
layout(location = 2) out float vDist;
// 贪心合并出的大面 uv 会超出单个图块，片元里要按图块折回才算"平铺"，
// 所以把图块矩形与是否需要折回一起送过去
layout(location = 3) flat out vec4 vTileRect;
layout(location = 4) flat out float vWrap;

layout(push_constant) uniform Pc {
    vec4 origin;
} pc;

layout(set = 0, binding = 0) uniform Ubo {
    mat4 viewProj;
    vec4 camPos;
    vec4 fogParams; // x 雾起, y 雾止, z 天红, w 天绿
    vec4 misc;      // x 天蓝, y 图集宽, z 图集高, w 备用
    vec4 day;       // x 日照 0..1, yzw 太阳方向
    vec4 texSharp;  // x 近景清晰距离(格), y 过渡结束距离, zw 备用
    vec4 tileRect[kMaxTiles]; // 每个图块的矩形：xy=图集内原点(px), zw=宽高(px)
} ubo;

void main() {
    // inPos.xyz 是 1/16 格单位的整数（13/16 高的框架、4..12 的眼块等亚方块几何都靠它）
    vec3 p = vec3(inPos.xyz) / 16.0;
    vec3 wp = p + pc.origin.xyz;
    gl_Position = ubo.viewProj * vec4(wp, 1.0);

    int tex = int(inMeta.z);
    vec2 tileUV = vec2(float(inMeta.x), float(inMeta.y));
    // 贴图分辨率按图块各自记录：uv 0..16 是"贴图内归一化坐标"，
    // 乘以该图块的像素尺寸后落在图集里（支持任意分辨率材质包）。
    // 注意：不要把 uv 平移到纹素中心（+0.5）。uv 0..16 是"贴图边界到边界"的
    // 归一化坐标，加半像素会让子矩形（如火把只有 2 纹素宽的侧面）整体偏移
    // 半个纹素，边缘采到透明纹素就成了一条裂缝。
    vec4 r = ubo.tileRect[tex];
    vec2 px = r.xy + tileUV * r.zw / 16.0;
    vUV = px / vec2(ubo.misc.y, ubo.misc.z);
    vTileRect = r;
    // 顶点 w 是 mesher 打的平铺标记：1=横向重复、2=纵向重复
    vWrap = float(inPos.w);
    // 明暗系数按原版语义是"显示空间乘法"（显示值 = 贴图 × 系数）：原版在伽马
    // 空间直接乘字节，线性空间里等价于乘 系数^2.2，否则暗部会被抬亮、失去对比。
    vShade = pow(float(inMeta.w) / 255.0, 2.2);
    vDist = length(wp - ubo.camPos.xyz);
}
