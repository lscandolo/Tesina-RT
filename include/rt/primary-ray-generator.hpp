#ifndef PRIMARY_RAY_GENERATOR_HPP
#define PRIMARY_RAY_GENERATOR_HPP

#include <rt/timing.hpp>
#include <rt/ray.hpp>
#include <rt/camera.hpp>

RT_ALIGN(4)
struct sample_cl{

	cl_float ox;
	cl_float oy;
	cl_float contribution;
};

class PrimaryRayGenerator{

public:
	bool initialize(CLInfo& clinfo);

	bool set_spp(int _spp, sample_cl const* samples);
	int  get_spp() const;
	const cl_mem& get_samples() const;
	cl_mem& get_samples();


	bool set_rays(const Camera& cam, RayBundle& bundle, uint32_t size[2],
		      const int32_t ray_count, const int32_t offset);

	void enable_timing(bool b);
	double get_exec_time();
	
private:

	CLKernelInfo ray_clk;

	cl_mem       samples_mem;
	cl_int       spp;

	bool         timing;
	rt_time_t    timer;
	double       time_ms;
};

#endif /* PRIMARY_RAY_GENERATOR_HPP */
