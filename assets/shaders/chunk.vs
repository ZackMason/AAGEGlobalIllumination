#version 460 core

#include <voxel_attr.glsl>

out VS_OUT {
    vec3 pos;
    vec3 normal;
    vec2 uv;
    vec2 luv;
    float lum;
} vs_out;

layout (std140, binding = 0) uniform camera_u {
    vec4 position;
    mat4 vp;
} uCamera;

uniform mat4 uM;

void main() {
    vs_out.pos = (uM * vec4(aPos, 1.0)).xyz;
    vs_out.normal = aNormal;
    vs_out.uv = aUV;
    vs_out.luv = aLUV;
    vs_out.lum = aLUM;

    gl_Position = (uCamera.vp * uM * vec4(aPos, 1.0));
}