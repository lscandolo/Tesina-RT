#pragma once
#ifndef RENDERER_HPP
#define RENDERER_HPP

#include <misc/log.hpp>
#include <misc/ini.hpp>

#include <gpu/interface.hpp>

#include <rt/ray.hpp>
#include <rt/framebuffer.hpp>
#include <rt/scene.hpp>

#include <rt/renderer-config.hpp>

#include <rt/primary-ray-generator.hpp>
#include <rt/secondary-ray-generator.hpp>
#include <rt/ray-shader.hpp>
#include <rt/tracer.hpp>
#include <rt/bvh-builder.hpp>
#include <rt/frame-stats.hpp>

#include <string>


class Renderer {

public:

        Renderer();

        uint32_t set_up_frame(memory_id tex_id, Scene& scene);
        uint32_t update_accelerator(Scene& scene);

        uint32_t update_configuration();

        uint32_t render_to_framebuffer(Scene& scene);

        uint32_t render_to_texture(Scene& scene);
        uint32_t render_to_texture(Scene& scene, memory_id tex_id);

        uint32_t copy_framebuffer();
        uint32_t copy_framebuffer(memory_id tex_id);

        uint32_t conclude_frame(Scene& scene);
        uint32_t conclude_frame(Scene& scene, memory_id tex_id);

        uint32_t initialize_from_ini_file(std::string file_path);
        uint32_t initialize(std::string log_filename = "rt-log");

        uint32_t get_framebuffer_h(){return fb_h;}
        uint32_t get_framebuffer_w(){return fb_w;}

        int32_t  set_samples_per_pixel(size_t spp,
                                       pixel_sample_cl const* pixel_samples);
        size_t   get_samples_per_pixel();

        void     set_static_bvh(bool b){static_bvh = b;}
        bool     is_static_bvh(){return static_bvh;}

        void     set_max_bounces(uint32_t b){max_bounces = b;}
        uint32_t get_max_bounces(){return max_bounces;}

        void     clear_stats() {stats.clear_times(); stats.clear_mean_times();}
        const FrameStats& get_frame_stats() {return stats;}

        INIReader ini;

        Log                   log;

        RendererConfig        config;

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

        uint32_t              fb_w,fb_h;
        size_t                tile_size;
        bool                  static_bvh;
        uint32_t              max_bounces;

        FrameStats            stats;
        rt_time_t             frame_timer;

        memory_id             target_tex_id;
};

#endif /* RENDERER_HPP */
