#version 450

layout(location=0) out vec2 outNDC;
const vec2 P[3] = vec2[3]( vec2(-1,-1), vec2(3,-1), vec2(-1,3) );

void main() {
    gl_Position = vec4(P[gl_VertexIndex], 0, 1);
    outNDC = P[gl_VertexIndex];
}