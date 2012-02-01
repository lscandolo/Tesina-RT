#ifndef RT_FRAMEBUFFER_HPP
#define RT_FRAMEBUFFER_HPP

#include <stdint.h>

#include <cl-gl/opencl-init.hpp>
#include <rt/ray.hpp>
#include <rt/scene.hpp>
#include <rt/cubemap.hpp>


class FrameBuffer {
	
public:

	// bool initialize(CLInfo& clinfo, uint32_t sz[2]);
	bool initialize(CLInfo& clinfo, uint32_t sz[2],
			SceneInfo& scene_info, cl_mem& cl_hit_mem);

	cl_mem& image_mem(){return img_mem;}

	bool clear();
	bool update(RayBundle& rays, Cubemap& cm, int32_t size, cl_int arg);
	bool copy(cl_mem& tex_mem);

private:

	uint32_t size[2];

	CLKernelInfo init_clk;
	CLKernelInfo update_clk;
	CLKernelInfo copy_clk;

	cl_mem img_mem;
};

#endif /* RT_FRAMEBUFFER_HPP */
