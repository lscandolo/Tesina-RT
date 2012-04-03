#ifndef RT_FRAMEBUFFER_HPP
#define RT_FRAMEBUFFER_HPP

#include <stdint.h>

#include <cl-gl/opencl-init.hpp>
#include <gpu/interface.hpp>
#include <rt/timing.hpp>
#include <rt/ray.hpp>
#include <rt/scene.hpp>
#include <rt/cubemap.hpp>

class FrameBuffer {
	
public:

	// bool initialize(CLInfo& clinfo, uint32_t sz[2]);
	bool initialize(CLInfo& clinfo, uint32_t sz[2]);

	// cl_mem& image_mem(){return *device.memory(img_mem_id).ptr();}
        // const DeviceMemory& img_mem() const {return device.memory(img_mem_id);}
        DeviceMemory& image_mem(){return device.memory(img_mem_id);}

	bool clear();
	bool copy(DeviceMemory& tex_mem);

	void enable_timing(bool b);
	double get_clear_exec_time();
	double get_copy_exec_time();

private:

        DeviceInterface device;

	uint32_t size[2];

	// CLKernelInfo init_clk;
	// CLKernelInfo copy_clk;

        memory_id img_mem_id;
        function_id init_id;
        function_id copy_id;

	// cl_mem img_mem;

	bool         timing;
	rt_time_t    clear_timer;
	double       clear_time_ms;
	rt_time_t    copy_timer;
	double       copy_time_ms;
};

#endif /* RT_FRAMEBUFFER_HPP */
