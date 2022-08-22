#version 460 core

layout(local_size_x = 1, local_size_y = 1) in;

layout(rgba8, binding = 0) uniform image2D uOutput;
    
struct vertex_t {
    vec3 p;
    vec3 n;
    vec2 uv;
    vec2 luv;
};

layout(std430, binding = 0) buffer vertices {
    vertex_t verts[];
};

uniform int vert_size;

const int texture_size = 1024;

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
    return float(in_triangle(uv, v0.luv, v1.luv, v2.luv)) * vec3(1); 
}

void main() {
    ivec2 pixel_coord = ivec2(gl_GlobalInvocationID.xy);
 //   imageStore(uOutput, pixel_coord, vec4(0));

    vec3 color = vec3(0.0);
    for (int i = 0; i < vert_size; i += 3) {
        //color += draw_triangle(vec2(pixel_coord) / 1024.0, verts[i], verts[i+1], verts[i+2]);
    }

    //imageStore(uOutput, pixel_coord, imageLoad(uOutput, pixel_coord) + vec4(0.001));
    imageStore(uOutput, pixel_coord, vec4(color, 1.0));
}