#pragma once
#ifndef RT_SECONDARY_RAY_GENERATOR_HPP
#define RT_SECONDARY_RAY_GENERATOR_HPP

#include <cl-gl/opencl-init.hpp>
#include <gpu/interface.hpp>
#include <rt/timing.hpp>
#include <rt/ray.hpp>
#include <rt/scene.hpp>
#include <rt/renderer-config.hpp>

class SecondaryRayGenerator {

public:

        SecondaryRayGenerator();
	int32_t initialize();

	int32_t generate(Scene& scene, RayBundle& ray_in, size_t rays_in,
                         HitBundle& hits, RayBundle& ray_out, size_t* rays_out);

        void set_max_rays(size_t max);

	void timing(bool b);
	double get_exec_time();

        void update_configuration(const RendererConfig& conf);

private:

	int32_t gen_scan(Scene& scene, RayBundle& ray_in, size_t rays_in,
                         HitBundle& hits, RayBundle& ray_out, size_t* rays_out);
	int32_t gen_scan_disc(Scene& scene, RayBundle& ray_in, size_t rays_in,
                              HitBundle& hits, RayBundle& ray_out, size_t* rays_out);

        int32_t gen_tasks(Scene& scene, RayBundle& ray_in, size_t rays_in,
                               HitBundle& hits, RayBundle& ray_out, size_t* rays_out);
        int32_t gen_tasks_disc(Scene& scene, RayBundle& ray_in, size_t rays_in,
                                    HitBundle& hits, RayBundle& ray_out, size_t* rays_out);

        function_id marker_id;
        function_id generator_id;

        function_id marker_disc_id;
        function_id generator_disc_id;

        function_id gen_sec_ray_id;

	cl_int m_generated_rays;
	cl_int m_max_rays;

        bool         m_tasks;
        bool         m_disc;
        bool         m_initialized;
	bool         m_timing;
	rt_time_t    m_timer;
	double       m_time_ms;

        memory_id count_id;

        memory_id counters_id;

        // memory_id count_in_id;
        // memory_id count_out_id;
        // memory_id totals_id;
};

#endif /* RT_SECONDARY_RAY_GENERATOR_HPP */
