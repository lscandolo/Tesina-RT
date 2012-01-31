#ifndef PRIMARY_RAY_GENERATOR_HPP
#define PRIMARY_RAY_GENERATOR_HPP

#include <rt/ray.hpp>
#include <rt/camera.hpp>

class PrimaryRayGenerator{

public:
	bool initialize(CLInfo& clinfo);
	bool set_rays(const Camera& cam, RayBundle& bundle, uint32_t size[2],
		      const int32_t ray_count, const int32_t offset);

	
private:

	CLKernelInfo ray_clk;

};

#endif /* PRIMARY_RAY_GENERATOR_HPP */
