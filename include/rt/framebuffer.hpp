#ifndef RT_FRAMEBUFFER_HPP
#define RT_FRAMEBUFFER_HPP

#include <stdint.h>

#include <cl-gl/opencl-init.hpp>
#include <rt/scene.hpp>


class FrameBuffer {
	
public:

	// bool initialize(CLInfo& clinfo, uint32_t sz[2]);
	bool initialize(CLInfo& clinfo, uint32_t sz[2],
			SceneInfo& scene_info, cl_mem& cl_hit_mem,
			cl_mem& cl_cm_posx_mem,
			cl_mem& cl_cm_negx_mem,
			cl_mem& cl_cm_posy_mem,
			cl_mem& cl_cm_negy_mem,
			cl_mem& cl_cm_posz_mem,
			cl_mem& cl_cm_negz_mem);

	cl_mem& image_mem(){return img_mem;}

	bool clear();
	bool update(cl_mem& ray_mem, int32_t size, cl_int arg);
	bool copy(cl_mem& tex_mem);

private:

	uint32_t size[2];

	CLKernelInfo init_clk;
	CLKernelInfo update_clk;
	CLKernelInfo copy_clk;

	cl_mem img_mem;
};

#endif /* RT_FRAMEBUFFER_HPP */
