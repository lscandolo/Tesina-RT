#ifndef PRIMARY_RAY_GENERATOR_HPP
#define PRIMARY_RAY_GENERATOR_HPP

#include <rt/ray.hpp>
#include <rt/camera.hpp>

class PrimaryRayGenerator{

public:
	static bool set_rays(Camera& cam, RayBundle& bundle, int32_t size[2]);

};

#endif /* PRIMARY_RAY_GENERATOR_HPP */
