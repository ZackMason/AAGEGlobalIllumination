// Author: Zackery Mason-Blaug
// Date: 2022-08-12
//////////////////////////////////////////////////////////


#pragma once

#include "core.hpp"

#include "Graphics/buffer.hpp"
#include "Graphics/vertex_array.hpp"

#include "Util/random.hpp"
#include "Util/logger.hpp"
#include "Util/profile.hpp"

#include "Math/ray.hpp"

#include <execution>

struct voxel_t {
    int     flag{0};
};

template <size_t W, size_t H, size_t D>
struct world_chunk_t {    
    struct vertex_t {
        v3f position{0.0f};
        v3f normal{0.0f, 1.0f, 0.0f};
        v2f uv{0.0f};
        v2f luv{0.0f};
        f32 lum{0.0f};
    };

    template <typename T>
    using chunk_array_t = std::vector<T>;

    buffer_t<chunk_array_t<vertex_t>> buffer;
    vertex_array_t vertex_array;
    chunk_array_t<voxel_t> voxels{};

    aabb_t<v3f> aabb;


    world_chunk_t()
        : buffer(), vertex_array(0)
    {
        voxels.reserve(W*H*D);
        voxels.resize(W*H*D);
                    
        aabb_t<v3i> top_hole;
        top_hole.min = {W/3+1,H/2,D/3+1};
        top_hole.max = {2*W/3-1,H+2,2*D/3-1};
        
        for (int x = 0; x < W; x++) {
            for (int y = 0; y < H; y++) {
                for (int z = 0; z < D; z++) {
                    // make box
                    get_voxel(x,y,z).flag = 0;
                    get_voxel(x,y,z).flag = !x || x == W-1 || !y || y == H-1 || !z || z == D-1;

                    // pillars
                    if (!0) {
                        get_voxel(x,y,z).flag |= x == W/3 && z == D/3;
                        get_voxel(x,y,z).flag |= x == 2*W/3 && z == D/3;
                        get_voxel(x,y,z).flag |= x == W/3 && z == 2*D/3;
                        get_voxel(x,y,z).flag |= x == 2*W/3 && z == 2*D/3;
                    }

                    if (top_hole.contains({x,y,z})) {
                        get_voxel(x,y,z).flag = 0;
                    }
                }
            }
        }

        for (int x = 0; x < W; x++) {
            for (int y = 0; y < H; y++) {
                for (int z = 0; z < D; z++) {
                    if (get_voxel(x,y,z).flag) {
                        emit_cube(v3f{x,y,z});
                    }
                }
            }
        }

        buffer.create();

        vertex_array.bind_ref()
            .set_attrib(0, 3, GL_FLOAT, sizeof(vertex_t), offsetof(vertex_t, position))
            .set_attrib(1, 3, GL_FLOAT, sizeof(vertex_t), offsetof(vertex_t, normal))
            .set_attrib(2, 2, GL_FLOAT, sizeof(vertex_t), offsetof(vertex_t, uv))
            .set_attrib(3, 2, GL_FLOAT, sizeof(vertex_t), offsetof(vertex_t, luv))
            .set_attrib(4, 1, GL_FLOAT, sizeof(vertex_t), offsetof(vertex_t, lum))
            .unbind();

        buffer.update_buffer();
        buffer.unbind();
    }

    void draw() {
        vertex_array.draw();
    }

    voxel_t& get_voxel(auto x, auto y, auto z) {
        return voxels[x + y*(W*D) + z * (W)];
    }

    voxel_t& get_voxel(v3i p) {
        return get_voxel(p.x, p.y, p.z);
    }

    voxel_t* try_voxel(v3i p) {
        if (p.x < 0 || p.y < 0 || p.z < 0 || p.x >= W || p.y >= H || p.z >= D) {
            return nullptr;
        }
        return &get_voxel(p);
    }

    void emit_cube(const v3f& pos) {
        emit_face(pos, {1,0,0});
        emit_face(pos, {-1,0,0});
        emit_face(pos, {0,1,0});
        emit_face(pos, {0,-1,0});
        emit_face(pos, {0,0,1});
        emit_face(pos, {0,0,-1});
    }

