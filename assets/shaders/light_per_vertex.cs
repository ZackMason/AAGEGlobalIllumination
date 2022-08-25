#version 460 core

layout(local_size_x = 1, local_size_y = 1) in;

layout(rgba8, binding = 0) uniform image2D uOutput;
    
struct vertex_t {
    vec3 p;
    vec3 n;
    vec2 uv;
    vec2 luv;
    float lum;
};

layout(std430, binding = 0) buffer vertices {
    vertex_t verts[];
};

uniform int vert_size;

const int texture_size = 1024;
const float uv_pixel_size = 1.0 / 1024.0;

float sign3(vec2 p1, vec2 p2, vec2 p3) {
    return (p1.x - p3.x) * (p2.y - p3.y) - (p2.x - p3.x) * (p1.y - p3.y);
}

bool in_triangle(vec2 pt, vec2 v1, vec2 v2, vec2 v3) {
    float d1, d2, d3;
    bool neg, pos;
    d1 = sign3(pt, v1, v2);
    d2 = sign3(pt, v2, v3);
    d3 = sign3(pt, v3, v1);

    neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
    return !(neg && pos);
}

vec3 draw_triangle(vec2 uv, vertex_t v0, vertex_t v1, vertex_t v2) {
    return float(in_triangle(uv, v0.luv, v1.luv, v2.luv)) * vec3(0.01); 
}

void main() {
    int triangle = int(gl_GlobalInvocationID.x);
    int v0 = triangle * 3 + 0;
    int v1 = triangle * 3 + 1;
    int v2 = triangle * 3 + 2;
    
    vec2 luv0 = verts[v0].luv;
    vec2 luv1 = verts[v1].luv;
    vec2 luv2 = verts[v2].luv;

    ivec2 lcoord0 = ivec2(luv0 * 1024.0);
    ivec2 lcoord1 = ivec2(luv1 * 1024.0);
    ivec2 lcoord2 = ivec2(luv2 * 1024.0);

    vec3 color = vec3(1.0, 0.0, 0.0);
 
    
    
    color = normalize(color);

    //imageStore(uOutput, pixel_coord, imageLoad(uOutput, pixel_coord) + vec4(0.001));
    imageStore(uOutput, pixel_coord, vec4(color, 1.0));
}