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
	bool initialize(CLInfo& clinfo, uint32_t sz[2]);

	cl_mem& image_mem(){return img_mem;}

	bool clear();
	bool copy(cl_mem& tex_mem);

private:

	uint32_t size[2];

	CLKernelInfo init_clk;
	CLKernelInfo copy_clk;

	cl_mem img_mem;
};

#endif /* RT_FRAMEBUFFER_HPP */
