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


enum rt_stage {
        BVH_BUILD = 0,
        PRIM_RAY_GEN,
        SEC_RAY_GEN,
        PRIM_TRACE,
        PRIM_SHADOW_TRACE,
        SEC_TRACE,
        SEC_SHADOW_TRACE,
        SHADE,
        FB_CLEAR,
        FB_COPY
};

const size_t rt_stages = 10;

struct FrameStats {
        double   get_stage_time(rt_stage stage) const {
                return stage_times[stage];}
        double   get_stage_mean_time(rt_stage stage) const {
                if (acc_frames)
                        return stage_acc_times[stage]/acc_frames;
                return 0;
        }

        double   get_frame_time() const {
                return frame_time;}
        double   get_mean_frame_time() const {
                if (acc_frames)
                        return frame_acc_time/acc_frames;
                return 0;
        };

        size_t  get_ray_count() const {
               return total_ray_count;}
        size_t  get_secondary_ray_count() const {
               return total_sec_ray_count;}

        void clear_times() {
                for (uint32_t i = 0; i < rt_stages; ++i) {
                        stage_times[i] = 0;
                }
        }

        void clear_mean_times() {
                acc_frames = 0;
                for (uint32_t i = 0; i < rt_stages; ++i) {
                        stage_acc_times[i] = 0;
                }
        }

        double stage_times[rt_stages];
        double stage_acc_times[rt_stages];
        uint32_t acc_frames;

        double frame_time;
        double frame_acc_time;

        size_t total_ray_count;
        size_t total_sec_ray_count;

};

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
        uint32_t initialize(CLInfo clinfo, std::string log_filename = "rt-log");

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