
#define WIN32_MEAN_AND_LEAN
#define NOMINMAX

#include "core.hpp"

#include <glad/glad.h>
#include <glfw/glfw3.h>

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

#include "Engine/window.hpp"
#include "Engine/orbit_camera.hpp"
#include "Engine/asset_loader.hpp"

#include "Graphics/shared_buffer.hpp"
#include "Graphics/texture2d.hpp"
#include "Graphics/compute.hpp"

#include "Math/transform.hpp"

#include "Util/timer.hpp"
#include "Util/color.hpp"

#include "Voxel/world_chunk.hpp"

#include <filesystem>

static constexpr v3f sky_color{0.3f, 0.2f, 1.0f};

struct game_t {
    orbit_camera_t camera{45.0f, 1.0f, 1.0f, 0.1f, 400.0f};
    world_chunk_t<32,16,32> current_chunk{};
    asset_loader_t asset_loader{};

    resource_handle_t<texture2d_t> light_map;
};

struct config_t {
    static void save(game_t& game) {
        v3f camera_pos = game.camera.get_position();
        std::array<f32, 6> camera_data = {
            camera_pos.x,
            camera_pos.y,
            camera_pos.z,
            game.camera.pitch,
            game.camera.yaw,
            game.camera.dist
        };

        std::ofstream file(fmt::format("{}config.txt", GAME_ASSETS_PATH), std::ios::binary | std::ios::out);
        file.write(reinterpret_cast<const char*>(camera_data.data()), 6 * sizeof(f32));
    }

    static void load(game_t& game) {
        if (!std::filesystem::exists(fmt::format("{}config.txt", GAME_ASSETS_PATH))) return;

        std::array<f32, 6> camera_data;
        std::ifstream file(fmt::format("{}config.txt", GAME_ASSETS_PATH), std::ios::binary | std::ios::in);
        
        file.read(reinterpret_cast<char*>(camera_data.data()), 6 * sizeof(f32));
        
        game.camera.position.x = camera_data[0];
        game.camera.position.y = camera_data[1];
        game.camera.position.z = camera_data[2];
        game.camera.pitch = camera_data[3];
        game.camera.yaw = camera_data[4];
        game.camera.dist = camera_data[5];
    }
};

struct camera_u { 
    using value_type = camera_u;
    void* data() {
        return &p;
    }
    size_t size() const {
        return 1;
    }

    v4f p;
    m44 vp;
};

struct light_u {
    using value_type = light_u;
    void* data() {
        return this;
    }
    size_t size() const {
        return 1;
    }

    void set_position(const v3f& v) {
        position = v4f{v, 0.0f};
    }

    v4f position;
};

