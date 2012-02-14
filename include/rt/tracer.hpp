#ifndef RT_TRACER_HPP
#define RT_TRACER_HPP

#include <cl-gl/opencl-init.hpp>
#include <rt/timing.hpp>
#include <rt/scene.hpp>
#include <rt/ray.hpp>

class Tracer {

public:

	bool initialize(CLInfo& clinfo);

	bool trace(SceneInfo& scene_info, int32_t ray_count, 
		   RayBundle& rays, HitBundle& hits);

	bool shadow_trace(SceneInfo& scene_info, int32_t ray_count, 
			  RayBundle& rays, HitBundle& hits);

	void enable_timing(bool b);
	double get_trace_exec_time();
	double get_shadow_exec_time();

private:

	CLKernelInfo tracer_clk;
	CLKernelInfo shadow_clk;

	bool         timing;
	rt_time_t    tracer_timer;
	double       tracer_time_ms;
	rt_time_t    shadow_timer;
	double       shadow_time_ms;
};

#endif /* RT_TRACER_HPP */
