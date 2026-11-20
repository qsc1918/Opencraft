#version 330 core

layout(location = 0) out vec2 vUV;

void main() {
    vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
    vUV = p;
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
    gl_Position.y = -gl_Position.y;
}