int main(int argc, char** argv) {
    random_s::randomize();

    shader_t::add_attribute_definition(
        {
            "vec3 aPos",
            "vec3 aNormal",
            "vec2 aUV",
            "vec2 aLUV",
            "float aLUM"
        },
        "voxel_attr",
        GAME_ASSETS_PATH
    );

    window_t window;
    window.width = 640;
    window.height = 480;
    window.open_window();

    window.set_title("AAGE Template");
    window.make_imgui_context();

    game_t game{};
    game.asset_loader.asset_dir = GAME_ASSETS_PATH;

    window.set_event_callback([&](auto& event){
        event_handler_t handler;

        handler.dispatch<mouse_scroll_event_t>(event, 
            std::bind(&orbit_camera_t::on_scroll_event, &game.camera, std::placeholders::_1));

        handler.dispatch<key_event_t>(event, [&](const key_event_t& e) {
            switch(e.key) {
                case GLFW_KEY_R:
                    game.asset_loader.reload<shader_t>();
                    break;
                case GLFW_KEY_Z:
                    window.close_window();
                    break;
                case GLFW_KEY_F:
                    window.toggle_fullscreen();
                    break;
            }

            return true;
        });
    });

    shared_buffer_t<camera_u> camera_buffer{
        create_shared_buffer(camera_u{v4f{game.camera.get_position(), 0.0f}, game.camera.view_projection()})
    };
    bind_buffer(camera_buffer, 0);

    resource_handle_t<shader_t> world_chunk_shader;
  
    world_chunk_shader = game.asset_loader.get_shader("world_chunk", {
        "shaders/chunk.vs", "shaders/chunk.fs"
    });

    glEnable(GL_DEPTH_TEST);

    timer32_t timer;

    constexpr auto alb_size{256};
    resource_handle_t<texture2d_t> albedo_texture = game.asset_loader.create_texture2d("albedo", alb_size, alb_size);
    
    albedo_texture.get().set([](int x, int y) -> u32 {
        constexpr auto margin = alb_size/16;
        using namespace color;
        return (x < margin || y < margin || x > alb_size-margin || y > alb_size-margin)
            ? "fefefeff"_rgba : "afafafff"_rgba;
    });

    resource_handle_t<shader_t> light_compute_shader = game.asset_loader.get_shader("light", {"shaders/light.cs"});
    game.light_map = game.asset_loader.create_texture2d("light_map", 1024, 1024);
    
    game.light_map.get().set([](int x, int y) -> u32 {
        return sin(x/1024.0f*3.1f*20.0f) * cos(y/1024.0f*3.1f*20.0f) > 0.0f ? color::rgba::black : color::rgba::white;
    });


    logger_t::info(fmt::format("Number of vertices: {}", game.current_chunk.vertex_array.size));

    config_t::load(game);

    struct light_t {
        v3f position;
        f32 strength;
    };

    std::vector<light_t> lights;
    lights.emplace_back(game.current_chunk.aabb.center(), 10.0f);
    lights.emplace_back(game.current_chunk.aabb.center() + v3f{0,1000,0}, 150.0f);

    logger_t::info("Computing lighting..");
    game.current_chunk.clear_light();
    for (const auto& light: lights) {
        game.current_chunk.compute_light(light.position, light.strength);
    }
    logger_t::info("Lighting completed!");

    while(!window.should_close()) {
        const auto dt = timer.get_dt(window.get_ticks());

        window.set_title(fmt::format("AAGE Template - FPS: {}", int(1.0f / dt + 0.5f)).c_str());

        window.imgui_new_frame();

        glClearColor(sky_color.r, sky_color.g, sky_color.b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        /////////////////////////////
        // Update 
        
        game.camera.update(window, dt);


        /////////////////////////////
        // Global Illumination

        constexpr auto compute_gi = false;

        if constexpr (compute_gi) {
            const auto compute_id = light_compute_shader.get().id;
            light_compute_shader.get().bind();
            light_compute_shader.get().set_int("uOutput", 0);
            light_compute_shader.get().set_int("vert_size", game.current_chunk.vertex_array.size);

            game.light_map.get().slot = 0;
            game.light_map.get().bind_image();
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, game.current_chunk.vertex_array.id);

            glDispatchCompute(1024, 1024, 1);
            glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
        } else {
            
        }


        /////////////////////////////
        // Render

        // update global camera uniform
        camera_u camera{v4f{game.camera.get_position(),0.0f}, game.camera.view_projection()};
        set_shared_buffer(camera_buffer, &camera);

        // draw world
        albedo_texture.get().bind_slot(0);
        albedo_texture.get().bind();
        
        world_chunk_shader.get().bind();
        world_chunk_shader.get().set_mat4("uM", m44{1.0f});
        world_chunk_shader.get().set_int("uAlbedo", 0);
        world_chunk_shader.get().set_int("uLightMap", 1);

        game.current_chunk.draw();

        /////////////////////////////
        ImGui::Begin("Debug");

            if (ImGui::Button("Recompute Light")) {
                game.current_chunk.clear_light();
                game.current_chunk.compute_light({32,32,32}, 10.0f);
            }

            const size_t tex_id = static_cast<size_t>(game.light_map.get().id);
            ImGui::Image(reinterpret_cast<ImTextureID>(tex_id),{128,128},{0,1},{1,0});
        ImGui::End();

        window.imgui_render();
        /////////////////////////////
        window.poll_events();
        window.swap_buffers();
    }

    config_t::save(game);

    return 0;
}