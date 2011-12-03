#include <stdint.h>

#include <rt/primary-ray-generator.hpp>

// static
bool 
PrimaryRayGenerator::set_rays(Camera& cam, RayBundle& bundle, int32_t size[2])
{
	/*!! Assert raybundle size is correct*/

	int32_t width = size[0];
	int32_t height = size[1];

	float x_start = (0.5f / width);
	float y_start = (0.5f / height);

	float x_step = (1.f / width);
	float y_step = (1.f / height);

	float current_y = y_start;

	for (uint32_t y = 0; y < height; ++y,current_y += y_step){
		float current_x = x_start;
		for (uint32_t x = 0; x < width; ++x, current_x += x_step) {
			uint32_t index = y*width+x;
			Ray& ray = bundle[index];
			ray = cam.get_ray(current_x,current_y);
		}
	}
	return true;
}

