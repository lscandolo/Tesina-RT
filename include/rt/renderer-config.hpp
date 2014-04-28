#pragma once
#ifndef RTCONFIG_HPP
#define RTCONFIG_HPP

class Renderer;

class RendererConfig {

        RendererConfig();

protected:

        friend class Renderer;
        void set_target(Renderer* r);

private:

        Renderer* target;

public:
        // Config parameters

        int use_lbvh;                // Done

        int bvh_refit_only;          // Done
        int bvh_depth;               // Done
        int bvh_min_leaf_size;       // Done

        int sec_ray_use_atomics;     // Done
        int sec_ray_use_disc;        // Done

        int prim_ray_quad_size;      // Done
        int prim_ray_use_zcurve;     // Done

        double tile_to_cores_ratio;  // Done
};

#endif // RTCONFIG_HPP
