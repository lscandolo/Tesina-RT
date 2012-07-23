#pragma once
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

        FrameBuffer() : device(*DeviceInterface::instance()) {}

	int32_t initialize(size_t sz[2]);

        DeviceMemory& image_mem(){return device.memory(img_mem_id);}

	int32_t clear();
	int32_t copy(DeviceMemory& tex_mem);

	void timing(bool b);
	double get_clear_exec_time();
	double get_copy_exec_time();

private:

        DeviceInterface& device;

	size_t size[2];

        memory_id img_mem_id;
        function_id init_id;
        function_id copy_id;

	bool         m_timing;
	rt_time_t    m_clear_timer;
	double       m_clear_time_ms;
	rt_time_t    m_copy_timer;
	double       m_copy_time_ms;
};

#endif /* RT_FRAMEBUFFER_HPP */
