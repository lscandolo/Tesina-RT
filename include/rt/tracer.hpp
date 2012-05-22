#ifndef RT_TRACER_HPP
#define RT_TRACER_HPP

#include <cl-gl/opencl-init.hpp>
#include <gpu/interface.hpp>
#include <rt/timing.hpp>
#include <rt/scene.hpp>
#include <rt/ray.hpp>

class Tracer {

public:

        Tracer();
	int32_t initialize(const CLInfo& clinfo);

	int32_t trace(Scene& scene, int32_t ray_count, 
                      RayBundle& rays, HitBundle& hits, bool secondary = false);
	int32_t trace(Scene& scene, DeviceMemory& bvh_mem, int32_t ray_count, 
                      RayBundle& rays, HitBundle& hits, bool secondary = false);

	int32_t shadow_trace(Scene& scene, int32_t ray_count, 
                             RayBundle& rays, HitBundle& hits, bool secondary = false);
	int32_t shadow_trace(Scene& scene, DeviceMemory& bvh_mem, int32_t ray_count, 
                             RayBundle& rays, HitBundle& hits, bool secondary = false);


	void timing(bool b);
	double get_trace_exec_time();
	double get_shadow_exec_time();

private:

        DeviceInterface device;
        function_id single_tracer_id;
        function_id multi_tracer_id;

        function_id single_shadow_id;
        function_id multi_shadow_id;

	// CLKernelInfo tracer_clk;
	// CLKernelInfo shadow_clk;

	bool         m_initialized;
	bool         m_timing;
	rt_time_t    m_tracer_timer;
	double       m_tracer_time_ms;
	rt_time_t    m_shadow_timer;
	double       m_shadow_time_ms;
};

#endif /* RT_TRACER_HPP */
