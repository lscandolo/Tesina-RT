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

        // Config parameters

        int use_lbvh;

        int bvh_refit_only;
        int bvh_depth;
        int bvh_min_leaf_size;

        int sec_ray_use_atomics;
        int sec_ray_use_disc;

        int prim_ray_quad_size;
        int prim_ray_use_zcurve;

        double tile_to_cores_ratio;

};

#endif RTCONFIG_HPP
