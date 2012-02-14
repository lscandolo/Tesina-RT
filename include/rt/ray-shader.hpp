#ifndef RT_RAY_SHADER_HPP
#define RT_RAY_SHADER_HPP

#include <cl-gl/opencl-init.hpp>
#include <rt/ray.hpp>
#include <rt/scene.hpp>
#include <rt/cubemap.hpp>
#include <rt/framebuffer.hpp>

class RayShader {

public:
	
	bool initialize(CLInfo& clinfo);
	bool shade(RayBundle& rays, HitBundle& hb, SceneInfo& scene_info, 
		   Cubemap& cm, FrameBuffer& fb, int32_t size);

private:

	CLKernelInfo shade_clk;
	CLKernelInfo aggregate_clk;
};

#endif /* RT_RAY-SHADER_HPP */