    void emit_face(const v3f& pos, const v3f& dir) {
        const v3i index = v3i{pos + dir};
        if (const auto voxel = try_voxel(index); voxel && voxel->flag) {
            //logger_t::info(fmt::format("Culling face: {} + {} = {}", pos, dir, index));
            return;
        }

        const f32 scale = 0.5f * voxel_size;
        const v3f tangent = v3f{dir.y, dir.z, dir.x} * scale;
        const v3f bitangent = glm::normalize(glm::cross(dir, tangent)) * scale;

        const auto emit = [&](float i, float j) {
            vertex_t vertex;
            vertex.position = voxel_size * pos + dir * scale + tangent * i + bitangent * j;
            vertex.normal = dir;
            vertex.uv = v2f{ i < 0 ? 0 : 1, j < 0 ? 0 : 1};
            vertex.luv = v2f{
                i < 0 ? current_luv.x : current_luv.x + luv_size,
                j < 0 ? current_luv.y : current_luv.y + luv_size
            };
            current_luv.x += luv_size + luv_gap;
            if (current_luv.x >= 1.0f) {
                current_luv.x = luv_gap;
                current_luv.y += luv_size + luv_gap;
            }
            if (current_luv.y >= 1.0f) {
                logger_t::warn("VOXEL :: Light Map UV exceeded 1.0f!");
            }
            
            buffer.data.push_back(vertex);
            
            aabb.expand(vertex.position);
        };

        emit(1,1);
        emit(-1, 1);
        emit(1,-1);

        emit(1,-1);
        emit(-1,1);
        emit(-1,-1);

        vertex_array.size = static_cast<GLsizei>(buffer.data.size());
    }

    void clear_light() {
          std::for_each(std::execution::par_unseq, buffer.data.begin(), buffer.data.end(), 
            [&](vertex_t& vertex) {
                vertex.lum = 0.1f;
        });
    }

    void compute_light(const v3f& position, const f32 strength) {
        profile_t p{"compute_light"};

        constexpr auto ray_count = 1;

        const auto ray_tri_intersect = 
            [](
                const ray_t& ray, 
                const v3f& v0,
                const v3f& v1,
                const v3f& v2,
                float& t        
            ) {
                const auto v0v1 = v1 - v0;
                const auto v0v2 = v2 - v0;
                const auto n = glm::cross(v0v1, v0v2);
                const auto area2 = glm::length(n);

                const float n_dot_rd = glm::dot(n, ray.direction);
                if (fabs(n_dot_rd) < 0.00001f) {
                    return false;
                }

                const float d = - glm::dot(n, v0);
                t = -(glm::dot(n, ray.origin) + d) / n_dot_rd;

                if (t < 0) return false;
                const auto p = ray.origin + t * ray.direction;

                v3f c;
                const auto e0 = v1 - v0;
                const auto vp0 = p - v0;
                c = glm::cross(e0, vp0);
                if (glm::dot(n, c) < 0) return false;

                const auto e1 = v2 - v1;
                const auto vp1 = p - v1;
                c = glm::cross(e1, vp1);
                if (glm::dot(n, c) < 0) return false;
                
                const auto e2 = v0 - v2;
                const auto vp2 = p - v2;
                c = glm::cross(e2, vp2);
                if (glm::dot(n, c) < 0) return false;

                return true;
            };

        random_t<xor64_random_t> rng;
        rng.randomize();

        std::for_each(std::execution::par_unseq, buffer.data.begin(), buffer.data.end(), 
            [&](vertex_t& vertex) {
                for (int j{0}; j < ray_count; j++) {
                    ray_t ray;
                    ray.origin = vertex.position + vertex.normal * 0.001f;
                    ray.direction = glm::normalize(position - vertex.position);
                    bool hit = false;

                    const float light_distance = glm::distance(ray.origin, position);

                    if (glm::dot(ray.direction, vertex.normal) < 0.0f) {
                        hit = true;
                    } else
                    for (auto i{0}; i < buffer.data.size(); i += 3) {
                        float t;
                        const auto this_hit = ray_tri_intersect(
                            ray, 
                            buffer.data[i].position,
                            buffer.data[i+1].position,
                            buffer.data[i+2].position,
                            t
                        );
                        if (this_hit && t < light_distance) {
                            hit = true;
                            break;
                        }
                    }

                    if (!hit) {
                        vertex.lum += strength / light_distance / f32(ray_count);
                    }
                }
        });

        buffer.update_buffer();

        logger_t::info(fmt::format("Light Time: {}ms", p.end()));

    }

    f32 voxel_size{8.0f};

private:
    f32 luv_size{1.0f/W/W};
    f32 luv_gap{luv_size*0.01f};
    v2f current_luv{luv_gap};
};

