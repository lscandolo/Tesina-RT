#pragma once
#ifndef RENDERER_HPP
#define RENDERER_HPP

#include <misc/log.hpp>
#include <misc/ini.hpp>

#include <gpu/interface.hpp>

#include <rt/ray.hpp>
#include <rt/framebuffer.hpp>
#include <rt/scene.hpp>

#include <rt/primary-ray-generator.hpp>
#include <rt/secondary-ray-generator.hpp>
#include <rt/ray-shader.hpp>
#include <rt/tracer.hpp>
#include <rt/bvh-builder.hpp>

#include <string>


class Renderer {

public:

        uint32_t set_up_frame(memory_id tex_id, Scene& scene);
        uint32_t update_accelerator(Scene& scene);

        uint32_t render_to_framebuffer(Scene& scene);

        uint32_t render_to_texture(Scene& scene);
        uint32_t render_to_texture(Scene& scene, memory_id tex_id);

        uint32_t copy_framebuffer();
        uint32_t copy_framebuffer(memory_id tex_id);

        uint32_t conclude_frame(Scene& scene);
        uint32_t conclude_frame(Scene& scene, memory_id tex_id);

        uint32_t initialize_from_ini_file(std::string file_path);
        //uint32_t initialize_scene(Scene& scene);
        uint32_t initialize(CLInfo clinfo);

        uint32_t get_framebuffer_h(){return fb_h;}
        uint32_t get_framebuffer_w(){return fb_w;}

        void     set_static_bvh(bool b){static_bvh = b;}
        void     set_max_bounces(size_t b){max_bounces = b;}

        INIReader ini;

private:

        RayBundle             ray_bundle_1,ray_bundle_2;
        HitBundle             hit_bundle;
        Cubemap               cubemap;
        FrameBuffer           framebuffer;
        Camera                camera;

        BVHBuilder            bvh_builder;
        PrimaryRayGenerator   prim_ray_gen;
        SecondaryRayGenerator sec_ray_gen;
        RayShader             ray_shader;
        Tracer                tracer;

        Log                   log;

        uint32_t fb_w,fb_h;

        rt_time_t frame_timer;

        void   clear_timers();
        double bvh_build_time;
        double prim_gen_time;
        double sec_gen_time;
        double prim_trace_time;
        double sec_trace_time;
        double prim_shadow_trace_time;
        double sec_shadow_trace_time;
        double shader_time;
        double fb_clear_time;
        double fb_copy_time;

        size_t total_ray_count;
        size_t total_sec_ray_count;
        size_t tile_size;

        memory_id target_tex_id;
        size_t max_bounces;
        bool static_bvh;

};

#endif /* RENDERER_HPP */