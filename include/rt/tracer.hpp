#ifndef RT_TRACER_HPP
#define RT_TRACER_HPP

#include <cl-gl/opencl-init.hpp>
#include <rt/scene.hpp>
#include <rt/ray.hpp>

class Tracer {

public:

	bool initialize(CLInfo& clinfo);

	bool trace(SceneInfo& scene_info, int32_t ray_count, 
		   RayBundle& rays, HitBundle& hits);

	bool shadow_trace(SceneInfo& scene_info, int32_t ray_count, 
			  RayBundle& rays, HitBundle& hits);

private:

	CLKernelInfo tracer_clk;
	CLKernelInfo shadow_clk;

};

#endif /* RT_TRACER_HPP */
