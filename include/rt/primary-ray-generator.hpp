#pragma once
#ifndef PRIMARY_RAY_GENERATOR_HPP
#define PRIMARY_RAY_GENERATOR_HPP

#include <gpu/interface.hpp>
#include <rt/timing.hpp>
#include <rt/ray.hpp>
#include <rt/camera.hpp>
#include <rt/renderer-config.hpp>

RT_ALIGN(4)
struct pixel_sample_cl{

	cl_float ox;
	cl_float oy;
	cl_float contribution;
};

class PrimaryRayGenerator{

public:

        PrimaryRayGenerator();
	int32_t initialize();

	int32_t set_spp(size_t spp, pixel_sample_cl const* psamples);
	size_t  get_spp() const;
	DeviceMemory&       get_pixel_samples();


	int32_t generate(const Camera& cam, RayBundle& bundle, size_t size[2],
                     const size_t ray_count, const size_t offset);

	void   timing(bool b);
	double get_exec_time();

        void update_configuration(const RendererConfig& conf);
	
private:

        function_id generator_id;
        function_id generator_zcurve_id;
        memory_id pixel_samples_id;

	// CLKernelInfo ray_clk;

        bool         m_initialized;
	bool         m_timing;
	rt_time_t    m_timer;
	double       m_time_ms;
        bool         m_use_zcurve;
	cl_int       m_quad_size;
	cl_int       m_spp;
};

#endif /* PRIMARY_RAY_GENERATOR_HPP */
