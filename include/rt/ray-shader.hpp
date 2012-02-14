#ifndef RT_RAY_SHADER_HPP
#define RT_RAY_SHADER_HPP

#include <cl-gl/opencl-init.hpp>
#include <rt/timing.hpp>
#include <rt/ray.hpp>
#include <rt/scene.hpp>
#include <rt/cubemap.hpp>
#include <rt/framebuffer.hpp>

class RayShader {

public:
	
	bool initialize(CLInfo& clinfo);
	bool shade(RayBundle& rays, HitBundle& hb, SceneInfo& scene_info, 
		   Cubemap& cm, FrameBuffer& fb, int32_t size);

	void enable_timing(bool b);
	double get_exec_time();

private:

	CLKernelInfo shade_clk;
	CLKernelInfo aggregate_clk;

	bool         timing;
	rt_time_t    timer;
	double       time_ms;
};

#endif /* RT_RAY-SHADER_HPP */
