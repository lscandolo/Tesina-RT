#include <rt/renderer-config.hpp>
#include <rt/renderer.hpp>

RendererConfig::RendererConfig()
  : target(0)
  , use_lbvh(true)
  , bvh_refit_only(false)
  , bvh_depth(32)
  , bvh_min_leaf_size(1)
  , sec_ray_use_atomics(false)
  , sec_ray_use_disc(true)
  , prim_ray_quad_size(32)
  , prim_ray_use_zcurve(false)
  , tile_to_cores_ratio(128)
{
}


void RendererConfig::set_target(Renderer* r)
{
        target = r;
}
