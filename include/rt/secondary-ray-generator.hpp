#ifndef RT_SECONDARY_RAY_GENERATOR_HPP
#define RT_SECONDARY_RAY_GENERATOR_HPP

#include <cl-gl/opencl-init.hpp>
#include <rt/timing.hpp>
#include <rt/ray.hpp>
#include <rt/scene.hpp>

class SecondaryRayGenerator {

public:

	bool initialize(CLInfo& clinfo);
	bool generate(SceneInfo& si, RayBundle& ray_in, int32_t rays_in,
		      HitBundle& hits, RayBundle& ray_out, int32_t* rays_out);
	void set_max_rays(int32_t max);

	void enable_timing(bool b);
	double get_exec_time();
      

private:

	cl_mem ray_count_mem;
	cl_int generated_rays;
	cl_int max_rays;
	CLKernelInfo generator_clk;

	bool         timing;
	rt_time_t    timer;
	double       time_ms;
};

#endif /* RT_SECONDARY_RAY_GENERATOR_HPP */
