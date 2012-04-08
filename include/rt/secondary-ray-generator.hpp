#ifndef RT_SECONDARY_RAY_GENERATOR_HPP
#define RT_SECONDARY_RAY_GENERATOR_HPP

#include <cl-gl/opencl-init.hpp>
#include <gpu/interface.hpp>
#include <rt/timing.hpp>
#include <rt/ray.hpp>
#include <rt/scene.hpp>

class SecondaryRayGenerator {

public:

        SecondaryRayGenerator();
	int32_t initialize(const CLInfo& clinfo);
	int32_t generate(Scene& scene, RayBundle& ray_in, size_t rays_in,
                         HitBundle& hits, RayBundle& ray_out, size_t* rays_out);
	void set_max_rays(size_t max);

	void timing(bool b);
	double get_exec_time();
      

private:

        DeviceInterface device;
        memory_id ray_count_id;
        function_id generator_id;

	cl_int m_generated_rays;
	cl_int m_max_rays;

        bool         m_initialized;
	bool         m_timing;
	rt_time_t    m_timer;
	double       m_time_ms;
};

#endif /* RT_SECONDARY_RAY_GENERATOR_HPP */
