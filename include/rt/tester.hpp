#pragma once
#ifndef RTTESTER_HPP
#define RTTESTER_HPP

#include <cl-gl/opengl-init.hpp>
#include <cl-gl/opencl-init.hpp>
#include <rt/rt.hpp>

struct resolution_t 
{
        resolution_t() {}
        resolution_t(size_t w, size_t h) : width(w), height(h) {}
        size_t width;
        size_t height;
};

class Tester 
{
public:

        static Tester* getSingletonPtr();

        size_t getWindowWidth();
        size_t getWindowHeight();

        Scene* getScene();
        Renderer* getRenderer();

        int initialize();
        void loop();
private:

        Tester();
        static Tester* instance;

        int testConfiguration();
        int renderOneFrame();

        int setResolution(int i);
        int loadScene(int i);

        void printStatsHeaders();
        void printStatsValues();

        Renderer renderer;
        Scene scene;

        GLuint    gl_tex_id;
        memory_id cl_tex_id;

        rt_time_t debug_timer;
        rt_time_t loop_timer;
        FrameStats debug_stats;

        resolution_t window_size;
        vec3* stats_camera_pos;
        vec3* stats_camera_dir;

/////////// test parameters
        std::vector<int>          scenes;
        std::vector<resolution_t> window_sizes;
        // std::vector<size_t>       rays_per_pixel;
        std::vector<int>          view_positions;
        std::vector<double>       tile_to_cores_ratios;
        std::vector<bool>         sec_ray_use_disc;
        std::vector<bool>         sec_ray_use_atomics;
        std::vector<size_t>       prim_ray_quad_sizes;
        std::vector<bool>         prim_ray_use_zcurve;
        std::vector<size_t>       bvh_max_depths;
        std::vector<size_t>       bvh_min_leaf_sizes;
        std::vector<bool>         bvh_refit_only;

///////////// test indices
        int scene_i;
        int wsize_i;
        int bvh_refit_i;
        int bvh_min_leaf_sizes_i;
        int bvh_max_depths_i;
        int prim_ray_use_zcurve_i;
        int prim_ray_quad_sizes_i;
        int sec_ray_use_atomics_i;
        int sec_ray_use_disc_i;
        int tile_to_cores_ratios_i;
        int view_positions_i;
};

#endif // RTTESTER_HPP
